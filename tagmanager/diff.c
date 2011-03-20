/*
*
*   Copyright (c) 2000-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for diff files (based on Sh parser).
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
	K_FUNCTION
} diffKind;

static kindOption DiffKinds [] = {
	{ TRUE, 'f', "function", "functions"}
};

/*
*   FUNCTION DEFINITIONS
*/

static void findDiffTags (void)
{
	vString *filename = vStringNew ();
	const unsigned char *line, *tmp;

	while ((line = fileReadLine ()) != NULL)
	{
		const unsigned char* cp = line;
		boolean skipSlash = FALSE;

		if (strncmp((const char*) cp, "--- ", (size_t) 4) == 0)
		{
			cp += 4;
			if (isspace ((int) *cp)) continue;

			/* strip any absolute path */
			if (*cp == '/' || *cp == '\\')
			{
				skipSlash = TRUE;
				tmp = (const unsigned char*) strrchr((const char*) cp,  '/');
				if (tmp == NULL)
				{	/* if no / is contained try \ in case of a Windows filename */
					tmp = (const unsigned char*) strrchr((const char*) cp, '\\');
					if (tmp == NULL)
					{	/* last fallback, probably the filename doesn't contain a path, so take it */
						if (cp[0] != 0)
						{
							tmp = cp;
							skipSlash = FALSE;
						}
					}
				}
			}
			else
				tmp = cp;

			if (tmp != NULL)
			{
				if (skipSlash) tmp++; /* skip the leading slash or backslash */
				while (! isspace(*tmp) && *tmp != '\0')
				{
					vStringPut(filename, *tmp);
					tmp++;
				}
				vStringTerminate(filename);
				makeSimpleTag (filename, DiffKinds, K_FUNCTION);
				vStringClear (filename);
			}
		}
	}
	vStringDelete (filename);
}

extern parserDefinition* DiffParser (void)
{
	static const char *const patterns [] = { "*.diff", "*.patch", NULL };
	static const char *const extensions [] = { "diff", NULL };
	parserDefinition* const def = parserNew ("Diff");
	def->kinds      = DiffKinds;
	def->kindCount  = KIND_COUNT (DiffKinds);
	def->patterns   = patterns;
	def->extensions = extensions;
	def->parser     = findDiffTags;
	return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
