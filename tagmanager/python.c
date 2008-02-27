/*
*
*   Copyright (c) 2000-2001, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Python language
*   files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */
#include <glib.h>
#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
    K_CLASS, K_FUNCTION, K_METHOD, K_VARIABLE
} pythonKind;

static kindOption PythonKinds [] = {
    { TRUE, 'c', "class",    "classes" },
    { TRUE, 'f', "function", "functions" },
    { TRUE, 'm', "member", "methods" },
    { TRUE, 'v', "variable", "variables" }
};

typedef struct _lastClass {
	gchar *name;
	gint indent;
} lastClass;

/*
*   FUNCTION DEFINITIONS
*/

static boolean isIdentifierFirstCharacter (int c)
{
	return (boolean) (isalpha (c) || c == '_');
}

static boolean isIdentifierCharacter (int c)
{
	return (boolean) (isalnum (c) || c == '_');
}


/* remove all previous classes with more indent than the current one */
static GList *clean_class_list(GList *list, gint indent)
{
	GList *tmp, *tmp2;

	tmp = g_list_first(list);
	while (tmp != NULL)
	{
		if (((lastClass*)tmp->data)->indent >= indent)
		{
			g_free(((lastClass*)tmp->data)->name);
			g_free(tmp->data);
			tmp2 = tmp->next;

			list = g_list_remove(list, tmp->data);
			tmp = tmp2;
		}
		else
		{
			tmp = tmp->next;
		}
	}

	return list;
}


static void findPythonTags (void)
{
    GList *parents = NULL, *tmp; /* list of classes which are around the token */
    vString *name = vStringNew ();
    gint indent;
    const unsigned char *line;
    boolean inMultilineString = FALSE;
    boolean wasInMultilineString = FALSE;
	lastClass *lastclass = NULL;
    boolean inFunction = FALSE;
    gint fn_indent = 0;

    while ((line = fileReadLine ()) != NULL)
    {
	const unsigned char *cp = line;
	indent = 0;
	while (*cp != '\0')
	{
	    if (*cp=='"' &&
		strncmp ((const char*) cp, "\"\"\"", (size_t) 3) == 0)
	    {
		inMultilineString = (boolean) !inMultilineString;
		if (! inMultilineString)
			wasInMultilineString = TRUE;
		cp += 3;
	    }
	    if (*cp=='\'' &&
		strncmp ((const char*) cp, "'''", (size_t) 3) == 0)
	    {
		inMultilineString = (boolean) !inMultilineString;
		if (! inMultilineString)
			wasInMultilineString = TRUE;
		cp += 3;
	    }

		if (*cp == '\0' || wasInMultilineString)
		{
			wasInMultilineString = FALSE;
			break;	/* at end of multiline string */
		}

		/* update indent-sensitive things */
		if (!inMultilineString && !isspace(*cp))
		{
			if (inFunction)
			{
				if (indent < fn_indent)
					inFunction = FALSE;
			}
		    if (lastclass != NULL)
		    {
				if (indent <= lastclass->indent)
				{
					GList *last;

					parents = clean_class_list(parents, indent);
					last = g_list_last(parents);
					if (last != NULL)
						lastclass = last->data;
					else
						lastclass = NULL;
				}
		    }
		}

	    if (inMultilineString)
		++cp;
		else if (isspace ((int) *cp))
		{
			/* count indentation amount of current line
			 * the indentation has to be made with tabs only _or_ spaces only, if they are mixed
			 * the code below gets confused */
			if (cp == line)
			{
				do
				{
					indent++;
					cp++;
				} while (isspace(*cp));
			}
			else
				cp++;	/* non-indent whitespace */
		}
	    else if (*cp == '#')
		break;
	    else if (strncmp ((const char*) cp, "class", (size_t) 5) == 0)
	    {
			cp += 5;
			if (isspace ((int) *cp))
			{
				lastClass *newclass = g_new(lastClass, 1);

				while (isspace ((int) *cp))
				++cp;
				while (isalnum ((int) *cp)  ||  *cp == '_')
				{
				vStringPut (name, (int) *cp);
				++cp;
				}
				vStringTerminate (name);

				newclass->name = g_strdup(vStringValue(name));
				newclass->indent = indent;
				parents = g_list_append(parents, newclass);
				if (lastclass == NULL)
					makeSimpleTag (name, PythonKinds, K_CLASS);
				else
					makeSimpleScopedTag (name, PythonKinds, K_CLASS,
						PythonKinds[K_CLASS].name, lastclass->name, "public");
				vStringClear (name);

				lastclass = newclass;
				break;	/* ignore rest of line so that lastclass is not reset immediately */
			}
	    }
	    else if (strncmp ((const char*) cp, "def", (size_t) 3) == 0)
	    {
		cp += 3;
		if (isspace ((int) *cp))
		{
		    while (isspace ((int) *cp))
			++cp;
		    while (isalnum ((int) *cp)  ||  *cp == '_')
		    {
			vStringPut (name, (int) *cp);
			++cp;
		    }
		    vStringTerminate (name);
		    if (!isspace(*line) || lastclass == NULL || strlen(lastclass->name) <= 0)
			makeSimpleTag (name, PythonKinds, K_FUNCTION);
		    else
			makeSimpleScopedTag (name, PythonKinds, K_METHOD,
					     PythonKinds[K_CLASS].name, lastclass->name, "public");
		    vStringClear (name);

		    inFunction = TRUE;
		    fn_indent = indent + 1;
		    break;	/* ignore rest of line so inFunction is not cancelled immediately */
		}
	    }
		else if (!inFunction && *(const char*)cp == '=')
		{
			/* Parse global and class variable names (C.x) from assignment statements.
			 * Object attributes (obj.x) are ignored.
			 * Assignment to a tuple 'x, y = 2, 3' not supported.
			 * TODO: ignore duplicate tags from reassignment statements. */
			const guchar *sp, *eq, *start;

			eq = cp + 1;
			while (*eq)
			{
				if (*eq == '=')
					goto skipvar;	/* ignore '==' operator and 'x=5,y=6)' function lines */
				if (*eq == '(')
					break;	/* allow 'x = func(b=2,y=2,' lines */
				eq++;
			}
			/* go backwards to the start of the line, checking we have valid chars */
			start = cp - 1;
			while (start >= line && isspace ((int) *start))
				--start;
			while (start >= line && isIdentifierCharacter ((int) *start))
				--start;
			if (!isIdentifierFirstCharacter(*(start + 1)))
				goto skipvar;
			sp = start;
			while (sp >= line && isspace ((int) *sp))
				--sp;
			if ((sp + 1) != line)	/* the line isn't a simple variable assignment */
				goto skipvar;
			/* the line is valid, parse the variable name */
			++start;
			while (isIdentifierCharacter ((int) *start))
			{
				vStringPut (name, (int) *start);
				++start;
			}
			vStringTerminate (name);

			if (lastclass == NULL)
				makeSimpleTag (name, PythonKinds, K_VARIABLE);
			else
				makeSimpleScopedTag (name, PythonKinds, K_VARIABLE,
					PythonKinds[K_CLASS].name, lastclass->name, "public");	/* class member variables */

			vStringClear (name);

			skipvar:
			++cp;
		}
	    else if (*cp != '\0')
	    {
		do
		    ++cp;
		while (isalnum ((int) *cp)  ||  *cp == '_');
	    }
	}
    }
    vStringDelete (name);

    /* clear the remaining elements in the list */
    tmp = g_list_first(parents);
    while (tmp != NULL)
    {
    	if (tmp->data)
    	{
			g_free(((lastClass*)tmp->data)->name);
			g_free(tmp->data);
    	}
    	tmp = tmp->next;
    }
    g_list_free(parents);
}

extern parserDefinition* PythonParser (void)
{
    static const char *const extensions [] = { "py", "python", NULL };
    parserDefinition* def = parserNew ("Python");
    def->kinds      = PythonKinds;
    def->kindCount  = KIND_COUNT (PythonKinds);
    def->extensions = extensions;
    def->parser     = findPythonTags;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
