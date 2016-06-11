/*
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
     K_SECTION,
     K_SUBSECTION,
     K_SUBSUBSECTION,
     K_CHAPTER,
     K_LABEL
} TeXKind;

static kindOption TeXKinds[] = {
     { TRUE, 'f', "function",      "command definitions" },
     { TRUE, 'c', "class",         "environment definitions" },
     { TRUE, 'm', "member",        "labels, sections and bibliography" },
     { TRUE, 'd', "macro",         "subsections" },
     { TRUE, 'v', "variable",      "subsubsections" },
     { TRUE, 'n', "namespace",     "chapters"},
     { TRUE, 's', "struct",        "labels and bibliography" }
};

#define TEX_BRACES (1<<0)
#define TEX_BSLASH (1<<1)
#define TEX_LABEL  (1<<2)

/*
*   FUNCTION DEFINITIONS
*/

static int getWord(const char * ref, const char **ptr)
{
    const char *p = *ptr;

	while ((*ref != '\0') && (*p != '\0') && (*ref == *p))
		ref++, p++;


    if (*ref)
		return FALSE;

	if (*p == '*') /* to allow something like \section*{foobar} */
		p++;

    *ptr = p;
	return TRUE;
}

static void createTag(int flags, TeXKind kind, const char * l)
{
    vString *name = vStringNew ();

    while ((*l == ' '))
 	l++;
    if (flags & (TEX_BRACES | TEX_LABEL))
    {
	if (*l == '[')
	{
	    while (*l != ']')
	    {
		if (*l == '\0')
		    goto no_tag;
		l++;
	    }
	    l++; /* skip the closing square bracket */
	}
	if (*l != '{')
 	    goto no_tag;
	l++;
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
 	    vStringPut(name, (int) *l);
 	    ++l;
 	} while ((*l != '\0') && (*l != '}'));
 	vStringTerminate(name);
 	if (name->buffer[0] != '}')
 		makeSimpleTag(name, TeXKinds, kind);
     }
     else if (isalpha((int) *l) || *l == '@')
     {
 	do
 	{
 	    vStringPut (name, (int) *l);
 	    ++l;
 	} while (isalpha((int) *l) || *l == '@');
 	vStringTerminate(name);
 	makeSimpleTag(name, TeXKinds, kind);
     }
     else
     {
 	vStringPut(name, (int) *l);
 	vStringTerminate(name);
 	makeSimpleTag(name, TeXKinds, kind);
     }

 no_tag:
     vStringDelete(name);
}

static void findTeXTags(void)
{
    const char *line;

    while ((line = (const char*)fileReadLine()) != NULL)
    {
 	const char *cp = line;
 	/*int escaped = 0;*/

 	for (; *cp != '\0'; cp++)
 	{
 	    if (*cp == '%')
			break;
 	    if (*cp == '\\')
 	    {
 		cp++;

 		/* \newcommand{\command} */
 		if (getWord("newcommand", &cp)
 		    || getWord("providecommand", &cp)
 		    || getWord("renewcommand", &cp)
 		    )
 		{
 		    createTag (TEX_BRACES|TEX_BSLASH, K_COMMAND, cp);
 		    continue;
 		}

 		/* \DeclareMathOperator{\command} */
 		else if (getWord("DeclareMathOperator", &cp))
 		{
 		    if (*cp == '*')
 			cp++;
 		    createTag(TEX_BRACES|TEX_BSLASH, K_COMMAND, cp);
 		    continue;
 		}

 		/* \def\command */
 		else if (getWord("def", &cp))
 		{
 		    createTag(TEX_BSLASH, K_COMMAND, cp);
 		    continue;
 		}

 		/* \newenvironment{name} */
 		else if ( getWord("newenvironment", &cp)
 		    || getWord("newtheorem", &cp)
 		    || getWord("begin", &cp)
 		    )
 		{
 		    createTag(TEX_BRACES, K_ENVIRONMENT, cp);
 		    continue;
 		}

 		/* \bibitem[label]{key} */
 		else if (getWord("bibitem", &cp))
 		{
 		    while (*cp == ' ')
 			cp++;
 		    if (*(cp++) != '[')
 			break;
 		    while ((*cp != '\0') && (*cp != ']'))
 			cp++;
 		    if (*(cp++) != ']')
 			break;
 		    createTag(TEX_LABEL, K_LABEL, cp);
 		    continue;
 		}

 		/* \label{key} */
 		else if (getWord("label", &cp))
 		{
 		    createTag(TEX_LABEL, K_LABEL, cp);
 		    continue;
 		}
 		/* \section{key} */
 		else if (getWord("section", &cp))
 		{
 		    createTag(TEX_LABEL, K_SECTION, cp);
 		    continue;
 		}
 		/* \subsection{key} */
 		else if (getWord("subsection", &cp))
 		{
 		    createTag(TEX_LABEL, K_SUBSECTION, cp);
 		    continue;
 		}
 		/* \subsubsection{key} */
 		else if (getWord("subsubsection", &cp))
 		{
 		    createTag(TEX_LABEL, K_SUBSUBSECTION, cp);
 		    continue;
 		}
 		/* \chapter{key} */
 		else if (getWord("chapter", &cp))
 		{
 		    createTag(TEX_LABEL, K_CHAPTER, cp);
 		    continue;
 		}
 	    }
 	}
    }
}

extern parserDefinition* LaTeXParser (void)
{
	static const char *const extensions [] = { "tex", "sty", "idx", NULL };
	parserDefinition * def = parserNew ("LaTeX");
	def->kinds      = TeXKinds;
	def->kindCount  = KIND_COUNT (TeXKinds);
	def->extensions = extensions;
	def->parser     = findTeXTags;
	return def;
}
