/*
*   Copyright (c) 1998-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Private definitions for parsing support.
*/
#ifndef CTAGS_MAIN_PARSE_H
#define CTAGS_MAIN_PARSE_H

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */
#include "types.h"

#include "dependency.h"
#include "field.h"
#include "kind.h"
#include "parsers.h"  /* contains list of parsers */
#include "strlist.h"
#include "ctags-api.h"

#ifdef HAVE_LIBXML
#include <libxml/xpath.h>
#include <libxml/tree.h>
#else
#define xmlNode void
#define xmlXPathCompExpr void
#define xmlXPathContext void
#endif

/*
*   MACROS
*/
#define LANG_AUTO   (-1)
#define LANG_IGNORE (-2)

/*
*   DATA DECLARATIONS
*/
typedef enum {
	RESCAN_NONE,   /* No rescan needed */
	RESCAN_FAILED, /* Scan failed, clear out tags added, rescan */
	RESCAN_APPEND  /* Scan succeeded, rescan */
} rescanReason;

typedef void (*createRegexTag) (const vString* const name);
typedef void (*simpleParser) (void);
typedef rescanReason (*rescanParser) (const unsigned int passCount);
typedef void (*parserInitialize) (langType language);

/* Per language finalizer is called anytime when ctags exits.
   (Exceptions are a kind of options are given when invoked. Here
   options are: --version, --help, --list-*, and so on.)

   The finalizer is called even when the initializer of the
   same parser is called or not. However, the finalizer can know
   whether the assoiciated initializer is invoked or not with the
   second parameter: INITIALIZED. If it is true, the initializer
   is called. */
typedef void (*parserFinalize) (langType language, bool initialized);

typedef const char * (*selectLanguage) (MIO *);

typedef enum {
	METHOD_NOT_CRAFTED    = 1 << 0,
	METHOD_REGEX          = 1 << 1,
	METHOD_XCMD           = 1 << 2,
	METHOD_XCMD_AVAILABLE = 1 << 3,
	METHOD_XPATH          = 1 << 4,
	METHOD_YAML           = 1 << 5,
} parsingMethod;

typedef struct {
	const char *const regex;
	const char* const name;
	const char* const kinds;
	const char *const flags;
	bool    *disabled;
} tagRegexTable;

typedef struct sTagXpathMakeTagSpec {
	int   kind;
	int   role;
	/* If make is NULL, just makeTagEntry is used instead. */
	void (*make) (xmlNode *node,
		      const struct sTagXpathMakeTagSpec *spec,
		      tagEntryInfo *tag,
		      void *userData);
} tagXpathMakeTagSpec;

typedef struct sTagXpathRecurSpec {
	void (*enter) (xmlNode *node,
		       const struct sTagXpathRecurSpec *spec,
		       xmlXPathContext *ctx,
		       void *userData);

	int  nextTable;		/* A parser can use this field any purpose.
				   main/lxpath part doesn't touch this. */

} tagXpathRecurSpec;

typedef struct sTagXpathTable
{
	const char *const xpath;
	enum  { LXPATH_TABLE_DO_MAKE, LXPATH_TABLE_DO_RECUR } specType;
	union {
		tagXpathMakeTagSpec makeTagSpec;
		tagXpathRecurSpec   recurSpec;
	} spec;
	xmlXPathCompExpr* xpathCompiled;
} tagXpathTable;

typedef struct sTagXpathTableTable {
	tagXpathTable *table;
	unsigned int   count;
} tagXpathTableTable;


typedef struct {
	const char *name;
	const int id;
} keywordTable;

struct sParserDefinition {
	/* defined by parser */
	char* name;                    /* name of language */
	kindDefinition* kindTable;     /* tag kinds handled by parser */
	unsigned int kindCount;        /* size of `kinds' list */
	kindDefinition* fileKind;      /* kind for overriding the default fileKind */
	const char *const *extensions; /* list of default extensions */
	const char *const *patterns;   /* list of default file name patterns */
	const char *const *aliases;    /* list of default aliases (alternative names) */
	parserInitialize initialize;   /* initialization routine, if needed */
	parserFinalize finalize;       /* finalize routine, if needed */
	simpleParser parser;           /* simple parser (common case) */
	rescanParser parser2;          /* rescanning parser (unusual case) */
	selectLanguage* selectLanguage; /* may be used to resolve conflicts */
	unsigned int method;           /* See METHOD_ definitions above */
	bool useCork;
	bool allowNullTag;
	bool requestAutomaticFQTag;
	tagRegexTable *tagRegexTable;
	unsigned int tagRegexCount;
	const keywordTable *keywordTable;
	unsigned int keywordCount;
	tagXpathTableTable *tagXpathTableTable;
	unsigned int tagXpathTableCount;
	bool invisible;
	fieldDefinition *fieldTable;
	unsigned int fieldCount;

	parserDependency * dependencies;
	unsigned int dependencyCount;
	/* used internally */
	langType id;		    /* id assigned to language */
	unsigned int enabled:1;	       /* currently enabled? */
	unsigned int initialized:1;    /* initialize() is called or not */
	unsigned int pseudoTagPrinted:1;   /* pseudo tags about this parser
					      is emitted or not. */
	subparser *subparsers;	/* The parsers on this list must be initialized when
				   this parser is initialized. */

	stringList* currentPatterns;   /* current list of file name patterns */
	stringList* currentExtensions; /* current list of extensions */
	stringList* currentAliases;    /* current list of aliases */
	unsigned int anonumousIdentiferId; /* managed by anon* functions */
};

typedef parserDefinition* (parserDefinitionFunc) (void);

typedef struct {
	size_t start;   /* character index in line where match starts */
	size_t length;  /* length of match */
} regexMatch;

typedef void (*regexCallback) (const char *line, const regexMatch *matches, unsigned int count,
			       void *userData);

typedef enum {
	LMAP_PATTERN   = 1 << 0,
	LMAP_EXTENSION = 1 << 1,
	LMAP_ALL       = LMAP_PATTERN | LMAP_EXTENSION,
} langmapType;


/*
*   FUNCTION PROTOTYPES
*/

/* Each parsers' definition function is called. The routine is expected to
 * return a structure allocated using parserNew(). This structure must,
 * at minimum, set the `parser' field.
 */
extern parserDefinitionFunc PARSER_LIST;
#ifdef HAVE_LIBXML
extern parserDefinitionFunc XML_PARSER_LIST;
#endif
#ifdef HAVE_LIBYAML
extern parserDefinitionFunc YAML_PARSER_LIST;
#endif


/* Language processing and parsing */
extern int makeSimpleTag (const vString* const name, const int kind);
extern int makeSimpleRefTag (const vString* const name, const int kindIndexS,
			     int roleIndex);
extern parserDefinition* parserNew (const char* name);
extern parserDefinition* parserNewFull (const char* name, char fileKind);
extern kindDefinition* getLanguageKind(const langType language, int kindIndex);
extern bool doesLanguageAllowNullTag (const langType language);
extern bool doesLanguageRequestAutomaticFQTag (const langType language);
extern const char *getLanguageName (const langType language);
extern kindDefinition* getLanguageFileKind (const langType language);
extern langType getNamedLanguage (const char *const name, size_t len);
extern langType getFileLanguage (const char *const fileName);
extern langType getLanguageForCommand (const char *const command, langType startFrom);
extern langType getLanguageForFilename (const char *const filename, langType startFrom);
extern bool isLanguageEnabled (const langType language);
extern bool isLanguageKindEnabled (const langType language, int kindIndex);

extern void installLanguageMapDefault (const langType language);
extern void installLanguageMapDefaults (void);
extern void clearLanguageMap (const langType language);
extern bool removeLanguageExtensionMap (const langType language, const char *const extension);
extern void addLanguageExtensionMap (const langType language, const char* extension,
				     bool exclusiveInAllLanguages);
extern bool removeLanguagePatternMap (const langType language, const char *const pattern);
extern void addLanguagePatternMap (const langType language, const char* ptrn,
				   bool exclusiveInAllLanguages);

extern void installLanguageAliasesDefault (const langType language);
extern void installLanguageAliasesDefaults (void);
extern void clearLanguageAliases (const langType language);
extern void addLanguageAlias (const langType language, const char* alias);

extern void printLanguageMap (const langType language, FILE *fp);
extern void printLanguageMaps (const langType language, langmapType type);
extern void enableLanguages (const bool state);
extern void enableLanguage (const langType language, const bool state);
extern void initializeParsing (void);
extern void initializeParser (langType language);
extern unsigned int countParsers (void);
extern void freeParserResources (void);
extern void printLanguageFileKind (const langType language);
extern void printLanguageKinds (const langType language, bool allKindFields);
extern void printLanguageRoles (const langType language, const char* letters);
extern void printLanguageAliases (const langType language);
extern void printLanguageList (void);
extern bool doesParserRequireMemoryStream (const langType language);
extern bool parseFile (const char *const fileName);
extern bool runParserInNarrowedInputStream (const langType language,
					       unsigned long startLine, int startCharOffset,
					       unsigned long endLine, int endCharOffset,
					       unsigned long sourceLineOffset);
#ifdef CTAGS_LIB
extern void createTagsWithFallback(unsigned char *buffer, size_t bufferSize,
	const char *fileName, const langType language,
	tagEntryFunction tagCallback, passStartCallback passCallback,
	void *userData);
extern const parserDefinition *getParserDefinition (langType language);
#endif

#ifdef HAVE_ICONV
extern void freeEncodingResources (void);
#endif


/* Regex interface */
extern void findRegexTags (void);
extern void findRegexTagsMainloop (int (* driver)(void));
extern bool matchRegex (const vString* const line, const langType language);
extern void addLanguageRegex (const langType language, const char* const regex);
extern void addTagRegex (const langType language, const char* const regex,
			 const char* const name, const char* const kinds, const char* const flags,
			 bool *disabled);
extern void addCallbackRegex (const langType language, const char *const regexo, const char *const flags,
			      const regexCallback callback, bool *disabled, void *userData);
extern void resetRegexKinds (const langType language, bool mode);
extern bool enableRegexKind (const langType language, const int kind, const bool mode);
extern bool enableRegexKindLong (const langType language, const char *kindLong, const bool mode);
extern bool isRegexKindEnabled (const langType language, const int kind);
extern bool hasRegexKind (const langType language, const int kind);
extern void printRegexKinds (const langType language, bool allKindFields, bool indent,
			     bool tabSeparated);
extern void foreachRegexKinds (const langType language, bool (* func) (kindDefinition*, void*), void *data);
extern void freeRegexResources (void);
extern bool checkRegex (void);
extern void useRegexMethod (const langType language);
extern void printRegexFlags (void);
extern bool hasScopeActionInRegex (const langType language);

#ifdef HAVE_COPROC
extern bool invokeXcmd (const char* const fileName, const langType language);
#endif
extern void addLanguageXcmd (const langType language, const char* const path);
extern void addTagXcmd (const langType language, vString* pathvstr, const char* flaggs);
extern void resetXcmdKinds (const langType language, bool mode);
extern bool enableXcmdKind (const langType language, const int kind, const bool mode);
extern bool enableXcmdKindLong (const langType language, const char *kindLong, const bool mode);
extern bool isXcmdKindEnabled (const langType language, const int kind);
extern bool hasXcmdKind (const langType language, const int kind);
extern void printXcmdKinds (const langType language, bool allKindFields, bool indent,
			    bool tabSeparated);
extern void foreachXcmdKinds (const langType language, bool (* func) (kindDefinition*, void*), void *data);
extern void freeXcmdResources (void);
extern void useXcmdMethod (const langType language);
extern void notifyAvailabilityXcmdMethod (const langType language);

/* Xpath interface */
extern void findXMLTags (xmlXPathContext *ctx, xmlNode *root,
			 const tagXpathTableTable *xpathTableTable,
			 const kindDefinition* const kinds, void *userData);
extern void addTagXpath (const langType language, tagXpathTable *xpathTable);


extern bool makeKindSeparatorsPseudoTags (const langType language,
					     const ptagDesc *pdesc);
extern bool makeKindDescriptionsPseudoTags (const langType language,
					       const ptagDesc *pdesc);

extern void anonGenerate (vString *buffer, const char *prefix, int kind);

#endif  /* CTAGS_MAIN_PARSE_H */
