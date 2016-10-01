/*
*   $Id: vhdl.c,v 1.0 2005/11/05
*
*   Copyright (c) 2005, Klaus Dannecker
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for the Vhdl HDL
*   (Hardware Description Language).
*
*/

/*
 *   INCLUDE FILES
 */
#include "general.h"    /* must always come first */

#include <string.h>
#include <setjmp.h>

#include "debug.h"
#include "keyword.h"
#include "parse.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"

/*
 *   DATA DECLARATIONS
 */
typedef enum eException { ExceptionNone, ExceptionEOF } exception_t;

typedef enum {
	K_UNDEFINED = -1,
	K_CONSTANT,
	K_TYPE,
	K_VARIABLE,
	K_ATTRIBUTE,
	K_SIGNAL,
	K_FUNCTION,
	K_PROCEDURE,
	K_COMPONENT,
	K_PACKAGE,
	K_PROCESS,
	K_ENTITY,
	K_ARCHITECTURE,
	K_PORT,
	K_BLOCK,
	K_ALIAS
} vhdlKind;

/*
 *   DATA DEFINITIONS
 */
static int Ungetc;
static int Lang_vhdl;
static jmp_buf Exception;
static vString* Name=NULL;
static vString* Lastname=NULL;
static vString* Keyword=NULL;
static vString* TagName=NULL;

static kindOption VhdlKinds [] = {
	{ true, 'c', "variable",     "constants" },
	{ true, 't', "typedef",      "types" },
	{ true, 'v', "variable",     "variables" },
	{ true, 'a', "attribute",    "attributes" },
	{ true, 's', "variable",     "signals" },
	{ true, 'f', "function",     "functions" },
	{ true, 'p', "function",     "procedure" },
	{ true, 'k', "member",       "components" },
	{ true, 'l', "namespace",    "packages" },
	{ true, 'm', "member",       "process" },
	{ true, 'n', "class",        "entity" },
	{ true, 'o', "struct",       "architecture" },
	{ true, 'u', "port",         "ports" },
	{ true, 'b', "member",       "blocks" },
	{ true, 'A', "typedef",      "alias" }
};

static keywordTable VhdlKeywordTable [] = {
	{ "constant",     K_CONSTANT },
	{ "variable",     K_VARIABLE },
	{ "type",         K_TYPE },
	{ "subtype",      K_TYPE },
	{ "signal",       K_SIGNAL },
	{ "function",     K_FUNCTION },
	{ "procedure",    K_PROCEDURE },
	{ "component",    K_COMPONENT },
	{ "package",      K_PACKAGE },
	{ "process",      K_PROCESS },
	{ "entity",       K_ENTITY },
	{ "architecture", K_ARCHITECTURE },
	{ "inout",        K_PORT },
	{ "in",           K_PORT },
	{ "out",          K_PORT },
	{ "block",        K_BLOCK },
	{ "alias",        K_ALIAS }
};


/*
 *   FUNCTION DEFINITIONS
 */

static void initialize (const langType language)
{
	Lang_vhdl = language;
}

static void vUngetc (int c)
{
	Assert (Ungetc == '\0');
	Ungetc = c;
}

static int vGetc (void)
{
	int c;
	if (Ungetc == '\0')
		c = getcFromInputFile ();
	else
	{
		c = Ungetc;
		Ungetc = '\0';
	}
	if (c == '-')
	{
		int c2 = getcFromInputFile ();
		if (c2 == EOF)
			longjmp (Exception, (int) ExceptionEOF);
		else if (c2 == '-')   /* strip comment until end-of-line */
		{
			do
				c = getcFromInputFile ();
			while (c != '\n'  &&  c != EOF);
		}
		else
			Ungetc = c2;
	}
	if (c == EOF)
		longjmp (Exception, (int) ExceptionEOF);
	return c;
}

static bool isIdentifierCharacter (const int c)
{
	return (bool)(isalnum (c)  ||  c == '_'  ||  c == '`');
}

static int skipWhite (int c)
{
	while (c==' ')
		c = vGetc ();
	return c;
}

static bool readIdentifier (vString *const name, int c)
{
	vStringClear (name);
	if (isIdentifierCharacter (c))
	{
		while (isIdentifierCharacter (c))
		{
			vStringPut (name, c);
			c = vGetc ();
		}
		vUngetc (c);
	}
	return (bool)(name->length > 0);
}

static void tagNameList (const vhdlKind kind, int c)
{
	Assert (isIdentifierCharacter (c));
	if (isIdentifierCharacter (c))
	{
		readIdentifier (TagName, c);
		makeSimpleTag (TagName, VhdlKinds, kind);
	}
}

static void findTag (vString *const name)
{
	int c = '\0';
	vhdlKind kind;
	vStringCopyToLower (Keyword, name);
	kind = (vhdlKind)lookupKeyword (vStringValue (Keyword), Lang_vhdl);
	if (kind == K_UNDEFINED)
	{
		c = skipWhite (vGetc ());
		vStringCopyS(Lastname,vStringValue(name));
		if (c == ':')
		{
			c = skipWhite (vGetc ());
			if (isIdentifierCharacter (c))
			{
				readIdentifier (name, c);
				vStringCopyToLower (Keyword, name);
				lookupKeyword (vStringValue (Keyword), Lang_vhdl);
				kind = (vhdlKind)lookupKeyword (vStringValue (Keyword), Lang_vhdl);
				if (kind == K_PROCESS || kind == K_BLOCK || kind == K_PORT)
				{
					makeSimpleTag (Lastname, VhdlKinds, kind);
				}
			}
		} else {
			vUngetc (c);
		}
	}
	else
	{
		if (kind == K_SIGNAL) {
			while (c!=':') {
				c = skipWhite (vGetc ());
				if (c==',')
					c = vGetc ();
				if (isIdentifierCharacter (c))
					tagNameList (kind, c);
				else
					break;
				c = vGetc ();
			}
		}
		else if (kind == K_PROCESS || kind == K_BLOCK) {
			vStringCopyS(TagName,"unnamed");
			makeSimpleTag (TagName, VhdlKinds, kind);
		} else {
			c = skipWhite (vGetc ());
			if (c=='\"')
				c = vGetc ();
			if (isIdentifierCharacter (c))
				tagNameList (kind, c);
		}
	}
}

static void findVhdlTags (void)
{
	volatile bool newStatement = true;
	volatile int c = '\0';
	exception_t exception = (exception_t) setjmp (Exception);
	Name = vStringNew ();
	Lastname = vStringNew ();
	Keyword = vStringNew ();
	TagName = vStringNew ();

	if (exception == ExceptionNone) while (c != EOF)
	{
		c = vGetc ();
		switch (c)
		{
			case ';':
			case '\n':
				newStatement = true;
				break;

			case ' ':
			case '\t':
				break;

			default:
				if (newStatement && readIdentifier (Name, c)) {
					findTag (Name);
				}
				newStatement = false;
				break;
		}
	}
	vStringDelete (Name);
	vStringDelete (Lastname);
	vStringDelete (Keyword);
	vStringDelete (TagName);
}

extern parserDefinition* VhdlParser (void)
{
	static const char *const extensions [] = { "vhdl", "vhd", NULL };
	parserDefinition* def = parserNew ("Vhdl");
	def->kinds      = VhdlKinds;
	def->kindCount  = ARRAY_SIZE (VhdlKinds);
	def->extensions = extensions;
	def->parser     = findVhdlTags;
	def->initialize = initialize;
	def->keywordTable = VhdlKeywordTable;
	def->keywordCount = ARRAY_SIZE (VhdlKeywordTable);
	return def;
}
