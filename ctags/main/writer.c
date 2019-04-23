/*
*   Copyright (c) 2016, Red Hat, Inc.
*   Copyright (c) 2016, Masatake YAMATO
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*/

#include "general.h"
#include "entry_p.h"
#include "writer_p.h"

/* GEANY DIFF */
/* Dummy definitions of writers as we don't need them */

tagWriter uCtagsWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = NULL,
	.defaultFileName = NULL,
	.private = NULL,
	.type = WRITER_DEFAULT,
};

tagWriter eCtagsWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = NULL,
	.defaultFileName = NULL,
	.private = NULL,
	.type = WRITER_DEFAULT,
};

tagWriter etagsWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = NULL,
	.defaultFileName = NULL,
	.private = NULL,
	.type = WRITER_DEFAULT,
};

tagWriter xrefWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = NULL,
	.defaultFileName = NULL,
	.private = NULL,
	.type = WRITER_DEFAULT,
};

tagWriter jsonWriter = {
	.writeEntry = NULL,
	.writePtagEntry = NULL,
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = NULL,
	.defaultFileName = NULL,
	.private = NULL,
	.type = WRITER_DEFAULT,
};

/* GEANY DIFF END */

static tagWriter *writerTable [WRITER_COUNT] = {
	[WRITER_U_CTAGS] = &uCtagsWriter,
	[WRITER_E_CTAGS] = &eCtagsWriter,
	[WRITER_ETAGS] = &etagsWriter,
	[WRITER_XREF]  = &xrefWriter,
	[WRITER_JSON]  = &jsonWriter,
};

static tagWriter *writer;

extern void setTagWriter (writerType wtype)
{
	writer = writerTable [wtype];
	writer->type = wtype;
}

extern void writerSetup (MIO *mio)
{
	if (writer->preWriteEntry)
		writer->private = writer->preWriteEntry (writer, mio);
	else
		writer->private = NULL;
}

extern bool writerTeardown (MIO *mio, const char *filename)
{
	if (writer->postWriteEntry)
	{
		bool r;
		r = writer->postWriteEntry (writer, mio, filename);
		writer->private = NULL;
		return r;
	}
	return false;
}

extern int writerWriteTag (MIO * mio, const tagEntryInfo *const tag)
{
	return writer->writeEntry (writer, mio, tag);
}

extern int writerWritePtag (MIO * mio,
					 const ptagDesc *desc,
					 const char *const fileName,
					 const char *const pattern,
					 const char *const parserName)
{
	if (writer->writePtagEntry == NULL)
		return -1;

	return writer->writePtagEntry (writer, mio, desc, fileName,
								   pattern, parserName);

}

extern void writerBuildFqTagCache (tagEntryInfo *const tag)
{
	if (writer->buildFqTagCache)
		writer->buildFqTagCache (writer, tag);
}


extern bool ptagMakeCtagsOutputMode (ptagDesc *desc, void *data CTAGS_ATTR_UNUSED)
{
	const char *mode ="";

	if (&uCtagsWriter == writer)
		mode = "u-ctags";
	else if (&eCtagsWriter == writer)
		mode = "e-ctags";

	return writePseudoTag (desc,
						   mode,
						   "u-ctags or e-ctags",
						   NULL);
}

extern const char *outputDefaultFileName (void)
{
	return writer->defaultFileName;
}

extern bool writerCanPrintPtag (void)
{
	return (writer->writePtagEntry)? true: false;
}
