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
		const unsigned char *cp = line;

		while (isspace ((int) *cp))
			++cp;

		if (*cp == '#')
			continue;

		bool functionFound;
		bool allAreDigits = true;

		if (strncmp ((const char *) cp, "function", (size_t) 8) == 0  && isspace ((int) cp [8]))
		{
			cp += 9;

			while (isspace ((int) *cp))
				++cp;

			if (*cp == '\0')
				continue;

			functionFound = true;
			const unsigned char *start = cp;

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
					functionFound = false;
					break;
				default:
					if (allAreDigits && ! isdigit ((int) *cp))
						allAreDigits = false;

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
					functionFound = false;
				else
				{
					while (isspace ((int) *cp))
						++cp;

					if (*cp == '(')
					{
						while (isspace ((int) *++cp))
							;

						if (*cp != ')')
							functionFound = false;
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
					allAreDigits = false;

				vStringPut (name, (int) *cp);
				++cp;
			}
			while (isalnum ((int) *cp) || *cp == '_');

			functionFound = false;

			if (! allAreDigits)
			{
				while (isspace ((int) *cp))
					++cp;

				if (*cp == '(')
				{
					while (isspace ((int) *++cp))
						;

					if (*cp == ')')
						functionFound = true;
				}
			}
		}

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
	def->kindCount  = ARRAY_SIZE (ShKinds);
	def->extensions = extensions;
	def->parser     = findShTags;
	return def;
}
