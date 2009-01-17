#!/bin/sh
#
# Author:  Enrico Tröger
# License: GPL v2 or later
#
# Create a C tags file for Geany from C header files
#

tmpfile="tmp.c.tags"
tagfile="data/c99.tags"

headers="\
assert.h \
complex.h \
ctype.h \
errno.h \
fenv.h \
float.h \
inttypes.h \
iso646.h \
limits.h \
locale.h \
math.h \
setjmp.h \
signal.h \
stdarg.h \
stdbool.h \
stddef.h \
stdint.h \
stdio.h \
stdlib.h \
string.h \
time.h \
wchar.h \
wctype.h"


# generate the tags file with Geany
geany -g "$tmpfile" $headers || exit 1

# remove any tags beginning with an underscrore
grep -v '^_' "$tmpfile" > "$tagfile"

rm "$tmpfile"
