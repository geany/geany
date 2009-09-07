/*
*   Copyright (c) 2009, Eric Forgeot
*
*   Based on work by Jon Strait
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Txt2tags files.
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

static kindOption Txt2tagsKinds[] = {
	{ TRUE, 'v', "members", "sections" },
	{ TRUE, 's', "struct",  "header1"}
};

/*
*   FUNCTION DEFINITIONS
*/

static void makeTxt2tagsTag (const vString* const name, boolean name_before)
{
	tagEntryInfo e;
	initTagEntry (&e, vStringValue(name));

	if (name_before)
		e.lineNumber--;	/* we want the line before the underline chars */
	e.kindName = "variable";
	e.kind = 'v';

	makeTagEntry(&e);
}

static void makeTxt2tagsTag2 (const vString* const name, boolean name_before)
{
	tagEntryInfo e;
	initTagEntry (&e, vStringValue(name));

	if (name_before)
		e.lineNumber--;
	e.kindName = "struct";
	e.kind = 's';

	makeTagEntry(&e);
}

static void findTxt2tagsTags (void)
{
	vString *name = vStringNew();
	const unsigned char *line;

	while ((line = fileReadLine()) != NULL)
	{
		/*int name_len = vStringLength(name);*/

		/* underlines must be the same length or more */
		/*if (name_len > 0 &&	(line[0] == '=' || line[0] == '-') && issame((const char*) line))
		{
			makeTxt2tagsTag(name, TRUE);
		}*/
		if (line[0] == '=') {
 			/*vStringClear(name);*/
			vStringCatS(name, (const char *) line);
			vStringTerminate(name);
			makeTxt2tagsTag(name, FALSE);
		}
		/*else if (line[0] == "=") {
			vStringClear(name);
			vStringCatS(name, (const char *) line);
			vStringTerminate(name);
			makeTxt2tagsTag(name, FALSE);
		}*/
		else if (strcmp((char*)line, "°") == 0) {
			/*vStringClear(name);*/
			vStringCatS(name, (const char *) line);
			vStringTerminate(name);
			makeTxt2tagsTag2(name, FALSE);
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

extern parserDefinition* Txt2tagsParser (void)
{
	static const char *const patterns [] = { "*.t2t", NULL };
	static const char *const extensions [] = { "t2t", NULL };
	parserDefinition* const def = parserNew ("Txt2tags");

	def->kinds = Txt2tagsKinds;
	def->kindCount = KIND_COUNT (Txt2tagsKinds);
	def->patterns = patterns;
	def->extensions = extensions;
	def->parser = findTxt2tagsTags;
	return def;
}

