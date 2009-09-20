/*
*   Copyright (c) 2009, Eric Forgeot
*
*   Based on work by Jon Strait
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Abc files.
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

static kindOption AbcKinds[] = {
	{ TRUE, 'm', "member", "sections" },
	{ TRUE, 's', "struct",  "header1"}
};

/*
*   FUNCTION DEFINITIONS
*/

/* checks if str is all the same character */
/*static boolean issame(const char *str)
{
	char first = *str;

	while (*(++str))
	{
		if (*str && *str != first)
			return FALSE;
	}
	return TRUE;
}*/


static void makeAbcTag (const vString* const name, boolean name_before)
{
	tagEntryInfo e;
	initTagEntry (&e, vStringValue(name));

	if (name_before)
		e.lineNumber--;	/* we want the line before the underline chars */
	e.kindName = AbcKinds[0].name;
	e.kind = AbcKinds[0].letter;

	makeTagEntry(&e);
}

/*static void makeAbcTag2 (const vString* const name, boolean name_before)
{
	tagEntryInfo e;
	initTagEntry (&e, vStringValue(name));

	if (name_before)
		e.lineNumber--;
	e.kindName = "struct";
	e.kind = 's';

	makeTagEntry(&e);
}*/

static void findAbcTags (void)
{
	vString *name = vStringNew();
	const unsigned char *line;

	while ((line = fileReadLine()) != NULL)
	{
		/*int name_len = vStringLength(name);*/

		/* underlines must be the same length or more */
		/*if (name_len > 0 &&	(line[0] == '=' || line[0] == '-') && issame((const char*) line))
		{
			makeAbcTag(name, TRUE);
		}*/
/*		if (line[1] == '%') {
			vStringClear(name);
			vStringCatS(name, (const char *) line);
			vStringTerminate(name);
			makeAbcTag(name, FALSE);
		}*/
		if (line[0] == 'T') {
			/*vStringClear(name);*/
			vStringCatS(name, " / ");
			vStringCatS(name, (const char *) line);
			vStringTerminate(name);
			makeAbcTag(name, FALSE);
		}
		else {
			vStringClear (name);
			if (! isspace(*line))
				vStringCatS(name, (const char*) line);
			vStringTerminate(name);
		}
	}
	vStringDelete (name);
}

extern parserDefinition* AbcParser (void)
{
	static const char *const patterns [] = { "*.abc", NULL };
	static const char *const extensions [] = { "abc", NULL };
	parserDefinition* const def = parserNew ("Abc");

	def->kinds = AbcKinds;
	def->kindCount = KIND_COUNT (AbcKinds);
	def->patterns = patterns;
	def->extensions = extensions;
	def->parser = findAbcTags;
	return def;
}

