#!/bin/sh
#
# Author:  Colomban Wendling <colomban@geany.org>
# License: GPL v2 or later
#
# Updates the `identifiers` entry in data/filetypes.python.
# Requires both Python 2 and 3.

set -e

file=data/filetypes.python

[ -f "$file" ]

py_2_and_3() {
  python2 "$@" && python3 "$@"
}

# sort_filter [exclude...]
sort_filter() {
  python -c '\
from sys import stdin; \
items=set(stdin.read().strip().split("\n")); \
exclude=['"$(for a in "$@"; do printf "'%s', " "$a"; done)"']; \
print(" ".join(sorted([i for i in items if i not in exclude])))
'
}

keywords=$(py_2_and_3 -c 'from keyword import kwlist; print("\n".join(kwlist))')
builtins=$(py_2_and_3 -c 'print("\n".join(dir(__builtins__)))')

primary=$(echo "$keywords" | sort_filter)
# builtins, but excluding keywords that are already listed in primary=
identifiers=$(echo "$builtins" | sort_filter $primary)

sed -e "s/^primary=.*$/primary=$primary/" \
    -e "s/^identifiers=.*$/identifiers=$identifiers/" \
    -i "$file"
