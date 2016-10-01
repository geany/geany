/*
*   Copyright (c) 1996-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains low level input and tag file read functions (newline
*   conversion for input files are performed at this level).
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>
#include <ctype.h>
#include <glib/gstdio.h>

#define FILE_WRITE
#include "read.h"
#include "debug.h"
#include "entry.h"
#include "main.h"
#include "routines.h"
#include "options.h"

/*
*   DATA DEFINITIONS
*/
inputFile File;                 /* globally read through macros */
static MIOPos StartOfLine;      /* holds deferred position of start of line */


/*
*   FUNCTION DEFINITIONS
*/

extern kindOption *getInputLanguageFileKind (void)
{
	return getLanguageFileKind (File.input.language);
}

extern void freeSourceFileResources (void)
{
	vStringDelete (File.input.name);
	vStringDelete (File.path);
	vStringDelete (File.source.name);
	vStringDelete (File.line);
}

/*
 *   Input file access functions
 */

static void setInputFileName (const char *const fileName)
{
	const char *const head = fileName;
	const char *const tail = baseFilename (head);

	if (File.input.name != NULL)
		vStringDelete (File.input.name);
	File.input.name = vStringNewInit (fileName);

	if (File.path != NULL)
		vStringDelete (File.path);
	if (tail == head)
		File.path = NULL;
	else
	{
		const size_t length = tail - head - 1;
		File.path = vStringNew ();
		vStringNCopyS (File.path, fileName, length);
	}
}
static void setSourceFileParameters (vString *const fileName, const langType language)
{
	if (File.source.name != NULL)
		vStringDelete (File.source.name);
	if (File.input.name != NULL)
		vStringDelete (File.input.name);
	File.source.name = fileName;
	File.input.name = vStringNewCopy(fileName);

	if (File.source.tagPath != NULL)
		eFree (File.source.tagPath);
	if (! Option.tagRelative || isAbsolutePath (vStringValue (fileName)))
		File.source.tagPath = vStringNewCopy (fileName);
	else
		File.source.tagPath =
				vStringNewOwn (relativeFilename (vStringValue (fileName),
								TagFile.directory));

	if (vStringLength (fileName) > TagFile.max.file)
		TagFile.max.file = vStringLength (fileName);

	File.source.isHeader = isIncludeFile (vStringValue (fileName));
	File.input.isHeader = File.source.isHeader;
	if (language != -1)
		File.source.language = language;
	else
		File.source.language = getFileLanguage (vStringValue (fileName));
	File.input.language = File.source.language;
}

static bool setSourceFileName (vString *const fileName)
{
	const langType language = getFileLanguage (vStringValue (fileName));
	bool result = false;
	if (language != LANG_IGNORE)
	{
		vString *pathName;
		if (isAbsolutePath (vStringValue (fileName)) || File.path == NULL)
			pathName = vStringNewCopy (fileName);
		else
			pathName = combinePathAndFile (vStringValue (File.path),
										vStringValue (fileName));
		setSourceFileParameters (pathName, -1);
		result = true;
	}
	return result;
}

/*
 *   Line directive parsing
 */

static int skipWhite (void)
{
	int c;
	do
		c = mio_getc (File.mio);
	while (c == ' '  ||  c == '\t');
	return c;
}

static unsigned long readLineNumber (void)
{
	unsigned long lNum = 0;
	int c = skipWhite ();
	while (c != EOF  &&  isdigit (c))
	{
		lNum = (lNum * 10) + (c - '0');
		c = mio_getc (File.mio);
	}
	mio_ungetc (File.mio, c);
	if (c != ' '  &&  c != '\t')
		lNum = 0;

	return lNum;
}

/* While ANSI only permits lines of the form:
 *   # line n "filename"
 * Earlier compilers generated lines of the form
 *   # n filename
 * GNU C will output lines of the form:
 *   # n "filename"
 * So we need to be fairly flexible in what we accept.
 */
static vString *readFileName (void)
{
	vString *const fileName = vStringNew ();
	bool quoteDelimited = false;
	int c = skipWhite ();

	if (c == '"')
	{
		c = mio_getc (File.mio);  /* skip double-quote */
		quoteDelimited = true;
	}
	while (c != EOF  &&  c != '\n'  &&
			(quoteDelimited ? (c != '"') : (c != ' '  &&  c != '\t')))
	{
		vStringPut (fileName, c);
		c = mio_getc (File.mio);
	}
	if (c == '\n')
		mio_ungetc (File.mio, c);
	vStringPut (fileName, '\0');

	return fileName;
}

static bool parseLineDirective (void)
{
	bool result = false;
	int c = skipWhite ();
	DebugStatement ( const char* lineStr = ""; )

	if (isdigit (c))
	{
		mio_ungetc (File.mio, c);
		result = true;
	}
	else if (c == 'l'  &&  mio_getc (File.mio) == 'i'  &&
			 mio_getc (File.mio) == 'n'  &&  mio_getc (File.mio) == 'e')
	{
		c = mio_getc (File.mio);
		if (c == ' '  ||  c == '\t')
		{
			DebugStatement ( lineStr = "line"; )
			result = true;
		}
	}
	if (result)
	{
		const unsigned long lNum = readLineNumber ();
		if (lNum == 0)
			result = false;
		else
		{
			vString *const fileName = readFileName ();
			if (vStringLength (fileName) == 0)
			{
				File.source.lineNumber = lNum - 1;  /* applies to NEXT line */
				DebugStatement ( debugPrintf (DEBUG_RAW, "#%s %ld", lineStr, lNum); )
			}
			else if (setSourceFileName (fileName))
			{
				File.source.lineNumber = lNum - 1;  /* applies to NEXT line */
				DebugStatement ( debugPrintf (DEBUG_RAW, "#%s %ld \"%s\"",
								lineStr, lNum, vStringValue (fileName)); )
			}

			if (Option.include.fileNames && vStringLength (fileName) > 0 &&
				lNum == 1)
			{
				tagEntryInfo tag;
				initTagEntry (&tag, baseFilename (vStringValue (fileName)), getInputLanguageFileKind ());

				tag.isFileEntry     = true;
				tag.lineNumberEntry = true;
				tag.lineNumber      = 1;

				makeTagEntry (&tag);
			}
			vStringDelete (fileName);
			result = true;
		}
	}
	return result;
}

/*
 *   Input file I/O operations
 */

/*  This function opens an input file, and resets the line counter.  If it
 *  fails, it will display an error message and leave the File.mio set to NULL.
 */
extern bool fileOpen (const char *const fileName, const langType language)
{
	const char *const openMode = "rb";
	bool opened = false;

	/*	If another file was already open, then close it.
	 */
	if (File.mio != NULL)
	{
		mio_free (File.mio);  /* close any open input file */
		File.mio = NULL;
	}

	File.mio = mio_new_file_full (fileName, openMode, g_fopen, fclose);
	if (File.mio == NULL)
		error (WARNING | PERROR, "cannot open \"%s\"", fileName);
	else
	{
		opened = true;

		setInputFileName (fileName);
		mio_getpos (File.mio, &StartOfLine);
		mio_getpos (File.mio, &File.filePosition);
		File.currentLine  = NULL;
		File.input.lineNumber   = 0L;
		File.eof          = false;
		File.newLine      = true;

		if (File.line != NULL)
			vStringClear (File.line);

		setSourceFileParameters (vStringNewInit (fileName), language);
		File.source.lineNumber = 0L;

		verbose ("OPENING %s as %s language %sfile\n", fileName,
				getLanguageName (language),
				File.source.isHeader ? "include " : "");
	}
	return opened;
}

/* The user should take care of allocate and free the buffer param. 
 * This func is NOT THREAD SAFE.
 * The user should not tamper with the buffer while this func is executing.
 */
extern bool bufferOpen (unsigned char *buffer, size_t buffer_size,
						const char *const fileName, const langType language )
{
	bool opened = false;
		
	/* Check whether a file of a buffer were already open, then close them.
	 */
	if (File.mio != NULL) {
		mio_free (File.mio);            /* close any open source file */
		File.mio = NULL;
	}

	/* check if we got a good buffer */
	if (buffer == NULL || buffer_size == 0) {
		opened = false;
		return opened;
	}
		
	opened = true;
			
	File.mio = mio_new_memory (buffer, buffer_size, NULL, NULL);
	setInputFileName (fileName);
	mio_getpos (File.mio, &StartOfLine);
	mio_getpos (File.mio, &File.filePosition);
	File.currentLine  = NULL;
	File.input.lineNumber   = 0L;
	File.eof          = false;
	File.newLine      = true;

	if (File.line != NULL)
		vStringClear (File.line);

	setSourceFileParameters (vStringNewInit (fileName), language);
	File.source.lineNumber = 0L;

	verbose ("OPENING %s as %s language %sfile\n", fileName,
			getLanguageName (language),
			File.source.isHeader ? "include " : "");

	return opened;
}

extern void fileClose (void)
{
	if (File.mio != NULL)
	{
		/*  The line count of the file is 1 too big, since it is one-based
		 *  and is incremented upon each newline.
		 */
		if (Option.printTotals)
			addTotals (0, File.input.lineNumber - 1L,
					  getFileSize (vStringValue (File.input.name)));

		mio_free (File.mio);
		File.mio = NULL;
	}
}

extern bool fileEOF (void)
{
	return File.eof;
}

/*  Action to take for each encountered input newline.
 */
static void fileNewline (void)
{
	File.filePosition = StartOfLine;
	File.newLine = false;
	File.input.lineNumber++;
	File.source.lineNumber++;
	DebugStatement ( if (Option.breakLine == File.input.lineNumber) lineBreak (); )
	DebugStatement ( debugPrintf (DEBUG_RAW, "%6ld: ", File.input.lineNumber); )
}

/*  This function reads a single character from the stream, performing newline
 *  canonicalization.
 */
static int iFileGetc (void)
{
	int	c;
readnext:
	c = mio_getc (File.mio);

	/*	If previous character was a newline, then we're starting a line.
	 */
	if (File.newLine  &&  c != EOF)
	{
		fileNewline ();
		if (c == '#'  &&  Option.lineDirectives)
		{
			if (parseLineDirective ())
				goto readnext;
			else
			{
				mio_setpos (File.mio, &StartOfLine);
				c = mio_getc (File.mio);
			}
		}
	}

	if (c == EOF)
		File.eof = true;
	else if (c == NEWLINE)
	{
		File.newLine = true;
		mio_getpos (File.mio, &StartOfLine);
	}
	else if (c == CRETURN)
	{
		/* Turn line breaks into a canonical form. The three commonly
		 * used forms if line breaks: LF (UNIX/Mac OS X), CR (Mac OS 9),
		 * and CR-LF (MS-DOS) are converted into a generic newline.
		 */
		const int next = mio_getc (File.mio);  /* is CR followed by LF? */
		if (next != NEWLINE)
			mio_ungetc (File.mio, next);

		c = NEWLINE;  /* convert CR into newline */
		File.newLine = true;
		mio_getpos (File.mio, &StartOfLine);
	}
	DebugStatement ( debugPutc (DEBUG_RAW, c); )
	return c;
}

extern void ungetcToInputFile (int c)
{
	const size_t len = ARRAY_SIZE (File.ungetchBuf);

	Assert (File.ungetchIdx < len);
	/* we cannot rely on the assertion that might be disabled in non-debug mode */
	if (File.ungetchIdx < len)
		File.ungetchBuf[File.ungetchIdx++] = c;
}

static vString *iFileGetLine (void)
{
	vString *result = NULL;
	int c;
	if (File.line == NULL)
		File.line = vStringNew ();
	vStringClear (File.line);
	do
	{
		c = iFileGetc ();
		if (c != EOF)
			vStringPut (File.line, c);
		if (c == '\n'  ||  (c == EOF  &&  vStringLength (File.line) > 0))
		{
#ifdef HAVE_REGEX
			if (vStringLength (File.line) > 0)
				matchRegex (File.line, File.source.language);
#endif
			result = File.line;
			break;
		}
	} while (c != EOF);
	Assert (result != NULL  ||  File.eof);
	return result;
}

/*  Do not mix use of readLineFromInputFile () and getcFromInputFile () for the same file.
 */
extern int getcFromInputFile (void)
{
	int c;

	/*  If there is an ungotten character, then return it.  Don't do any
	 *  other processing on it, though, because we already did that the
	 *  first time it was read through getcFromInputFile ().
	 */
	if (File.ungetchIdx > 0)
	{
		c = File.ungetchBuf[--File.ungetchIdx];
		return c;  /* return here to avoid re-calling debugPutc () */
	}
	do
	{
		if (File.currentLine != NULL)
		{
			c = *File.currentLine++;
			if (c == '\0')
				File.currentLine = NULL;
		}
		else
		{
			vString* const line = iFileGetLine ();
			if (line != NULL)
				File.currentLine = (unsigned char*) vStringValue (line);
			if (File.currentLine == NULL)
				c = EOF;
			else
				c = '\0';
		}
	} while (c == '\0');
	DebugStatement ( debugPutc (DEBUG_READ, c); )
	return c;
}

/* returns the nth previous character (0 meaning current), or def if nth cannot
 * be accessed.  Note that this can't access previous line data. */
extern int getNthPrevCFromInputFile (unsigned int nth, int def)
{
	const unsigned char *base = (unsigned char *) vStringValue (File.line);
	const unsigned int offset = File.ungetchIdx + 1 + nth;

	if (File.currentLine != NULL && File.currentLine >= base + offset)
		return (int) *(File.currentLine - offset);
	else
		return def;
}

extern int skipToCharacterInInputFile (int c)
{
	int d;
	do
	{
		d = getcFromInputFile ();
	} while (d != EOF && d != c);
	return d;
}

/*  An alternative interface to getcFromInputFile (). Do not mix use of readLineFromInputFile()
 *  and getcFromInputFile() for the same file. The returned string does not contain
 *  the terminating newline. A NULL return value means that all lines in the
 *  file have been read and we are at the end of file.
 */
extern const unsigned char *readLineFromInputFile (void)
{
	vString* const line = iFileGetLine ();
	const unsigned char* result = NULL;
	if (line != NULL)
	{
		result = (const unsigned char*) vStringValue (line);
		vStringStripNewline (line);
		DebugStatement ( debugPrintf (DEBUG_READ, "%s\n", result); )
	}
	return result;
}

/*
 *   Raw file line reading with automatic buffer sizing
 */
extern char *readLineRaw (vString *const vLine, MIO *const mio)
{
	char *result = NULL;

	vStringClear (vLine);
	if (mio == NULL)  /* to free memory allocated to buffer */
		error (FATAL, "NULL file pointer");
	else
	{
		bool reReadLine;

		/*  If reading the line places any character other than a null or a
		 *  newline at the last character position in the buffer (one less
		 *  than the buffer size), then we must resize the buffer and
		 *  reattempt to read the line.
		 */
		do
		{
			char *const pLastChar = vStringValue (vLine) + vStringSize (vLine) -2;
			long startOfLine;

			startOfLine = mio_tell(mio);
			reReadLine = false;
			*pLastChar = '\0';
			result = mio_gets (mio, vStringValue (vLine), (int) vStringSize (vLine));
			if (result == NULL)
			{
				if (! mio_eof (mio))
					error (FATAL | PERROR, "Failure on attempt to read file");
			}
			else if (*pLastChar != '\0'  &&
					 *pLastChar != '\n'  &&  *pLastChar != '\r')
			{
				/*  buffer overflow */
				reReadLine = vStringAutoResize (vLine);
				if (reReadLine)
					mio_seek (mio, startOfLine, SEEK_SET);
				else
					error (FATAL | PERROR, "input line too big; out of memory");
			}
			else
			{
				char* eol;
				vStringSetLength (vLine);
				/* canonicalize new line */
				eol = vStringValue (vLine) + vStringLength (vLine) - 1;
				if (*eol == '\r')
					*eol = '\n';
				else if (vStringLength (vLine) != 1 && *(eol - 1) == '\r'  &&  *eol == '\n')
				{
					*(eol - 1) = '\n';
					*eol = '\0';
					--vLine->length;
				}
			}
		} while (reReadLine);
	}
	return result;
}

/*  Places into the line buffer the contents of the line referenced by
 *  "location".
 */
extern char *readLineFromBypass (
		vString *const vLine, MIOPos location, long *const pSeekValue)
{
	MIOPos orignalPosition;
	char *result;

	mio_getpos (File.mio, &orignalPosition);
	mio_setpos (File.mio, &location);
	if (pSeekValue != NULL)
		*pSeekValue = mio_tell (File.mio);
	result = readLineRaw (vLine, File.mio);
	mio_setpos (File.mio, &orignalPosition);
	/* If the file is empty, we can't get the line
	   for location 0. readLineFromBypass doesn't know
	   what itself should do; just report it to the caller. */
	return result;
}
