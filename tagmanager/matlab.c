/*
*
*   Copyright (c) 2000-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Matlab scripts.
*   The tags 'function' and 'struct' are parsed.
*	Author Roland Baudin <roland65@free.fr>
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
    K_FUNCTION,
	K_STRUCT
} MatlabKind;

static kindOption MatlabKinds [] = {
    { TRUE, 'f', "function", "Functions" },
	{ TRUE, 's', "struct", "Structures" },
};

/*
*   FUNCTION DEFINITIONS
*/

static void findMatlabTags (void)
{
    vString *name = vStringNew ();
    const unsigned char *line;
	const unsigned char *p;

    while ((line = fileReadLine ()) != NULL)
    {
		int i, ic;

		if (line [0] == '\0'  ||  line [0] == '%')
			continue;

		/* search if the line has a comment */
		for (ic = 0  ;  line [ic] != '\0'  &&  line [ic]!='%'  ;  ++ic)
			;

		/* function tag */

		/* read first word */
		for (i = 0  ;  line [i] != '\0'  &&  ! isspace (line [i])  ;  ++i)
			;

		if (strncmp ((const char *) line, "function", (size_t) 8) == 0)
		{
			const unsigned char *cp = line + i;
			const unsigned char *ptr = cp;
			boolean eq=FALSE;

			while (isspace ((int) *cp))
				++cp;

			/* search for '=' character in the line */
			while (*ptr != '\0')
			{
				if (*ptr == '=')
				{
					eq=TRUE;
					break;
				}
				ptr++;
			}

			/* '=' was found => get the right most part of the line after '=' and before '%' */
			if (eq)
			{
				ptr++;
				while (isspace ((int) *ptr))
					++ptr;

				while (*ptr != '\0' && *ptr != '%')
				{
					vStringPut (name, (int) *ptr);
					++ptr;
				}
			}

			/* '=' was not found => get the right most part of the line after
			 * 'function' and before '%' */
			else
			{
				while (*cp != '\0' && *cp != '%')
				{
					vStringPut (name, (int) *cp);
					++cp;
				}
			}

			vStringTerminate (name);
			makeSimpleTag (name, MatlabKinds, K_FUNCTION);
			vStringClear (name);
		}

		/* struct tag */

		/* search if the line contains the keyword 'struct' */
		p=(const unsigned char*) strstr ((const char*) line, "struct");

		/* and avoid the part after the '%' if any */
		if ( p != NULL && ic>0 && p < line+ic)
		{
			const unsigned char *cp = line;

			/* get the left most part of the line before '=' */
			while (*cp != '\0' && !isspace(*cp) && *cp != '=' )
			{
				vStringPut (name, (int) *cp);
				++cp;
			}

			vStringTerminate (name);
			makeSimpleTag (name, MatlabKinds, K_STRUCT);
			vStringClear (name);
		}
    }
    vStringDelete (name);
}

extern parserDefinition* MatlabParser (void)
{
    static const char *const extensions [] = { "m", NULL };
    parserDefinition* def = parserNew ("Matlab");
    def->kinds      = MatlabKinds;
    def->kindCount  = KIND_COUNT (MatlabKinds);
    def->extensions = extensions;
    def->parser     = findMatlabTags;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
