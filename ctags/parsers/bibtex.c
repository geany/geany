/*
 *   Copyright (c) 2000-2001, Jérôme Plût
 *   Copyright (c) 2006, Enrico Tröger
 *   Copyright (c) 2019, Mirco Schönfeld
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 *   This module contains functions for generating tags for source files
 *   for the BibTex formatting system. 
 *   https://en.wikipedia.org/wiki/BibTeX
 */

/*
*   INCLUDE FILES
*/
#include "general.h"    /* must always come first */

#include <ctype.h>
#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"
#include "routines.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_ARTICLE,
  K_BOOK,
  K_PAPER,
  K_THESIS,
  K_OTHER
} BibKind;

static kindDefinition BibKinds[] = {
	{ true, 'f', "function",        "@article @inbook @incollection" },
	{ true, 'c', "class",        "@book @booklet @proceedings" },
	{ true, 'm', "member",        "@inproceedings @conference" },
	{ true, 'v', "variable",        "@phdthesis @mastersthesis" },
	{ true, 's', "struct",        "@manual @misc @techreport" }
};

/*
*   FUNCTION DEFINITIONS
*/
#define BIB_LABEL  (1<<2)

static int getBibWord(const char * ref, const char **ptr)
{
	const char *p = *ptr;

	while ((*ref != '\0') && (*p != '\0') && (*ref == *p))
		ref++, p++;


	if (*ref)
		return false;

	*ptr = p;
	return true;
}

static void createBibTag(int flags, BibKind kind, const char * l)
{
	vString *name = vStringNew ();

	while ((*l == ' '))
		l++;
	if (flags & (BIB_LABEL))
	{
		if (*l != '{')
			goto no_tag;
		l++;

		do
		{
			vStringPut(name, (int) *l);
			++l;
		} while ((*l != '\0') && (*l != ','));
		if (name->buffer[0] != ',')
			makeSimpleTag(name, kind);
	}
	else
	{
		vStringPut(name, (int) *l);
		makeSimpleTag(name, kind);
	}

no_tag:
	vStringDelete(name);
}

static void findBibTags(void)
{
	const char *line;

	while ((line = (const char*)readLineFromInputFile()) != NULL)
	{
		const char *cp = line;
		/*int escaped = 0;*/

		for (; *cp != '\0'; cp++)
		{
			if (*cp == '%')
				break;
			if (*cp == '@')
			{
				cp++;

				if (getBibWord("article", &cp))
				{
					createBibTag(BIB_LABEL, K_ARTICLE, cp);
					continue;
				}else if (getBibWord("inbook", &cp))
				{
					createBibTag(BIB_LABEL, K_ARTICLE, cp);
					continue;
				}else if (getBibWord("incollection", &cp))
				{
					createBibTag(BIB_LABEL, K_ARTICLE, cp);
					continue;
				}else if (getBibWord("book", &cp))
				{
					createBibTag(BIB_LABEL, K_BOOK, cp);
					continue;
				}else if (getBibWord("booklet", &cp))
				{
					createBibTag(BIB_LABEL, K_BOOK, cp);
					continue;
				}else if (getBibWord("proceedings", &cp))
				{
					createBibTag(BIB_LABEL, K_BOOK, cp);
					continue;
				}else if (getBibWord("inproceedings", &cp))
				{
					createBibTag(BIB_LABEL, K_PAPER, cp);
					continue;
				}else if (getBibWord("conference", &cp))
				{
					createBibTag(BIB_LABEL, K_PAPER, cp);
					continue;
				}else if (getBibWord("phdthesis", &cp))
				{
					createBibTag(BIB_LABEL, K_THESIS, cp);
					continue;
				}else if (getBibWord("mastersthesis", &cp))
				{
					createBibTag(BIB_LABEL, K_THESIS, cp);
					continue;
				}else if (getBibWord("manual", &cp))
				{
					createBibTag(BIB_LABEL, K_OTHER, cp);
					continue;
				}else if (getBibWord("misc", &cp))
				{
					createBibTag(BIB_LABEL, K_OTHER, cp);
					continue;
				}else if (getBibWord("techreport", &cp))
				{
					createBibTag(BIB_LABEL, K_OTHER, cp);
					continue;
				}
			}
		}
	}
}

extern parserDefinition* BibParser (void)
{
	static const char *const extensions [] = { "bib", NULL };
	parserDefinition * def = parserNew ("Bib");
	def->kindTable  = BibKinds;
	def->kindCount  = ARRAY_SIZE (BibKinds);
	def->extensions = extensions;
	def->parser     = findBibTags;
	return def;
}
