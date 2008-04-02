#!/bin/sh

# Little shell script to display some basic statistics about Geany's translation
# files. It also checks the menu accelerators.
#
# Copyright 2007-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
# Copyright 2007-2008 Frank Lanitz <enrico(dot)troeger(at)uvena(dot)de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.


check_accelerators=""
linguas=""
me=`basename $0`


usage()
{
	echo "usage: $me [OPTION] [languages...]"
	echo
	echo "OPTIONs are:"
	echo "-h --help           this help screen"
  	echo "-a --accelerators   check also for menu accelerators"
  	echo "languages           list of language codes which should be tested"
  	echo
  	echo "example: $me -a de fr hu"
}


# parse cmd line arguments
while [ $# -gt 0 ]
do
	case $1 in
		--accelerators)
			check_accelerators="--check-accelerators=_"
			;;
		-a)
		  check_accelerators="--check-accelerators=_"
		  ;;
		--help)
		  usage;
		  exit 1;
		  ;;
		-h)
		  usage;
		  exit 1;
		  ;;
		*)
		  linguas="$linguas "$1
		  ;;
	esac
	shift
done



# if no languages where specified on the command line, take all listed languages in LINGUAS
if [ -z "$linguas" ]
then
	linguas=`sed -e '/^#/d' po/LINGUAS`
fi

# dash and zsh don't need the -e switch to echo, bash does
# maybe there is a better way to detect whether we are running on a bash
eswitch=""
if [ "$BASH" ]
then
	eswitch="-e"
fi

# do the work
if [ $check_accelerators ]
then
   for lang in $linguas
   do
     msgfmt --check --statistics $check_accelerators po/$lang.po;
   done
else
   for lang in $linguas
   do
     # maybe the regexp can be optimized, regexps are not my best friends
     creationdate=`grep "PO-Revision-Date:" po/$lang.po | sed 's/.*: \([0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\} [0-9]\{2\}:[0-9]\{2\}[+|-][0-9]\{4\}\).*/\1/'`
     echo -n $eswitch $lang"\t("$creationdate"):\t"
     msgfmt --check --statistics po/$lang.po;
   done
fi


