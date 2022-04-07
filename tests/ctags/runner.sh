#!/bin/bash

# error out on undefined variable expansion, useful for debugging
set -u

GEANY="$1"
TMPDIR=$(mktemp -d) || exit 99
CONFDIR="$TMPDIR/config/"

trap 'rm -rf "$TMPDIR"' EXIT

# make sure we don't use an old or modified system version of the filetype
# related configuration files
mkdir -p "$CONFDIR" || exit 99
mkdir -p "$CONFDIR/filedefs/" || exit 99
cp "${top_srcdir:-../..}"/data/filetype_extensions.conf "$CONFDIR" || exit 99
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

"$GEANY" -c "$CONFDIR" -P -g "$tagfile" "$source" "$@" || exit 1
diff -u "$result" "$tagfile" || exit 2
