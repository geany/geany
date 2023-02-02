#!/bin/sh
#
# Author:  Colomban Wendling <colomban@geany.org>
# License: GPL v2 or later
#
# Updates the `identifiers` entry in data/filetypes.python.in.
# Requires Python 3.10+.

set -e

file=data/filedefs/filetypes.python.in

[ -f "$file" ]

# sort_filter [exclude...]
sort_filter() {
  python3 -c '\
from sys import stdin; \
items=set(stdin.read().strip().split("\n")); \
exclude=['"$(for a in "$@"; do printf "'%s', " "$a"; done)"']; \
print(" ".join(sorted([i for i in items if i not in exclude])))
'
}

keywords=$(python3 -c 'from keyword import kwlist, softkwlist; print("\n".join(kwlist + softkwlist))')
builtins=$(python3 -c 'print("\n".join(dir(__builtins__)))')

primary=$(echo "$keywords" | sort_filter)
# builtins, but excluding keywords that are already listed in primary=
identifiers=$(echo "$builtins" | sort_filter $primary)

sed -e "s/^primary=.*$/primary=$primary/" \
    -e "s/^identifiers=.*$/identifiers=$identifiers/" \
    -i "$file"
