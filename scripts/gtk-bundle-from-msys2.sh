#!/bin/sh

ABI=i686

GLIB=
ADW=
GTK=

use_cache="no"
make_zip="no"
gtkv="3"

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
	"-h"|"--help")
		echo "gtk-bundle-from-msys2.sh [-c] [-z] [-2 | -3]"
		echo "   -c Use pacman cache. Otherwise pacman will download"
		echo "      archive files"
		echo "   -z Create a zip afterwards"
		echo "   -2 Prefer gtk2"
		echo "   -3 Prefer gtk3"
		exit 1
		;;
	*)
		cachedir="$opt"
		;;
	esac
done

if [ -z "$cachedir" ]; then
	cachedir="/var/cache/pacman/pkg"
fi

if [ "$use_cache" = "yes" ] && ! [ -d "$cachedir" ]; then
	echo "Cache dir \"$cachedir\" not a directory"
	exit 1
fi

gtk="gtk$gtkv"

getpkg() {
	if [ "$use_cache" = "yes" ]; then
		ls $cachedir/mingw-w64-$ABI-$1-* | sort -V | tail -n 1
	else
		pacman -Sp mingw-w64-$ABI-$1
	fi
}

LIBS=$(getpkg gcc-libs)
INTL=$(getpkg gettext)
ICONV=$(getpkg libiconv)
GLIB=$(getpkg glib2)
ATK=$(getpkg atk)
PANGO=$(getpkg pango)
CAIRO=$(getpkg cairo)
ADW=$(getpkg adwaita-icon-theme)
GTK=$(getpkg $gtk)

cat <<EOF
Using:
gcc-libs: $LIBS
intl: $INTL
iconv: $ICONV
glib: $GLIB
atk: $ATK
pango: $PANGO
cairo: $CAIRO
adwaita $ADW
$gtk: $GTK
EOF


for i in $LIBS $INTL $ICONV $GLIB $ATK $PANGO $CAURIO $ADW $GTK; do
	if [ "$use_cache" = "yes" -a -e "$i" ]; then
		echo "Extracting $i from cache"
		tar xaf $i
	else
		echo "Download $i using curl"
		curl -L "$i" | tar -x --xz
	fi
	echo "Running post_install script"
	test -f .INSTALL && /bin/bash -c ". .INSTALL; post_install"
	if [ "$make_zip" = "yes" -a "$i" = "$GTK" ]; then
		VERSION=$(grep ^pkgver .PKGINFO | sed -e 's,^pkgver = ,,' -e 's,-.*$,,')
	fi
	rm -f .INSTALL .MTREE .PKGINFO
done

if [ -d mingw32 ]; then
	for d in bin etc include lib locale share; do
		rm -rf $d
		mv mingw32/$d .
	done
	rmdir mingw32
fi

if [ "$make_zip" = "yes" ]; then
	if [ -z "$VERSION" ]; then
		VERSION="unknown-version"
	fi
	echo "Packing the bundle"
	zip -r gtk-$VERSION.zip bin etc include lib locale share
fi
