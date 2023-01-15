#!/bin/bash
#
# Copyright 2022 The Geany contributors
# License: GPLv2
#
# Helper script to build Geany for Windows in a Docker container.
# The following steps are performed:
# - clone Geany repository if necessary (i.e. if it is not bind-mounted into the container)
# - cross-compile Geany for Windows 64bit and GTK3
# - sign all binaries and installer (if /certificates exist and contains cert.pem and key.pem)
# - download Geany-Themes for bundling
# - create GTK3 bundle with all dependencies (including grep and sort)
# - create the NSIS installer in ${OUTPUT_DIRECTORY}
# - test the created NSIS installer and compiled Geany
# - test uninstaller and check there is nothing left after uninstalling
#
# This script has to be executed within the Docker container.
# The Docker container should have a bind-mount for ${OUTPUT_DIRECTORY}
# where the resulting installer binary is stored.
#
# To test the installer and Geany binary "wine" is used.
# Please note that we need to use wine32 and wine64 as the
# created installer and uninstaller binaries are 32bit whereas the created
# Geany binary is 64bit.
#

GEANY_VERSION=  # will be set below from configure.ac
GEANY_GIT_REVISION=  # will be set below from configure.ac
OUTPUT_DIRECTORY="/output"
GEANY_GIT_REPOSITORY="https://github.com/geany/geany.git"
GEANY_THEMES_REPOSITORY="https://github.com/geany/geany-themes.git"

# rather static values, unlikely to be changed
GTK_BUNDLE_DIR="/build/gtk-bundle"
GEANY_SOURCE_DIR="/geany-source"
GEANY_BUILD_DIR="/build/geany-build"
GEANY_RELEASE_DIR="/build/geany-release"
GEANY_INSTALLER_FILENAME=  # will be set below
GEANY_INSTALLATION_DIR_WIN="C:\\geany_install"
GEANY_INSTALLATION_DIR=$(winepath --unix ${GEANY_INSTALLATION_DIR_WIN})
GEANY_THEMES_DIR="/build/geany-themes"
GEANY_RELEASE_BINARY_PATTERNS=(
	"${GEANY_RELEASE_DIR}/bin/geany.exe"
	"${GEANY_RELEASE_DIR}/bin/*.dll"
	"${GEANY_RELEASE_DIR}/lib/geany/*.dll"
)

# CI CFLAGS
CFLAGS="\
	-Wall \
	-Wextra \
	-O2 \
	-Wunused \
	-Wno-unused-parameter \
	-Wunreachable-code \
	-Wformat=2 \
	-Wundef \
	-Wpointer-arith \
	-Wwrite-strings \
	-Waggregate-return \
	-Wmissing-prototypes \
	-Wmissing-declarations \
	-Wmissing-noreturn \
	-Wmissing-format-attribute \
	-Wredundant-decls \
	-Wnested-externs \
	-Wno-deprecated-declarations"

# cross-compilation environment
ARCH="x86_64"
MINGW_ARCH="mingw64"
HOST="x86_64-w64-mingw32"
export CC="/usr/bin/${HOST}-gcc"
export CPP="/usr/bin/${HOST}-cpp"
export CXX="/usr/bin/${HOST}-g++"
export AR="/usr/bin/${HOST}-ar"
export STRIP="/usr/bin/${HOST}-strip"
export WINDRES="/usr/bin/${HOST}-windres"
export CFLAGS="-I/windows/${MINGW_ARCH}/include/ ${CFLAGS}"
export LDFLAGS="-static-libgcc ${LDFLAGS}"
export PKG_CONFIG_SYSROOT_DIR="/windows"
export PKG_CONFIG_PATH="/windows/${MINGW_ARCH}/lib/pkgconfig/"
export PKG_CONFIG="/usr/bin/pkg-config"
export NOCONFIGURE=1

# stop on errors
set -e


log() {
	echo "=========== $(date '+%Y-%m-%d %H:%M:%S %Z') $* ==========="
}


git_clone_geany_if_necessary() {
	if [ -d ${GEANY_SOURCE_DIR} ]; then
		log "Copying Geany source"
		cp --archive ${GEANY_SOURCE_DIR}/ ${GEANY_BUILD_DIR}/
	else
		log "Cloning Geany repository from ${GEANY_GIT_REPOSITORY}"
		git clone --depth 1 ${GEANY_GIT_REPOSITORY} ${GEANY_BUILD_DIR}
	fi
}


parse_geany_version() {
	GEANY_VERSION=$(sed -n -E -e 's/^AC_INIT.\[Geany\], \[(.+)\],/\1/p' ${GEANY_BUILD_DIR}/configure.ac)
	GEANY_GIT_REVISION=$(cd ${GEANY_BUILD_DIR} && git rev-parse --short --revs-only HEAD 2>/dev/null || true)
	TIMESTAMP=$(date +%Y%m%d%H%M%S)
	# add pull request number if this is a CI and a PR build
	if [ "${GITHUB_PULL_REQUEST}" ]; then
		GEANY_VERSION="${GEANY_VERSION}_ci_${TIMESTAMP}_${GEANY_GIT_REVISION}_pr${GITHUB_PULL_REQUEST}"
	elif [ "${CI}" -a "${GEANY_GIT_REVISION}" ]; then
		GEANY_VERSION="${GEANY_VERSION}_ci_${TIMESTAMP}_${GEANY_GIT_REVISION}"
	elif [ "${GEANY_GIT_REVISION}" ]; then
		GEANY_VERSION="${GEANY_VERSION}_git_${TIMESTAMP}_${GEANY_GIT_REVISION}"
	fi
	GEANY_INSTALLER_FILENAME="geany-${GEANY_VERSION}_setup.exe"
}


log_environment() {
	log "Using environment"
	CONFIGURE_OPTIONS="--disable-silent-rules --host=${HOST} --prefix=${GEANY_RELEASE_DIR} --with-libiconv-prefix=/windows/mingw64"
	echo "Geany version        : ${GEANY_VERSION}"
	echo "Geany GIT revision   : ${GEANY_GIT_REVISION}"
	echo "PATH                 : ${PATH}"
	echo "HOST                 : ${HOST}"
	echo "CC                   : ${CC}"
	echo "CFLAGS               : ${CFLAGS}"
	echo "Configure            : ${CONFIGURE_OPTIONS}"
}


patch_version_information() {
	log "Patching version information"

	if [ -z "${GEANY_GIT_REVISION}" ] && [ -z "${CI}" ]; then
		return
	fi

	# parse version string and decrement the patch and/or minor levels to keep nightly build
	# versions below the next release version
	regex='^([0-9]*)[.]([0-9]*)([.]([0-9]*))?'
	if [[ ${GEANY_VERSION} =~ $regex ]]; then
		MAJOR="${BASH_REMATCH[1]}"
		MINOR="${BASH_REMATCH[2]}"
		PATCH="${BASH_REMATCH[4]}"
		if [ -z "${PATCH}" ] || [ "${PATCH}" = "0" ]; then
			MINOR="$((MINOR-1))"
			PATCH="90"
		else
			PATCH="$((PATCH-1))"
		fi
	else
		echo "Could not extract or parse version tag" >&2
		exit 1
	fi
	# replace version information in configure.ac and for Windows binaries
	sed -i -E "s/^AC_INIT.\[Geany\], \[(.+)\],/AC_INIT(\[Geany\], \[${GEANY_VERSION}\],/" ${GEANY_BUILD_DIR}/configure.ac
	sed -i -E "s/^#define VER_FILEVERSION_STR[[:space:]]+\".*\"+/#define VER_FILEVERSION_STR \"${GEANY_VERSION}\"/" ${GEANY_BUILD_DIR}/geany_private.rc
	sed -i -E "s/^#define VER_FILEVERSION[[:space:]]+[0-9,]+/#define VER_FILEVERSION ${MAJOR},${MINOR},${PATCH},90/" ${GEANY_BUILD_DIR}/geany_private.rc
	sed -i -E "s/^[[:space:]]+version=\"[0-9.]+\"/version=\"${MAJOR}.${MINOR}.${PATCH}.90\"/" ${GEANY_BUILD_DIR}/geany.exe.manifest
	sed -i -E "s/^!define PRODUCT_VERSION \"@VERSION@\"/!define PRODUCT_VERSION \"${GEANY_VERSION}\"/" ${GEANY_BUILD_DIR}/geany.nsi.in
	sed -i -E "s/^!define PRODUCT_VERSION_ID \"@VERSION@.0.0\"/!define PRODUCT_VERSION_ID \"${MAJOR}.${MINOR}.${PATCH}.90\"/" ${GEANY_BUILD_DIR}/geany.nsi.in
}


build_geany() {
	cd ${GEANY_BUILD_DIR}
	log "Running autogen.sh"
	./autogen.sh
	log "Running configure"
	./configure ${CONFIGURE_OPTIONS}
	log "Running make"
	make
	log "Running install-strip"
	make install-strip
}


sign_file() {
	echo "Sign file $1"
	if [ -f /certificates/cert.pem ] && [ -f /certificates/key.pem ]; then
		osslsigncode sign \
			-certs /certificates/cert.pem \
			-key /certificates/key.pem \
			-n "Geany Binary" \
			-i "https://www.geany.org/" \
			-ts http://zeitstempel.dfn.de/ \
			-h sha512 \
			-in ${1} \
			-out ${1}-signed
		mv ${1}-signed ${1}
	else
		echo "Skip signing due to missing certificate"
	fi
}


sign_geany_binaries() {
	log "Signing Geany binary files"
	for binary_file_pattern in ${GEANY_RELEASE_BINARY_PATTERNS[@]}; do
		for binary_file in $(ls ${binary_file_pattern}); do
			sign_file ${binary_file}
		done
	done
}


create_gtk_bundle() {
	log "Creating GTK bundle"
	mkdir ${GTK_BUNDLE_DIR}
	cd ${GTK_BUNDLE_DIR}
	bash ${GEANY_BUILD_DIR}/scripts/gtk-bundle-from-msys2.sh -x -3
}


fetch_geany_themes() {
	log "Cloning Geany-Themes repository from ${GEANY_THEMES_REPOSITORY}"
	git clone --depth 1 ${GEANY_THEMES_REPOSITORY} ${GEANY_THEMES_DIR}
}


create_installer() {
	log "Creating NSIS installer"
	makensis \
		-V3 \
		-WX \
		-DGEANY_INSTALLER_NAME="${GEANY_INSTALLER_FILENAME}" \
		-DGEANY_RELEASE_DIR=${GEANY_RELEASE_DIR} \
		-DGEANY_THEMES_DIR=${GEANY_THEMES_DIR} \
		-DGTK_BUNDLE_DIR=${GTK_BUNDLE_DIR} \
		${GEANY_BUILD_DIR}/geany.nsi
}


sign_installer() {
	log "Signing NSIS installer"
	sign_file ${GEANY_BUILD_DIR}/${GEANY_INSTALLER_FILENAME}
}


test_installer() {
	log "Test NSIS installer"
	exiftool -FileName -FileType -FileVersion -FileVersionNumber ${GEANY_BUILD_DIR}/${GEANY_INSTALLER_FILENAME}
	# install Geany: perform a silent install and check for installed files
	mingw-w64-i686-wine ${GEANY_BUILD_DIR}/${GEANY_INSTALLER_FILENAME} /S /D=${GEANY_INSTALLATION_DIR_WIN}
	# check if we have something installed
	test -f ${GEANY_INSTALLATION_DIR}/uninst.exe || exit 1
	test -f ${GEANY_INSTALLATION_DIR}/bin/geany.exe || exit 1
	test -f ${GEANY_INSTALLATION_DIR}/lib/geany/export.dll || exit 1
	test ! -f ${GEANY_INSTALLATION_DIR}/lib/geany/demoplugin.dll || exit 1
}


log_geany_version() {
	log "Log installed Geany version"
	mingw-w64-x86_64-wine ${GEANY_INSTALLATION_DIR}/bin/geany.exe --version 2>/dev/null
	exiftool -FileName -FileType -FileVersion -FileVersionNumber ${GEANY_INSTALLATION_DIR}/bin/geany.exe
}


test_uninstaller() {
	log "Test NSIS uninstaller"
	# uninstall Geany and test if everything is clean
	mingw-w64-i686-wine ${GEANY_INSTALLATION_DIR}/uninst.exe /S
	sleep 10  # it seems the uninstaller returns earlier than the files are actually removed, so wait a moment
	test ! -e ${GEANY_INSTALLATION_DIR}
}


copy_installer_and_bundle() {
	log "Copying NSIS installer and GTK bundle"
	cp ${GEANY_BUILD_DIR}/${GEANY_INSTALLER_FILENAME} ${OUTPUT_DIRECTORY}
	cd ${GTK_BUNDLE_DIR}
	zip --quiet --recurse-paths ${OUTPUT_DIRECTORY}/gtk_bundle_$(date '+%Y%m%dT%H%M%S').zip *
}


main() {
	git_clone_geany_if_necessary

	parse_geany_version
	log_environment
	patch_version_information
	build_geany
	sign_geany_binaries

	create_gtk_bundle
	fetch_geany_themes
	create_installer
	sign_installer

	test_installer
	log_geany_version
	test_uninstaller

	copy_installer_and_bundle

	log "Done."
}


main
