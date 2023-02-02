#!/bin/bash
#
# Fetch and extract Geany dependencies for Windows/MSYS2
# This script will download (or use Pacman's cache) to extract
# dependencies as defined below needed to run Geany.
# To be run within a MSYS2 shell. The extracted files will be
# placed into the current directory.

ABI=x86_64  # do not change, i686 is not supported any longer
use_cache="no"
make_zip="no"
gtkv="3"
run_pi="y"
cross="no"

UNX_UTILS_URL="https://download.geany.org/contrib/UnxUpdates.zip"

# Wine commands for 32bit and 64bit binaries (we still need 32bit for UnxUtils sort.exe)
# Used only when "-x" is set
EXE_WRAPPER_32="mingw-w64-i686-wine"
EXE_WRAPPER_64="mingw-w64-x86_64-wine"

package_urls=""
gtk3_dependency_pkgs=""
gtk4_dependency_pkgs=""

packages="
adwaita-icon-theme
atk
brotli
bzip2
cairo
expat
fontconfig
freetype
fribidi
gcc-libs
gdk-pixbuf2
gettext
glib2
graphite2
grep
gtk-update-icon-cache
harfbuzz
hicolor-icon-theme
libdatrie
libepoxy
libffi
libiconv
libpng
librsvg
libthai
libwinpthread-git
libxml2
pango
pcre
pixman
xz
zlib
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
		"-3")
			gtkv="3"
			;;
		"-4")
			gtkv="4"
			;;
		"-n")
			run_pi=""
			;;
		"-x")
			cross="yes"
			;;
		"-h"|"--help")
			echo "gtk-bundle-from-msys2.sh [-c] [-h] [-n] [-z] [-3 | -4] [CACHEDIR]"
			echo "      -c Use pacman cache. Otherwise pacman will download"
			echo "         archive files"
			echo "      -h Show this help screen"
			echo "      -n Do not run post install scripts of the packages"
			echo "      -z Create a zip afterwards"
			echo "      -3 Prefer gtk3"
			echo "      -4 Prefer gtk4"
			echo "      -x Set when the script is executed in a cross-compilation context (e.g. to use wine)"
			echo "CACHEDIR Directory where to look for cached packages (default: /var/cache/pacman/pkg)"
			exit 1
			;;
		*)
			cachedir="$opt"
			;;
		esac
	done
}

set -e  # stop on errors
# enable extended globbing as we need it in _getpkg
shopt -s extglob

initialize() {
	if [ -z "$cachedir" ]; then
		cachedir="/var/cache/pacman/pkg"
	fi

	if [ "$use_cache" = "yes" ] && ! [ -d "$cachedir" ]; then
		echo "Cache dir \"$cachedir\" not a directory"
		exit 1
	fi

	if [ "$cross" != "yes" ]; then
		# if running natively, we do not need wine or any other wrappers
		EXE_WRAPPER_32=""
		EXE_WRAPPER_64=""
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
		package_info=$(pacman -Qi mingw-w64-$ABI-$1)
		package_version=$(echo "$package_info" | grep "^Version " | cut -d':' -f 2 | tr -d '[[:space:]]')
		# use @(gz|xz|zst) to filter out signature files (e.g. mingw-w64-x86_64-...-any.pkg.tar.zst.sig)
		ls $cachedir/mingw-w64-${ABI}-${1}-${package_version}-*.tar.@(gz|xz|zst) | sort -V | tail -n 1
	else
		# -dd to ignore dependencies as we listed them already above in $packages and
		# make pacman ignore its possibly existing cache (otherwise we would get an URL to the cache)
		pacman -Sddp --cachedir /nonexistent mingw-w64-${ABI}-${1}
	fi
}

_remember_package_source() {
	if [ "$use_cache" = "yes" ]; then
		package_url=$(pacman -Sddp mingw-w64-${ABI}-${2})
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
			filename=$(basename "$pkg")
			curl --silent --location --output "$filename" "$pkg"
			tar xf "$filename"
			rm "$filename"
		fi
		if [ "$make_zip" = "yes" -a "$i" = "$gtk" ]; then
			VERSION=$(grep ^pkgver .PKGINFO | sed -e 's,^pkgver = ,,' -e 's,-.*$,,')
		fi
		rm -f .INSTALL .MTREE .PKGINFO .BUILDINFO
	done
}

move_extracted_files() {
	echo "Move extracted data to destination directory"
	if [ -d mingw64 ]; then
		for d in bin etc home include lib locale share var; do
			if [ -d "mingw64/$d" ]; then
				rm -rf $d
				# prevent sporadic 'permission denied' errors on my system, not sure why they happen
				sleep 0.5
				mv mingw64/$d .
			fi
		done
		rmdir mingw64
	fi
}

delayed_post_install() {
	if [ "$run_pi" ]; then
		echo "Execute delayed post install tasks"
		# Commands have been collected manually from the various .INSTALL scripts
		${EXE_WRAPPER_64} bin/fc-cache.exe -f
		${EXE_WRAPPER_64} bin/gdk-pixbuf-query-loaders.exe --update-cache
		${EXE_WRAPPER_64} bin/gtk-update-icon-cache-3.0.exe -q -t -f share/icons/hicolor
		${EXE_WRAPPER_64} bin/gtk-update-icon-cache-3.0.exe -q -t -f share/icons/Adwaita
		${EXE_WRAPPER_64} bin/glib-compile-schemas share/glib-2.0/schemas/
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
	rm -rf lib/cmake
	rm -rf lib/gettext
	rm -rf lib/libffi-*
	rm -rf lib/pkgconfig
	rm -rf lib/python3.9
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
	rm -rf share/gettext-*
	rm -rf share/gir-1.0
	rm -rf share/glib-2.0/codegen
	rm -rf share/glib-2.0/gdb
	rm -rf share/glib-2.0/gettext
	rm -rf share/graphite2
	rm -rf share/gtk-3.0
	rm -rf share/gtk-doc
	rm -rf share/icons/Adwaita/cursors
	rm -rf share/info
	rm -rf share/man
	rm -rf share/thumbnailers
	rm -rf share/vala
	rm -rf share/xml
	rm -rf usr/share/libalpm
	# cleanup binaries and libs (delete anything except *.dll and GSpawn helper binaries)
	find bin ! -name '*.dll' ! -name 'grep.exe' ! -name 'gspawn-win32-helper*.exe' -type f -delete
	# cleanup empty directories
	find . -type d -empty -delete
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
	# sort.exe from UnxUtils is a 32bit binary, so use $EXE_WRAPPER_32
	sort_version="$(${EXE_WRAPPER_32} bin/sort.exe --version | sed -n 1p)"
	# use "sed -n 1p" instead of "head -n1" as grep will otherwise prints a weird error,
	# probably because the output pipe is closed prematurely
	grep_version="$(${EXE_WRAPPER_64} bin/grep.exe --version | sed -n 1p)"
	filename="ReadMe.Dependencies.Geany.txt"
	cat << EOF > "${filename}"
This installation contains dependencies for Geany which are distributed
as binaries (usually .dll files) as well as additional files
necessary for these dependencies.
Following is a list of all included binary packages with their
full download URL as used to create this installation.

sort.exe is extracted from the ZIP archive at
${UNX_UTILS_URL}.
Sort version: ${sort_version}

grep.exe is taken from a 64bit MSYS2 installation and
is bundled together with its dependencies.
Grep version: ${grep_version}

Other dependencies are provided by the MSYS2 project
(https://msys2.github.io) and were downloaded from:
EOF
	echo -e "${package_urls}" >> "${filename}"
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
delayed_post_install
cleanup_unnecessary_files
download_and_extract_sort
create_bundle_dependency_info_file
create_zip_archive
