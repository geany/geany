
/*
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*   This module contains functions for generating tags for Rust files.
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

#define DEBUG 1 

#if defined(DEBUG) && DEBUG
#	define dbprintf(...) printf(__VA_ARGS__)
#else /* !DEBUG */
#	define dbprintf(...) /* nothing */
#endif


typedef enum {
	K_NONE,
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

#define PARSE_EXIT_SHIFT 16
#define PARSE_EXIT_MASK (0xff<<(PARSE_EXIT_SHIFT))
#define PARSE_GET_EXIT_COUNT(PARSE_ACTION) ((PARSE_ACTION)>>PARSE_EXIT_SHIFT)
#define PARSE_EXIT_CODE(ACTION,RET_LEVELS) ((ACTION)|((RET_LEVELS)<<PARSE_EXIT_SHIFT))
typedef enum  {
	PARSE_NEXT,
	PARSE_RECURSE,
	PARSE_IGNORE_BALANCED,
	PARSE_IGNORE_TYPE_PARAMS,
	PARSE_EXIT=PARSE_EXIT_CODE(0,1),
	PARSE_EXIT2=PARSE_EXIT_CODE(0,2)
}
RustParserAction;

static kindOption RustKinds[] = {
	{TRUE, 'm', "namespace", "module"},
	{TRUE, 'n', "namespace", "module"},
	{TRUE, 's', "struct", "structural type"},
	{TRUE, 'i', "interface", "trait interface"},
	{TRUE, 'c', "implementation", "implementation"},
	{TRUE, 'f', "function", "Function"},
	{TRUE, 'e', "enumeration", "Enum "}, // TODO tagged 'u' union, or enum ('e')?
	{TRUE, 't', "typedef", "Type Alias"},
	{TRUE, 'v', "variable", "Global variable"},
	{TRUE, 'M', "macro", "Macro Definition"},
	{TRUE, 'm', "field", "A struct field"},
	{TRUE, 'g', "enum", "An enum variant"},
	{TRUE, 'F', "method", "A method"},
};

typedef enum {
	Tok_COMMA,	/* ',' */
	Tok_PLUS,	/* '+' */
	Tok_MINUS,	/* '-' */
	Tok_MUL,
	Tok_AMP,
	Tok_OR,
	Tok_DOLLAR,
	Tok_MOD,
	Tok_XOR,
	Tok_QU,
	Tok_AT,
	Tok_TILDRE,
	Tok_DIV,
	Tok_PARL,	/* '(' */
	Tok_PARR,	/* ')' */
	Tok_LT,	/* '<' */
	Tok_GT,	/* '>' */
	Tok_CurlL,	/* '{' */
	Tok_CurlR,	/* '}' */
	Tok_SQUAREL,	/* '[' */
	Tok_SQUARER,	/* ']' */
	Tok_SEMI,	/* ';' */
	Tok_COLON,	/* ':' */
	Tok_Sharp,	/* '#' */
	Tok_Backslash,	/* '\\' */
	Tok_EOL,	/* '\r''\n' */
	Tok_any,

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
	RustLET,
	RustUSE,
	RustFOR,
	RustIN,
	RustAS,
	RustMATCH,

	Tok_EOF	/* END of file */
} RustKeyword;

const char* rustTokStr[]={
	",","+","-","*","&","|","$","%","^","?","@","~","/","(",")","<",">","{","}","[","]",";",":","#","\\","\n","any",
	"mod","struct","trait","impl","fn","enum","type","static","macro_rules!",
	"","","","","","pub","priv","unsafe","extern","let","use","for","in","as","match"
};
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
	{"mod", RustMOD},
	{"use", RustUSE},
	{"for", RustFOR},
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

/* The lexer is in charge of reading the file.
 * Some of sub-lexer (like eatComment) also read file.
 * lexing is finished when the lexer return Tok_EOF */
static RustKeyword lex(LexingState * st)
{
	int retType;

	/* handling data input here */
	if (st->cp == NULL || st->cp[0] == '\0')
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
				return Tok_DIV;
			}
			break;
		case '*':
			st->cp++;
			return Tok_MUL;
		case '&':
			st->cp++;
			return Tok_AMP;
		case '@':
			st->cp++;
			return Tok_AT;
		case '~':
			st->cp++;
			return Tok_TILDRE;
		case '|':
			st->cp++;
			return Tok_OR;
		case '$':
			st->cp++;
			return Tok_DOLLAR;
		case '?':
			st->cp++;
			return Tok_QU;

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
			return Tok_COMMA;
		case ';':
			st->cp++;
			return Tok_SEMI;
		case ':':
			st->cp++;
			return Tok_COLON;
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

typedef RustParserAction (*fParse)( RustToken what,LexingState* st, struct RustParserContext* ctx);

typedef struct RustParserContext {
	vString*	name;
	RustKind	kind;
	fParse		parser;
	int	main_ident_set;
	struct RustParserContext* parent;
} RustParserContext;



/* Used to centralise tag creation, and be able to add
 * more information to it in the future */
static void addTag (vString * const ident, const char* arglist, int kind, const RustParserContext* ctx, const RustParserContext* parent)
{
	tagEntryInfo tag;
	initTagEntry (&tag, vStringValue (ident));


	tag.lineNumber=getSourceLineNumber();
	tag.filePosition	= getInputFilePosition();
	tag.sourceFileName=getSourceFileName();

	tag.kindName=RustKinds[kind].name;
	tag.kind = RustKinds[kind].letter;

	if (parent) {
		dbprintf("%p nested in %p %d %s %s\n",
		         ctx, parent, parent->kind, RustKinds[parent->kind].name,
		         vStringValue(parent->name));
		tag.extensionFields.scope[0]=RustKinds[parent->kind].name;
		tag.extensionFields.scope[1]=vStringValue(parent->name);
	}
	tag.extensionFields.arglist=arglist;	// takes ownership
	makeTagEntry (&tag);
}

static RustParserContext* ctxParentParent(RustParserContext* ctx) {
	if (ctx->parent)
		return ctx->parent->parent;
	else return NULL;
}

// stores the 'main identifier' for a block eg function name, struct name..
static void rpc_set_main_ident(RustParserContext* ctx,vString* ident,RustKind kind) {
	if (!ctx->main_ident_set) {
		ctx->main_ident_set++;
		dbprintf("%d\n",ctx->main_ident_set);
		ctx->kind=kind;
		if (ctx->name) {
			vStringDelete(ctx->name);
		}
		ctx->name=vStringNewCopy(ident);
		dbprintf("ctx set main ident %p %d %s\n", ctx, ctx->kind, vStringValue(ctx->name));
	}
}

static void addTag_MainIdent(vString* ident, const char* arglist, RustKind kind, RustParserContext* ctx) {
	if (!ctx->main_ident_set) {
		ctx->main_ident_set++;
		dbprintf("%d\n",ctx->main_ident_set);
		
		addTag(ident,arglist, kind,ctx, ctxParentParent(ctx));
		ctx->kind=kind;
		if (ctx->name) {
			vStringDelete(ctx->name);
		}
		ctx->name=vStringNewCopy(ident);
		dbprintf("tag set main ident %p %d %s\n", ctx, ctx->kind, vStringValue(ctx->name));
	}
}

static void ignoreBalanced (LexingState* st)
{
	int ignoreBalanced_count = 1;
	while (ignoreBalanced_count>0) {
		RustToken tok=lex(st);
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
		default:
			break;
		}
	}
}

static void str_realloc_cat(char** ppStr, const char* src) {
	*ppStr=realloc(*ppStr,strlen(*ppStr)+strlen(src)+1);
	strcat(*ppStr, src);
};

static const char* lex_strdup_balanced(LexingState* st,const char* open) {
	char* str=strdup(open);
	
	int ignoreBalanced_count = 1;
	while (ignoreBalanced_count>0) {
		RustToken tok=lex(st);
		switch (tok) {
		case Tok_PARL:
			str_realloc_cat(&str,"(");
			ignoreBalanced_count++;
			continue;
		case Tok_CurlL:
			str_realloc_cat(&str,"{");
			ignoreBalanced_count++;
			continue;
		case Tok_SQUAREL:
			str_realloc_cat(&str,"[");
			ignoreBalanced_count++;
			continue;
		case Tok_EOF:
			ignoreBalanced_count--;
			continue;
		case Tok_PARR:
			str_realloc_cat(&str,")");
			ignoreBalanced_count--;
			continue;
		case Tok_CurlR:
			str_realloc_cat(&str,"}");
			ignoreBalanced_count--;
			continue;
		case Tok_SQUARER:
			str_realloc_cat(&str,"]");
			ignoreBalanced_count--;
			continue;
		case Tok_any:
		case RustIDENTIFIER:
			str_realloc_cat(&str,vStringValue(st->name));
			break;
		default:
			str_realloc_cat(&str,rustTokStr[tok]);
			str_realloc_cat(&str," ");
			break;
		}
	}
	return str;
}


static void ignoreTypeParams (LexingState* st)
{
	int ignoreBalanced_count = 1;
	dbprintf("ignore type params\n");
	while (ignoreBalanced_count>0) {
		RustToken tok=lex(st);
		switch (tok) {
		case Tok_LT:
			ignoreBalanced_count++;
			continue;
		case Tok_GT:
			ignoreBalanced_count--;
			continue;
		default:
			break;
		}
	}
}

// type declaration :....,;)}
static RustParserAction parseSkipTypeDecl(RustToken what, LexingState* st, RustParserContext* ctx)
{
	switch (what) {
	case Tok_SEMI:
	case Tok_COMMA:
		return PARSE_EXIT;
	case Tok_GT:
	case Tok_PARR:
	case Tok_CurlR:	// TODO: THIS MUST PUSH IT BACK , or have peek instead of consume?
		return PARSE_EXIT2;	 // exit2 means parent gets a } yuk this is a horrible workaround
	case Tok_PARL:
		return PARSE_IGNORE_BALANCED;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	default: return PARSE_NEXT;
	}
	return PARSE_NEXT;
}

static RustParserAction parseStructFields(RustToken what, LexingState* st, RustParserContext* ctx)
{
	dbprintf("parse struct fields\n");
	switch (what) {
	case RustIDENTIFIER:	//only want the rirst .. until ?!
		addTag(st->name, NULL, K_FIELD, ctx,ctx->parent);
	return PARSE_NEXT;
	break;
	case Tok_COLON:
		ctx->parser = parseSkipTypeDecl;
		return PARSE_RECURSE;
	break;
	case Tok_CurlR:
		return PARSE_EXIT;
	default: return PARSE_NEXT;
	}
}


static RustParserAction parseStructDecl ( RustToken what,LexingState* st,  RustParserContext* ctx)
{
	dbprintf("parse struct: %s\n", vStringValue(st->name));
	switch (what)
	{
	// todo: typeparams.
	case RustIDENTIFIER:
		addTag_MainIdent (st->name, NULL,K_STRUCT,ctx);
		return PARSE_NEXT;
	case Tok_PARL: // todo - parse 'argument block' here
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
	case Tok_CurlL:
		ctx->parser = parseStructFields;
		return PARSE_RECURSE|PARSE_EXIT;
		break;
	case Tok_SEMI:
		return PARSE_EXIT;
	default: return PARSE_EXIT;
	}
	return PARSE_EXIT;
}

static RustParserAction parseEnumVariants(RustToken what, LexingState* st, RustParserContext* ctx)
{
	dbprintf("parse enum variant: %s\n", vStringValue(st->name));
	switch (what) {

	// TODO: should really be parsing a collection of structs here
	// then we'd get members of structural types too.
	// un-named ones are more common in rust code though

	case RustIDENTIFIER:
		addTag(st->name,NULL,K_VARIANT,ctx,ctx->parent);
	break;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_PARL:
	case Tok_CurlL:
		return PARSE_IGNORE_BALANCED;
	case Tok_CurlR:
		return PARSE_EXIT;
	break;
	default: return PARSE_NEXT;
	}
	return PARSE_NEXT;
}

static RustParserAction parseEnumDecl ( RustToken what,LexingState* st,  RustParserContext* ctx)
{
	dbprintf("parse enumdecl: %s\n", vStringValue(st->name));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_MainIdent (st->name, NULL,K_STRUCT,ctx);
		return PARSE_NEXT;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_CurlL:
		ctx->parser=parseEnumVariants;
		return	PARSE_RECURSE|PARSE_EXIT;
	case Tok_PARL:
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
	default: return PARSE_EXIT;
	}
	return PARSE_EXIT;
}



static RustParserAction parseMethod ( RustToken what,LexingState* st, RustParserContext* ctx)
{
	dbprintf("parse method");
	// TODO - reduce cut/paste, factor in common part with Fn!
	// its all the same except K_METHOD instead of K_FN - but we want that to distinguish .completions
	dbprintf("%p parse fn: %s\n", ctx, vStringValue(st->name));
	switch (what)
	{
	case RustIDENTIFIER:
		//stash the method name, we generate the actual method when we get the arg block (...)
		rpc_set_main_ident(ctx, st->name, K_METHOD);		
		return PARSE_NEXT;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_PARL:	// param block
		{
			const char* args=lex_strdup_balanced(st,"(");
			dbprintf("adding method %s",vStringValue(ctx->name));
			printf("args");
			addTag(ctx->name, args,K_METHOD,ctx,ctxParentParent(ctx));
			return PARSE_NEXT;
		}
	case Tok_CurlL:	// fn body, then quit.
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
		break;
	case Tok_SEMI:	// fn body, then quit.
		return PARSE_EXIT;
	default: return PARSE_NEXT;
	}
	return PARSE_NEXT;
}


static RustParserAction parseFn ( RustToken what,LexingState* st, RustParserContext* ctx)
{
	dbprintf("%p parse fn: %s\n", ctx, vStringValue(st->name));
	switch (what)
	{
	case RustIDENTIFIER:
		//stash the method name, we generate the actual method when we get the arg block (...)
		rpc_set_main_ident(ctx, st->name, K_METHOD);		
		return PARSE_NEXT;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_PARL:	// param block
		{
			const char* args=lex_strdup_balanced(st,"(");
			dbprintf("adding method %s",vStringValue(ctx->name));
			printf("args");
			addTag(ctx->name, args,K_METHOD,ctx,ctxParentParent(ctx));
			return PARSE_NEXT;
		}

	case Tok_CurlL:	// fn body, then quit.
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
		break;
	case Tok_SEMI:	// fn body, then quit.
		return PARSE_EXIT;
	default: return PARSE_NEXT;
	}
	return PARSE_NEXT;
}

static RustParserAction parseMethods ( RustToken what,LexingState* st, RustParserContext* ctx)
{
	switch (what) {
	case RustFN:
		ctx->parser=parseMethod;
		return PARSE_RECURSE;
	case Tok_CurlR:
		return PARSE_EXIT;
	default: return PARSE_NEXT;
	}
	return PARSE_NEXT;
}

static RustParserAction parseTrait ( RustToken what,LexingState* st, RustParserContext* ctx)
{
	dbprintf("%p parse trait: %s\n", ctx, vStringValue(st->name));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_MainIdent (st->name, NULL,K_TRAIT,ctx);
		return PARSE_NEXT;
	case Tok_CurlL:
		ctx->parser=parseMethods;
		return PARSE_RECURSE|PARSE_EXIT;
		break;
	case Tok_PARL:
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
		break;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	default: return PARSE_EXIT;
	}
	return PARSE_EXIT;
}

static RustParserAction parseImpl ( RustToken what,LexingState* st,  RustParserContext* ctx)
{
	printf("parse impl: %s\n", vStringValue(st->name));
	switch (what)
	{
	case RustFOR:	// clearn the main ident so the next overwrites it.
					// this allows gathering member functions on the struct.
					//  impl <TRAIT> for <STRUCT> {...}
		ctx->main_ident_set=0;
		return PARSE_NEXT;
	case RustIDENTIFIER:
		addTag_MainIdent (st->name,NULL, K_IMPL,ctx);
		return PARSE_NEXT;
	case Tok_CurlL:
		printf("foo\n");
		ctx->parser=parseMethods;
		return PARSE_RECURSE|PARSE_EXIT;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_PARL:
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
		break;
	default: return PARSE_EXIT;
	}
	return PARSE_EXIT;
}



/* Handle the "strong" top levels, all 'big' declarations
 * happen here */
static RustParserAction parseModBody (RustToken what,LexingState* st,   RustParserContext* ctx);

static RustParserAction parseModDecl (RustToken what,LexingState* st,  RustParserContext* ctx) {
	dbprintf("PARSE MOD HAEADER\n");
	switch (what) {
	case RustIDENTIFIER:
		dbprintf("adding mod tag %s\n",vStringValue(st->name));
		addTag_MainIdent (st->name, NULL,K_MOD, ctx);
		return PARSE_NEXT;

	case Tok_CurlL:
		ctx->parser=parseModBody;
		return PARSE_RECURSE|PARSE_EXIT;
	case Tok_SEMI:
		return PARSE_EXIT;
	default: return PARSE_NEXT;
	}
	return PARSE_NEXT;
}

static RustParserAction parseModBody (RustToken what,LexingState* st, RustParserContext* ctx)
{
	dbprintf("(parse mod body: %d)", what);
	switch (what)
	{

	case RustFN:
		ctx->parser=parseFn;
		return PARSE_RECURSE;

	case RustMOD:		//same as global scope?!
		dbprintf("*********8MOD*******8\n");
		ctx->parser=parseModDecl;
		return PARSE_RECURSE;

	case RustSTRUCT:
		ctx->parser=parseStructDecl;
		return PARSE_RECURSE;

	case RustTRAIT:
		ctx->parser=parseTrait;
		return PARSE_RECURSE;

	case RustIMPL:
		ctx->parser=parseImpl;
		return PARSE_RECURSE;

	case RustENUM:
		ctx->parser=parseEnumDecl;
		return PARSE_RECURSE;
	case Tok_CurlR:
		/* we don't care */
		return PARSE_EXIT;

	case Tok_PARL:
	case Tok_CurlL:
		return PARSE_IGNORE_BALANCED;
	
	default:
		/* we don't care */
		return PARSE_NEXT;
	}
}

/// Parser Nestable iterator
static int parseRecursive(LexingState* st,  RustParserContext* parentContext) {
	int ret=0;
	RustParserContext ctx;
	ctx.name=vStringNew();
	ctx.kind=K_NONE;
	ctx.parser=NULL;
	ctx.main_ident_set=0;
	ctx.parent=parentContext;

	Assert(parentContext && parentContext->parser);

	dbprintf("%p %d:%s . this=%p\n", parentContext, parentContext->kind, vStringValue(parentContext->name), &ctx);

	while (1){
		int action=0,sub_action=0;

		RustToken tok=lex(st);
		dbprintf("(%d %s)\n", tok, vStringValue(st->name));
		if (tok==Tok_EOF)
			break;
		action=parentContext->parser(tok, st,  &ctx);
		sub_action = action & (~PARSE_EXIT_MASK);
		dbprintf("action: %d\n", action);
		if (sub_action==PARSE_RECURSE) {
			int ret_level=0;
			dbprintf("recurse: %p\n", ctx.parser);
			ret_level=parseRecursive(st, &ctx);
			if (ret_level>0) {
				ret=ret_level-1;
				goto return_early;
			}
		} else if (sub_action==PARSE_IGNORE_BALANCED) {
			dbprintf("ignore balanced\n");
			ignoreBalanced(st);
		} else if (sub_action==PARSE_IGNORE_TYPE_PARAMS) {
			dbprintf("ignore TypeParams\n");
			ignoreTypeParams(st);
		}
		if (action & PARSE_EXIT_MASK) {
			//number of levels to exit.
			ret=PARSE_GET_EXIT_COUNT(action)-1;
			
			break;
		}
	}
return_early:
	vStringDelete(ctx.name);
	return ret;
}



static void findRustTags (void)
{
	LexingState st;
	RustParserContext ctx;

	st.name = vStringNew();

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
	static const char *const extensions[] = { "rs", NULL };
	parserDefinition *def = parserNew ("Rust");
	def->kinds = RustKinds;
	def->kindCount = KIND_COUNT (RustKinds);
	def->extensions = extensions;
	def->parser = findRustTags;
	def->initialize = rustInitialize;

	dbprintf("rust parser\n");

	return def;
}
