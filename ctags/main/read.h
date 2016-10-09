/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to read.c
*/
#ifndef CTAGS_MAIN_READ_H
#define CTAGS_MAIN_READ_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <stdio.h>
#include <ctype.h>

#include "parse.h"
#include "vstring.h"
#include "mio.h"

/*
*   MACROS
*/

/*
*   DATA DECLARATIONS
*/

enum eCharacters {
	/* white space characters */
	SPACE         = ' ',
	NEWLINE       = '\n',
	CRETURN       = '\r',
	FORMFEED      = '\f',
	TAB           = '\t',
	VTAB          = '\v',

	/* some hard to read characters */
	DOUBLE_QUOTE  = '"',
	SINGLE_QUOTE  = '\'',
	BACKSLASH     = '\\',

	/* symbolic representations, above 0xFF not to conflict with any byte */
	STRING_SYMBOL = ('S' + 0xff),
	CHAR_SYMBOL   = ('C' + 0xff)
};

/*  Maintains the state of the current input file.
 */
typedef struct sInputFileInfo {
	vString *name;           /* name to report for input file */
	vString *tagPath;        /* path of input file relative to tag file */
	unsigned long lineNumber;/* line number in the input file */
	unsigned long lineNumberOrigin; /* The value set to `lineNumber'
					   when `resetInputFile' is called
					   on the input stream.
					   This is needed for nested stream. */
	bool isHeader;           /* is input file a header file? */
	langType language;       /* language of input file */
} inputFileInfo;

typedef struct sInputFile {
	vString *path;      /* path of input file (if any) */
	vString *line;      /* last line read from file */
	const unsigned char* currentLine;   /* current line being worked on */
	MIO     *mio;       /* MIO stream used for reading the file */
	MIOPos  filePosition;   /* file position of current line */
	unsigned int ungetchIdx;
	int     ungetchBuf[3];  /* characters that were ungotten */

	/*  Contains data pertaining to the original source file in which the tag
	 *  was defined. This may be different from the input file when #line
	 *  directives are processed (i.e. the input file is preprocessor output).
	 */
	inputFileInfo input; /* name, lineNumber */
	inputFileInfo source;
} inputFile;

/*
*   GLOBAL VARIABLES
*/
/* should not be modified externally */
extern inputFile File;

/*
*   FUNCTION PROTOTYPES
*/

/* InputFile: reading from fp in inputFile with updating fields in input fields */
extern unsigned long getInputLineNumber (void);
extern int getInputLineOffset (void);
extern const char *getInputFileName (void);
extern MIOPos getInputFilePosition (void);
extern MIOPos getInputFilePositionForLine (int line);
extern langType getInputLanguage (void);
extern const char *getInputLanguageName (void);
extern const char *getInputFileTagPath (void);
extern bool isInputLanguage (langType lang);
extern bool isInputHeaderFile (void);
extern bool isInputLanguageKindEnabled (char c);
extern bool doesInputLanguageAllowNullTag (void);
extern kindOption *getInputLanguageFileKind (void);
extern bool doesInputLanguageRequestAutomaticFQTag (void);

extern void freeSourceFileResources (void);
extern bool fileOpen (const char *const fileName, const langType language);
extern void fileClose (void);
extern int getcFromInputFile (void);
extern int getNthPrevCFromInputFile (unsigned int nth, int def);
extern int skipToCharacterInInputFile (int c);
extern void ungetcToInputFile (int c);
extern const unsigned char *readLineFromInputFile (void);
extern char *readLineRaw (vString *const vLine, MIO *const mio);
extern char *readSourceLine (vString *const vLine, MIOPos location, long *const pSeekValue);
extern bool bufferOpen (unsigned char *buffer, size_t buffer_size,
                        const char *const fileName, const langType language );
#define bufferClose fileClose

extern const char *getSourceFileTagPath (void);
extern const char *getSourceLanguageName (void);
extern unsigned long getSourceLineNumber (void);

/* Bypass: reading from fp in inputFile WITHOUT updating fields in input fields */
extern char *readLineFromBypass (vString *const vLine, MIOPos location, long *const pSeekValue);

#endif  /* CTAGS_MAIN_READ_H */
