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

typedef enum {
	K_SECTION = 0, K_HEADER
} Txt2tagsKind;

static kindOption Txt2tagsKinds[] = {
	{ TRUE, 'm', "member", "sections" },
	{ TRUE, 's', "struct",  "header1"}
};

/*
*   FUNCTION DEFINITIONS
*/

static void parse_title (vString* const name, const char control_char)
{
	char *text = vStringValue(name);
	char *p = text;
	int offset_start = 0;
	boolean in_or_after_title = FALSE;

	while (p != NULL && *p != '\0')
	{
		if (*p == control_char)
		{
			if (in_or_after_title)
				break;
			else
				offset_start++;
		}
		else
			in_or_after_title = TRUE;
		p++;
	}
	*p = '\0';
	vStringCopyS(name, text + offset_start);
	vStringStripLeading(name);
	vStringStripTrailing(name);
}

static void makeTxt2tagsTag (const vString* const name, boolean name_before, Txt2tagsKind type)
{
	tagEntryInfo e;
	kindOption *kind = &Txt2tagsKinds[type];
	initTagEntry (&e, vStringValue(name));

	if (name_before)
		e.lineNumber--;	/* we want the line before the underline chars */
	e.kindName = kind->name;
	e.kind = kind->letter;

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
		if (line[0] == '=' || line[0] == '+') {
 			/*vStringClear(name);*/
			vStringCatS(name, (const char *) line);
			vStringTerminate(name);
			parse_title(name, line[0]);
			makeTxt2tagsTag(name, FALSE, K_SECTION);
		}
		/* TODO what exactly should this match?
		 * K_HEADER ('struct') isn't matched in src/symbols.c */
		else if (strcmp((char*)line, "°") == 0) {
			/*vStringClear(name);*/
			vStringCatS(name, (const char *) line);
			vStringTerminate(name);
			makeTxt2tagsTag(name, FALSE, K_HEADER);
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

