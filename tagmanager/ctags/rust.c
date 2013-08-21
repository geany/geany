
/*
*   Copyright (c) 2010, Vincent Berthoux
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Objective C
*   language files.
*/
/*
*   INCLUDE FILES
*/
#include "general.h"	/* must always come first */

#include <string.h>

#include "keyword.h"
#include "entry.h"
#include "options.h"
#include "read.h"
#include "vstring.h"

/* To get rid of unused parameter warning in
 * -Wextra */
#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif


typedef enum {
	K_MOD,
	K_STRUCT,
	K_TRAIT,
	K_IMPL,
	K_FN,
	K_ENUM,
	K_TYPE,
	K_STATIC,
	K_MACRO,
	K_FIELD,
	K_VARIANT,
	K_METHOD
} rustKind;

static kindOption RustKinds[] = {
	{TRUE, 'm', "mod", "module"},
	{TRUE, 's', "struct", "structural type"},
	{TRUE, 't', "trait", "trait interface"},
	{TRUE, 'i', "impl", "implementation"},
	{TRUE, 'f', "fn", "Function"},
	{TRUE, 'e', "enum", "Enum "},
	{TRUE, 't', "type", "Type Alias"},
	{TRUE, 'g', "static", "Global variable"},
	{TRUE, 'M', "macro", "Macro Definition"},
	{TRUE, 'f', "field", "A struct field"},
	{TRUE, 'v', "variant", "An enum variant"},
	{TRUE, 'F', "method", "A method"},
};

typedef enum {
	RustMOD,
	RustSTRUCT,
	RustTRAIT,
	RustIMPL,
	RustFN,
	RustENUM,
	RustTYPEDECL,
	RustSTATIC,
	RustMACRO,
	RustFIELD,
	RustVARIANT,
	RustMETHOD,
	RustIDENTIFIER,
	RustTYPEPARAM,

	Tok_COMA,	/* ',' */
	Tok_PLUS,	/* '+' */
	Tok_MINUS,	/* '-' */
	Tok_PARL,	/* '(' */
	Tok_PARR,	/* ')' */
	Tok_CurlL,	/* '{' */
	Tok_CurlR,	/* '}' */
	Tok_SQUAREL,	/* '[' */
	Tok_SQUARER,	/* ']' */
	Tok_semi,	/* ';' */
	Tok_dpoint,	/* ':' */
	Tok_Sharp,	/* '#' */
	Tok_Backslash,	/* '\\' */
	Tok_EOL,	/* '\r''\n' */
	Tok_any,

	Tok_EOF	/* END of file */
} rustKeyword;

typedef rustKeyword rustToken;

typedef struct sRustKeywordDesc {
	const char *name;
	rustKeyword id;
} rustKeywordDesc;


static const rustKeywordDesc rustKeywordTable[] = {
	{"type", RustTYPEDECL},
	{"struct", RustSTRUCT},
	{"trait", RustTRAIT},
	{"impl", RustIMPL},
	{"enum", RustENUM},
	{"fn", RustFN},
	{"static", RustSTATIC},
	{"macro_rules!", RustMACRO},
};

static langType Lang_Rust;

/*//////////////////////////////////////////////////////////////////
//// lexingInit             */
typedef struct _lexingState {
	vString *name;	/* current parsed identifier/operator */
	const unsigned char *cp;	/* position in stream */
} lexingState;

static void initKeywordHash (void)
{
	const size_t count = sizeof (rustKeywordTable) / sizeof (rustKeywordDesc);
	size_t i;

	for (i = 0; i < count; ++i)
	{
		addKeyword (rustKeywordTable[i].name, Lang_Rust,
			(int) rustKeywordTable[i].id);
	}
}

/*//////////////////////////////////////////////////////////////////////
//// Lexing                                     */
static boolean isNum (char c)
{
	return c >= '0' && c <= '9';
}

static boolean isLowerAlpha (char c)
{
	return c >= 'a' && c <= 'z';
}

static boolean isUpperAlpha (char c)
{
	return c >= 'A' && c <= 'Z';
}

static boolean isAlpha (char c)
{
	return isLowerAlpha (c) || isUpperAlpha (c);
}

static boolean isIdent (char c)
{
	return isNum (c) || isAlpha (c) || c == '_';
}

static boolean isSpace (char c)
{
	return c == ' ' || c == '\t';
}

/* return true if it end with an end of line */
static void eatWhiteSpace (lexingState * st)
{
	const unsigned char *cp = st->cp;
	while (isSpace (*cp))
		cp++;

	st->cp = cp;
}

static void eatString (lexingState * st)
{
	boolean lastIsBackSlash = FALSE;
	boolean unfinished = TRUE;
	const unsigned char *c = st->cp + 1;

	while (unfinished)
	{
		/* end of line should never happen.
		 * we tolerate it */
		if (c == NULL || c[0] == '\0')
			break;
		else if (*c == '"' && !lastIsBackSlash)
			unfinished = FALSE;
		else
			lastIsBackSlash = *c == '\\';

		c++;
	}

	st->cp = c;
}

static void eatComment (lexingState * st)
{
	boolean unfinished = TRUE;
	boolean lastIsStar = FALSE;
	const unsigned char *c = st->cp + 2;

	while (unfinished)
	{
		/* we've reached the end of the line..
		 * so we have to reload a line... */
		if (c == NULL || *c == '\0')
		{
			st->cp = fileReadLine ();
			/* WOOPS... no more input...
			 * we return, next lexing read
			 * will be null and ok */
			if (st->cp == NULL)
				return;
			c = st->cp;
		}
		/* we've reached the end of the comment */
		else if (*c == '/' && lastIsStar)
			unfinished = FALSE;
		else
		{
			lastIsStar = '*' == *c;
			c++;
		}
	}

	st->cp = c;
}

static void readIdentifier (lexingState * st)
{
	const unsigned char *p;
	vStringClear (st->name);

	/* first char is a simple letter */
	if (isAlpha (*st->cp) || *st->cp == '_')
		vStringPut (st->name, (int) *st->cp);

	/* Go till you get identifier chars */
	for (p = st->cp + 1; isIdent (*p); p++)
		vStringPut (st->name, (int) *p);

	st->cp = p;

	vStringTerminate (st->name);
}

/* read the @something directives */
static void readIdentifierRustDirective (lexingState * st)
{
	const unsigned char *p;
	vStringClear (st->name);

	/* first char is a simple letter */
	if (*st->cp == '#')
		vStringPut (st->name, (int) *st->cp);

	/* Go till you get identifier chars */
	for (p = st->cp + 1; isIdent (*p); p++)
		vStringPut (st->name, (int) *p);

	st->cp = p;

	vStringTerminate (st->name);
}

/* The lexer is in charge of reading the file.
 * Some of sub-lexer (like eatComment) also read file.
 * lexing is finished when the lexer return Tok_EOF */
static rustKeyword lex (lexingState * st)
{
	int retType;

	/* handling data input here */
	while (st->cp == NULL || st->cp[0] == '\0')
	{
		st->cp = fileReadLine ();
		if (st->cp == NULL)
			return Tok_EOF;

		return Tok_EOL;
	}

	if (isAlpha (*st->cp))
	{
		readIdentifier (st);
		retType = lookupKeyword (vStringValue (st->name), Lang_Rust);

		if (retType == -1)	/* If it's not a keyword */
		{
			return RustIDENTIFIER;
		}
		else
		{
			return retType;
		}
	}
	else if (*st->cp == '#')
	{
		readIdentifierRustDirective (st);
		retType = lookupKeyword (vStringValue (st->name), Lang_Rust);

		if (retType == -1)	/* If it's not a keyword */
		{
			return Tok_any;
		}
		else
		{
			return retType;
		}
	}
	else if (isSpace (*st->cp))
	{
		eatWhiteSpace (st);
		return lex (st);
	}
	else
		switch (*st->cp)
		{
		case '(':
			st->cp++;
			return Tok_PARL;

		case '\\':
			st->cp++;
			return Tok_Backslash;

		case '#':
			st->cp++;
			return Tok_Sharp;

		case '/':
			if (st->cp[1] == '*')	/* ergl, a comment */
			{
				eatComment (st);
				return lex (st);
			}
			else if (st->cp[1] == '/')
			{
				st->cp = NULL;
				return lex (st);
			}
			else
			{
				st->cp++;
				return Tok_any;
			}
			break;

		case ')':
			st->cp++;
			return Tok_PARR;
		case '{':
			st->cp++;
			return Tok_CurlL;
		case '}':
			st->cp++;
			return Tok_CurlR;
		case '[':
			st->cp++;
			return Tok_SQUAREL;
		case ']':
			st->cp++;
			return Tok_SQUARER;
		case ',':
			st->cp++;
			return Tok_COMA;
		case ';':
			st->cp++;
			return Tok_semi;
		case ':':
			st->cp++;
			return Tok_dpoint;
		case '"':
			eatString (st);
			return Tok_any;
		case '+':
			st->cp++;
			return Tok_PLUS;
		case '-':
			st->cp++;
			return Tok_MINUS;

		default:
			st->cp++;
			break;
		}

	/* default return if nothing is recognized,
	 * shouldn't happen, but at least, it will
	 * be handled without destroying the parsing. */
	return Tok_any;
}

/*//////////////////////////////////////////////////////////////////////
//// Parsing                                    */
typedef void (*parseNext) (vString * const ident, rustToken what);

/********** Helpers */
/* This variable hold the 'parser' which is going to
 * handle the next token */
static parseNext toDoNext;

/* Special variable used by parser eater to
 * determine which action to put after their
 * job is finished. */
static parseNext comeAfter;

/* Used by some parsers detecting certain token
 * to revert to previous parser. */
static parseNext fallback;


/********** Grammar */
static void parseMod (vString * const ident, rustToken what);
static void parseImplMethods (vString * const ident, rustToken what);
static void parseImplMethods (vString * const ident, rustToken what);
static vString *tempName = NULL;
static vString *parentName = NULL;
static rustKind parentType = K_MOD;

/* used to prepare tag for OCaml, just in case their is a need to
 * add additional information to the tag. */
static void prepareTag (tagEntryInfo * tag, vString const *name, rustKind kind)
{
	initTagEntry (tag, vStringValue (name));
	tag->kindName = RustKinds[kind].name;
	tag->kind = RustKinds[kind].letter;

	if (parentName != NULL)
	{
		tag->extensionFields.scope[0] = RustKinds[parentType].name;
		tag->extensionFields.scope[1] = vStringValue (parentName);
	}
}

static void pushEnclosingContext (const vString * parent, rustKind type)
{
	vStringCopy (parentName, parent);
	parentType = type;
}

static void popEnclosingContext (void)
{
	vStringClear (parentName);
}

/* Used to centralise tag creation, and be able to add
 * more information to it in the future */
static void addTag (vString * const ident, int kind)
{
	tagEntryInfo toCreate;
	prepareTag (&toCreate, ident, kind);
	makeTagEntry (&toCreate);
}

static rustToken waitedToken, fallBackToken;

/* Ignore everything till waitedToken and jump to comeAfter.
 * If the "end" keyword is encountered break, doesn't remember
 * why though. */
static void tillToken (vString * const UNUSED (ident), rustToken what)
{
	if (what == waitedToken)
		toDoNext = comeAfter;
}

static void tillTokenOrFallBack (vString * const UNUSED (ident), rustToken what)
{
	if (what == waitedToken)
		toDoNext = comeAfter;
	else if (what == fallBackToken)
	{
		toDoNext = fallback;
	}
}

static int ignoreBalanced_count = 0;
static void ignoreBalanced (vString * const UNUSED (ident), rustToken what)
{

	switch (what)
	{
	case Tok_PARL:
	case Tok_CurlL:
	case Tok_SQUAREL:
		ignoreBalanced_count++;
		break;

	case Tok_PARR:
	case Tok_CurlR:
	case Tok_SQUARER:
		ignoreBalanced_count--;
		break;

	default:
		/* don't care */
		break;
	}

	if (ignoreBalanced_count == 0)
		toDoNext = comeAfter;
}

static void parseFields (vString * const ident, rustToken what)
{
	switch (what)
	{
	case Tok_CurlR:
		toDoNext = &parseImplMethods;
		break;

	case Tok_SQUAREL:
	case Tok_PARL:
		toDoNext = &ignoreBalanced;
		comeAfter = &parseFields;
		break;

		/* we got an identifier, keep track of it */
	case RustIDENTIFIER:
		vStringCopy (tempName, ident);
		break;

		/* our last kept identifier must be our variable name =) */
	case Tok_semi:
		addTag (tempName, K_FIELD);
		vStringClear (tempName);
		break;

	default:
		/* NOTHING */
		break;
	}
}

rustKind methodKind;


static vString *fullMethodName;
static vString *prevIdent;

static void parseMethodsName (vString * const ident, rustToken what)
{
	switch (what)
	{
	case Tok_PARL:
		toDoNext = &tillToken;
		comeAfter = &parseMethodsName;
		waitedToken = Tok_PARR;
		break;

	case Tok_dpoint:
		vStringCat (fullMethodName, prevIdent);
		vStringCatS (fullMethodName, ":");
		vStringClear (prevIdent);
		break;

	case RustIDENTIFIER:
		vStringCopy (prevIdent, ident);
		break;

	case Tok_CurlL:
	case Tok_semi:
		/* method name is not simple */
		if (vStringLength (fullMethodName) != '\0')
		{
			addTag (fullMethodName, methodKind);
			vStringClear (fullMethodName);
		}
		else
			addTag (prevIdent, methodKind);

		toDoNext = &parseImplMethods;
		parseImplMethods (ident, what);
		vStringClear (prevIdent);
		break;

	default:
		break;
	}
}

static void parseImplMethodsName (vString * const ident, rustToken what)
{
	switch (what)
	{
	case Tok_PARL:
		toDoNext = &tillToken;
		comeAfter = &parseImplMethodsName;
		waitedToken = Tok_PARR;
		break;

	case Tok_dpoint:
		vStringCat (fullMethodName, prevIdent);
		vStringCatS (fullMethodName, ":");
		vStringClear (prevIdent);
		break;

	case RustIDENTIFIER:
		vStringCopy (prevIdent, ident);
		break;

	case Tok_CurlL:
	case Tok_semi:
		/* method name is not simple */
		if (vStringLength (fullMethodName) != '\0')
		{
			addTag (fullMethodName, methodKind);
			vStringClear (fullMethodName);
		}
		else
			addTag (prevIdent, methodKind);

		toDoNext = &parseImplMethods;
		parseImplMethods (ident, what);
		vStringClear (prevIdent);
		break;

	default:
		break;
	}
}

static void parseImpl (vString * const ident, rustToken what)
{
	switch (what)
	{
	case RustFN:	/* + */
		toDoNext = &parseImplMethodsName;
		methodKind = K_METHOD;
		break;


	case Tok_CurlL:	/* { */
		toDoNext = &ignoreBalanced;
		ignoreBalanced (ident, what);
		comeAfter = &parseImplMethodsName;
		break;

	default:
		break;
	}
}

static void parseBlock (vString * const ident, rustToken what)
{
}

static void parseFunction (vString * const ident, rustToken what)
{
 	addTag (ident, K_FN);
	switch (what)
	{
	case RustFN:	// + 
		toDoNext = &parseImplMethodsName;
		methodKind = K_METHOD;
		break;


	case Tok_CurlL:	/* { */
		toDoNext = &ignoreBalanced;
		ignoreBalanced (ident, what);
		comeAfter = &parseImplMethods;
		break;

	default:
		break;
	}
}


static void parseImplMethods (vString * const UNUSED (ident), rustToken what)
{
	switch (what)
	{
	case RustFN:	/* + */
		toDoNext = &parseImplMethodsName;
		methodKind = K_METHOD;
		break;


	case Tok_CurlL:	/* { */
		toDoNext = &parseFields;
		break;

	default:
		break;
	}
}




// trait and impl bodies are the same.
static void parseTrait (vString * const ident, rustToken what)
{
 	addTag (ident, K_TRAIT);

	switch (what) { 
	case RustFN:	/* + */
		toDoNext = &parseImplMethodsName;
		methodKind = K_METHOD;
		break;

	case RustIDENTIFIER:
		addTag (ident, K_METHOD);
		pushEnclosingContext (ident, K_METHOD);
	break;
	}

	toDoNext = &parseImplMethods;
}

static void parseStructMembers (vString * const ident, rustToken what)
{
	static parseNext prev = NULL;

	if (prev != NULL)
	{
		comeAfter = prev;
		prev = NULL;
	}

	switch (what)
	{
	case RustIDENTIFIER:
		vStringCopy (tempName, ident);//expect :type next,?
		addTag (tempName, K_FIELD);
		break;

	case Tok_COMA:	/* ',' */
	case Tok_semi:	/* ';' */
//		addTag (tempName, K_FIELD);
// type of 
		vStringClear (tempName);
		break;

		/* some types are complex, the only one
		 * we will loose is the function type.
		 */
	case Tok_CurlL:	/* '{' */
	case Tok_PARL:	/* '(' */
	case Tok_SQUAREL:	/* '[' */
		toDoNext = &ignoreBalanced;
		prev = comeAfter;
		comeAfter = &parseStructMembers;
		ignoreBalanced (ident, what);
		break;

	case Tok_CurlR:
		toDoNext = comeAfter;
		break;

	default:
		/* don't care */
		break;
	}
}

/* Called just after the struct keyword */
static boolean parseStruct_gotName = FALSE;
static void parseStruct (vString * const ident, rustToken what)
{
	switch (what)
	{
	case RustIDENTIFIER:
		if (!parseStruct_gotName)
		{
			addTag (ident, K_STRUCT);
			pushEnclosingContext (ident, K_STRUCT);
			parseStruct_gotName = TRUE;
		}
		else
		{
			parseStruct_gotName = FALSE;
			popEnclosingContext ();
			toDoNext = comeAfter;
			comeAfter (ident, what);
		}
		break;

	case Tok_CurlL:
		toDoNext = &parseStructMembers;
		break;

		/* maybe it was just a forward declaration
		 * in which case, we pop the context */
	case Tok_semi:
		if (parseStruct_gotName)
			popEnclosingContext ();

		toDoNext = comeAfter;
		comeAfter (ident, what);
		break;

	default:
		/* we don't care */
		break;
	}
}

/* Parse enumeration members, ignoring potential initialization */
static parseNext parseEnumVariants_prev = NULL;
static void parseEnumVariants (vString * const ident, rustToken what)
{
	if (parseEnumVariants_prev != NULL)
	{
		comeAfter = parseEnumVariants_prev;
		parseEnumVariants_prev = NULL;
	}

	switch (what)
	{
	case RustIDENTIFIER:
		addTag (ident, K_ENUM);
		parseEnumVariants_prev = comeAfter;
		waitedToken = Tok_COMA;
		/* last item might not have a coma */
		fallBackToken = Tok_CurlR;
		fallback = comeAfter;
		comeAfter = parseEnumVariants;
		toDoNext = &tillTokenOrFallBack;
		break;

	case Tok_CurlR:
		toDoNext = comeAfter;
		popEnclosingContext ();
		break;

	default:
		/* don't care */
		break;
	}
}

/* parse enum ... { ... */
static boolean parseEnum_named = FALSE;
static void parseEnum (vString * const ident, rustToken what)
{
	switch (what)
	{
	case RustIDENTIFIER:
		if (!parseEnum_named)
		{
			addTag (ident, K_ENUM);
			pushEnclosingContext (ident, K_ENUM);
			parseEnum_named = TRUE;
		}
		else
		{
			parseEnum_named = FALSE;
			popEnclosingContext ();
			toDoNext = comeAfter;
			comeAfter (ident, what);
		}
		break;

	case Tok_CurlL:	/* '{' */
		toDoNext = &parseEnumVariants;
		parseEnum_named = FALSE;
		break;

	case Tok_semi:	/* ';' */
		if (parseEnum_named)
			popEnclosingContext ();
		toDoNext = comeAfter;
		comeAfter (ident, what);
		break;

	default:
		/* don't care */
		break;
	}
}

/* Parse something like
 * type ident =;
 * ignoring the defined type but in the case of struct,
 * in which case struct are parsed.
 */
static void parseTypedecl (vString * const ident, rustToken what)
{
	switch (what)
	{
	case RustSTRUCT:
		toDoNext = &parseStruct;
		comeAfter = &parseTypedecl;
		break;

	case RustENUM:
		toDoNext = &parseEnum;
		comeAfter = &parseTypedecl;
		break;

	case RustIDENTIFIER:
		vStringCopy (tempName, ident);
		break;

	case Tok_semi:	/* ';' */
		addTag (tempName, K_TYPE);
		vStringClear (tempName);
		toDoNext = &parseMod;
		break;

	default:
		/* we don't care */
		break;
	}
}

static boolean ignorePreprocStuff_escaped = FALSE;
static void ignorePreprocStuff (vString * const UNUSED (ident), rustToken what)
{
	switch (what)
	{
	case Tok_Backslash:
		ignorePreprocStuff_escaped = TRUE;
		break;

	case Tok_EOL:
		if (ignorePreprocStuff_escaped)
		{
			ignorePreprocStuff_escaped = FALSE;
		}
		else
		{
			toDoNext = &parseMod;
		}
		break;

	default:
		ignorePreprocStuff_escaped = FALSE;
		break;
	}
}

static void parseMacroRules (vString * const ident, rustToken what)
{
//	if (what == ObjcIDENTIFIER)
//		addTag (ident, K_MACRO);

	toDoNext = &ignorePreprocStuff;
}

static void parsePreproc (vString * const ident, rustToken what)
{
	switch (what)
	{
/*	case RustIDENTIFIER:
		if (strcmp (vStringValue (ident), "define") == 0)
			toDoNext = &parseMacroRules;
		else
			toDoNext = &ignorePreprocStuff;
		break;
*/
	default:
		toDoNext = &ignorePreprocStuff;
		break;
	}
}

/* Handle the "strong" top levels, all 'big' declarations
 * happen here */
static void parseMod (vString * const ident, rustToken what)
{
	switch (what)
	{
	case Tok_Sharp:
		toDoNext = &parsePreproc;
		break;

	case RustSTRUCT:
		toDoNext = &parseStruct;
		comeAfter = &parseMod;
		break;

	case RustMOD:		//same as global scope?!
		toDoNext = &parseMod;
		comeAfter = &parseMod;
		break;

	case RustIDENTIFIER:
		/* we keep track of the identifier if we
		 * come across a function. */
		vStringCopy (tempName, ident);
		break;

	case Tok_PARL:
		/* if we find an opening parenthesis it means we
		 * found a function (or a macro...) */
		addTag (tempName, K_FN);
		vStringClear (tempName);
		comeAfter = &parseMod;
		toDoNext = &ignoreBalanced;
		ignoreBalanced (ident, what);
		break;

	case RustTRAIT:
		toDoNext = &parseTrait;
		break;

	case RustIMPL:
		toDoNext = &parseImpl;
		break;
	case RustFN:
		toDoNext = &parseFunction;
		break;

	case RustTYPEDECL:
		toDoNext = parseTypedecl;
		comeAfter = &parseMod;
		break;

	case Tok_CurlL:
		comeAfter = &parseMod;
		toDoNext = &ignoreBalanced;
		ignoreBalanced (ident, what);
		break;


	default:
		/* we don't care */
		break;
	}
}

/*////////////////////////////////////////////////////////////////
//// Deal with the system                                       */

static void findRustTags (void)
{
	vString *name = vStringNew ();
	lexingState st;
	rustToken tok;

	parentName = vStringNew ();
	tempName = vStringNew ();
	fullMethodName = vStringNew ();
	prevIdent = vStringNew ();

	/* (Re-)initialize state variables, this might be a second file */
	comeAfter = NULL;
	fallback = NULL;
	parentType = K_TRAIT;
	ignoreBalanced_count = 0;
	methodKind = 0;
	parseStruct_gotName = FALSE;
	parseEnumVariants_prev = NULL;
	parseEnum_named = FALSE;
	ignorePreprocStuff_escaped = FALSE;

	st.name = vStringNew ();
	st.cp = fileReadLine ();
	toDoNext = &parseMod;
	tok = lex (&st);
	while (tok != Tok_EOF)
	{
		(*toDoNext) (st.name, tok);
		tok = lex (&st);
	}

	vStringDelete (name);
	vStringDelete (parentName);
	vStringDelete (tempName);
	vStringDelete (fullMethodName);
	vStringDelete (prevIdent);
	parentName = NULL;
	tempName = NULL;
	prevIdent = NULL;
	fullMethodName = NULL;
}

static void rustInitialize (const langType language)
{
	Lang_Rust = language;

	initKeywordHash ();
}

extern parserDefinition *RustParser (void)
{
	printf("rust parser\n");
	static const char *const extensions[] = { "rs", NULL };
	parserDefinition *def = parserNew ("Rust");
	def->kinds = RustKinds;
	def->kindCount = KIND_COUNT (RustKinds);
	def->extensions = extensions;
	def->parser = findRustTags;
	def->initialize = rustInitialize;

	return def;
}
