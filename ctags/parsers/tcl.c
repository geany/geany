/*
*   Copyright (c) 2000-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for TCL scripts.
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

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_CLASS, K_METHOD, K_PROCEDURE, K_MODULE
} tclKind;

static kindDefinition TclKinds [] = {
	{ true, 'c', "class",     "classes" },
	{ true, 'm', "method",    "methods" },
	{ true, 'p', "procedure", "procedures" },
	{ true, 'n', "module",    "modules" }
};

/*
*   FUNCTION DEFINITIONS
*/

static const unsigned char *makeTclTag (
		const unsigned char *cp,
		vString *const name,
		const tclKind kind)
{
	vStringClear (name);
	while ((int) *cp != '\0'  &&  ! isspace ((int) *cp))
	{
		vStringPut (name, (int) *cp);
		++cp;
	}
	makeSimpleTag (name, kind);
	return cp;
}

static bool match (const unsigned char *line, const char *word)
{
	size_t len = strlen (word);
	bool matched = (strncmp ((const char*) line, word, len) == 0);

	if (matched)
	{
		/* check that the word is followed by a space to avoid detecting something
		 * like "proc_new ..." */
		matched = isspace (*(line + len));
	}
	return matched;
}

static void findTclTags (void)
{
	vString *name = vStringNew ();
	const unsigned char *line;

	while ((line = readLineFromInputFile ()) != NULL)
	{
		const unsigned char *cp;

		while (isspace (line [0])) 
			++line;
		
		if (line [0] == '\0'  ||  line [0] == '#')
			continue;

		/* read first word */
		for (cp = line ; *cp != '\0'  &&  ! isspace ((int) *cp) ; ++cp)
			;
		if (! isspace ((int) *cp))
			continue;
		while (isspace ((int) *cp))
			++cp;
		/* Now `line' points at first word and `cp' points at next word */

		if (match (line, "proc"))
			cp = makeTclTag (cp, name, K_PROCEDURE);
		else if (match (line, "class") || match (line, "itcl::class"))
			cp = makeTclTag (cp, name, K_CLASS);
		else if (match (line, "public") ||
				match (line, "protected") ||
				match (line, "private"))
		{
			if (match (cp, "method"))
			{
				cp += 6;
				while (isspace ((int) *cp))
					++cp;
				cp = makeTclTag (cp, name, K_METHOD);
			}
		}
		else if (match (line, "method"))
		{
			cp = makeTclTag (cp, name, K_METHOD);
		}
		else if (match (line, "oo::class") ) {
			if (match (cp, "create"))
			{
				cp += 6;
				while (isspace ((int) *cp))
					++cp;
				cp = makeTclTag (cp, name, K_CLASS);
			}
		}
		else if (match (line, "namespace") ) {
			if (match (cp, "eval"))
			{
				cp += 4;
				while (isspace ((int) *cp))
					++cp;
				cp = makeTclTag (cp, name, K_MODULE);
			}
		}

	}
	vStringDelete (name);
}

extern parserDefinition* TclParser (void)
{
	static const char *const extensions [] = { "tcl", "tk", "wish", "itcl", NULL };
	parserDefinition* def = parserNew ("Tcl");
	def->kindTable  = TclKinds;
	def->kindCount  = ARRAY_SIZE (TclKinds);
	def->extensions = extensions;
	def->parser     = findTclTags;
	return def;
}
