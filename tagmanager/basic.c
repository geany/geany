/*
 *   $Id:$
 *
 *   Copyright (c) 2000-2006, Darren Hiebert, Elias Pschernig
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License.
 *
 *   This module contains functions for generating tags for BlitzBasic
 *   (BlitzMax), PureBasic and FreeBasic language files. For now, this is kept
 *   quite simple - but feel free to ask for more things added any time -
 *   patches are of course most welcome.
 */

/*
 *   INCLUDE FILES
 */
#include "general.h" /* must always come first */

#include <string.h>

#include "parse.h"
#include "read.h"
#include "vstring.h"

/*
 *   DATA DEFINITIONS
 */
typedef enum {
	K_CONST,
	K_FUNCTION,
	K_LABEL,
	K_TYPE,
	K_VARIABLE,
	K_ENUM
} BasicKind;

typedef struct {
	char const *token;
	BasicKind kind;
} KeyWord;

static kindOption BasicKinds[] = {
	{TRUE, 'c', "macro", "constants"},
	{TRUE, 'f', "function", "functions"},
	{TRUE, 'l', "namespace", "labels"},
	{TRUE, 't', "struct", "types"},
	{TRUE, 'v', "variable", "variables"},
	{TRUE, 'g', "externvar", "enumerations"}
};

static KeyWord freebasic_keywords[] = {
	{"dim", K_VARIABLE}, // must always be the first
	{"common", K_VARIABLE}, // must always be the second
	{"const", K_CONST}, // must always be the third
	{"function", K_FUNCTION},
	{"sub", K_FUNCTION},
	{"private sub", K_FUNCTION},
	{"public sub", K_FUNCTION},
	{"private function", K_FUNCTION},
	{"public function", K_FUNCTION},
	{"type", K_TYPE},
	{"enum", K_ENUM},
	{NULL, 0}
};

/*
 *   FUNCTION DEFINITIONS
 */

/* Match the name of a dim or const starting at pos. */
static int extract_dim (char const *pos, vString * name, BasicKind kind)
{
	while (isspace (*pos))
		pos++;

	vStringClear (name);

	if (strncasecmp (pos, "shared", 6) == 0)
		pos += 6; // skip keyword "shared"

	while (isspace (*pos))
		pos++;

	// capture "dim as String str"
	if (strncasecmp (pos, "as", 2) == 0)
	{
			pos += 2; // skip keyword "as"

		while (isspace (*pos))
			pos++;
		while (!isspace (*pos)) // skip next part which is a type
			pos++;
		while (isspace (*pos))
			pos++;
		// now we are at the name
	}

	// capture "dim as foo ptr bar"
	if (strncasecmp (pos, "ptr", 3) == 0)
	{
		pos += 3; // skip keyword "ptr"

		while (isspace (*pos))
			pos++;
	}

	for (; *pos && !isspace (*pos) && *pos != '(' && *pos != ','; pos++)
		vStringPut (name, *pos);
	vStringTerminate (name);
	makeSimpleTag (name, BasicKinds, kind);

	// if the line contains a ',', we have multiple declarations
	while (*pos && strchr (pos, ','))
	{
		while (*pos != ',') // skip all we don't need(e.g. "..., new_array(5), " we skip "(5)")
			pos++;
		while (isspace (*pos) || *pos == ',')
			pos++;

		vStringClear (name);
		for (; *pos && !isspace (*pos) && *pos != '(' && *pos != ','; pos++)
			vStringPut (name, *pos);
		vStringTerminate (name);
		makeSimpleTag (name, BasicKinds, kind);
	}

	vStringDelete (name);
	return 1;
}

/* Match the name of a tag (function, variable, type, ...) starting at pos. */
static char const *extract_name (char const *pos, vString * name)
{
	while (isspace (*pos))
		pos++;
	vStringClear (name);
	for (; *pos && !isspace (*pos) && *pos != '(' && *pos != ',' && *pos != '='; pos++)
		vStringPut (name, *pos);
	vStringTerminate (name);
	return pos;
}

/* Match a keyword starting at p (case insensitive). */
static int match_keyword (const char *p, KeyWord const *kw)
{
	vString *name;
	size_t i;
	int j;
	for (i = 0; i < strlen (kw->token); i++)
	{
		if (tolower (p[i]) != kw->token[i])
			return 0;
	}
	name = vStringNew ();
	p += i;
	if (kw == &freebasic_keywords[0] ||
		kw == &freebasic_keywords[1] ||
		kw == &freebasic_keywords[2])
		return extract_dim (p, name, kw->kind); // extract_dim adds the found tag(s)

	for (j = 0; j < 1; j++)
	{
		p = extract_name (p, name);
	}
	makeSimpleTag (name, BasicKinds, kw->kind);
	vStringDelete (name);

	return 1;
}

/* Match a "label:" style label. */
static void match_colon_label (char const *p)
{
	char const *end = p + strlen (p) - 1;
	while (isspace (*end))
		end--;
	if (*end == ':')
	{
		vString *name = vStringNew ();
		vStringNCatS (name, p, end - p);
		makeSimpleTag (name, BasicKinds, K_LABEL);
		vStringDelete (name);
	}
}

static void findBasicTags (void)
{
	const char *line;
	KeyWord *keywords;

	keywords = freebasic_keywords;

	while ((line = (const char *) fileReadLine ()) != NULL)
	{
		const char *p = line;
		KeyWord const *kw;

		while (isspace (*p))
			p++;

		/* Empty line? */
		if (!*p)
			continue;

		/* In Basic, keywords always are at the start of the line. */
		for (kw = keywords; kw->token; kw++)
			if (match_keyword (p, kw)) break;

		/* Is it a label? */
		match_colon_label (p);
	}
}

parserDefinition *FreeBasicParser (void)
{
	static char const *extensions[] = { "bas", "bi", "bb", "pb", NULL };
	parserDefinition *def = parserNew ("FreeBasic");
	def->kinds = BasicKinds;
	def->kindCount = KIND_COUNT (BasicKinds);
	def->extensions = extensions;
	def->parser = findBasicTags;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
