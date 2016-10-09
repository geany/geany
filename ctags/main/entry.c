/*
*   Copyright (c) 1996-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for creating tag entries.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>
#include <ctype.h>        /* to define isspace () */
#include <errno.h>

#if defined (HAVE_SYS_TYPES_H)
# include <sys/types.h>	  /* to declare off_t on some hosts */
#endif
#if defined (HAVE_TYPES_H)
# include <types.h>       /* to declare off_t on some hosts */
#endif
#if defined (HAVE_UNISTD_H)
# include <unistd.h>      /* to declare close (), ftruncate (), truncate () */
#endif

/*  These header files provide for the functions necessary to do file
 *  truncation.
 */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif

#include "debug.h"
#include "entry.h"
#include "field.h"
#include "fmt.h"
#include "kind.h"
#include "main.h"
#include "options.h"
#include "output.h"
#include "ptag.h"
#include "read.h"
#include "routines.h"
#include "sort.h"
#include "strlist.h"
#include "xtag.h"

/*
*   MACROS
*/

/*
 *  Portability defines
 */
#if !defined(HAVE_TRUNCATE) && !defined(HAVE_FTRUNCATE) && !defined(HAVE_CHSIZE)
# define USE_REPLACEMENT_TRUNCATE
#endif

/*  Hack for ridiculous practice of Microsoft Visual C++.
 */
#if defined (WIN32) && defined (_MSC_VER)
# define chsize         _chsize
# define open           _open
# define close          _close
# define O_RDWR         _O_RDWR
#endif


/*  Maintains the state of the tag file.
 */
typedef struct eTagFile {
	char *name;
	char *directory;
	MIO *mio;
	struct sNumTags { unsigned long added, prev; } numTags;
	struct sMax { size_t line, tag; } max;
	vString *vLine;

	unsigned int cork;
	struct sCorkQueue {
		struct sTagEntryInfo* queue;
		unsigned int length;
		unsigned int count;
	} corkQueue;

	bool patternCacheValid;
} tagFile;

/*
*   DATA DEFINITIONS
*/

tagFile TagFile = {
    NULL,               /* tag file name */
    NULL,               /* tag file directory (absolute) */
    NULL,               /* file pointer */
    { 0, 0 },           /* numTags */
    { 0, 0 },        /* max */
    NULL,                /* vLine */
    .cork = false,
    .corkQueue = {
	    .queue = NULL,
	    .length = 0,
	    .count  = 0
    },
    .patternCacheValid = false,
};

static bool TagsToStdout = false;

/*
*   FUNCTION PROTOTYPES
*/
#ifdef NEED_PROTO_TRUNCATE
extern int truncate (const char *path, off_t length);
#endif

#ifdef NEED_PROTO_FTRUNCATE
extern int ftruncate (int fd, off_t length);
#endif

/*
*   FUNCTION DEFINITIONS
*/

extern void freeTagFileResources (void)
{
	if (TagFile.directory != NULL)
		eFree (TagFile.directory);
	vStringDelete (TagFile.vLine);
}

extern const char *tagFileName (void)
{
	return TagFile.name;
}

/*
*   Pseudo tag support
*/

extern void abort_if_ferror(MIO *const mio)
{
	if (mio_error (mio))
		error (FATAL | PERROR, "cannot write tag file");
}

static void rememberMaxLengths (const size_t nameLength, const size_t lineLength)
{
	if (nameLength > TagFile.max.tag)
		TagFile.max.tag = nameLength;

	if (lineLength > TagFile.max.line)
		TagFile.max.line = lineLength;
}

static void addCommonPseudoTags (void)
{
	int i;

	for (i = 0; i < PTAG_COUNT; i++)
	{
		if (isPtagCommonInParsers (i))
			makePtagIfEnabled (i, NULL);
	}
}

extern void makeFileTag (const char *const fileName)
{
	xtagType     xtag = XTAG_UNKNOWN;

	if (isXtagEnabled(XTAG_FILE_NAMES))
		xtag = XTAG_FILE_NAMES;

	if (xtag != XTAG_UNKNOWN)
	{
		tagEntryInfo tag;
		kindOption  *kind;

		kind = getInputLanguageFileKind();
		Assert (kind);
		kind->enabled = isXtagEnabled(XTAG_FILE_NAMES);

		/* TODO: you can return here if enabled == false. */

		initTagEntry (&tag, baseFilename (fileName), kind);

		tag.isFileEntry     = true;
		tag.lineNumberEntry = true;
		markTagExtraBit (&tag, xtag);

		tag.lineNumber = 1;
		if (isFieldEnabled (FIELD_END))
		{
			/* isFieldEnabled is called again in the rendering
			   stage. However, it is called here for avoiding
			   unnecessary read line loop. */
			while (readLineFromInputFile () != NULL)
				; /* Do nothing */
			tag.extensionFields.endLine = getInputLineNumber ();
		}

		makeTagEntry (&tag);
	}
}

static void updateSortedFlag (
		const char *const line, MIO *const mio, MIOPos startOfLine)
{
	const char *const tab = strchr (line, '\t');

	if (tab != NULL)
	{
		const long boolOffset = tab - line + 1;  /* where it should be */

		if (line [boolOffset] == '0'  ||  line [boolOffset] == '1')
		{
			MIOPos nextLine;

			if (mio_getpos (mio, &nextLine) == -1 || mio_setpos (mio, &startOfLine) == -1)
				error (WARNING, "Failed to update 'sorted' pseudo-tag");
			else
			{
				MIOPos flagLocation;
				int c, d;

				do
					c = mio_getc (mio);
				while (c != '\t'  &&  c != '\n');
				mio_getpos (mio, &flagLocation);
				d = mio_getc (mio);
				if (c == '\t'  &&  (d == '0'  ||  d == '1')  &&
					d != (int) Option.sorted)
				{
					mio_setpos (mio, &flagLocation);
					mio_putc (mio, Option.sorted == SO_FOLDSORTED ? '2' :
						(Option.sorted == SO_SORTED ? '1' : '0'));
				}
				mio_setpos (mio, &nextLine);
			}
		}
	}
}

/*  Look through all line beginning with "!_TAG_FILE", and update those which
 *  require it.
 */
static long unsigned int updatePseudoTags (MIO *const mio)
{
	enum { maxEntryLength = 20 };
	char entry [maxEntryLength + 1];
	unsigned long linesRead = 0;
	MIOPos startOfLine;
	size_t entryLength;
	const char *line;

	sprintf (entry, "%sTAG_FILE", PSEUDO_TAG_PREFIX);
	entryLength = strlen (entry);
	Assert (entryLength < maxEntryLength);

	mio_getpos (mio, &startOfLine);
	line = readLineRaw (TagFile.vLine, mio);
	while (line != NULL  &&  line [0] == entry [0])
	{
		++linesRead;
		if (strncmp (line, entry, entryLength) == 0)
		{
			char tab, classType [16];

			if (sscanf (line + entryLength, "%15s%c", classType, &tab) == 2  &&
				tab == '\t')
			{
				if (strcmp (classType, "_SORTED") == 0)
					updateSortedFlag (line, mio, startOfLine);
			}
			mio_getpos (mio, &startOfLine);
		}
		line = readLineRaw (TagFile.vLine, mio);
	}
	while (line != NULL)  /* skip to end of file */
	{
		++linesRead;
		line = readLineRaw (TagFile.vLine, mio);
	}
	return linesRead;
}

/*
 *  Tag file management
 */



static bool isTagFile (const char *const filename)
{
	bool ok = false;  /* we assume not unless confirmed */
	MIO *const mio = mio_new_file (filename, "rb");

	if (mio == NULL  &&  errno == ENOENT)
		ok = true;
	else if (mio != NULL)
	{
		const char *line = readLineRaw (TagFile.vLine, mio);

		if (line == NULL)
			ok = true;
		mio_free (mio);
	}
	return ok;
}

extern void openTagFile (void)
{
	setDefaultTagFileName ();
	TagsToStdout = isDestinationStdout ();

	if (TagFile.vLine == NULL)
		TagFile.vLine = vStringNew ();

	/*  Open the tags file.
	 */
	if (TagsToStdout)
	{
		/* Open a tempfile with read and write mode. Read mode is used when
		 * write the result to stdout. */
		TagFile.mio = tempFile ("w+", &TagFile.name);
		if (isXtagEnabled (XTAG_PSEUDO_TAGS))
			addCommonPseudoTags ();
	}
	else
	{
		bool fileExists;

		TagFile.name = eStrdup (Option.tagFileName);
		fileExists = doesFileExist (TagFile.name);
		if (fileExists  &&  ! isTagFile (TagFile.name))
			error (FATAL,
			  "\"%s\" doesn't look like a tag file; I refuse to overwrite it.",
				  TagFile.name);

		if (Option.etags)
		{
			if (Option.append  &&  fileExists)
				TagFile.mio = mio_new_file (TagFile.name, "a+b");
			else
				TagFile.mio = mio_new_file (TagFile.name, "w+b");
		}
		else
		{
			if (Option.append  &&  fileExists)
			{
				TagFile.mio = mio_new_file (TagFile.name, "r+");
				if (TagFile.mio != NULL)
				{
					TagFile.numTags.prev = updatePseudoTags (TagFile.mio);
					mio_free (TagFile.mio);
					TagFile.mio = mio_new_file (TagFile.name, "a+");
				}
			}
			else
			{
				TagFile.mio = mio_new_file (TagFile.name, "w");
				if (TagFile.mio != NULL && isXtagEnabled (XTAG_PSEUDO_TAGS))
					addCommonPseudoTags ();
			}
		}
		if (TagFile.mio == NULL)
			error (FATAL | PERROR, "cannot open tag file");
	}
	if (TagsToStdout)
		TagFile.directory = eStrdup (CurrentDirectory);
	else
		TagFile.directory = absoluteDirname (TagFile.name);
}

#ifdef USE_REPLACEMENT_TRUNCATE

static void copyBytes (MIO* const fromMio, MIO* const toMio, const long size)
{
	enum { BufferSize = 1000 };
	long toRead, numRead;
	char* buffer = xMalloc (BufferSize, char);
	long remaining = size;
	do
	{
		toRead = (0 < remaining && remaining < BufferSize) ?
					remaining : (long) BufferSize;
		numRead = mio_read (fromMio, buffer, (size_t) 1, (size_t) toRead);
		if (mio_write (toMio, buffer, (size_t)1, (size_t)numRead) < (size_t)numRead)
			error (FATAL | PERROR, "cannot complete write");
		if (remaining > 0)
			remaining -= numRead;
	} while (numRead == toRead  &&  remaining != 0);
	eFree (buffer);
}

static void copyFile (const char *const from, const char *const to, const long size)
{
	MIO* const fromMio = mio_new_file (from, "rb");
	if (fromMio == NULL)
		error (FATAL | PERROR, "cannot open file to copy");
	else
	{
		MIO* const toMio = mio_new_file (to, "wb");
		if (toMio == NULL)
			error (FATAL | PERROR, "cannot open copy destination");
		else
		{
			copyBytes (fromMio, toMio, size);
			mio_free (toMio);
		}
		mio_free (fromMio);
	}
}

/*  Replacement for missing library function.
 */
static int replacementTruncate (const char *const name, const long size)
{
	char *tempName = NULL;
	MIO *mio = tempFile ("w", &tempName);
	mio_free (mio);
	copyFile (name, tempName, size);
	copyFile (tempName, name, WHOLE_FILE);
	remove (tempName);
	eFree (tempName);

	return 0;
}

#endif

/*
 *  Tag entry management
 */

static void writeTagEntry (const tagEntryInfo *const tag)
{
	int length = 0;

/*	if (tag->placeholder)
		return;
	if (! tag->kind->enabled)
		return;
	if (tag->extensionFields.roleIndex != ROLE_INDEX_DEFINITION
	    && ! isXtagEnabled (XTAG_REFERENCE_TAGS))
		return;
*/
	DebugStatement ( debugEntry (tag); )
/*	Assert (writer); */
/*
	if (includeExtensionFlags ()
	    && isXtagEnabled (XTAG_QUALIFIED_TAGS)
	    && doesInputLanguageRequestAutomaticFQTag ())
		buildFqTagCache (tag);
*/
	/* length = writer->writeEntry (TagFile.mio, tag, writerData); */
	if (TagEntryFunction != NULL)
		length = TagEntryFunction(tag, TagEntryUserData);

	++TagFile.numTags.added;
	rememberMaxLengths (strlen (tag->name), (size_t) length);
	DebugStatement ( mio_flush (TagFile.mio); )

	/*abort_if_ferror (TagFile.mio);*/
}

extern int makeTagEntry (const tagEntryInfo *const tag)
{
	int r = CORK_NIL;
	Assert (tag->name != NULL);

/*
	if (getInputLanguageFileKind() != tag->kind)
	{
		if (! isInputLanguageKindEnabled (tag->kind->letter) &&
		    (tag->extensionFields.roleIndex == ROLE_INDEX_DEFINITION))
			return CORK_NIL;
		if ((tag->extensionFields.roleIndex != ROLE_INDEX_DEFINITION)
		    && (! tag->kind->roles[tag->extensionFields.roleIndex].enabled))
			return CORK_NIL;
	}
*/

	if (tag->name [0] == '\0' && (!tag->placeholder))
	{
/*		if (!doesInputLanguageAllowNullTag()) */
			error (WARNING, "ignoring null tag in %s(line: %lu)",
			       getInputFileName (), tag->lineNumber);
		goto out;
	}

/*	if (TagFile.cork)
		r = queueTagEntry (tag);
	else*/
		writeTagEntry (tag);
out:
	return r;
}

extern void initTagEntry (tagEntryInfo *const e, const char *const name, const kindOption *kind)
{
	Assert (File.source.name != NULL);
	memset (e, 0, sizeof (tagEntryInfo));
	e->lineNumberEntry  = (bool) (Option.locate == EX_LINENUM);
	e->lineNumber       = getSourceLineNumber ();
	e->language         = getSourceLanguageName ();
	e->filePosition     = getInputFilePosition ();
	e->sourceFileName   = getSourceFileTagPath ();
	e->name             = name;
	e->kind             = kind;
}

extern void setTagWriter (tagWriter *t)
{
/*	writer = t; */
}

extern void    markTagExtraBit     (tagEntryInfo *const tag, xtagType extra)
{
	unsigned int index;
	unsigned int offset;

	Assert (extra < XTAG_COUNT);
	Assert (extra != XTAG_UNKNOWN);

	index = (extra / 8);
	offset = (extra % 8);
	tag->extra [ index ] |= (1 << offset);
}

extern unsigned long numTagsAdded(void)
{
	return TagFile.numTags.added;
}

extern void setNumTagsAdded (unsigned long nadded)
{
	TagFile.numTags.added = nadded;
}

extern unsigned long numTagsTotal(void)
{
	return TagFile.numTags.added + TagFile.numTags.prev;
}

extern unsigned long maxTagsLine (void)
{
	return (unsigned long)TagFile.max.line;
}

extern void setMaxTagsLine (unsigned long max)
{
	TagFile.max.line = max;
}

extern void invalidatePatternCache(void)
{
	TagFile.patternCacheValid = false;
}

extern void tagFilePosition (MIOPos *p)
{
	mio_getpos (TagFile.mio, p);
}

extern void setTagFilePosition (MIOPos *p)
{
	mio_setpos (TagFile.mio, p);
}

extern const char* getTagFileDirectory (void)
{
	return TagFile.directory;
}
