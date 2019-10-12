#!/bin/sh
#
# License: GPL v2 or later
#
# Copyright 2019 Thibault Lindecker (Communication & Systemes) - thibault.lindecker@c-s.fr
#
# Create a dgibi tags file for Geany from '.procedur' files
#

CASTEM_VERSION=19
CASTEM_EXISTE=`which castem${CASTEM_VERSION}`
if [ $? -ne 0 ] ; then
	echo "You need to have Cast3M 20${CASTEM_VERSION} developper edition installed to run this script"
	echo "See http://www-cast3m.cea.fr/index.php"
	exit 1
fi
CASTEM_PROCEDURES="$(dirname ${CASTEM_EXISTE})/../procedur/*"
if [ $? -ne 0 ] ; then
	echo "You need to have Cast3M 20${CASTEM_VERSION} developper edition installed to run this script"
	echo "See http://www-cast3m.cea.fr/index.php"
	exit 1
fi

tmpfile="tmp.dgibi.tags"
tagfile="data/tags/std.dgibi.tags"

# generation du fichier de tags
geany -g ${tmpfile} ${CASTEM_PROCEDURES}

# On ne garde que les tags des procedures (pas ceux des variables)
grep -a "=" ${tmpfile} > ${tagfile}
rm ${tmpfile}
