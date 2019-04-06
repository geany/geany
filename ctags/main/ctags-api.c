/*
*   Copyright (c) 2016, Jiri Techet
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   Defines ctags API when compiled as a library.
*/

#include "general.h"  /* must always come first */

#ifdef CTAGS_LIB

#include "ctags-api.h"
#include "types.h"
#include "routines.h"
#include "error.h"
#include "output.h"
#include "parse.h"
#include "options.h"
#include "trashbox.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

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

extern void ctagsInit(void)
{
	setErrorPrinter (nofatalErrorPrinter, NULL);
	setTagWriter (&ctagsWriter);

	checkRegex ();
	initFieldDescs ();

	initializeParsing ();
	initOptions ();

	initDefaultTrashBox ();

	/* make sure all parsers are initialized */
	initializeParser (LANG_AUTO);
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

	createTagsWithFallback(buffer, bufferSize, fileName, language,
		tagCallback, passCallback, userData);
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
	const parserDefinition *def = getParserDefinition(lang);
	unsigned int i;
	static char kinds[257];

	for (i = 0; i < def->kindCount; i++)
		kinds[i] = def->kindTable[i].letter;
	kinds[i] = '\0';

	return kinds;
}


extern const char *ctagsGetKindName(char kind, int lang)
{
	const parserDefinition *def = getParserDefinition(lang);
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
	const parserDefinition *def = getParserDefinition(lang);
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
	return getParserDefinition(lang)->method & METHOD_REGEX;
}


extern unsigned int ctagsGetLangCount(void)
{
	return countParsers();
}

#endif /* CTAGS_LIB */
