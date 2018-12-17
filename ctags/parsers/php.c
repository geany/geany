/*
*   Copyright (c) 2013, Colomban Wendling <ban@herbesfolles.org>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains code for generating tags for the PHP scripting
*   language.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "parse.h"
#include "read.h"
#include "vstring.h"
#include "keyword.h"
#include "entry.h"
#include "routines.h"
#include "debug.h"


#define SCOPE_SEPARATOR "::"


enum {
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
	KEYWORD_finally,
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
	KEYWORD_xor,
	KEYWORD_yield
};
typedef int keywordId; /* to allow KEYWORD_NONE */

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
	K_LOCAL_VARIABLE,
	K_NAMESPACE,
	K_TRAIT,
	K_VARIABLE,
	COUNT_KIND
} phpKind;

static kindDefinition PhpKinds[COUNT_KIND] = {
	{ true, 'c', "class",		"classes" },
	{ true, 'd', "define",		"constant definitions" },
	{ true, 'f', "function",	"functions" },
	{ true, 'i', "interface",	"interfaces" },
	{ false, 'l', "local",		"local variables" },
	{ true, 'n', "namespace",	"namespaces" },
	{ true, 't', "trait",		"traits" },
	{ true, 'v', "variable",	"variables" }
};

static const keywordTable PhpKeywordTable[] = {
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
	{ "finally",		KEYWORD_finally			},
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
	{ "xor",			KEYWORD_xor				},
	{ "yield",			KEYWORD_yield			}
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
	TOKEN_VARIABLE,
	TOKEN_AMPERSAND
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
static langType Lang_zephir;

static bool InPhp = false; /* whether we are between <? ?> */

/* current statement details */
static struct {
	accessType access;
	implType impl;
} CurrentStatement;

/* Current namespace */
static vString *CurrentNamespace;


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

static void initPhpEntry (tagEntryInfo *const e, const tokenInfo *const token,
						  const phpKind kind, const accessType access)
{
	static vString *fullScope = NULL;
	int parentKind = -1;

	if (fullScope == NULL)
		fullScope = vStringNew ();
	else
		vStringClear (fullScope);

	if (vStringLength (CurrentNamespace) > 0)
	{
		vStringCopy (fullScope, CurrentNamespace);
		parentKind = K_NAMESPACE;
	}

	initTagEntry (e, vStringValue (token->string), kind);

	e->lineNumber	= token->lineNumber;
	e->filePosition	= token->filePosition;

	if (access != ACCESS_UNDEFINED)
		e->extensionFields.access = accessToString (access);
	if (vStringLength (token->scope) > 0)
	{
		parentKind = token->parentKind;
		if (vStringLength (fullScope) > 0)
			vStringCatS (fullScope, SCOPE_SEPARATOR);
		vStringCat (fullScope, token->scope);
	}
	if (vStringLength (fullScope) > 0)
	{
		Assert (parentKind >= 0);

		e->extensionFields.scopeKindIndex = parentKind;
		e->extensionFields.scopeName = vStringValue (fullScope);
	}
}

static void makeSimplePhpTag (const tokenInfo *const token, const phpKind kind,
							  const accessType access)
{
	if (PhpKinds[kind].enabled)
	{
		tagEntryInfo e;

		initPhpEntry (&e, token, kind, access);
		makeTagEntry (&e);
	}
}

static void makeNamespacePhpTag (const tokenInfo *const token, const vString *const name)
{
	if (PhpKinds[K_NAMESPACE].enabled)
	{
		tagEntryInfo e;

		initTagEntry (&e, vStringValue (name), K_NAMESPACE);

		e.lineNumber	= token->lineNumber;
		e.filePosition	= token->filePosition;

		makeTagEntry (&e);
	}
}

static void makeClassOrIfaceTag (const phpKind kind, const tokenInfo *const token,
								 vString *const inheritance, const implType impl)
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

static void makeFunctionTag (const tokenInfo *const token,
							 const vString *const arglist,
							 const accessType access, const implType impl)
{
	if (PhpKinds[K_FUNCTION].enabled)
	{
		tagEntryInfo e;

		initPhpEntry (&e, token, K_FUNCTION, access);

		if (impl != IMPL_UNDEFINED)
			e.extensionFields.implementation = implToString (impl);
		if (arglist)
			e.extensionFields.signature = vStringValue (arglist);

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
	token->lineNumber   = getInputLineNumber ();
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
					   bool scope)
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
			size_t n = ARRAY_SIZE (PhpKeywordTable);
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

static void addToScope (tokenInfo *const token, const vString *const extra)
{
	if (vStringLength (token->scope) > 0)
		vStringCatS (token->scope, SCOPE_SEPARATOR);
	vStringCatS (token->scope, vStringValue (extra));
}

static bool isIdentChar (const int c)
{
	return (isalnum (c) || c == '_' || c >= 0x80);
}

static void parseString (vString *const string, const int delimiter)
{
	while (true)
	{
		int c = getcFromInputFile ();

		if (c == '\\' && (c = getcFromInputFile ()) != EOF)
			vStringPut (string, (char) c);
		else if (c == EOF || c == delimiter)
			break;
		else
			vStringPut (string, (char) c);
	}
}

/* reads an HereDoc or a NowDoc (the part after the <<<).
 * 	<<<[ \t]*(ID|'ID'|"ID")
 * 	...
 * 	ID;?
 *
 * note that:
 *  1) starting ID must be immediately followed by a newline;
 *  2) closing ID is the same as opening one;
 *  3) closing ID must be immediately followed by a newline or a semicolon
 *     then a newline.
 *
 * Example of a *single* valid heredoc:
 * 	<<< FOO
 * 	something
 * 	something else
 * 	FOO this is not an end
 * 	FOO; this isn't either
 * 	FOO; # neither this is
 * 	FOO;
 * 	# previous line was the end, but the semicolon wasn't required
 */
static void parseHeredoc (vString *const string)
{
	int c;
	unsigned int len;
	char delimiter[64]; /* arbitrary limit, but more is crazy anyway */
	int quote = 0;

	do
	{
		c = getcFromInputFile ();
	}
	while (c == ' ' || c == '\t');

	if (c == '\'' || c == '"')
	{
		quote = c;
		c = getcFromInputFile ();
	}
	for (len = 0; len < ARRAY_SIZE (delimiter) - 1; len++)
	{
		if (! isIdentChar (c))
			break;
		delimiter[len] = (char) c;
		c = getcFromInputFile ();
	}
	delimiter[len] = 0;

	if (len == 0) /* no delimiter, give up */
		goto error;
	if (quote)
	{
		if (c != quote) /* no closing quote for quoted identifier, give up */
			goto error;
		c = getcFromInputFile ();
	}
	if (c != '\r' && c != '\n') /* missing newline, give up */
		goto error;

	do
	{
		c = getcFromInputFile ();

		if (c != '\r' && c != '\n')
			vStringPut (string, (char) c);
		else
		{
			/* new line, check for a delimiter right after */
			int nl = c;
			int extra = EOF;

			c = getcFromInputFile ();
			for (len = 0; c != 0 && (c - delimiter[len]) == 0; len++)
				c = getcFromInputFile ();

			if (delimiter[len] != 0)
				ungetcToInputFile (c);
			else
			{
				/* line start matched the delimiter, now check whether there
				 * is anything after it */
				if (c == '\r' || c == '\n')
				{
					ungetcToInputFile (c);
					break;
				}
				else if (c == ';')
				{
					int d = getcFromInputFile ();
					if (d == '\r' || d == '\n')
					{
						/* put back the semicolon since it's not part of the
						 * string.  we can't put back the newline, but it's a
						 * whitespace character nobody cares about it anyway */
						ungetcToInputFile (';');
						break;
					}
					else
					{
						/* put semicolon in the string and continue */
						extra = ';';
						ungetcToInputFile (d);
					}
				}
			}
			/* if we are here it wasn't a delimiter, so put everything in the
			 * string */
			vStringPut (string, (char) nl);
			vStringNCatS (string, delimiter, len);
			if (extra != EOF)
				vStringPut (string, (char) extra);
		}
	}
	while (c != EOF);

	return;

error:
	ungetcToInputFile (c);
}

static void parseIdentifier (vString *const string, const int firstChar)
{
	int c = firstChar;
	do
	{
		vStringPut (string, (char) c);
		c = getcFromInputFile ();
	} while (isIdentChar (c));
	ungetcToInputFile (c);
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

static bool isSpace (int c)
{
	return (c == '\t' || c == ' ' || c == '\v' ||
			c == '\n' || c == '\r' || c == '\f');
}

static int skipWhitespaces (int c)
{
	while (isSpace (c))
		c = getcFromInputFile ();
	return c;
}

/* <script[:white:]+language[:white:]*=[:white:]*(php|'php'|"php")[:white:]*>
 * 
 * This is ugly, but the whole "<script language=php>" tag is and we can't
 * really do better without adding a lot of code only for this */
static bool isOpenScriptLanguagePhp (int c)
{
	int quote = 0;

	/* <script[:white:]+language[:white:]*= */
	if (c                                   != '<' ||
		tolower ((c = getcFromInputFile ()))         != 's' ||
		tolower ((c = getcFromInputFile ()))         != 'c' ||
		tolower ((c = getcFromInputFile ()))         != 'r' ||
		tolower ((c = getcFromInputFile ()))         != 'i' ||
		tolower ((c = getcFromInputFile ()))         != 'p' ||
		tolower ((c = getcFromInputFile ()))         != 't' ||
		! isSpace ((c = getcFromInputFile ()))              ||
		tolower ((c = skipWhitespaces (c))) != 'l' ||
		tolower ((c = getcFromInputFile ()))         != 'a' ||
		tolower ((c = getcFromInputFile ()))         != 'n' ||
		tolower ((c = getcFromInputFile ()))         != 'g' ||
		tolower ((c = getcFromInputFile ()))         != 'u' ||
		tolower ((c = getcFromInputFile ()))         != 'a' ||
		tolower ((c = getcFromInputFile ()))         != 'g' ||
		tolower ((c = getcFromInputFile ()))         != 'e' ||
		(c = skipWhitespaces (getcFromInputFile ())) != '=')
		return false;

	/* (php|'php'|"php")> */
	c = skipWhitespaces (getcFromInputFile ());
	if (c == '"' || c == '\'')
	{
		quote = c;
		c = getcFromInputFile ();
	}
	if (tolower (c)                         != 'p' ||
		tolower ((c = getcFromInputFile ()))         != 'h' ||
		tolower ((c = getcFromInputFile ()))         != 'p' ||
		(quote != 0 && (c = getcFromInputFile ()) != quote) ||
		(c = skipWhitespaces (getcFromInputFile ())) != '>')
		return false;

	return true;
}

static int findPhpStart (void)
{
	int c;
	do
	{
		if ((c = getcFromInputFile ()) == '<')
		{
			c = getcFromInputFile ();
			/* <? and <?php, but not <?xml */
			if (c == '?')
			{
				/* don't enter PHP mode on "<?xml", yet still support short open tags (<?) */
				if (tolower ((c = getcFromInputFile ())) != 'x' ||
					tolower ((c = getcFromInputFile ())) != 'm' ||
					tolower ((c = getcFromInputFile ())) != 'l')
				{
					break;
				}
			}
			/* <script language="php"> */
			else
			{
				ungetcToInputFile (c);
				if (isOpenScriptLanguagePhp ('<'))
					break;
			}
		}
	}
	while (c != EOF);

	return c;
}

static int skipSingleComment (void)
{
	int c;
	do
	{
		c = getcFromInputFile ();
		if (c == '\r')
		{
			int next = getcFromInputFile ();
			if (next != '\n')
				ungetcToInputFile (next);
			else
				c = next;
		}
		/* ?> in single-line comments leaves PHP mode */
		else if (c == '?')
		{
			int next = getcFromInputFile ();
			if (next == '>')
				InPhp = false;
			else
				ungetcToInputFile (next);
		}
	} while (InPhp && c != EOF && c != '\n' && c != '\r');
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
			InPhp = true;
	}
	else
		c = getcFromInputFile ();

	c = skipWhitespaces (c);

	token->lineNumber   = getInputLineNumber ();
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
		case '&': token->type = TOKEN_AMPERSAND;			break;

		case '=':
		{
			int d = getcFromInputFile ();
			if (d == '=' || d == '>')
				token->type = TOKEN_OPERATOR;
			else
			{
				ungetcToInputFile (d);
				token->type = TOKEN_EQUAL_SIGN;
			}
			break;
		}

		case '\'':
		case '"':
			token->type = TOKEN_STRING;
			parseString (token->string, c);
			token->lineNumber = getInputLineNumber ();
			token->filePosition = getInputFilePosition ();
			break;

		case '<':
		{
			int d = getcFromInputFile ();
			if (d == '/')
			{
				/* </script[:white:]*> */
				if (tolower ((d = getcFromInputFile ())) == 's' &&
					tolower ((d = getcFromInputFile ())) == 'c' &&
					tolower ((d = getcFromInputFile ())) == 'r' &&
					tolower ((d = getcFromInputFile ())) == 'i' &&
					tolower ((d = getcFromInputFile ())) == 'p' &&
					tolower ((d = getcFromInputFile ())) == 't' &&
					(d = skipWhitespaces (getcFromInputFile ())) == '>')
				{
					InPhp = false;
					goto getNextChar;
				}
				else
				{
					ungetcToInputFile (d);
					token->type = TOKEN_UNDEFINED;
				}
			}
			else if (d == '<' && (d = getcFromInputFile ()) == '<')
			{
				token->type = TOKEN_STRING;
				parseHeredoc (token->string);
			}
			else
			{
				ungetcToInputFile (d);
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
		case '%':
		{
			int d = getcFromInputFile ();
			if (d != '=')
				ungetcToInputFile (d);
			token->type = TOKEN_OPERATOR;
			break;
		}

		case '/': /* division or comment start */
		{
			int d = getcFromInputFile ();
			if (d == '/') /* single-line comment */
			{
				skipSingleComment ();
				goto getNextChar;
			}
			else if (d == '*')
			{
				do
				{
					c = skipToCharacterInInputFile ('*');
					if (c != EOF)
					{
						c = getcFromInputFile ();
						if (c == '/')
							break;
						else
							ungetcToInputFile (c);
					}
				} while (c != EOF && c != '\0');
				goto getNextChar;
			}
			else
			{
				if (d != '=')
					ungetcToInputFile (d);
				token->type = TOKEN_OPERATOR;
			}
			break;
		}

		case '$': /* variable start */
		{
			int d = getcFromInputFile ();
			if (! isIdentChar (d))
			{
				ungetcToInputFile (d);
				token->type = TOKEN_UNDEFINED;
			}
			else
			{
				parseIdentifier (token->string, d);
				token->type = TOKEN_VARIABLE;
			}
			break;
		}

		case '?': /* maybe the end of the PHP chunk */
		{
			int d = getcFromInputFile ();
			if (d == '>')
			{
				InPhp = false;
				goto getNextChar;
			}
			else
			{
				ungetcToInputFile (d);
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
				token->keyword = analyzeToken (token->string, getInputLanguage ());
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

static void enterScope (tokenInfo *const parentToken,
						const vString *const extraScope,
						const int parentKind);

/* parses a class or an interface:
 * 	class Foo {}
 * 	class Foo extends Bar {}
 * 	class Foo extends Bar implements iFoo, iBar {}
 * 	interface iFoo {}
 * 	interface iBar extends iFoo {} */
static bool parseClassOrIface (tokenInfo *const token, const phpKind kind)
{
	bool readNext = true;
	implType impl = CurrentStatement.impl;
	tokenInfo *name;
	vString *inheritance = NULL;

	readToken (token);
	if (token->type != TOKEN_IDENTIFIER)
		return false;

	name = newToken ();
	copyToken (name, token, true);

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
		readNext = false;

	deleteToken (name);
	vStringDelete (inheritance);

	return readNext;
}

/* parses a trait:
 * 	trait Foo {} */
static bool parseTrait (tokenInfo *const token)
{
	bool readNext = true;
	tokenInfo *name;

	readToken (token);
	if (token->type != TOKEN_IDENTIFIER)
		return false;

	name = newToken ();
	copyToken (name, token, true);

	makeSimplePhpTag (name, K_TRAIT, ACCESS_UNDEFINED);

	readToken (token);
	if (token->type == TOKEN_OPEN_CURLY)
		enterScope (token, name->string, K_TRAIT);
	else
		readNext = false;

	deleteToken (name);

	return readNext;
}

/* parse a function
 *
 * if @name is NULL, parses a normal function
 * 	function myfunc($foo, $bar) {}
 * 	function &myfunc($foo, $bar) {}
 *
 * if @name is not NULL, parses an anonymous function with name @name
 * 	$foo = function($foo, $bar) {}
 * 	$foo = function&($foo, $bar) {}
 * 	$foo = function($foo, $bar) use ($x, &$y) {} */
static bool parseFunction (tokenInfo *const token, const tokenInfo *name)
{
	bool readNext = true;
	accessType access = CurrentStatement.access;
	implType impl = CurrentStatement.impl;
	tokenInfo *nameFree = NULL;

	readToken (token);
	/* skip a possible leading ampersand (return by reference) */
	if (token->type == TOKEN_AMPERSAND)
		readToken (token);

	if (! name)
	{
		if (token->type != TOKEN_IDENTIFIER)
			return false;

		name = nameFree = newToken ();
		copyToken (nameFree, token, true);
		readToken (token);
	}

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
				case TOKEN_AMPERSAND:		vStringPut (arglist, '&');		break;
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

		makeFunctionTag (name, arglist, access, impl);
		vStringDelete (arglist);

		readToken (token); /* normally it's an open brace or "use" keyword */
	}

	/* if parsing Zephir, skip function return type hint */
	if (getInputLanguage () == Lang_zephir && token->type == TOKEN_OPERATOR)
	{
		do
			readToken (token);
		while (token->type != TOKEN_EOF &&
			   token->type != TOKEN_OPEN_CURLY &&
			   token->type != TOKEN_CLOSE_CURLY &&
			   token->type != TOKEN_SEMICOLON);
	}

	/* skip use(...) */
	if (token->type == TOKEN_KEYWORD && token->keyword == KEYWORD_use)
	{
		readToken (token);
		if (token->type == TOKEN_OPEN_PAREN)
		{
			int depth = 1;

			do
			{
				readToken (token);
				switch (token->type)
				{
					case TOKEN_OPEN_PAREN:  depth++; break;
					case TOKEN_CLOSE_PAREN: depth--; break;
					default: break;
				}
			}
			while (token->type != TOKEN_EOF && depth > 0);

			readToken (token);
		}
	}

	if (token->type == TOKEN_OPEN_CURLY)
		enterScope (token, name->string, K_FUNCTION);
	else
		readNext = false;

	if (nameFree)
		deleteToken (nameFree);

	return readNext;
}

/* parses declarations of the form
 * 	const NAME = VALUE */
static bool parseConstant (tokenInfo *const token)
{
	tokenInfo *name;

	readToken (token); /* skip const keyword */
	if (token->type != TOKEN_IDENTIFIER)
		return false;

	name = newToken ();
	copyToken (name, token, true);

	readToken (token);
	if (token->type == TOKEN_EQUAL_SIGN)
		makeSimplePhpTag (name, K_DEFINE, ACCESS_UNDEFINED);

	deleteToken (name);

	return token->type == TOKEN_EQUAL_SIGN;
}

/* parses declarations of the form
 * 	define('NAME', 'VALUE')
 * 	define(NAME, 'VALUE) */
static bool parseDefine (tokenInfo *const token)
{
	int depth = 1;

	readToken (token); /* skip "define" identifier */
	if (token->type != TOKEN_OPEN_PAREN)
		return false;

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

	return false;
}

/* parses declarations of the form
 * 	$var = VALUE
 * 	$var; */
static bool parseVariable (tokenInfo *const token)
{
	tokenInfo *name;
	bool readNext = true;
	accessType access = CurrentStatement.access;

	name = newToken ();
	copyToken (name, token, true);

	readToken (token);
	if (token->type == TOKEN_EQUAL_SIGN)
	{
		phpKind kind = K_VARIABLE;

		if (token->parentKind == K_FUNCTION)
			kind = K_LOCAL_VARIABLE;

		readToken (token);
		if (token->type == TOKEN_KEYWORD &&
			token->keyword == KEYWORD_function &&
			PhpKinds[kind].enabled)
		{
			if (parseFunction (token, name))
				readToken (token);
			readNext = (bool) (token->type == TOKEN_SEMICOLON);
		}
		else
		{
			makeSimplePhpTag (name, kind, access);
			readNext = false;
		}
	}
	else if (token->type == TOKEN_SEMICOLON)
	{
		/* generate tags for variable declarations in classes
		 * 	class Foo {
		 * 		protected $foo;
		 * 	}
		 * but don't get fooled by stuff like $foo = $bar; */
		if (token->parentKind == K_CLASS || token->parentKind == K_INTERFACE)
			makeSimplePhpTag (name, K_VARIABLE, access);
	}
	else
		readNext = false;

	deleteToken (name);

	return readNext;
}

/* parses namespace declarations
 * 	namespace Foo {}
 * 	namespace Foo\Bar {}
 * 	namespace Foo;
 * 	namespace Foo\Bar;
 * 	namespace;
 * 	napespace {} */
static bool parseNamespace (tokenInfo *const token)
{
	tokenInfo *nsToken = newToken ();

	vStringClear (CurrentNamespace);
	copyToken (nsToken, token, false);

	do
	{
		readToken (token);
		if (token->type == TOKEN_IDENTIFIER)
		{
			if (vStringLength (CurrentNamespace) > 0)
				vStringPut (CurrentNamespace, '\\');
			vStringCat (CurrentNamespace, token->string);
		}
	}
	while (token->type != TOKEN_EOF &&
		   token->type != TOKEN_SEMICOLON &&
		   token->type != TOKEN_OPEN_CURLY);

	if (vStringLength (CurrentNamespace) > 0)
		makeNamespacePhpTag (nsToken, CurrentNamespace);

	if (token->type == TOKEN_OPEN_CURLY)
		enterScope (token, NULL, -1);

	deleteToken (nsToken);

	return true;
}

static void enterScope (tokenInfo *const parentToken,
						const vString *const extraScope,
						const int parentKind)
{
	tokenInfo *token = newToken ();
	int origParentKind = parentToken->parentKind;

	copyToken (token, parentToken, true);

	if (extraScope)
	{
		addToScope (token, extraScope);
		token->parentKind = parentKind;
	}

	readToken (token);
	while (token->type != TOKEN_EOF &&
		   token->type != TOKEN_CLOSE_CURLY)
	{
		bool readNext = true;

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
					case KEYWORD_trait:		readNext = parseTrait (token);						break;
					case KEYWORD_function:	readNext = parseFunction (token, NULL);				break;
					case KEYWORD_const:		readNext = parseConstant (token);					break;
					case KEYWORD_define:	readNext = parseDefine (token);						break;

					case KEYWORD_namespace:	readNext = parseNamespace (token);	break;

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

	copyToken (parentToken, token, false);
	parentToken->parentKind = origParentKind;
	deleteToken (token);
}

static void findTags (void)
{
	tokenInfo *const token = newToken ();

	CurrentStatement.access = ACCESS_UNDEFINED;
	CurrentStatement.impl = IMPL_UNDEFINED;
	CurrentNamespace = vStringNew ();

	do
	{
		enterScope (token, NULL, -1);
	}
	while (token->type != TOKEN_EOF); /* keep going even with unmatched braces */

	vStringDelete (CurrentNamespace);
	deleteToken (token);
}

static void findPhpTags (void)
{
	InPhp = false;
	findTags ();
}

static void findZephirTags (void)
{
	InPhp = true;
	findTags ();
}

static void initializePhpParser (const langType language)
{
	Lang_php = language;
}

static void initializeZephirParser (const langType language)
{
	Lang_zephir = language;
}

extern parserDefinition* PhpParser (void)
{
	static const char *const extensions [] = { "php", "php3", "php4", "php5", "phtml", NULL };
	parserDefinition* def = parserNew ("PHP");
	def->kindTable  = PhpKinds;
	def->kindCount  = ARRAY_SIZE (PhpKinds);
	def->extensions = extensions;
	def->parser     = findPhpTags;
	def->initialize = initializePhpParser;
	def->keywordTable = PhpKeywordTable;
	def->keywordCount = ARRAY_SIZE (PhpKeywordTable);
	return def;
}

extern parserDefinition* ZephirParser (void)
{
	static const char *const extensions [] = { "zep", NULL };
	parserDefinition* def = parserNew ("Zephir");
	def->kindTable  = PhpKinds;
	def->kindCount  = ARRAY_SIZE (PhpKinds);
	def->extensions = extensions;
	def->parser     = findZephirTags;
	def->initialize = initializeZephirParser;
	def->keywordTable = PhpKeywordTable;
	def->keywordCount = ARRAY_SIZE (PhpKeywordTable);
	return def;
}
