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

/*
*   DATA DEFINITIONS
*/
typedef enum {
    K_SECTION,
    K_KEY
} confKind;

static kindOption ConfKinds [] = {
    { TRUE, 'n', "namespace",  "sections"},
    { TRUE, 'm', "macro", "keys"}
};

/*
*   FUNCTION DEFINITIONS
*/

static boolean isIdentifier (int c)
{
    /* allow whitespace within keys and sections */
    return (boolean)(isalnum (c) || isspace (c) ||  c == '_');
}

static void findConfTags (void)
{
    vString *name = vStringNew ();
    vString *scope = vStringNew ();
    const unsigned char *line;

    while ((line = fileReadLine ()) != NULL)
    {
		const unsigned char* cp = line;
		boolean possible = TRUE;

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
			vStringTerminate (name);
			makeSimpleTag (name, ConfKinds, K_SECTION);
			/* remember section name */
			vStringCopy (scope, name);
			vStringTerminate (scope);
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
				vStringTerminate (name);
				vStringStripTrailing (name);
				while (isspace ((int) *cp))
					++cp;
				if (*cp == '=')
				{
					if (vStringLength (scope) > 0)
						makeSimpleScopedTag (name, ConfKinds, K_KEY,
							"section", vStringValue(scope), NULL);
					else
						makeSimpleTag (name, ConfKinds, K_KEY);
				}
				vStringClear (name);
			}
			else if (isspace ((int) *cp))
				possible = TRUE;
			else
				possible = FALSE;

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
    def->kindCount  = KIND_COUNT (ConfKinds);
    def->patterns   = patterns;
    def->extensions = extensions;
    def->parser     = findConfTags;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
