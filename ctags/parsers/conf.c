/*
*
*   Copyright (c) 2000-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for config files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <ctype.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"
#include "routines.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_SECTION,
	K_KEY
} confKind;

static kindOption ConfKinds [] = {
	{ true, 'n', "namespace",  "sections"},
	{ true, 'm', "macro", "keys"}
};

/*
*   FUNCTION DEFINITIONS
*/

static bool isIdentifier (int c)
{
	/* allow whitespace within keys and sections */
	return (bool)(isalnum (c) || isspace (c) ||  c == '_');
}

static void findConfTags (void)
{
	vString *name = vStringNew ();
	vString *scope = vStringNew ();
	const unsigned char *line;

	while ((line = readLineFromInputFile ()) != NULL)
	{
		const unsigned char* cp = line;
		bool possible = true;

		if (isspace ((int) *cp) || *cp == '#' || (*cp != '\0' && *cp == '/' && *(cp+1) == '/'))
			continue;

		/* look for a section */
		if (*cp != '\0' && *cp == '[')
		{
			++cp;
			while (*cp != '\0' && *cp != ']')
			{
				vStringPut (name, (int) *cp);
				++cp;
			}
			makeSimpleTag (name, ConfKinds, K_SECTION);
			/* remember section name */
			vStringCopy (scope, name);
			vStringClear (name);
			continue;
		}

		while (*cp != '\0')
		{
			/*  We look for any sequence of identifier characters following a white space */
			if (possible && isIdentifier ((int) *cp))
			{
				while (isIdentifier ((int) *cp))
				{
					vStringPut (name, (int) *cp);
					++cp;
				}
				vStringStripTrailing (name);
				while (isspace ((int) *cp))
					++cp;
				if (*cp == '=')
				{
					tagEntryInfo e;
					initTagEntry (&e, vStringValue (name), &(ConfKinds [K_KEY]));

					if (vStringLength (scope) > 0)
					{
						e.extensionFields.scopeKind = &(ConfKinds [K_SECTION]);
						e.extensionFields.scopeName = vStringValue(scope);
					}
					makeTagEntry (&e);
				}
				vStringClear (name);
			}
			else if (isspace ((int) *cp))
				possible = true;
			else
				possible = false;

			if (*cp != '\0')
				++cp;
		}
	}
	vStringDelete (name);
	vStringDelete (scope);
}

extern parserDefinition* ConfParser (void)
{
	static const char *const patterns [] = { "*.ini", "*.conf", NULL };
	static const char *const extensions [] = { "conf", NULL };
	parserDefinition* const def = parserNew ("Conf");
	def->kinds      = ConfKinds;
	def->kindCount  = ARRAY_SIZE (ConfKinds);
	def->patterns   = patterns;
	def->extensions = extensions;
	def->parser     = findConfTags;
	return def;
}
