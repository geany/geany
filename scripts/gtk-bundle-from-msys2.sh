#!/bin/sh
#
# Fetch and extract Geany dependencies for Windows/MSYS2
# This script will download (or use Pacman's cache) to extract
# dependencies as defined below needed to run Geany.
# To be run within a MSYS2 shell. The extracted files will be
# placed into the current directory.

ABI=i686
use_cache="no"
make_zip="no"
gtkv="3"
run_pi="y"

UNX_UTILS_URL="http://unxutils.sourceforge.net/UnxUpdates.zip"
GREP_URL="https://download.geany.org/contrib/grep-2.23.tar.xz"

package_urls=""
gtk2_dependency_pkgs=""
gtk3_dependency_pkgs="
libepoxy
hicolor-icon-theme
adwaita-icon-theme
"

packages="
gcc-libs
pcre
zlib
expat
libffi
libiconv
bzip2
libffi
libpng
gettext
glib2
libwinpthread-git
harfbuzz
fontconfig
freetype
atk
pango
cairo
pixman
gdk-pixbuf2
"

handle_command_line_options() {
	for opt in "$@"; do
		case "$opt" in
		"-c"|"--cache")
			use_cache="yes"
			;;
		"-z"|"--zip")
			make_zip="yes"
			;;
		"-2")
			gtkv="2"
			;;
		"-3")
			gtkv="3"
			;;
		"-n")
			run_pi=""
			;;
		"-h"|"--help")
			echo "gtk-bundle-from-msys2.sh [-c] [-h] [-n] [-z] [-2 | -3] [CACHEDIR]"
			echo "      -c Use pacman cache. Otherwise pacman will download"
			echo "         archive files"
			echo "      -h Show this help screen"
			echo "      -n Do not run post install scripts of the packages"
			echo "      -z Create a zip afterwards"
			echo "      -2 Prefer gtk2"
			echo "      -3 Prefer gtk3"
			echo "CACHEDIR Directory where to look for cached packages (default: /var/cache/pacman/pkg)"
			exit 1
			;;
		*)
			cachedir="$opt"
			;;
		esac
	done
}

initialize() {
	if [ -z "$cachedir" ]; then
		cachedir="/var/cache/pacman/pkg"
	fi

	if [ "$use_cache" = "yes" ] && ! [ -d "$cachedir" ]; then
		echo "Cache dir \"$cachedir\" not a directory"
		exit 1
	fi

	gtk="gtk$gtkv"
	eval "gtk_dependency_pkgs=\${${gtk}_dependency_pkgs}"

	pkgs="
${packages}
${gtk_dependency_pkgs}
${gtk}
"
}

_getpkg() {
	if [ "$use_cache" = "yes" ]; then
		package_info=`pacman -Qi mingw-w64-$ABI-$1`
		package_version=`echo "$package_info" | grep "^Version " | cut -d':' -f 2 | tr -d '[[:space:]]'`
		ls $cachedir/mingw-w64-${ABI}-${1}-${package_version}-* | sort -V | tail -n 1
	else
		pacman -Sp mingw-w64-${ABI}-${1}
	fi
}

_remember_package_source() {
	if [ "$use_cache" = "yes" ]; then
		package_url=`pacman -Sp mingw-w64-${ABI}-${2}`
	else
		package_url="${1}"
	fi
	package_urls="${package_urls}\n${package_url}"
}

extract_packages() {
	for i in $pkgs; do
		pkg=$(_getpkg $i)
		_remember_package_source $pkg $i
		if [ "$use_cache" = "yes" ]; then
			if [ -e "$pkg" ]; then
				echo "Extracting $pkg from cache"
				tar xaf $pkg
			else
				echo "ERROR: File $pkg not found"
				exit 1
			fi
		else
			echo "Download $pkg using curl"
			curl -L "$pkg" | tar -x --xz
		fi
		if [ -n "$run_pi" ] && [ -f .INSTALL ]; then
			echo "Running post_install script for \"$i\""
			/bin/bash -c ". .INSTALL; post_install"
		fi
		if [ "$make_zip" = "yes" -a "$i" = "$gtk" ]; then
			VERSION=$(grep ^pkgver .PKGINFO | sed -e 's,^pkgver = ,,' -e 's,-.*$,,')
		fi
		rm -f .INSTALL .MTREE .PKGINFO .BUILDINFO
	done
}

move_extracted_files() {
	echo "Move extracted data to destination directory"
	if [ -d mingw32 ]; then
		for d in bin etc home include lib locale share var; do
			if [ -d "mingw32/$d" ]; then
				rm -rf $d
				# prevent sporadic 'permission denied' errors on my system, not sure why they happen
				sleep 0.5
				mv mingw32/$d .
			fi
		done
		rmdir mingw32
	fi
}

cleanup_unnecessary_files() {
	echo "Cleanup unnecessary files"
	# cleanup temporary files
	rm -rf var/cache/fontconfig
	rmdir var/cache
	rmdir var
	# cleanup development and other unnecessary files
	rm -rf include
	rm -rf lib/gettext
	rm -rf lib/libffi-*
	rm -rf lib/pkgconfig
	find lib -name '*.a' -delete
	find lib -name '*.typelib' -delete
	find lib -name '*.def' -delete
	find lib -name '*.h' -delete
	find lib -name '*.sh' -delete
	# cleanup other unnecessary files
	rm -rf share/aclocal
	rm -rf share/applications
	rm -rf share/bash-completion
	rm -rf share/doc
	rm -rf share/gdb
	rm -rf share/gettext
	rm -rf share/gir-1.0
	rm -rf share/glib-2.0/codegen
	rm -rf share/glib-2.0/gdb
	rm -rf share/glib-2.0/gettext
	rm -rf share/gtk-2.0
	rm -rf share/gtk-3.0
	rm -rf share/gtk-doc
	rm -rf share/info
	rm -rf share/man
	rm -rf share/xml
	# cleanup binaries and libs (delete anything except *.dll)
	find bin ! -name '*.dll' -type f -delete
	# cleanup empty directories
	find . -type d -empty -delete
}

download_and_compile_grep() {
	grep_archive="grep_source.tar.xz"
	grep_build_dir="grep_build"
	grep_build_log="grep_build.log"
	echo "Download and compile 'grep' (see ${grep_build_dir}/${grep_build_log} for details)"
	mkdir ${grep_build_dir}
	cd ${grep_build_dir}
	wget --no-verbose -O ${grep_archive} ${GREP_URL}
	tar xf ${grep_archive}
	grep-2*/configure > ${grep_build_log} 2>&1 || exit 1
	make >> ${grep_build_log} 2>&1 || exit 1
	strip src/grep.exe
	cp src/grep.exe ../bin/
	cd ..
	rm -rf ${grep_build_dir}
}

download_and_extract_sort() {
	echo "Download and unpack 'sort'"
	# add sort to the bin directory
	unxutils_archive="unxutilsupdates.zip"
	wget --no-verbose -O ${unxutils_archive} ${UNX_UTILS_URL}
	unzip ${unxutils_archive} sort.exe -d bin/
	rm ${unxutils_archive}
}

create_bundle_dependency_info_file() {
	filename="ReadMe.Dependencies.Geany.txt"
	cat << EOF > "${filename}"
This installation contains dependencies for Geany which are distributed
as binaries (usually .dll files) as well as additional files
necessary for these dependencies.
Following is a list of all included binary packages with their
full download URL as used to create this installation.

sort.exe is extracted from the ZIP archive at
${UNX_UTILS_URL}.

grep.exe is self-compiled from the sources available at
${GREP_URL}.
Used command to compile: ./configure && make

Other dependencies are provided by the MSYS2 project
(https://msys2.github.io) and were downloaded from:
EOF
	echo -e "${package_urls}" >> "${filename}"
	unix2dos "${filename}"
}

create_zip_archive() {
	if [ "$make_zip" = "yes" ]; then
		if [ -z "$VERSION" ]; then
			VERSION="unknown-version"
		fi
		echo "Packing the bundle"
		zip -r gtk-$VERSION.zip bin etc include lib locale share var
	fi
}


# main()
handle_command_line_options $@
initialize
extract_packages
move_extracted_files
cleanup_unnecessary_files
download_and_compile_grep
download_and_extract_sort
create_bundle_dependency_info_file
create_zip_archive
