
/*
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

#define dbprintf //


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
typedef enum  {
	PARSE_NEXT,
	PARSE_RECURSE,
	PARSE_IGNORE_BALANCED,
	PARSE_IGNORE_TYPE_PARAMS,
	PARSE_EXIT=0x10000,
	PARSE_EXIT2=0x20000,
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
	RustFOR,

	Tok_COMMA,	/* ',' */
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
	Tok_SEMI,	/* ';' */
	Tok_COLON,	/* ':' */
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
static void addTag (vString * const ident, int kind, const RustParserContext* ctx, const RustParserContext* parent)
{
	tagEntryInfo tag;
	initTagEntry (&tag, vStringValue (ident));


	tag.lineNumber=getSourceLineNumber();
	tag.filePosition	= getInputFilePosition();
	tag.sourceFileName=getSourceFileName();

	tag.kindName=RustKinds[kind].name;
	tag.kind = RustKinds[kind].letter;

//	if (ctx->parent) {
		// TODO - we currently nest 2 levels, can we avoid that. eg
		//  trait
		//    trait header...
		//		trait body
		//			trait function1
		//			trait function2
		//			..
	if (parent) {
		dbprintf("%x nested in  %x %d %s %s\n",ctx,parent, parent->kind, RustKinds[parent->kind].name,vStringValue(parent->name));
		tag.extensionFields.scope[0]=RustKinds[parent->kind].name;
		tag.extensionFields.scope[1]=vStringValue(parent->name);
	}
//	} 
	//else printf("not nested\n");
	// TODO arglist would be filled in here 
	//	: tag.extentionFields.arglist
	makeTagEntry (&tag);
}

static void ignoreBalanced (struct _LexingState* st)
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

static void ignoreTypeParams (struct _LexingState* st)
{
	dbprintf("ignore type params\n");
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

// type declaration :....,;)}
static RustParserAction parseSkipTypeDecl(RustToken what, vString* ident, RustParserContext* ctx)
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
	}
	return PARSE_NEXT;
}

static RustParserAction parseStructFields(RustToken what, vString* ident, RustParserContext* ctx)
{
	dbprintf("parse struct fields\n");
	switch (what) {
	case RustIDENTIFIER:	//only want the rirst .. until ?!
		addTag(ident, K_FIELD, ctx,ctx->parent);
	break;
	case Tok_COLON:
		ctx->parser = parseSkipTypeDecl;
		return PARSE_RECURSE;
	break;
	case Tok_CurlR:
		return PARSE_EXIT;
	}
	return PARSE_NEXT;
}


static RustParserAction parseStructDecl ( RustToken what,vString*  ident,  RustParserContext* ctx)
{
	dbprintf("parse struct: %s\n",vStringValue(ident));
	switch (what)
	{
	// todo: typeparams.
	case RustIDENTIFIER:
		addTag_MainIdent (ident, K_STRUCT,ctx);
		return PARSE_NEXT;
	case Tok_PARL: // todo - parse 'argument block' here
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
	case Tok_CurlL:	
		ctx->parser = parseStructFields;
		return PARSE_RECURSE|PARSE_EXIT;
		break;
	case Tok_SEMI:
		return PARSE_EXIT;
	}
	return PARSE_EXIT;
}

static RustParserAction parseEnumVariants(RustToken what, vString* ident, RustParserContext* ctx)
{
	dbprintf("parse enum variant: %s\n",vStringValue(ident));
	switch (what) {

	// TODO: should really be parsing a collection of structs here
	// then we'd get members of structural types too.
	// un-named ones are more common in rust code though 

	case RustIDENTIFIER:
		addTag(ident,K_VARIANT,ctx,ctx->parent);
	break;
	case Tok_LT:
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_PARL:
	case Tok_CurlL:
		return PARSE_IGNORE_BALANCED;
	case Tok_CurlR:
		return PARSE_EXIT;
	break;
	}
	return PARSE_NEXT;
}

static RustParserAction parseEnumDecl ( RustToken what,vString*  ident,  RustParserContext* ctx)
{
	dbprintf("parse enumdecl: %s\n",vStringValue(ident));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_MainIdent (ident, K_STRUCT,ctx);
		return PARSE_NEXT;
	case Tok_LT:	
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_CurlL:	
		ctx->parser=parseEnumVariants;
		return	PARSE_RECURSE|PARSE_EXIT;
	case Tok_PARL:		
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
	}
	return PARSE_EXIT;
}

static RustParserAction parseFn ( RustToken what,vString*  ident, RustParserContext* ctx)
{
	dbprintf("%x parse fn: %s\n",ctx, vStringValue(ident));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_MainIdent (ident, K_FN,ctx);
		return PARSE_NEXT;
	case Tok_LT:	
		return PARSE_IGNORE_TYPE_PARAMS;
	case Tok_PARL:	// param block
		return PARSE_IGNORE_BALANCED;
	case Tok_CurlL:	// fn body, then quit.
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
		break;
	case Tok_SEMI:	// fn body, then quit.
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
	dbprintf("%x parse trait: %s\n",ctx, vStringValue(ident));
	switch (what)
	{
	case RustIDENTIFIER:
		addTag_MainIdent (ident, K_TRAIT,ctx);
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
	}
	return PARSE_EXIT;
}

RustParserContext* ctxParentParent(RustParserContext* ctx) {
	if (ctx->parent)
		return ctx->parent->parent;
	else return NULL;
}
void addTag_MainIdent(vString* ident, RustKind kind, RustParserContext* ctx) {
	if (!ctx->main_ident_set) {
		ctx->main_ident_set++;
		dbprintf("%d\n",ctx->main_ident_set);
		
		addTag(ident,kind,ctx, ctxParentParent(ctx));
		ctx->kind=kind;
		if (ctx->name) {
			vStringDelete(ctx->name);
		}
		ctx->name=vStringNewCopy(ident);
		dbprintf("set main ident %x %d %s\n",ctx, ctx->kind, vStringValue(ctx->name));
	}
}

static RustParserAction parseImpl ( RustToken what,vString*  ident,  RustParserContext* ctx)
{
	dbprintf("parse impl: %s\n",vStringValue(ident));
	switch (what)
	{
	case RustFOR:	// clearn the main ident so the next overwrites it. 
					// this allows gathering member functions on the struct.
		ctx->main_ident_set=0;
		return PARSE_NEXT;
	case RustIDENTIFIER:
		addTag_MainIdent (ident, K_IMPL,ctx);
		return PARSE_NEXT;
	case Tok_CurlL:	
		ctx->parser=parseMethods;
		return PARSE_RECURSE|PARSE_EXIT;
	case Tok_LT:	
		return PARSE_IGNORE_TYPE_PARAMS;		
	case Tok_PARL:	
		return PARSE_IGNORE_BALANCED|PARSE_EXIT;
		break;
	}
	return PARSE_EXIT;
}



/* Handle the "strong" top levels, all 'big' declarations
 * happen here */
static RustParserAction parseModBody (RustToken what,vString*  ident,   RustParserContext* ctx);

static RustParserAction parseModDecl (RustToken what,vString*  ident,  RustParserContext* ctx) {
	dbprintf("PARSE MOD HAEADER\n");
	switch (what) {
	case RustIDENTIFIER:
		dbprintf("adding mod tag %s\n",vStringValue(ident));
		addTag_MainIdent (ident, K_MOD, ctx);
		return PARSE_NEXT;

	case Tok_CurlL:
		ctx->parser=parseModBody;
		return PARSE_RECURSE|PARSE_EXIT;
	case Tok_SEMI:
		return PARSE_EXIT;
	}
	return PARSE_NEXT;
}
static RustParserAction parseModBody (RustToken what,vString*  ident,   RustParserContext* ctx)
{
	dbprintf("(parse mod body: %d)",what);
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
int parseRecursive(LexingState* st,  RustParserContext* parentContext) {
	RustParserContext ctx;
	ctx.name=vStringNew();
	ctx.kind=K_NONE;
	ctx.parser=NULL;
	ctx.main_ident_set=0;
	ctx.parent=parentContext;
	if (parentContext)
		dbprintf("%x %d:%s . this=%x\n",parentContext,parentContext->kind,vStringValue(parentContext->name),&ctx);
	int ret=0;

	while (1){
		RustToken tok=lex(st);
		dbprintf("(%d %s)\n",tok,vStringValue(st->name));
		if (tok==Tok_EOF)
			break;
		if (!parentContext->parser) {
			dbprintf("%d %x?! no callback\n");
		}
		int action=parentContext->parser(tok,st->name,  &ctx);
		int sub_action = action & (~PARSE_EXIT_MASK);
		//printf("action:\n",action);
		if (sub_action==PARSE_RECURSE) {
			dbprintf("recurse: %x\n",ctx.parser);
			int ret_level=parseRecursive(st, &ctx);
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
			ret=((action)>>16)-1;
			
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
	dbprintf("rust parser\n");
	static const char *const extensions[] = { "rs", NULL };
	parserDefinition *def = parserNew ("Rust");
	def->kinds = RustKinds;
	def->kindCount = KIND_COUNT (RustKinds);
	def->extensions = extensions;
	def->parser = findRustTags;
	def->initialize = rustInitialize;

	return def;
}
