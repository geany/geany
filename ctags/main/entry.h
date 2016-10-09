/*
*   Copyright (c) 1998-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   External interface to entry.c
*/
#ifndef CTAGS_MAIN_ENTRY_H
#define CTAGS_MAIN_ENTRY_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "types.h"

#include "field.h"
#include "mio.h"
#include "vstring.h"
#include "kind.h"

/*
*   MACROS
*/
#define WHOLE_FILE  -1L

#define NO_PARSER_FIELD -1
#define CORK_NIL 0

/*
*   DATA DECLARATIONS
*/
typedef struct sTagField {
	fieldType  ftype;
	const char* value;
} tagField;

/*  Information about the current tag candidate.
 */
struct sTagEntryInfo {
	bool     lineNumberEntry;   /* pattern or line number entry */
	unsigned long lineNumber;   /* line number of tag */
	MIOPos      filePosition;   /* file position of line containing tag */
	const char* language;       /* language of source file */
	bool     isFileScope;       /* is tag visible only within source file? */
	bool     isFileEntry;       /* is this just an entry for a file name? */
	bool     truncateLine;      /* truncate tag line at end of tag name? */
	const char *sourceFileName; /* name of source file */
	const char *name;           /* name of the tag */
	const kindOption *kind;     /* kind descriptor */
	struct {
		const char* access;
		const char* fileScope;
		const char* implementation;
		const char* inheritance;

		const kindOption* scopeKind;
		const char* scopeName;

		const char *signature; /* Argument list for functions and macros with arguments */
		const char *varType;
	} extensionFields;          /* list of extension fields*/
};

/*
*   GLOBAL VARIABLES
*/

/*
*   FUNCTION PROTOTYPES
*/
extern void freeTagFileResources (void);
extern const char *tagFileName (void);
extern void copyBytes (MIO* const fromMio, MIO* const toMio, const long size);
extern void copyFile (const char *const from, const char *const to, const long size);
extern void openTagFile (void);
extern void closeTagFile (const bool resize);
extern void makeTagEntry (const tagEntryInfo *const tag);
extern void initTagEntry (tagEntryInfo *const e, const char *const name, const kindOption *kind);

extern unsigned long numTagsAdded(void);
extern void setNumTagsAdded (unsigned long nadded);
extern unsigned long numTagsTotal(void);
extern unsigned long maxTagsLine(void);
extern void setMaxTagsLine (unsigned long max);
extern void invalidatePatternCache(void);
extern void tagFilePosition (MIOPos *p);
extern void setTagFilePosition (MIOPos *p);
extern const char* getTagFileDirectory (void);
extern void getTagScopeInformation (tagEntryInfo *const tag,
				    const char **kind, const char **name);

#endif  /* CTAGS_MAIN_ENTRY_H */
