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
    K_CLASS, K_FUNCTION, K_METHOD
} pythonKind;

static kindOption PythonKinds [] = {
    { TRUE, 'c', "class",    "classes" },
    { TRUE, 'f', "function", "functions" },
    { TRUE, 'm', "member", "methods" }
};

typedef struct _lastClass {
	gchar *name;
	gint indent;
} lastClass;

/*
*   FUNCTION DEFINITIONS
*/


// remove all previous classes with more indent than the current one
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
    GList *parents = NULL, *tmp; // list of classes which are around the token
    vString *name = vStringNew ();
    gint indent;
    const unsigned char *line;
    boolean inMultilineString = FALSE;

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
		cp += 3;
	    }
	    if (*cp=='\'' &&
		strncmp ((const char*) cp, "'''", (size_t) 3) == 0)
	    {
		inMultilineString = (boolean) !inMultilineString;
		cp += 3;
	    }

	    if (inMultilineString)
		++cp;
		else if (isspace ((int) *cp))
		{
			cp++;
			// count indentation amount of current line
			// the indentation has to be made with tabs only _or_ spaces only, if they are mixed
			// the code below gets confused
			indent++;
		}
	    else if (*cp == '#')
		break;
	    else if (strncmp ((const char*) cp, "class", (size_t) 5) == 0)
	    {
			cp += 5;
			if (isspace ((int) *cp))
			{
				GList *last = g_list_last(parents);
				lastClass *lastclass = NULL;
				lastClass *newclass = g_new(lastClass, 1);

				if (last != NULL) lastclass = last->data;

				while (isspace ((int) *cp))
				++cp;
				while (isalnum ((int) *cp)  ||  *cp == '_')
				{
				vStringPut (name, (int) *cp);
				++cp;
				}
				vStringTerminate (name);

				parents = clean_class_list(parents, indent);

				newclass->name = g_strdup(vStringValue(name));
				newclass->indent = indent;
				parents = g_list_append(parents, newclass);
				makeSimpleTag (name, PythonKinds, K_CLASS);
				vStringClear (name);
			}
	    }
	    else if (strncmp ((const char*) cp, "def", (size_t) 3) == 0)
	    {
		cp += 3;
		if (isspace ((int) *cp))
		{
		    GList *last;
		    lastClass *lastclass = NULL;

			parents = clean_class_list(parents, indent);
			last = g_list_last(parents);
			if (last != NULL) lastclass = last->data;

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
		}
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

    // clear the remaining elements in the list
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
