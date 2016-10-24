/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to entry.c
*/

#include "general.h"  /* must always come first */

#include "entry.h"
#include "mio.h"
#include "options.h"
#include "output.h"
#include "read.h"
#include "ptag.h"

/* GEANY DIFF */
/* Dummy definitions of writers as we don't need them */

tagWriter etagsWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.useStdoutByDefault = false,
};

tagWriter ctagsWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.useStdoutByDefault = false,
};

tagWriter xrefWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.useStdoutByDefault = false,
};

tagWriter jsonWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.useStdoutByDefault = false,
};

extern bool ptagMakeJsonOutputVersion (ptagDesc *desc, void *data CTAGS_ATTR_UNUSED)
{
	return false;
}

/* GEANY DIFF END */
