/*
*   Copyright (c) 2000-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for generating tags for Python language
*   files.
*/
/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>

#include "entry.h"
#include "nestlevel.h"
#include "options.h"
#include "read.h"
#include "main.h"
#include "vstring.h"
#include "routines.h"
#include "debug.h"
#include "xtag.h"

/*
*   DATA DEFINITIONS
*/

struct corkInfo {
	int index;
};

struct nestingLevelUserData {
	int indentation;
};
#define PY_NL_INDENTATION(nl) ((struct nestingLevelUserData *)nestingLevelGetUserData(nl))->indentation


typedef enum {
	K_CLASS, K_FUNCTION, K_METHOD, K_VARIABLE, K_IMPORT
} pythonKind;

static kindDefinition PythonKinds[] = {
	{true, 'c', "class",    "classes"},
	{true, 'f', "function", "functions"},
	{true, 'm', "member",   "class members"},
	{true, 'v', "variable", "variables"},
	{true, 'x', "unknown", "name referring a classe/variable/function/module defined in other module"}
};

typedef enum {
	A_PUBLIC, A_PRIVATE, A_PROTECTED
} pythonAccess;

static const char *const PythonAccesses[] = {
	"public", "private", "protected"
};

static char const * const singletriple = "'''";
static char const * const doubletriple = "\"\"\"";

/*
*   FUNCTION DEFINITIONS
*/

static bool isIdentifierFirstCharacter (int c)
{
	return (bool) (isalpha (c) || c == '_');
}

static bool isIdentifierCharacter (int c)
{
	return (bool) (isalnum (c) || c == '_');
}

/* follows PEP-8, and always reports single-underscores as protected
 * See:
 * - http://www.python.org/dev/peps/pep-0008/#method-names-and-instance-variables
 * - http://www.python.org/dev/peps/pep-0008/#designing-for-inheritance
 */
static pythonAccess accessFromIdentifier (const vString *const ident,
	pythonKind kind, bool has_parent, bool parent_is_class)
{
	const char *const p = vStringValue (ident);
	const size_t len = vStringLength (ident);

	/* inside a function/method, private */
	if (has_parent && !parent_is_class)
		return A_PRIVATE;
	/* not starting with "_", public */
	else if (len < 1 || p[0] != '_')
		return A_PUBLIC;
	/* "__...__": magic methods */
	else if (kind == K_METHOD && parent_is_class &&
			 len > 3 && p[1] == '_' && p[len - 2] == '_' && p[len - 1] == '_')
		return A_PUBLIC;
	/* "__...": name mangling */
	else if (parent_is_class && len > 1 && p[1] == '_')
		return A_PRIVATE;
	/* "_...": suggested as non-public, but easily accessible */
	else
		return A_PROTECTED;
}

static void addAccessFields (tagEntryInfo *const entry,
	const vString *const ident, pythonKind kind,
	bool has_parent, bool parent_is_class)
{
	pythonAccess access;

	access = accessFromIdentifier (ident, kind, has_parent, parent_is_class);
	entry->extensionFields.access = PythonAccesses [access];
	/* FIXME: should we really set isFileScope in addition to access? */
	if (access == A_PRIVATE)
		entry->isFileScope = true;
}

/* Given a string with the contents of a line directly after the "def" keyword,
 * extract all relevant information and create a tag.
 */
static struct corkInfo makeFunctionTag (vString *const function,
	vString *const parent, int is_class_parent, const char *arglist)
{
	tagEntryInfo tag;
	int corkIndex;
	struct corkInfo info;

	if (vStringLength (parent) > 0)
	{
		if (is_class_parent)
		{
			initTagEntry (&tag, vStringValue (function), K_METHOD);
			tag.extensionFields.scopeKindIndex = K_CLASS;
		}
		else
		{
			initTagEntry (&tag, vStringValue (function), K_FUNCTION);
			tag.extensionFields.scopeKindIndex = K_FUNCTION;
		}
		tag.extensionFields.scopeName = vStringValue (parent);
	}
	else
		initTagEntry (&tag, vStringValue (function), K_FUNCTION);

	tag.extensionFields.signature = arglist;

	addAccessFields (&tag, function, is_class_parent ? K_METHOD : K_FUNCTION,
		vStringLength (parent) > 0, is_class_parent);

	corkIndex = makeTagEntry (&tag);

	info.index = corkIndex;
	return info;
}

/* Given a string with the contents of the line directly after the "class"
 * keyword, extract all necessary information and create a tag.
 */
static int makeClassTag (vString *const class, vString *const inheritance,
	vString *const parent, int is_class_parent)
{
	tagEntryInfo tag;
	initTagEntry (&tag, vStringValue (class), K_CLASS);
	if (vStringLength (parent) > 0)
	{
		if (is_class_parent)
		{
			tag.extensionFields.scopeKindIndex = K_CLASS;
			tag.extensionFields.scopeName = vStringValue (parent);
		}
		else
		{
			tag.extensionFields.scopeKindIndex = K_FUNCTION;
			tag.extensionFields.scopeName = vStringValue (parent);
		}
	}
	tag.extensionFields.inheritance = vStringValue (inheritance);
	addAccessFields (&tag, class, K_CLASS, vStringLength (parent) > 0,
		is_class_parent);
	return makeTagEntry (&tag);
}

static void makeVariableTag (vString *const var, vString *const parent,
	bool is_class_parent)
{
	tagEntryInfo tag;
	initTagEntry (&tag, vStringValue (var), K_VARIABLE);
	if (vStringLength (parent) > 0)
	{
		tag.extensionFields.scopeKindIndex = K_CLASS;
		tag.extensionFields.scopeName = vStringValue (parent);
	}
	addAccessFields (&tag, var, K_VARIABLE, vStringLength (parent) > 0,
		is_class_parent);
	makeTagEntry (&tag);
}

/* Skip a single or double quoted string. */
static const char *skipString (const char *cp)
{
	const char *start = cp;
	int escaped = 0;
	for (cp++; *cp; cp++)
	{
		if (escaped)
			escaped--;
		else if (*cp == '\\')
			escaped++;
		else if (*cp == *start)
			return cp + 1;
	}
	return cp;
}

/* Skip everything up to an identifier start. */
static const char *skipEverything (const char *cp)
{
	int match;
	for (; *cp; cp++)
	{
		if (*cp == '#')
			return strchr(cp, '\0');

		match = 0;
		if (*cp == '"' || *cp == '\'')
			match = 1;

		/* these checks find unicode, binary (Python 3) and raw strings */
		if (!match)
		{
			bool r_first = (*cp == 'r' || *cp == 'R');

			/* "r" | "R" | "u" | "U" | "b" | "B" */
			if (r_first || *cp == 'u' || *cp == 'U' ||  *cp == 'b' || *cp == 'B')
			{
				unsigned int i = 1;

				/*  r_first -> "rb" | "rB" | "Rb" | "RB"
				   !r_first -> "ur" | "UR" | "Ur" | "uR" | "br" | "Br" | "bR" | "BR" */
				if (( r_first && (cp[i] == 'b' || cp[i] == 'B')) ||
					(!r_first && (cp[i] == 'r' || cp[i] == 'R')))
					i++;

				if (cp[i] == '\'' || cp[i] == '"')
				{
					match = 1;
					cp += i;
				}
			}
		}
		if (match)
		{
			cp = skipString(cp);
			if (!*cp) break;
		}
		if (isIdentifierFirstCharacter ((int) *cp))
			return cp;
		if (match)
			cp--; /* avoid jumping over the character after a skipped string */
	}
	return cp;
}

/* Skip an identifier. */
static const char *skipIdentifier (const char *cp)
{
	while (isIdentifierCharacter ((int) *cp))
		cp++;
	return cp;
}

static const char *findDefinitionOrClass (const char *cp)
{
	while (*cp)
	{
		cp = skipEverything (cp);
		if (!strncmp(cp, "def", 3) || !strncmp(cp, "class", 5) ||
			!strncmp(cp, "cdef", 4) || !strncmp(cp, "cpdef", 5))
		{
			return cp;
		}
		cp = skipIdentifier (cp);
	}
	return NULL;
}

static const char *skipSpace (const char *cp)
{
	while (isspace ((int) *cp))
		++cp;
	return cp;
}

/* Starting at ''cp'', parse an identifier into ''identifier''. */
static const char *parseIdentifier (const char *cp, vString *const identifier)
{
	vStringClear (identifier);
	while (isIdentifierCharacter ((int) *cp))
	{
		vStringPut (identifier, (int) *cp);
		++cp;
	}
	return cp;
}

static int parseClass (const char *cp, vString *const class,
	vString *const parent, int is_class_parent)
{
	int corkIndex;
	vString *const inheritance = vStringNew ();
	vStringClear (inheritance);
	cp = parseIdentifier (cp, class);
	cp = skipSpace (cp);
	if (*cp == '(')
	{
		++cp;
		while (*cp != ')')
		{
			if (*cp == '\0')
			{
				/* Closing parenthesis can be in follow up line. */
				cp = (const char *) readLineFromInputFile ();
				if (!cp) break;
				vStringPut (inheritance, ' ');
				continue;
			}
			vStringPut (inheritance, *cp);
			++cp;
		}
	}
	corkIndex = makeClassTag (class, inheritance, parent, is_class_parent);
	vStringDelete (inheritance);
	return corkIndex;
}

static void parseImports (const char *cp)
{
	const char *pos;
	vString *name, *name_next;

	cp = skipEverything (cp);

	if ((pos = strstr (cp, "import")) == NULL)
		return;

	cp = pos + 6;

	/* continue only if there is some space between the keyword and the identifier */
	if (! isspace (*cp))
		return;

	cp++;
	cp = skipSpace (cp);

	name = vStringNew ();
	name_next = vStringNew ();

	cp = skipEverything (cp);
	while (*cp)
	{
		cp = parseIdentifier (cp, name);

		cp = skipEverything (cp);
		/* we parse the next possible import statement as well to be able to ignore 'foo' in
		 * 'import foo as bar' */
		parseIdentifier (cp, name_next);

		/* take the current tag only if the next one is not "as" */
		if (strcmp (vStringValue (name_next), "as") != 0 &&
			strcmp (vStringValue (name), "as") != 0)
		{
			makeSimpleTag (name, K_IMPORT);
		}
	}
	vStringDelete (name);
	vStringDelete (name_next);
}

/* modified from lcpp.c getArglistFromStr().
 * warning: terminates rest of string past arglist!
 * note: does not ignore brackets inside strings! */
static char *parseArglist(const char *buf)
{
	char *start, *end;
	int level;
	if (NULL == buf)
		return NULL;
	if (NULL == (start = strchr(buf, '(')))
		return NULL;
	for (level = 1, end = start + 1; level > 0; ++end)
	{
		if ('\0' == *end)
			break;
		else if ('(' == *end)
			++ level;
		else if (')' == *end)
			-- level;
	}
	*end = '\0';
	return strdup(start);
}

static struct corkInfo parseFunction (const char *cp, vString *const def,
	vString *const parent, int is_class_parent)
{
	char *arglist;
	struct corkInfo info;

	cp = parseIdentifier (cp, def);
	arglist = parseArglist (cp);
	info = makeFunctionTag (def, parent, is_class_parent, arglist);
	if (arglist != NULL)
		eFree (arglist);
	return info;
}

/* Get the combined name of a nested symbol. Classes are separated with ".",
 * functions with "/". For example this code:
 * class MyClass:
 *     def myFunction:
 *         def SubFunction:
 *             class SubClass:
 *                 def Method:
 *                     pass
 * Would produce this string:
 * MyClass.MyFunction/SubFunction/SubClass.Method
 */
static bool constructParentString(NestingLevels *nls, int indent,
	vString *result)
{
	int i;
	NestingLevel *prev = NULL;
	int is_class = false;
	vStringClear (result);
	for (i = 0; i < nls->n; i++)
	{
		NestingLevel *nl = nestingLevelsGetNth (nls, i);
		tagEntryInfo *e;

		if (indent <= PY_NL_INDENTATION(nl))
			break;
		if (prev)
		{
			vStringCatS(result, ".");	/* make Geany symbol list grouping work properly */
/*
			if (prev->kindIndex == K_CLASS)
				vStringCatS(result, ".");
			else
				vStringCatS(result, "/");
*/
		}

		e = getEntryOfNestingLevel (nl);
		if (e)
		{
			vStringCatS(result, e->name);
			is_class = (e->kindIndex == K_CLASS);
		}
		else
			is_class = false;

		prev = nl;
	}
	return is_class;
}

/* Check indentation level and truncate nesting levels accordingly */
static void checkIndent(NestingLevels *nls, int indent)
{
	int i;
	NestingLevel *n;

	for (i = 0; i < nls->n; i++)
	{
		n = nestingLevelsGetNth (nls, i);
		if (n && indent <= PY_NL_INDENTATION(n))
		{
			/* truncate levels */
			nls->n = i;
			break;
		}
	}
}

static void addNestingLevel(NestingLevels *nls, int indentation, struct corkInfo *info)
{
	int i;
	NestingLevel *nl = NULL;

	for (i = 0; i < nls->n; i++)
	{
		nl = nestingLevelsGetNth(nls, i);
		if (indentation <= PY_NL_INDENTATION(nl)) break;
	}
	if (i == nls->n)
		nl = nestingLevelsPush(nls, info->index);
	else
		/* reuse existing slot */
		nl = nestingLevelsTruncate (nls, i + 1, info->index);

	PY_NL_INDENTATION(nl) = indentation;
}

/* Return a pointer to the start of the next triple string, or NULL. Store
 * the kind of triple string in "which" if the return is not NULL.
 */
static char const *find_triple_start(char const *string, char const **which)
{
	char const *cp = string;

	for (; *cp; cp++)
	{
		if (*cp == '#')
			break;
		if (*cp == '"' || *cp == '\'')
		{
			if (strncmp(cp, doubletriple, 3) == 0)
			{
				*which = doubletriple;
				return cp;
			}
			if (strncmp(cp, singletriple, 3) == 0)
			{
				*which = singletriple;
				return cp;
			}
			cp = skipString(cp);
			if (!*cp) break;
			cp--; /* avoid jumping over the character after a skipped string */
		}
	}
	return NULL;
}

/* Find the end of a triple string as pointed to by "which", and update "which"
 * with any other triple strings following in the given string.
 */
static void find_triple_end(char const *string, char const **which)
{
	char const *s = string;
	while (1)
	{
		/* Check if the string ends in the same line. */
		s = strstr (s, *which);
		if (!s) break;
		s += 3;
		*which = NULL;
		/* If yes, check if another one starts in the same line. */
		s = find_triple_start(s, which);
		if (!s) break;
		s += 3;
	}
}

static const char *findVariable(const char *line)
{
	/* Parse global and class variable names (C.x) from assignment statements.
	 * Object attributes (obj.x) are ignored.
	 * Assignment to a tuple 'x, y = 2, 3' not supported.
	 * TODO: ignore duplicate tags from reassignment statements. */
	const char *cp, *sp, *eq, *start;

	cp = strstr(line, "=");
	if (!cp)
		return NULL;
	eq = cp + 1;
	while (*eq)
	{
		if (*eq == '=')
			return NULL;	/* ignore '==' operator and 'x=5,y=6)' function lines */
		if (*eq == '(' || *eq == '#')
			break;	/* allow 'x = func(b=2,y=2,' lines and comments at the end of line */
		eq++;
	}

	/* go backwards to the start of the line, checking we have valid chars */
	start = cp - 1;
	while (start >= line && isspace ((int) *start))
		--start;
	while (start >= line && isIdentifierCharacter ((int) *start))
		--start;
	if (!isIdentifierFirstCharacter(*(start + 1)))
		return NULL;
	sp = start;
	while (sp >= line && isspace ((int) *sp))
		--sp;
	if ((sp + 1) != line)	/* the line isn't a simple variable assignment */
		return NULL;
	/* the line is valid, parse the variable name */
	++start;
	return start;
}

/* Skip type declaration that optionally follows a cdef/cpdef */
static const char *skipTypeDecl (const char *cp, bool *is_class)
{
	const char *lastStart = cp, *ptr = cp;
	int loopCount = 0;
	ptr = skipSpace(cp);
	if (!strncmp("extern", ptr, 6)) {
		ptr += 6;
		ptr = skipSpace(ptr);
		if (!strncmp("from", ptr, 4)) { return NULL; }
	}
	if (!strncmp("class", ptr, 5)) {
		ptr += 5 ;
		*is_class = true;
		ptr = skipSpace(ptr);
		return ptr;
	}
	/* limit so that we don't pick off "int item=obj()" */
	while (*ptr && loopCount++ < 2) {
		while (*ptr && *ptr != '=' && *ptr != '(' && !isspace(*ptr)) {
			/* skip over e.g. 'cpdef numpy.ndarray[dtype=double, ndim=1]' */
			if(*ptr == '[') {
				while (*ptr && *ptr != ']') ptr++;
				if (*ptr) ptr++;
			} else {
				ptr++;
			}
		}
		if (!*ptr || *ptr == '=') return NULL;
		if (*ptr == '(') {
			return lastStart; /* if we stopped on a '(' we are done */
		}
		ptr = skipSpace(ptr);
		lastStart = ptr;
		while (*lastStart == '*') lastStart++;  /* cdef int *identifier */
	}
	return NULL;
}

/* checks if there is a lambda at position of cp, and return its argument list
 * if so.
 * We don't return the lambda name since it is useless for now since we already
 * know it when we call this function, and it would be a little slower. */
static bool varIsLambda (const char *cp, char **arglist)
{
	bool is_lambda = false;

	cp = skipSpace (cp);
	cp = skipIdentifier (cp); /* skip the lambda's name */
	cp = skipSpace (cp);
	if (*cp == '=')
	{
		cp++;
		cp = skipSpace (cp);
		if (strncmp (cp, "lambda", 6) == 0)
		{
			const char *tmp;

			cp += 6; /* skip the lambda */
			tmp = skipSpace (cp);
			/* check if there is a space after lambda to detect assignations
			 * starting with 'lambdaXXX' */
			if (tmp != cp)
			{
				vString *args = vStringNew ();

				cp = tmp;
				vStringPut (args, '(');
				for (; *cp != 0 && *cp != ':'; cp++)
					vStringPut (args, *cp);
				vStringPut (args, ')');
				if (arglist)
					*arglist = strdup (vStringValue (args));
				vStringDelete (args);
				is_lambda = true;
			}
		}
	}
	return is_lambda;
}

/* checks if @p cp has keyword @p keyword at the start, and fills @p cp_n with
 * the position of the next non-whitespace after the keyword */
static bool matchKeyword (const char *keyword, const char *cp, const char **cp_n)
{
	size_t kw_len = strlen (keyword);
	if (strncmp (cp, keyword, kw_len) == 0 && isspace (cp[kw_len]))
	{
		*cp_n = skipSpace (&cp[kw_len + 1]);
		return true;
	}
	return false;
}

static void findPythonTags (void)
{
	vString *const continuation = vStringNew ();
	vString *const name = vStringNew ();
	vString *const parent = vStringNew();

	NestingLevels *const nesting_levels = nestingLevelsNew(sizeof (struct nestingLevelUserData));

	const char *line;
	int line_skip = 0;
	char const *longStringLiteral = NULL;

	while ((line = (const char *) readLineFromInputFile ()) != NULL)
	{
		const char *cp = line, *candidate;
		char const *longstring;
		char const *keyword, *variable;
		int indent;

		cp = skipSpace (cp);

		if (*cp == '\0')  /* skip blank line */
			continue;

		/* Skip comment if we are not inside a multi-line string. */
		if (*cp == '#' && !longStringLiteral)
			continue;

		/* Deal with line continuation. */
		if (!line_skip) vStringClear(continuation);
		vStringCatS(continuation, line);
		vStringStripTrailing(continuation);
		if (vStringLast(continuation) == '\\')
		{
			vStringChop(continuation);
			vStringCatS(continuation, " ");
			line_skip = 1;
			continue;
		}
		cp = line = vStringValue(continuation);
		cp = skipSpace (cp);
		indent = cp - line;
		line_skip = 0;

		/* Deal with multiline string ending. */
		if (longStringLiteral)
		{
			find_triple_end(cp, &longStringLiteral);
			continue;
		}

		checkIndent(nesting_levels, indent);

		/* Find global and class variables */
		variable = findVariable(line);
		if (variable)
		{
			const char *start = variable;
			char *arglist;
			bool parent_is_class;

			vStringClear (name);
			while (isIdentifierCharacter ((int) *start))
			{
				vStringPut (name, (int) *start);
				++start;
			}

			parent_is_class = constructParentString(nesting_levels, indent, parent);
			if (varIsLambda (variable, &arglist))
			{
				/* show class members or top-level script lambdas only */
				if (parent_is_class || vStringLength(parent) == 0)
					makeFunctionTag (name, parent, parent_is_class, arglist);
				eFree (arglist);
			}
			else
			{
				/* skip variables in methods */
				if (parent_is_class || vStringLength(parent) == 0)
					makeVariableTag (name, parent, parent_is_class);
			}
		}

		/* Deal with multiline string start. */
		longstring = find_triple_start(cp, &longStringLiteral);
		if (longstring)
		{
			longstring += 3;
			find_triple_end(longstring, &longStringLiteral);
			/* We don't parse for any tags in the rest of the line. */
			continue;
		}

		/* Deal with def and class keywords. */
		keyword = findDefinitionOrClass (cp);
		if (keyword)
		{
			bool found = false;
			bool is_class = false;
			if (matchKeyword ("def", keyword, &cp))
			{
				found = true;
			}
			else if (matchKeyword ("class", keyword, &cp))
			{
				found = true;
				is_class = true;
			}
			else if (matchKeyword ("cdef", keyword, &cp))
			{
				candidate = skipTypeDecl (cp, &is_class);
				if (candidate)
				{
					found = true;
					cp = candidate;
				}

			}
			else if (matchKeyword ("cpdef", keyword, &cp))
			{
				candidate = skipTypeDecl (cp, &is_class);
				if (candidate)
				{
					found = true;
					cp = candidate;
				}
			}

			if (found)
			{
				bool is_parent_class;
				struct corkInfo info;

				is_parent_class =
					constructParentString(nesting_levels, indent, parent);

				if (is_class)
				{
					info.index = parseClass (cp, name, parent, is_parent_class);
				}
				else
					info = parseFunction(cp, name, parent, is_parent_class);

				addNestingLevel(nesting_levels, indent, &info);
			}
			continue;
		}
		/* Find and parse imports */
		parseImports(line);
	}

	/* Force popping all nesting levels. */
	checkIndent(nesting_levels, 0);

	/* Clean up all memory we allocated. */
	vStringDelete (parent);
	vStringDelete (name);
	vStringDelete (continuation);
	nestingLevelsFree (nesting_levels);
}

extern parserDefinition *PythonParser (void)
{
    static const char *const extensions[] = { "py", "pyx", "pxd", "pxi" ,"scons", NULL };
	parserDefinition *def = parserNew ("Python");
	def->kindTable = PythonKinds;
	def->kindCount = ARRAY_SIZE (PythonKinds);
	def->extensions = extensions;
	def->parser = findPythonTags;
	def->useCork = true;
	return def;
}
