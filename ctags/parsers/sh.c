/*
*   Copyright (c) 2000-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for scripts for the
*   Bourne shell (and its derivatives, the Korn and Z shells).
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>

#include "parse.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"
#include "xtag.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_FUNCTION
} shKind;

static kindOption ShKinds [] = {
	{ true, 'f', "function", "functions"}
};

/*
*   FUNCTION DEFINITIONS
*/

/*  Reject any tag "main" from a file named "configure". These appear in
 *  here-documents in GNU autoconf scripts and will add a haystack to the
 *  needle.
 */
static bool hackReject (const vString* const tagName)
{
	const char *const scriptName = baseFilename (getInputFileName ());
	bool result = (bool) (strcmp (scriptName, "configure") == 0  &&
						  strcmp (vStringValue (tagName), "main") == 0);
	return result;
}

static void findShTags (void)
{
	vString *name = vStringNew ();
	const unsigned char *line;

	while ((line = readLineFromInputFile ()) != NULL)
	{
		const unsigned char* cp = line;
		bool functionFound = false;

		if (line [0] == '#')
			continue;

		while (isspace (*cp))
			cp++;
		if (strncmp ((const char*) cp, "function", (size_t) 8) == 0  &&
			isspace ((int) cp [8]))
		{
			functionFound = true;
			cp += 8;
			if (! isspace ((int) *cp))
				continue;
			while (isspace ((int) *cp))
				++cp;
		}
		if (! (isalnum ((int) *cp) || *cp == '_'))
			continue;
		while (isalnum ((int) *cp)  ||  *cp == '_')
		{
			vStringPut (name, (int) *cp);
			++cp;
		}
		while (isspace ((int) *cp))
			++cp;
		if (*cp++ == '(')
		{
			while (isspace ((int) *cp))
				++cp;
			if (*cp == ')'  && ! hackReject (name))
				functionFound = true;
		}
		if (functionFound)
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
	def->kindCount  = ARRAY_SIZE (ShKinds);
	def->extensions = extensions;
	def->parser     = findShTags;
	return def;
}
