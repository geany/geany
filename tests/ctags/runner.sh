#!/bin/sh

# error out on undefined variable expansion, useful for debugging
set -u

# FIXME: get this from automake so we have $(EXEEXT)
GEANY="${top_builddir:-../..}/src/geany"
TMPDIR=$(mktemp -d) || exit 99
CONFDIR="$TMPDIR/config/"

trap 'rm -rf "$TMPDIR"' EXIT

# make sure we don't use an old or modified system version of the filetype
# related configuration files
mkdir -p "$CONFDIR" || exit 99
mkdir -p "$CONFDIR/filedefs/" || exit 99
cp "${srcdir:-.}"/../../data/filetype_extensions.conf "$CONFDIR" || exit 99
cp "${srcdir:-.}"/../../data/filedefs/filetypes.* "$CONFDIR/filedefs/" || exit 99

result="$1"
source="${result%.*}"
tagfile="$TMPDIR/test.${source##*.}.tags"

"$GEANY" -c "$CONFDIR" -P -g "$tagfile" "$source" || exit 1
diff -u "$result" "$tagfile" || exit 2
