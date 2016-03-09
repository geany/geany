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

typedef gboolean (*tm_ctags_callback) (const tagEntryInfo *const tag,
	gboolean invalidate, void *user_data);


void tm_ctags_init();

void tm_ctags_parse(guchar *buffer, gsize buffer_size,
	const gchar *file_name, TMParserType lang, tm_ctags_callback callback,
	gpointer user_data);

const gchar *tm_ctags_get_lang_name(TMParserType lang);

TMParserType tm_ctags_get_named_lang(const gchar *name);

G_END_DECLS

#endif /* TM_CTAGS_WRAPPERS */
