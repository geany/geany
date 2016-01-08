/*
 *	 Copyright (c) 2003, Darren Hiebert
 *
 *	 This source code is released for free distribution under the terms of the
 *	 GNU General Public License.
 *
 *	 This module contains functions for generating tags for JavaScript language
 *	 files.
 *
 *	 This is a good reference for different forms of the function statement:
 *		 http://www.permadi.com/tutorial/jsFunc/
 *   Another good reference:
 *       http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Guide
 */

/*
 *	 INCLUDE FILES
 */
#include "general.h"	/* must always come first */
#include <ctype.h>	/* to define isalpha () */
#include <string.h>
#include <mio/mio.h>
#ifdef DEBUG
#include <stdio.h>
#endif

#include "keyword.h"
#include "parse.h"
#include "read.h"
#include "main.h"
#include "vstring.h"

/*
 *	 MACROS
 */
#define isType(token,t)		(boolean) ((token)->type == (t))
#define isKeyword(token,k)	(boolean) ((token)->keyword == (k))

/*
 *	 DATA DECLARATIONS
 */

/*
 * Tracks class and function names already created
 */
static stringList *ClassNames;
static stringList *FunctionNames;

/*	Used to specify type of keyword.
*/
typedef enum eKeywordId {
	KEYWORD_NONE = -1,
	KEYWORD_function,
	KEYWORD_capital_function,
	KEYWORD_capital_object,
	KEYWORD_prototype,
	KEYWORD_var,
	KEYWORD_let,
	KEYWORD_const,
	KEYWORD_new,
	KEYWORD_this,
	KEYWORD_for,
	KEYWORD_while,
	KEYWORD_do,
	KEYWORD_if,
	KEYWORD_else,
	KEYWORD_switch,
	KEYWORD_try,
	KEYWORD_catch,
	KEYWORD_finally,
	KEYWORD_sap,
	KEYWORD_return
} keywordId;

/*	Used to determine whether keyword is valid for the token language and
 *	what its ID is.
 */
typedef struct sKeywordDesc {
	const char *name;
	keywordId id;
} keywordDesc;

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
	TOKEN_FORWARD_SLASH,
	TOKEN_OPEN_SQUARE,
	TOKEN_CLOSE_SQUARE,
	TOKEN_REGEXP,
	TOKEN_POSTFIX_OPERATOR,
	TOKEN_BINARY_OPERATOR
} tokenType;

typedef struct sTokenInfo {
	tokenType		type;
	keywordId		keyword;
	vString *		string;
	vString *		scope;
	unsigned long 	lineNumber;
	MIOPos			filePosition;
	int				nestLevel;
	boolean			ignoreTag;
} tokenInfo;

/*
 *	DATA DEFINITIONS
 */

static tokenType LastTokenType;

static langType Lang_js;

typedef enum {
	JSTAG_FUNCTION,
	JSTAG_CLASS,
	JSTAG_METHOD,
	JSTAG_PROPERTY,
	JSTAG_CONSTANT,
	JSTAG_VARIABLE,
	JSTAG_COUNT
} jsKind;

static kindOption JsKinds [] = {
	{ TRUE,  'f', "function",	  "functions"		   },
	{ TRUE,  'c', "class",		  "classes"			   },
	{ TRUE,  'm', "method",		  "methods"			   },
	{ TRUE,  'p', "member",		  "properties"		   },
	{ TRUE,  'C', "macro",		  "constants"		   },
	{ TRUE,  'v', "variable",	  "global variables"   }
};

static const keywordDesc JsKeywordTable [] = {
	/* keyword		keyword ID */
	{ "function",	KEYWORD_function			},
	{ "Function",	KEYWORD_capital_function	},
	{ "Object",		KEYWORD_capital_object		},
	{ "prototype",	KEYWORD_prototype			},
	{ "var",		KEYWORD_var					},
	{ "let",		KEYWORD_let					},
	{ "const",		KEYWORD_const				},
	{ "new",		KEYWORD_new					},
	{ "this",		KEYWORD_this				},
	{ "for",		KEYWORD_for					},
	{ "while",		KEYWORD_while				},
	{ "do",			KEYWORD_do					},
	{ "if",			KEYWORD_if					},
	{ "else",		KEYWORD_else				},
	{ "switch",		KEYWORD_switch				},
	{ "try",		KEYWORD_try					},
	{ "catch",		KEYWORD_catch				},
	{ "finally",	KEYWORD_finally				},
	{ "sap",	    KEYWORD_sap    				},
	{ "return",		KEYWORD_return				}
};

/*
 *	 FUNCTION DEFINITIONS
 */

/* Recursive functions */
static void parseFunction (tokenInfo *const token);
static boolean parseBlock (tokenInfo *const token, tokenInfo *const orig_parent);
static boolean parseLine (tokenInfo *const token, tokenInfo *const parent, boolean is_inside_class);
static void parseUI5 (tokenInfo *const token);

static boolean isIdentChar (const int c)
{
	return (boolean)
		(isalpha (c) || isdigit (c) || c == '$' ||
		 c == '@' || c == '_' || c == '#');
}

static void buildJsKeywordHash (void)
{
	const size_t count = sizeof (JsKeywordTable) /
		sizeof (JsKeywordTable [0]);
	size_t i;
	for (i = 0	;  i < count  ;  ++i)
	{
		const keywordDesc* const p = &JsKeywordTable [i];
		addKeyword (p->name, Lang_js, (int) p->id);
	}
}

static tokenInfo *newToken (void)
{
	tokenInfo *const token = xMalloc (1, tokenInfo);

	token->type			= TOKEN_UNDEFINED;
	token->keyword		= KEYWORD_NONE;
	token->string		= vStringNew ();
	token->scope		= vStringNew ();
	token->nestLevel	= 0;
	token->ignoreTag	= FALSE;
	token->lineNumber   = getSourceLineNumber ();
	token->filePosition = getInputFilePosition ();

	return token;
}

static void deleteToken (tokenInfo *const token)
{
	vStringDelete (token->string);
	vStringDelete (token->scope);
	eFree (token);
}

/*
 *	 Tag generation functions
 */

static void makeJsTag (tokenInfo *const token, const jsKind kind, vString *const signature)
{
	if (JsKinds [kind].enabled && ! token->ignoreTag )
	{
		const char *name = vStringValue (token->string);
		vString *fullscope = vStringNewCopy (token->scope);
		const char *p;
		tagEntryInfo e;

		if ((p = strrchr (name, '.')) != NULL)
		{
			if (vStringLength (fullscope) > 0)
				vStringPut (fullscope, '.');
			vStringNCatS (fullscope, name, p - name);
			name = p + 1;
		}

		initTagEntry (&e, name);

		e.lineNumber   = token->lineNumber;
		e.filePosition = token->filePosition;
		e.kindName	   = JsKinds [kind].name;
		e.kind		   = JsKinds [kind].letter;

		if ( vStringLength(fullscope) > 0 )
		{
			jsKind parent_kind = JSTAG_CLASS;

			/* if we're creating a function (and not a method),
			 * guess we're inside another function */
			if (kind == JSTAG_FUNCTION)
				parent_kind = JSTAG_FUNCTION;

			e.extensionFields.scope[0] = JsKinds [parent_kind].name;
			e.extensionFields.scope[1] = vStringValue (fullscope);
		}

		if (signature && vStringLength(signature))
		{
			size_t i;
			/* sanitize signature by replacing all control characters with a
			 * space (because it's simple).
			 * there should never be any junk in a valid signature, but who
			 * knows what the user wrote and CTags doesn't cope well with weird
			 * characters. */
			for (i = 0; i < signature->length; i++)
			{
				unsigned char c = (unsigned char) signature->buffer[i];
				if (c < 0x20 /* below space */ || c == 0x7F /* DEL */)
					signature->buffer[i] = ' ';
			}
			e.extensionFields.arglist = vStringValue(signature);
		}

		makeTagEntry (&e);
		vStringDelete (fullscope);
	}
}

static void makeClassTag (tokenInfo *const token, vString *const signature)
{
	vString *	fulltag;

	if ( ! token->ignoreTag )
	{
		fulltag = vStringNew ();
		if (vStringLength (token->scope) > 0)
		{
			vStringCopy(fulltag, token->scope);
			vStringCatS (fulltag, ".");
			vStringCatS (fulltag, vStringValue(token->string));
		}
		else
		{
			vStringCopy(fulltag, token->string);
		}
		vStringTerminate(fulltag);
		if ( ! stringListHas(ClassNames, vStringValue (fulltag)) )
		{
			stringListAdd (ClassNames, vStringNewCopy (fulltag));
			makeJsTag (token, JSTAG_CLASS, signature);
		}
		vStringDelete (fulltag);
	}
}

static void makeFunctionTag (tokenInfo *const token, vString *const signature)
{
	vString *	fulltag;

	if ( ! token->ignoreTag )
	{
		fulltag = vStringNew ();
		if (vStringLength (token->scope) > 0)
		{
			vStringCopy(fulltag, token->scope);
			vStringCatS (fulltag, ".");
			vStringCatS (fulltag, vStringValue(token->string));
		}
		else
		{
			vStringCopy(fulltag, token->string);
		}
		vStringTerminate(fulltag);
		if ( ! stringListHas(FunctionNames, vStringValue (fulltag)) )
		{
			stringListAdd (FunctionNames, vStringNewCopy (fulltag));
			makeJsTag (token, JSTAG_FUNCTION, signature);
		}
		vStringDelete (fulltag);
	}
}

/*
 *	 Parsing functions
 */

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
	boolean end = FALSE;
	while (! end)
	{
		int c = fileGetc ();
		if (c == EOF)
			end = TRUE;
		else if (c == '\\')
		{
			/* Eat the escape sequence (\", \', etc).  We properly handle
			 * <LineContinuation> by eating a whole \<CR><LF> not to see <LF>
			 * as an unescaped character, which is invalid and handled below.
			 * Also, handle the fact that <LineContinuation> produces an empty
			 * sequence.
			 * See ECMA-262 7.8.4 */
			c = fileGetc();
			if (c != '\r' && c != '\n')
				vStringPut(string, c);
			else if (c == '\r')
			{
				c = fileGetc();
				if (c != '\n')
					fileUngetc (c);
			}
		}
		else if (c == delimiter)
			end = TRUE;
		else if (c == '\r' || c == '\n')
		{
			/* those are invalid when not escaped */
			end = TRUE;
			/* we don't want to eat the newline itself to let the automatic
			 * semicolon insertion code kick in */
			fileUngetc (c);
		}
		else
			vStringPut (string, c);
	}
	vStringTerminate (string);
}

static void parseRegExp (void)
{
	int c;
	boolean in_range = FALSE;

	do
	{
		c = fileGetc ();
		if (! in_range && c == '/')
		{
			do /* skip flags */
			{
				c = fileGetc ();
			} while (isalpha (c));
			fileUngetc (c);
			break;
		}
		else if (c == '\\')
			c = fileGetc (); /* skip next character */
		else if (c == '[')
			in_range = TRUE;
		else if (c == ']')
			in_range = FALSE;
	} while (c != EOF);
}

/*	Read a C identifier beginning with "firstChar" and places it into
 *	"name".
 */
static void parseIdentifier (vString *const string, const int firstChar)
{
	int c = firstChar;
	Assert (isIdentChar (c));
	do
	{
		vStringPut (string, c);
		c = fileGetc ();
	} while (isIdentChar (c));
	vStringTerminate (string);
	fileUngetc (c);		/* unget non-identifier character */
}

static keywordId analyzeToken (vString *const name)
{
	vString *keyword = vStringNew ();
	keywordId result;
	vStringCopyToLower (keyword, name);
	result = (keywordId) lookupKeyword (vStringValue (keyword), Lang_js);
	vStringDelete (keyword);
	return result;
}

static void readTokenFull (tokenInfo *const token, boolean include_newlines, vString *const repr)
{
	int c;
	int i;

	token->type			= TOKEN_UNDEFINED;
	token->keyword		= KEYWORD_NONE;
	vStringClear (token->string);

getNextChar:
	i = 0;
	do
	{
		c = fileGetc ();
		i++;
	}
	while (c == '\t'  ||  c == ' ' ||
		   ((c == '\r' || c == '\n') && ! include_newlines));

	token->lineNumber   = getSourceLineNumber ();
	token->filePosition = getInputFilePosition ();

	if (repr)
	{
		if (i > 1)
			vStringPut (repr, ' ');
		vStringPut (repr, c);
	}

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
		case '=': token->type = TOKEN_EQUAL_SIGN;			break;
		case '[': token->type = TOKEN_OPEN_SQUARE;			break;
		case ']': token->type = TOKEN_CLOSE_SQUARE;			break;

		case '+':
		case '-':
			{
				int d = fileGetc ();
				if (d == c) /* ++ or -- */
					token->type = TOKEN_POSTFIX_OPERATOR;
				else
				{
					fileUngetc (d);
					token->type = TOKEN_BINARY_OPERATOR;
				}
				break;
			}

		case '*':
		case '%':
		case '?':
		case '>':
		case '<':
		case '^':
		case '|':
		case '&':
			token->type = TOKEN_BINARY_OPERATOR;
			break;

		case '\r':
		case '\n':
			/* This isn't strictly correct per the standard, but following the
			 * real rules means understanding all statements, and that's not
			 * what the parser currently does.  What we do here is a guess, by
			 * avoiding inserting semicolons that would make the statement on
			 * the left invalid.  Hopefully this should not have false negatives
			 * (e.g. should not miss insertion of a semicolon) but might have
			 * false positives (e.g. it will wrongfully emit a semicolon for the
			 * newline in "foo\n+bar").
			 * This should however be mostly harmless as we only deal with
			 * newlines in specific situations where we know a false positive
			 * wouldn't hurt too bad. */
			switch (LastTokenType)
			{
				/* these cannot be the end of a statement, so hold the newline */
				case TOKEN_EQUAL_SIGN:
				case TOKEN_COLON:
				case TOKEN_PERIOD:
				case TOKEN_FORWARD_SLASH:
				case TOKEN_BINARY_OPERATOR:
				/* and these already end one, no need to duplicate it */
				case TOKEN_SEMICOLON:
				case TOKEN_COMMA:
				case TOKEN_CLOSE_CURLY:
				case TOKEN_OPEN_CURLY:
					include_newlines = FALSE; /* no need to recheck */
					goto getNextChar;
					break;
				default:
					token->type = TOKEN_SEMICOLON;
			}
			break;

		case '\'':
		case '"':
				  token->type = TOKEN_STRING;
				  parseString (token->string, c);
				  token->lineNumber = getSourceLineNumber ();
				  token->filePosition = getInputFilePosition ();
				  if (repr)
				  {
					  vStringCat (repr, token->string);
					  vStringPut (repr, c);
				  }
				  break;

		case '\\':
				  c = fileGetc ();
				  if (c != '\\'  && c != '"'  &&  !isspace (c))
					  fileUngetc (c);
				  token->type = TOKEN_CHARACTER;
				  token->lineNumber = getSourceLineNumber ();
				  token->filePosition = getInputFilePosition ();
				  break;

		case '/':
				  {
					  int d = fileGetc ();
					  if ( (d != '*') &&		/* is this the start of a comment? */
							  (d != '/') )		/* is a one line comment? */
					  {
						  fileUngetc (d);
						  switch (LastTokenType)
						  {
							  case TOKEN_CHARACTER:
							  case TOKEN_IDENTIFIER:
							  case TOKEN_STRING:
							  case TOKEN_CLOSE_CURLY:
							  case TOKEN_CLOSE_PAREN:
							  case TOKEN_CLOSE_SQUARE:
								  token->type = TOKEN_FORWARD_SLASH;
								  break;

							  default:
								  token->type = TOKEN_REGEXP;
								  parseRegExp ();
								  token->lineNumber = getSourceLineNumber ();
								  token->filePosition = getInputFilePosition ();
								  break;
						  }
					  }
					  else
					  {
						  if (repr) /* remove the / we added */
							  repr->buffer[--repr->length] = 0;
						  if (d == '*')
						  {
							  do
							  {
								  skipToCharacter ('*');
								  c = fileGetc ();
								  if (c == '/')
									  break;
								  else
									  fileUngetc (c);
							  } while (c != EOF && c != '\0');
							  goto getNextChar;
						  }
						  else if (d == '/')	/* is this the start of a comment?  */
						  {
							  skipToCharacter ('\n');
							  /* if we care about newlines, put it back so it is seen */
							  if (include_newlines)
								  fileUngetc ('\n');
							  goto getNextChar;
						  }
					  }
					  break;
				  }

		case '#':
				  /* skip shebang in case of e.g. Node.js scripts */
				  if (token->lineNumber > 1)
					  token->type = TOKEN_UNDEFINED;
				  else if ((c = fileGetc ()) != '!')
				  {
					  fileUngetc (c);
					  token->type = TOKEN_UNDEFINED;
				  }
				  else
				  {
					  skipToCharacter ('\n');
					  goto getNextChar;
				  }
				  break;

		default:
				  if (! isIdentChar (c))
					  token->type = TOKEN_UNDEFINED;
				  else
				  {
					  parseIdentifier (token->string, c);
					  token->lineNumber = getSourceLineNumber ();
					  token->filePosition = getInputFilePosition ();
					  token->keyword = analyzeToken (token->string);
					  if (isKeyword (token, KEYWORD_NONE))
						  token->type = TOKEN_IDENTIFIER;
					  else
						  token->type = TOKEN_KEYWORD;
					  if (repr && vStringLength (token->string) > 1)
						  vStringCatS (repr, vStringValue (token->string) + 1);
				  }
				  break;
	}

	LastTokenType = token->type;
}

static void readToken (tokenInfo *const token)
{
	readTokenFull (token, FALSE, NULL);
}

static void copyToken (tokenInfo *const dest, tokenInfo *const src)
{
	dest->nestLevel = src->nestLevel;
	dest->lineNumber = src->lineNumber;
	dest->filePosition = src->filePosition;
	dest->type = src->type;
	dest->keyword = src->keyword;
	vStringCopy(dest->string, src->string);
	vStringCopy(dest->scope, src->scope);
}

/*
 *	 Token parsing functions
 */

static void skipArgumentList (tokenInfo *const token, boolean include_newlines, vString *const repr)
{
	int nest_level = 0;

	if (isType (token, TOKEN_OPEN_PAREN))	/* arguments? */
	{
		nest_level++;
		if (repr)
			vStringPut (repr, '(');
		while (nest_level > 0 && ! isType (token, TOKEN_EOF))
		{
			readTokenFull (token, FALSE, repr);
			if (isType (token, TOKEN_OPEN_PAREN))
				nest_level++;
			else if (isType (token, TOKEN_CLOSE_PAREN))
				nest_level--;
		}
		readTokenFull (token, include_newlines, NULL);
	}
}

static void skipArrayList (tokenInfo *const token, boolean include_newlines)
{
	int nest_level = 0;

	/*
	 * Handle square brackets
	 *	 var name[1]
	 * So we must check for nested open and closing square brackets
	 */

	if (isType (token, TOKEN_OPEN_SQUARE))	/* arguments? */
	{
		nest_level++;
		while (nest_level > 0 && ! isType (token, TOKEN_EOF))
		{
			readToken (token);
			if (isType (token, TOKEN_OPEN_SQUARE))
				nest_level++;
			else if (isType (token, TOKEN_CLOSE_SQUARE))
				nest_level--;
		}
		readTokenFull (token, include_newlines, NULL);
	}
}

static void addContext (tokenInfo* const parent, const tokenInfo* const child)
{
	if (vStringLength (parent->string) > 0)
	{
		vStringCatS (parent->string, ".");
	}
	vStringCatS (parent->string, vStringValue(child->string));
	vStringTerminate(parent->string);
}

static void addToScope (tokenInfo* const token, vString* const extra)
{
	if (vStringLength (token->scope) > 0)
	{
		vStringCatS (token->scope, ".");
	}
	vStringCatS (token->scope, vStringValue(extra));
	vStringTerminate(token->scope);
}

/*
 *	 Scanning functions
 */

static boolean findCmdTerm (tokenInfo *const token, boolean include_newlines)
{
	/*
	 * Read until we find either a semicolon or closing brace.
	 * Any nested braces will be handled within.
	 */
	while (! isType (token, TOKEN_SEMICOLON) &&
		   ! isType (token, TOKEN_CLOSE_CURLY) &&
		   ! isType (token, TOKEN_EOF))
	{
		/* Handle nested blocks */
		if ( isType (token, TOKEN_OPEN_CURLY))
		{
			parseBlock (token, token);
			readTokenFull (token, include_newlines, NULL);
		}
		else if ( isType (token, TOKEN_OPEN_PAREN) )
		{
			skipArgumentList(token, include_newlines, NULL);
		}
		else if ( isType (token, TOKEN_OPEN_SQUARE) )
		{
			skipArrayList(token, include_newlines);
		}
		else
		{
			readTokenFull (token, include_newlines, NULL);
		}
	}

	return isType (token, TOKEN_SEMICOLON);
}

static void parseSwitch (tokenInfo *const token)
{
	/*
	 * switch (expression) {
	 * case value1:
	 *	   statement;
	 *	   break;
	 * case value2:
	 *	   statement;
	 *	   break;
	 * default : statement;
	 * }
	 */

	readToken (token);

	if (isType (token, TOKEN_OPEN_PAREN))
	{
		/*
		 * Handle nameless functions, these will only
		 * be considered methods.
		 */
		skipArgumentList(token, FALSE, NULL);
	}

	if (isType (token, TOKEN_OPEN_CURLY))
	{
		parseBlock (token, token);
	}
}

static boolean parseLoop (tokenInfo *const token, tokenInfo *const parent)
{
	/*
	 * Handles these statements
	 *	   for (x=0; x<3; x++)
	 *		   document.write("This text is repeated three times<br>");
	 *
	 *	   for (x=0; x<3; x++)
	 *	   {
	 *		   document.write("This text is repeated three times<br>");
	 *	   }
	 *
	 *	   while (number<5){
	 *		   document.write(number+"<br>");
	 *		   number++;
	 *	   }
	 *
	 *	   do{
	 *		   document.write(number+"<br>");
	 *		   number++;
	 *	   }
	 *	   while (number<5);
	 */
	boolean is_terminated = TRUE;

	if (isKeyword (token, KEYWORD_for) || isKeyword (token, KEYWORD_while))
	{
		readToken(token);

		if (isType (token, TOKEN_OPEN_PAREN))
		{
			/*
			 * Handle nameless functions, these will only
			 * be considered methods.
			 */
			skipArgumentList(token, FALSE, NULL);
		}

		if (isType (token, TOKEN_OPEN_CURLY))
		{
			/*
			 * This will be either a function or a class.
			 * We can only determine this by checking the body
			 * of the function.  If we find a "this." we know
			 * it is a class, otherwise it is a function.
			 */
			parseBlock (token, parent);
		}
		else
		{
			is_terminated = parseLine(token, parent, FALSE);
		}
	}
	else if (isKeyword (token, KEYWORD_do))
	{
		readToken(token);

		if (isType (token, TOKEN_OPEN_CURLY))
		{
			/*
			 * This will be either a function or a class.
			 * We can only determine this by checking the body
			 * of the function.  If we find a "this." we know
			 * it is a class, otherwise it is a function.
			 */
			parseBlock (token, parent);
		}
		else
		{
			is_terminated = parseLine(token, parent, FALSE);
		}

		if (is_terminated)
			readToken(token);

		if (isKeyword (token, KEYWORD_while))
		{
			readToken(token);

			if (isType (token, TOKEN_OPEN_PAREN))
			{
				/*
				 * Handle nameless functions, these will only
				 * be considered methods.
				 */
				skipArgumentList(token, TRUE, NULL);
			}
			if (! isType (token, TOKEN_SEMICOLON))
				is_terminated = FALSE;
		}
	}

	return is_terminated;
}

static boolean parseIf (tokenInfo *const token, tokenInfo *const parent)
{
	boolean read_next_token = TRUE;
	/*
	 * If statements have two forms
	 *	   if ( ... )
	 *		   one line;
	 *
	 *	   if ( ... )
	 *		  statement;
	 *	   else
	 *		  statement
	 *
	 *	   if ( ... ) {
	 *		  multiple;
	 *		  statements;
	 *	   }
	 *
	 *
	 *	   if ( ... ) {
	 *		  return elem
	 *	   }
	 *
	 *     This example if correctly written, but the
	 *     else contains only 1 statement without a terminator
	 *     since the function finishes with the closing brace.
	 *
     *     function a(flag){
     *         if(flag)
     *             test(1);
     *         else
     *             test(2)
     *     }
	 *
	 * TODO:  Deal with statements that can optional end
	 *		  without a semi-colon.  Currently this messes up
	 *		  the parsing of blocks.
	 *		  Need to somehow detect this has happened, and either
	 *		  backup a token, or skip reading the next token if
	 *		  that is possible from all code locations.
	 *
	 */

	readToken (token);

	if (isKeyword (token, KEYWORD_if))
	{
		/*
		 * Check for an "else if" and consume the "if"
		 */
		readToken (token);
	}

	if (isType (token, TOKEN_OPEN_PAREN))
	{
		/*
		 * Handle nameless functions, these will only
		 * be considered methods.
		 */
		skipArgumentList(token, FALSE, NULL);
	}

	if (isType (token, TOKEN_OPEN_CURLY))
	{
		/*
		 * This will be either a function or a class.
		 * We can only determine this by checking the body
		 * of the function.  If we find a "this." we know
		 * it is a class, otherwise it is a function.
		 */
		parseBlock (token, parent);
	}
	else
	{
		/* The next token should only be read if this statement had its own
		 * terminator */
		read_next_token = findCmdTerm (token, TRUE);
	}
	return read_next_token;
}

static void parseFunction (tokenInfo *const token)
{
	tokenInfo *const name = newToken ();
	vString *const signature = vStringNew ();
	boolean is_class = FALSE;

	/*
	 * This deals with these formats
	 *	   function validFunctionTwo(a,b) {}
	 */

	readToken (name);
	/* Add scope in case this is an INNER function */
	addToScope(name, token->scope);

	readToken (token);
	while (isType (token, TOKEN_PERIOD))
	{
		readToken (token);
		if ( isKeyword(token, KEYWORD_NONE) )
		{
			addContext (name, token);
			readToken (token);
		}
	}

	if ( isType (token, TOKEN_OPEN_PAREN) )
		skipArgumentList(token, FALSE, signature);

	if ( isType (token, TOKEN_OPEN_CURLY) )
	{
		is_class = parseBlock (token, name);
		if ( is_class )
			makeClassTag (name, signature);
		else
			makeFunctionTag (name, signature);
	}

	findCmdTerm (token, FALSE);

	vStringDelete (signature);
	deleteToken (name);
}

static boolean parseBlock (tokenInfo *const token, tokenInfo *const orig_parent)
{
	boolean is_class = FALSE;
	boolean read_next_token = TRUE;
	vString * saveScope = vStringNew ();
	tokenInfo *const parent = newToken ();

	/* backup the parent token to allow calls like parseBlock(token, token) */
	copyToken (parent, orig_parent);

	token->nestLevel++;
	/*
	 * Make this routine a bit more forgiving.
	 * If called on an open_curly advance it
	 */
	if ( isType (token, TOKEN_OPEN_CURLY) &&
			isKeyword(token, KEYWORD_NONE) )
		readToken(token);

	if (! isType (token, TOKEN_CLOSE_CURLY))
	{
		/*
		 * Read until we find the closing brace,
		 * any nested braces will be handled within
		 */
		do
		{
			read_next_token = TRUE;
			if (isKeyword (token, KEYWORD_this))
			{
				/*
				 * Means we are inside a class and have found
				 * a class, not a function
				 */
				is_class = TRUE;
				vStringCopy(saveScope, token->scope);
				addToScope (token, parent->string);

				/*
				 * Ignore the remainder of the line
				 * findCmdTerm(token);
				 */
				read_next_token = parseLine (token, parent, is_class);

				vStringCopy(token->scope, saveScope);
			}
			else if (isKeyword (token, KEYWORD_var) ||
					 isKeyword (token, KEYWORD_let) ||
					 isKeyword (token, KEYWORD_const))
			{
				/*
				 * Potentially we have found an inner function.
				 * Set something to indicate the scope
				 */
				vStringCopy(saveScope, token->scope);
				addToScope (token, parent->string);
				read_next_token = parseLine (token, parent, is_class);
				vStringCopy(token->scope, saveScope);
			}
			else if (isKeyword (token, KEYWORD_function))
			{
				vStringCopy(saveScope, token->scope);
				addToScope (token, parent->string);
				parseFunction (token);
				vStringCopy(token->scope, saveScope);
			}
			else if (isType (token, TOKEN_OPEN_CURLY))
			{
				/* Handle nested blocks */
				parseBlock (token, parent);
			}
			else
			{
				/*
				 * It is possible for a line to have no terminator
				 * if the following line is a closing brace.
				 * parseLine will detect this case and indicate
				 * whether we should read an additional token.
				 */
				read_next_token = parseLine (token, parent, is_class);
			}

			/*
			 * Always read a new token unless we find a statement without
			 * a ending terminator
			 */
			if( read_next_token )
				readToken(token);

			/*
			 * If we find a statement without a terminator consider the
			 * block finished, otherwise the stack will be off by one.
			 */
		} while (! isType (token, TOKEN_EOF) &&
				 ! isType (token, TOKEN_CLOSE_CURLY) && read_next_token);
	}

	deleteToken (parent);
	vStringDelete(saveScope);
	token->nestLevel--;

	return is_class;
}

static boolean parseMethods (tokenInfo *const token, tokenInfo *const class)
{
	tokenInfo *const name = newToken ();
	boolean has_methods = FALSE;

	/*
	 * This deals with these formats
	 *	   validProperty  : 2,
	 *	   validMethod    : function(a,b) {}
	 *	   'validMethod2' : function(a,b) {}
     *     container.dirtyTab = {'url': false, 'title':false, 'snapshot':false, '*': false}		
	 */

	do
	{
		readToken (token);
		if (isType (token, TOKEN_CLOSE_CURLY)) 
		{
			/*
			 * This was most likely a variable declaration of a hash table.
			 * indicate there were no methods and return.
			 */
			has_methods = FALSE;
			goto cleanUp;
		}

		if (isType (token, TOKEN_STRING) || isKeyword(token, KEYWORD_NONE))
		{
			copyToken(name, token);

			readToken (token);
			if ( isType (token, TOKEN_COLON) )
			{
				readToken (token);
				if ( isKeyword (token, KEYWORD_function) )
				{
					vString *const signature = vStringNew ();

					readToken (token);
					if ( isType (token, TOKEN_OPEN_PAREN) )
					{
						skipArgumentList(token, FALSE, signature);
					}

					if (isType (token, TOKEN_OPEN_CURLY))
					{
						has_methods = TRUE;
						addToScope (name, class->string);
						makeJsTag (name, JSTAG_METHOD, signature);
						parseBlock (token, name);

						/*
						 * Read to the closing curly, check next
						 * token, if a comma, we must loop again
						 */
						readToken (token);
					}

					vStringDelete (signature);
				}
				else
				{
						vString * saveScope = vStringNew ();
						boolean has_child_methods = FALSE;

						/* skip whatever is the value */
						while (! isType (token, TOKEN_COMMA) &&
						       ! isType (token, TOKEN_CLOSE_CURLY) &&
						       ! isType (token, TOKEN_EOF))
						{
							if (isType (token, TOKEN_OPEN_CURLY))
							{
								vStringCopy (saveScope, token->scope);
								addToScope (token, class->string);
								has_child_methods = parseMethods (token, name);
								vStringCopy (token->scope, saveScope);
								readToken (token);
							}
							else if (isType (token, TOKEN_OPEN_PAREN))
							{
								skipArgumentList (token, FALSE, NULL);
							}
							else if (isType (token, TOKEN_OPEN_SQUARE))
							{
								skipArrayList (token, FALSE);
							}
							else
							{
								readToken (token);
							}
						}
						vStringDelete (saveScope);

						has_methods = TRUE;
						addToScope (name, class->string);
						if (has_child_methods)
							makeJsTag (name, JSTAG_CLASS, NULL);
						else
							makeJsTag (name, JSTAG_PROPERTY, NULL);
				}
			}
		}
	} while ( isType(token, TOKEN_COMMA) );

	findCmdTerm (token, FALSE);

cleanUp:
	deleteToken (name);

	return has_methods;
}

static boolean parseStatement (tokenInfo *const token, tokenInfo *const parent, boolean is_inside_class)
{
	tokenInfo *const name = newToken ();
	tokenInfo *const secondary_name = newToken ();
	tokenInfo *const method_body_token = newToken ();
	vString * saveScope = vStringNew ();
	boolean is_class = FALSE;
	boolean is_var = FALSE;
	boolean is_const = FALSE;
	boolean is_terminated = TRUE;
	boolean is_global = FALSE;
	boolean has_methods = FALSE;
	vString *	fulltag;

	vStringClear(saveScope);
	/*
	 * Functions can be named or unnamed.
	 * This deals with these formats:
	 * Function
	 *	   validFunctionOne = function(a,b) {}
	 *	   testlib.validFunctionFive = function(a,b) {}
	 *	   var innerThree = function(a,b) {}
	 *	   var innerFour = (a,b) {}
	 *	   var D2 = secondary_fcn_name(a,b) {}
	 *	   var D3 = new Function("a", "b", "return a+b;");
	 * Class
	 *	   testlib.extras.ValidClassOne = function(a,b) {
	 *		   this.a = a;
	 *	   }
	 * Class Methods
	 *	   testlib.extras.ValidClassOne.prototype = {
	 *		   'validMethodOne' : function(a,b) {},
	 *		   'validMethodTwo' : function(a,b) {}
	 *	   }
     *     ValidClassTwo = function ()
     *     {
     *         this.validMethodThree = function() {}
     *         // unnamed method
     *         this.validMethodFour = () {}
     *     }
	 *	   Database.prototype.validMethodThree = Database_getTodaysDate;
	 */

	if ( is_inside_class )
		is_class = TRUE;
	/*
	 * var can precede an inner function
	 */
	if ( isKeyword(token, KEYWORD_var) ||
		 isKeyword(token, KEYWORD_let) ||
		 isKeyword(token, KEYWORD_const) )
	{
		is_const = isKeyword(token, KEYWORD_const);
		/*
		 * Only create variables for global scope
		 */
		if ( token->nestLevel == 0 )
		{
			is_global = TRUE;
		}
		readToken(token);
	}

	if ( isKeyword(token, KEYWORD_this) )
	{
		readToken(token);
		if (isType (token, TOKEN_PERIOD))
		{
			readToken(token);
		}
	}

	copyToken(name, token);

	while (! isType (token, TOKEN_CLOSE_CURLY) &&
	       ! isType (token, TOKEN_SEMICOLON)   &&
	       ! isType (token, TOKEN_EQUAL_SIGN)  &&
	       ! isType (token, TOKEN_EOF))
	{
		if (isType (token, TOKEN_OPEN_CURLY))
			parseBlock (token, parent);

		/* Potentially the name of the function */
		readToken (token);
		if (isType (token, TOKEN_PERIOD))
		{
			/*
			 * Cannot be a global variable is it has dot references in the name
			 */
			is_global = FALSE;
			do
			{
				readToken (token);
				if ( isKeyword(token, KEYWORD_NONE) )
				{
					if ( is_class )
					{
						addToScope(token, name->string);
					}
					else
						addContext (name, token);

					readToken (token);
				}
				else if ( isKeyword(token, KEYWORD_prototype) )
				{
					/*
					 * When we reach the "prototype" tag, we infer:
					 *     "BindAgent" is a class
					 *     "build"     is a method
					 *
					 * function BindAgent( repeatableIdName, newParentIdName ) {
					 * }
					 *
					 * CASE 1
					 * Specified function name: "build"
					 *     BindAgent.prototype.build = function( mode ) {
					 *     	  maybe parse nested functions
					 *     }
					 *
					 * CASE 2
					 * Prototype listing
					 *     ValidClassOne.prototype = {
					 *         'validMethodOne' : function(a,b) {},
					 *         'validMethodTwo' : function(a,b) {}
					 *     }
					 *
					 */
					makeClassTag (name, NULL);
					is_class = TRUE;

					/*
					 * There should a ".function_name" next.
					 */
					readToken (token);
					if (isType (token, TOKEN_PERIOD))
					{
						/*
						 * Handle CASE 1
						 */
						readToken (token);
						if ( isKeyword(token, KEYWORD_NONE) )
						{
							vString *const signature = vStringNew ();

							vStringCopy(saveScope, token->scope);
							addToScope(token, name->string);

							readToken (method_body_token);
							vStringCopy (method_body_token->scope, token->scope);

							while (! isType (method_body_token, TOKEN_SEMICOLON) &&
							       ! isType (method_body_token, TOKEN_CLOSE_CURLY) &&
							       ! isType (method_body_token, TOKEN_OPEN_CURLY) &&
							       ! isType (method_body_token, TOKEN_EOF))
							{
								if ( isType (method_body_token, TOKEN_OPEN_PAREN) )
									skipArgumentList(method_body_token, FALSE,
													 vStringLength (signature) == 0 ? signature : NULL);
								else
									readToken (method_body_token);
							}

							makeJsTag (token, JSTAG_METHOD, signature);
							vStringDelete (signature);

							if ( isType (method_body_token, TOKEN_OPEN_CURLY))
							{
								parseBlock (method_body_token, token);
								is_terminated = TRUE;
							}
							else
								is_terminated = isType (method_body_token, TOKEN_SEMICOLON);
							goto cleanUp;
						}
					}
					else if (isType (token, TOKEN_EQUAL_SIGN))
					{
						readToken (token);
						if (isType (token, TOKEN_OPEN_CURLY))
						{
							/*
							 * Handle CASE 2
							 *
							 * Creates tags for each of these class methods
							 *     ValidClassOne.prototype = {
							 *         'validMethodOne' : function(a,b) {},
							 *         'validMethodTwo' : function(a,b) {}
							 *     }
							 */
							parseMethods(token, name);
							/*
							 * Find to the end of the statement
							 */
							findCmdTerm (token, FALSE);
							token->ignoreTag = FALSE;
							is_terminated = TRUE;
							goto cleanUp;
						}
					}
				}
				else
					readToken (token);
			} while (isType (token, TOKEN_PERIOD));
		}

		if ( isType (token, TOKEN_OPEN_PAREN) )
			skipArgumentList(token, FALSE, NULL);

		if ( isType (token, TOKEN_OPEN_SQUARE) )
			skipArrayList(token, FALSE);

		/*
		if ( isType (token, TOKEN_OPEN_CURLY) )
		{
			is_class = parseBlock (token, name);
		}
		*/
	}

	if ( isType (token, TOKEN_CLOSE_CURLY) )
	{
		/*
		 * Reaching this section without having
		 * processed an open curly brace indicates
		 * the statement is most likely not terminated.
		 */
		is_terminated = FALSE;
		goto cleanUp;
	}

	if ( isType (token, TOKEN_SEMICOLON) )
	{
		/*
		 * Only create variables for global scope
		 */
		if ( token->nestLevel == 0 && is_global )
		{
			/*
			 * Handles this syntax:
			 *	   var g_var2;
			 */
			if (isType (token, TOKEN_SEMICOLON))
				makeJsTag (name, is_const ? JSTAG_CONSTANT : JSTAG_VARIABLE, NULL);
		}
		/*
		 * Statement has ended.
		 * This deals with calls to functions, like:
		 *     alert(..);
		 */
		goto cleanUp;
	}

	if ( isType (token, TOKEN_EQUAL_SIGN) )
	{
		int parenDepth = 0;

		readToken (token);

		/* rvalue might be surrounded with parentheses */
		while (isType (token, TOKEN_OPEN_PAREN))
		{
			parenDepth++;
			readToken (token);
		}

		if ( isKeyword (token, KEYWORD_function) )
		{
			vString *const signature = vStringNew ();

			readToken (token);

			if ( isKeyword (token, KEYWORD_NONE) &&
					! isType (token, TOKEN_OPEN_PAREN) )
			{
				/*
				 * Functions of this format:
				 *	   var D2A = function theAdd(a, b)
				 *	   {
				 *		  return a+b;
				 *	   }
				 * Are really two separate defined functions and
				 * can be referenced in two ways:
				 *	   alert( D2A(1,2) );			  // produces 3
				 *	   alert( theAdd(1,2) );		  // also produces 3
				 * So it must have two tags:
				 *	   D2A
				 *	   theAdd
				 * Save the reference to the name for later use, once
				 * we have established this is a valid function we will
				 * create the secondary reference to it.
				 */
				copyToken(secondary_name, token);
				readToken (token);
			}

			if ( isType (token, TOKEN_OPEN_PAREN) )
				skipArgumentList(token, FALSE, signature);

			if (isType (token, TOKEN_OPEN_CURLY))
			{
				/*
				 * This will be either a function or a class.
				 * We can only determine this by checking the body
				 * of the function.  If we find a "this." we know
				 * it is a class, otherwise it is a function.
				 */
				if ( is_inside_class )
				{
					makeJsTag (name, JSTAG_METHOD, signature);
					if ( vStringLength(secondary_name->string) > 0 )
						makeFunctionTag (secondary_name, signature);
					parseBlock (token, name);
				}
				else
				{
					is_class = parseBlock (token, name);
					if ( is_class )
						makeClassTag (name, signature);
					else
						makeFunctionTag (name, signature);

					if ( vStringLength(secondary_name->string) > 0 )
						makeFunctionTag (secondary_name, signature);
				}
			}

			vStringDelete (signature);
		}
		else if (isType (token, TOKEN_OPEN_CURLY))
		{
			/*
			 * Creates tags for each of these class methods
			 *     ValidClassOne.prototype = {
			 *         'validMethodOne' : function(a,b) {},
			 *         'validMethodTwo' : function(a,b) {}
			 *     }
			 * Or checks if this is a hash variable.
			 *     var z = {};
			 */
			has_methods = parseMethods(token, name);
			if (has_methods)
				makeJsTag (name, JSTAG_CLASS, NULL);
			else
			{
				/*
				 * Only create variables for global scope
			 */
				if ( token->nestLevel == 0 && is_global )
				{
					/*
					 * A pointer can be created to the function.  
					 * If we recognize the function/class name ignore the variable.
					 * This format looks identical to a variable definition.
					 * A variable defined outside of a block is considered
					 * a global variable:
					 *	   var g_var1 = 1;
					 *	   var g_var2;
					 * This is not a global variable:
					 *	   var g_var = function;
					 * This is a global variable:
					 *	   var g_var = different_var_name;
					 */
					fulltag = vStringNew ();
					if (vStringLength (token->scope) > 0)
					{
						vStringCopy(fulltag, token->scope);
						vStringCatS (fulltag, ".");
						vStringCatS (fulltag, vStringValue(token->string));
					}
					else
					{
						vStringCopy(fulltag, token->string);
					}
					vStringTerminate(fulltag);
					if ( ! stringListHas(FunctionNames, vStringValue (fulltag)) &&
							! stringListHas(ClassNames, vStringValue (fulltag)) )
					{
						makeJsTag (name, is_const ? JSTAG_CONSTANT : JSTAG_VARIABLE, NULL);
					}
					vStringDelete (fulltag);
				}
			}
			if (isType (token, TOKEN_CLOSE_CURLY)) 
			{
				/*
				 * Assume the closing parantheses terminates
				 * this statements.
				 */
				is_terminated = TRUE;
			}
		}
		else if (isKeyword (token, KEYWORD_new))
		{
			readToken (token);
			is_var = isType (token, TOKEN_IDENTIFIER);
			if ( isKeyword (token, KEYWORD_function) ||
					isKeyword (token, KEYWORD_capital_function) ||
					isKeyword (token, KEYWORD_capital_object) ||
					is_var )
			{
				if ( isKeyword (token, KEYWORD_capital_object) )
					is_class = TRUE;

				readToken (token);
				if ( isType (token, TOKEN_OPEN_PAREN) )
					skipArgumentList(token, TRUE, NULL);

				if (isType (token, TOKEN_SEMICOLON))
				{
					if ( token->nestLevel == 0 )
					{
						if ( is_var )
						{
							makeJsTag (name, is_const ? JSTAG_CONSTANT : JSTAG_VARIABLE, NULL);
						}
						else
						{
							if ( is_class )
							{
								makeClassTag (name, NULL);
							} else {
								/* FIXME: we cannot really get a meaningful
								 * signature from a `new Function()` call,
								 * so for now just don't set any */
								makeFunctionTag (name, NULL);
							}
						}
					}
				}
				else if (isType (token, TOKEN_CLOSE_CURLY))
					is_terminated = FALSE;
			}
		}
		else if (isKeyword (token, KEYWORD_NONE))
		{
			/*
			 * Only create variables for global scope
			 */
			if ( token->nestLevel == 0 && is_global )
			{
				/*
				 * A pointer can be created to the function.
				 * If we recognize the function/class name ignore the variable.
				 * This format looks identical to a variable definition.
				 * A variable defined outside of a block is considered
				 * a global variable:
				 *	   var g_var1 = 1;
				 *	   var g_var2;
				 * This is not a global variable:
				 *	   var g_var = function;
				 * This is a global variable:
				 *	   var g_var = different_var_name;
				 */
				fulltag = vStringNew ();
				if (vStringLength (token->scope) > 0)
				{
					vStringCopy(fulltag, token->scope);
					vStringCatS (fulltag, ".");
					vStringCatS (fulltag, vStringValue(token->string));
				}
				else
				{
					vStringCopy(fulltag, token->string);
				}
				vStringTerminate(fulltag);
				if ( ! stringListHas(FunctionNames, vStringValue (fulltag)) &&
						! stringListHas(ClassNames, vStringValue (fulltag)) )
				{
					makeJsTag (name, is_const ? JSTAG_CONSTANT : JSTAG_VARIABLE, NULL);
				}
				vStringDelete (fulltag);
			}
		}

		if (parenDepth > 0)
		{
			while (parenDepth > 0 && ! isType (token, TOKEN_EOF))
			{
				if (isType (token, TOKEN_OPEN_PAREN))
					parenDepth++;
				else if (isType (token, TOKEN_CLOSE_PAREN))
					parenDepth--;
				readTokenFull (token, TRUE, NULL);
			}
			if (isType (token, TOKEN_CLOSE_CURLY))
				is_terminated = FALSE;
		}
	}

	/* if we aren't already at the cmd end, advance to it and check whether
	 * the statement was terminated */
	if (! isType (token, TOKEN_CLOSE_CURLY) &&
	    ! isType (token, TOKEN_SEMICOLON))
	{
		/*
		 * Statements can be optionally terminated in the case of
		 * statement prior to a close curly brace as in the
		 * document.write line below:
		 *
		 * function checkForUpdate() {
		 *	   if( 1==1 ) {
		 *		   document.write("hello from checkForUpdate<br>")
		 *	   }
		 *	   return 1;
		 * }
		 */
		is_terminated = findCmdTerm (token, TRUE);
	}

cleanUp:
	vStringCopy(token->scope, saveScope);
	deleteToken (name);
	deleteToken (secondary_name);
	deleteToken (method_body_token);
	vStringDelete(saveScope);

	return is_terminated;
}

static void parseUI5 (tokenInfo *const token)
{
	tokenInfo *const name = newToken ();
	/*
	 * SAPUI5 is built on top of jQuery.
	 * It follows a standard format:
	 *     sap.ui.controller("id.of.controller", {
	 *         method_name : function... {
	 *         },
	 *
	 *         method_name : function ... {
	 *         }
	 *     }
	 *
	 * Handle the parsing of the initial controller (and the 
	 * same for "view") and then allow the methods to be
	 * parsed as usual.
	 */

	readToken (token);

	if (isType (token, TOKEN_PERIOD))
	{
		readToken (token);
		while (! isType (token, TOKEN_OPEN_PAREN) &&
			   ! isType (token, TOKEN_EOF))
		{
			readToken (token);
		}
		readToken (token);

		if (isType (token, TOKEN_STRING))
		{
			copyToken(name, token);
			readToken (token);
		}

		if (isType (token, TOKEN_COMMA))
			readToken (token);

		do
		{
			parseMethods (token, name);
		} while (! isType (token, TOKEN_CLOSE_CURLY) &&
				 ! isType (token, TOKEN_EOF));
	}

	deleteToken (name);
}

static boolean parseLine (tokenInfo *const token, tokenInfo *const parent, boolean is_inside_class)
{
	boolean is_terminated = TRUE;
	/*
	 * Detect the common statements, if, while, for, do, ...
	 * This is necessary since the last statement within a block "{}"
	 * can be optionally terminated.
	 *
	 * If the statement is not terminated, we need to tell
	 * the calling routine to prevent reading an additional token
	 * looking for the end of the statement.
	 */

	if (isType(token, TOKEN_KEYWORD))
	{
		switch (token->keyword)
		{
			case KEYWORD_for:
			case KEYWORD_while:
			case KEYWORD_do:
				is_terminated = parseLoop (token, parent);
				break;
			case KEYWORD_if:
			case KEYWORD_else:
			case KEYWORD_try:
			case KEYWORD_catch:
			case KEYWORD_finally:
				/* Common semantics */
				is_terminated = parseIf (token, parent);
				break;
			case KEYWORD_switch:
				parseSwitch (token);
				break;
			case KEYWORD_return:
				is_terminated = findCmdTerm (token, TRUE);
				break;
			default:
				is_terminated = parseStatement (token, parent, is_inside_class);
				break;
		}
	}
	else
	{
		/*
		 * Special case where single line statements may not be
		 * SEMICOLON terminated.  parseBlock needs to know this
		 * so that it does not read the next token.
		 */
		is_terminated = parseStatement (token, parent, is_inside_class);
	}
	return is_terminated;
}

static void parseJsFile (tokenInfo *const token)
{
	do
	{
		readToken (token);

		if (isType (token, TOKEN_KEYWORD) && token->keyword == KEYWORD_function)
			parseFunction (token);
		else if (isType (token, TOKEN_KEYWORD) && token->keyword == KEYWORD_sap)
			parseUI5 (token);
		else
			parseLine (token, token, FALSE);
	} while (! isType (token, TOKEN_EOF));
}

static void initialize (const langType language)
{
	Assert (sizeof (JsKinds) / sizeof (JsKinds [0]) == JSTAG_COUNT);
	Lang_js = language;
	buildJsKeywordHash ();
}

static void findJsTags (void)
{
	tokenInfo *const token = newToken ();

	ClassNames = stringListNew ();
	FunctionNames = stringListNew ();
	LastTokenType = TOKEN_UNDEFINED;

	parseJsFile (token);

	stringListDelete (ClassNames);
	stringListDelete (FunctionNames);
	ClassNames = NULL;
	FunctionNames = NULL;
	deleteToken (token);
}

/* Create parser definition structure */
extern parserDefinition* JavaScriptParser (void)
{
	static const char *const extensions [] = { "js", NULL };
	parserDefinition *const def = parserNew ("JavaScript");
	def->extensions = extensions;
	/*
	 * New definitions for parsing instead of regex
	 */
	def->kinds		= JsKinds;
	def->kindCount	= KIND_COUNT (JsKinds);
	def->parser		= findJsTags;
	def->initialize = initialize;

	return def;
}
/* vi:set tabstop=4 shiftwidth=4 noexpandtab: */
