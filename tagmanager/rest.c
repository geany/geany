/*
*
*   Copyright (c) 2007-2008, Nick Treleaven
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

/*
*   DATA DEFINITIONS
*/
typedef enum {
    K_CHAPTER,
    K_SECTION,
	K_SUBSECTION,
	K_SUBSUBSECTION
} restKind;

static kindOption RestKinds[] = {
	{ TRUE, 'n', "namespace",     "chapters"},
	{ TRUE, 'm', "member",        "sections" },
	{ TRUE, 'd', "macro",         "subsections" },
	{ TRUE, 'v', "variable",      "subsubsections" }
};

/*
*   FUNCTION DEFINITIONS
*/

static void makeRestTag (const vString* const name,
			   kindOption* const kinds, const int kind)
{
    if (name != NULL  &&  vStringLength (name) > 0)
    {
        tagEntryInfo e;
        initTagEntry (&e, vStringValue (name));

        e.lineNumber--;	/* we want the line before the '---' underline chars */
        e.kindName = kinds [kind].name;
        e.kind     = kinds [kind].letter;

        makeTagEntry (&e);
    }
}

/* TODO: Parse any section with ispunct() underlining, in the order of first use.
 * Also parse overlining & underlining as higher-level sections. */
static void findRestTags (void)
{
    vString *name = vStringNew ();
    const unsigned char *line;

    while ((line = fileReadLine ()) != NULL)
    {
		int line_len = strlen((const char*) line);

		if (line_len >= 3 && vStringLength(name) > 0 &&
			! strstr((const char*) line, " "))	/* don't parse table borders */
		{
			if (strncmp((const char*) line, "===", 3) == 0)
			{
				makeRestTag(name, RestKinds, K_CHAPTER);
				continue;
			}
			else if (strncmp((const char*) line, "---", 3) == 0)
			{
				makeRestTag(name, RestKinds, K_SECTION);
				continue;
			}
			else if (strncmp((const char*) line, "^^^", 3) == 0)
			{
				makeRestTag(name, RestKinds, K_SUBSECTION);
				continue;
			}
			else if (strncmp((const char*) line, "```", 3) == 0)
			{
				makeRestTag(name, RestKinds, K_SUBSUBSECTION);
				continue;
			}
		}
		vStringClear (name);
		if (! isspace(*line))
			vStringCatS(name, (const char*) line);
		vStringTerminate(name);
    }
    vStringDelete (name);
}

extern parserDefinition* RestParser (void)
{
    static const char *const patterns [] = { "*.rest", "*.reST", NULL };
    static const char *const extensions [] = { "rest", NULL };
    parserDefinition* const def = parserNew ("reStructuredText");
    def->kinds      = RestKinds;
    def->kindCount  = KIND_COUNT (RestKinds);
    def->patterns   = patterns;
    def->extensions = extensions;
    def->parser     = findRestTags;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
