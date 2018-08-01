/*
*   Copyright (c) 2000-2002, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for LISP files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include "parse.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_FUNCTION
} lispKind;

static kindOption LispKinds [] = {
	{true, 'f', "function", "functions" }
};

/*
*   FUNCTION DEFINITIONS
*/

/*
 * lisp tag functions
 *  look for (def or (DEF, quote or QUOTE
 */
static int L_isdef (const unsigned char *strp)
{
	return ( (strp [1] == 'd' || strp [1] == 'D')
		  && (strp [2] == 'e' || strp [2] == 'E')
		  && (strp [3] == 'f' || strp [3] == 'F'));
}

static int L_isquote (const unsigned char *strp)
{
	return ( (*(++strp) == 'q' || *strp == 'Q')
		  && (*(++strp) == 'u' || *strp == 'U')
		  && (*(++strp) == 'o' || *strp == 'O')
		  && (*(++strp) == 't' || *strp == 'T')
		  && (*(++strp) == 'e' || *strp == 'E')
		  && isspace (*(++strp)));
}

static void L_getit (vString *const name, const unsigned char *dbp)
{
	const unsigned char *p;

	if (*dbp == '\'')  /* Skip prefix quote */
		dbp++;
	else if (*dbp == '(' && L_isquote (dbp))  /* Skip "(quote " */
	{
		dbp += 7;
		while (isspace (*dbp))
		dbp++;
	}
	for (p=dbp ; *p!='\0' && *p!='(' && !isspace ((int) *p) && *p!=')' ; p++)
		vStringPut (name, *p);

	if (vStringLength (name) > 0)
		makeSimpleTag (name, LispKinds, K_FUNCTION);
	vStringClear (name);
}

/* Algorithm adapted from from GNU etags.
 */
static void findLispTags (void)
{
	vString *name = vStringNew ();
	const unsigned char* p;

	while ((p = readLineFromInputFile ()) != NULL)
	{
		if (*p == '(')
		{
			if (L_isdef (p))
			{
				while (*p != '\0' && !isspace ((int) *p))
					p++;
				while (isspace ((int) *p))
					p++;
				L_getit (name, p);
			}
			else
			{
				/* Check for (foo::defmumble name-defined ... */
				do
					p++;
				while (*p != '\0' && !isspace ((int) *p)
						&& *p != ':' && *p != '(' && *p != ')');
				if (*p == ':')
				{
					do
						p++;
					while (*p == ':');

					if (L_isdef (p - 1))
					{
						while (*p != '\0' && !isspace ((int) *p))
							p++;
						while (isspace (*p))
							p++;
						L_getit (name, p);
					}
				}
			}
		}
	}
	vStringDelete (name);
}

extern parserDefinition* LispParser (void)
{
	static const char *const extensions [] = {"cl", "clisp", "el", "l", "lisp", "lsp", NULL};
	//static const char *const aliases [] = {"clisp", "emacs-lisp", NULL};
	parserDefinition* def = parserNew ("Lisp");
	def->kinds      = LispKinds;
	def->kindCount  = ARRAY_SIZE (LispKinds);
	def->extensions = extensions;
	//def->aliases    = aliases;
	def->parser     = findLispTags;
	return def;
}
