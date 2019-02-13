/**
 *   Copyright (c) 2015, Miloslav Nenadál <nenadalm@gmail.com>
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License version 2 or (at your option) any later version.
 *
 *   This module contains code for generating tags for the Clojure language.
 */

#include "general.h"

#include <string.h>

#include "parse.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"
#include "entry.h"

typedef enum {
	K_FUNCTION,
	K_NAMESPACE
} clojureKind;

static kindOption ClojureKinds[] = {
	{true, 'f', "function", "functions"},
	{true, 'n', "namespace", "namespaces"},
};

static int isNamespace (const char *strp)
{
	return strncmp (++strp, "ns", 2) == 0 && isspace (strp[2]);
}

static int isFunction (const char *strp)
{
	return strncmp (++strp, "defn", 4) == 0 && isspace (strp[4]);
}

static int isQuote (const char *strp)
{
	return strncmp (++strp, "quote", 5) == 0 && isspace (strp[5]);
}

static void functionName (vString * const name, const char *dbp)
{
	const char *p;

	if (*dbp == '\'')
		dbp++;
	else if (*dbp == '(' && isQuote (dbp))
	{
		dbp += 7;
		while (isspace (*dbp))
			dbp++;
	}

	for (p = dbp; *p != '\0' && *p != '(' && !isspace ((int) *p) && *p != ')';
		p++)
		vStringPut (name, *p);
}

const char* skipMetadata (const char *dbp)
{
	while (1)
	{
		if (*dbp == '^')
		{
			dbp++;
			if (*dbp == '{')
			{
				/* skipping an arraymap */
				for (; *dbp != '\0' && *dbp != '}'; dbp++)
					;
			}
			else
			{
				/* skip a keyword or a symbol */
				for (; *dbp != '\0' && !isspace((unsigned char)*dbp); dbp++)
					;
			}

			if (*dbp == '\0')
				break;

			dbp++;
			while (isspace ((unsigned char)*dbp))
				dbp++;
		}
		else
			break;
	}

	return dbp;
}

static int makeNamespaceTag (vString * const name, const char *dbp)
{
	dbp = skipMetadata (dbp);
	functionName (name, dbp);
	if (vStringLength (name) > 0 && ClojureKinds[K_NAMESPACE].enabled)
	{
		tagEntryInfo tag;
		initTagEntry (&tag, vStringValue (name), &(ClojureKinds[K_NAMESPACE]));
		tag.lineNumber = getInputLineNumber ();
		tag.filePosition = getInputFilePosition ();
		makeTagEntry (&tag);
		return K_FUNCTION;
	}
	else
		return K_NAMESPACE;
}

static void makeFunctionTag (vString *const name, const char *dbp, int scope_index)
{
	functionName (name, dbp);
	if (vStringLength (name) > 0 && ClojureKinds[K_FUNCTION].enabled)
	{
		tagEntryInfo tag;
		initTagEntry (&tag, vStringValue (name), &(ClojureKinds[K_FUNCTION]));
		tag.lineNumber = getInputLineNumber ();
		tag.filePosition = getInputFilePosition ();
		tag.extensionFields.scopeKind = &(ClojureKinds[scope_index]);
		makeTagEntry (&tag);
	}
}

static void skipToSymbol (const char **p)
{
	while (**p != '\0' && !isspace ((int) **p))
		*p = *p + 1;
	while (isspace ((int) **p))
		*p = *p + 1;
}

static void findClojureTags (void)
{
	vString *name = vStringNew ();
	const char *p;
	int scope_index = K_FUNCTION;

	while ((p = (char *)readLineFromInputFile ()) != NULL)
	{
		vStringClear (name);

		while (isspace (*p))
			p++;

		if (*p == '(')
		{
			if (isNamespace (p))
			{
				skipToSymbol (&p);
				scope_index = makeNamespaceTag (name, p);
			}
			else if (isFunction (p))
			{
				skipToSymbol (&p);
				makeFunctionTag (name, p, &(ClojureKinds[scope_index]));
			}
		}
	}
	vStringDelete (name);
}

extern parserDefinition* ClojureParser (void)
{
	static const char *const extensions[] = {"clj", "cljs", "cljc", NULL};
	parserDefinition *def = parserNew ("Clojure");
	def->kinds            = ClojureKinds;
	def->kindCount        = ARRAY_SIZE (ClojureKinds);
	def->extensions       = extensions;
	def->parser           = findClojureTags;
	return def;
}
