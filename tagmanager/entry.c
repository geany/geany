/*
*
*   Copyright (c) 1996-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for creating tag entries.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>
#include <ctype.h>	/* to define isspace () */
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>

#if defined (HAVE_SYS_TYPES_H)
# include <sys/types.h>	    /* to declare off_t on some hosts */
#endif
#if defined (HAVE_TYPES_H)
# include <types.h>	    /* to declare off_t on some hosts */
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

#include "ctags.h"
#include "entry.h"
#include "main.h"
#include "options.h"
#include "read.h"
#include "sort.h"
#include "strlist.h"

/*
*   MACROS
*/
#define PSEUDO_TAG_PREFIX	"!_"

#define includeExtensionFlags()		(Option.tagFileFormat > 1)

/*
 *  Portability defines
 */
#if !defined(HAVE_TRUNCATE) && !defined(HAVE_FTRUNCATE) && !defined(HAVE_CHSIZE)
# define USE_REPLACEMENT_TRUNCATE
#endif

/*  Hack for rediculous practice of Microsoft Visual C++.
 */
#if defined (WIN32) && defined (_MSC_VER)
# define chsize		_chsize
# define open		_open
# define close		_close
# define O_RDWR 	_O_RDWR
#endif

/*
*   DATA DEFINITIONS
*/

tagFile TagFile = {
    NULL,		/* tag file name */
    NULL,		/* tag file directory (absolute) */
    NULL,		/* file pointer */
    { 0, 0 },		/* numTags */
    { 0, 0, 0 },	/* max */
    { NULL, NULL, 0 },	/* etags */
    NULL		/* vLine */
};

static boolean TagsToStdout = FALSE;

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

static void rememberMaxLengths (const size_t nameLength, const size_t lineLength)
{
    if (nameLength > TagFile.max.tag)
	TagFile.max.tag = nameLength;

    if (lineLength > TagFile.max.line)
	TagFile.max.line = lineLength;
}

static void writePseudoTag (const char *const tagName,
			    const char *const fileName,
			    const char *const pattern)
{
    const int length = fprintf (TagFile.fp, "%s%s\t%s\t/%s/\n",
			       PSEUDO_TAG_PREFIX, tagName, fileName, pattern);
    ++TagFile.numTags.added;
    rememberMaxLengths (strlen (tagName), (size_t) length);
}

static void addPseudoTags (void)
{
    if (! Option.xref)
    {
	char format [11];
	const char *formatComment = "unknown format";

	sprintf (format, "%u", Option.tagFileFormat);

	if (Option.tagFileFormat == 1)
	    formatComment = "original ctags format";
	else if (Option.tagFileFormat == 2)
	    formatComment =
		    "extended format; --format=1 will not append ;\" to lines";

	writePseudoTag ("TAG_FILE_FORMAT", format, formatComment);
	writePseudoTag ("TAG_FILE_SORTED", Option.sorted ? "1":"0",
		       "0=unsorted, 1=sorted");
	writePseudoTag ("TAG_PROGRAM_AUTHOR",	AUTHOR_NAME,  AUTHOR_EMAIL);
	writePseudoTag ("TAG_PROGRAM_NAME",	PROGRAM_NAME, "");
	writePseudoTag ("TAG_PROGRAM_URL",	PROGRAM_URL,  "official site");
	writePseudoTag ("TAG_PROGRAM_VERSION",	PROGRAM_VERSION, "");
    }
}

static void updateSortedFlag (const char *const line,
			      FILE *const fp, fpos_t startOfLine)
{
    const char *const tab = strchr (line, '\t');

    if (tab != NULL)
    {
	const long boolOffset = tab - line + 1;		/* where it should be */

	if (line [boolOffset] == '0'  ||  line [boolOffset] == '1')
	{
	    fpos_t nextLine;

	    if (fgetpos (fp, &nextLine) == -1 || fsetpos (fp, &startOfLine) == -1)
		error (WARNING, "Failed to update 'sorted' pseudo-tag");
	    else
	    {
		fpos_t flagLocation;
		int c, d;

		do
		    c = fgetc (fp);
		while (c != '\t'  &&  c != '\n');
		fgetpos (fp, &flagLocation);
		d = fgetc (fp);
		if (c == '\t'  &&  (d == '0'  ||  d == '1')  &&
		    d != (int) Option.sorted)
		{
		    fsetpos (fp, &flagLocation);
		    fputc (Option.sorted ? '1' : '0', fp);
		}
		fsetpos (fp, &nextLine);
	    }
	}
    }
}

/*  Look through all line beginning with "!_TAG_FILE", and update those which
 *  require it.
 */
static long unsigned int updatePseudoTags (FILE *const fp)
{
    enum { maxClassLength = 20 };
    char class [maxClassLength + 1];
    unsigned long linesRead = 0;
    fpos_t startOfLine;
    size_t classLength;
    const char *line;

    sprintf (class, "%sTAG_FILE", PSEUDO_TAG_PREFIX);
    classLength = strlen (class);
    Assert (classLength < maxClassLength);

    fgetpos (fp, &startOfLine);
    line = readLine (TagFile.vLine, fp);
    while (line != NULL  &&  line [0] == class [0])
    {
	++linesRead;
	if (strncmp (line, class, classLength) == 0)
	{
	    char tab, classType [16];

	    if (sscanf (line + classLength, "%15s%c", classType, &tab) == 2  &&
		tab == '\t')
	    {
		if (strcmp (classType, "_SORTED") == 0)
		    updateSortedFlag (line, fp, startOfLine);
	    }
	    fgetpos (fp, &startOfLine);
	}
	line = readLine (TagFile.vLine, fp);
    }
    while (line != NULL)			/* skip to end of file */
    {
	++linesRead;
	line = readLine (TagFile.vLine, fp);
    }
    return linesRead;
}

/*
 *  Tag file management
 */



static boolean isTagFile (const char *const filename)
{
    boolean ok = FALSE;			/* we assume not unless confirmed */
    FILE *const fp = g_fopen (filename, "rb");

    if (fp == NULL  &&  errno == ENOENT)
	ok = TRUE;
    else if (fp != NULL)
    {
	const char *line = readLine (TagFile.vLine, fp);

	if (line == NULL)
	    ok = TRUE;
	fclose (fp);
    }
    return ok;
}

extern void copyBytes (FILE* const fromFp, FILE* const toFp, const long size)
{
    enum { BufferSize = 1000 };
    long toRead, numRead;
    char* buffer = xMalloc (BufferSize, char);
    long remaining = size;
    do
    {
	toRead = (0 < remaining && remaining < BufferSize) ?
		    remaining : BufferSize;
	numRead = fread (buffer, (size_t) 1, (size_t) toRead, fromFp);
	if (fwrite (buffer, (size_t)1, (size_t)numRead, toFp) < (size_t)numRead)
	    error (FATAL | PERROR, "cannot complete write");
	if (remaining > 0)
	    remaining -= numRead;
    } while (numRead == toRead  &&  remaining != 0);
    eFree (buffer);
}

extern void copyFile (const char *const from, const char *const to, const long size)
{
    FILE* const fromFp = g_fopen (from, "rb");
    if (fromFp == NULL)
	error (FATAL | PERROR, "cannot open file to copy");
    else
    {
	FILE* const toFp = g_fopen (to, "wb");
	if (toFp == NULL)
	    error (FATAL | PERROR, "cannot open copy destination");
	else
	{
	    copyBytes (fromFp, toFp, size);
	    fclose (toFp);
	}
	fclose (fromFp);
    }
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
	TagFile.fp = tempFile ("w", &TagFile.name);
    else
    {
	boolean fileExists;

	setDefaultTagFileName ();
	TagFile.name = eStrdup (Option.tagFileName);
	fileExists = doesFileExist (TagFile.name);
	if (fileExists  &&  ! isTagFile (TagFile.name))
	    error (FATAL,
	      "\"%s\" doesn't look like a tag file; I refuse to overwrite it.",
		  TagFile.name);

    if (Option.append  &&  fileExists)
    {
	TagFile.fp = g_fopen (TagFile.name, "r+");
	if (TagFile.fp != NULL)
	{
	    TagFile.numTags.prev = updatePseudoTags (TagFile.fp);
	    fclose (TagFile.fp);
	    TagFile.fp = g_fopen (TagFile.name, "a+");
	}
    }
    else
    {
	TagFile.fp = g_fopen (TagFile.name, "w");
	if (TagFile.fp != NULL)
	    addPseudoTags ();
    }

	if (TagFile.fp == NULL)
	{
	    error (FATAL | PERROR, "cannot open tag file");
	    exit (1);
	}
    }
    if (TagsToStdout)
	TagFile.directory = eStrdup (CurrentDirectory);
    else
	TagFile.directory = absoluteDirname (TagFile.name);
}

#ifdef USE_REPLACEMENT_TRUNCATE

/*  Replacement for missing library function.
 */
static int replacementTruncate (const char *const name, const long size)
{
    char *tempName = NULL;
    FILE *fp = tempFile ("w", &tempName);
    fclose (fp);
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


extern void makeTagEntry (const tagEntryInfo *const tag)
{
    Assert (tag->name != NULL);
    if (tag->name [0] == '\0')
	error (WARNING, "ignoring null tag in %s", vStringValue (File.name));
    else
    {
	int length = 0;

	if (NULL != TagEntryFunction)
		length = TagEntryFunction(tag);

	++TagFile.numTags.added;
	rememberMaxLengths (strlen (tag->name), (size_t) length);
    }
}

extern void initTagEntry (tagEntryInfo *const e, const char *const name)
{
    Assert (File.source.name != NULL);
    memset (e, 0, sizeof (tagEntryInfo));
    e->lineNumberEntry	= (boolean) (Option.locate == EX_LINENUM);
    e->lineNumber	= getSourceLineNumber ();
    e->language		= getSourceLanguageName ();
    e->filePosition	= getInputFilePosition ();
    e->sourceFileName	= getSourceFileTagPath ();
    e->name		= name;
}

/* vi:set tabstop=8 shiftwidth=4: */
