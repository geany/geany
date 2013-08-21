
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
#include "main.h"

#include <string.h>

#include "keyword.h"
#include "parse.h"
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
} RustKind;

typedef enum  {
	PARSE_EXIT,
	PARSE_NEXT,
	PARSE_RECURSE,
	PARSE_IGNORE_BALANCED,
	PARSE_IGNORE_TYPE_PARAMS,
	PARSE_IGNORE_BALANCED_EXIT
}
RustParserAction;

static kindOption RustKinds[] = {
	{TRUE, 'm', "namespace", "module"},
	{TRUE, 's', "struct", "structural type"},
	{TRUE, 't', "interface", "trait interface"},
	{TRUE, 'i', "implementation", "implementation"},
	{TRUE, 'f', "function", "Function"},
	{TRUE, 'e', "union", "Enum "},
	{TRUE, 't', "typedef", "Type Alias"},
	{TRUE, 'g', "variable", "Global variable"},
	{TRUE, 'M', "macro", "Macro Definition"},
	{TRUE, 'f', "field", "A struct field"},
	{TRUE, 'v', "enum", "An enum variant"},
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
	RustPUB,
	RustPRIV,
	RustUNSAFE,
	RustEXTERN,
	RustUSE,

	Tok_COMA,	/* ',' */
	Tok_PLUS,	/* '+' */
	Tok_MINUS,	/* '-' */
	Tok_PARL,	/* '(' */
	Tok_PARR,	/* ')' */
	Tok_LT,	/* '<' */
	Tok_GT,	/* '>' */
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
} RustKeyword;

typedef RustKeyword RustToken;

typedef struct sRustKeywordDesc {
	const char *name;
	RustKeyword id;
} RustKeywordDesc;


static const RustKeywordDesc RustKeywordTable[] = {
	{"type", RustTYPEDECL},
	{"struct", RustSTRUCT},
	{"trait", RustTRAIT},
	{"impl", RustIMPL},
	{"enum", RustENUM},
	{"fn", RustFN},
	{"static", RustSTATIC},
	{"macro_rules!", RustMACRO},
	{"pub", RustPUB},
	{"priv", RustPRIV},
	{"unsafe", RustUNSAFE},
	{"extern", RustEXTERN},
	{"use", RustUSE},
	{"mod", RustMOD},
};

static langType Lang_Rust;

/*//////////////////////////////////////////////////////////////////
//// lexingInit             */
typedef struct _LexingState {
	vString *name;	/* current parsed identifier/operator */
	const unsigned char *cp;	/* position in stream */
} LexingState;

static void initKeywordHash (void)
{
	const size_t count = sizeof (RustKeywordTable) / sizeof (RustKeywordDesc);
	size_t i;

	for (i = 0; i < count; ++i)
	{
		printf("added keyword %s\n", RustKeywordTable[i]);
		addKeyword (RustKeywordTable[i].name, Lang_Rust,
			(int) RustKeywordTable[i].id);
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
static void eatWhiteSpace (LexingState * st)
{
	const unsigned char *cp = st->cp;
	while (isSpace (*cp))
		cp++;

	st->cp = cp;
}

static void eatString (LexingState * st)
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

static void eatComment (LexingState * st)
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

static void readIdentifier (LexingState * st)
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
static void readIdentifierRustDirective (LexingState * st)
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
/*
static PARSER_ACTION parsePreproc (vString * const ident, RustToken what, const RustParseContext* parent, RustParseContext* ctx) {
	switch (what) {
		
	}
}
*/

/* The lexer is in charge of reading the file.
 * Some of sub-lexer (like eatComment) also read file.
 * lexing is finished when the lexer return Tok_EOF */
static RustKeyword lex(LexingState * st)
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
		case '<':
			st->cp++;
			return Tok_LT;
		case '>':
			st->cp++;
			return Tok_GT;

		default:
			st->cp++;
			break;
		}

	/* default return if nothing is recognized,
	 * shouldn't happen, but at least, it will
	 * be handled without destroying the parsing. */
	return Tok_any;
}


/********** Grammar */

struct RustParserContext;

typedef RustParserAction (*fParse)( RustToken what,vString* ident, struct RustParserContext* ctx);

typedef struct RustParserContext {
	vString*	name;
	RustKind	kind;
	fParse		parser;
	int	main_ident_set;
	struct RustParserContext* parent;
} RustParserContext;



static void printTagEntry(const tagEntryInfo *tag)
{
	fprintf(stderr, "Tag: %s (%s) [ impl: %s, scope: %s, type: %s\n", tag->name,
	tag->kindName, tag->extensionFields.implementation, tag->extensionFields.scope[1],
	tag->extensionFields.varType);
}

/* used to prepare tag for OCaml, just in case their is a need to
 * add additional information to the tag. */
static void prepareTag (tagEntryInfo * tag, vString const *name, RustKind kind)
{



//	if (parentName != NULL)
//	{
//		tag->extensionFields.scope[0] = RustKinds[parentType].name;
//		tag->extensionFields.scope[1] = vStringValue (parentName);
//	}
}

/* Used to centralise tag creation, and be able to add
 * more information to it in the future */
static void addTag (vString * const ident, int kind, const RustParserContext* ctx)
{
	tagEntryInfo tag;
	initTagEntry (&tag, vStringValue (ident));


	tag.lineNumber=getSourceLineNumber();
	tag.filePosition	= getInputFilePosition();
	tag.sourceFileName=getSourceFileName();

	tag.kindName=RustKinds[kind].name;
	tag.kind = RustKinds[kind].letter;

	//printTagEntry(&tag);
	makeTagEntry (&tag);
}

static void ignoreBalanced (struct LexingState* st)
{
	int ignoreBalanced_count = 1;
	while (ignoreBalanced_count>0) {
		RustKind tok=lex(st);
		switch (tok) {
		case Tok_PARL:
		case Tok_CurlL:
		case Tok_SQUAREL:
			ignoreBalanced_count++;
			continue;
		case Tok_EOF:
		case Tok_PARR:
		case Tok_CurlR:
		case Tok_SQUARER:
			ignoreBalanced_count--;
			continue;
		}
	}
}

static void ignoreTypeParams (struct LexingState* st)
{
	printf("ignore type params\n");
	int ignoreBalanced_count = 1;
	while (ignoreBalanced_count>0) {
		RustKind tok=lex(st);
		switch (tok) {
		case Tok_LT:
			ignoreBalanced_count++;
			continue;
		case Tok_GT:
			ignoreBalanced_count--;
			continue;
		}
	}
}

static RustParserAction parseStruct ( RustToken what,vString*  ident,  RustParserContext* ctx)
{
	printf("parse struct: %s",vStringValue(ident));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_FirstIdent (ident, K_STRUCT,ctx);
		return PARSE_NEXT;
	case Tok_PARL:	
	case Tok_CurlL:	
		return PARSE_IGNORE_BALANCED_EXIT;
		break;
	}
	return PARSE_EXIT;
}
static RustParserAction parseEnum ( RustToken what,vString*  ident,  RustParserContext* ctx)
{
	printf("parse struct: %s",ident);
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_FirstIdent (ident, K_STRUCT,ctx);
		return PARSE_NEXT;
	case Tok_LT:	
		return PARSE_IGNORE_TYPE_PARAMS;

	case Tok_PARL:	
	case Tok_CurlL:	
		return PARSE_IGNORE_BALANCED_EXIT;
		break;
	}
	return PARSE_EXIT;
}

static RustParserAction parseFn ( RustToken what,vString*  ident, RustParserContext* ctx)
{
	printf("parse fn: %s",vStringValue(ident));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_FirstIdent (ident, K_FN,ctx);
		return PARSE_NEXT;
	case Tok_LT:	
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_PARL:	// param block
		return PARSE_IGNORE_BALANCED;
	case Tok_CurlL:	// fn body, then quit.
		return PARSE_IGNORE_BALANCED_EXIT;
		break;
	case Tok_semi:	// fn body, then quit.
		return PARSE_EXIT;
	}
	return PARSE_NEXT;
}

static RustParserAction parseMethods ( RustToken what,vString*  ident, RustParserContext* ctx)
{
	switch (what) {
	case RustFN:
		ctx->parser=parseFn;
		return PARSE_RECURSE;
	case Tok_CurlR:
		return PARSE_EXIT;
	}
	return PARSE_NEXT;
} 

static RustParserAction parseTrait ( RustToken what,vString*  ident, RustParserContext* ctx)
{
	printf("parse trait: %s",vStringValue(ident));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_FirstIdent (ident, K_TRAIT,ctx);
		return PARSE_NEXT;
	case Tok_CurlL:	
		ctx->parser=parseMethods;
		return PARSE_RECURSE;
		break;
	case Tok_PARL:	
		return PARSE_IGNORE_BALANCED_EXIT;
		break;
	case Tok_LT:	
		return PARSE_IGNORE_TYPE_PARAMS;
	}
	return PARSE_EXIT;
}

void addTag_FirstIdent(vString* ident, RustKind kind, RustParserContext* ctx) {
	if (!ctx->main_ident_set) {
		printf("%d\n",ctx->main_ident_set);
		addTag(ident,kind,ctx);
		ctx->main_ident_set++;
	}
}

static RustParserAction parseImpl ( RustToken what,vString*  ident,  RustParserContext* ctx)
{
	printf("parse impl: %s",vStringValue(ident));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_FirstIdent (ident, K_IMPL,ctx);
		return PARSE_NEXT;
	case Tok_CurlL:	
		ctx->parser=parseMethods;
		return PARSE_RECURSE;
	case Tok_LT:	
		return PARSE_IGNORE_TYPE_PARAMS;		
	case Tok_PARL:	
		return PARSE_IGNORE_BALANCED_EXIT;
		break;
	}
	return PARSE_EXIT;
}



/* Handle the "strong" top levels, all 'big' declarations
 * happen here */
static RustParserAction parseModBody (RustToken what,vString*  ident,   RustParserContext* ctx);

static RustParserAction parseModHeader (RustToken what,vString*  ident,  RustParserContext* ctx) {
	printf("PARSE MDODE HAEADER");
	switch (what) {
	case RustIDENTIFIER:
		printf("adding mod tag %s\n",vStringValue(ident));
		addTag_FirstIdent (ident, K_MOD, ctx);
		return PARSE_NEXT;

	case Tok_CurlL:
		ctx->parser=parseModBody;
		return PARSE_RECURSE;
	}
	return PARSE_NEXT;
}
static RustParserAction parseModBody (RustToken what,vString*  ident,   RustParserContext* ctx)
{
	printf("(parse mod body: %d)",what);
	switch (what)
	{

	case RustFN:
		ctx->parser=parseFn;
		return PARSE_RECURSE;

	case RustMOD:		//same as global scope?!
		printf("*********8MOD*******8\n");
		ctx->parser=parseModHeader;
		return PARSE_RECURSE;

	case RustSTRUCT:
		ctx->parser=parseStruct;
		return PARSE_RECURSE;

	case RustTRAIT:
		ctx->parser=parseTrait;
		return PARSE_RECURSE;

	case RustIMPL:
		ctx->parser=parseImpl;
		return PARSE_RECURSE;

	case RustENUM:
		ctx->parser=parseEnum;
		return PARSE_RECURSE;

/*
	// rust module decls are always (fn|struct|trait|impl|mod) <ident> ...;
	case RustIDENTIFIER:
		// we keep track of the identifier if we
		// come across a function. 
		vStringCopy (tempName, ident);
		break;
*/

/*
	case Tok_PARL:
		// if we find an opening parenthesis it means we
		// found a function (or a macro...) 
//		addTag (tempName, K_FN);
		vStringClear (tempName);
		comeAfter = &parseMod;
		toDoNext = &ignoreBalanced;
		ignoreBalanced (ident, what);
		break;

	case RustFN:
		toDoNext = &parseFunction;
		comeAfter = &parseMod;
		break;


	case RustTRAIT:
		toDoNext = &parseTrait;
		break;
	case RustENUM:
		toDoNext = &parseEnum;
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
*/
	case Tok_PARL:	
	case Tok_CurlL:	
		return PARSE_IGNORE_BALANCED;
	
	default:
		/* we don't care */
		return PARSE_NEXT;
	}
}

/// Parser Main Loop
void parseRecursive(LexingState* st, const RustParserContext* parentContext) {
	RustParserContext ctx;
	ctx.name=vStringNew();
	ctx.kind=parentContext->kind;
	ctx.parser=NULL;
	ctx.main_ident_set=0;
	ctx.parent=parentContext;

	while (1){
		RustToken tok=lex(st);
		printf("(%d %s)\n",tok,vStringValue(st->name));
		if (tok==Tok_EOF)
			break;
		
		int action=parentContext->parser(tok,st->name,  &ctx);
		//printf("action:\n",action);
		if (action==PARSE_RECURSE) {
			parseRecursive(st, &ctx);
		} else if (action==PARSE_EXIT) {
			break;
		} else if (action==PARSE_IGNORE_BALANCED) {
			printf("ignore balanced\n");
			ignoreBalanced(st);
		} else if (action==PARSE_IGNORE_TYPE_PARAMS) {
			printf("ignore TypeParams\n");
			ignoreTypeParams(st);
		}
		 else if (action==PARSE_IGNORE_BALANCED_EXIT) {
			printf("ignore balanced\n");
			ignoreBalanced(st);
			break;
		}
	}
	vStringDelete(ctx.name);
}



static void findRustTags (void)
{
	LexingState st;
	st.name = vStringNew();
	RustToken tok;
	RustParserContext ctx;
	ctx.name=vStringNew();
	ctx.kind=K_MOD;	// root is a module.
	ctx.parser= parseModBody;
	ctx.main_ident_set=0;
	ctx.parent=0;

	parseRecursive(&st, &ctx);
	vStringDelete(ctx.name);
	vStringDelete(st.name);
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
