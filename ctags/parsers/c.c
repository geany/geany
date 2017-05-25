/*
*   Copyright (c) 1996-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for parsing and scanning C, C++, C#, D and Java
*   source files.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"        /* must always come first */

#include <string.h>
#include <setjmp.h>

#include "debug.h"
#include "mio.h"
#include "entry.h"
#include "lcpp.h"
#include "keyword.h"
#include "options.h"
#include "parse.h"
#include "read.h"
#include "routines.h"
#include "xtag.h"

/*
*   MACROS
*/

#define activeToken(st)     ((st)->token [(int) (st)->tokenIndex])
#define parentDecl(st)      ((st)->parent == NULL ? \
                            DECL_NONE : (st)->parent->declaration)
#define isType(token,t)     (bool) ((token)->type == (t))
#define insideEnumBody(st)  (bool) ((st)->parent == NULL ? false : \
									   ((st)->parent->declaration == DECL_ENUM))
#define isExternCDecl(st,c) (bool) ((c) == STRING_SYMBOL  && \
									   ! (st)->haveQualifyingName && \
									   (st)->scope == SCOPE_EXTERN)

#define isOneOf(c,s)        (bool) (strchr ((s), (c)) != NULL)

/*
*   DATA DECLARATIONS
*/

enum { NumTokens = 12 };

typedef enum eException
{
	ExceptionNone, ExceptionEOF, ExceptionFormattingError,
	ExceptionBraceFormattingError
} exception_t;

/*  Used to specify type of keyword.
 */
enum eKeywordId
{
	KEYWORD_ATTRIBUTE, KEYWORD_ABSTRACT, KEYWORD_ALIAS,
	KEYWORD_BOOLEAN, KEYWORD_BYTE, KEYWORD_BAD_STATE, KEYWORD_BAD_TRANS,
	KEYWORD_BIND, KEYWORD_BIND_VAR, KEYWORD_BIT, KEYWORD_BODY,
	KEYWORD_CASE, KEYWORD_CATCH, KEYWORD_CHAR, KEYWORD_CLASS, KEYWORD_CONST,
	KEYWORD_CONSTRAINT, KEYWORD_COVERAGE_BLOCK, KEYWORD_COVERAGE_DEF,
	KEYWORD_DEFAULT, KEYWORD_DELEGATE, KEYWORD_DELETE, KEYWORD_DO,
	KEYWORD_DOUBLE,
	KEYWORD_ELSE, KEYWORD_ENUM, KEYWORD_EXPLICIT, KEYWORD_EXTERN,
	KEYWORD_EXTENDS, KEYWORD_EVENT,
	KEYWORD_FINAL, KEYWORD_FINALLY, KEYWORD_FLOAT, KEYWORD_FOR, KEYWORD_FRIEND, KEYWORD_FUNCTION,
	KEYWORD_GET, KEYWORD_GOTO,
	KEYWORD_IF, KEYWORD_IMPLEMENTS, KEYWORD_IMPORT, KEYWORD_IN, KEYWORD_INLINE, KEYWORD_INT,
	KEYWORD_INOUT, KEYWORD_INPUT, KEYWORD_INTEGER, KEYWORD_INTERFACE,
	KEYWORD_INTERNAL,
	KEYWORD_LOCAL, KEYWORD_LONG,
	KEYWORD_M_BAD_STATE, KEYWORD_M_BAD_TRANS, KEYWORD_M_STATE, KEYWORD_M_TRANS,
	KEYWORD_MODULE, KEYWORD_MUTABLE,
	KEYWORD_NAMESPACE, KEYWORD_NEW, KEYWORD_NEWCOV, KEYWORD_NATIVE, KEYWORD_NOEXCEPT,
	KEYWORD_OPERATOR, KEYWORD_OUT, KEYWORD_OUTPUT, KEYWORD_OVERLOAD, KEYWORD_OVERRIDE,
	KEYWORD_PACKED, KEYWORD_PORT, KEYWORD_PACKAGE, KEYWORD_PRIVATE,
	KEYWORD_PROGRAM, KEYWORD_PROTECTED, KEYWORD_PUBLIC,
	KEYWORD_REF, KEYWORD_REGISTER, KEYWORD_RETURN,
	KEYWORD_SHADOW, KEYWORD_STATE,
	KEYWORD_SET, KEYWORD_SHORT, KEYWORD_SIGNAL, KEYWORD_SIGNED, KEYWORD_SIZE_T, KEYWORD_STATIC,
	KEYWORD_STATIC_ASSERT, KEYWORD_STRING,
	KEYWORD_STRUCT, KEYWORD_SWITCH, KEYWORD_SYNCHRONIZED,
	KEYWORD_TASK, KEYWORD_TEMPLATE, KEYWORD_THIS, KEYWORD_THROW,
	KEYWORD_THROWS, KEYWORD_TRANSIENT, KEYWORD_TRANS, KEYWORD_TRANSITION,
	KEYWORD_TRY, KEYWORD_TYPEDEF, KEYWORD_TYPENAME,
	KEYWORD_UINT, KEYWORD_ULONG, KEYWORD_UNION, KEYWORD_UNSIGNED, KEYWORD_USHORT,
	KEYWORD_USING,
	KEYWORD_VIRTUAL, KEYWORD_VOID, KEYWORD_VOLATILE,
	KEYWORD_WCHAR_T, KEYWORD_WEAK, KEYWORD_WHILE
};
typedef int keywordId; /* to allow KEYWORD_NONE */

/*  Used to determine whether keyword is valid for the current language and
 *  what its ID is.
 */
typedef struct sKeywordDesc
{
	const char *name;
	keywordId id;
	short isValid [7]; /* indicates languages for which kw is valid */
} keywordDesc;

/*  Used for reporting the type of object parsed by nextToken ().
 */
typedef enum eTokenType
{
	TOKEN_NONE,          /* none */
	TOKEN_ARGS,          /* a parenthetical pair and its contents */
	TOKEN_BRACE_CLOSE,
	TOKEN_BRACE_OPEN,
	TOKEN_COMMA,         /* the comma character */
	TOKEN_DOUBLE_COLON,  /* double colon indicates nested-name-specifier */
	TOKEN_KEYWORD,
	TOKEN_NAME,          /* an unknown name */
	TOKEN_PACKAGE,       /* a Java package name */
	TOKEN_PAREN_NAME,    /* a single name in parentheses */
	TOKEN_SEMICOLON,     /* the semicolon character */
	TOKEN_SPEC,          /* a storage class specifier, qualifier, type, etc. */
	TOKEN_STAR,          /* pointer detection */
	TOKEN_ARRAY,         /* array detection */
	TOKEN_COUNT
} tokenType;

/*  This describes the scoping of the current statement.
 */
typedef enum eTagScope
{
	SCOPE_GLOBAL,        /* no storage class specified */
	SCOPE_STATIC,        /* static storage class */
	SCOPE_EXTERN,        /* external storage class */
	SCOPE_FRIEND,        /* declares access only */
	SCOPE_TYPEDEF,       /* scoping depends upon context */
	SCOPE_COUNT
} tagScope;

typedef enum eDeclaration
{
	DECL_NONE,
	DECL_BASE,           /* base type (default) */
	DECL_CLASS,
	DECL_ENUM,
	DECL_EVENT,
	DECL_SIGNAL,
	DECL_FUNCTION,
	DECL_FUNCTION_TEMPLATE,
	DECL_IGNORE,         /* non-taggable "declaration" */
	DECL_INTERFACE,
	DECL_MODULE,
	DECL_NAMESPACE,
	DECL_NOMANGLE,       /* C++ name demangling block */
	DECL_PACKAGE,
	DECL_STRUCT,
	DECL_UNION,
	DECL_COUNT
} declType;

typedef enum eVisibilityType
{
	ACCESS_UNDEFINED,
	ACCESS_PRIVATE,
	ACCESS_PROTECTED,
	ACCESS_PUBLIC,
	ACCESS_DEFAULT,      /* Java-specific */
	ACCESS_COUNT
} accessType;

/*  Information about the parent class of a member (if any).
 */
typedef struct sMemberInfo
{
	accessType access;           /* access of current statement */
	accessType accessDefault;    /* access default for current statement */
} memberInfo;

typedef struct sTokenInfo
{
	tokenType     type;
	keywordId     keyword;
	vString*      name;          /* the name of the token */
	unsigned long lineNumber;    /* line number of tag */
	MIOPos        filePosition;  /* file position of line containing name */
} tokenInfo;

typedef enum eImplementation
{
	IMP_DEFAULT,
	IMP_ABSTRACT,
	IMP_VIRTUAL,
	IMP_PURE_VIRTUAL,
	IMP_COUNT
} impType;

/*  Describes the statement currently undergoing analysis.
 */
typedef struct sStatementInfo
{
	tagScope	scope;
	declType	declaration;    /* specifier associated with TOKEN_SPEC */
	bool		gotName;        /* was a name parsed yet? */
	bool		haveQualifyingName;  /* do we have a name we are considering? */
	bool		gotParenName;   /* was a name inside parentheses parsed yet? */
	bool		gotArgs;        /* was a list of parameters parsed yet? */
	unsigned int nSemicolons;   /* how many semicolons did we see in that statement */
	impType		implementation; /* abstract or concrete implementation? */
	unsigned int tokenIndex;    /* currently active token */
	tokenInfo*	token [((int) NumTokens)];
	tokenInfo*	context;        /* accumulated scope of current statement */
	tokenInfo*	blockName;      /* name of current block */
	memberInfo	member;         /* information regarding parent class/struct */
	vString*	parentClasses;  /* parent classes */
	struct sStatementInfo *parent;  /* statement we are nested within */
	long 			argEndPosition; /* Position where argument list ended */
	tokenInfo* 		firstToken; /* First token in the statement */
} statementInfo;

/*  Describes the type of tag being generated.
 */
typedef enum eTagType
{
	TAG_UNDEFINED,
	TAG_CLASS,       /* class name */
	TAG_ENUM,        /* enumeration name */
	TAG_ENUMERATOR,  /* enumerator (enumeration value) */
	TAG_FIELD,       /* field (Java) */
	TAG_FUNCTION,    /* function definition */
	TAG_INTERFACE,   /* interface declaration */
	TAG_MEMBER,      /* structure, class or interface member */
	TAG_METHOD,      /* method declaration */
	TAG_NAMESPACE,   /* namespace name */
	TAG_PACKAGE,     /* package name / D module name */
	TAG_PROTOTYPE,   /* function prototype or declaration */
	TAG_STRUCT,      /* structure name */
	TAG_TYPEDEF,     /* typedef name */
	TAG_UNION,       /* union name */
	TAG_VARIABLE,    /* variable definition */
	TAG_EXTERN_VAR,  /* external variable declaration */
	TAG_MACRO,       /* #define s */
	TAG_EVENT,       /* event */
	TAG_SIGNAL,      /* signal */
	TAG_LOCAL,       /* local variable definition */
	TAG_PROPERTY,    /* property name */
	TAG_COUNT        /* must be last */
} tagType;

typedef struct sParenInfo
{
	bool isParamList;
	bool isKnrParamList;
	bool isNameCandidate;
	bool invalidContents;
	bool nestedArgs;
	unsigned int parameterCount;
} parenInfo;

/*
*   DATA DEFINITIONS
*/

static jmp_buf Exception;

static langType Lang_c;
static langType Lang_cpp;
static langType Lang_csharp;
static langType Lang_java;
static langType Lang_d;
static langType Lang_glsl;
static langType Lang_ferite;
static langType Lang_vala;

/* Used to index into the CKinds table. */
typedef enum
{
	CK_UNDEFINED = -1,
	CK_CLASS, CK_DEFINE, CK_ENUMERATOR, CK_FUNCTION,
	CK_ENUMERATION, CK_MEMBER, CK_NAMESPACE, CK_PROTOTYPE,
	CK_STRUCT, CK_TYPEDEF, CK_UNION, CK_VARIABLE,
	CK_EXTERN_VARIABLE
} cKind;

static kindOption CKinds [] = {
	{ true,  'c', "class",      "classes"},
	{ true,  'd', "macro",      "macro definitions"},
	{ true,  'e', "enumerator", "enumerators (values inside an enumeration)"},
	{ true,  'f', "function",   "function definitions"},
	{ true,  'g', "enum",       "enumeration names"},
	{ true,  'm', "member",     "class, struct, and union members"},
	{ true,  'n', "namespace",  "namespaces"},
	{ false, 'p', "prototype",  "function prototypes"},
	{ true,  's', "struct",     "structure names"},
	{ true,  't', "typedef",    "typedefs"},
	{ true,  'u', "union",      "union names"},
	{ true,  'v', "variable",   "variable definitions"},
	{ false, 'x', "externvar",  "external variable declarations"},
};

/* Used to index into the DKinds table. */
typedef enum
{
	DK_UNDEFINED = -1,
	DK_CLASS, DK_ENUMERATOR, DK_FUNCTION,
	DK_ENUMERATION, DK_INTERFACE, DK_MEMBER, DK_NAMESPACE, DK_PROTOTYPE,
	DK_STRUCT, DK_TYPEDEF, DK_UNION, DK_VARIABLE,
	DK_EXTERN_VARIABLE
} dKind;

static kindOption DKinds [] = {
	{ true,  'c', "class",      "classes"},
	{ true,  'e', "enumerator", "enumerators (values inside an enumeration)"},
	{ true,  'f', "function",   "function definitions"},
	{ true,  'g', "enum",       "enumeration names"},
	{ true,  'i', "interface",  "interfaces"},
	{ true,  'm', "member",     "class, struct, and union members"},
	{ true,  'n', "namespace",  "namespaces"},
	{ false, 'p', "prototype",  "function prototypes"},
	{ true,  's', "struct",     "structure names"},
	{ true,  't', "typedef",    "typedefs"},
	{ true,  'u', "union",      "union names"},
	{ true,  'v', "variable",   "variable definitions"},
	{ false, 'x', "externvar",  "external variable declarations"},
};

/* Used to index into the JavaKinds table. */
typedef enum
{
	JK_UNDEFINED = -1,
	JK_CLASS, JK_FIELD, JK_INTERFACE, JK_METHOD,
	JK_PACKAGE, JK_ENUMERATOR, JK_ENUMERATION
} javaKind;

static kindOption JavaKinds [] = {
	{ true,  'c', "class",         "classes"},
	{ true,  'f', "field",         "fields"},
	{ true,  'i', "interface",     "interfaces"},
	{ true,  'm', "method",        "methods"},
	{ true,  'p', "package",       "packages"},
	{ true,  'e', "enumConstant",  "enum constants"},
	{ true,  'g', "enum",          "enum types"},
};

typedef enum
{
	CSK_UNDEFINED = -1,
	CSK_CLASS, CSK_DEFINE, CSK_ENUMERATOR, CSK_EVENT, CSK_FIELD,
	CSK_ENUMERATION, CSK_INTERFACE, CSK_LOCAL, CSK_METHOD,
	CSK_NAMESPACE, CSK_PROPERTY, CSK_STRUCT, CSK_TYPEDEF
} csharpKind;

static kindOption CsharpKinds [] = {
	{ true,  'c', "class",      "classes"},
	{ true,  'd', "macro",      "macro definitions"},
	{ true,  'e', "enumerator", "enumerators (values inside an enumeration)"},
	{ true,  'E', "event",      "events"},
	{ true,  'f', "field",      "fields"},
	{ true,  'g', "enum",       "enumeration names"},
	{ true,  'i', "interface",  "interfaces"},
	{ false, 'l', "local",      "local variables"},
	{ true,  'm', "method",     "methods"},
	{ true,  'n', "namespace",  "namespaces"},
	{ true,  'p', "property",   "properties"},
	{ true,  's', "struct",     "structure names"},
	{ true,  't', "typedef",    "typedefs"},
};

typedef enum {
	VK_UNDEFINED = -1,
	VK_CLASS, VK_DEFINE, VK_ENUMERATOR, VK_FIELD,
	VK_ENUMERATION, VK_INTERFACE, VK_LOCAL, VK_METHOD,
	VK_NAMESPACE, VK_PROPERTY, VK_SIGNAL, VK_STRUCT
} valaKind;

static kindOption ValaKinds [] = {
	{ true,  'c', "class",      "classes"},
	{ true,  'd', "macro",      "macro definitions"},
	{ true,  'e', "enumerator", "enumerators (values inside an enumeration)"},
	{ true,  'f', "field",      "fields"},
	{ true,  'g', "enum",       "enumeration names"},
	{ true,  'i', "interface",  "interfaces"},
	{ false, 'l', "local",      "local variables"},
	{ true,  'm', "method",     "methods"},
	{ true,  'n', "namespace",  "namespaces"},
	{ true,  'p', "property",   "properties"},
	{ true,  'S', "signal",     "signals"},
	{ true,  's', "struct",     "structure names"},
};

/* Note: some keyword aliases are added in initializeDParser, initializeValaParser */
static const keywordDesc KeywordTable [] = {
	/*                                              C++                  */
	/*                                       ANSI C  |  C# Java          */
	/*                                            |  |  |  |  Vera       */
	/*                                            |  |  |  |  |  Vala    */
	/*                                            |  |  |  |  |  |  D    */
	/* keyword          keyword ID                |  |  |  |  |  |  |    */
	{ "__attribute__",  KEYWORD_ATTRIBUTE,      { 1, 1, 1, 0, 0, 0, 1 } },
	{ "abstract",       KEYWORD_ABSTRACT,       { 0, 0, 1, 1, 0, 1, 1 } },
	{ "bad_state",      KEYWORD_BAD_STATE,      { 0, 0, 0, 0, 1, 0, 0 } },
	{ "bad_trans",      KEYWORD_BAD_TRANS,      { 0, 0, 0, 0, 1, 0, 0 } },
	{ "bind",           KEYWORD_BIND,           { 0, 0, 0, 0, 1, 0, 0 } },
	{ "bind_var",       KEYWORD_BIND_VAR,       { 0, 0, 0, 0, 1, 0, 0 } },
	{ "bit",            KEYWORD_BIT,            { 0, 0, 0, 0, 1, 0, 0 } },
	{ "body",           KEYWORD_BODY,           { 0, 0, 0, 0, 0, 0, 1 } },
	{ "boolean",        KEYWORD_BOOLEAN,        { 0, 0, 0, 1, 0, 0, 0 } },
	{ "byte",           KEYWORD_BYTE,           { 0, 0, 0, 1, 0, 0, 1 } },
	{ "case",           KEYWORD_CASE,           { 1, 1, 1, 1, 0, 1, 1 } },
	{ "catch",          KEYWORD_CATCH,          { 0, 1, 1, 0, 0, 1, 1 } },
	{ "char",           KEYWORD_CHAR,           { 1, 1, 1, 1, 0, 1, 1 } },
	{ "class",          KEYWORD_CLASS,          { 0, 1, 1, 1, 1, 1, 1 } },
	{ "const",          KEYWORD_CONST,          { 1, 1, 1, 1, 0, 1, 1 } },
	{ "constraint",     KEYWORD_CONSTRAINT,     { 0, 0, 0, 0, 1, 0, 0 } },
	{ "coverage_block", KEYWORD_COVERAGE_BLOCK, { 0, 0, 0, 0, 1, 0, 0 } },
	{ "coverage_def",   KEYWORD_COVERAGE_DEF,   { 0, 0, 0, 0, 1, 0, 0 } },
	{ "do",             KEYWORD_DO,             { 1, 1, 1, 1, 0, 1, 1 } },
	{ "default",        KEYWORD_DEFAULT,        { 1, 1, 1, 1, 0, 1, 1 } },
	{ "delegate",       KEYWORD_DELEGATE,       { 0, 0, 1, 0, 0, 1, 1 } },
	{ "delete",         KEYWORD_DELETE,         { 0, 1, 0, 0, 0, 1, 1 } },
	{ "double",         KEYWORD_DOUBLE,         { 1, 1, 1, 1, 0, 1, 1 } },
	{ "else",           KEYWORD_ELSE,           { 1, 1, 0, 1, 0, 1, 1 } },
	{ "enum",           KEYWORD_ENUM,           { 1, 1, 1, 1, 1, 1, 1 } },
	{ "event",          KEYWORD_EVENT,          { 0, 0, 1, 0, 1, 0, 0 } },
	{ "explicit",       KEYWORD_EXPLICIT,       { 0, 1, 1, 0, 0, 0, 1 } },
	{ "extends",        KEYWORD_EXTENDS,        { 0, 0, 0, 1, 1, 0, 0 } },
	{ "extern",         KEYWORD_EXTERN,         { 1, 1, 1, 0, 1, 1, 0 } },
	{ "extern",         KEYWORD_NAMESPACE,      { 0, 0, 0, 0, 0, 0, 1 } },	/* parse block */
	{ "final",          KEYWORD_FINAL,          { 0, 0, 0, 1, 0, 0, 1 } },
	{ "finally",        KEYWORD_FINALLY,        { 0, 0, 0, 0, 0, 1, 1 } },
	{ "float",          KEYWORD_FLOAT,          { 1, 1, 1, 1, 0, 1, 1 } },
	{ "for",            KEYWORD_FOR,            { 1, 1, 1, 1, 0, 1, 1 } },
	{ "friend",         KEYWORD_FRIEND,         { 0, 1, 0, 0, 0, 0, 0 } },
	{ "function",       KEYWORD_FUNCTION,       { 0, 0, 0, 0, 1, 0, 1 } },
	{ "get",            KEYWORD_GET,            { 0, 0, 0, 0, 0, 1, 0 } },
	{ "goto",           KEYWORD_GOTO,           { 1, 1, 1, 1, 0, 1, 1 } },
	{ "if",             KEYWORD_IF,             { 1, 1, 1, 1, 0, 1, 1 } },
	{ "implements",     KEYWORD_IMPLEMENTS,     { 0, 0, 0, 1, 0, 0, 0 } },
	{ "import",         KEYWORD_IMPORT,         { 0, 0, 0, 1, 0, 0, 1 } },
	{ "inline",         KEYWORD_INLINE,         { 0, 1, 0, 0, 0, 1, 0 } },
	{ "in",             KEYWORD_IN,             { 0, 0, 0, 0, 0, 0, 1 } },
	{ "inout",          KEYWORD_INOUT,          { 0, 0, 0, 0, 1, 0, 0 } },
	{ "inout",          KEYWORD_CONST,          { 0, 0, 0, 0, 0, 0, 1 } }, /* treat like const */
	{ "input",          KEYWORD_INPUT,          { 0, 0, 0, 0, 1, 0, 0 } },
	{ "int",            KEYWORD_INT,            { 1, 1, 1, 1, 0, 1, 1 } },
	{ "integer",        KEYWORD_INTEGER,        { 0, 0, 0, 0, 1, 0, 0 } },
	{ "interface",      KEYWORD_INTERFACE,      { 0, 0, 1, 1, 1, 1, 1 } },
	{ "internal",       KEYWORD_INTERNAL,       { 0, 0, 1, 0, 0, 0, 0 } },
	{ "local",          KEYWORD_LOCAL,          { 0, 0, 0, 0, 1, 0, 0 } },
	{ "long",           KEYWORD_LONG,           { 1, 1, 1, 1, 0, 1, 1 } },
	{ "m_bad_state",    KEYWORD_M_BAD_STATE,    { 0, 0, 0, 0, 1, 0, 0 } },
	{ "m_bad_trans",    KEYWORD_M_BAD_TRANS,    { 0, 0, 0, 0, 1, 0, 0 } },
	{ "m_state",        KEYWORD_M_STATE,        { 0, 0, 0, 0, 1, 0, 0 } },
	{ "m_trans",        KEYWORD_M_TRANS,        { 0, 0, 0, 0, 1, 0, 0 } },
	{ "mutable",        KEYWORD_MUTABLE,        { 0, 1, 0, 0, 0, 0, 0 } },
	{ "module",         KEYWORD_MODULE,         { 0, 0, 0, 0, 0, 0, 1 } },
	{ "namespace",      KEYWORD_NAMESPACE,      { 0, 1, 1, 0, 0, 1, 0 } },
	{ "native",         KEYWORD_NATIVE,         { 0, 0, 0, 1, 0, 0, 0 } },
	{ "new",            KEYWORD_NEW,            { 0, 1, 1, 1, 0, 1, 1 } },
	{ "newcov",         KEYWORD_NEWCOV,         { 0, 0, 0, 0, 1, 0, 0 } },
	{ "noexcept",       KEYWORD_NOEXCEPT,       { 0, 1, 0, 0, 0, 0, 0 } },
	{ "operator",       KEYWORD_OPERATOR,       { 0, 1, 1, 0, 0, 0, 0 } },
	{ "out",            KEYWORD_OUT,            { 0, 0, 0, 0, 0, 1, 1 } },
	{ "output",         KEYWORD_OUTPUT,         { 0, 0, 0, 0, 1, 0, 0 } },
	{ "overload",       KEYWORD_OVERLOAD,       { 0, 1, 0, 0, 0, 0, 0 } },
	{ "override",       KEYWORD_OVERRIDE,       { 0, 0, 1, 0, 0, 1, 1 } },
	{ "package",        KEYWORD_PACKAGE,        { 0, 0, 0, 1, 0, 0, 1 } },
	{ "packed",         KEYWORD_PACKED,         { 0, 0, 0, 0, 1, 0, 0 } },
	{ "port",           KEYWORD_PORT,           { 0, 0, 0, 0, 1, 0, 0 } },
	{ "private",        KEYWORD_PRIVATE,        { 0, 1, 1, 1, 0, 1, 1 } },
	{ "program",        KEYWORD_PROGRAM,        { 0, 0, 0, 0, 1, 0, 0 } },
	{ "protected",      KEYWORD_PROTECTED,      { 0, 1, 1, 1, 1, 1, 1 } },
	{ "public",         KEYWORD_PUBLIC,         { 0, 1, 1, 1, 1, 1, 1 } },
	{ "ref",            KEYWORD_REF,            { 0, 0, 0, 0, 0, 1, 1 } },
	{ "register",       KEYWORD_REGISTER,       { 1, 1, 0, 0, 0, 0, 0 } },
	{ "return",         KEYWORD_RETURN,         { 1, 1, 1, 1, 0, 1, 1 } },
	{ "set",            KEYWORD_SET,            { 0, 0, 0, 0, 0, 1, 0 } },
	{ "shadow",         KEYWORD_SHADOW,         { 0, 0, 0, 0, 1, 0, 0 } },
	{ "short",          KEYWORD_SHORT,          { 1, 1, 1, 1, 0, 1, 1 } },
	{ "signal",         KEYWORD_SIGNAL,         { 0, 0, 0, 0, 0, 1, 0 } },
	{ "signed",         KEYWORD_SIGNED,         { 1, 1, 0, 0, 0, 0, 0 } },
	{ "size_t",         KEYWORD_SIZE_T,         { 0, 0, 0, 0, 0, 1, 0 } },
	{ "state",          KEYWORD_STATE,          { 0, 0, 0, 0, 1, 0, 0 } },
	{ "static",         KEYWORD_STATIC,         { 1, 1, 1, 1, 1, 1, 1 } },
	{ "static_assert",  KEYWORD_STATIC_ASSERT,  { 0, 1, 0, 0, 0, 0, 0 } },
	{ "string",         KEYWORD_STRING,         { 0, 0, 1, 0, 1, 1, 0 } },
	{ "struct",         KEYWORD_STRUCT,         { 1, 1, 1, 0, 0, 1, 1 } },
	{ "switch",         KEYWORD_SWITCH,         { 1, 1, 1, 1, 0, 1, 1 } },
	{ "synchronized",   KEYWORD_SYNCHRONIZED,   { 0, 0, 0, 1, 0, 0, 1 } },
	{ "task",           KEYWORD_TASK,           { 0, 0, 0, 0, 1, 0, 0 } },
	{ "template",       KEYWORD_TEMPLATE,       { 0, 1, 0, 0, 0, 0, 0 } },
	{ "template",       KEYWORD_NAMESPACE,      { 0, 0, 0, 0, 0, 0, 1 } },	/* parse block */
	{ "this",           KEYWORD_THIS,           { 0, 0, 1, 1, 0, 1, 0 } },	/* 0 to allow D ctor tags */
	{ "throw",          KEYWORD_THROW,          { 0, 1, 1, 1, 0, 1, 1 } },
	{ "throws",         KEYWORD_THROWS,         { 0, 0, 0, 1, 0, 1, 0 } },
	{ "trans",          KEYWORD_TRANS,          { 0, 0, 0, 0, 1, 0, 0 } },
	{ "transition",     KEYWORD_TRANSITION,     { 0, 0, 0, 0, 1, 0, 0 } },
	{ "transient",      KEYWORD_TRANSIENT,      { 0, 0, 0, 1, 0, 0, 0 } },
	{ "try",            KEYWORD_TRY,            { 0, 1, 1, 0, 0, 1, 1 } },
	{ "typedef",        KEYWORD_TYPEDEF,        { 1, 1, 1, 0, 1, 0, 1 } },
	{ "typename",       KEYWORD_TYPENAME,       { 0, 1, 0, 0, 0, 0, 0 } },
	{ "uint",           KEYWORD_UINT,           { 0, 0, 1, 0, 0, 1, 1 } },
	{ "ulong",          KEYWORD_ULONG,          { 0, 0, 1, 0, 0, 1, 1 } },
	{ "union",          KEYWORD_UNION,          { 1, 1, 0, 0, 0, 0, 1 } },
	{ "unsigned",       KEYWORD_UNSIGNED,       { 1, 1, 1, 0, 0, 0, 1 } },
	{ "ushort",         KEYWORD_USHORT,         { 0, 0, 1, 0, 0, 1, 1 } },
	{ "using",          KEYWORD_USING,          { 0, 1, 1, 0, 0, 1, 0 } },
	{ "virtual",        KEYWORD_VIRTUAL,        { 0, 1, 1, 0, 1, 1, 0 } },
	{ "void",           KEYWORD_VOID,           { 1, 1, 1, 1, 1, 1, 1 } },
	{ "volatile",       KEYWORD_VOLATILE,       { 1, 1, 1, 1, 0, 0, 1 } },
	{ "wchar_t",        KEYWORD_WCHAR_T,        { 0, 1, 1, 0, 0, 0, 0 } },
	{ "weak",           KEYWORD_WEAK,           { 0, 0, 0, 0, 0, 1, 0 } },
	{ "while",          KEYWORD_WHILE,          { 1, 1, 1, 1, 0, 1, 1 } }
};


/*
*   FUNCTION PROTOTYPES
*/
static void createTags (const unsigned int nestLevel, statementInfo *const parent);
static void copyToken (tokenInfo *const dest, const tokenInfo *const src);
static const char *getVarType (const statementInfo *const st,
							   const tokenInfo *const token);

/*
*   FUNCTION DEFINITIONS
*/

/* Debugging functions added by Biswa */
#if defined(DEBUG_C) && DEBUG_C
static char *tokenTypeName[] = {
	"none", "args", "'}'", "'{'", "','", "'::'", "keyword", "name",
	"package", "paren-name", "';'",	"spec", "*", "[]", "count"
};

static char *tagScopeNames[] = {
	"global", "static", "extern", "friend", "typedef", "count"};

static char *declTypeNames[] = {
	"none", "base", "class", "enum", "function", "ignore", "interface",
	"namespace", "nomangle", "package", "struct", "union", "count"};

static char *impTypeNames[] = {
	"default", "abstract", "virtual", "pure-virtual", "count"};

void printToken(const tokenInfo *const token)
{
	fprintf(stderr, "Type: %s, Keyword: %d, name: %s\n", tokenTypeName[token->type],
			token->keyword, vStringValue(token->name));
}

void printTagEntry(const tagEntryInfo *tag)
{
	fprintf(stderr, "Tag: %s (%s) [ impl: %s, scope: %s, type: %s\n", tag->name,
	tag->kindName, tag->extensionFields.implementation, tag->extensionFields.scope[1],
	tag->extensionFields.varType);
}

void printStatement(const statementInfo *const statement)
{
	int i;
	statementInfo *st = (statementInfo *) statement;
	while (NULL != st)
	{
		fprintf(stderr, "Statement Info:\n------------------------\n");
		fprintf(stderr, "scope: %s, decl: %s, impl: %s\n", tagScopeNames[st->scope],
				declTypeNames[st->declaration], impTypeNames[st->implementation]);
		for (i=0; i < NumTokens; ++i)
		{
			fprintf(stderr, "Token %d %s: ", i, (i == st->tokenIndex)?"(current)":"");
			printToken(st->token[i]);
		}
		fprintf(stderr, "Context: ");
		printToken(st->context);
		fprintf(stderr, "Block: ");
		printToken(st->blockName);
		fprintf(stderr, "Parent classes: %s\n", vStringValue(st->parentClasses));
		fprintf(stderr, "First token: ");
		printToken(st->firstToken);
		if (NULL != st->parent)
			fprintf(stderr, "Printing Parent:\n");
		st = st->parent;
	}
	fprintf(stderr, "-----------------------------------------------\n");
}
#endif

extern bool includingDefineTags (void)
{
	if (isInputLanguage(Lang_c) ||
		isInputLanguage(Lang_cpp) ||
		isInputLanguage(Lang_csharp) ||
		isInputLanguage(Lang_ferite) ||
		isInputLanguage(Lang_glsl) ||
		isInputLanguage(Lang_vala))
		return CKinds [CK_DEFINE].enabled;

	return false;
}

/*
*   Token management
*/

static void initToken (tokenInfo* const token)
{
	token->type			= TOKEN_NONE;
	token->keyword		= KEYWORD_NONE;
	token->lineNumber	= getInputLineNumber ();
	token->filePosition	= getInputFilePosition ();
	vStringClear (token->name);
}

static void advanceToken (statementInfo* const st)
{
	if (st->tokenIndex >= (unsigned int) NumTokens - 1)
		st->tokenIndex = 0;
	else
		++st->tokenIndex;
	initToken (st->token [st->tokenIndex]);
}

static tokenInfo *prevToken (const statementInfo *const st, unsigned int n)
{
	unsigned int tokenIndex;
	unsigned int num = (unsigned int) NumTokens;
	Assert (n < num);
	tokenIndex = (st->tokenIndex + num - n) % num;
	return st->token [tokenIndex];
}

static void setToken (statementInfo *const st, const tokenType type)
{
	tokenInfo *token;
	token = activeToken (st);
	initToken (token);
	token->type = type;
}

static void retardToken (statementInfo *const st)
{
	if (st->tokenIndex == 0)
		st->tokenIndex = (unsigned int) NumTokens - 1;
	else
		--st->tokenIndex;
	setToken (st, TOKEN_NONE);
}

static tokenInfo *newToken (void)
{
	tokenInfo *const token = xMalloc (1, tokenInfo);
	token->name = vStringNew ();
	initToken (token);
	return token;
}

static void deleteToken (tokenInfo *const token)
{
	if (token != NULL)
	{
		vStringDelete (token->name);
		eFree (token);
	}
}

static const char *accessString (const accessType laccess)
{
	static const char *const names [] = {
		"?", "private", "protected", "public", "default"
	};
	Assert (ARRAY_SIZE (names) == ACCESS_COUNT);
	Assert ((int) laccess < ACCESS_COUNT);
	return names[(int) laccess];
}

static const char *implementationString (const impType imp)
{
	static const char *const names [] = {
		"?", "abstract", "virtual", "pure virtual"
	};
	Assert (ARRAY_SIZE (names) == IMP_COUNT);
	Assert ((int) imp < IMP_COUNT);
	return names [(int) imp];
}

/*
*   Debugging functions
*/

#ifdef DEBUG

#define boolString(c)   ((c) ? "TRUE" : "FALSE")

static const char *tokenString (const tokenType type)
{
	static const char *const names [] = {
		"none", "args", "}", "{", "comma", "double colon", "keyword", "name",
		"package", "paren-name", "semicolon", "specifier", "*", "[]"
	};
	Assert (ARRAY_SIZE (names) == TOKEN_COUNT);
	Assert ((int) type < TOKEN_COUNT);
	return names [(int) type];
}

static const char *scopeString (const tagScope scope)
{
	static const char *const names [] = {
		"global", "static", "extern", "friend", "typedef"
	};
	Assert (ARRAY_SIZE (names) == SCOPE_COUNT);
	Assert ((int) scope < SCOPE_COUNT);
	return names [(int) scope];
}

static const char *declString (const declType declaration)
{
	static const char *const names [] = {
		"?", "base", "class", "enum", "event", "signal", "function",
		"function template", "ignore", "interface", "module", "namespace",
		"no mangle", "package", "struct", "union",
	};
	Assert (ARRAY_SIZE (names) == DECL_COUNT);
	Assert ((int) declaration < DECL_COUNT);
	return names [(int) declaration];
}

static const char *keywordString (const keywordId keyword)
{
	const size_t count = ARRAY_SIZE (KeywordTable);
	const char *name = "none";
	size_t i;
	for (i = 0  ;  i < count  ;  ++i)
	{
		const keywordDesc *p = &KeywordTable [i];
		if (p->id == keyword)
		{
			name = p->name;
			break;
		}
	}
	return name;
}

static void CTAGS_ATTR_UNUSED pt (tokenInfo *const token)
{
	if (isType (token, TOKEN_NAME))
		printf ("type: %-12s: %-13s   line: %lu\n",
			tokenString (token->type), vStringValue (token->name),
			token->lineNumber);
	else if (isType (token, TOKEN_KEYWORD))
		printf ("type: %-12s: %-13s   line: %lu\n",
			tokenString (token->type), keywordString (token->keyword),
			token->lineNumber);
	else
		printf ("type: %-12s                  line: %lu\n",
			tokenString (token->type), token->lineNumber);
}

static void CTAGS_ATTR_UNUSED ps (statementInfo *const st)
{
	unsigned int i;
	printf("scope: %s   decl: %s   gotName: %s   gotParenName: %s\n",
		   scopeString (st->scope), declString (st->declaration),
		   boolString (st->gotName), boolString (st->gotParenName));
	printf("haveQualifyingName: %s\n", boolString (st->haveQualifyingName));
	printf("access: %s   default: %s\n", accessString (st->member.access),
		   accessString (st->member.accessDefault));
	printf("token  : ");
	pt(activeToken (st));
	for (i = 1  ;  i < (unsigned int) NumTokens  ;  ++i)
	{
		printf("prev %u : ", i);
		pt(prevToken (st, i));
	}
	printf("context: ");
	pt(st->context);
}

#endif

/*
*   Statement management
*/

static bool isDataTypeKeyword (const tokenInfo *const token)
{
	switch (token->keyword)
	{
		case KEYWORD_BOOLEAN:
		case KEYWORD_BYTE:
		case KEYWORD_CHAR:
		case KEYWORD_DOUBLE:
		case KEYWORD_FLOAT:
		case KEYWORD_INT:
		case KEYWORD_LONG:
		case KEYWORD_SHORT:
		case KEYWORD_VOID:
		case KEYWORD_WCHAR_T:
		case KEYWORD_SIZE_T:
			return true;
		default:
			return false;
	}
}

#if 0
static bool isVariableKeyword (const tokenInfo *const token)
{
	switch (token->keyword)
	{
		case KEYWORD_CONST:
		case KEYWORD_EXTERN:
		case KEYWORD_REGISTER:
		case KEYWORD_STATIC:
		case KEYWORD_VIRTUAL:
		case KEYWORD_SIGNED:
		case KEYWORD_UNSIGNED:
			return true;
		default:
			return false;
	}
}
#endif

static bool isContextualKeyword (const tokenInfo *const token)
{
	bool result;
	switch (token->keyword)
	{
		case KEYWORD_CLASS:
		case KEYWORD_ENUM:
		case KEYWORD_INTERFACE:
		case KEYWORD_NAMESPACE:
		case KEYWORD_STRUCT:
		case KEYWORD_UNION:
		{
			result = true;
			break;
		}

		default:
		{
			result = false;
			break;
		}
	}
	return result;
}

static bool isContextualStatement (const statementInfo *const st)
{
	bool result = false;

	if (st != NULL)
	{
		if (isInputLanguage (Lang_vala))
		{
			/* All can be a contextual statement as properties can be of any type */
			result = true;
		}
		else
		{
			switch (st->declaration)
			{
				case DECL_CLASS:
				case DECL_ENUM:
				case DECL_INTERFACE:
				case DECL_NAMESPACE:
				case DECL_STRUCT:
				case DECL_UNION:
				{
					result = true;
					break;
				}

				default:
				{
					result = false;
					break;
				}
			}
		}
	}
	return result;
}

static bool isMember (const statementInfo *const st)
{
	bool result;
	if (isType (st->context, TOKEN_NAME))
		result = true;
	else
		result = isContextualStatement (st->parent);
	return result;
}

static void initMemberInfo (statementInfo *const st)
{
	accessType accessDefault = ACCESS_UNDEFINED;

	if (st->parent != NULL) switch (st->parent->declaration)
	{
		case DECL_ENUM:
		case DECL_NAMESPACE:
		{
			accessDefault = ACCESS_UNDEFINED;
			break;
		}
		case DECL_CLASS:
		{
			if (isInputLanguage (Lang_java))
				accessDefault = ACCESS_DEFAULT;
			else
				accessDefault = ACCESS_PRIVATE;
			break;
		}
		case DECL_INTERFACE:
		case DECL_STRUCT:
		case DECL_UNION:
		{
			accessDefault = ACCESS_PUBLIC;
			break;
		}
		default:
			break;
	}
	st->member.accessDefault = accessDefault;
	st->member.access		 = accessDefault;
}

static void reinitStatement (statementInfo *const st, const bool partial)
{
	unsigned int i;

	if (! partial)
	{
		st->scope = SCOPE_GLOBAL;
		if (isContextualStatement (st->parent))
			st->declaration = DECL_BASE;
		else
			st->declaration = DECL_NONE;
	}
	st->gotParenName		= false;
	st->implementation		= IMP_DEFAULT;
	st->gotArgs				= false;
	st->gotName				= false;
	st->nSemicolons			= 0;
	st->haveQualifyingName	= false;
	st->argEndPosition		= 0;

	st->tokenIndex			= 0;
	for (i = 0  ;  i < (unsigned int) NumTokens  ;  ++i)
	{
		initToken (st->token [i]);
	}

	initToken (st->context);
	initToken (st->blockName);
	vStringClear (st->parentClasses);

	/* Init member info. */
	if (! partial)
		st->member.access = st->member.accessDefault;

	/* Init first token */
	if (!partial)
		initToken(st->firstToken);
}

static void reinitStatementWithToken (statementInfo *const st,
									  tokenInfo *token, const bool partial)
{
	tokenInfo *const save = newToken ();
	/* given token can be part of reinit statementInfo */
	copyToken (save, token);
	reinitStatement (st, partial);
	token = activeToken (st);
	copyToken (token, save);
	deleteToken (save);
	++st->tokenIndex;	/* this is quite safe because current tokenIndex = 0 */
}

static void initStatement (statementInfo *const st, statementInfo *const parent)
{
	st->parent = parent;
	initMemberInfo (st);
	reinitStatement (st, false);
	if (parent)
	{
		const tokenInfo *const src = activeToken (parent);
		tokenInfo *const dst = activeToken (st);
		copyToken (dst, src);
		st->tokenIndex++;
	}
}

/*
*   Tag generation functions
*/
static cKind cTagKind (const tagType type)
{
	cKind result = CK_UNDEFINED;
	switch (type)
	{
		case TAG_CLASS:      result = CK_CLASS;       break;
		case TAG_ENUM:       result = CK_ENUMERATION; break;
		case TAG_ENUMERATOR: result = CK_ENUMERATOR;  break;
		case TAG_FUNCTION:   result = CK_FUNCTION;    break;
		case TAG_MEMBER:     result = CK_MEMBER;      break;
		case TAG_NAMESPACE:  result = CK_NAMESPACE;   break;
		case TAG_PROTOTYPE:  result = CK_PROTOTYPE;   break;
		case TAG_STRUCT:     result = CK_STRUCT;      break;
		case TAG_TYPEDEF:    result = CK_TYPEDEF;     break;
		case TAG_UNION:      result = CK_UNION;       break;
		case TAG_VARIABLE:   result = CK_VARIABLE;    break;
		case TAG_EXTERN_VAR: result = CK_EXTERN_VARIABLE; break;

		default: Assert ("Bad C tag type" == NULL); break;
	}
	return result;
}

static csharpKind csharpTagKind (const tagType type)
{
	csharpKind result = CSK_UNDEFINED;
	switch (type)
	{
		case TAG_CLASS:      result = CSK_CLASS;           break;
		case TAG_ENUM:       result = CSK_ENUMERATION;     break;
		case TAG_ENUMERATOR: result = CSK_ENUMERATOR;      break;
		case TAG_EVENT:      result = CSK_EVENT;           break;
		case TAG_FIELD:      result = CSK_FIELD ;          break;
		case TAG_INTERFACE:  result = CSK_INTERFACE;       break;
		case TAG_LOCAL:      result = CSK_LOCAL;           break;
		case TAG_METHOD:     result = CSK_METHOD;          break;
		case TAG_NAMESPACE:  result = CSK_NAMESPACE;       break;
		case TAG_PROPERTY:   result = CSK_PROPERTY;        break;
		case TAG_STRUCT:     result = CSK_STRUCT;          break;
		case TAG_TYPEDEF:    result = CSK_TYPEDEF;         break;

		default: Assert ("Bad C# tag type" == NULL); break;
	}
	return result;
}

static dKind dTagKind (const tagType type)
{
	dKind result = DK_UNDEFINED;
	switch (type)
	{
		case TAG_CLASS:      result = DK_CLASS;           break;
		case TAG_ENUM:       result = DK_ENUMERATION;     break;
		case TAG_ENUMERATOR: result = DK_ENUMERATOR;      break;
		case TAG_FUNCTION:   result = DK_FUNCTION;        break;
		case TAG_INTERFACE:  result = DK_INTERFACE;       break;
		case TAG_MEMBER:     result = DK_MEMBER;          break;
		case TAG_NAMESPACE:  result = DK_NAMESPACE;       break;
		case TAG_PROTOTYPE:  result = DK_PROTOTYPE;       break;
		case TAG_STRUCT:     result = DK_STRUCT;          break;
		case TAG_TYPEDEF:    result = DK_TYPEDEF;         break;
		case TAG_UNION:      result = DK_UNION;           break;
		case TAG_VARIABLE:   result = DK_VARIABLE;        break;
		case TAG_EXTERN_VAR: result = DK_EXTERN_VARIABLE; break;

		default: Assert ("Bad D tag type" == NULL); break;
	}
	return result;
}

static valaKind valaTagKind (const tagType type)
{
	valaKind result = VK_UNDEFINED;
	switch (type)
	{
		case TAG_CLASS:      result = VK_CLASS;           break;
		case TAG_ENUM:       result = VK_ENUMERATION;     break;
		case TAG_ENUMERATOR: result = VK_ENUMERATOR;      break;
		case TAG_SIGNAL:     result = VK_SIGNAL;          break;
		case TAG_FIELD:      result = VK_FIELD ;          break;
		case TAG_INTERFACE:  result = VK_INTERFACE;       break;
		case TAG_LOCAL:      result = VK_LOCAL;           break;
		case TAG_METHOD:     result = VK_METHOD;          break;
		case TAG_NAMESPACE:  result = VK_NAMESPACE;       break;
		case TAG_PROPERTY:   result = VK_PROPERTY;        break;
		case TAG_STRUCT:     result = VK_STRUCT;          break;

		default: Assert ("Bad Vala tag type" == NULL); break;
	}
	return result;
}

static javaKind javaTagKind (const tagType type)
{
	javaKind result = JK_UNDEFINED;
	switch (type)
	{
		case TAG_CLASS:      result = JK_CLASS;         break;
		case TAG_FIELD:      result = JK_FIELD;         break;
		case TAG_INTERFACE:  result = JK_INTERFACE;     break;
		case TAG_METHOD:     result = JK_METHOD;        break;
		case TAG_PACKAGE:    result = JK_PACKAGE;       break;
		case TAG_ENUM:       result = JK_ENUMERATION;   break;
		case TAG_ENUMERATOR: result = JK_ENUMERATOR;    break;

		default: Assert ("Bad Java tag type" == NULL); break;
	}
	return result;
}

static const kindOption *tagKind (const tagType type)
{
	const kindOption* result;
	if (isInputLanguage (Lang_java))
		result = &JavaKinds [javaTagKind (type)];
	else if (isInputLanguage (Lang_csharp))
		result = &CsharpKinds [csharpTagKind (type)];
	else if (isInputLanguage (Lang_d))
		result = &DKinds [dTagKind (type)];
	else if (isInputLanguage (Lang_vala))
		result = &ValaKinds [valaTagKind (type)];
	else
		result = &CKinds [cTagKind (type)];
	return result;
}

/*
static bool includeTag (const tagType type, const bool isFileScope)
{
	bool result;
	if (isFileScope  &&  ! Option.include.fileScope)
		result = false;
	else if (isInputLanguage (Lang_java))
		result = JavaKinds [javaTagKind (type)].enabled;
	else
		result = CKinds [cTagKind (type)].enabled;
	return result;
}
*/

static tagType declToTagType (const declType declaration)
{
	tagType type = TAG_UNDEFINED;

	switch (declaration)
	{
		case DECL_CLASS:        type = TAG_CLASS;       break;
		case DECL_ENUM:         type = TAG_ENUM;        break;
		case DECL_FUNCTION:     type = TAG_FUNCTION;    break;
		case DECL_FUNCTION_TEMPLATE: type = TAG_FUNCTION; break;
		case DECL_INTERFACE:    type = TAG_INTERFACE;   break;
		case DECL_NAMESPACE:    type = TAG_NAMESPACE;   break;
		case DECL_STRUCT:       type = TAG_STRUCT;      break;
		case DECL_UNION:        type = TAG_UNION;       break;

		default: Assert ("Unexpected declaration" == NULL); break;
	}
	return type;
}

static const char* accessField (const statementInfo *const st)
{
	const char* result = NULL;

	if ((isInputLanguage (Lang_cpp) || isInputLanguage (Lang_d) || isInputLanguage (Lang_ferite))  &&
			st->scope == SCOPE_FRIEND)
		result = "friend";
	else if (st->member.access != ACCESS_UNDEFINED)
		result = accessString (st->member.access);
	return result;
}

static void addOtherFields (tagEntryInfo* const tag, const tagType type,
							const tokenInfo *const nameToken,
							const statementInfo *const st, vString *const scope)
{
	/*  For selected tag types, append an extension flag designating the
	 *  parent object in which the tag is defined.
	 */
	switch (type)
	{
		default: break;

		case TAG_NAMESPACE:
		case TAG_CLASS:
		case TAG_ENUM:
		case TAG_ENUMERATOR:
		case TAG_FIELD:
		case TAG_FUNCTION:
		case TAG_INTERFACE:
		case TAG_MEMBER:
		case TAG_METHOD:
		case TAG_PROTOTYPE:
		case TAG_STRUCT:
		case TAG_TYPEDEF:
		case TAG_UNION:
		{
			if (vStringLength (scope) > 0  &&
				(isMember (st) || st->parent->declaration == DECL_NAMESPACE))
			{
				if (isType (st->context, TOKEN_NAME))
					tag->extensionFields.scopeKind = tagKind (TAG_CLASS);
				else
					tag->extensionFields.scopeKind =
						tagKind (declToTagType (parentDecl (st)));
				tag->extensionFields.scopeName = vStringValue (scope);
			}
			if ((type == TAG_CLASS  ||  type == TAG_INTERFACE  ||
				 type == TAG_STRUCT) && vStringLength (st->parentClasses) > 0)
			{
				tag->extensionFields.inheritance =
						vStringValue (st->parentClasses);
			}
			if (st->implementation != IMP_DEFAULT &&
				(isInputLanguage (Lang_cpp) || isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala) ||
				 isInputLanguage (Lang_java) || isInputLanguage (Lang_d) || isInputLanguage (Lang_ferite)))
			{
				tag->extensionFields.implementation =
						implementationString (st->implementation);
			}
			if (isMember (st))
			{
				tag->extensionFields.access = accessField (st);
			}
            if ((true == st->gotArgs) && (true == Option.extensionFields.argList) &&
				((TAG_FUNCTION == type) || (TAG_METHOD == type) || (TAG_PROTOTYPE == type)))
			{
				tag->extensionFields.signature = cppGetArglistFromFilePos(
						tag->filePosition, tag->name);
			}
			break;
		}
	}

	if ((TAG_FIELD == type) || (TAG_MEMBER == type) ||
		(TAG_EXTERN_VAR == type) || (TAG_TYPEDEF == type) ||
		(TAG_VARIABLE == type) || (TAG_METHOD == type) ||
		(TAG_PROTOTYPE == type) || (TAG_FUNCTION == type))
	{
		if (((TOKEN_NAME == st->firstToken->type) || isDataTypeKeyword(st->firstToken))
			&& (0 != strcmp(vStringValue(st->firstToken->name), tag->name)))
		{
			tag->extensionFields.varType = getVarType(st, nameToken);
		}
	}
}

static const char *getVarType (const statementInfo *const st,
							   const tokenInfo *const nameToken)
{
	static vString *vt = NULL;
	unsigned int i;
	unsigned int end = st->tokenIndex;
	bool seenType = false;

	switch (st->declaration) {
		case DECL_BASE:
		case DECL_FUNCTION:
		case DECL_FUNCTION_TEMPLATE:
			break;
		default:
			return vStringValue(st->firstToken->name);
	}

	if (vt == NULL)
		vt = vStringNew();
	else
		vStringClear(vt);

	/* find the end of the type signature in the token list */
	for (i = 0; i < st->tokenIndex; i++)
	{
		const tokenInfo *const t = st->token[i];

		/* stop if we find the token used to generate the tag name, or
		 * a name token in the middle yet not preceded by a scope separator */
		if ((t == nameToken ||
		     (t->type == nameToken->type &&
		      t->keyword == nameToken->keyword &&
		      t->lineNumber == nameToken->lineNumber &&
		      strcmp(vStringValue(t->name), vStringValue(nameToken->name)) == 0)) ||
		    (t->type == TOKEN_NAME && seenType &&
		     (i > 0 && st->token[i - 1]->type != TOKEN_DOUBLE_COLON)))
		{
			break;
		}
		if (t->type != TOKEN_DOUBLE_COLON)
			end = i + 1;
		if (t->type == TOKEN_NAME)
			seenType = true;
		else if (t->type == TOKEN_KEYWORD && isDataTypeKeyword(t))
			seenType = true;
	}

	/* ugly historic workaround when we can't figure out the type */
	if (end < 2 && ! st->gotArgs)
		return vStringValue(st->firstToken->name);

	for (i = 0; i < end; i++)
	{
		tokenInfo *t = st->token[i];

		switch (t->type)
		{
			case TOKEN_NAME:	/* user typename */
				break;
			case TOKEN_KEYWORD:
				if ((t->keyword != KEYWORD_EXTERN && t->keyword != KEYWORD_STATIC) &&	/* uninteresting keywords */
				    (st->gotArgs ||
				     /* ignore uninteresting keywords for non-functions */
				     (t->keyword != KEYWORD_PUBLIC &&
				      t->keyword != KEYWORD_PRIVATE &&
				      t->keyword != KEYWORD_PROTECTED &&
				      t->keyword != KEYWORD_FINAL &&
				      t->keyword != KEYWORD_TYPEDEF &&
				      /* hack for D static conditions */
				      t->keyword != KEYWORD_IF)))
				{
					break;
				}
				continue;
			case TOKEN_STAR: vStringCatS(vt, " *"); continue;
			case TOKEN_ARRAY: vStringCatS(vt, "[]"); continue;
			case TOKEN_DOUBLE_COLON:
				vStringCatS(vt, "::");
				continue;
			default: continue;
		}
		if (vStringLength(vt) > 0)
			if (isalpha(vStringValue(vt)[vStringLength(vt) - 1]))
				vStringPut(vt, ' ');
		vStringCat(vt, t->name);
	}
	return vStringValue(vt);
}

static void addContextSeparator (vString *const scope)
{
	if (isInputLanguage (Lang_c)  ||  isInputLanguage (Lang_cpp))
		vStringCatS (scope, "::");
	else if (isInputLanguage (Lang_java) || isInputLanguage (Lang_d) || isInputLanguage (Lang_ferite) ||
			 isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala))
		vStringCatS (scope, ".");
}

static void findScopeHierarchy (vString *const string,
								const statementInfo *const st)
{
	const char* const anon = "<anonymous>";
	bool nonAnonPresent = false;

	vStringClear (string);
	if (isType (st->context, TOKEN_NAME))
	{
		vStringCopy (string, st->context->name);
		nonAnonPresent = true;
	}
	if (st->parent != NULL)
	{
		vString *temp = vStringNew ();
		const statementInfo *s;

		for (s = st->parent  ;  s != NULL  ;  s = s->parent)
		{
			if (isContextualStatement (s) ||
				s->declaration == DECL_NAMESPACE)
			{
				vStringCopy (temp, string);
				vStringClear (string);
				if (isType (s->blockName, TOKEN_NAME))
				{
					if (isType (s->context, TOKEN_NAME) &&
						vStringLength (s->context->name) > 0)
					{
						vStringCat (string, s->context->name);
						addContextSeparator (string);
					}
					vStringCat (string, s->blockName->name);
					nonAnonPresent = true;
				}
				else
					vStringCopyS (string, anon);
				if (vStringLength (temp) > 0)
					addContextSeparator (string);
				vStringCat (string, temp);
			}
		}
		vStringDelete (temp);

		if (! nonAnonPresent)
			vStringClear (string);
	}
}

static void makeExtraTagEntry (const tagType type, tagEntryInfo *const e,
							   vString *const scope)
{
	if (isXtagEnabled(XTAG_QUALIFIED_TAGS)  &&
		scope != NULL  &&  vStringLength (scope) > 0)
	{
		vString *const scopedName = vStringNew ();

		if (type != TAG_ENUMERATOR)
			vStringCopy (scopedName, scope);
		else
		{
			/* remove last component (i.e. enumeration name) from scope */
			const char* const sc = vStringValue (scope);
			const char* colon = strrchr (sc, ':');
			if (colon != NULL)
			{
				while (*colon == ':'  &&  colon > sc)
					--colon;
				vStringNCopy (scopedName, scope, colon + 1 - sc);
			}
		}
		if (vStringLength (scopedName) > 0)
		{
			addContextSeparator (scopedName);
			vStringCatS (scopedName, e->name);
			e->name = vStringValue (scopedName);
			makeTagEntry (e);
		}
		vStringDelete (scopedName);
	}
}

static void makeTag (const tokenInfo *const token,
					 const statementInfo *const st,
					 bool isFileScope, const tagType type)
{
#ifdef DEBUG_C
	printToken(token);
	fprintf(stderr, "<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>\n");
	printStatement(st);
#endif
	/*  Nothing is really of file scope when it appears in a header file.
	 */
	isFileScope = (bool) (isFileScope && ! isInputHeaderFile ());

	if (isType (token, TOKEN_NAME)  &&  vStringLength (token->name) > 0  /* &&
		includeTag (type, isFileScope) */)
	{
		vString *scope;
		tagEntryInfo e;

		/* take only functions which are introduced by "function ..." */
		if (type == TAG_FUNCTION && isInputLanguage (Lang_ferite) &&
			strncmp("function", st->firstToken->name->buffer, 8) != 0)
		{
			return;
		}

		initTagEntry (&e, vStringValue (token->name), tagKind (type));

		e.lineNumber	= token->lineNumber;
		e.filePosition	= token->filePosition;
		e.isFileScope = isFileScope;

		scope = vStringNew ();
		findScopeHierarchy (scope, st);
		addOtherFields (&e, type, token, st, scope);

#ifdef DEBUG_C
		printTagEntry(&e);
#endif
		makeTagEntry (&e);
		makeExtraTagEntry (type, &e, scope);
		vStringDelete (scope);
		if (NULL != e.extensionFields.signature)
			free((char *) e.extensionFields.signature);
	}
}

static bool isValidTypeSpecifier (const declType declaration)
{
	bool result;
	switch (declaration)
	{
		case DECL_BASE:
		case DECL_CLASS:
		case DECL_ENUM:
		case DECL_STRUCT:
		case DECL_UNION:
			result = true;
			break;

		default:
			result = false;
			break;
	}
	return result;
}

static void qualifyEnumeratorTag (const statementInfo *const st,
								  const tokenInfo *const nameToken)
{
	if (isType (nameToken, TOKEN_NAME))
		makeTag (nameToken, st, true, TAG_ENUMERATOR);
}

static void qualifyFunctionTag (const statementInfo *const st,
								const tokenInfo *const nameToken)
{
	if (isType (nameToken, TOKEN_NAME))
	{
		const tagType type = (isInputLanguage (Lang_java) || isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala))
								? TAG_METHOD : TAG_FUNCTION;
		const bool isFileScope =
						(bool) (st->member.access == ACCESS_PRIVATE ||
						(!isMember (st)  &&  st->scope == SCOPE_STATIC));

		makeTag (nameToken, st, isFileScope, type);
	}
}

static void qualifyFunctionDeclTag (const statementInfo *const st,
									const tokenInfo *const nameToken)
{
	if (! isType (nameToken, TOKEN_NAME))
		;
	else if (isInputLanguage (Lang_java) || isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala))
		qualifyFunctionTag (st, nameToken);
	else if (st->scope == SCOPE_TYPEDEF)
		makeTag (nameToken, st, true, TAG_TYPEDEF);
	else if (isValidTypeSpecifier (st->declaration) &&
			 ! (isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala)))
		makeTag (nameToken, st, true, TAG_PROTOTYPE);
}

static void qualifyCompoundTag (const statementInfo *const st,
								const tokenInfo *const nameToken)
{
	if (isType (nameToken, TOKEN_NAME))
	{
		const tagType type = declToTagType (st->declaration);

		if (type != TAG_UNDEFINED)
			makeTag (nameToken, st, (bool) (! isInputLanguage (Lang_java) &&
											! isInputLanguage (Lang_csharp) &&
											! isInputLanguage (Lang_vala)), type);
    }
}

static void qualifyBlockTag (statementInfo *const st,
							 const tokenInfo *const nameToken)
{
	switch (st->declaration)
	{
		case DECL_CLASS:
		case DECL_ENUM:
		case DECL_INTERFACE:
		case DECL_NAMESPACE:
		case DECL_STRUCT:
		case DECL_UNION:
			qualifyCompoundTag (st, nameToken);
			break;
		default: break;
	}
}

static void qualifyVariableTag (const statementInfo *const st,
								const tokenInfo *const nameToken)
{
	/*	We have to watch that we do not interpret a declaration of the
	 *	form "struct tag;" as a variable definition. In such a case, the
	 *	token preceding the name will be a keyword.
	 */
	if (! isType (nameToken, TOKEN_NAME))
		;
	else if (st->declaration == DECL_IGNORE)
		;
	else if (st->scope == SCOPE_TYPEDEF)
		makeTag (nameToken, st, true, TAG_TYPEDEF);
	else if (st->declaration == DECL_PACKAGE)
		makeTag (nameToken, st, false, TAG_PACKAGE);
	else if (st->declaration == DECL_MODULE) /* handle modules in D as namespaces */
		makeTag (nameToken, st, false, TAG_NAMESPACE);
	else if (isValidTypeSpecifier (st->declaration))
	{
		if (isMember (st))
		{
			if (isInputLanguage (Lang_java) || isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala))
				makeTag (nameToken, st, (bool) (st->member.access == ACCESS_PRIVATE), TAG_FIELD);
			else if (st->scope == SCOPE_GLOBAL  ||  st->scope == SCOPE_STATIC)
				makeTag (nameToken, st, true, TAG_MEMBER);
		}
		else if (isInputLanguage (Lang_java) || isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala))
			;
		else
		{
			if (st->scope == SCOPE_EXTERN  ||  ! st->haveQualifyingName)
				makeTag (nameToken, st, false, TAG_EXTERN_VAR);
			else
				makeTag (nameToken, st, (bool) (st->scope == SCOPE_STATIC), TAG_VARIABLE);
		}
	}
}

/*
*   Parsing functions
*/

static int skipToOneOf (const char *const chars)
{
	int c;
	do
		c = cppGetc ();
	while (c != EOF  &&  c != '\0'  &&  strchr (chars, c) == NULL);

	return c;
}

/*  Skip to the next non-white character.
 */
static int skipToNonWhite (void)
{
	int c;

	do
	{
		c = cppGetc ();
	}
	while (isspace (c));

	return c;
}

/*  Skips to the next brace in column 1. This is intended for cases where
 *  preprocessor constructs result in unbalanced braces.
 */
static void skipToFormattedBraceMatch (void)
{
	int c, next;

	c = cppGetc ();
	next = cppGetc ();
	while (c != EOF  &&  (c != '\n'  ||  next != '}'))
	{
		c = next;
		next = cppGetc ();
	}
}

/*  Skip to the matching character indicated by the pair string. If skipping
 *  to a matching brace and any brace is found within a different level of a
 *  #if conditional statement while brace formatting is in effect, we skip to
 *  the brace matched by its formatting. It is assumed that we have already
 *  read the character which starts the group (i.e. the first character of
 *  "pair").
 */
static void skipToMatch (const char *const pair)
{
	const bool braceMatching = (bool) (strcmp ("{}", pair) == 0);
	const bool braceFormatting = (bool) (cppIsBraceFormat () && braceMatching);
	const unsigned int initialLevel = cppGetDirectiveNestLevel ();
	const int begin = pair [0], end = pair [1];
	const unsigned long inputLineNumber = getInputLineNumber ();
	int matchLevel = 1;
	int c = '\0';
	if (isInputLanguage(Lang_d) && pair[0] == '<')
		return; /* ignore e.g. Foo!(x < 2) */
	while (matchLevel > 0  &&  (c = cppGetc ()) != EOF)
	{
		if (c == begin)
		{
			++matchLevel;
			if (braceFormatting  &&  cppGetDirectiveNestLevel () != initialLevel)
			{
				skipToFormattedBraceMatch ();
				break;
			}
		}
		else if (c == end)
		{
			--matchLevel;
			if (braceFormatting  &&  cppGetDirectiveNestLevel () != initialLevel)
			{
				skipToFormattedBraceMatch ();
				break;
			}
		}
		/* early out if matching "<>" and we encounter a ";" or "{" to mitigate
		 * match problems with C++ generics containing a static expression like
		 *     foo<X<Y> bar;
		 * normally neither ";" nor "{" could appear inside "<>" anyway. */
		else if (isInputLanguage (Lang_cpp) && begin == '<' &&
		         (c == ';' || c == '{'))
		{
			cppUngetc (c);
			break;
		}
	}
	if (c == EOF)
	{
		verbose ("%s: failed to find match for '%c' at line %lu\n",
				getInputFileName (), begin, inputLineNumber);
		if (braceMatching)
			longjmp (Exception, (int) ExceptionBraceFormattingError);
		else
			longjmp (Exception, (int) ExceptionFormattingError);
	}
}

static void skipParens (void)
{
	const int c = skipToNonWhite ();

	if (c == '(')
		skipToMatch ("()");
	else
		cppUngetc (c);
}

static void skipBraces (void)
{
	const int c = skipToNonWhite ();

	if (c == '{')
		skipToMatch ("{}");
	else
		cppUngetc (c);
}

static keywordId analyzeKeyword (const char *const name)
{
	const keywordId id = (keywordId) lookupKeyword (name, getSourceLanguage ());

	/* ignore D @attributes and Java @annotations(...), but show them in function signatures */
	if ((isInputLanguage(Lang_d) || isInputLanguage(Lang_java)) && id == KEYWORD_NONE && name[0] == '@')
	{
		skipParens(); /* if annotation has parameters, skip them */
		return KEYWORD_CONST;
	}
	return id;
}

static void analyzeIdentifier (tokenInfo *const token)
{
	char *const name = vStringValue (token->name);
	const char *replacement = NULL;
	bool parensToo = false;

	if (isInputLanguage (Lang_java)  ||
		! isIgnoreToken (name, &parensToo, &replacement))
	{
		if (replacement != NULL)
			token->keyword = analyzeKeyword (replacement);
		else
			token->keyword = analyzeKeyword (vStringValue (token->name));

		if (token->keyword == KEYWORD_NONE)
			token->type = TOKEN_NAME;
		else
			token->type = TOKEN_KEYWORD;
	}
	else
	{
		initToken (token);
		if (parensToo)
		{
			int c = skipToNonWhite ();

			if (c == '(')
				skipToMatch ("()");
		}
	}
}

static void readIdentifier (tokenInfo *const token, const int firstChar)
{
	vString *const name = token->name;
	int c = firstChar;

	initToken (token);

	/* Bug #1585745 (CTags): strangely, C++ destructors allow whitespace between
	* the ~ and the class name. */
	if (isInputLanguage (Lang_cpp) && firstChar == '~')
	{
		vStringPut (name, c);
		c = skipToNonWhite ();
	}

	do
	{
		vStringPut (name, c);
		c = cppGetc ();
	} while (cppIsident (c) || (isInputLanguage (Lang_vala) && '.' == c));
	cppUngetc (c);        /* unget non-identifier character */

	/* Vala supports '?' at end of a type (with or without whitespace before) for nullable types */
	if (isInputLanguage (Lang_vala))
	{
		c = skipToNonWhite ();
		if ('?' == c)
			vStringPut (name, c);
		else
			cppUngetc (c);
	}

	analyzeIdentifier (token);
}

static void readPackageName (tokenInfo *const token, const int firstChar)
{
	vString *const name = token->name;
	int c = firstChar;

	initToken (token);

	while (cppIsident (c)  ||  c == '.')
	{
		vStringPut (name, c);
		c = cppGetc ();
	}
	cppUngetc (c);        /* unget non-package character */
}

static void readPackageOrNamespace (statementInfo *const st, const declType declaration)
{
	st->declaration = declaration;

	if (declaration == DECL_NAMESPACE && !(isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala)))
	{
		/* In C++ a namespace is specified one level at a time. */
		return;
	}
	else
	{
		/* In C#, a namespace can also be specified like a Java package name. */
		tokenInfo *const token = activeToken (st);
		Assert (isType (token, TOKEN_KEYWORD));
		readPackageName (token, skipToNonWhite ());
		token->type = TOKEN_NAME;
		st->gotName = true;
		st->haveQualifyingName = true;
	}
}

static void readPackage (statementInfo *const st)
{
	tokenInfo *const token = activeToken (st);
	Assert (isType (token, TOKEN_KEYWORD));
	readPackageName (token, skipToNonWhite ());
	token->type = TOKEN_NAME;
	if (isInputLanguage (Lang_d))
		st->declaration = DECL_MODULE;
	else
		st->declaration = DECL_PACKAGE;
	st->gotName = true;
	st->haveQualifyingName = true;
}

static void processName (statementInfo *const st)
{
	Assert (isType (activeToken (st), TOKEN_NAME));
	if (st->gotName  &&  st->declaration == DECL_NONE)
		st->declaration = DECL_BASE;
	st->gotName = true;
	st->haveQualifyingName = true;
}

static void readOperator (statementInfo *const st)
{
	const char *const acceptable = "+-*/%^&|~!=<>,[]";
	const tokenInfo* const prev = prevToken (st,1);
	tokenInfo *const token = activeToken (st);
	vString *const name = token->name;
	int c = skipToNonWhite ();

	/*  When we arrive here, we have the keyword "operator" in 'name'.
	 */
	if (isType (prev, TOKEN_KEYWORD) && (prev->keyword == KEYWORD_ENUM ||
		prev->keyword == KEYWORD_STRUCT || prev->keyword == KEYWORD_UNION))
			; /* ignore "operator" keyword if preceded by these keywords */
	else if (c == '(')
	{
		/*  Verify whether this is a valid function call (i.e. "()") operator.
		 */
		if (cppGetc () == ')')
		{
			vStringPut (name, ' ');  /* always separate operator from keyword */
			c = skipToNonWhite ();
			if (c == '(')
				vStringCatS (name, "()");
		}
		else
		{
			skipToMatch ("()");
			c = cppGetc ();
		}
	}
	else if (cppIsident1 (c))
	{
		/*  Handle "new" and "delete" operators, and conversion functions
		 *  (per 13.3.1.1.2 [2] of the C++ spec).
		 */
		bool whiteSpace = true;  /* default causes insertion of space */
		do
		{
			if (isspace (c))
				whiteSpace = true;
			else
			{
				if (whiteSpace)
				{
					vStringPut (name, ' ');
					whiteSpace = false;
				}
				vStringPut (name, c);
			}
			c = cppGetc ();
		} while (! isOneOf (c, "(;")  &&  c != EOF);
	}
	else if (isOneOf (c, acceptable))
	{
		vStringPut (name, ' ');  /* always separate operator from keyword */
		do
		{
			vStringPut (name, c);
			c = cppGetc ();
		} while (isOneOf (c, acceptable));
	}

	cppUngetc (c);

	token->type	= TOKEN_NAME;
	token->keyword = KEYWORD_NONE;
	processName (st);
}

static void copyToken (tokenInfo *const dest, const tokenInfo *const src)
{
	dest->type         = src->type;
	dest->keyword      = src->keyword;
	dest->filePosition = src->filePosition;
	dest->lineNumber   = src->lineNumber;
	vStringCopy (dest->name, src->name);
}

static void setAccess (statementInfo *const st, const accessType laccess)
{
	if (isMember (st))
	{
		if (isInputLanguage (Lang_cpp) || isInputLanguage (Lang_d) || isInputLanguage (Lang_ferite))
		{
			int c = skipToNonWhite ();

			if (c == ':')
				reinitStatementWithToken (st, prevToken (st, 1), false);
			else
				cppUngetc (c);

			st->member.accessDefault = laccess;
		}
		st->member.access = laccess;
	}
}

static void discardTypeList (tokenInfo *const token)
{
	int c = skipToNonWhite ();
	while (cppIsident1 (c))
	{
		readIdentifier (token, c);
		c = skipToNonWhite ();
		if (c == '.'  ||  c == ',')
			c = skipToNonWhite ();
	}
	cppUngetc (c);
}

static void addParentClass (statementInfo *const st, tokenInfo *const token)
{
	if (vStringLength (token->name) > 0  &&
		vStringLength (st->parentClasses) > 0)
	{
		vStringPut (st->parentClasses, ',');
	}
	vStringCat (st->parentClasses, token->name);
}

static void readParents (statementInfo *const st, const int qualifier)
{
	tokenInfo *const token = newToken ();
	tokenInfo *const parent = newToken ();
	int c;

	do
	{
		c = skipToNonWhite ();
		if (cppIsident1 (c))
		{
			readIdentifier (token, c);
			if (isType (token, TOKEN_NAME))
				vStringCat (parent->name, token->name);
			else
			{
				addParentClass (st, parent);
				initToken (parent);
			}
		}
		else if (c == qualifier)
			vStringPut (parent->name, c);
		else if (c == '<')
			skipToMatch ("<>");
		else if (isType (token, TOKEN_NAME))
		{
			addParentClass (st, parent);
			initToken (parent);
		}
	} while (c != '{'  &&  c != EOF);
	cppUngetc (c);
	deleteToken (parent);
	deleteToken (token);
}

static void checkIsClassEnum (statementInfo *const st, const declType decl)
{
	if (! isInputLanguage (Lang_cpp) || st->declaration != DECL_ENUM)
		st->declaration = decl;
}

static void processToken (tokenInfo *const token, statementInfo *const st)
{
	switch (token->keyword)		/* is it a reserved word? */
	{
		default: break;

		case KEYWORD_NONE:      processName (st);                       break;
		case KEYWORD_ABSTRACT:  st->implementation = IMP_ABSTRACT;      break;
		case KEYWORD_ATTRIBUTE: skipParens (); initToken (token);       break;
		case KEYWORD_CATCH:     skipParens (); skipBraces ();           break;
		case KEYWORD_CHAR:      st->declaration = DECL_BASE;            break;
		case KEYWORD_CLASS:     checkIsClassEnum (st, DECL_CLASS);      break;
		case KEYWORD_CONST:     st->declaration = DECL_BASE;            break;
		case KEYWORD_DOUBLE:    st->declaration = DECL_BASE;            break;
		case KEYWORD_ENUM:      st->declaration = DECL_ENUM;            break;
		case KEYWORD_EXTENDS:   readParents (st, '.');
		                        setToken (st, TOKEN_NONE);              break;
		case KEYWORD_FLOAT:     st->declaration = DECL_BASE;            break;
		case KEYWORD_FRIEND:    st->scope       = SCOPE_FRIEND;         break;
		case KEYWORD_IMPLEMENTS:readParents (st, '.');
		                        setToken (st, TOKEN_NONE);              break;
		case KEYWORD_IMPORT:    st->declaration = DECL_IGNORE;          break;
		case KEYWORD_INT:       st->declaration = DECL_BASE;            break;
		case KEYWORD_BOOLEAN:   st->declaration = DECL_BASE;            break;
		case KEYWORD_WCHAR_T:   st->declaration = DECL_BASE;            break;
		case KEYWORD_SIZE_T:    st->declaration = DECL_BASE;            break;
		case KEYWORD_INTERFACE: st->declaration = DECL_INTERFACE;       break;
		case KEYWORD_LONG:      st->declaration = DECL_BASE;            break;
		case KEYWORD_OPERATOR:  readOperator (st);                      break;
		case KEYWORD_MODULE:    readPackage (st);                       break;
		case KEYWORD_PRIVATE:   setAccess (st, ACCESS_PRIVATE);         break;
		case KEYWORD_PROTECTED: setAccess (st, ACCESS_PROTECTED);       break;
		case KEYWORD_PUBLIC:    setAccess (st, ACCESS_PUBLIC);          break;
		case KEYWORD_SHORT:     st->declaration = DECL_BASE;            break;
		case KEYWORD_SIGNED:    st->declaration = DECL_BASE;            break;
		case KEYWORD_STRUCT:    checkIsClassEnum (st, DECL_STRUCT);     break;
		case KEYWORD_STATIC_ASSERT: skipParens ();                      break;
		case KEYWORD_THROWS:    discardTypeList (token);                break;
		case KEYWORD_TYPEDEF:   st->scope	= SCOPE_TYPEDEF;            break;
		case KEYWORD_UNION:     st->declaration = DECL_UNION;           break;
		case KEYWORD_UNSIGNED:  st->declaration = DECL_BASE;            break;
		case KEYWORD_USING:     st->declaration = DECL_IGNORE;          break;
		case KEYWORD_VOID:      st->declaration = DECL_BASE;            break;
		case KEYWORD_VOLATILE:  st->declaration = DECL_BASE;            break;
		case KEYWORD_VIRTUAL:   st->implementation = IMP_VIRTUAL;       break;

		case KEYWORD_NAMESPACE: readPackageOrNamespace (st, DECL_NAMESPACE); break;
		case KEYWORD_PACKAGE:   readPackageOrNamespace (st, DECL_PACKAGE);   break;
		case KEYWORD_EVENT:
		{
			if (isInputLanguage (Lang_csharp))
				st->declaration = DECL_EVENT;
			break;
		}
		case KEYWORD_SIGNAL:
		{
			if (isInputLanguage (Lang_vala))
				st->declaration = DECL_SIGNAL;
			break;
		}
		case KEYWORD_EXTERN:
		{
			if (! isInputLanguage (Lang_csharp) || !st->gotName)
			{
				/*reinitStatement (st, false);*/
				st->scope = SCOPE_EXTERN;
				st->declaration = DECL_BASE;
			}
			break;
		}
		case KEYWORD_STATIC:
		{
			if (! isInputLanguage (Lang_java) && ! isInputLanguage (Lang_csharp) && ! isInputLanguage (Lang_vala))
			{
				/*reinitStatement (st, false);*/
				st->scope = SCOPE_STATIC;
				st->declaration = DECL_BASE;
			}
			break;
		}
		case KEYWORD_IF:
			if (isInputLanguage (Lang_d))
			{	/* static if (is(typeof(__traits(getMember, a, name)) == function)) */
				int c = skipToNonWhite ();
				if (c == '(')
					skipToMatch ("()");
			}
			break;
	}
}

/*
*   Parenthesis handling functions
*/

static void restartStatement (statementInfo *const st)
{
	tokenInfo *const save = newToken ();
	tokenInfo *token = activeToken (st);

	copyToken (save, token);
	DebugStatement ( if (debug (DEBUG_PARSE)) printf ("<ES>");)
	reinitStatement (st, false);
	token = activeToken (st);
	copyToken (token, save);
	deleteToken (save);
	processToken (token, st);
}

/*  Skips over a mem-initializer-list of a ctor-initializer, defined as:
 *
 *  mem-initializer-list:
 *    mem-initializer, mem-initializer-list
 *
 *  mem-initializer:
 *    [::] [nested-name-spec] class-name (...)
 *    identifier
 */
static void skipMemIntializerList (tokenInfo *const token)
{
	int c;

	do
	{
		c = skipToNonWhite ();
		while (cppIsident1 (c)  ||  c == ':')
		{
			if (c != ':')
				readIdentifier (token, c);
			c = skipToNonWhite ();
		}
		if (c == '<')
		{
			skipToMatch ("<>");
			c = skipToNonWhite ();
		}
		if (c == '(')
		{
			skipToMatch ("()");
			c = skipToNonWhite ();
		}
	} while (c == ',');
	cppUngetc (c);
}

static void skipMacro (statementInfo *const st)
{
	tokenInfo *const prev2 = prevToken (st, 2);

	if (isType (prev2, TOKEN_NAME))
		retardToken (st);
	skipToMatch ("()");
}

static bool isDPostArgumentToken(tokenInfo *const token)
{
	switch (token->keyword)
	{
		/* Note: some other keywords e.g. immutable are parsed as
		 * KEYWORD_CONST - see initializeDParser */
		case KEYWORD_CONST:
		/* template constraint */
		case KEYWORD_IF:
		/* contracts */
		case KEYWORD_IN:
		case KEYWORD_OUT:
		case KEYWORD_BODY:
			return true;
		default:
			break;
	}
	/* @attributes */
	if (vStringValue(token->name)[0] == '@')
		return true;
	return false;
}

/*  Skips over characters following the parameter list. This will be either
 *  non-ANSI style function declarations or C++ stuff. Our choices:
 *
 *  C (K&R):
 *    int func ();
 *    int func (one, two) int one; float two; {...}
 *  C (ANSI):
 *    int func (int one, float two);
 *    int func (int one, float two) {...}
 *  C++:
 *    int foo (...) [const|volatile] [throw (...)];
 *    int foo (...) [const|volatile] [throw (...)] [ctor-initializer] {...}
 *    int foo (...) [const|volatile] [throw (...)] try [ctor-initializer] {...}
 *        catch (...) {...}
 */
static bool skipPostArgumentStuff (
		statementInfo *const st, parenInfo *const info)
{
	tokenInfo *const token = activeToken (st);
	unsigned int parameters = info->parameterCount;
	unsigned int elementCount = 0;
	bool restart = false;
	bool end = false;
	int c = skipToNonWhite ();

	do
	{
		switch (c)
		{
		case ')':                               break;
		case ':': skipMemIntializerList (token);break;  /* ctor-initializer */
		case '[': skipToMatch ("[]");           break;
		case '=': cppUngetc (c); end = true;    break;
		case '{': cppUngetc (c); end = true;    break;
		case '}': cppUngetc (c); end = true;    break;

			case '(':
			{
				if (elementCount > 0)
					++elementCount;
				skipToMatch ("()");
				break;
			}

			case ';':
			{
				if (parameters == 0  ||  elementCount < 2)
				{
					cppUngetc (c);
					end = true;
				}
				else if (--parameters == 0)
					end = true;
				break;
			}

			default:
			{
				if (cppIsident1 (c))
				{
					readIdentifier (token, c);
					if (isInputLanguage(Lang_d) && isDPostArgumentToken(token))
						token->keyword = KEYWORD_CONST;

					switch (token->keyword)
					{
					case KEYWORD_ATTRIBUTE:	skipParens ();	break;
					case KEYWORD_THROW:	skipParens ();		break;
					case KEYWORD_CONST:						break;
					case KEYWORD_NOEXCEPT:					break;
					case KEYWORD_TRY:						break;
					case KEYWORD_VOLATILE:					break;

					case KEYWORD_CATCH:			case KEYWORD_CLASS:
					case KEYWORD_EXPLICIT:		case KEYWORD_EXTERN:
					case KEYWORD_FRIEND:		case KEYWORD_INLINE:
					case KEYWORD_MUTABLE:		case KEYWORD_NAMESPACE:
					case KEYWORD_NEW:			case KEYWORD_OPERATOR:
					case KEYWORD_OVERLOAD:		case KEYWORD_PRIVATE:
					case KEYWORD_PROTECTED:		case KEYWORD_PUBLIC:
					case KEYWORD_STATIC:		case KEYWORD_TEMPLATE:
					case KEYWORD_TYPEDEF:		case KEYWORD_TYPENAME:
					case KEYWORD_USING:			case KEYWORD_VIRTUAL:
						/*  Never allowed within parameter declarations.
						 */
						restart = true;
						end = true;
						break;

					default:
						/* "override" and "final" are only keywords in the declaration of a virtual
						 * member function, so need to be handled specially, not as keywords */
						if (isInputLanguage(Lang_cpp) && isType (token, TOKEN_NAME) &&
							(strcmp ("override", vStringValue (token->name)) == 0 ||
							 strcmp ("final", vStringValue (token->name)) == 0))
							;
						else if (isType (token, TOKEN_NONE))
							;
						else if (info->isKnrParamList  &&  info->parameterCount > 0)
							++elementCount;
						else
						{
							/*  If we encounter any other identifier immediately
							 *  following an empty parameter list, this is almost
							 *  certainly one of those Microsoft macro "thingies"
							 *  that the automatic source code generation sticks
							 *  in. Terminate the current statement.
							 */
							restart = true;
							end = true;
						}
						break;
					}
				}
			}
		}
		if (! end)
		{
			c = skipToNonWhite ();
			if (c == EOF)
				end = true;
		}
	} while (! end);

	if (restart)
		restartStatement (st);
	else
		setToken (st, TOKEN_NONE);

	return (bool) (c != EOF);
}

static void skipJavaThrows (statementInfo *const st)
{
	tokenInfo *const token = activeToken (st);
	int c = skipToNonWhite ();

	if (cppIsident1 (c))
	{
		readIdentifier (token, c);
		if (token->keyword == KEYWORD_THROWS)
		{
			do
			{
				c = skipToNonWhite ();
				if (cppIsident1 (c))
				{
					readIdentifier (token, c);
					c = skipToNonWhite ();
				}
			} while (c == '.'  ||  c == ',');
		}
	}
	cppUngetc (c);
	setToken (st, TOKEN_NONE);
}

static void skipValaPostParens (statementInfo *const st)
{
	tokenInfo *const token = activeToken (st);
	int c = skipToNonWhite ();

	while (cppIsident1 (c))
	{
		readIdentifier (token, c);
		if (token->keyword == KEYWORD_ATTRIBUTE)
		{
			/* parse contracts */
			skipParens ();
			c = skipToNonWhite ();
		}
		else if (token->keyword == KEYWORD_THROWS)
		{
			do
			{
				c = skipToNonWhite ();
				if (cppIsident1 (c))
				{
					readIdentifier (token, c);
					c = skipToNonWhite ();
				}
			} while (c == '.'  ||  c == ',');
		}
		else
			break;
	}
	cppUngetc (c);
	setToken (st, TOKEN_NONE);
}

static void analyzePostParens (statementInfo *const st, parenInfo *const info)
{
	const unsigned long inputLineNumber = getInputLineNumber ();
	int c = skipToNonWhite ();

	cppUngetc (c);
	if (isOneOf (c, "{;,="))
		;
	else if (isInputLanguage (Lang_java))
		skipJavaThrows (st);
	else if (isInputLanguage (Lang_vala))
		skipValaPostParens(st);
	else
	{
		if (! skipPostArgumentStuff (st, info))
		{
			verbose (
				"%s: confusing argument declarations beginning at line %lu\n",
				getInputFileName (), inputLineNumber);
			longjmp (Exception, (int) ExceptionFormattingError);
		}
	}
}

static int parseParens (statementInfo *const st, parenInfo *const info)
{
	tokenInfo *const token = activeToken (st);
	unsigned int identifierCount = 0;
	unsigned int depth = 1;
	bool firstChar = true;
	int nextChar = '\0';

	info->parameterCount = 1;
	do
	{
		int c = skipToNonWhite ();

		switch (c)
		{
			case '&':
			case '*':
			{
				/* DEBUG_PRINT("parseParens, po++\n"); */
				info->isKnrParamList = false;
				if (identifierCount == 0)
					info->isParamList = false;
				initToken (token);
				break;
			}
			case ':':
			{
				info->isKnrParamList = false;
				break;
			}
			case '.':
			{
				info->isNameCandidate = false;
				info->isKnrParamList = false;
				break;
			}
			case ',':
			{
				info->isNameCandidate = false;
				if (info->isKnrParamList)
				{
					++info->parameterCount;
					identifierCount = 0;
				}
				break;
			}
			case '=':
			{
				info->isKnrParamList = false;
				info->isNameCandidate = false;
				if (firstChar)
				{
					info->isParamList = false;
					skipMacro (st);
					depth = 0;
				}
				break;
			}
			case '[':
			{
				info->isKnrParamList = false;
				skipToMatch ("[]");
				break;
			}
			case '<':
			{
				info->isKnrParamList = false;
				skipToMatch ("<>");
				break;
			}
			case ')':
			{
				if (firstChar)
					info->parameterCount = 0;
				--depth;
				break;
			}
			case '(':
			{
				info->isKnrParamList = false;
				if (firstChar)
				{
					info->isNameCandidate = false;
					cppUngetc (c);
					skipMacro (st);
					depth = 0;
				}
				else if (isType (token, TOKEN_PAREN_NAME))
				{
					c = skipToNonWhite ();
					if (c == '*')        /* check for function pointer */
					{
						skipToMatch ("()");
						c = skipToNonWhite ();
						if (c == '(')
							skipToMatch ("()");
					}
					else
					{
						cppUngetc (c);
						cppUngetc ('(');
						info->nestedArgs = true;
					}
				}
				else
					++depth;
				break;
			}

			default:
			{
				if (cppIsident1 (c))
				{
					if (++identifierCount > 1)
						info->isKnrParamList = false;
					readIdentifier (token, c);
					if (isType (token, TOKEN_NAME)  &&  info->isNameCandidate)
						token->type = TOKEN_PAREN_NAME;
					else if (isType (token, TOKEN_KEYWORD))
					{
						info->isKnrParamList = false;
						info->isNameCandidate = false;
					}
				}
				else if (isInputLanguage(Lang_d) && c == '!')
				{ /* D template instantiation */
					info->isNameCandidate = false;
					info->isKnrParamList = false;
				}
				else
				{
					info->isParamList     = false;
					info->isKnrParamList  = false;
					info->isNameCandidate = false;
					info->invalidContents = true;
				}
				break;
			}
		}
		firstChar = false;
	} while (! info->nestedArgs  &&  depth > 0  &&
			 (info->isKnrParamList  ||  info->isNameCandidate));

	if (! info->nestedArgs) while (depth > 0)
	{
		skipToMatch ("()");
		--depth;
	}
	if (st->argEndPosition == 0)
		st->argEndPosition = mio_tell (File.mio);

	if (! info->isNameCandidate)
		initToken (token);

	return nextChar;
}

static void initParenInfo (parenInfo *const info)
{
	info->isParamList			= true;
	info->isKnrParamList		= true;
	info->isNameCandidate		= true;
	info->invalidContents		= false;
	info->nestedArgs			= false;
	info->parameterCount		= 0;
}

static void analyzeParens (statementInfo *const st)
{
	tokenInfo *const prev = prevToken (st, 1);

	if (! isType (prev, TOKEN_NONE))    /* in case of ignored enclosing macros */
	{
		tokenInfo *const token = activeToken (st);
		parenInfo info;
		int c;

		initParenInfo (&info);
		parseParens (st, &info);
		c = skipToNonWhite ();
		cppUngetc (c);
		if (info.invalidContents)
		{
			reinitStatement (st, false);
		}
		else if (info.isNameCandidate  &&  isType (token, TOKEN_PAREN_NAME)  &&
				 ! st->gotParenName  &&
				 (! info.isParamList || ! st->haveQualifyingName  ||
				  c == '('  ||
				  (c == '='  &&  st->implementation != IMP_VIRTUAL) ||
				  (st->declaration == DECL_NONE  &&  isOneOf (c, ",;"))))
		{
			token->type = TOKEN_NAME;
			processName (st);
			st->gotParenName = true;
			if (isInputLanguage(Lang_d) && c == '(' && isType (prev, TOKEN_NAME))
			{
				st->declaration = DECL_FUNCTION_TEMPLATE;
				copyToken (st->blockName, prev);
			}
		}
		else if (! st->gotArgs  &&  info.isParamList)
		{
			st->gotArgs = true;
			setToken (st, TOKEN_ARGS);
			advanceToken (st);
			analyzePostParens (st, &info);
		}
		else
			setToken (st, TOKEN_NONE);
	}
}

/*
*   Token parsing functions
*/

static void addContext (statementInfo *const st, const tokenInfo* const token)
{
	if (isType (token, TOKEN_NAME))
	{
		if (vStringLength (st->context->name) > 0)
		{
			if (isInputLanguage (Lang_c)  ||  isInputLanguage (Lang_cpp))
				vStringCatS (st->context->name, "::");
			else if (isInputLanguage (Lang_java) ||
					 isInputLanguage (Lang_d) || isInputLanguage (Lang_ferite) ||
					 isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala))
				vStringCatS (st->context->name, ".");
		}
		vStringCat (st->context->name, token->name);
		st->context->type = TOKEN_NAME;
	}
}

static bool inheritingDeclaration (declType decl)
{
	/* enum base types */
	if (decl == DECL_ENUM)
	{
		return (bool) (isInputLanguage (Lang_cpp) || isInputLanguage (Lang_csharp) ||
			isInputLanguage (Lang_d));
	}
	return (bool) (
		decl == DECL_CLASS ||
		decl == DECL_STRUCT ||
		decl == DECL_INTERFACE);
}

static void processColon (statementInfo *const st)
{
	int c = cppGetc ();
	const bool doubleColon = (bool) (c == ':');

	if (doubleColon)
	{
		setToken (st, TOKEN_DOUBLE_COLON);
		st->haveQualifyingName = false;
	}
	else
	{
		cppUngetc (c);
		if ((isInputLanguage (Lang_cpp) || isInputLanguage (Lang_csharp) || isInputLanguage (Lang_d) ||
			isInputLanguage (Lang_vala)) &&
			inheritingDeclaration (st->declaration))
		{
			readParents (st, ':');
		}
		else if (parentDecl (st) == DECL_STRUCT || parentDecl (st) == DECL_CLASS)
		{
			c = skipToOneOf (",;");
			if (c == ',')
				setToken (st, TOKEN_COMMA);
			else if (c == ';')
				setToken (st, TOKEN_SEMICOLON);
		}
		else
		{
			const tokenInfo *const prev  = prevToken (st, 1);
			const tokenInfo *const prev2 = prevToken (st, 2);
			if (prev->keyword == KEYWORD_DEFAULT ||
				prev2->keyword == KEYWORD_CASE ||
				st->parent != NULL)
			{
				reinitStatement (st, false);
			}
		}
	}
}

/*  Skips over any initializing value which may follow an '=' character in a
 *  variable definition.
 */
static int skipInitializer (statementInfo *const st)
{
	bool done = false;
	int c;

	while (! done)
	{
		c = skipToNonWhite ();

		if (c == EOF)
			longjmp (Exception, (int) ExceptionFormattingError);
		else switch (c)
		{
			case ',':
			case ';': done = true; break;

			case '0':
				if (st->implementation == IMP_VIRTUAL)
					st->implementation = IMP_PURE_VIRTUAL;
				break;

			case '[': skipToMatch ("[]"); break;
			case '(': skipToMatch ("()"); break;
			case '{': skipToMatch ("{}"); break;

			case '}':
				if (insideEnumBody (st))
					done = true;
				else if (! cppIsBraceFormat ())
				{
					verbose ("%s: unexpected closing brace at line %lu\n",
							getInputFileName (), getInputLineNumber ());
					longjmp (Exception, (int) ExceptionBraceFormattingError);
				}
				break;

			default: break;
		}
	}
	return c;
}

static void processInitializer (statementInfo *const st)
{
	const bool inEnumBody = insideEnumBody (st);
	const int c = skipInitializer (st);

	if (c == ';')
		setToken (st, TOKEN_SEMICOLON);
	else if (c == ',')
		setToken (st, TOKEN_COMMA);
	else if (c == '}'  &&  inEnumBody)
	{
		cppUngetc (c);
		setToken (st, TOKEN_COMMA);
	}
	if (st->scope == SCOPE_EXTERN)
		st->scope = SCOPE_GLOBAL;
}

static void parseIdentifier (statementInfo *const st, const int c)
{
	tokenInfo *const token = activeToken (st);

	readIdentifier (token, c);
	if (! isType (token, TOKEN_NONE))
		processToken (token, st);
}

static void parseGeneralToken (statementInfo *const st, const int c)
{
	const tokenInfo *const prev = prevToken (st, 1);

	if (cppIsident1(c))
	{
		parseIdentifier (st, c);
		if (isType (st->context, TOKEN_NAME) &&
			isType (activeToken (st), TOKEN_NAME) && isType (prev, TOKEN_NAME))
		{
			initToken (st->context);
		}
	}
	else if (isExternCDecl (st, c))
	{
		st->declaration = DECL_NOMANGLE;
		st->scope = SCOPE_GLOBAL;
	}
}

/*  Reads characters from the pre-processor and assembles tokens, setting
 *  the current statement state.
 */
static void nextToken (statementInfo *const st)
{
	int c;
	tokenInfo *token = activeToken (st);
	do
	{
		c = skipToNonWhite();
		switch (c)
		{
			case EOF: longjmp (Exception, (int) ExceptionEOF);  break;
			case '(': analyzeParens (st); token = activeToken (st); break;
			case '*': setToken (st, TOKEN_STAR);                break;
			case ',': setToken (st, TOKEN_COMMA);               break;
			case ':': processColon (st);                        break;
			case ';': setToken (st, TOKEN_SEMICOLON);           break;
			case '<': skipToMatch ("<>");                       break;
			case '=': processInitializer (st);                  break;
			case '[':
				/* Hack for Vala: [..] can be a function attribute.
				 * Seems not to have bad side effects, but have to test it more. */
				if (!isInputLanguage (Lang_vala))
					setToken (st, TOKEN_ARRAY);
				skipToMatch ("[]");
				break;
			case '{': setToken (st, TOKEN_BRACE_OPEN);          break;
			case '}': setToken (st, TOKEN_BRACE_CLOSE);         break;
			default:  parseGeneralToken (st, c);                break;
		}
	} while (isType (token, TOKEN_NONE));

	if (isType (token, TOKEN_SEMICOLON) && st->parent)
		st->parent->nSemicolons ++;

	/* We want to know about non-keyword variable types */
	if (TOKEN_NONE == st->firstToken->type)
	{
		if ((TOKEN_NAME == token->type) || isDataTypeKeyword(token))
			copyToken(st->firstToken, token);
	}
}

/*
*   Scanning support functions
*/
static unsigned int contextual_fake_count = 0;
static statementInfo *CurrentStatement = NULL;

static statementInfo *newStatement (statementInfo *const parent)
{
	statementInfo *const st = xMalloc (1, statementInfo);
	unsigned int i;

	for (i = 0  ;  i < (unsigned int) NumTokens  ;  ++i)
		st->token [i] = newToken ();

	st->context = newToken ();
	st->blockName = newToken ();
	st->parentClasses = vStringNew ();
	st->firstToken = newToken();

	initStatement (st, parent);
	CurrentStatement = st;

	return st;
}

static void deleteStatement (void)
{
	statementInfo *const st = CurrentStatement;
	statementInfo *const parent = st->parent;
	unsigned int i;

	for (i = 0  ;  i < (unsigned int) NumTokens  ;  ++i)
	{
		deleteToken (st->token [i]);       st->token [i] = NULL;
	}
	deleteToken (st->blockName);           st->blockName = NULL;
	deleteToken (st->context);             st->context = NULL;
	vStringDelete (st->parentClasses);     st->parentClasses = NULL;
	deleteToken(st->firstToken);
	eFree (st);
	CurrentStatement = parent;
}

static void deleteAllStatements (void)
{
	while (CurrentStatement != NULL)
		deleteStatement ();
}

static bool isStatementEnd (const statementInfo *const st)
{
	const tokenInfo *const token = activeToken (st);
	bool isEnd;

	if (isType (token, TOKEN_SEMICOLON))
		isEnd = true;
	else if (isType (token, TOKEN_BRACE_CLOSE))
		/* Java, D, C#, Vala do not require semicolons to end a block. Neither do
		 * C++ namespaces. All other blocks require a semicolon to terminate them.
		 */
		isEnd = (bool) (isInputLanguage (Lang_java) || isInputLanguage (Lang_d) ||
						isInputLanguage (Lang_csharp) || isInputLanguage (Lang_vala) ||
						! isContextualStatement (st));
	else
		isEnd = false;

	return isEnd;
}

static void checkStatementEnd (statementInfo *const st)
{
	const tokenInfo *const token = activeToken (st);
	bool comma = isType (token, TOKEN_COMMA);

	if (comma || isStatementEnd (st))
	{
		reinitStatementWithToken (st, activeToken (st), comma);

		DebugStatement ( if (debug (DEBUG_PARSE)) printf ("<ES>"); )
		cppEndStatement ();
	}
	else
	{
		cppBeginStatement ();
		advanceToken (st);
	}
}

static void nest (statementInfo *const st, const unsigned int nestLevel)
{
	switch (st->declaration)
	{
		case DECL_CLASS:
		case DECL_ENUM:
		case DECL_INTERFACE:
		case DECL_NAMESPACE:
		case DECL_NOMANGLE:
		case DECL_STRUCT:
		case DECL_UNION:
			createTags (nestLevel, st);
			break;
		default:
			skipToMatch ("{}");
			break;
	}
	advanceToken (st);
	setToken (st, TOKEN_BRACE_CLOSE);
}

static void tagCheck (statementInfo *const st)
{
	const tokenInfo *const token = activeToken (st);
	const tokenInfo *const prev  = prevToken (st, 1);
	const tokenInfo *const prev2 = prevToken (st, 2);

	switch (token->type)
	{
		case TOKEN_NAME:
		{
			if (insideEnumBody (st) &&
				/* Java enumerations can contain members after a semicolon */
				(! isInputLanguage(Lang_java) || st->parent->nSemicolons < 1))
				qualifyEnumeratorTag (st, token);
			break;
		}
#if 0
		case TOKEN_PACKAGE:
		{
			if (st->haveQualifyingName)
				makeTag (token, st, false, TAG_PACKAGE);
			break;
		}
#endif
		case TOKEN_BRACE_OPEN:
		{
			if (isType (prev, TOKEN_ARGS))
			{
				if (st->declaration == DECL_FUNCTION_TEMPLATE)
					qualifyFunctionTag (st, st->blockName);
				else if (st->haveQualifyingName)
				{
					if (isType (prev2, TOKEN_NAME))
						copyToken (st->blockName, prev2);
					/* D structure templates */
					if (isInputLanguage (Lang_d) &&
						(st->declaration == DECL_CLASS || st->declaration == DECL_STRUCT ||
						st->declaration == DECL_INTERFACE || st->declaration == DECL_NAMESPACE))
						qualifyBlockTag (st, prev2);
					else
					{
						st->declaration = DECL_FUNCTION;
						qualifyFunctionTag (st, prev2);
					}
				}
			}
			else if (isContextualStatement (st))
			{
				tokenInfo *name_token = (tokenInfo *)prev;
				bool free_name_token = false;

				/* C++ 11 allows class <name> final { ... } */
				if (isInputLanguage (Lang_cpp) && isType (prev, TOKEN_NAME) &&
					strcmp("final", vStringValue(prev->name)) == 0 &&
					isType(prev2, TOKEN_NAME))
				{
					name_token = (tokenInfo *)prev2;
					copyToken (st->blockName, name_token);
				}
				else if (isType (name_token, TOKEN_NAME))
				{
					if (!isInputLanguage (Lang_vala))
						copyToken (st->blockName, name_token);
					else
					{
						switch (st->declaration)
						{
							case DECL_CLASS:
							case DECL_ENUM:
							case DECL_INTERFACE:
							case DECL_NAMESPACE:
							case DECL_STRUCT:
								copyToken (st->blockName, name_token);
								break;

							/* anything else can be a property */
							default:
								/* makeTag (prev, st, false, TAG_PROPERTY); */
								/* FIXME: temporary hack to get properties shown */
								makeTag (prev, st, false, TAG_FIELD);
								break;
						}
					}
				}
				else if (isInputLanguage (Lang_csharp))
					makeTag (prev, st, false, TAG_PROPERTY);
				else
				{
					tokenInfo *contextual_token = (tokenInfo *)prev;
					if(isContextualKeyword (contextual_token))
					{
						char buffer[64];

						name_token = newToken ();
						free_name_token = true;
						copyToken (name_token, contextual_token);

						sprintf(buffer, "anon_%s_%d", name_token->name->buffer, contextual_fake_count++);
						vStringClear(name_token->name);
						vStringCatS(name_token->name, buffer);

						name_token->type = TOKEN_NAME;
						name_token->keyword	= KEYWORD_NONE;

						advanceToken (st);
						contextual_token = activeToken (st);
						copyToken (contextual_token, token);
						copyToken ((tokenInfo *const)token, name_token);
						copyToken (st->blockName, name_token);
						copyToken (st->firstToken, name_token);
					}
				}
				qualifyBlockTag (st, name_token);
				if (free_name_token)
					deleteToken (name_token);
			}
			break;
		}
		case TOKEN_ARRAY:
		case TOKEN_SEMICOLON:
		case TOKEN_COMMA:
		{
			if (insideEnumBody (st) &&
				/* Java enumerations can contain members after a semicolon */
				(! isInputLanguage (Lang_java) || st->parent->nSemicolons < 2))
				;
			else if (isType (prev, TOKEN_NAME))
			{
				if (isContextualKeyword (prev2))
					makeTag (prev, st, true, TAG_EXTERN_VAR);
				else
					qualifyVariableTag (st, prev);
			}
			else if (isType (prev, TOKEN_ARGS)  &&  isType (prev2, TOKEN_NAME))
			{
				qualifyFunctionDeclTag (st, prev2);
			}
			break;
		}
		default:
			break;
	}
}

/*  Parses the current file and decides whether to write out and tags that
 *  are discovered.
 */
static void createTags (const unsigned int nestLevel,
						statementInfo *const parent)
{
	statementInfo *const st = newStatement (parent);

	DebugStatement ( if (nestLevel > 0) debugParseNest (true, nestLevel); )
	while (true)
	{
		tokenInfo *token;

		nextToken (st);
		token = activeToken (st);
		if (isType (token, TOKEN_BRACE_CLOSE))
		{
			if (nestLevel > 0)
				break;
			else
			{
				verbose ("%s: unexpected closing brace at line %lu\n",
						getInputFileName (), getInputLineNumber ());
				longjmp (Exception, (int) ExceptionBraceFormattingError);
			}
		}
		else if (isType (token, TOKEN_DOUBLE_COLON))
		{
			addContext (st, prevToken (st, 1));
			advanceToken (st);
		}
		else
		{
			tagCheck (st);/* this can add new token */
			if (isType (activeToken (st), TOKEN_BRACE_OPEN))
				nest (st, nestLevel + 1);
			checkStatementEnd (st);
		}
	}
	deleteStatement ();
	DebugStatement ( if (nestLevel > 0) debugParseNest (false, nestLevel - 1); )
}

static bool findCTags (const unsigned int passCount)
{
	exception_t exception;
	bool retry;

	contextual_fake_count = 0;

	Assert (passCount < 3);
	cppInit ((bool) (passCount > 1), isInputLanguage (Lang_csharp), isInputLanguage (Lang_cpp), &(CKinds [CK_DEFINE]));

	exception = (exception_t) setjmp (Exception);
	retry = false;

	if (exception == ExceptionNone)
	{
		createTags (0, NULL);
	}
	else
	{
		deleteAllStatements ();
		if (exception == ExceptionBraceFormattingError  &&  passCount == 1)
		{
			retry = true;
			verbose ("%s: retrying file with fallback brace matching algorithm\n",
					getInputFileName ());
		}
	}
	cppTerminate ();
	return retry;
}

static void buildKeywordHash (const langType language, unsigned int idx)
{
	const size_t count = ARRAY_SIZE (KeywordTable);
	size_t i;
	for (i = 0  ;  i < count  ;  ++i)
	{
		const keywordDesc* const p = &KeywordTable [i];
		if (p->isValid [idx])
			addKeyword (p->name, language, (int) p->id);
	}
}

static void initializeCParser (const langType language)
{
	Lang_c = language;
	buildKeywordHash (language, 0);
}

static void initializeCppParser (const langType language)
{
	Lang_cpp = language;
	buildKeywordHash (language, 1);
}

static void initializeJavaParser (const langType language)
{
	Lang_java = language;
	buildKeywordHash (language, 3);
}

static void initializeDParser (const langType language)
{
	/* treat these like const - some are for parsing like const(Type), some are just
	 * function attributes */
	const char *const_aliases[] = {"immutable", "nothrow", "pure", "shared", NULL};
	const char **s;

	Lang_d = language;
	buildKeywordHash (language, 6);

	for (s = const_aliases; *s != NULL; s++)
	{
		addKeyword (*s, language, KEYWORD_CONST);
	}
	/* other keyword aliases */
	addKeyword ("alias", language, KEYWORD_TYPEDEF);
	/* skip 'static assert(...)' like 'static if (...)' */
	addKeyword ("assert", language, KEYWORD_IF);
	addKeyword ("unittest", language, KEYWORD_BODY);	/* ignore */
	addKeyword ("version", language, KEYWORD_NAMESPACE);	/* parse block */
}

static void initializeGLSLParser (const langType language)
{
	Lang_glsl = language;
	buildKeywordHash (language, 0); /* C keywords */
}

static void initializeFeriteParser (const langType language)
{
	Lang_ferite = language;
	buildKeywordHash (language, 1);	/* C++ keywords */
}

static void initializeCsharpParser (const langType language)
{
	Lang_csharp = language;
	buildKeywordHash (language, 2);
}

static void initializeValaParser (const langType language)
{
	Lang_vala = language;
	buildKeywordHash (language, 5);

	/* keyword aliases */
	addKeyword ("ensures", language, KEYWORD_ATTRIBUTE);	/* ignore */
	addKeyword ("errordomain", language, KEYWORD_ENUM); /* looks like enum */
	addKeyword ("requires", language, KEYWORD_ATTRIBUTE);	/* ignore */
}

extern parserDefinition* CParser (void)
{
	static const char *const extensions [] = { "c", "pc", "sc", NULL };
	parserDefinition* def = parserNew ("C");
	def->kinds      = CKinds;
	def->kindCount  = ARRAY_SIZE (CKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeCParser;
	return def;
}

extern parserDefinition* CppParser (void)
{
	static const char *const extensions [] = {
		"c++", "cc", "cp", "cpp", "cxx", "h", "h++", "hh", "hp", "hpp", "hxx",
		"i",
#ifndef CASE_INSENSITIVE_FILENAMES
		"C", "H",
#endif
		NULL
	};
	parserDefinition* def = parserNew ("C++");
	def->kinds      = CKinds;
	def->kindCount  = ARRAY_SIZE (CKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeCppParser;
	return def;
}

extern parserDefinition* JavaParser (void)
{
	static const char *const extensions [] = { "java", NULL };
	parserDefinition* def = parserNew ("Java");
	def->kinds      = JavaKinds;
	def->kindCount  = ARRAY_SIZE (JavaKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeJavaParser;
	return def;
}

extern parserDefinition* DParser (void)
{
	static const char *const extensions [] = { "d", "di", NULL };
	parserDefinition* def = parserNew ("D");
	def->kinds      = DKinds;
	def->kindCount  = ARRAY_SIZE (DKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeDParser;
	return def;
}

extern parserDefinition* GLSLParser (void)
{
	static const char *const extensions [] = { "glsl", "frag", "vert", NULL };
	parserDefinition* def = parserNew ("GLSL");
	def->kinds      = CKinds;
	def->kindCount  = ARRAY_SIZE (CKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeGLSLParser;
	return def;
}

extern parserDefinition* FeriteParser (void)
{
	static const char *const extensions [] = { "fe", NULL };
	parserDefinition* def = parserNew ("Ferite");
	def->kinds      = CKinds;
	def->kindCount  = ARRAY_SIZE (CKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeFeriteParser;
	return def;
}

extern parserDefinition* CsharpParser (void)
{
	static const char *const extensions [] = { "cs", NULL };
	parserDefinition* def = parserNew ("C#");
	def->kinds      = CsharpKinds;
	def->kindCount  = ARRAY_SIZE (CsharpKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeCsharpParser;
	return def;
}

extern parserDefinition* ValaParser (void)
{
	static const char *const extensions [] = { "vala", NULL };
	parserDefinition* def = parserNew ("Vala");
	def->kinds      = ValaKinds;
	def->kindCount  = ARRAY_SIZE (ValaKinds);
	def->extensions = extensions;
	def->parser2    = findCTags;
	def->initialize = initializeValaParser;
	return def;
}
