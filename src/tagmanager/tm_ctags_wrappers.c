/*
 *      tm_ctags_wrappers.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2016 Jiri Techet <techet(at)gmail(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "tm_ctags_wrappers.h"

#include "general.h"
#include "entry.h"
#include "parse.h"
#include "read.h"
#include "geany.h"
#include "error.h"
#include "output.h"
#include "options.h"

#include <string.h>
#include <errno.h>

typedef struct {
	TMCtagsNewTagCallback tag_callback;
	TMCtagsPassStartCallback pass_callback;
	gpointer user_data;
} CallbackUserData;

extern bool nofatalErrorPrinter (const errorSelection selection,
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

void tm_ctags_init(void)
{
	setErrorPrinter (nofatalErrorPrinter, NULL);
	setTagWriter (&ctagsWriter);

	checkRegex ();
	initFieldDescs ();

	initializeParsing ();
	initOptions ();
	initializeParser (LANG_AUTO);
}


static bool tag_callback_internal(const tagEntryInfo *tag, gpointer user_data)
{
	CallbackUserData *callback_data = user_data;

	return callback_data->tag_callback(tag, callback_data->user_data);
}

static bool pass_callback_internal(gpointer user_data)
{
	CallbackUserData *callback_data = user_data;

	return callback_data->pass_callback(callback_data->user_data);
}

void tm_ctags_parse(guchar *buffer, gsize buffer_size,
	const gchar *file_name, TMParserType lang, TMCtagsNewTagCallback tag_callback,
	TMCtagsPassStartCallback pass_callback, gpointer user_data)
{
	CallbackUserData callback_data = {tag_callback, pass_callback, user_data};

	g_return_if_fail(buffer || file_name);

	createTagsWithFallback(buffer, buffer_size, file_name, lang,
		tag_callback_internal, pass_callback_internal, &callback_data);
}


const gchar *tm_ctags_get_lang_name(TMParserType lang)
{
	return getLanguageName(lang);
}


TMParserType tm_ctags_get_named_lang(const gchar *name)
{
	return getNamedLanguage(name, 0);
}


const gchar *tm_ctags_get_lang_kinds(TMParserType lang)
{
	guint i;
	parserDefinition *def = LanguageTable[lang];
	static gchar kinds[257];

	for (i = 0; i < def->kindCount; i++)
		kinds[i] = def->kinds[i].letter;
	kinds[i] = '\0';

	return kinds;
}


const gchar *tm_ctags_get_kind_name(gchar kind, TMParserType lang)
{
	guint i;
	parserDefinition *def = LanguageTable[lang];

	for (i = 0; i < def->kindCount; i++)
	{
		if (def->kinds[i].letter == kind)
			return def->kinds[i].name;
	}
	return "unknown";
}


gchar tm_ctags_get_kind_from_name(const gchar *name, TMParserType lang)
{
	guint i;
	parserDefinition *def = LanguageTable[lang];

	for (i = 0; i < def->kindCount; i++)
	{
		if (g_strcmp0(def->kinds[i].name, name) == 0)
			return def->kinds[i].letter;
	}
	return '-';
}


gboolean tm_ctags_is_using_regex_parser(TMParserType lang)
{
	return LanguageTable[lang]->method & METHOD_REGEX;
}


guint tm_ctags_get_lang_count(void)
{
	return countParsers();
}
