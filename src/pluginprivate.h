/*
 *      pluginprivate.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_PLUGIN_PRIVATE_H
#define GEANY_PLUGIN_PRIVATE_H 1

#include "plugindata.h"
#include "ui_utils.h"	/* GeanyAutoSeparator */
#include "keybindings.h"	/* GeanyKeyGroup */

#include "gtkcompat.h"


G_BEGIN_DECLS

typedef struct SignalConnection
{
	GObject	*object;
	gulong	handler_id;
}
SignalConnection;

typedef enum _LoadedFlags {
	LOADED_OK = 0x01,
	IS_LEGACY = 0x02,
	LOAD_DATA = 0x04,
}
LoadedFlags;

typedef struct GeanyPluginPrivate Plugin;	/* shorter alias */

typedef struct GeanyPluginPrivate
{
	gchar			*filename;				/* plugin filename (/path/libname.so) */
	PluginInfo		info;				/* plugin name, description, etc */
	GeanyPlugin		public;				/* fields the plugin can read */

	GeanyPluginFuncs cbs;					/* Callbacks set by geany_plugin_register() */
	void		(*configure_single) (GtkWidget *parent); /* plugin configure dialog, optional and deprecated */

	/* extra stuff */
	PluginFields	fields;
	GeanyKeyGroup	*key_group;
	GeanyAutoSeparator	toolbar_separator;
	GArray			*signal_ids;			/* SignalConnection's to disconnect when unloading */
	GList			*sources;				/* GSources to destroy when unloading */

	gpointer		cb_data;				/* user data passed back to functions in GeanyPluginFuncs */
	GDestroyNotify	cb_data_destroy;		/* called when the plugin is unloaded, for cb_data */
	LoadedFlags		flags;					/* bit-or of LoadedFlags */

	/* proxy plugin support */
	GeanyProxyFuncs	proxy_cbs;
	Plugin			*proxy;					/* The proxy that handles this plugin */
	gpointer		proxy_data;				/* Data passed to the proxy hooks of above proxy, so
											 * this gives the proxy a pointer to each plugin */
	gint			proxied_count;			/* count of active plugins this provides a proxy for
											 * (a count because of possibly nested proxies) */
}
GeanyPluginPrivate;

#define PLUGIN_LOADED_OK(p) (((p)->flags & LOADED_OK) != 0)
#define PLUGIN_IS_LEGACY(p) (((p)->flags & IS_LEGACY) != 0)
#define PLUGIN_HAS_LOAD_DATA(p) (((p)->flags & LOAD_DATA) != 0)

void plugin_watch_object(Plugin *plugin, gpointer object);
void plugin_make_resident(Plugin *plugin);
gpointer plugin_get_module_symbol(Plugin *plugin, const gchar *sym);

G_END_DECLS

#endif /* GEANY_PLUGIN_PRIVATE_H */
