#!/bin/bash

# error out on undefined variable expansion, useful for debugging
set -u

GEANY="$1"
PRINTER="${top_srcdir:-../..}"/scripts/print-tags.py
TMPDIR=$(mktemp -d) || exit 99
CONFDIR="$TMPDIR/config/"

trap 'rm -rf "$TMPDIR"' EXIT

# make sure we don't use an old or modified system version of the filetype
# related configuration files
mkdir -p "$CONFDIR" || exit 99
mkdir -p "$CONFDIR/filedefs/" || exit 99
# Add *.Filetype_unittest extension so we can match filetypes for which there
# are no extension patterns, like e.g. Meson.
sed 's/^\([^=[]\{1,\}\)\(=[^;]\{1,\}\(;[^;]\{1,\}\)*\);*$/\1\2;*.\1_unittest;/' \
  < "${top_srcdir:-../..}"/data/filetype_extensions.conf > "$CONFDIR/filetype_extensions.conf" || exit 99
cp "${top_srcdir:-../..}"/data/filedefs/filetypes.* "$CONFDIR/filedefs/" || exit 99

shift
if [ "$1" = "--result" ]; then
  # --result $result $source...
  [ $# -gt 2 ] || exit 99
  shift
  result="$1"
  shift
  source="$1"
else
  # result is $1 and source is inferred from result
  result="$1"
  source="${result%.*}"
fi
shift

tagfile="$TMPDIR/test.${source##*.}.tags"
outfile="$TMPDIR/test.${source##*.}.out"

"$GEANY" -c "$CONFDIR" -P -g "$tagfile" "$source" "$@" || exit 1
cat "$tagfile" | "$PRINTER" > "$outfile" || exit 3
diff -u "$result" "$outfile" || exit 2
