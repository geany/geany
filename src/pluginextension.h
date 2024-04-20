/*
 *      pluginextension.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2023 The Geany contributors
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

#ifndef GEANY_PLUGIN_EXTENSION_H
#define GEANY_PLUGIN_EXTENSION_H 1

#include "document.h"

/* Temporary define so the plugin can check whether it's compiled against
 * Geany with plugin extension support or not */
#define HAVE_GEANY_PLUGIN_EXTENSION 1

G_BEGIN_DECLS


typedef struct {
	gboolean (*autocomplete_provided)(GeanyDocument *doc);
	void (*autocomplete_perform)(GeanyDocument *doc);

	gboolean (*calltips_provided)(GeanyDocument *doc);
	void (*calltips_show)(GeanyDocument *doc, gboolean force);

	gboolean (*goto_provided)(GeanyDocument *doc);
	void (*goto_perform)(GeanyDocument *doc, gint pos, gboolean definition);

	gboolean (*doc_symbols_provided)(GeanyDocument *doc);
	GPtrArray *(*doc_symbols_get)(GeanyDocument *doc);

	gboolean (*symbol_highlight_provided)(GeanyDocument *doc);

	gchar _dummy[1024];
} PluginExtension;


void plugin_extension_register(PluginExtension *extension);
void plugin_extension_unregister(PluginExtension *extension);


#ifdef GEANY_PRIVATE

gboolean plugin_extension_autocomplete_provided(GeanyDocument *doc);
void plugin_extension_autocomplete_perform(GeanyDocument *doc);

gboolean plugin_extension_calltips_provided(GeanyDocument *doc);
void plugin_extension_calltips_show(GeanyDocument *doc, gboolean force);

gboolean plugin_extension_goto_provided(GeanyDocument *doc);
void plugin_extension_goto_perform(GeanyDocument *doc, gint pos, gboolean definition);

gboolean plugin_extension_doc_symbols_provided(GeanyDocument *doc);
GPtrArray *plugin_extension_doc_symbols_get(GeanyDocument *doc);

gboolean plugin_extension_symbol_highlight_provided(GeanyDocument *doc);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_PLUGIN_EXTENSION_H */
