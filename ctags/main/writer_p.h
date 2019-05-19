/*
*   Copyright (c) 2016, Red Hat, Inc.
*   Copyright (c) 2016, Masatake YAMATO
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*/
#ifndef CTAGS_MAIN_WRITER_PRIVATE_H
#define CTAGS_MAIN_WRITER_PRIVATE_H

#include "general.h"  /* must always come first */
#include "mio.h"
#include "types.h"

/* Other than writeEntry can be NULL.
   The value returned from preWriteEntry is passed to writeEntry,
   and postWriteEntry. If a resource is allocated in
   preWriteEntry, postWriteEntry should free it. */

typedef enum eWriterType {
	WRITER_DEFAULT,
	WRITER_U_CTAGS = WRITER_DEFAULT,
	WRITER_E_CTAGS,
	WRITER_ETAGS,
	WRITER_XREF,
	WRITER_JSON,
	WRITER_CUSTOM,
	WRITER_COUNT,
} writerType;

struct sTagWriter;
typedef struct sTagWriter tagWriter;
struct sTagWriter {
	int (* writeEntry) (tagWriter *writer, MIO * mio, const tagEntryInfo *const tag,
						void *clientData);
	int (* writePtagEntry) (tagWriter *writer, MIO * mio, const ptagDesc *desc,
							const char *const fileName,
							const char *const pattern,
							const char *const parserName,
							void *clientData);
	void * (* preWriteEntry) (tagWriter *writer, MIO * mio,
							  void *clientData);

	/* Returning TRUE means the output file may be shrunk.
	   In such case the callee may do truncate output file. */
	bool (* postWriteEntry)  (tagWriter *writer, MIO * mio, const char* filename,
							  void *clientData);
	void (* rescanFailedEntry) (tagWriter *writer, unsigned long validTagNum,
								void *clientData);
	bool (* treatFieldAsFixed) (int fieldType);
	const char *defaultFileName;

	/* The value returned from preWriteEntry is stored `private' field.
	   The value must be released in postWriteEntry. */
	void *private;
	writerType type;
	/* The value passed as the second argument for writerSetup iss
	 * stored here. Unlink `private' field, ctags does nothing more. */
	void *clientData;
};

/* customWriter is used only if otype is WRITER_CUSTOM */
extern void setTagWriter (writerType otype, tagWriter *customWriter);
extern void writerSetup  (MIO *mio, void *clientData);
extern bool writerTeardown (MIO *mio, const char *filename);

int writerWriteTag (MIO * mio, const tagEntryInfo *const tag);
int writerWritePtag (MIO * mio,
					 const ptagDesc *desc,
					 const char *const fileName,
					 const char *const pattern,
					 const char *const parserName);

void writerRescanFailed (unsigned long validTagNum);

extern const char *outputDefaultFileName (void);

extern void truncateTagLineAfterTag (char *const line, const char *const token,
			     const bool discardNewline);
extern void abort_if_ferror(MIO *const fp);

extern bool ptagMakeJsonOutputVersion (ptagDesc *desc, void *data CTAGS_ATTR_UNUSED);
extern bool ptagMakeCtagsOutputMode (ptagDesc *desc, void *data CTAGS_ATTR_UNUSED);

extern bool writerCanPrintPtag (void);
extern bool writerDoesTreatFieldAsFixed (int fieldType);

#endif	/* CTAGS_MAIN_WRITER_PRIVATE_H */
