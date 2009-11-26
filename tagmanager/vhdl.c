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
#include "general.h"	/* must always come first */

#include <string.h>
#include <setjmp.h>

#include "keyword.h"
#include "parse.h"
#include "read.h"
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
    K_ATRIBUTE,
    K_SIGNAL,
    K_FUNCTION,
    K_PROCEDURE,
    K_COMPONENT,
    K_PACKAGE,
    K_PROCESS,
    K_ENTITY,
    K_ARCHITECTURE,
    K_PORT,
    K_ALIAS
} vhdlKind;

typedef struct {
    const char *keyword;
    vhdlKind kind;
} keywordAssoc;

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
 { TRUE, 'c', "variable",     "constants" },
 { TRUE, 't', "typedef",      "types" },
 { TRUE, 'v', "variable",     "variables" },
 { TRUE, 'a', "atribute",     "atributes" },
 { TRUE, 's', "variable",     "signals" },
 { TRUE, 'f', "function",     "functions" },
 { TRUE, 'p', "function",     "procedure" },
 { TRUE, 'k', "member",       "components" },
 { TRUE, 'l', "namespace",    "packages" },
 { TRUE, 'm', "member",       "process" },
 { TRUE, 'n', "class",        "entity" },
 { TRUE, 'o', "struct",       "architecture" },
 { TRUE, 'u', "port",         "ports" },
 { TRUE, 'v', "typedef",      "alias" }
 };

static keywordAssoc VhdlKeywordTable [] = {
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
    { "alias",        K_ALIAS }
};


/*
 *   FUNCTION DEFINITIONS
 */

static void initialize (const langType language)
{
    size_t i;
    const size_t count = sizeof (VhdlKeywordTable) /
			 sizeof (VhdlKeywordTable [0]);
    Lang_vhdl = language;
    for (i = 0  ;  i < count  ;  ++i)
    {
		const keywordAssoc* const p = &VhdlKeywordTable [i];
		addKeyword (p->keyword, language, (int) p->kind);
    }
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
	c = fileGetc ();
    else
    {
		c = Ungetc;
		Ungetc = '\0';
    }
    if (c == '-')
    {
		int c2 = fileGetc ();
		if (c2 == EOF)
			longjmp (Exception, (int) ExceptionEOF);
		else if (c2 == '-')   /* strip comment until end-of-line */
		{
			do
			c = fileGetc ();
			while (c != '\n'  &&  c != EOF);
		}
		else
			Ungetc = c2;
	}
    if (c == EOF)
		longjmp (Exception, (int) ExceptionEOF);
	return c;
}

static boolean isIdentifierCharacter (const int c)
{
    return (boolean)(isalnum (c)  ||  c == '_'  ||  c == '`');
}

static int skipWhite (int c)
{
    while (c==' ')
	c = vGetc ();
    return c;
}

static boolean readIdentifier (vString *const name, int c)
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
		vStringTerminate (name);
    }
    return (boolean)(name->length > 0);
}

static void tagNameList (const vhdlKind kind, int c)
{
    Assert (isIdentifierCharacter (c));
	if (isIdentifierCharacter (c))
	{
		readIdentifier (TagName, c);
		makeSimpleTag (TagName, VhdlKinds, kind);
		vUngetc (c);
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
					if (kind == K_PROCESS || kind == K_PORT)
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
		else if (kind == K_PROCESS) {
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
    volatile boolean newStatement = TRUE;
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
			newStatement = TRUE;
			break;

			case ' ':
			case '\t':
			break;

			default:
			if (newStatement && readIdentifier (Name, c)) {
				findTag (Name);
				}
			newStatement = FALSE;
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
    def->kindCount  = KIND_COUNT (VhdlKinds);
    def->extensions = extensions;
    def->parser     = findVhdlTags;
    def->initialize = initialize;
    return def;
}

/* vi:set tabstop=8 shiftwidth=4: */
