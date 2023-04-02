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
#include "routines.h"
#include "read.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_FUNCTION,
	K_STRUCT
} MatlabKind;

static kindDefinition MatlabKinds [] = {
	{ true, 'f', "function", "Functions" },
	{ true, 's', "struct", "Structures" },
};

/*
*   FUNCTION DEFINITIONS
*/

static void findMatlabTags (void)
{
	vString *name = vStringNew ();
	const unsigned char *line;
	const unsigned char *p;
	unsigned int blkComm = 0; /* 0 = no comment, 1 = block comment, 2+ = nested */

	while ((line = readLineFromInputFile ()) != NULL)
	{
		int i, ic;

		/* check for opening/closing block comments (`%{`...`%}`); may be nested */
		for (i = 0; isspace ((int) line [i]); ++i)
			;

		if (strncmp ((const char *) (line + i), "%{", 2) == 0)
		{
			/* check if %{ appears on the line on its own (maybe with whitespace) */
			for (ic = i + 2; isspace ((int) line [ic]); ++ic)
				;

			if (line [ic] == '\0')
			{
				++blkComm; /* block nesting has incremented by 1 level */
				continue; /* skip to next line */
			}
		}
		else if (blkComm > 0 && strncmp ((const char *) (line + i), "%}", 2) == 0)
		{
			/* check if %} appears on the line on its own (maybe with whitespace) */
			for (ic = i+2; isspace ((int) line [ic]); ++ic)
				;

			if (line [ic] == '\0')
			{
				--blkComm; /* we've closed a nested block; when 0 we've left all nested comments */
				continue; /* skip to next line */
			}
		}

		if (blkComm > 0)
			continue; /* ignore lines within block comments */

		/* ignore lines that are comments */
		if (line [0] == '\0'  ||  line [0] == '%')
			continue;

		/* search if the line has a comment */
		for (ic = 0  ;  line [ic] != '\0'  &&  line [ic]!='%'  ;  ++ic)
			;

		/* function tag */

		/* read first word */
		for (i = 0  ;  line [i] != '\0'  &&  ! isspace (line [i])  ;  ++i)
			;

		if (strncmp ((const char *) line, "function", (size_t) 8) == 0
			&& isspace (line [8]))
		{
			const unsigned char *cp = line + i;
			const unsigned char *ptr = cp;
			bool eq=false;

			while (isspace ((int) *cp))
				++cp;

			/* search for '=' character in the line (ignoring comments) */
			while (*ptr != '\0')
			{
				if (*ptr == '%')
					break;

				if (*ptr == '=')
				{
					eq=true;
					break;
				}
				ptr++;
			}

			/* '=' was found => get the first word of the line after '=' */
			if (eq)
			{
				ptr++;
				while (isspace ((int) *ptr))
					++ptr;

				while (isalnum ((int) *ptr) || *ptr == '_')
				{
					vStringPut (name, (int) *ptr);
					++ptr;
				}
			}

			/* '=' was not found => get the first word of the line after "function" */
			else
			{
				while (isalnum ((int) *cp) || *cp == '_')
				{
					vStringPut (name, (int) *cp);
					++cp;
				}
			}

			makeSimpleTag (name, K_FUNCTION);
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

			makeSimpleTag (name, K_STRUCT);
			vStringClear (name);
		}
	}
	vStringDelete (name);
}

extern parserDefinition* MatLabParser (void)
{
	static const char *const extensions [] = { "m", NULL };
	parserDefinition* def = parserNew ("Matlab");
	def->kindTable  = MatlabKinds;
	def->kindCount  = ARRAY_SIZE (MatlabKinds);
	def->extensions = extensions;
	def->parser     = findMatlabTags;
	return def;
}
