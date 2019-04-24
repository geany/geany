/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines ctags API when compiled as a library.
*/

#include "general.h"  /* must always come first */

#ifdef GEANY_CTAGS_LIB

#include "ctags-api.h"
#include "types.h"
#include "routines.h"
#include "error.h"
#include "mio.h"
#include "writer_p.h"
#include "parse_p.h"
#include "options_p.h"
#include "trashbox.h"
#include "field.h"
#include "xtag.h"
#include "entry_p.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

static int writeEntry (tagWriter *writer, MIO * mio, const tagEntryInfo *const tag);

tagWriter geanyWriter = {
	.writeEntry = writeEntry,
	.writePtagEntry = NULL, /* no pseudo-tags */
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.buildFqTagCache = NULL,
	.defaultFileName = "geany_tags_file_which_should_never_appear_anywhere",
	.private = NULL,
	.type = WRITER_U_CTAGS /* not really but we must use some of the builtin types */
};

static tagEntryFunction geanyTagEntryFunction = NULL;
static void *geanyTagEntryUserData = NULL;


static bool nofatalErrorPrinter (const errorSelection selection,
					  const char *const format,
					  va_list ap, void *data CTAGS_ATTR_UNUSED)
{
	fprintf (stderr, "%s: ", (selection & WARNING) ? "Warning: " : "Error");
	vfprintf (stderr, format, ap);
	if (selection & PERROR)
#ifdef HAVE_STRERROR
		fprintf (stderr, " : %s", strerror (errno));
#else
		perror (" ");
#endif
	fputs ("\n", stderr);

	return false;
}


static void enableAllLangKinds()
{
	unsigned int lang;

	for (lang = 0; lang < countParsers(); lang++)
	{
		unsigned int kindNum = countLanguageKinds(lang);
		unsigned int kind;

		for (kind = 0; kind < kindNum; kind++)
		{
			kindDefinition *def = getLanguageKind(lang, kind);
			enableKind(def, true);
		}
	}
}


static void initCtagsTag(ctagsTag *tag, const tagEntryInfo *info)
{
	tag->name = info->name;
	tag->signature = info->extensionFields.signature;
	tag->scopeName = info->extensionFields.scopeName;
	tag->inheritance = info->extensionFields.inheritance;
	tag->varType = info->extensionFields.typeRef[1];
	tag->access = info->extensionFields.access;
	tag->implementation = info->extensionFields.implementation;
	tag->kindLetter = getLanguageKind(info->langType, info->kindIndex)->letter;
	tag->isFileScope = info->isFileScope;
	tag->lineNumber = info->lineNumber;
	tag->lang = info->langType;
}


static int writeEntry (tagWriter *writer, MIO * mio, const tagEntryInfo *const tag)
{
	ctagsTag t;

	getTagScopeInformation((tagEntryInfo *)tag, NULL, NULL);
	initCtagsTag(&t, tag);
	geanyTagEntryFunction(&t, geanyTagEntryUserData);

	/* output length - we don't write anything to the MIO */
	return 0;
}


/* keep in sync with ctags main() - use only things interesting for us */
extern void ctagsInit(void)
{
	initDefaultTrashBox ();

	setErrorPrinter (nofatalErrorPrinter, NULL);
	geanySetTagWriter (&geanyWriter);

	checkRegex ();
	initFieldObjects ();
	initXtagObjects ();

	initializeParsing ();
	initOptions ();

	/* make sure all parsers are initialized */
	initializeParser (LANG_AUTO);

	/* change default value which is false */
	enableXtag(XTAG_TAGS_GENERATED_BY_GUEST_PARSERS, true);

	/* some kinds we are interested in are disabled by default */
	enableAllLangKinds();
}


extern void ctagsParse(unsigned char *buffer, size_t bufferSize,
	const char *fileName, const langType language,
	tagEntryFunction tagCallback, passStartCallback passCallback,
	void *userData)
{
	if (buffer == NULL && fileName == NULL)
	{
		error(FATAL, "Neither buffer nor file provided to ctagsParse()");
		return;
	}

	geanyTagEntryFunction = tagCallback;
	geanyTagEntryUserData = userData;
	geanyCreateTags(buffer, bufferSize, fileName, language,
		passCallback, userData);
}


extern const char *ctagsGetLangName(int lang)
{
	return getLanguageName(lang);
}


extern int ctagsGetNamedLang(const char *name)
{
	return getNamedLanguage(name, 0);
}


extern const char *ctagsGetLangKinds(int lang)
{
	const parserDefinition *def = geanyGetParserDefinition(lang);
	unsigned int i;
	static char kinds[257];

	for (i = 0; i < def->kindCount; i++)
		kinds[i] = def->kindTable[i].letter;
	kinds[i] = '\0';

	return kinds;
}


extern const char *ctagsGetKindName(char kind, int lang)
{
	const parserDefinition *def = geanyGetParserDefinition(lang);
	unsigned int i;

	for (i = 0; i < def->kindCount; i++)
	{
		if (def->kindTable[i].letter == kind)
			return def->kindTable[i].name;
	}
	return "unknown";
}


extern char ctagsGetKindFromName(const char *name, int lang)
{
	const parserDefinition *def = geanyGetParserDefinition(lang);
	unsigned int i;

	for (i = 0; i < def->kindCount; i++)
	{
		if (strcmp(def->kindTable[i].name, name) == 0)
			return def->kindTable[i].letter;
	}
	return '-';
}


extern bool ctagsIsUsingRegexParser(int lang)
{
	return geanyGetParserDefinition(lang)->method & METHOD_REGEX;
}


extern unsigned int ctagsGetLangCount(void)
{
	return countParsers();
}

#endif /* GEANY_CTAGS_LIB */
