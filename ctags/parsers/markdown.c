/*
*
*   Copyright (c) 2009, Jon Strait
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Markdown files.
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
#include "routines.h"

/*
*   DATA DEFINITIONS
*/

static kindOption MarkdownKinds[] = {
	{ true, 'v', "variable", "sections" }
};

/*
*   FUNCTION DEFINITIONS
*/

/* checks if str is all the same character */
static bool issame(const char *str)
{
	char first = *str;

	while (*(++str))
	{
		if (*str && *str != first)
			return false;
	}
	return true;
}

static void makeMarkdownTag (const vString* const name, bool name_before)
{
	tagEntryInfo e;
	initTagEntry (&e, vStringValue(name), &(MarkdownKinds [0]));

	if (name_before)
		e.lineNumber--;	/* we want the line before the underline chars */

	makeTagEntry(&e);
}


static void findMarkdownTags (void)
{
	vString *name = vStringNew();
	const unsigned char *line;

	while ((line = readLineFromInputFile()) != NULL)
	{
		int name_len = vStringLength(name);

		/* underlines must be the same length or more */
		if (name_len > 0 &&	(line[0] == '=' || line[0] == '-') && issame((const char*) line))
		{
			makeMarkdownTag(name, true);
		}
		else if (line[0] == '#') {
			vStringClear(name);
			vStringCatS(name, (const char *) line);
			makeMarkdownTag(name, false);
		}
		else {
			vStringClear (name);
			if (! isspace(*line))
				vStringCatS(name, (const char*) line);
		}
	}
	vStringDelete (name);
}

extern parserDefinition* MarkdownParser (void)
{
	static const char *const patterns [] = { "*.md", NULL };
	static const char *const extensions [] = { "md", NULL };
	parserDefinition* const def = parserNew ("Markdown");

	def->kinds = MarkdownKinds;
	def->kindCount = ARRAY_SIZE (MarkdownKinds);
	def->patterns = patterns;
	def->extensions = extensions;
	def->parser = findMarkdownTags;
	return def;
}

