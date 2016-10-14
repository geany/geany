/*
*   Copyright (c) 1996-2003, Darren Hiebert
*
*   Author: Darren Hiebert <dhiebert@users.sourceforge.net>
*           http://ctags.sourceforge.net
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*   It is provided on an as-is basis and no responsibility is accepted for its
*   failure to perform as expected.
*
*   This is a reimplementation of the ctags (1) program. It is an attempt to
*   provide a fully featured ctags program which is free of the limitations
*   which most (all?) others are subject to.
*
*   This module contains the start-up code and routines to determine the list
*   of files to parsed for tags.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>

#include "options.h"
#include "output.h"


extern void addTotals (
		const unsigned int files, const long unsigned int lines,
		const long unsigned int bytes)
{
}

extern bool isDestinationStdout (void)
{
	bool toStdout = false;

	if (outpuFormatUsedStdoutByDefault() ||  Option.filter  ||
		(Option.tagFileName != NULL  &&  (strcmp (Option.tagFileName, "-") == 0
						  || strcmp (Option.tagFileName, "/dev/stdout") == 0
		)))
		toStdout = true;
	return toStdout;
}
