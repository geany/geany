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
    K_MACRO
} shKind;

static kindOption ConfKinds [] = {
    { TRUE, 'm', "macro", "macros"}
};

/*
*   FUNCTION DEFINITIONS
*/

static boolean isIdentifier (int c)
{
    return (boolean)(isalnum (c)  ||  c == '_');
}

static void findConfTags (void)
{
    vString *name = vStringNew ();
    const unsigned char *line;

    while ((line = fileReadLine ()) != NULL)
    {
	const unsigned char* cp = line;
	boolean possible = TRUE;

	while (isspace ((int) *cp))
	    ++cp;
	if (*cp == '#')
	    continue;

	while (*cp != '\0')
	{
	    /*  We look for any sequence of identifier characters following
	     *  either a white space or a colon and followed by either = or :=
	     */
	    if (possible && isIdentifier ((int) *cp))
	    {
		while (isIdentifier ((int) *cp))
		{
		    vStringPut (name, (int) *cp);
		    ++cp;
		}
		vStringTerminate (name);
		while (isspace ((int) *cp))
		    ++cp;
		if ( *cp == ':')
		    ++cp;
		if ( *cp == '=')
		    makeSimpleTag (name, ConfKinds, K_MACRO);
		vStringClear (name);
	    }
	    else if (isspace ((int) *cp) ||  *cp == ':')
		possible = TRUE;
	    else
		possible = FALSE;
	    if (*cp != '\0')
		++cp;
	}
    }
    vStringDelete (name);
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
