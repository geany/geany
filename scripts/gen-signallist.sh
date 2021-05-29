#!/bin/sh

glade_file=$1
output=$2

sed -n 's/^.*handler="\([^"]\{1,\}\)".*$/ITEM(\1)/p' "$glade_file" | sort | uniq > "$output"
