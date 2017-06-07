/*
 *      symbols.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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


#ifndef GEANY_SYMBOLS_H
#define GEANY_SYMBOLS_H 1

#include "document.h"

#include <glib.h>

G_BEGIN_DECLS

const gchar *symbols_get_context_separator(gint ft_id);


#ifdef GEANY_PRIVATE

enum
{
	SYMBOLS_SORT_BY_NAME,
	SYMBOLS_SORT_BY_APPEARANCE,
	SYMBOLS_SORT_USE_PREVIOUS
};


void symbols_init(void);

void symbols_finalize(void);

void symbols_reload_config_files(void);

void symbols_global_tags_loaded(guint file_type_idx);

GString *symbols_find_typenames_as_string(TMParserType lang, gboolean global);

gboolean symbols_recreate_tag_list(GeanyDocument *doc, gint sort_mode);

gint symbols_generate_global_tags(gint argc, gchar **argv, gboolean want_preprocess);

void symbols_show_load_tags_dialog(void);

gboolean symbols_goto_tag(const gchar *name, gboolean definition);

gint symbols_get_current_function(GeanyDocument *doc, const gchar **tagname);

gint symbols_get_current_scope(GeanyDocument *doc, const gchar **tagname);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_SYMBOLS_H */
