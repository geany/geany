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

py2_builtins=$(python2 -c 'print("\n".join(dir(__builtins__)))')
py3_builtins=$(python3 -c 'print("\n".join(dir(__builtins__)))')

# merge Python 2 and 3 keywords but exclude the ones that are already listed primary=
identifiers=$( (echo "$py2_builtins" && echo "$py3_builtins") | python -c '\
from sys import stdin; \
builtins=set(stdin.read().strip().split("\n")); \
exclude=["False", "None", "True", "exec"]; \
print(" ".join(sorted([i for i in builtins if i not in exclude])))
')

sed -e "s/^identifiers=.*$/identifiers=$identifiers/" -i "$file"
