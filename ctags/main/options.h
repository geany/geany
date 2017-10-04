/*
*   Copyright (c) 1998-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines external interface to option processing.
*/
#ifndef CTAGS_MAIN_OPTIONS_H
#define CTAGS_MAIN_OPTIONS_H

#if defined(OPTION_WRITE)
# define CONST_OPTION
#else
# define CONST_OPTION const
#endif

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <stdarg.h>

#include "args.h"
#include "parse.h"
#include "strlist.h"
#include "vstring.h"

/*
*   DATA DECLARATIONS
*/


/*  This stores the command line options.
 */
typedef struct sOptionValues {
	struct sInclude {
		bool fileNames;      /* include tags for source file names */
		bool fileScope;      /* include tags of file scope only */
	} include;
	struct sExtFields {         /* extension field content control */
		bool access;
		bool fileScope;
		bool implementation;
		bool inheritance;
		bool kind;
		bool kindKey;
		bool kindLong;
		bool language;
		bool lineNumber;
		bool scope;
		bool filePosition;  /* Write file position */
		bool argList;       /* Write function and macro argumentlist */
	} extensionFields;
	stringList* ignore;     /* -I  name of file containing tokens to ignore */
	bool append;            /* -a  append to "tags" file */
	bool backward;          /* -B  regexp patterns search backwards */
	enum eLocate {
		EX_MIX,             /* line numbers for defines, patterns otherwise */
		EX_LINENUM,         /* -n  only line numbers in tag file */
		EX_PATTERN          /* -N  only patterns in tag file */
	} locate;               /* --excmd  EX command used to locate tag */
	bool recurse;           /* -R  recurse into directories */
	bool sorted;            /* -u,--sort  sort tags */
	bool verbose;           /* -V  verbose */
	bool xref;              /* -x  generate xref output instead */
	char *fileList;         /* -L  name of file containing names of files */
	char *tagFileName;      /* -o  name of tags file */
	stringList* headerExt;  /* -h  header extensions */
	stringList* etagsInclude;/* --etags-include  list of TAGS files to include*/
	unsigned int tagFileFormat;/* --format  tag file format (level) */
	bool if0;               /* --if0  examine code within "#if 0" branch */
	bool kindLong;          /* --kind-long */
	langType language;      /* --lang specified language override */
	bool followLinks;       /* --link  follow symbolic links? */
	bool filter;            /* --filter  behave as filter: files in, tags out */
	char* filterTerminator; /* --filter-terminator  string to output */
	bool qualifiedTags;     /* --qualified-tags include class-qualified tag */
	bool tagRelative;       /* --tag-relative file paths relative to tag file */
	bool printTotals;       /* --totals  print cumulative statistics */
	bool lineDirectives;    /* --linedirectives  process #line directives */
	bool nestFunction;      /* --nest Nest inside function blocks for tags */
	bool machinable;        /* --machinable */
	bool withListHeader;    /* --with-list-header */
} optionValues;

/*
*   GLOBAL VARIABLES
*/
extern CONST_OPTION optionValues		Option;

/*
*   FUNCTION PROTOTYPES
*/
extern void verbose (const char *const format, ...) CTAGS_ATTR_PRINTF (1, 2);
extern void freeList (stringList** const pString);
extern void setDefaultTagFileName (void);

extern bool isIncludeFile (const char *const fileName);
extern bool isExcludedFile (const char* const name);
extern bool isIgnoreToken (const char *const name, bool *const pIgnoreParens, const char **const replacement);
extern void readOptionConfiguration (void);
extern void initOptions (void);
extern void freeOptionResources (void);

void addIgnoreListFromFile (const char *const fileName);

#endif  /* CTAGS_MAIN_OPTIONS_H */
