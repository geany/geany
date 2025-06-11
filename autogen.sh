#!/bin/sh
# Run this to generate all the initial makefiles, etc.

set -e

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

DIE=0

(test -f "$srcdir/configure.ac") || {
	echo "**Error**: Directory "\`$srcdir\'" does not look like the top-level package directory"
	exit 1
}

(pkg-config --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have pkg-config installed to compile $package."
        echo "Download the appropriate package for your distribution."
               result="no"
        DIE=1
}

if test -z "$*" -a "$NOCONFIGURE" != 1; then
	echo "**Warning**: I am going to run \`configure' with no arguments."
	echo "If you wish to pass any to it, please specify them on the"
	echo \`$0\'" command line."
	echo
fi

echo "Processing configure.ac"

(cd "$srcdir"; autoreconf --install --verbose)

if [ "$NOCONFIGURE" = 1 ]; then
    echo "Done. configure skipped."
    exit 0;
fi
echo "Running $srcdir/configure $@ ..."
"$srcdir/configure" "$@" && echo "Now type 'make' to compile." || exit 1
