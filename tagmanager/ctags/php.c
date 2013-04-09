/*
*   Copyright (c) 2013, Colomban Wendling <ban@herbesfolles.org>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains code for generating tags for the PHP scripting
*   language.
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


typedef enum {
	KEYWORD_NONE = -1,
	KEYWORD_abstract,
	KEYWORD_and,
	KEYWORD_as,
	KEYWORD_break,
	KEYWORD_callable,
	KEYWORD_case,
	KEYWORD_catch,
	KEYWORD_class,
	KEYWORD_clone,
	KEYWORD_const,
	KEYWORD_continue,
	KEYWORD_declare,
	KEYWORD_define,
	KEYWORD_default,
	KEYWORD_do,
	KEYWORD_echo,
	KEYWORD_else,
	KEYWORD_elif,
	KEYWORD_enddeclare,
	KEYWORD_endfor,
	KEYWORD_endforeach,
	KEYWORD_endif,
	KEYWORD_endswitch,
	KEYWORD_endwhile,
	KEYWORD_extends,
	KEYWORD_final,
	KEYWORD_for,
	KEYWORD_foreach,
	KEYWORD_function,
	KEYWORD_global,
	KEYWORD_goto,
	KEYWORD_if,
	KEYWORD_implements,
	KEYWORD_include,
	KEYWORD_include_once,
	KEYWORD_instanceof,
	KEYWORD_insteadof,
	KEYWORD_interface,
	KEYWORD_namespace,
	KEYWORD_new,
	KEYWORD_or,
	KEYWORD_print,
	KEYWORD_private,
	KEYWORD_protected,
	KEYWORD_public,
	KEYWORD_require,
	KEYWORD_require_once,
	KEYWORD_return,
	KEYWORD_static,
	KEYWORD_switch,
	KEYWORD_throw,
	KEYWORD_trait,
	KEYWORD_try,
	KEYWORD_use,
	KEYWORD_var,
	KEYWORD_while,
	KEYWORD_xor
} keywordId;

typedef enum {
	ACCESS_UNDEFINED,
	ACCESS_PRIVATE,
	ACCESS_PROTECTED,
	ACCESS_PUBLIC,
	COUNT_ACCESS
} accessType;

typedef enum {
	IMPL_UNDEFINED,
	IMPL_ABSTRACT,
	COUNT_IMPL
} implType;

typedef enum {
	K_CLASS,
	K_DEFINE,
	K_FUNCTION,
	K_INTERFACE,
	K_VARIABLE,
	COUNT_KIND
} phpKind;

static kindOption PhpKinds[COUNT_KIND] = {
	{ TRUE, 'c', "class",		"classes" },
	{ TRUE, 'm', "macro",		"constant definitions" },
	{ TRUE, 'f', "function",	"functions" },
	{ TRUE, 'i', "interface",	"interfaces" },
	{ TRUE, 'v', "variable",	"variables" }
};

typedef struct {
	const char *name;
	keywordId id;
} keywordDesc;

static const keywordDesc PhpKeywordTable[] = {
	/* keyword			keyword ID */
	{ "abstract",		KEYWORD_abstract		},
	{ "and",			KEYWORD_and				},
	{ "as",				KEYWORD_as				},
	{ "break",			KEYWORD_break			},
	{ "callable",		KEYWORD_callable		},
	{ "case",			KEYWORD_case			},
	{ "catch",			KEYWORD_catch			},
	{ "cfunction",		KEYWORD_function		}, /* nobody knows what the hell this is, but it seems to behave much like "function" so bind it to it */
	{ "class",			KEYWORD_class			},
	{ "clone",			KEYWORD_clone			},
	{ "const",			KEYWORD_const			},
	{ "continue",		KEYWORD_continue		},
	{ "declare",		KEYWORD_declare			},
	{ "define",			KEYWORD_define			}, /* this isn't really a keyword but we handle it so it's easier this way */
	{ "default",		KEYWORD_default			},
	{ "do",				KEYWORD_do				},
	{ "echo",			KEYWORD_echo			},
	{ "else",			KEYWORD_else			},
	{ "elseif",			KEYWORD_elif			},
	{ "enddeclare",		KEYWORD_enddeclare		},
	{ "endfor",			KEYWORD_endfor			},
	{ "endforeach",		KEYWORD_endforeach		},
	{ "endif",			KEYWORD_endif			},
	{ "endswitch",		KEYWORD_endswitch		},
	{ "endwhile",		KEYWORD_endwhile		},
	{ "extends",		KEYWORD_extends			},
	{ "final",			KEYWORD_final			},
	{ "for",			KEYWORD_for				},
	{ "foreach",		KEYWORD_foreach			},
	{ "function",		KEYWORD_function		},
	{ "global",			KEYWORD_global			},
	{ "goto",			KEYWORD_goto			},
	{ "if",				KEYWORD_if				},
	{ "implements",		KEYWORD_implements		},
	{ "include",		KEYWORD_include			},
	{ "include_once",	KEYWORD_include_once	},
	{ "instanceof",		KEYWORD_instanceof		},
	{ "insteadof",		KEYWORD_insteadof		},
	{ "interface",		KEYWORD_interface		},
	{ "namespace",		KEYWORD_namespace		},
	{ "new",			KEYWORD_new				},
	{ "or",				KEYWORD_or				},
	{ "print",			KEYWORD_print			},
	{ "private",		KEYWORD_private			},
	{ "protected",		KEYWORD_protected		},
	{ "public",			KEYWORD_public			},
	{ "require",		KEYWORD_require			},
	{ "require_once",	KEYWORD_require_once	},
	{ "return",			KEYWORD_return			},
	{ "static",			KEYWORD_static			},
	{ "switch",			KEYWORD_switch			},
	{ "throw",			KEYWORD_throw			},
	{ "trait",			KEYWORD_trait			},
	{ "try",			KEYWORD_try				},
	{ "use",			KEYWORD_use				},
	{ "var",			KEYWORD_var				},
	{ "while",			KEYWORD_while			},
	{ "xor",			KEYWORD_xor				}
};


typedef enum eTokenType {
	TOKEN_UNDEFINED,
	TOKEN_EOF,
	TOKEN_CHARACTER,
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
	keywordId		keyword;
	vString *		string;
	vString *		scope;
	unsigned long 	lineNumber;
	MIOPos			filePosition;
	int 			parentKind; /* -1 if none */
} tokenInfo;

static langType Lang_php;

static boolean InPhp = FALSE; /* whether we are between <? ?> */

/* current statement details */
struct {
	accessType access;
	implType impl;
} CurrentStatement;


static void buildPhpKeywordHash (void)
{
	const size_t count = sizeof (PhpKeywordTable) / sizeof (PhpKeywordTable[0]);
	size_t i;
	for (i = 0; i < count ; i++)
	{
		const keywordDesc* const p = &PhpKeywordTable[i];
		addKeyword (p->name, Lang_php, (int) p->id);
	}
}

static const char *accessToString (const accessType access)
{
	static const char *const names[COUNT_ACCESS] = {
		"undefined",
		"private",
		"protected",
		"public"
	};

	Assert (access < COUNT_ACCESS);

	return names[access];
}

static const char *implToString (const implType impl)
{
	static const char *const names[COUNT_IMPL] = {
		"undefined",
		"abstract"
	};

	Assert (impl < COUNT_IMPL);

	return names[impl];
}

static void initPhpEntry (tagEntryInfo *e, const tokenInfo *const token, phpKind kind, accessType access)
{
	initTagEntry (e, vStringValue (token->string));

	e->lineNumber	= token->lineNumber;
	e->filePosition	= token->filePosition;
	e->kindName		= PhpKinds[kind].name;
	e->kind			= (char) PhpKinds[kind].letter;

	if (access != ACCESS_UNDEFINED)
		e->extensionFields.access = accessToString (access);
	if (vStringLength(token->scope) > 0)
	{
		Assert (token->parentKind >= 0);

		e->extensionFields.scope[0] = PhpKinds[token->parentKind].name;
		e->extensionFields.scope[1] = vStringValue (token->scope);
	}
}

static void makeSimplePhpTag (tokenInfo *const token, phpKind kind, accessType access)
{
	if (PhpKinds[kind].enabled)
	{
		tagEntryInfo e;

		initPhpEntry (&e, token, kind, access);
		makeTagEntry (&e);
	}
}

static void makeClassOrIfaceTag (phpKind kind, tokenInfo *const token,
								 vString *const inheritance, implType impl)
{
	if (PhpKinds[kind].enabled)
	{
		tagEntryInfo e;

		initPhpEntry (&e, token, kind, ACCESS_UNDEFINED);

		if (impl != IMPL_UNDEFINED)
			e.extensionFields.implementation = implToString (impl);
		if (vStringLength (inheritance) > 0)
			e.extensionFields.inheritance = vStringValue (inheritance);

		makeTagEntry (&e);
	}
}

static void makeFunctionTag (tokenInfo *const token, vString *const arglist,
							 accessType access, implType impl)
{ 
	if (PhpKinds[K_FUNCTION].enabled)
	{
		tagEntryInfo e;

		initPhpEntry (&e, token, K_FUNCTION, access);

		if (impl != IMPL_UNDEFINED)
			e.extensionFields.implementation = implToString (impl);
		if (arglist)
			e.extensionFields.arglist = vStringValue (arglist);

		makeTagEntry (&e);
	}
}

static tokenInfo *newToken (void)
{
	tokenInfo *const token = xMalloc (1, tokenInfo);

	token->type			= TOKEN_UNDEFINED;
	token->keyword		= KEYWORD_NONE;
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

static void copyToken (tokenInfo *const dest, tokenInfo *const src, boolean scope)
{
	dest->lineNumber = src->lineNumber;
	dest->filePosition = src->filePosition;
	dest->type = src->type;
	dest->keyword = src->keyword;
	vStringCopy(dest->string, src->string);
	dest->parentKind = src->parentKind;
	if (scope)
		vStringCopy(dest->scope, src->scope);
}

#if 0
#include <stdio.h>

static const char *tokenTypeName (const tokenType type)
{
	switch (type)
	{
		case TOKEN_UNDEFINED:		return "undefined";
		case TOKEN_EOF:				return "EOF";
		case TOKEN_CHARACTER:		return "character";
		case TOKEN_CLOSE_PAREN:		return "')'";
		case TOKEN_SEMICOLON:		return "';'";
		case TOKEN_COLON:			return "':'";
		case TOKEN_COMMA:			return "','";
		case TOKEN_OPEN_PAREN:		return "'('";
		case TOKEN_OPERATOR:		return "operator";
		case TOKEN_IDENTIFIER:		return "identifier";
		case TOKEN_KEYWORD:			return "keyword";
		case TOKEN_STRING:			return "string";
		case TOKEN_PERIOD:			return "'.'";
		case TOKEN_OPEN_CURLY:		return "'{'";
		case TOKEN_CLOSE_CURLY:		return "'}'";
		case TOKEN_EQUAL_SIGN:		return "'='";
		case TOKEN_OPEN_SQUARE:		return "'['";
		case TOKEN_CLOSE_SQUARE:	return "']'";
		case TOKEN_VARIABLE:		return "variable";
	}
	return NULL;
}

static void printToken (const tokenInfo *const token)
{
	fprintf (stderr, "%p:\n\ttype:\t%s\n\tline:\t%lu\n\tscope:\t%s\n", (void *) token,
			 tokenTypeName (token->type),
			 token->lineNumber,
			 vStringValue (token->scope));
	switch (token->type)
	{
		case TOKEN_IDENTIFIER:
		case TOKEN_STRING:
		case TOKEN_VARIABLE:
			fprintf (stderr, "\tcontent:\t%s\n", vStringValue (token->string));
			break;

		case TOKEN_KEYWORD:
		{
			size_t n = sizeof PhpKeywordTable / sizeof PhpKeywordTable[0];
			size_t i;

			fprintf (stderr, "\tkeyword:\t");
			for (i = 0; i < n; i++)
			{
				if (PhpKeywordTable[i].id == token->keyword)
				{
					fprintf (stderr, "%s\n", PhpKeywordTable[i].name);
					break;
				}
			}
			if (i >= n)
				fprintf (stderr, "(unknown)\n");
		}

		default: break;
	}
}
#endif

static void addToScope (tokenInfo* const token, vString* const extra)
{
	if (vStringLength (token->scope) > 0)
		vStringCatS (token->scope, "." /* "::" */);
	vStringCatS (token->scope, vStringValue (extra));
	vStringTerminate(token->scope);
}

static boolean isIdentChar (const int c)
{
	return (isalnum (c) || c == '_');
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

static int skipToEndOfLine (void)
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

static keywordId analyzeToken (vString *const name, langType language)
{
	vString *keyword = vStringNew ();
	keywordId result;
	vStringCopyToLower (keyword, name);
	result = lookupKeyword (vStringValue (keyword), language);
	vStringDelete (keyword);
	return result;
}

static int findPhpStart (void)
{
	int c;
	do
	{
		if ((c = fileGetc ()) == '<' &&
			(c = fileGetc ()) == '?' &&
			/* don't enter PHP mode on "<?xml", yet still support short open tags (<?) */
			(tolower ((c = fileGetc ())) != 'x' ||
			 tolower ((c = fileGetc ())) != 'm' ||
			 tolower ((c = fileGetc ())) != 'l'))
		{
			break;
		}
	}
	while (c != EOF);

	return c;
}

static void readToken (tokenInfo *const token)
{
	int c;

	token->type		= TOKEN_UNDEFINED;
	token->keyword	= KEYWORD_NONE;
	vStringClear (token->string);

getNextChar:

	if (! InPhp)
	{
		c = findPhpStart ();
		if (c != EOF)
			InPhp = TRUE;
	}
	else
		c = fileGetc ();

	while (c == '\t' || c == ' ' || c == '\n' || c == '\r')
	{
		c = fileGetc ();
	}

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

		case '=':
		{
			int d = fileGetc ();
			if (d == '=' || d == '>')
				token->type = TOKEN_OPERATOR;
			else
			{
				fileUngetc (d);
				token->type = TOKEN_EQUAL_SIGN;
			}
			break;
		}

		case '\'':
		case '"':
			token->type = TOKEN_STRING;
			parseString (token->string, c);
			token->lineNumber = getSourceLineNumber ();
			token->filePosition = getInputFilePosition ();
			break;

		case '#': /* comment */
			skipToEndOfLine ();
			goto getNextChar;
			break;

		case '+':
		case '-':
		case '*':
		case '%':
		{
			int d = fileGetc ();
			if (d != '=')
				fileUngetc (d);
			token->type = TOKEN_OPERATOR;
			break;
		}

		case '/': /* division or comment start */
		{
			int d = fileGetc ();
			if (d == '/') /* single-line comment */
			{
				skipToEndOfLine ();
				goto getNextChar;
			}
			else if (d == '*')
			{
				do
				{
					c = skipToCharacter ('*');
					if (c != EOF)
					{
						c = fileGetc ();
						if (c == '/')
							break;
						else
							fileUngetc (c);
					}
				} while (c != EOF && c != '\0');
				goto getNextChar;
			}
			else
			{
				if (d != '=')
					fileUngetc (d);
				token->type = TOKEN_OPERATOR;
			}
			break;
		}

		case '$': /* variable start */
			parseIdentifier (token->string, c);
			token->type = TOKEN_VARIABLE;
			break;

		case '?': /* maybe the end of the PHP chunk */
		{
			int d = fileGetc ();
			if (d == '>')
			{
				InPhp = FALSE;
				goto getNextChar;
			}
			else
			{
				fileUngetc (d);
				token->type = TOKEN_UNDEFINED;
			}
			break;
		}

		default:
			if (! isIdentChar (c))
				token->type = TOKEN_UNDEFINED;
			else
			{
				parseIdentifier (token->string, c);
				token->keyword = analyzeToken (token->string, Lang_php);
				if (token->keyword == KEYWORD_NONE)
					token->type = TOKEN_IDENTIFIER;
				else
					token->type = TOKEN_KEYWORD;
			}
			break;
	}

	if (token->type == TOKEN_SEMICOLON ||
		token->type == TOKEN_OPEN_CURLY ||
		token->type == TOKEN_CLOSE_CURLY)
	{
		/* reset current statement details on statement end, and when entering
		 * a deeper scope.
		 * it is a bit ugly to do this in readToken(), but it makes everything
		 * a lot simpler. */
		CurrentStatement.access = ACCESS_UNDEFINED;
		CurrentStatement.impl = IMPL_UNDEFINED;
	}
}

static void enterScope (tokenInfo *const token, vString *const scope, int parentKind);

/* parses a class or an interface:
 * 	class Foo {}
 * 	class Foo extends Bar {}
 * 	class Foo extends Bar implements iFoo, iBar {}
 * 	interface iFoo {}
 * 	interface iBar extends iFoo {} */
static boolean parseClassOrIface (tokenInfo *const token, phpKind kind)
{
	boolean readNext = TRUE;
	implType impl = CurrentStatement.impl;
	tokenInfo *name;
	vString *inheritance = NULL;

	readToken (token);
	if (token->type != TOKEN_IDENTIFIER)
		return FALSE;

	name = newToken ();
	copyToken (name, token, TRUE);

	inheritance = vStringNew ();
	/* skip until the open bracket and assume every identifier (not keyword)
	 * is an inheritance (like in "class Foo extends Bar implements iA, iB") */
	do
	{
		readToken (token);

		if (token->type == TOKEN_IDENTIFIER)
		{
			if (vStringLength (inheritance) > 0)
				vStringPut (inheritance, ',');
			vStringCat (inheritance, token->string);
		}
	}
	while (token->type != TOKEN_EOF &&
		   token->type != TOKEN_OPEN_CURLY);

	makeClassOrIfaceTag (kind, name, inheritance, impl);

	if (token->type == TOKEN_OPEN_CURLY)
		enterScope (token, name->string, K_CLASS);
	else
		readNext = FALSE;

	deleteToken (name);
	vStringDelete (inheritance);

	return readNext;
}

static boolean parseFunction (tokenInfo *const token)
{
	boolean readNext = TRUE;
	accessType access = CurrentStatement.access;
	implType impl = CurrentStatement.impl;
	tokenInfo *name;

	readToken (token);
	if (token->type != TOKEN_IDENTIFIER)
		return FALSE;

	name = newToken ();
	copyToken (name, token, TRUE);

	readToken (token);
	if (token->type == TOKEN_OPEN_PAREN)
	{
		vString *arglist = vStringNew ();
		int depth = 1;
		int c;

		vStringPut (arglist, '(');
		do
		{
			c = fileGetc ();
			if (c != EOF)
			{
				vStringPut (arglist, (char) c);
				if (c == '(')
					depth++;
				else if (c == ')')
					depth--;
			}
		}
		while (depth > 0 && c != EOF);
		vStringTerminate (arglist);

		makeFunctionTag (name, arglist, access, impl);
		vStringDelete (arglist);

		readToken (token); /* normally it's an open brace */
	}
	if (token->type == TOKEN_OPEN_CURLY)
		enterScope (token, name->string, K_FUNCTION);
	else
		readNext = FALSE;

	deleteToken (name);

	return readNext;
}

/* parses declarations of the form
 * 	const NAME = VALUE */
static boolean parseConstant (tokenInfo *const token)
{
	tokenInfo *name;

	readToken (token); /* skip const keyword */
	if (token->type != TOKEN_IDENTIFIER)
		return FALSE;

	name = newToken ();
	copyToken (name, token, TRUE);

	readToken (token);
	if (token->type == TOKEN_EQUAL_SIGN)
		makeSimplePhpTag (name, K_DEFINE, ACCESS_UNDEFINED);

	deleteToken (name);

	return token->type == TOKEN_EQUAL_SIGN;
}

/* parses declarations of the form
 * 	define('NAME', 'VALUE')
 * 	define(NAME, 'VALUE) */
static boolean parseDefine (tokenInfo *const token)
{
	int depth = 1;

	readToken (token); /* skip "define" identifier */
	if (token->type != TOKEN_OPEN_PAREN)
		return FALSE;

	readToken (token);
	if (token->type == TOKEN_STRING ||
		token->type == TOKEN_IDENTIFIER)
	{
		makeSimplePhpTag (token, K_DEFINE, ACCESS_UNDEFINED);
		readToken (token);
	}

	/* skip until the close parenthesis.
	 * no need to handle nested blocks since they would be invalid
	 * in this context anyway (the VALUE may only be a scalar, like
	 * 	42
	 * 	(42)
	 * and alike) */
	while (token->type != TOKEN_EOF && depth > 0)
	{
		switch (token->type)
		{
			case TOKEN_OPEN_PAREN:	depth++; break;
			case TOKEN_CLOSE_PAREN:	depth--; break;
			default: break;
		}
		readToken (token);
	}

	return FALSE;
}

/* parses declarations of the form
 * 	$var = VALUE
 * 	$var; */
static boolean parseVariable (tokenInfo *const token)
{
	tokenInfo *name;
	boolean readNext = TRUE;
	accessType access = CurrentStatement.access;

	/* don't generate variable tags inside functions */
	if (token->parentKind == K_FUNCTION)
		return TRUE;

	name = newToken ();
	copyToken (name, token, TRUE);

	readToken (token);
	if (token->type == TOKEN_EQUAL_SIGN || token->type == TOKEN_SEMICOLON)
		makeSimplePhpTag (name, K_VARIABLE, access);
	else
		readNext = FALSE;

	deleteToken (name);

	return readNext;
}

static void enterScope (tokenInfo *const parentToken, vString *const extraScope, int parentKind)
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
				switch (token->keyword)
				{
					case KEYWORD_class:		readNext = parseClassOrIface (token, K_CLASS);		break;
					case KEYWORD_interface:	readNext = parseClassOrIface (token, K_INTERFACE);	break;
					case KEYWORD_function:	readNext = parseFunction (token);					break;
					case KEYWORD_const:		readNext = parseConstant (token);					break;
					case KEYWORD_define:	readNext = parseDefine (token);						break;

					case KEYWORD_private:	CurrentStatement.access = ACCESS_PRIVATE;	break;
					case KEYWORD_protected:	CurrentStatement.access = ACCESS_PROTECTED;	break;
					case KEYWORD_public:	CurrentStatement.access = ACCESS_PUBLIC;	break;
					case KEYWORD_var:		CurrentStatement.access = ACCESS_PUBLIC;	break;

					case KEYWORD_abstract:	CurrentStatement.impl = IMPL_ABSTRACT;		break;

					default: break;
				}
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

static void findPhpTags (void)
{
	tokenInfo *const token = newToken ();

	InPhp = FALSE;
	CurrentStatement.access = ACCESS_UNDEFINED;
	CurrentStatement.impl = IMPL_UNDEFINED;

	do
	{
		enterScope (token, NULL, -1);
	}
	while (token->type != TOKEN_EOF); /* keep going even with unmatched braces */

	deleteToken (token);
}

static void initialize (const langType language)
{
	Lang_php = language;
	buildPhpKeywordHash ();
}

extern parserDefinition* PhpParser (void)
{
	static const char *const extensions [] = { "php", "php3", "php4", "php5", "phtml", NULL };
	parserDefinition* def = parserNew ("PHP");
	def->kinds      = PhpKinds;
	def->kindCount  = KIND_COUNT (PhpKinds);
	def->extensions = extensions;
	def->parser     = findPhpTags;
	def->initialize = initialize;
	return def;
}

/* vi:set tabstop=4 shiftwidth=4: */
