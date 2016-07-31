/*
 * Copyright (c) 2014, Colomban Wendling <colomban@geany.org>
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License.
 */
/*
 * This module contains functions for generating tags for JSON files.
 *
 * http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-404.pdf
 *
 * This implementation is forgiving and allows many constructs that are not
 * actually valid but that don't conflict with the format.  This is intend to
 * better support partly broken or unfinished files.
 */

#include "general.h"

#include <string.h>
#include "main.h"
#include "entry.h"
#include "keyword.h"
#include "parse.h"
#include "read.h"
#include "vstring.h"

typedef enum {
	TOKEN_EOF,
	TOKEN_UNDEFINED,
	TOKEN_OPEN_SQUARE,
	TOKEN_CLOSE_SQUARE,
	TOKEN_OPEN_CURLY,
	TOKEN_CLOSE_CURLY,
	TOKEN_COLON,
	TOKEN_COMMA,
	TOKEN_TRUE,
	TOKEN_FALSE,
	TOKEN_NULL,
	TOKEN_NUMBER,
	TOKEN_STRING
} tokenType;

typedef enum {
	TAG_NONE = -1,
	TAG_OBJECT,
	TAG_ARRAY,
	TAG_NUMBER,
	TAG_STRING,
	TAG_BOOLEAN,
	TAG_NULL,
	TAG_COUNT
} jsonKind;

typedef struct {
	tokenType		type;
	jsonKind		scopeKind;
	vString			*string;
	vString			*scope;
	unsigned long	lineNumber;
	MIOPos			filePosition;
} tokenInfo;

typedef enum {
	KEYWORD_true,
	KEYWORD_false,
	KEYWORD_null
} keywordId;

static langType Lang_json;

static kindOption JsonKinds [] = {
	{ TRUE,  'o', "object",		"objects"	},
	{ TRUE,  'a', "array",		"arrays"	},
	{ TRUE,  'n', "number",		"numbers"	},
	{ TRUE,  's', "string",		"strings"	},
	{ TRUE,  'b', "boolean",	"booleans"	},
	{ TRUE,  'z', "null",		"nulls"		}
};

static tokenInfo *newToken (void)
{
	tokenInfo *const token = xMalloc (1, tokenInfo);

	token->type			= TOKEN_UNDEFINED;
	token->scopeKind	= TAG_NONE;
	token->string		= vStringNew ();
	token->scope		= vStringNew ();
	token->lineNumber	= getSourceLineNumber ();
	token->filePosition	= getInputFilePosition ();

	return token;
}

static void deleteToken (tokenInfo *const token)
{
	vStringDelete (token->string);
	vStringDelete (token->scope);
	eFree (token);
}

static void copyToken (tokenInfo *const dest, tokenInfo *const src)
{
	dest->type = src->type;
	dest->scopeKind = src->scopeKind;
	vStringCopy (dest->string, src->string);
	vStringCopy (dest->scope, src->scope);
	dest->lineNumber = src->lineNumber;
	dest->filePosition = src->filePosition;
}

static void makeJsonTag (tokenInfo *const token, const jsonKind kind)
{
	tagEntryInfo e;

	if (! JsonKinds[kind].enabled)
		return;

	initTagEntry (&e, vStringValue (token->string));

	e.lineNumber	= token->lineNumber;
	e.filePosition	= token->filePosition;
	e.kindName		= JsonKinds[kind].name;
	e.kind			= JsonKinds[kind].letter;

	if (vStringLength (token->scope) > 0)
	{
		Assert (token->scopeKind > TAG_NONE && token->scopeKind < TAG_COUNT);

		e.extensionFields.scope[0] = JsonKinds[token->scopeKind].name;
		e.extensionFields.scope[1] = vStringValue (token->scope);
	}

	makeTagEntry (&e);
}

static boolean isIdentChar (int c)
{
	return (isalnum (c) || c == '+' || c == '-' || c == '.');
}

static void readTokenFull (tokenInfo *const token,
						   boolean includeStringRepr)
{
	int c;

	token->type = TOKEN_UNDEFINED;
	vStringClear (token->string);

	do
		c = fileGetc ();
	while (c == '\t' || c == ' ' || c == '\r' || c == '\n');

	token->lineNumber   = getSourceLineNumber ();
	token->filePosition = getInputFilePosition ();

	switch (c)
	{
		case EOF: token->type = TOKEN_EOF;			break;
		case '[': token->type = TOKEN_OPEN_SQUARE;	break;
		case ']': token->type = TOKEN_CLOSE_SQUARE;	break;
		case '{': token->type = TOKEN_OPEN_CURLY;	break;
		case '}': token->type = TOKEN_CLOSE_CURLY;	break;
		case ':': token->type = TOKEN_COLON;		break;
		case ',': token->type = TOKEN_COMMA;		break;

		case '"':
		{
			boolean escaped = FALSE;
			token->type = TOKEN_STRING;
			while (TRUE)
			{
				c = fileGetc ();
				/* we don't handle unicode escapes but they are safe */
				if (escaped)
					escaped = FALSE;
				else if (c == '\\')
					escaped = TRUE;
				else if (c >= 0x00 && c <= 0x1F)
					break; /* break on invalid, unescaped, control characters */
				else if (c == '"' || c == EOF)
					break;
				if (includeStringRepr)
					vStringPut (token->string, c);
			}
			vStringTerminate (token->string);
			break;
		}

		default:
			if (! isIdentChar (c))
				token->type = TOKEN_UNDEFINED;
			else
			{
				do
				{
					vStringPut (token->string, c);
					c = fileGetc ();
				}
				while (c != EOF && isIdentChar (c));
				vStringTerminate (token->string);
				fileUngetc (c);
				switch (lookupKeyword (vStringValue (token->string), Lang_json))
				{
					case KEYWORD_true:	token->type = TOKEN_TRUE;	break;
					case KEYWORD_false:	token->type = TOKEN_FALSE;	break;
					case KEYWORD_null:	token->type = TOKEN_NULL;	break;
					default:			token->type = TOKEN_NUMBER;	break;
				}
			}
			break;
	}
}

#define readToken(t) (readTokenFull ((t), FALSE))

static void pushScope (tokenInfo *const token,
					   const tokenInfo *const parent,
					   const jsonKind parentKind)
{
	if (vStringLength (token->scope) > 0)
		vStringPut (token->scope, '.');
	vStringCat (token->scope, parent->string);
	vStringTerminate (token->scope);
	token->scopeKind = parentKind;
}

static void popScope (tokenInfo *const token,
					  const tokenInfo *const parent)
{
	vStringTruncate (token->scope, vStringLength (parent->scope));
	token->scopeKind = parent->scopeKind;
}

#define skipToOneOf2(token, type1, type2) \
	(skipToOneOf3 (token, type1, type2, TOKEN_EOF /* dummy */))

#define skipTo(token, type) \
	(skipToOneOf3 (token, type, /* dummies */ TOKEN_EOF, TOKEN_EOF))

static void skipToOneOf3 (tokenInfo *const token,
						  const tokenType type1,
						  const tokenType type2,
						  const tokenType type3)
{
	while (token->type != TOKEN_EOF &&
		   token->type != type1 &&
		   token->type != type2 &&
		   token->type != type3)
	{
		readToken (token);
		if (token->type == TOKEN_OPEN_CURLY)
		{
			skipTo (token, TOKEN_CLOSE_CURLY);
			readToken (token);
		}
		else if (token->type == TOKEN_OPEN_SQUARE)
		{
			skipTo (token, TOKEN_CLOSE_SQUARE);
			readToken (token);
		}
	}
}

static jsonKind tokenToKind (const tokenType type)
{
	switch (type)
	{
		case TOKEN_OPEN_CURLY:	return TAG_OBJECT;
		case TOKEN_OPEN_SQUARE:	return TAG_ARRAY;
		case TOKEN_STRING:		return TAG_STRING;
		case TOKEN_TRUE:
		case TOKEN_FALSE:		return TAG_BOOLEAN;
		case TOKEN_NUMBER:		return TAG_NUMBER;
		default:				return TAG_NULL;
	}
}

static void parseValue (tokenInfo *const token)
{
	if (token->type == TOKEN_OPEN_CURLY)
	{
		tokenInfo *name = newToken ();

		do
		{
			readTokenFull (token, TRUE);
			if (token->type == TOKEN_STRING)
			{
				jsonKind tagKind = TAG_NULL; /* default in case of invalid value */

				copyToken (name, token);

				/* skip any possible garbage before the value */
				skipToOneOf3 (token, TOKEN_CLOSE_CURLY, TOKEN_COLON, TOKEN_COMMA);

				if (token->type == TOKEN_COLON)
				{
					readToken (token);
					tagKind = tokenToKind (token->type);

					pushScope (token, name, tagKind);
					parseValue (token);
					popScope (token, name);
				}

				makeJsonTag (name, tagKind);
			}
			/* skip to the end of the construct */
			skipToOneOf2 (token, TOKEN_CLOSE_CURLY, TOKEN_COMMA);
		}
		while (token->type != TOKEN_EOF &&
			   token->type != TOKEN_CLOSE_CURLY);

		if (token->type == TOKEN_CLOSE_CURLY)
			readToken (token);

		deleteToken (name);
	}
	else if (token->type == TOKEN_OPEN_SQUARE)
	{
		tokenInfo *name = newToken ();
		char buf[32];
		unsigned int nth = 0;

		readToken (token);
		while (token->type != TOKEN_EOF &&
			   token->type != TOKEN_CLOSE_SQUARE)
		{
			jsonKind tagKind;

			tagKind = tokenToKind (token->type);

			copyToken (name, token);
			snprintf (buf, sizeof buf, "%u", nth++);
			vStringCopyS (name->string, buf);

			makeJsonTag (name, tagKind);
			pushScope (token, name, tagKind);
			parseValue (token);
			popScope (token, name);

			/* skip to the end of the construct */
			skipToOneOf2 (token, TOKEN_CLOSE_SQUARE, TOKEN_COMMA);
			if (token->type != TOKEN_CLOSE_SQUARE)
				readToken (token);
		}

		if (token->type == TOKEN_CLOSE_SQUARE)
			readToken (token);

		deleteToken (name);
	}
}

static void findJsonTags (void)
{
	tokenInfo *const token = newToken ();

	/* We allow multiple top-level elements, although it's not actually valid
	 * JSON.  An interesting side effect of this is that we allow a leading
	 * Unicode BOM mark -- even though ok, many JSON parsers will choke on it */
	do
	{
		readToken (token);
		parseValue (token);
	}
	while (token->type != TOKEN_EOF);

	deleteToken (token);
}

static void initialize (const langType language)
{
	Lang_json = language;
	addKeyword ("true", language, KEYWORD_true);
	addKeyword ("false", language, KEYWORD_false);
	addKeyword ("null", language, KEYWORD_null);
}

/* Create parser definition structure */
extern parserDefinition* JsonParser (void)
{
	static const char *const extensions [] = { "json", NULL };
	parserDefinition *const def = parserNew ("JSON");
	def->extensions = extensions;
	def->kinds		= JsonKinds;
	def->kindCount	= KIND_COUNT (JsonKinds);
	def->parser		= findJsonTags;
	def->initialize = initialize;

	return def;
}
