/*
*   Copyright (c) 1996-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions to process command line options.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>  /* to declare isspace () */

#include "ctags.h"
#include "routines.h"
#define OPTION_WRITE
#include "options.h"
#include "parse.h"

#define CTAGS_ENVIRONMENT       "CTAGS"

#define CTAGS_FILE  "tags"


/*  The following separators are permitted for list options.
 */
#define EXTENSION_SEPARATOR '.'
#define PATTERN_START '('
#define PATTERN_STOP  ')'
#define IGNORE_SEPARATORS   ", \t\n"

#ifndef DEFAULT_FILE_FORMAT
# define DEFAULT_FILE_FORMAT  2
#endif

#if defined (WIN32) || defined (HAVE_OPENDIR)
# define RECURSE_SUPPORTED
#endif

#define isCompoundOption(c)  (bool) (strchr ("fohiILpDb", (c)) != NULL)



/*
*   DATA DEFINITIONS
*/

static stringList* Excluded = NULL;

optionValues Option = {
	{
		false,          /* --extra=f */
		true,           /* --file-scope */
	},
	{
		true,           /* -fields=a */
		true,           /* -fields=f */
		false,          /* -fields=m */
		true,           /* -fields=i */
		false,          /* -fields=k */
		true,           /* -fields=z */
		true,           /* -fields=K */
		false,          /* -fields=l */
		true,           /* -fields=n */
		true,           /* -fields=s */
		true,           /* -fields=P */
		true            /* -fields=A */
	},
	NULL,               /* -I */
	false,              /* -a */
	false,              /* -B */
#ifdef MACROS_USE_PATTERNS
	EX_PATTERN,         /* -n, --excmd */
#else
	EX_MIX,             /* -n, --excmd */
#endif
	false,              /* -R */
	true,               /* -u, --sort */
	false,              /* -V */
	false,              /* -x */
	NULL,               /* -L */
	NULL,               /* -o */
	NULL,               /* -h */
	NULL,               /* --etags-include */
	DEFAULT_FILE_FORMAT,/* --format */
	false,              /* --if0 */
	false,              /* --kind-long */
	LANG_AUTO,          /* --lang */
	true,               /* --links */
	false,              /* --filter */
	NULL,               /* --filter-terminator */
	false,              /* --qualified-tags */
	false,              /* --tag-relative */
	false,              /* --totals */
	false,              /* --line-directives */
	false,              /* --nest */
	.machinable = false,
	.withListHeader = true,
};


extern void verbose (const char *const format, ...)
{
}

extern void freeList (stringList** const pList)
{
	if (*pList != NULL)
	{
		stringListDelete (*pList);
		*pList = NULL;
	}
}

extern void setDefaultTagFileName (void)
{
	Option.tagFileName = eStrdup (CTAGS_FILE);
}

/*  Determines whether the specified file name is considered to be a header
 *  file for the purposes of determining whether enclosed tags are global or
 *  static.
 */
extern bool isIncludeFile (const char *const fileName)
{
	return false;
}
