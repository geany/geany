/*
*   Copyright (c) 2000-2002, Darren Hiebert
*   Copyright (c) 2009-2011, Enrico Tröger
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for NSIS scripts (based on sh.c).
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>

#include "parse.h"
#include "read.h"
#include "main.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_SECTION,
	K_FUNCTION,
	K_VARIABLE
} NsisKind;

static kindOption NsisKinds [] = {
	{ TRUE, 'n', "namespace", "sections"},
	{ TRUE, 'f', "function", "functions"},
	{ TRUE, 'v', "variable", "variables"}
};

/*
*   FUNCTION DEFINITIONS
*/

static void findNsisTags (void)
{
	vString *name = vStringNew ();
	const unsigned char *line;

	while ((line = fileReadLine ()) != NULL)
	{
		const unsigned char* cp = line;

		while (isspace (*cp))
			cp++;

		if (*cp == '#' || *cp == ';')
			continue;

		/* functions */
		if (strncasecmp ((const char*) cp, "function", (size_t) 8) == 0 &&
			isspace ((int) cp [8]))
		{
			cp += 8;
			/* skip all whitespace */
			while (isspace ((int) *cp))
				++cp;
			while (isalnum ((int) *cp) || *cp == '_' || *cp == '-' || *cp == '.' || *cp == '!')
			{
				vStringPut (name, (int) *cp);
				++cp;
			}
			vStringTerminate (name);
			makeSimpleTag (name, NsisKinds, K_FUNCTION);
			vStringClear (name);
		}
		/* variables */
		else if (strncasecmp ((const char*) cp, "var", (size_t) 3) == 0 &&
			isspace ((int) cp [3]))
		{
			cp += 3;
			/* skip all whitespace */
			while (isspace ((int) *cp))
				++cp;
			/* skip any flags */
			while (*cp == '/')
			{
				++cp;
				while (! isspace ((int) *cp))
					++cp;
				while (isspace ((int) *cp))
					++cp;
			}
			while (isalnum ((int) *cp) || *cp == '_')
			{
				vStringPut (name, (int) *cp);
				++cp;
			}
			vStringTerminate (name);
			makeSimpleTag (name, NsisKinds, K_VARIABLE);
			vStringClear (name);
		}
		/* sections */
		else if (strncasecmp ((const char*) cp, "section", (size_t) 7) == 0  &&
				 isspace ((int) cp [7]))
		{
			boolean in_quotes = FALSE;
			cp += 7;
			/* skip all whitespace */
			while (isspace ((int) *cp))
			++cp;
			while (isalnum ((int) *cp) || isspace ((int) *cp) ||
				   *cp == '_' || *cp == '-' || *cp == '.' || *cp == '!' || *cp == '"')
			{
				if (*cp == '"')
				{
					if (in_quotes)
						break;
					else
					{
						in_quotes = TRUE;
						++cp;
						continue;
					}
				}
				vStringPut (name, (int) *cp);
				++cp;
			}
			vStringTerminate (name);
			makeSimpleTag (name, NsisKinds, K_SECTION);
			vStringClear (name);
		}
	}
	vStringDelete (name);
}

extern parserDefinition* NsisParser (void)
{
	static const char *const extensions [] = {
		"nsi", "nsh", NULL
	};
	parserDefinition* def = parserNew ("NSIS");
	def->kinds      = NsisKinds;
	def->kindCount  = KIND_COUNT (NsisKinds);
	def->extensions = extensions;
	def->parser     = findNsisTags;
	return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
