/*
 *      pluginutils.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2009-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#ifndef GEANY_PLUGIN_UTILS_H
#define GEANY_PLUGIN_UTILS_H 1

#ifdef HAVE_PLUGINS

#include "keybindings.h"	/* GeanyKeyGroupCallback */

#include "gtkcompat.h"

G_BEGIN_DECLS

/* avoid including plugindata.h otherwise this redefines the GEANY() macro */
struct GeanyPlugin;
struct GeanyDocument;

gint geany_api_version(void);

void plugin_add_toolbar_item(struct GeanyPlugin *plugin, GtkToolItem *item);

void plugin_module_make_resident(struct GeanyPlugin *plugin);

void plugin_signal_connect(struct GeanyPlugin *plugin,
		GObject *object, const gchar *signal_name, gboolean after,
		GCallback callback, gpointer user_data);

guint plugin_timeout_add(struct GeanyPlugin *plugin, guint interval, GSourceFunc function,
		gpointer data);

guint plugin_timeout_add_seconds(struct GeanyPlugin *plugin, guint interval, GSourceFunc function,
		gpointer data);

guint plugin_idle_add(struct GeanyPlugin *plugin, GSourceFunc function, gpointer data);

struct GeanyKeyGroup *plugin_set_key_group(struct GeanyPlugin *plugin,
		const gchar *section_name, gsize count, GeanyKeyGroupCallback callback);

GeanyKeyGroup *plugin_set_key_group_full(struct GeanyPlugin *plugin,
		const gchar *section_name, gsize count, GeanyKeyGroupFunc cb, gpointer pdata, GDestroyNotify destroy_notify);

void plugin_show_configure(struct GeanyPlugin *plugin);

void plugin_builder_connect_signals(struct GeanyPlugin *plugin,
	GtkBuilder *builder, gpointer user_data);

gpointer plugin_get_document_data(struct GeanyPlugin *plugin,
	struct GeanyDocument *doc, const gchar *key);

void plugin_set_document_data(struct GeanyPlugin *plugin, struct GeanyDocument *doc,
	const gchar *key, gpointer data);

void plugin_set_document_data_full(struct GeanyPlugin *plugin,
	struct GeanyDocument *doc, const gchar *key, gpointer data,
	GDestroyNotify free_func);

G_END_DECLS

#endif /* HAVE_PLUGINS */
#endif /* GEANY_PLUGIN_UTILS_H */
