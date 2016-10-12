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

/*
*   MACROS
*/
#define LANG_AUTO   (-1)
#define LANG_IGNORE (-2)

/*
*   DATA DECLARATIONS
*/

typedef void (*createRegexTag) (const vString* const name);
typedef void (*simpleParser) (void);
typedef bool (*rescanParser) (const unsigned int passCount);
typedef void (*parserInitialize) (langType language);

typedef enum {
	METHOD_NOT_CRAFTED    = 1 << 0,
	METHOD_REGEX          = 1 << 1,
	METHOD_XCMD           = 1 << 2,
	METHOD_XCMD_AVAILABLE = 1 << 3,
	METHOD_XPATH          = 1 << 4,
} parsingMethod;

typedef struct {
	const char *const regex;
	const char* const name;
	const char* const kinds;
	const char *const flags;
	bool    *disabled;
} tagRegexTable;

typedef struct {
	const char *name;
	const int id;
} keywordTable;

struct sParserDefinition {
	/* defined by parser */
	char* name;                    /* name of language */
	kindOption* kinds;             /* tag kinds handled by parser */
	unsigned int kindCount;        /* size of `kinds' list */
	kindOption* fileKind;          /* kind for overriding the default fileKind */
	const char *const *extensions; /* list of default extensions */
	const char *const *patterns;   /* list of default file name patterns */
	parserInitialize initialize;   /* initialization routine, if needed */
	simpleParser parser;           /* simple parser (common case) */
	rescanParser parser2;          /* rescanning parser (unusual case) */
	unsigned int method;           /* See METHOD_ definitions above */

	/* used internally */
	unsigned int id;                /* id assigned to language */
	bool enabled;                   /* currently enabled? */
	stringList* currentPatterns;    /* current list of file name patterns */
	stringList* currentExtensions;  /* current list of extensions */
	tagRegexTable *tagRegexTable;
	unsigned int tagRegexCount;
	const keywordTable *keywordTable;
	unsigned int keywordCount;

	unsigned int initialized:1;    /* initialize() is called or not */
	subparser *subparsers;	/* The parsers on this list must be initialized when
				   this parser is initialized. */
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
extern parserDefinition** LanguageTable;
extern parserDefinitionFunc PARSER_LIST;
/* Legacy interface */
extern bool includingDefineTags (void);


/* Language processing and parsing */
extern int makeSimpleTag (const vString* const name, kindOption* const kinds, const int kind);

extern parserDefinition* parserNew (const char* name);
extern parserDefinition* parserNewFull (const char* name, char fileKind);
extern const char *getLanguageName (const langType language);
extern kindOption* getLanguageFileKind (const langType language);
extern langType getNamedLanguage (const char *const name, size_t len);
extern langType getFileLanguage (const char *const fileName);
extern void installLanguageMapDefault (const langType language);
extern void installLanguageMapDefaults (void);
extern void clearLanguageMap (const langType language);
extern void addLanguageExtensionMap (const langType language, const char* extension);
extern void addLanguagePatternMap (const langType language, const char* ptrn);
extern void printLanguageMap (const langType language);
extern void enableLanguages (const bool state);
extern void enableLanguage (const langType language, const bool state);
extern void initializeParsing (void);
extern void initializeParser (langType language);
extern unsigned int countParsers (void);
extern void processLanguageDefineOption (const char *const option, const char *const parameter);
extern bool processKindOption (const char *const option, const char *const parameter);

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
extern void foreachRegexKinds (const langType language, bool (* func) (kindOption*, void*), void *data);
extern void freeRegexResources (void);
extern bool checkRegex (void);
extern void useRegexMethod (const langType language);
extern void printRegexFlags (void);
extern bool hasScopeActionInRegex (const langType language);

#endif  /* CTAGS_MAIN_PARSE_H */
