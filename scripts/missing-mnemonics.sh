#!/bin/sh
# Author:	Nick Treleaven
# License:	GPL V2 or later
# Usage:	check-mnemonics.sh [file list]


if [ $# -ge 1 ]; then
	FILES=$@
else
	FILES='src/*.c plugins/*.c'
fi


fgrep -n 'menu_item_new' $FILES |egrep -v '".*_[a-zA-Z0-9]' |fgrep -v from_stock |fgrep -v '_("No custom commands defined.")' |fgrep -vi '_("invisible")' |egrep '_\(".+' --color
