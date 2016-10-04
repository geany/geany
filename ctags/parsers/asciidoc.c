/*
*
* 	Copyright (c) 2012, Lex Trotman
*   Based on Rest code by Nick Treleaven, see rest.c
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for asciidoc files.
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
#include "nestlevel.h"
#include "routines.h"

/*
*   DATA DEFINITIONS
*/
typedef enum {
	K_CHAPTER = 0,
	K_SECTION,
	K_SUBSECTION,
	K_SUBSUBSECTION,
	K_LEVEL5SECTION,
	SECTION_COUNT
} asciidocKind;

static kindOption AsciidocKinds[] = {
	{ true, 'n', "namespace",     "chapters"},
	{ true, 'm', "member",        "sections" },
	{ true, 'd', "macro",         "level2sections" },
	{ true, 'v', "variable",      "level3sections" },
	{ true, 's', "struct",        "level4sections" }
};

static char kindchars[SECTION_COUNT]={ '=', '-', '~', '^', '+' };

static NestingLevels *nestingLevels = NULL;

/*
*   FUNCTION DEFINITIONS
*/

static NestingLevel *getNestingLevel(const int kind)
{
	NestingLevel *nl;

	while (1)
	{
		nl = nestingLevelsGetCurrent(nestingLevels);
		if (nl && nl->type >= kind)
			nestingLevelsPop(nestingLevels);
		else
			break;
	}
	return nl;
}

static void makeAsciidocTag (const vString* const name, const int kind)
{
	const NestingLevel *const nl = getNestingLevel(kind);

	if (vStringLength (name) > 0)
	{
		tagEntryInfo e;
		initTagEntry (&e, vStringValue (name), &(AsciidocKinds [kind]));

		e.lineNumber--;	/* we want the line before the '---' underline chars */

		if (nl && nl->type < kind)
		{
			e.extensionFields.scopeKind = &(AsciidocKinds [nl->type]);
			e.extensionFields.scopeName = vStringValue (nl->name);
		}
		makeTagEntry (&e);
	}
	nestingLevelsPush(nestingLevels, name, kind);
}


static int get_kind(char c)
{
	int i;

	for (i = 0; i < SECTION_COUNT; i++)
	{
		if (kindchars[i] == c)
			return i;
	}
	return -1;
}


/* computes the length of an UTF-8 string
 * if the string doesn't look like UTF-8, return -1
 * FIXME consider East_Asian_Width Unicode property */
static int utf8_strlen(const char *buf, int buf_len)
{
	int len = 0;
	const char *end = buf + buf_len;

	for (len = 0; buf < end; len ++)
	{
		/* perform quick and naive validation (no sub-byte checking) */
		if (! (*buf & 0x80))
			buf ++;
		else if ((*buf & 0xe0) == 0xc0)
			buf += 2;
		else if ((*buf & 0xf0) == 0xe0)
			buf += 3;
		else if ((*buf & 0xf8) == 0xf0)
			buf += 4;
		else /* not a valid leading UTF-8 byte, abort */
			return -1;

		if (buf > end) /* incomplete last byte */
			return -1;
	}

	return len;
}


static void findAsciidocTags(void)
{
	vString *name = vStringNew();
	const unsigned char *line;
	unsigned char in_block = '\0';  /* holds the block marking char or \0 if not in block */

	nestingLevels = nestingLevelsNew();

	while ((line = readLineFromInputFile()) != NULL)
	{
		int line_len = strlen((const char*) line);
		int name_len_bytes = vStringLength(name);
		int name_len = utf8_strlen(vStringValue(name), name_len_bytes);

		/* if the name doesn't look like UTF-8, assume one-byte charset */
		if (name_len < 0) name_len = name_len_bytes;
		
		/* if its a title underline, or a delimited block marking character */
		if (line[0] == '=' || line[0] == '-' || line[0] == '~' ||
			line[0] == '^' || line[0] == '+' || line[0] == '.' ||
			line[0] == '*' || line[0] == '_' || line[0] == '/')
		{
			int n_same;
			for (n_same = 1; line[n_same] == line[0]; ++n_same);
			
			/* is it a two line title or a delimited block */
			if (n_same == line_len)
			{
				/* if in a block, can't be block start or title, look for block end */
				if (in_block)
				{
					if (line[0] == in_block) in_block = '\0';
				}
				
				/* if its a =_~^+ and the same length +-2 as the line before then its a title */
				/* (except in the special case its a -- open block start line) */
				else if ((line[0] == '=' || line[0] == '-' || line[0] == '~' ||
							line[0] == '^' || line[0] == '+') &&
						line_len <= name_len + 2 && line_len >= name_len - 2 &&
						!(line_len == 2 && line[0] == '-'))
				{
					int kind = get_kind((char)(line[0]));
					if (kind >= 0)
					{
						makeAsciidocTag(name, kind);
						continue;
					}
				}
				
				/* else if its 4 or more /+-.*_= (plus the -- special case) its a block start */
				else if (((line[0] == '/' || line[0] == '+' || line[0] == '-' ||
						   line[0] == '.' || line[0] == '*' || line[0] == '_' ||
						   line[0] == '=') && line_len >= 4 )
						 || (line[0] == '-' && line_len == 2))
				{
					in_block = line[0];
				}
			}
			
			/* otherwise is it a one line title */
			else if (line[0] == '=' && n_same <= 5 && isspace(line[n_same]) &&
					!in_block)
			{
				int kind = n_same - 1;
				int start = n_same;
				int end = line_len - 1;
				while (line[end] == line[0])--end;
				while (isspace(line[start]))++start;
				while (isspace(line[end]))--end;
				vStringClear(name);
				vStringNCatS(name, (const char*)(&(line[start])), end - start + 1);
				makeAsciidocTag(name, kind);
				continue;
			}
		}
		vStringClear(name);
		if (! isspace(*line))
			vStringCatS(name, (const char*) line);
	}
	vStringDelete(name);
	nestingLevelsFree(nestingLevels);
}

extern parserDefinition* AsciidocParser (void)
{
	static const char *const patterns [] = { "*.asciidoc", NULL };
	static const char *const extensions [] = { "asciidoc", NULL };
	parserDefinition* const def = parserNew ("Asciidoc");

	def->kinds = AsciidocKinds;
	def->kindCount = ARRAY_SIZE (AsciidocKinds);
	def->patterns = patterns;
	def->extensions = extensions;
	def->parser = findAsciidocTags;
	return def;
}
