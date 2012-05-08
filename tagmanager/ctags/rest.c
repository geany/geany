/*
*
*   Copyright (c) 2007-2011, Nick Treleaven
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for reStructuredText (reST) files.
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
#include "nestlevel.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_CHAPTER = 0,
	K_SECTION,
	K_SUBSECTION,
	K_SUBSUBSECTION,
	SECTION_COUNT
} restKind;

static kindOption RestKinds[] = {
	{ TRUE, 'n', "namespace",     "chapters"},
	{ TRUE, 'm', "member",        "sections" },
	{ TRUE, 'd', "macro",         "subsections" },
	{ TRUE, 'v', "variable",      "subsubsections" }
};

static char kindchars[SECTION_COUNT];

static NestingLevels *nestingLevels = NULL;

/*
*   FUNCTION DEFINITIONS
*/

static NestingLevel *getNestingLevel(const int kind)
{
	NestingLevel *nl;

	while (1)
	{
		nl = nestingLevelsGetCurrent(nestingLevels);
		if (nl && nl->type >= kind)
			nestingLevelsPop(nestingLevels);
		else
			break;
	}
	return nl;
}

static void makeRestTag (const vString* const name, const int kind)
{
	const NestingLevel *const nl = getNestingLevel(kind);

	if (vStringLength (name) > 0)
	{
		tagEntryInfo e;
		initTagEntry (&e, vStringValue (name));

		e.lineNumber--;	/* we want the line before the '---' underline chars */
		e.kindName = RestKinds [kind].name;
		e.kind = RestKinds [kind].letter;

		if (nl && nl->type < kind)
		{
			e.extensionFields.scope [0] = RestKinds [nl->type].name;
			e.extensionFields.scope [1] = vStringValue (nl->name);
		}
		makeTagEntry (&e);
	}
	nestingLevelsPush(nestingLevels, name, kind);
}


/* checks if str is all the same character */
static boolean issame(const char *str)
{
	char first = *str;

	while (*str)
	{
		char c;

		str++;
		c = *str;
		if (c && c != first)
			return FALSE;
	}
	return TRUE;
}


static int get_kind(char c)
{
	int i;

	for (i = 0; i < SECTION_COUNT; i++)
	{
		if (kindchars[i] == c)
			return i;

		if (kindchars[i] == 0)
		{
			kindchars[i] = c;
			return i;
		}
	}
	return -1;
}


/* TODO: parse overlining & underlining as distinct sections. */
static void findRestTags (void)
{
	vString *name = vStringNew ();
	const unsigned char *line;

	memset(kindchars, 0, sizeof kindchars);
	nestingLevels = nestingLevelsNew();

	while ((line = fileReadLine ()) != NULL)
	{
		int line_len = strlen((const char*) line);
		int name_len = vStringLength(name);

		/* underlines must be the same length or more */
		if (line_len >= name_len && name_len > 0 &&
			ispunct(line[0]) && issame((const char*) line))
		{
			char c = line[0];
			int kind = get_kind(c);

			if (kind >= 0)
			{
				makeRestTag(name, kind);
				continue;
			}
		}
		vStringClear (name);
		if (! isspace(*line))
			vStringCatS(name, (const char*) line);
		vStringTerminate(name);
	}
	vStringDelete (name);
	nestingLevelsFree(nestingLevels);
}

extern parserDefinition* RestParser (void)
{
	static const char *const patterns [] = { "*.rest", "*.reST", NULL };
	static const char *const extensions [] = { "rest", NULL };
	parserDefinition* const def = parserNew ("reStructuredText");

	def->kinds = RestKinds;
	def->kindCount = KIND_COUNT (RestKinds);
	def->patterns = patterns;
	def->extensions = extensions;
	def->parser = findRestTags;
	return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
