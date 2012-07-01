/*
*
*   Copyright (c) 1996-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions to process command line options.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>	/* to declare isspace () */

#include "ctags.h"
#include "main.h"
#define OPTION_WRITE
#include "options.h"
#include "parse.h"

#include <glib.h>

#define CTAGS_ENVIRONMENT	"CTAGS"

#define CTAGS_FILE	"tags"


/*  The following separators are permitted for list options.
 */
#define EXTENSION_SEPARATOR '.'
#define PATTERN_START '('
#define PATTERN_STOP  ')'
#define IGNORE_SEPARATORS   ", \t\n"

#ifndef DEFAULT_FILE_FORMAT
# define DEFAULT_FILE_FORMAT	2
#endif

#if defined (MSDOS) || defined (WIN32) || defined (OS2) || defined (AMIGA) || defined (HAVE_OPENDIR)
# define RECURSE_SUPPORTED
#endif

#define isCompoundOption(c)	(boolean) (strchr ("fohiILpDb", (c)) != NULL)



/*
*   DATA DEFINITIONS
*/

optionValues Option = {
    {
	FALSE,		/* --extra=f */
	FALSE,		/* --extra=q */
	TRUE,		/* --file-scope */
    },
    {
	TRUE,		/* -fields=a */
	TRUE,		/* -fields=f */
	FALSE,		/* -fields=m */
	TRUE,		/* -fields=i */
	FALSE,		/* -fields=k */
	TRUE,		/* -fields=z */
	TRUE,		/* -fields=K */
	FALSE,		/* -fields=l */
	TRUE,		/* -fields=n */
	TRUE,		/* -fields=s */
	TRUE,		/* -fields=P */
	TRUE		/* -fields=A */
    },
    NULL,		/* -I */
    FALSE,		/* -a */
    FALSE,		/* -B */
    FALSE,		/* -e */
#ifdef MACROS_USE_PATTERNS
    EX_PATTERN,		/* -n, --excmd */
#else
    EX_MIX,		/* -n, --excmd */
#endif
    FALSE,		/* -R */
    TRUE,		/* -u, --sort */
    FALSE,		/* -V */
    FALSE,		/* -x */
    NULL,		/* -L */
    NULL,		/* -o */
    NULL,		/* -h */
    DEFAULT_FILE_FORMAT,/* --format */
    FALSE,		/* --if0 */
    FALSE,		/* --kind-long */
    LANG_AUTO,		/* --lang */
    TRUE,		/* --links */
    FALSE,		/* --filter */
    NULL,		/* --filter-terminator */
    FALSE,		/* --qualified-tags */
    FALSE,		/* --tag-relative */
    FALSE,		/* --totals */
    FALSE,		/* --line-directives */
	FALSE,
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

/*
 *  File extension and language mapping
 */
extern const char *fileExtension (const char *const fileName)
{
    const char *extension;
    const char *pDelimiter = NULL;

    pDelimiter = strrchr (fileName, '.');

    if (pDelimiter == NULL)
	extension = "";
    else
	extension = pDelimiter + 1;	/* skip to first char of extension */

    return extension;
}

/*  Determines whether the specified file name is considered to be a header
 *  file for the purposes of determining whether enclosed tags are global or
 *  static.
 */
extern boolean isIncludeFile (const char *const fileName)
{
    return FALSE;
}

/* tags_ignore is a NULL-terminated array of strings, read from ~/.config/geany/ignore.tags.
 * This file contains a space or newline separated list of symbols which should be ignored
 * by the C/C++ parser, see -I command line option of ctags for details. */
gchar **c_tags_ignore = NULL;

/*  Determines whether or not "name" should be ignored, per the ignore list.
 */
extern boolean isIgnoreToken (const char *const name,
			      boolean *const pIgnoreParens,
			      const char **const replacement)
{
	boolean result = FALSE;

	if (c_tags_ignore != NULL)
	{
		const size_t nameLen = strlen (name);
		unsigned int i;
		guint len = g_strv_length (c_tags_ignore);
		vString *token = vStringNew();

		if (pIgnoreParens != NULL)
			*pIgnoreParens = FALSE;

		for (i = 0  ;  i < len ;  ++i)
		{
			size_t tokenLen;

			vStringCopyS (token, c_tags_ignore[i]);
			vStringTerminate (token);
			tokenLen = vStringLength (token);

			if (tokenLen >= 2 && vStringChar (token, tokenLen - 1) == '*' &&
				strncmp (vStringValue (token), name, tokenLen - 1) == 0)
			{
				result = TRUE;
				break;
			}
			if (strncmp (vStringValue (token), name, nameLen) == 0)
			{
				if (nameLen == tokenLen)
				{
					result = TRUE;
					break;
				}
				else if (tokenLen == nameLen + 1  &&
						vStringChar (token, tokenLen - 1) == '+')
				{
					result = TRUE;
					if (pIgnoreParens != NULL)
						*pIgnoreParens = TRUE;
					break;
				}
				else if (vStringChar (token, nameLen) == '=')
				{
					if (replacement != NULL)
						*replacement = vStringValue (token) + nameLen + 1;
					break;
				}
			}
		}
		vStringDelete (token);
	}
	return result;
}

void addIgnoreListFromFile (const char *const fileName)
{
    stringList* tokens = stringListNewFromFile (fileName);
    if (Option.ignore == NULL)
	Option.ignore = tokens;
    else
	stringListCombine (Option.ignore, tokens);
}



/*
 *  Generic option parsing
 */
#define readOptionConfiguration
#define initOptions
#define freeOptionResources

/* vi:set tabstop=8 shiftwidth=4: */
