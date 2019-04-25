/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines ctags API when compiled as a library.
*/

#include "general.h"  /* must always come first */

#include "tm_ctags.h"
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

#define CTAGS_LANG(x) ((x) >= 0 ? (x) + 1 : (x))
#define GEANY_LANG(x) ((x) >= 1 ? (x) - 1 : (x))

static int writeEntry (tagWriter *writer, MIO * mio, const tagEntryInfo *const tag);
static void rescanFailed (tagWriter *writer, unsigned long validTagNum);

tagWriter geanyWriter = {
	.writeEntry = writeEntry,
	.writePtagEntry = NULL, /* no pseudo-tags */
	.preWriteEntry = NULL,
	.postWriteEntry = NULL,
	.rescanFailedEntry = rescanFailed,
	.buildFqTagCache = NULL,
	.defaultFileName = "geany_tags_file_which_should_never_appear_anywhere",
	.private = NULL,
	.type = WRITER_U_CTAGS /* not really but we must use some of the builtin types */
};

static tagEntryFunction geanyTagEntryFunction = NULL;
static rescanFailedFunction geanyRescanFailedFunction = NULL;
static void *geanyUserData = NULL;


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
	tag->lang = GEANY_LANG(info->langType);
}


static int writeEntry (tagWriter *writer, MIO * mio, const tagEntryInfo *const tag)
{
	ctagsTag t;

	getTagScopeInformation((tagEntryInfo *)tag, NULL, NULL);
	initCtagsTag(&t, tag);
	geanyTagEntryFunction(&t, geanyUserData);

	/* output length - we don't write anything to the MIO */
	return 0;
}


static void rescanFailed (tagWriter *writer, unsigned long validTagNum)
{
	geanyRescanFailedFunction (validTagNum, geanyUserData);
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
	const char *fileName, const int language,
	tagEntryFunction tagCallback, rescanFailedFunction rescanCallback,
	void *userData)
{
	if (buffer == NULL && fileName == NULL)
	{
		error(FATAL, "Neither buffer nor file provided to ctagsParse()");
		return;
	}

	geanyTagEntryFunction = tagCallback;
	geanyRescanFailedFunction = rescanCallback;
	geanyUserData = userData;

	geanyCreateTags(buffer, bufferSize, fileName, CTAGS_LANG(language));
}


extern const char *ctagsGetLangName(int lang)
{
	return getLanguageName(CTAGS_LANG(lang));
}


extern int ctagsGetNamedLang(const char *name)
{
	return GEANY_LANG(getNamedLanguage(name, 0));
}


extern const char *ctagsGetLangKinds(int lang)
{
	unsigned int kindNum = countLanguageKinds(CTAGS_LANG(lang));
	static char kinds[257];
	unsigned int i;

	for (i = 0; i < kindNum; i++)
		kinds[i] = getLanguageKind(CTAGS_LANG(lang), i)->letter;
	kinds[i] = '\0';

	return kinds;
}


extern const char *ctagsGetKindName(char kind, int lang)
{
	kindDefinition *def = getLanguageKindForLetter (CTAGS_LANG(lang), kind);
	return def ? def->name : "unknown";
}


extern char ctagsGetKindFromName(const char *name, int lang)
{
	kindDefinition *def = getLanguageKindForName (CTAGS_LANG(lang), name);
	return def ? def->letter : '-';
}


extern unsigned int ctagsGetLangCount(void)
{
	return GEANY_LANG(countParsers());
}
