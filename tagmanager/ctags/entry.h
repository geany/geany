/*
*
*   Copyright (c) 1998-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   External interface to entry.c
*/
#ifndef _ENTRY_H
#define _ENTRY_H

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <mio/mio.h>

#include "vstring.h"

/*
*   MACROS
*/
#define WHOLE_FILE  -1L

/*
*   DATA DECLARATIONS
*/

/*  Maintains the state of the tag file.
 */
typedef struct eTagFile {
    char *name;
    char *directory;
    MIO *mio;
    struct sNumTags { unsigned long added, prev; } numTags;
    struct sMax { size_t line, tag, file; } max;
    struct sEtags {
	char *name;
	MIO *mio;
	size_t byteCount;
    } etags;
    vString *vLine;
} tagFile;

typedef struct sTagFields {
    unsigned int count;		/* number of additional extension flags */
    const char *const *label;	/* list of labels for extension flags */
    const char *const *value;	/* list of values for extension flags */
} tagFields;

/*  Information about the current tag candidate.
 */
typedef struct sTagEntryInfo {
    boolean	lineNumberEntry;/* pattern or line number entry */
    unsigned long lineNumber;	/* line number of tag */
    MIOPos	filePosition;	/* file position of line containing tag */
    int bufferPosition;		/* buffer position of line containing tag */
    const char*	language;	/* language of source file */
    boolean	isFileScope;	/* is tag visibile only within source file? */
    boolean	isFileEntry;	/* is this just an entry for a file name? */
    boolean	truncateLine;	/* truncate tag line at end of tag name? */
    const char *sourceFileName;	/* name of source file */
    const char *name;		/* name of the tag */
    const char *kindName;	/* kind of tag */
    char	kind;		/* single character representation of kind */
    struct {
	const char* access;
	const char* fileScope;
	const char* implementation;
	const char* inheritance;
	const char* scope [2];	/* value and key */
	const char *arglist; /* Argument list for functions and macros with arguments */
	const char *varType;
    } extensionFields;		/* list of extension fields*/
	int type;
	unsigned long seekPosition;
} tagEntryInfo;

/*
*   GLOBAL VARIABLES
*/
extern tagFile TagFile;

/*
*   FUNCTION PROTOTYPES
*/
extern void freeTagFileResources (void);
extern const char *tagFileName (void);
extern void copyBytes (MIO* const fromMio, MIO* const toMio, const long size);
extern void copyFile (const char *const from, const char *const to, const long size);
extern void openTagFile (void);
extern void closeTagFile (const boolean resize);
extern void beginEtagsFile (void);
extern void endEtagsFile (const char *const name);
extern void makeTagEntry (const tagEntryInfo *const tag);
extern void setTagArglistByName (const char *tag_name, const char *arglist);
extern void initTagEntry (tagEntryInfo *const e, const char *const name);

#endif	/* _ENTRY_H */

/* vi:set tabstop=8 shiftwidth=4: */
