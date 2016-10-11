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

typedef struct {
	TMCtagsNewTagCallback tag_callback;
	gpointer user_data;
} CallbackUserData;


void tm_ctags_init(void)
{
	initializeParsing();
	installLanguageMapDefaults();
}


static gboolean parse_callback(const tagEntryInfo *tag, gpointer user_data)
{
	CallbackUserData *callback_data = user_data;

	return callback_data->tag_callback(tag, callback_data->user_data);
}


void tm_ctags_parse(guchar *buffer, gsize buffer_size,
	const gchar *file_name, TMParserType lang, TMCtagsNewTagCallback tag_callback,
	TMCtagsPassStartCallback pass_callback, gpointer user_data)
{
	CallbackUserData callback_data = {tag_callback, user_data};
	gboolean retry = TRUE;
	guint passCount = 0;

	g_return_if_fail(buffer || file_name);

	if (! LanguageTable [lang]->enabled)
	{
#ifdef TM_DEBUG
		g_warning("ignoring %s (language disabled)\n", file_name);
#endif
		return;
	}

	setTagEntryFunction(parse_callback, &callback_data);
	while (retry && passCount < 3)
	{
		pass_callback(user_data);
		if ((!buffer && openInputFile (file_name, lang, NULL)) ||
			(buffer && bufferOpen (file_name, lang, buffer, buffer_size)))
		{
			if (LanguageTable [lang]->parser != NULL)
			{
				LanguageTable [lang]->parser ();
				retry = FALSE;
			}
			else if (LanguageTable [lang]->parser2 != NULL)
				retry = LanguageTable [lang]->parser2 (passCount);
			closeInputFile ();
		}
		else
		{
			g_warning("Unable to open %s", file_name);
			break;
		}
		++ passCount;
	}
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
