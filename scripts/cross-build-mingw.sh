#!/bin/sh
# A script to automate setup and build for Windows cross-compilation
#
# What it does:
# 1) prepare a build directory
# 2) download and unpacked the dependencies
#    (see http://www.gtk.org/download/win32.php)
# 2.1) fixup the unpacked pkg-config file paths
# 3) setup the few required paths
# 4) configure with sensible options for this
# 5) build
# 6) install in a local directory
# 7) pack the installation in a ZIP file, ready to be used
#    (but does not pack the dependencies)

# You may change those
HOST=i686-w64-mingw32
GTK2_BUNDLE_ZIP="https://download.geany.org/contrib/gtk/gtk+-bundle_2.24.10-20120208_win32.zip"
GTK3_BUNDLE_ZIP="https://download.geany.org/contrib/gtk/gtk+-bundle_3.8.2-20131001_win32.zip"
BUILDDIR=_build-cross-mingw
GTK3=no
CONFIGUREFLAGS="--enable-nls"
MAKEFLAGS="${MAKEFLAGS:--j2}"

while getopts '32b:h' o; do
  case "$o" in
    3) GTK3=yes;;
    2) GTK3=no;;
    b) BUILDDIR="$OPTARG";;
    h)
      cat <<EOF
USAGE: $0 [-2|-3] [-b DIR] [-h]

-2      Build against GTK2
-3      Build against GTK3
-b DIR  Use DIR as build directory
-h      Show this help and exit
EOF
      exit 0;;
    *) echo "Invalid option $o (see -h)">&2; exit 1;;
  esac
done
shift $((OPTIND - 1))

CONFIGUREFLAGS="$CONFIGUREFLAGS --enable-gtk3=$GTK3"
if [ "$GTK3" = yes ]; then
  BUNDLE_ZIP="$GTK3_BUNDLE_ZIP"
else
  BUNDLE_ZIP="$GTK2_BUNDLE_ZIP"
fi

# USAGE: fetch_and_unzip URL DEST_PREFIX
fetch_and_unzip()
{
  local basename=${1##*/}
  curl -L -# "$1" > "$basename"
  unzip -qn "$basename" -d "$2"
  rm -f "$basename"
}

if test -d "$BUILDDIR"; then
  cat >&2 <<EOF
** Directory "$BUILDDIR/" already exists.
If it was created by this tool and just want to build, simply run make:
  $ make -C "$BUILDDIR/_build/"

If however you want to recreate it, please remove it first:
  $ rm -rf "$BUILDDIR/"
EOF
  exit 1
fi

set -e
set -x


test -f configure
# check if the host tools are available, because configure falls back
# on default non-prefixed tools if they are missing, and it can spit
# quite a lot of subtle errors.  also avoids going further if something
# is obviously missing.
type "$HOST-gcc"

SRCDIR="$PWD"

mkdir "$BUILDDIR"
cd "$BUILDDIR"

mkdir _deps
fetch_and_unzip "$GTK3_BUNDLE_ZIP" _deps
[ "$GTK3" = yes ] || fetch_and_unzip "$BUNDLE_ZIP" _deps
# fixup the prefix= in the pkg-config files
sed -i "s%^\(prefix=\).*$%\1$PWD/_deps%" _deps/lib/pkgconfig/*.pc

export PKG_CONFIG_PATH="$PWD/_deps/lib/pkgconfig/"
export CPPFLAGS="-I$PWD/_deps/include"
export LDFLAGS="-L$PWD/_deps/lib"

mkdir _build
cd _build
"$SRCDIR/configure" \
  --host=$HOST \
  --disable-silent-rules \
  --prefix="$PWD/../_install" \
  $CONFIGUREFLAGS || {
  cat config.log
  exit 1
}
make $MAKEFLAGS
make $MAKEFLAGS install
