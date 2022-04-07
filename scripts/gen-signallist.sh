#!/bin/sh

set -e

HEADER="/* This file is auto-generated, do not edit. */"
TEXT=$(sed -n 's/^.*handler="\([^"]\{1,\}\)".*$/ITEM(\1)/p' "$1" | sort | uniq)

printf "%s\n%s\n" "$HEADER" "$TEXT" > "$2"
