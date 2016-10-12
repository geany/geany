/*
*   Copyright (c) 1996-2003, Darren Hiebert
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for managing input languages and
*   dispatching files to the appropriate language parser.
*/

/*
*   INCLUDE FILES
*/
#include "general.h"  /* must always come first */

#include <string.h>

#include "debug.h"
#include "entry.h"
#include "flags.h"
#include "keyword.h"
#include "main.h"
#define OPTION_WRITE
#include "options.h"
#include "parsers.h"
#include "promise.h"
#include "ptag.h"
#include "read.h"
#include "routines.h"
#include "vstring.h"
#ifdef HAVE_ICONV
# include "mbcs.h"
#endif
#include "xtag.h"

/*
 * FUNCTION PROTOTYPES
 */

static void installKeywordTable (const langType language);
static void installTagRegexTable (const langType language);

/*
*   DATA DEFINITIONS
*/
static parserDefinitionFunc* BuiltInParsers[] = { PARSER_LIST };
parserDefinition** LanguageTable = NULL;
static unsigned int LanguageCount = 0;
static kindOption defaultFileKind = {
	.enabled     = false,
	.letter      = KIND_FILE_DEFAULT,
	.name        = KIND_FILE_DEFAULT_LONG,
	.description = KIND_FILE_DEFAULT_LONG,
};

/*
*   FUNCTION DEFINITIONS
*/

extern unsigned int countParsers (void)
{
	return LanguageCount;
}

extern int makeSimpleTag (
		const vString* const name, kindOption* const kinds, const int kind)
{
	int r = CORK_NIL;

	if (name != NULL  &&  vStringLength (name) > 0)
	{
		tagEntryInfo e;
		initTagEntry (&e, vStringValue (name), & kinds [kind]);

		r = makeTagEntry (&e);
	}
	return r;
}


/*
*   parserDescription mapping management
*/

extern parserDefinition* parserNew (const char* name)
{
	return parserNewFull (name, 0);
}

extern parserDefinition* parserNewFull (const char* name, char fileKind)
{
	parserDefinition* result = xCalloc (1, parserDefinition);
	result->name = eStrdup (name);

	/* TODO: implement custom file kind */
	result->fileKind = &defaultFileKind;

	result->enabled = true;
	return result;
}

extern const char *getLanguageName (const langType language)
{
	const char* result;
	if (language == LANG_IGNORE)
		result = "unknown";
	else
	{
		Assert (0 <= language  &&  language < (int) LanguageCount);
		result = LanguageTable [language]->name;
	}
	return result;
}

extern kindOption* getLanguageFileKind (const langType language)
{
	kindOption* kind;

	Assert (0 <= language  &&  language < (int) LanguageCount);

	kind = LanguageTable [language]->fileKind;

	Assert (kind != KIND_NULL);

	return kind;
}

extern langType getNamedLanguage (const char *const name, size_t len)
{
	langType result = LANG_IGNORE;
	unsigned int i;
	Assert (name != NULL);

	for (i = 0  ;  i < LanguageCount  &&  result == LANG_IGNORE  ;  ++i)
	{
		const parserDefinition* const lang = LanguageTable [i];
		if (lang->name != NULL)
		{
			if (len == 0)
			{
				if (strcasecmp (name, lang->name) == 0)
					result = i;
			}
			else
			{
				vString* vstr = vStringNewInit (name);
				vStringTruncate (vstr, len);

				if (strcasecmp (vStringValue (vstr), lang->name) == 0)
					result = i;
				vStringDelete (vstr);
			}
		}
	}
	return result;
}

static langType getExtensionLanguage (const char *const extension)
{
	langType result = LANG_IGNORE;
	unsigned int i;
	for (i = 0  ;  i < LanguageCount  &&  result == LANG_IGNORE  ;  ++i)
	{
		stringList* const exts = LanguageTable [i]->currentExtensions;
		if (exts != NULL  &&  stringListExtensionMatched (exts, extension))
			result = i;
	}
	return result;
}

static langType getPatternLanguage (const char *const fileName)
{
	langType result = LANG_IGNORE;
	const char* base = baseFilename (fileName);
	unsigned int i;
	for (i = 0  ;  i < LanguageCount  &&  result == LANG_IGNORE  ;  ++i)
	{
		stringList* const ptrns = LanguageTable [i]->currentPatterns;
		if (ptrns != NULL  &&  stringListFileMatched (ptrns, base))
			result = i;
	}
	return result;
}

#ifdef SYS_INTERPRETER

/*  The name of the language interpreter, either directly or as the argument
 *  to "env".
 */
static vString* determineInterpreter (const char* const cmd)
{
	vString* const interpreter = vStringNew ();
	const char* p = cmd;
	do
	{
		vStringClear (interpreter);
		for ( ;  isspace ((int) *p)  ;  ++p)
			;  /* no-op */
		for ( ;  *p != '\0'  &&  ! isspace ((int) *p)  ;  ++p)
			vStringPut (interpreter, (int) *p);
	} while (strcmp (vStringValue (interpreter), "env") == 0);
	return interpreter;
}

static langType getInterpreterLanguage (const char *const fileName)
{
	langType result = LANG_IGNORE;
	FILE* const fp = fopen (fileName, "r");
	if (fp != NULL)
	{
		vString* const vLine = vStringNew ();
		const char* const line = readLineRaw (vLine, fp);
		if (line != NULL  &&  line [0] == '#'  &&  line [1] == '!')
		{
			const char* const lastSlash = strrchr (line, '/');
			const char *const cmd = lastSlash != NULL ? lastSlash+1 : line+2;
			vString* const interpreter = determineInterpreter (cmd);
			result = getExtensionLanguage (vStringValue (interpreter));
			vStringDelete (interpreter);
		}
		vStringDelete (vLine);
		fclose (fp);
	}
	return result;
}

#endif

extern langType getFileLanguage (const char *const fileName)
{
	langType language = Option.language;
	if (language == LANG_AUTO)
	{
		language = getExtensionLanguage (fileExtension (fileName));
		if (language == LANG_IGNORE)
			language = getPatternLanguage (fileName);
#ifdef SYS_INTERPRETER
		if (language == LANG_IGNORE  &&  isExecutable (fileName))
			language = getInterpreterLanguage (fileName);
#endif
	}
	return language;
}

extern void printLanguageMap (const langType language)
{
}

extern void installLanguageMapDefault (const langType language)
{
	parserDefinition* lang;
	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];
	if (lang->currentPatterns != NULL)
		stringListDelete (lang->currentPatterns);
	if (lang->currentExtensions != NULL)
		stringListDelete (lang->currentExtensions);

	if (lang->patterns == NULL)
		lang->currentPatterns = stringListNew ();
	else
	{
		lang->currentPatterns =
			stringListNewFromArgv (lang->patterns);
	}
	if (lang->extensions == NULL)
		lang->currentExtensions = stringListNew ();
	else
	{
		lang->currentExtensions =
			stringListNewFromArgv (lang->extensions);
	}
	BEGIN_VERBOSE(vfp);
	{
	printLanguageMap (language);
	putc ('\n', vfp);
	}
	END_VERBOSE();
}

extern void installLanguageMapDefaults (void)
{
	unsigned int i;
	for (i = 0  ;  i < LanguageCount  ;  ++i)
	{
		verbose ("    %s: ", getLanguageName (i));
		installLanguageMapDefault (i);
	}
}

extern void clearLanguageMap (const langType language)
{
	Assert (0 <= language  &&  language < (int) LanguageCount);
	stringListClear (LanguageTable [language]->currentPatterns);
	stringListClear (LanguageTable [language]->currentExtensions);
}

extern void addLanguagePatternMap (const langType language, const char* ptrn)
{
	vString* const str = vStringNewInit (ptrn);
	parserDefinition* lang;
	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];
	if (lang->currentPatterns == NULL)
		lang->currentPatterns = stringListNew ();
	stringListAdd (lang->currentPatterns, str);
}

extern void addLanguageExtensionMap (const langType language,
									 const char* extension)
{
	vString* const str = vStringNewInit (extension);
	Assert (0 <= language  &&  language < (int) LanguageCount);
	stringListAdd (LanguageTable [language]->currentExtensions, str);
}

extern void enableLanguage (const langType language, const bool state)
{
	Assert (0 <= language  &&  language < (int) LanguageCount);
	LanguageTable [language]->enabled = state;
}

extern void enableLanguages (const bool state)
{
	unsigned int i;
	for (i = 0  ;  i < LanguageCount  ;  ++i)
		LanguageTable [i]->enabled = state;
}

static void initializeParserOne (langType lang)
{
	parserDefinition *const parser = LanguageTable [lang];

	installKeywordTable (lang);
	installTagRegexTable (lang);

	if ((parser->initialize != NULL) && (parser->initialized == false))
	{
		parser->initialize (lang);
		parser->initialized = true;
	}
}

extern void initializeParser (langType lang)
{
	if (lang == LANG_AUTO)
	{
		int i;
		for (i = 0; i < countParsers(); i++)
			initializeParserOne (i);
	}
	else
		initializeParserOne (lang);
}

static void initializeParsers (void)
{
	int i;
	for (i = 0; i < LanguageCount;  i++)
		initializeParserOne(i);
}

extern void initializeParsing (void)
{
	unsigned int builtInCount;
	unsigned int i;

	builtInCount = ARRAY_SIZE (BuiltInParsers);
	LanguageTable = xMalloc (builtInCount, parserDefinition*);

	verbose ("Installing parsers: ");
	for (i = 0  ;  i < builtInCount  ;  ++i)
	{
		parserDefinition* const def = (*BuiltInParsers [i]) ();
		if (def != NULL)
		{
			bool accepted = false;
			if (def->name == NULL  ||  def->name[0] == '\0')
				error (FATAL, "parser definition must contain name\n");
			else if (def->method & METHOD_REGEX)
			{
				def->parser = findRegexTags;
				accepted = true;
			}
			else if ((def->parser == NULL)  ==  (def->parser2 == NULL))
				error (FATAL,
		"%s parser definition must define one and only one parsing routine\n",
					   def->name);
			else
				accepted = true;
			if (accepted)
			{
				verbose ("%s%s", i > 0 ? ", " : "", def->name);
				def->id = LanguageCount++;
				LanguageTable [def->id] = def;
			}
		}
	}
	enableLanguages (true);
	initializeParsers ();
}

/*
*   Option parsing
*/

extern void processLanguageDefineOption (const char *const option,
										 const char *const parameter CTAGS_ATTR_UNUSED)
{
}

extern bool processKindOption (
		const char *const option, const char *const parameter)
{
	return false;
}

extern bool processAliasOption (
		const char *const option, const char *const parameter)
{
	return false;
}

extern void useRegexMethod (const langType language)
{
	parserDefinition* lang;

	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];
	lang->method |= METHOD_REGEX;
}

static void installTagRegexTable (const langType language)
{
	parserDefinition* lang;
	unsigned int i;

	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];


	if (lang->tagRegexTable != NULL)
	{
	    for (i = 0; i < lang->tagRegexCount; ++i)
		    addTagRegex (language,
				 lang->tagRegexTable [i].regex,
				 lang->tagRegexTable [i].name,
				 lang->tagRegexTable [i].kinds,
				 lang->tagRegexTable [i].flags,
				 (lang->tagRegexTable [i].disabled));
	}
}

static void installKeywordTable (const langType language)
{
	parserDefinition* lang;
	unsigned int i;

	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];

	if (lang->keywordTable != NULL)
	{
		for (i = 0; i < lang->keywordCount; ++i)
			addKeyword (lang->keywordTable [i].name,
				    language,
				    lang->keywordTable [i].id);
	}
}
