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
#include "fmt_p.h"
#include "mio.h"
#include "options_p.h"
#include "writer_p.h"


static int writeXrefEntry  (tagWriter *writer CTAGS_ATTR_UNUSED,
							MIO * mio, const tagEntryInfo *const tag);
static void buildXrefFqTagCache (tagWriter *writer, tagEntryInfo *const tag);

tagWriter xrefWriter = {
	.writeEntry = writeXrefEntry,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = buildXrefFqTagCache,
	.defaultFileName = NULL,
};

static int writeXrefEntry (tagWriter *writer CTAGS_ATTR_UNUSED,
						   MIO * mio, const tagEntryInfo *const tag)
{
	int length;
	static fmtElement *fmt1;
	static fmtElement *fmt2;

	if (Option.customXfmt)
		length = fmtPrint (Option.customXfmt, mio, tag);
	else
	{
		if (tag->isFileEntry)
			return 0;

		if (Option.tagFileFormat == 1)
		{
			if (fmt1 == NULL)
				fmt1 = fmtNew ("%-16N %4n %-16F %C");
			length = fmtPrint (fmt1, mio, tag);
		}
		else
		{
			if (fmt2 == NULL)
				fmt2 = fmtNew ("%-16N %-10K %4n %-16F %C");
			length = fmtPrint (fmt2, mio, tag);
		}
	}

	mio_putc (mio, '\n');
	length++;

	return length;
}

static void buildXrefFqTagCache (tagWriter *writer, tagEntryInfo *const tag)
{
	renderFieldEscaped (writer->type, FIELD_SCOPE_KIND_LONG, tag,
			    NO_PARSER_FIELD, NULL);
	renderFieldEscaped (writer->type, FIELD_SCOPE, tag,
			    NO_PARSER_FIELD, NULL);
}
