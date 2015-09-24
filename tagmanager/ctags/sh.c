/*
*   Copyright (c) 2000-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for scripts for the
*   Bourne shell (and its derivatives, the Korn and Z shells).
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
    K_FUNCTION
} shKind;

static kindOption ShKinds [] = {
    { TRUE, 'f', "function", "functions"}
};

/*
*   FUNCTION DEFINITIONS
*/

/*  Reject any tag "main" from a file named "configure". These appear in
 *  here-documents in GNU autoconf scripts and will add a haystack to the
 *  needle.
 */
static boolean hackReject (const vString* const tagName)
{
    const char *const scriptName = baseFilename (vStringValue (File.name));
    boolean result = (boolean) (strcmp (scriptName, "configure") == 0  &&
			       strcmp (vStringValue (tagName), "main") == 0);
    return result;
}

static void findShTags (void)
{
    vString *name = vStringNew ();
    const unsigned char *line;

    while ((line = fileReadLine ()) != NULL)
    {
	const unsigned char* cp = line;

	while (isspace ((int) *cp))
	    ++cp;

	if (*cp == '#')
	    continue;

	boolean functionFound;
	boolean allAreDigits = TRUE;

	if (strncmp ((const char*) cp, "function", (size_t) 8) == 0  && isspace ((int) cp [8]))
	{
	    cp += 9;

	    while (isspace ((int) *cp))
		++cp;

	    if (*cp == '\0')
		continue;

	    functionFound = TRUE;
	    const unsigned char* start = cp;

	    do
	    {
		switch (*cp)
		{
		case '"':
		case '$':
		case '&':
		case '\'':
		case ')':
		case ';':
		case '<':
		case '=':
		case '>':
		case '`':
		    functionFound = FALSE;
		    break;
		default:
		    if (allAreDigits && ! isdigit ((int) *cp))
			allAreDigits = FALSE;

		    vStringPut (name, (int) *cp);
		    ++cp;
		}
	    }
	    while (functionFound && *cp != '\0' && ! isspace ((int) *cp) && *cp != '(');

	    if (cp == start)
		continue;

	    if (functionFound)
	    {
		if (allAreDigits)
		    functionFound = FALSE;
		else
		{
		    while (isspace ((int) *cp))
			++cp;

		    if (*cp == '(')
		    {
			while (isspace ((int) *++cp))
			    ;

			if (*cp != ')')
			    functionFound = FALSE;
		    }
		}
	    }
	}
	else
	{
	    if (! (isalnum ((int) *cp) || *cp == '_'))
		continue;

	    do
	    {
		if (! isdigit ((int) *cp))
		    allAreDigits = FALSE;

		vStringPut (name, (int) *cp);
		++cp;
	    } while (isalnum ((int) *cp) || *cp == '_');

	    functionFound = FALSE;

	    if (! allAreDigits)
	    {
		while (isspace ((int) *cp))
		    ++cp;

		if (*cp == '(')
		{
		    while (isspace ((int) *++cp))
			;

		    if (*cp == ')')
			functionFound = TRUE;
		}
	    }
	}

	vStringTerminate (name);

	if (functionFound && ! hackReject (name))
	    makeSimpleTag (name, ShKinds, K_FUNCTION);

	vStringClear (name);
    }

    vStringDelete (name);
}

extern parserDefinition* ShParser (void)
{
    static const char *const extensions [] = {
	"sh", "SH", "bsh", "bash", "ksh", "zsh", "ash", NULL
    };
    parserDefinition* def = parserNew ("Sh");
    def->kinds      = ShKinds;
    def->kindCount  = KIND_COUNT (ShKinds);
    def->extensions = extensions;
    def->parser     = findShTags;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
