/*
 *      tm_ctags_wrappers.h - this file is part of Geany, a fast and lightweight IDE
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

#ifndef TM_CTAGS_WRAPPERS
#define TM_CTAGS_WRAPPERS

#include <glib.h>

#include "tm_parser.h"

#include "entry.h" /* for sTagEntryInfo */


G_BEGIN_DECLS

/* Callback invoked for every tag found by the parser. The return value is
 * currently unused. */
typedef gboolean (*TMCtagsNewTagCallback) (const tagEntryInfo *const tag,
	void *user_data);

/* Callback invoked at the beginning of every parsing pass. The return value is
 * currently unused */
typedef gboolean (*TMCtagsPassStartCallback) (void *user_data);


void tm_ctags_init(void);

void tm_ctags_parse(guchar *buffer, gsize buffer_size,
	const gchar *file_name, TMParserType lang, TMCtagsNewTagCallback tag_callback,
	TMCtagsPassStartCallback pass_callback, gpointer user_data);

const gchar *tm_ctags_get_lang_name(TMParserType lang);

TMParserType tm_ctags_get_named_lang(const gchar *name);

const gchar *tm_ctags_get_lang_kinds(TMParserType lang);

const gchar *tm_ctags_get_kind_name(gchar kind, TMParserType lang);

gchar tm_ctags_get_kind_from_name(const gchar *name, TMParserType lang);

gboolean tm_ctags_is_using_regex_parser(TMParserType lang);

guint tm_ctags_get_lang_count(void);

G_END_DECLS

#endif /* TM_CTAGS_WRAPPERS */
