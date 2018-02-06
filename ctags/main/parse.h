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

#include "parsers.h"  /* contains list of parsers */
#include "strlist.h"
#include "entry.h"

/*
*   MACROS
*/
#define LANG_AUTO   (-1)
#define LANG_IGNORE (-2)

/*
*   DATA DECLARATIONS
*/
typedef int langType;

typedef void (*createRegexTag) (const vString* const name);
typedef void (*simpleParser) (void);
typedef bool (*rescanParser) (const unsigned int passCount);
typedef void (*parserInitialize) (langType language);
typedef int (*tagEntryFunction) (const tagEntryInfo *const tag, void *user_data);

typedef enum {
	METHOD_NOT_CRAFTED    = 1 << 0,
	METHOD_REGEX          = 1 << 1,
	METHOD_XCMD           = 1 << 2,
	METHOD_XCMD_AVAILABLE = 1 << 3,
	METHOD_XPATH          = 1 << 4,
} parsingMethod;

typedef struct {
	const char *const regex;
	const char *const name;
	const char *const kinds;
	const char *const flags;
	bool *disabled;
} tagRegexTable;

typedef struct {
	const char *name;
	const int id;
} keywordTable;

typedef struct {
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
	unsigned int method;           /* See PARSE__... definitions above */

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
	unsigned int tagRegexInstalled:1; /* tagRegexTable is installed or not. */
	unsigned int keywordInstalled:1;  /* keywordTable is installed or not. */
} parserDefinition;

typedef parserDefinition* (parserDefinitionFunc) (void);

typedef struct {
	int start;      /* character index in line where match starts */
	size_t length;  /* length of match */
} regexMatch;

typedef void (*regexCallback) (const char *line, const regexMatch *matches, unsigned int count);

/*
*   FUNCTION PROTOTYPES
*/

/* Each parsers' definition function is called. The routine is expected to
 * return a structure allocated using parserNew(). This structure must,
 * at minimum, set the `parser' field.
 */
extern parserDefinitionFunc PARSER_LIST;
extern parserDefinition** LanguageTable;
extern unsigned int LanguageCount;
/* Legacy interface */
extern bool includingDefineTags (void);
extern void processLegacyKindOption (const char *const parameter);

/* Language processing and parsing */
extern void makeSimpleTag (const vString* const name, kindOption* const kinds, const int kind);

extern parserDefinition* parserNew (const char* name);
extern parserDefinition* parserNewFull (const char* name, char fileKind);
extern const char *getLanguageName (const langType language);
extern kindOption* getLanguageFileKind (const langType language);
extern langType getNamedLanguage (const char *const name);
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
extern void freeParserResources (void);
extern void processLanguageDefineOption (const char *const option, const char *const parameter);
extern bool processKindOption (const char *const option, const char *const parameter);
extern void printKindOptions (void);
extern bool parseFile (const char *const fileName);

extern void installKeywordTable (const langType language);

/* Regex interface */
#ifdef HAVE_REGEX
extern void findRegexTags (void);
extern bool matchRegex (const vString* const line, const langType language);
#endif
extern bool processRegexOption (const char *const option, const char *const parameter);
extern void addLanguageRegex (const langType language, const char* const regex);
extern void installTagRegexTable (const langType language);
extern void addTagRegex (const langType language, const char* const regex, const char* const name, const char* const kinds, const char* const flags);
extern void addCallbackRegex (const langType language, const char* const regex, const char* flags, const regexCallback callback);
extern void disableRegexKinds (const langType language CTAGS_ATTR_UNUSED);
extern bool enableRegexKind (const langType language, const int kind, const bool mode);
extern void printRegexKindOptions (const langType language);
extern void printRegexKinds (const langType language, bool indent);
extern void freeRegexResources (void);
extern void checkRegex (void);


/* Extra stuff for Tag Manager */
extern tagEntryFunction TagEntryFunction;
extern void *TagEntryUserData;
extern void setTagEntryFunction(tagEntryFunction entry_function, void *user_data);

#endif  /* CTAGS_MAIN_PARSE_H */
