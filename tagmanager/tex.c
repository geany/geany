/*
 *   $Id$
 *
 *   Copyright (c) 2000-2001, Jérôme Plût
 *   Copyright (c) 2006, Enrico Tröger
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 *   This module contains functions for generating tags for source files
 *   for the TeX formatting system.
 */

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <ctype.h>
#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
     K_COMMAND,
     K_ENVIRONMENT,
     K_LABEL
} TeXKind;

static kindOption TeXKinds[] = {
     /* Commands - including \newcommand, \providecommand, \renewcommand,
      * \def, \DeclareMathOperator. */
     { TRUE, 'c', "class",      "command definitions"     },
     /* Environment - \newenvironment, \newtheorem */
     { TRUE, 'n', "namespace",  "environment definitions" },
     /* Labels - \label, \bibitem. */
     { TRUE, 't', "typedef",        "labels and bibliography" }
};

#define TEX_BRACES (1<<0)
#define TEX_BSLASH (1<<1)
#define TEX_LABEL  (1<<2)

/*
*   FUNCTION DEFINITIONS
*/

static int getWord (const char * ref, const unsigned char ** pointer)
{
     const char * p = *pointer;

     while ((*ref != '\0') && (*p != '\0') && (*ref == *p))
 	ref++, p++;

     if (*ref)
 	return FALSE;

     *pointer = p;
     return TRUE;
}

static void createTag (int flags, TeXKind kind, const char * l)
{
     vString *name = vStringNew ();

     while ((*l == ' '))
 	l++;
     if (flags & (TEX_BRACES | TEX_LABEL))
     {
 	if ((*(l++)) != '{')
 	    goto no_tag;
     }
     if (flags & TEX_BSLASH)
     {
 	if ((*(l++)) != '\\')
 	    goto no_tag;
     }
     if (flags & TEX_LABEL)
     {
 	do
 	{
 	    vStringPut (name, (int) *l);
 	    ++l;
 	} while ((*l != '\0') && (*l != '}'));
 	vStringTerminate (name);
 	makeSimpleTag (name, TeXKinds, kind);
     }
     else if (isalpha ((int) *l) || *l == '@')
     {
 	do
 	{
 	    vStringPut (name, (int) *l);
 	    ++l;
 	} while (isalpha ((int) *l) || *l == '@');
 	vStringTerminate (name);
 	makeSimpleTag (name, TeXKinds, kind);
     }
     else
     {
 	vStringPut (name, (int) *l);
 	vStringTerminate (name);
 	makeSimpleTag (name, TeXKinds, kind);
     }

 no_tag:
     vStringDelete (name);
}

static void findTeXTags (void)
{
    const unsigned char *line;

    while ((line = fileReadLine ()) != NULL)
    {
 	const unsigned char *cp = line;
 	//int escaped = 0;

 	for (; *cp != '\0'; cp++)
 	{
 	    if (*cp == '%')
 		break;
 	    if (*cp == '\\')
 	    {
 		cp++;

 		/* \newcommand{\command} */
 		if (getWord ("newcommand", &cp)
 		    || getWord ("providecommand", &cp)
 		    || getWord ("renewcommand", &cp)
 		    )
 		{
 		    createTag (TEX_BRACES|TEX_BSLASH, K_COMMAND, cp);
 		    continue;
 		}

 		/* \DeclareMathOperator{\command} */
 		if (getWord ("DeclareMathOperator", &cp))
 		{
 		    if (*cp == '*')
 			cp++;
 		    createTag (TEX_BRACES|TEX_BSLASH, K_COMMAND, cp);
 		    continue;
 		}

 		/* \def\command */
 		if (getWord ("def", &cp))
 		{
 		    createTag (TEX_BSLASH, K_COMMAND, cp);
 		    continue;
 		}

 		/* \newenvironment{name} */
 		if ( getWord ("newenvironment", &cp)
 		    || getWord ("newtheorem", &cp)
 		    )
 		{
 		    createTag (TEX_BRACES, K_ENVIRONMENT, cp);
 		    continue;
 		}

 		/* \bibitem[label]{key} */
 		if (getWord ("bibitem", &cp))
 		{
 		    while (*cp == ' ')
 			cp++;
 		    if (*(cp++) != '[')
 			break;
 		    while ((*cp != '\0') && (*cp != ']'))
 			cp++;
 		    if (*(cp++) != ']')
 			break;
 		    createTag (TEX_LABEL, K_LABEL, cp);
 		    continue;
 		}

 		/* \label{key} */
 		if (getWord ("label", &cp))
 		{
 		    createTag (TEX_LABEL, K_LABEL, cp);
 		    continue;
 		}
 	    }
 	}
    }
}

extern parserDefinition* TexParser (void)
{
	static const char *const extensions [] = { "tex", "sty", "cls", NULL };
	parserDefinition * def = parserNew ("Tex");
	def->kinds      = TeXKinds;
	def->kindCount  = KIND_COUNT (TeXKinds);
	def->extensions = extensions;
	def->parser     = findTeXTags;
	return def;
}

