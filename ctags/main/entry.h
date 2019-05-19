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

#include <stdint.h>

#include "field.h"
#include "xtag.h"
#include "mio.h"
#include "ptrarray.h"
#include "nestlevel.h"

/*
*   MACROS
*/

/*
*   DATA DECLARATIONS
*/
typedef struct sTagField {
	fieldType  ftype;
	const char* value;
	bool valueOwner;			/* used only in parserFieldsDynamic */
} tagField;

typedef uint64_t roleBitsType;

/*  Information about the current tag candidate.
 */
struct sTagEntryInfo {
	unsigned int lineNumberEntry:1;  /* pattern or line number entry */
	unsigned int isFileScope    :1;  /* is tag visible only within input file? */
	unsigned int isFileEntry    :1;  /* is this just an entry for a file name? */
	unsigned int truncateLineAfterTag :1;  /* truncate tag line at end of tag name? */
	unsigned int placeholder    :1;	 /* This is just a part of scope context.
					    Put this entry to cork queue but
					    don't print it to tags file. */
	unsigned int skipAutoFQEmission:1; /* If a parser makes a fq tag for the
										  current tag by itself, set this. */

	unsigned long lineNumber;     /* line number of tag */
	const char* pattern;	      /* pattern for locating input line
				       * (may be NULL if not present) *//*  */
	unsigned int boundaryInfo;    /* info about nested input stream */
	MIOPos      filePosition;     /* file position of line containing tag */
	langType langType;         /* language of input file */
	const char *inputFileName;   /* name of input file */
	const char *name;             /* name of the tag */
	int kindIndex;	      /* kind descriptor */
	uint8_t extra[ ((XTAG_COUNT) / 8) + 1 ];
	uint8_t *extraDynamic;		/* Dynamically allocated but freed by per parser TrashBox */

	struct {
		const char* access;
		const char* fileScope;
		const char* implementation;
		const char* inheritance;

		/* Which scopeKindIndex belong to. If the value is LANG_AUTO,
		   the value for langType field of this structure is used as default value.
		   LANG_AUTO is set automatically in initTagEntryInfo. */
		langType    scopeLangType;
		int         scopeKindIndex;
		const char* scopeName;
		int         scopeIndex;   /* cork queue entry for upper scope tag.
					     This field is meaningful if the value
					     is not CORK_NIL and scope[0]  and scope[1] are
					     NULL. */

		const char* signature;

		/* type (union/struct/etc.) and name for a variable or typedef. */
		const char* typeRef [2];  /* e.g., "struct" and struct name */

#define ROLE_INDEX_DEFINITION -1
#define ROLE_NAME_DEFINITION "def"
#define ROLE_MAX_COUNT (sizeof(roleBitsType) * 8)
		roleBitsType roleBits; /* for role of reference tag */

#ifdef HAVE_LIBXML
		const char* xpath;
#endif
		unsigned long endLine;
	} extensionFields;  /* list of extension fields*/

	/* `usedParserFields' tracks how many parser own fields are
	   used. If it is a few (less than PRE_ALLOCATED_PARSER_FIELDS),
	   statically allocated parserFields is used. If more fields than
	   PRE_ALLOCATED_PARSER_FIELDS is defined and attached, parserFieldsDynamic
	   is used. */
	unsigned int usedParserFields;
#define PRE_ALLOCATED_PARSER_FIELDS 5
#define NO_PARSER_FIELD -1
	tagField     parserFields [PRE_ALLOCATED_PARSER_FIELDS];
	ptrArray *   parserFieldsDynamic;

	/* Following source* fields are used only when #line is found
	   in input and --line-directive is given in ctags command line. */
	langType sourceLangType;
	const char *sourceFileName;
	unsigned long sourceLineNumberDifference;
};


/*
*   GLOBAL VARIABLES
*/


/*
*   FUNCTION PROTOTYPES
*/
extern int makeTagEntry (const tagEntryInfo *const tag);
extern void initTagEntry (tagEntryInfo *const e, const char *const name,
			  int kindIndex);
extern void initRefTagEntry (tagEntryInfo *const e, const char *const name,
			     int kindIndex, int roleIndex);

extern void assignRole(tagEntryInfo *const e, int roleIndex);
extern bool isRoleAssigned(const tagEntryInfo *const e, int roleIndex);

extern int makeQualifiedTagEntry (const tagEntryInfo *const e);


#define CORK_NIL 0
tagEntryInfo *getEntryInCorkQueue   (unsigned int n);
tagEntryInfo *getEntryOfNestingLevel (const NestingLevel *nl);
size_t        countEntryInCorkQueue (void);

extern void    markTagExtraBit     (tagEntryInfo *const tag, xtagType extra);
extern bool isTagExtraBitMarked (const tagEntryInfo *const tag, xtagType extra);

extern void attachParserField (tagEntryInfo *const tag, fieldType ftype, const char* value);
extern void attachParserFieldToCorkEntry (int index, fieldType ftype, const char* value);

#endif  /* CTAGS_MAIN_ENTRY_H */
