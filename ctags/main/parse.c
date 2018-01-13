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


#include "mio.h"
#include "entry.h"
#include "keyword.h"
#include "main.h"
#define OPTION_WRITE
#include "options.h"
#include "parsers.h"
#include "read.h"
#include "vstring.h"
#include "routines.h"

/*
*   DATA DEFINITIONS
*/
static parserDefinitionFunc* BuiltInParsers[] = { PARSER_LIST };
parserDefinition** LanguageTable = NULL;
unsigned int LanguageCount = 0;
static kindOption defaultFileKind = {
	.enabled     = false,
	.letter      = KIND_FILE_DEFAULT,
	.name        = KIND_FILE_DEFAULT_LONG,
	.description = KIND_FILE_DEFAULT_LONG,
};

tagEntryFunction TagEntryFunction = NULL;
void *TagEntryUserData = NULL;

/*
*   FUNCTION DEFINITIONS
*/

extern void setTagEntryFunction(tagEntryFunction entry_function, void *user_data)
{
	TagEntryFunction = entry_function;
	TagEntryUserData = user_data;
}


extern void makeSimpleTag (const vString* const name,
						   kindOption* const kinds, const int kind)
{
	if (name != NULL  &&  vStringLength (name) > 0)
	{
		tagEntryInfo e;
		initTagEntry (&e, vStringValue (name), &(kinds [kind]));

		makeTagEntry (&e);
	}
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
	/*Assert (0 <= language  &&  language < (int) LanguageCount);*/
	if (language < 0) return NULL;
		return LanguageTable [language]->name;
}

extern kindOption* getLanguageFileKind (const langType language)
{
	kindOption* kind;

	Assert (0 <= language  &&  language < (int) LanguageCount);

	kind = LanguageTable [language]->fileKind;

	Assert (kind != NULL);

	return kind;
}

extern langType getNamedLanguage (const char *const name)
{
	langType result = LANG_IGNORE;
	unsigned int i;
	Assert (name != NULL);
	for (i = 0  ;  i < LanguageCount  &&  result == LANG_IGNORE  ;  ++i)
	{
		if (LanguageTable [i]->name != NULL)
			if (stricmp (name, LanguageTable [i]->name) == 0)
				result = i;
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
		for ( ;  isspace (*p)  ;  ++p)
			;  /* no-op */
		for ( ;  *p != '\0'  &&  ! isspace (*p)  ;  ++p)
			vStringPut (interpreter, (int) *p);
	} while (strcmp (vStringValue (interpreter), "env") == 0);
	return interpreter;
}

static langType getInterpreterLanguage (const char *const fileName)
{
	langType result = LANG_IGNORE;
	FILE* const fp = g_fopen (fileName, "r");
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
	bool first = true;
	unsigned int i;
	stringList* map = LanguageTable [language]->currentPatterns;
	Assert (0 <= language  &&  language < (int) LanguageCount);
	for (i = 0  ;  map != NULL  &&  i < stringListCount (map)  ;  ++i)
	{
		printf ("%s(%s)", (first ? "" : " "),
				vStringValue (stringListItem (map, i)));
		first = false;
	}
	map = LanguageTable [language]->currentExtensions;
	for (i = 0  ;  map != NULL  &&  i < stringListCount (map)  ;  ++i)
	{
		printf ("%s.%s", (first ? "" : " "),
				vStringValue (stringListItem (map, i)));
		first = false;
	}
}

extern void installLanguageMapDefault (const langType language)
{
	Assert (language >= 0);
	if (LanguageTable [language]->currentPatterns != NULL)
		stringListDelete (LanguageTable [language]->currentPatterns);
	if (LanguageTable [language]->currentExtensions != NULL)
		stringListDelete (LanguageTable [language]->currentExtensions);

	if (LanguageTable [language]->patterns == NULL)
		LanguageTable [language]->currentPatterns = stringListNew ();
	else
	{
		LanguageTable [language]->currentPatterns =
			stringListNewFromArgv (LanguageTable [language]->patterns);
	}
	if (LanguageTable [language]->extensions == NULL)
		LanguageTable [language]->currentExtensions = stringListNew ();
	else
	{
		LanguageTable [language]->currentExtensions =
			stringListNewFromArgv (LanguageTable [language]->extensions);
	}
}

extern void installLanguageMapDefaults (void)
{
	unsigned int i;
	for (i = 0  ;  i < LanguageCount  ;  ++i)
	{
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
	Assert (0 <= language  &&  language < (int) LanguageCount);
	if (LanguageTable [language]->currentPatterns == NULL)
		LanguageTable [language]->currentPatterns = stringListNew ();
	stringListAdd (LanguageTable [language]->currentPatterns, str);
}

extern void addLanguageExtensionMap (const langType language,
									 const char* extension)
{
	vString* const str = vStringNewInit (extension);
	Assert (0 <= language  &&  language < (int) LanguageCount);
	stringListAdd (LanguageTable [language]->currentExtensions, str);
}

extern void enableLanguages (const bool state)
{
	unsigned int i;
	for (i = 0  ;  i < LanguageCount  ;  ++i)
		LanguageTable [i]->enabled = state;
}

extern void enableLanguage (const langType language, const bool state)
{
	Assert (0 <= language  &&  language < (int) LanguageCount);
	LanguageTable [language]->enabled = state;
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

static void initializeParsers (void)
{
	unsigned int i;
	for (i = 0; i < LanguageCount;  i++)
		initializeParserOne(i);
}

extern void initializeParsing (void)
{
	unsigned int builtInCount;
	unsigned int i;

	builtInCount = sizeof (BuiltInParsers) / sizeof (BuiltInParsers [0]);
	LanguageTable = xMalloc (builtInCount, parserDefinition*);

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
#ifdef HAVE_REGEX
				def->parser = findRegexTags;
				accepted = true;
#endif
			}
			else if ((def->parser == NULL)  ==  (def->parser2 == NULL))
				error (FATAL,
		"%s parser definition must define one and only one parsing routine\n",
					   def->name);
			else
				accepted = true;
			if (accepted)
			{
				def->id = LanguageCount++;
				LanguageTable [def->id] = def;
			}
		}
	}
	enableLanguages (true);
	initializeParsers ();
}

extern void freeParserResources (void)
{
	unsigned int i;
	for (i = 0  ;  i < LanguageCount  ;  ++i)
	{
		freeList (&LanguageTable [i]->currentPatterns);
		freeList (&LanguageTable [i]->currentExtensions);
		eFree (LanguageTable [i]->name);
		LanguageTable [i]->name = NULL;
		eFree (LanguageTable [i]);
	}
	eFree (LanguageTable);
	LanguageTable = NULL;
	LanguageCount = 0;
}

/*
*   Option parsing
*/

extern void processLanguageDefineOption (const char *const option,
										 const char *const parameter CTAGS_ATTR_UNUSED)
{
#ifdef HAVE_REGEX
	if (parameter [0] == '\0')
		error (WARNING, "No language specified for \"%s\" option", option);
	else if (getNamedLanguage (parameter) != LANG_IGNORE)
		error (WARNING, "Language \"%s\" already defined", parameter);
	else
	{
		unsigned int i = LanguageCount++;
		parserDefinition* const def = parserNew (parameter);
		def->parser            = findRegexTags;
		def->currentPatterns   = stringListNew ();
		def->currentExtensions = stringListNew ();
		def->method            = METHOD_NOT_CRAFTED;
		def->enabled           = true;
		def->id                = i;
		LanguageTable = xRealloc (LanguageTable, i + 1, parserDefinition*);
		LanguageTable [i] = def;
	}
#else
	error (WARNING, "regex support not available; required for --%s option",
		   option);
#endif
}

static kindOption *langKindOption (const langType language, const int flag)
{
	unsigned int i;
	kindOption* result = NULL;
	const parserDefinition* lang;
	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];
	for (i=0  ;  i < lang->kindCount  &&  result == NULL  ;  ++i)
		if (lang->kinds [i].letter == flag)
			result = &lang->kinds [i];
	return result;
}

extern void processLegacyKindOption (const char *const parameter)
{
	const langType lang = getNamedLanguage ("c");
	bool clear = false;
	const char* p = parameter;
	bool mode = true;
	int c;

	error (WARNING, "-i option is deprecated; use --c-types option instead");
	if (*p == '=')
	{
		clear = true;
		++p;
	}
	if (clear  &&  *p != '+'  &&  *p != '-')
	{
		unsigned int i;
		for (i = 0  ;  i < LanguageTable [lang]->kindCount  ;  ++i)
			LanguageTable [lang]->kinds [i].enabled = false;
		Option.include.fileNames= false;
		Option.include.fileScope= false;
	}
	while ((c = *p++) != '\0') switch (c)
	{
		case '+': mode = true;  break;
		case '-': mode = false; break;

		case 'F': Option.include.fileNames = mode; break;
		case 'S': Option.include.fileScope = mode; break;

		default:
		{
			kindOption* const opt = langKindOption (lang, c);
			if (opt != NULL)
				opt->enabled = mode;
			else
				error (WARNING, "Unsupported parameter '%c' for -i option", c);
		} break;
	}
}

static void disableLanguageKinds (const langType language)
{
	if (LanguageTable [language]->method & METHOD_REGEX)
#ifdef HAVE_REGEX
		disableRegexKinds (language);
#else
		;
#endif
	else
	{
		unsigned int i;
		for (i = 0  ;  i < LanguageTable [language]->kindCount  ;  ++i)
			LanguageTable [language]->kinds [i].enabled = false;
	}
}

static bool enableLanguageKind (const langType language,
								const int kind, const bool mode)
{
	bool result = false;
	if (LanguageTable [language]->method & METHOD_REGEX)
#ifdef HAVE_REGEX
		result = enableRegexKind (language, kind, mode);
#else
		;
#endif
	else
	{
		kindOption* const opt = langKindOption (language, kind);
		if (opt != NULL)
		{
			opt->enabled = mode;
			result = true;
		}
	}
	return result;
}

static void processLangKindOption (const langType language,
								   const char *const option,
								   const char *const parameter)
{
	const char *p = parameter;
	bool mode = true;
	int c;

	Assert (0 <= language  &&  language < (int) LanguageCount);
	if (*p != '+'  &&  *p != '-')
		disableLanguageKinds (language);
	while ((c = *p++) != '\0') switch (c)
	{
		case '+': mode = true;  break;
		case '-': mode = false; break;

		default:
		{
			if (! enableLanguageKind (language, c, mode))
				error (WARNING, "Unsupported parameter '%c' for --%s option",
					c, option);
		} break;
	}
}

extern bool processKindOption (const char *const option,
							   const char *const parameter)
{
	bool handled = false;
	const char* const dash = strchr (option, '-');
	if (dash != NULL  &&
		(strcmp (dash + 1, "types") == 0  ||  strcmp (dash + 1, "kinds") == 0))
	{
		langType language;
		vString* langName = vStringNew ();
		vStringNCopyS (langName, option, dash - option);
		language = getNamedLanguage (vStringValue (langName));
		if (language == LANG_IGNORE)
			error (WARNING, "Unknown language specified in \"%s\" option", option);
		else
			processLangKindOption (language, option, parameter);
		vStringDelete (langName);
		handled = true;
	}
	return handled;
}

static void printLangugageKindOption (const kindOption* const kind)
{
	printf ("          %c  %s%s\n", kind->letter,
		kind->description != NULL ? kind->description :
			(kind->name != NULL ? kind->name : ""),
		kind->enabled ? "" : " [off]");
}

static void printLangugageKindOptions (const langType language)
{
	const parserDefinition* lang;
	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];
	if (lang->kinds != NULL  ||  lang->method & METHOD_NOT_CRAFTED)
	{
		unsigned int i;
		char* const name = newLowerString (lang->name);
		printf ("  --%s-types=[+|-]kinds\n", name);
		eFree (name);
		if (lang->kinds != NULL)
			for (i = 0  ;  i < lang->kindCount  ;  ++i)
				printLangugageKindOption (lang->kinds + i);
#ifdef HAVE_REGEX
		/*printRegexKindOptions (language);*/ /* unused */
#endif
	}
}

extern void printKindOptions (void)
{
	unsigned int i;

	printf (
 "\n  The following options are used to specify which language-specific tag\n\
  types (or kinds) should be included in the tag file. \"Kinds\" is a group of\n\
  one-letter flags designating kinds of tags to either include or exclude from\n\
  the output. Each letter or group of letters may be preceded by either '+' to\n\
  add it to those already included, or '-' to exclude it from the output. In\n\
  the absence of any preceding '+' or '-' sign, only those kinds listed in\n\
  \"kinds\" will be included in the output. Below each option is a list of the\n\
  flags accepted. All kinds are enabled by default unless otherwise noted.\n\n");

	for (i = 0  ;  i < LanguageCount  ;  ++i)
		printLangugageKindOptions (i);
}

/*
*   File parsing
*/

static void makeFileTag (const char *const fileName)
{
	if (Option.include.fileNames)
	{
		tagEntryInfo tag;
		initTagEntry (&tag, baseFilename (fileName), getInputLanguageFileKind ());

		tag.isFileEntry     = true;
		tag.lineNumberEntry = true;
		tag.lineNumber      = 1;

		makeTagEntry (&tag);
	}
}

static bool createTagsForFile (const char *const fileName,
							   const langType language,
							   const unsigned int passCount)
{
	bool retried = false;

	if (fileOpen (fileName, language))
	{

		makeFileTag (fileName);

		if (LanguageTable [language]->parser != NULL)
			LanguageTable [language]->parser ();
		else if (LanguageTable [language]->parser2 != NULL)
			retried = LanguageTable [language]->parser2 (passCount);


		fileClose ();
	}

	return retried;
}

static bool createTagsWithFallback (const char *const fileName,
									const langType language)
{
	const unsigned long numTags = TagFile.numTags.added;
	MIOPos tagFilePosition;
	unsigned int passCount = 0;
	bool tagFileResized = false;

	mio_getpos (TagFile.mio, &tagFilePosition);
	while (createTagsForFile (fileName, language, ++passCount))
	{
		/*  Restore prior state of tag file.
		 */
		mio_setpos (TagFile.mio, &tagFilePosition);
		TagFile.numTags.added = numTags;
		tagFileResized = true;
	}
	return tagFileResized;
}

extern bool parseFile (const char *const fileName)
{
	bool tagFileResized = false;
	langType language = Option.language;
	if (Option.language == LANG_AUTO)
		language = getFileLanguage (fileName);
	Assert (language != LANG_AUTO);
	if (Option.filter)
		openTagFile ();

	tagFileResized = createTagsWithFallback (fileName, language);

	addTotals (1, 0L, 0L);

	return tagFileResized;
}

extern void installTagRegexTable (const langType language)
{
	parserDefinition* lang;
	unsigned int i;

	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];


	if ((lang->tagRegexTable != NULL) && (lang->tagRegexInstalled == false))
	{
	    for (i = 0; i < lang->tagRegexCount; ++i)
		    addTagRegex (language,
				 lang->tagRegexTable [i].regex,
				 lang->tagRegexTable [i].name,
				 lang->tagRegexTable [i].kinds,
				 lang->tagRegexTable [i].flags);
	    lang->tagRegexInstalled = true;
	}
}

extern void installKeywordTable (const langType language)
{
	parserDefinition* lang;
	unsigned int i;

	Assert (0 <= language  &&  language < (int) LanguageCount);
	lang = LanguageTable [language];

	if ((lang->keywordTable != NULL) && (lang->keywordInstalled == false))
	{
		for (i = 0; i < lang->keywordCount; ++i)
			addKeyword (lang->keywordTable [i].name,
				    language,
				    lang->keywordTable [i].id);
		lang->keywordInstalled = true;
	}
}
