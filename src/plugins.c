/*
 *      plugins.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */

/* Code to manage, load and unload plugins. */

#include "geany.h"

#ifdef HAVE_PLUGINS

#include "plugins.h"
#include "plugindata.h"
#include "support.h"
#include "utils.h"
#include "document.h"
#include "templates.h"
#include "sciwrappers.h"
#include "ui_utils.h"


typedef struct Plugin
{
	GModule 	*module;
	gchar		*filename;		// plugin filename (/path/libname.so)
	PluginData		data;
	PluginFields	fields;

	PluginInfo*	(*info) ();	/* Returns plugin name, description */
	void	(*init) (PluginData *data);	/* Called when the plugin is enabled */
	void	(*cleanup) ();		/* Called when the plugin is disabled or when Geany exits */
}
Plugin;

static GList *plugin_list = NULL;


static DocumentFuncs doc_funcs = {
	&document_new_file,
	&document_get_cur_idx,
	&document_get_current
};

static ScintillaFuncs sci_funcs = {
	&sci_set_text,
	&sci_insert_text,
	&sci_get_current_position,
	&sci_get_text,
	&sci_get_length,
	&sci_replace_sel,
	&sci_get_selected_text,
	&sci_get_selected_text_length
};

static TemplateFuncs template_funcs = {
	&templates_get_template_fileheader
};

static UtilsFuncs utils_funcs = {
	&utils_str_equal,
	&utils_str_replace,
	&utils_get_file_list
};

static UIUtilsFuncs uiutils_funcs = {
	&ui_dialog_vbox_new,
	&ui_frame_new_with_alignment
};


static void
init_plugin_data(PluginData *data)
{
	data->app = app;
	data->tools_menu = lookup_widget(app->window, "tools1_menu");
	data->doc_array = doc_array;

	data->document = &doc_funcs;
	data->sci = &sci_funcs;
	data->templates = &template_funcs;
	data->utils = &utils_funcs;
	data->ui = &uiutils_funcs;
}


/* Prevent the same plugin filename being loaded more than once.
 * Note: g_module_name always returns the .so name, even when Plugin::filename
 * is an .la file. */
static gboolean
plugin_loaded(GModule *module)
{
	const gchar *fname = g_module_name(module);
	GList *item;

	for (item = plugin_list; item != NULL; item = g_list_next(item))
	{
		Plugin *p = item->data;

		if (utils_str_equal(fname, g_module_name(p->module)))
			return TRUE;
	}
	return FALSE;
}


static gboolean
plugin_check_version(GModule *module)
{
	gint (*version_check)(gint) = NULL;
	gint result;

	g_module_symbol(module, "version_check", (void *) &version_check);

	if (version_check)
	{
		result = version_check(abi_version);
		if (result < 0)
		{
			geany_debug("Plugin \"%s\" is not binary compatible with this "
				"release of Geany - recompile it.", g_module_name(module));
			return FALSE;
		}
		if (result > 0)
		{
			geany_debug("Plugin \"%s\" requires a newer version of Geany (API >= v%d).",
				g_module_name(module), result);
			return FALSE;
		}
	}
	return TRUE;
}


static Plugin*
plugin_new(const gchar *fname)
{
	Plugin *plugin;
	GModule *module;
	PluginInfo* (*info)();
	PluginFields **plugin_fields;

	g_return_val_if_fail(fname, NULL);
	g_return_val_if_fail(g_module_supported(), NULL);

	/* Don't use G_MODULE_BIND_LAZY otherwise we can get unresolved symbols at runtime,
	 * causing a segfault. Without that flag the module will safely fail to load.
	 * G_MODULE_BIND_LOCAL also helps find undefined symbols e.g. app when it would
	 * otherwise not be detected due to the shadowing of Geany's app variable.
	 * Also without G_MODULE_BIND_LOCAL calling info() in a plugin will be shadowed. */
	module = g_module_open(fname, G_MODULE_BIND_LOCAL);
	if (! module)
	{
		g_warning("%s", g_module_error());
		return NULL;
	}

	if (plugin_loaded(module))
	{
		geany_debug("Plugin \"%s\" already loaded.", fname);

		if (! g_module_close(module))
			g_warning("%s: %s", fname, g_module_error());
		return NULL;
	}

	if (! plugin_check_version(module))
	{
		if (! g_module_close(module))
			g_warning("%s: %s", fname, g_module_error());
		return NULL;
	}

	g_module_symbol(module, "info", (void *) &info);
	if (info == NULL)
	{
		geany_debug("Unknown plugin info for \"%s\"!", fname);

		if (! g_module_close(module))
			g_warning("%s: %s", fname, g_module_error());
		return NULL;
	}
	geany_debug("Initializing plugin '%s' (%s)",
		info()->name, info()->description);

	plugin = g_new0(Plugin, 1);
	plugin->info = info;
	plugin->filename = g_strdup(fname);
	plugin->module = module;

	init_plugin_data(&plugin->data);

	g_module_symbol(module, "plugin_fields", (void *) &plugin_fields);
	if (plugin_fields)
		*plugin_fields = &plugin->fields;

	g_module_symbol(module, "init", (void *) &plugin->init);
	g_module_symbol(module, "cleanup", (void *) &plugin->cleanup);

	if (plugin->init)
		plugin->init(&plugin->data);

	if (plugin->fields.flags & PLUGIN_IS_DOCUMENT_SENSITIVE)
	{
		gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) ? TRUE : FALSE;
		gtk_widget_set_sensitive(plugin->fields.menu_item, enable);
	}

	geany_debug("Loaded:   %s (%s)", fname,
		NVL(plugin->info()->name, "<Unknown>"));
	return plugin;
}


static void
plugin_free(Plugin *plugin)
{
	g_return_if_fail(plugin);
	g_return_if_fail(plugin->module);

	if (plugin->cleanup)
		plugin->cleanup();

	if (plugin->fields.menu_item != NULL)
	{
		gtk_widget_destroy(plugin->fields.menu_item);
	}

	if (! g_module_close(plugin->module))
		g_warning("%s: %s", plugin->filename, g_module_error());
	else
		geany_debug("Unloaded: %s", plugin->filename);

	g_free(plugin->filename);
	g_free(plugin);
}


// TODO: Pass -DLIBDIR=\"$(libdir)/geany\" in Makefile.am
#define LIBDIR \
	PACKAGE_DATA_DIR G_DIR_SEPARATOR_S ".." G_DIR_SEPARATOR_S "lib" \
		G_DIR_SEPARATOR_S PACKAGE

void plugins_init()
{
	const gchar *path = LIBDIR;
	GSList *list, *item;

	list = utils_get_file_list(path, NULL, NULL);

	for (item = list; item != NULL; item = g_slist_next(item))
	{
		gchar *fname = g_strconcat(path, G_DIR_SEPARATOR_S, item->data, NULL);
		Plugin *plugin;

		plugin = plugin_new(fname);
		if (plugin != NULL)
		{
			plugin_list = g_list_append(plugin_list, plugin);
		}
		g_free(fname);
	}

	g_slist_foreach(list, (GFunc) g_free, NULL);
	g_slist_free(list);
}


void plugins_free()
{
	if (plugin_list != NULL)
	{
		g_list_foreach(plugin_list, (GFunc) plugin_free, NULL);
		g_list_free(plugin_list);
	}
}


void plugins_update_document_sensitive(gboolean enabled)
{
	GList *item;

	for (item = plugin_list; item != NULL; item = g_list_next(item))
	{
		Plugin *plugin = item->data;

		if (plugin->fields.flags & PLUGIN_IS_DOCUMENT_SENSITIVE)
			gtk_widget_set_sensitive(plugin->fields.menu_item, enabled);
	}
}

#endif
