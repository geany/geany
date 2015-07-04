/*
*   Copyright (c) 2015, Enrico Tröger <enrico.troeger@uvena.de>
*
*   Loosely based on the PHP tags parser since the syntax is somewhat similar
*   regarding variable and function definitions.
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains code for generating tags for Windows PowerShell scripts.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "main.h"
#include "parse.h"
#include "read.h"
#include "vstring.h"
#include "keyword.h"
#include "entry.h"
#include <string.h>

#define SCOPE_SEPARATOR "::"


#define ARRAY_LENGTH(array) (sizeof array / sizeof array[0])

#define ACCESS_UNDEFINED NULL
static const char *const accessTypes[] = {
	ACCESS_UNDEFINED,
	"global",
	"local",
	"script",
	"private"
};

typedef enum {
	K_FUNCTION,
	K_VARIABLE,
	COUNT_KIND
} powerShellKind;

static kindOption PowerShellKinds[COUNT_KIND] = {
	{ TRUE, 'f', "function",	"functions" },
	{ TRUE, 'v', "variable",	"variables" }
};


typedef enum eTokenType {
	TOKEN_UNDEFINED,
	TOKEN_EOF,
	TOKEN_CLOSE_PAREN,
	TOKEN_SEMICOLON,
	TOKEN_COLON,
	TOKEN_COMMA,
	TOKEN_KEYWORD,
	TOKEN_OPEN_PAREN,
	TOKEN_OPERATOR,
	TOKEN_IDENTIFIER,
	TOKEN_STRING,
	TOKEN_PERIOD,
	TOKEN_OPEN_CURLY,
	TOKEN_CLOSE_CURLY,
	TOKEN_EQUAL_SIGN,
	TOKEN_OPEN_SQUARE,
	TOKEN_CLOSE_SQUARE,
	TOKEN_VARIABLE
} tokenType;

typedef struct {
	tokenType		type;
	vString *		string;
	vString *		scope;
	unsigned long	lineNumber;
	MIOPos			filePosition;
	int 			parentKind; /* -1 if none */
} tokenInfo;


static const char *findValidAccessType (const char *const access)
{
	unsigned int i;
	if (access == ACCESS_UNDEFINED)
		return ACCESS_UNDEFINED; /* early out to save the for-loop if possible */
	for (i = 0; i < ARRAY_LENGTH(accessTypes); i++)
	{
		if (accessTypes[i] == ACCESS_UNDEFINED)
			continue;
		if (strcasecmp (access, accessTypes[i]) == 0)
			return accessTypes[i];
		i++;
	}
	return ACCESS_UNDEFINED;
}

static void initPowerShellEntry (tagEntryInfo *const e, const tokenInfo *const token,
								 const powerShellKind kind, const char *const access)
{
	initTagEntry (e, vStringValue (token->string));

	e->lineNumber	= token->lineNumber;
	e->filePosition	= token->filePosition;
	e->kindName		= PowerShellKinds[kind].name;
	e->kind			= (char) PowerShellKinds[kind].letter;

	if (access != NULL)
		e->extensionFields.access = access;
	if (vStringLength (token->scope) > 0)
	{
		int parentKind = token->parentKind;
		Assert (parentKind >= 0);

		e->extensionFields.scope[0] = PowerShellKinds[parentKind].name;
		e->extensionFields.scope[1] = vStringValue (token->scope);
	}
}

static void makeSimplePowerShellTag (const tokenInfo *const token, const powerShellKind kind,
									 const char *const access)
{
	if (PowerShellKinds[kind].enabled)
	{
		tagEntryInfo e;

		initPowerShellEntry (&e, token, kind, access);
		makeTagEntry (&e);
	}
}

static void makeFunctionTag (const tokenInfo *const token, const vString *const arglist,
							 const char *const access)
{
	if (PowerShellKinds[K_FUNCTION].enabled)
	{
		tagEntryInfo e;

		initPowerShellEntry (&e, token, K_FUNCTION, access);

		if (arglist)
			e.extensionFields.arglist = vStringValue (arglist);

		makeTagEntry (&e);
	}
}

static tokenInfo *newToken (void)
{
	tokenInfo *const token = xMalloc (1, tokenInfo);

	token->type			= TOKEN_UNDEFINED;
	token->string		= vStringNew ();
	token->scope		= vStringNew ();
	token->lineNumber   = getSourceLineNumber ();
	token->filePosition = getInputFilePosition ();
	token->parentKind	= -1;

	return token;
}

static void deleteToken (tokenInfo *const token)
{
	vStringDelete (token->string);
	vStringDelete (token->scope);
	eFree (token);
}

static void copyToken (tokenInfo *const dest, const tokenInfo *const src,
					   boolean scope)
{
	dest->lineNumber = src->lineNumber;
	dest->filePosition = src->filePosition;
	dest->type = src->type;
	vStringCopy (dest->string, src->string);
	dest->parentKind = src->parentKind;
	if (scope)
		vStringCopy (dest->scope, src->scope);
}

static void addToScope (tokenInfo *const token, const vString *const extra)
{
	if (vStringLength (token->scope) > 0)
		vStringCatS (token->scope, SCOPE_SEPARATOR);
	vStringCatS (token->scope, vStringValue (extra));
	vStringTerminate (token->scope);
}

static boolean isIdentChar (const int c)
{
	return (isalnum (c) || c == ':' || c == '_' || c == '-' || c >= 0x80);
}

static int skipToCharacter (const int c)
{
	int d;
	do
	{
		d = fileGetc ();
	} while (d != EOF  &&  d != c);
	return d;
}

static void parseString (vString *const string, const int delimiter)
{
	while (TRUE)
	{
		int c = fileGetc ();

		if (c == '\\' && (c = fileGetc ()) != EOF)
			vStringPut (string, (char) c);
		else if (c == EOF || c == delimiter)
			break;
		else
			vStringPut (string, (char) c);
	}
	vStringTerminate (string);
}

static void parseIdentifier (vString *const string, const int firstChar)
{
	int c = firstChar;
	do
	{
		vStringPut (string, (char) c);
		c = fileGetc ();
	} while (isIdentChar (c));
	fileUngetc (c);
	vStringTerminate (string);
}

static boolean isTokenFunction (vString *const name)
{
	return (strcasecmp (vStringValue (name), "function") == 0 ||
			strcasecmp (vStringValue (name), "filter") == 0);
}

static boolean isSpace (int c)
{
	return (c == '\t' || c == ' ' || c == '\v' ||
			c == '\n' || c == '\r' || c == '\f');
}

static int skipWhitespaces (int c)
{
	while (isSpace (c))
		c = fileGetc ();
	return c;
}

static int skipSingleComment (void)
{
	int c;
	do
	{
		c = fileGetc ();
		if (c == '\r')
		{
			int next = fileGetc ();
			if (next != '\n')
				fileUngetc (next);
			else
				c = next;
		}
	} while (c != EOF && c != '\n' && c != '\r');
	return c;
}

static void readToken (tokenInfo *const token)
{
	int c;

	token->type		= TOKEN_UNDEFINED;
	vStringClear (token->string);

getNextChar:

	c = fileGetc ();
	c = skipWhitespaces (c);

	token->lineNumber   = getSourceLineNumber ();
	token->filePosition = getInputFilePosition ();

	switch (c)
	{
		case EOF: token->type = TOKEN_EOF;					break;
		case '(': token->type = TOKEN_OPEN_PAREN;			break;
		case ')': token->type = TOKEN_CLOSE_PAREN;			break;
		case ';': token->type = TOKEN_SEMICOLON;			break;
		case ',': token->type = TOKEN_COMMA;				break;
		case '.': token->type = TOKEN_PERIOD;				break;
		case ':': token->type = TOKEN_COLON;				break;
		case '{': token->type = TOKEN_OPEN_CURLY;			break;
		case '}': token->type = TOKEN_CLOSE_CURLY;			break;
		case '[': token->type = TOKEN_OPEN_SQUARE;			break;
		case ']': token->type = TOKEN_CLOSE_SQUARE;			break;
		case '=': token->type = TOKEN_EQUAL_SIGN;			break;

		case '\'':
		case '"':
			token->type = TOKEN_STRING;
			parseString (token->string, c);
			token->lineNumber = getSourceLineNumber ();
			token->filePosition = getInputFilePosition ();
			break;

		case '<':
		{
			int d = fileGetc ();
			if (d == '#')
			{
				/* <# ... #> multiline comment */
				do
				{
					c = skipToCharacter ('#');
					if (c != EOF)
					{
						c = fileGetc ();
						if (c == '>')
							break;
						else
							fileUngetc (c);
					}
				} while (c != EOF);
				goto getNextChar;
			}
			else
			{
				fileUngetc (d);
				token->type = TOKEN_UNDEFINED;
			}
			break;
		}

		case '#': /* comment */
			skipSingleComment ();
			goto getNextChar;
			break;

		case '+':
		case '-':
		case '*':
		case '/':
		case '%':
		{
			int d = fileGetc ();
			if (d != '=')
				fileUngetc (d);
			token->type = TOKEN_OPERATOR;
			break;
		}

		case '$': /* variable start */
		{
			int d = fileGetc ();
			if (! isIdentChar (d))
			{
				fileUngetc (d);
				token->type = TOKEN_UNDEFINED;
			}
			else
			{
				parseIdentifier (token->string, d);
				token->type = TOKEN_VARIABLE;
			}
			break;
		}

		default:
			if (! isIdentChar (c))
				token->type = TOKEN_UNDEFINED;
			else
			{
				parseIdentifier (token->string, c);
				if (isTokenFunction (token->string))
					token->type = TOKEN_KEYWORD;
				else
					token->type = TOKEN_IDENTIFIER;
			}
			break;
	}
}

static void enterScope (tokenInfo *const parentToken,
						const vString *const extraScope,
						const int parentKind);

/* strip a possible PowerShell scope specification and convert it to accessType */
static const char *parsePowerShellScope (tokenInfo *const token)
{
	const char *access = ACCESS_UNDEFINED;
	const char *const tokenName = vStringValue (token->string);
	const char *powershellScopeEnd;

	powershellScopeEnd = strchr (tokenName, ':');
	if (powershellScopeEnd)
	{
		size_t powershellScopeLen;
		vString * powershellScope = vStringNew ();

		powershellScopeLen = (size_t)(powershellScopeEnd - tokenName);
		/* extract the scope */
		vStringNCopyS (powershellScope, tokenName, powershellScopeLen);
		vStringTerminate (powershellScope);
		/* cut the resulting scope string from the identifier */
		memmove (token->string->buffer,
				 /* +1 to skip the leading colon */
				 token->string->buffer + powershellScopeLen + 1,
				 /* +1 for the skipped leading colon and - 1 to include the trailing \0 byte */
				 token->string->length + 1 - powershellScopeLen - 1);
		token->string->length -= powershellScopeLen + 1;

		access = findValidAccessType (vStringValue (powershellScope));

		vStringDelete (powershellScope);
	}
	return access;
}


/* parse a function
 *
 * 	function myfunc($foo, $bar) {}
 */
static boolean parseFunction (tokenInfo *const token)
{
	boolean readNext = TRUE;
	tokenInfo *nameFree = NULL;
	const char *access;

	readToken (token);

	if (token->type != TOKEN_IDENTIFIER)
		return FALSE;

	access = parsePowerShellScope (token);

	nameFree = newToken ();
	copyToken (nameFree, token, TRUE);
	readToken (token);

	if (token->type == TOKEN_OPEN_PAREN)
	{
		vString *arglist = vStringNew ();
		int depth = 1;

		vStringPut (arglist, '(');
		do
		{
			readToken (token);

			switch (token->type)
			{
				case TOKEN_OPEN_PAREN:  depth++; break;
				case TOKEN_CLOSE_PAREN: depth--; break;
				default: break;
			}
			/* display part */
			switch (token->type)
			{
				case TOKEN_CLOSE_CURLY:		vStringPut (arglist, '}');		break;
				case TOKEN_CLOSE_PAREN:		vStringPut (arglist, ')');		break;
				case TOKEN_CLOSE_SQUARE:	vStringPut (arglist, ']');		break;
				case TOKEN_COLON:			vStringPut (arglist, ':');		break;
				case TOKEN_COMMA:			vStringCatS (arglist, ", ");	break;
				case TOKEN_EQUAL_SIGN:		vStringCatS (arglist, " = ");	break;
				case TOKEN_OPEN_CURLY:		vStringPut (arglist, '{');		break;
				case TOKEN_OPEN_PAREN:		vStringPut (arglist, '(');		break;
				case TOKEN_OPEN_SQUARE:		vStringPut (arglist, '[');		break;
				case TOKEN_PERIOD:			vStringPut (arglist, '.');		break;
				case TOKEN_SEMICOLON:		vStringPut (arglist, ';');		break;
				case TOKEN_STRING:			vStringCatS (arglist, "'...'");	break;

				case TOKEN_IDENTIFIER:
				case TOKEN_KEYWORD:
				case TOKEN_VARIABLE:
				{
					switch (vStringLast (arglist))
					{
						case 0:
						case ' ':
						case '{':
						case '(':
						case '[':
						case '.':
							/* no need for a space between those and the identifier */
							break;

						default:
							vStringPut (arglist, ' ');
							break;
					}
					if (token->type == TOKEN_VARIABLE)
						vStringPut (arglist, '$');
					vStringCat (arglist, token->string);
					break;
				}

				default: break;
			}
		}
		while (token->type != TOKEN_EOF && depth > 0);

		vStringTerminate (arglist);

		makeFunctionTag (nameFree, arglist, access);
		vStringDelete (arglist);

		readToken (token);
	}
	else if (token->type == TOKEN_OPEN_CURLY)
	{	/* filters doesn't need to have an arglist */
		makeFunctionTag (nameFree, NULL, access);
	}

	if (token->type == TOKEN_OPEN_CURLY)
		enterScope (token, nameFree->string, K_FUNCTION);
	else
		readNext = FALSE;

	if (nameFree)
		deleteToken (nameFree);

	return readNext;
}

/* parses declarations of the form
 * 	$var = VALUE
 */
static boolean parseVariable (tokenInfo *const token)
{
	tokenInfo *name;
	boolean readNext = TRUE;
	const char *access;

	name = newToken ();
	copyToken (name, token, TRUE);

	readToken (token);
	if (token->type == TOKEN_EQUAL_SIGN)
	{
		if (token->parentKind != K_FUNCTION)
		{	/* ignore local variables (i.e. within a function) */
			access = parsePowerShellScope (name);
			makeSimplePowerShellTag (name, K_VARIABLE, access);
			readNext = TRUE;
		}
	}
	else
		readNext = FALSE;

	deleteToken (name);

	return readNext;
}

static void enterScope (tokenInfo *const parentToken,
						const vString *const extraScope,
						const int parentKind)
{
	tokenInfo *token = newToken ();
	int origParentKind = parentToken->parentKind;

	copyToken (token, parentToken, TRUE);

	if (extraScope)
	{
		addToScope (token, extraScope);
		token->parentKind = parentKind;
	}

	readToken (token);
	while (token->type != TOKEN_EOF &&
		   token->type != TOKEN_CLOSE_CURLY)
	{
		boolean readNext = TRUE;

		switch (token->type)
		{
			case TOKEN_OPEN_CURLY:
				enterScope (token, NULL, -1);
				break;

			case TOKEN_KEYWORD:
				readNext = parseFunction (token);
				break;

			case TOKEN_VARIABLE:
				readNext = parseVariable (token);
				break;

			default: break;
		}

		if (readNext)
			readToken (token);
	}

	copyToken (parentToken, token, FALSE);
	parentToken->parentKind = origParentKind;
	deleteToken (token);
}

static void findPowerShellTags (void)
{
	tokenInfo *const token = newToken ();

	do
	{
		enterScope (token, NULL, -1);
	}
	while (token->type != TOKEN_EOF); /* keep going even with unmatched braces */

	deleteToken (token);
}

extern parserDefinition* PowerShellParser (void)
{
	static const char *const extensions [] = { "ps1", "psm1", NULL };
	parserDefinition* def = parserNew ("PowerShell");
	def->kinds      = PowerShellKinds;
	def->kindCount  = KIND_COUNT (PowerShellKinds);
	def->extensions = extensions;
	def->parser     = findPowerShellTags;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
