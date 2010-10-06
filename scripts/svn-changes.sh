#!/bin/sh
# Copyright:	2008, Nick Treleaven
# License:		GNU GPL V2 or later
# Warranty:		NONE

# Displays a summary of Subversion working copy changes in ChangeLog
# format, plus warnings about any unknown files.

# -s for spaces instead of comma separation
if [ "$1" = -s ]; then
	SPACES="set"
	shift
fi

# -q to not print warnings
if [ "$1" = -q ]; then
	QUIET="set"
	shift
fi

status=`svn st $*`

# get list of files changed.
# remove extraneous text, e.g. ? entries
files=`echo "$status" |egrep '^[A-Z]'`
# get filenames on one line
files=`echo "$files" |egrep -o '[^A-Z].[ ]+(.+)' |xargs`
# remove ChangeLog
files=`echo "$files" |sed "s/ ChangeLog\b//"`
# add commas if -s argument is not given
if [ -z "$SPACES" ]; then
	files=`echo "$files" |sed "s/ /, /g"`
fi

# show modifications
if [ -n "$files" ]; then
	echo 'Changes:'
	if [ -z $SPACES ]; then
		files="${files}:"
	fi
	# indent and wrap
	OUTFILE=/tmp/fmt
	echo -n '   '$files | fmt -w 72 >$OUTFILE
	# put ' * ' for first line
	cat $OUTFILE | sed '1s/   / * /'
else
	echo 'No changes.'
fi

# warn about anything that isn't a modification or addition
if [ -n "$QUIET" ]; then
	exit
fi
warn=`echo "$status" |egrep '^[^MA]'`
if [ -n "$warn" ]; then
	echo 'Warnings:'
	echo $warn
else
	echo 'No warnings.'
fi
