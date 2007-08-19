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

#include <string.h>

#include "Scintilla.h"
#include "ScintillaWidget.h"

#include "plugins.h"
#include "plugindata.h"
#include "support.h"
#include "utils.h"
#include "document.h"
#include "filetypes.h"
#include "templates.h"
#include "sciwrappers.h"
#include "ui_utils.h"
#include "editor.h"
#include "dialogs.h"
#include "msgwindow.h"
#include "geanyobject.h"

#ifdef G_OS_WIN32
# define PLUGIN_EXT "dll"
#else
# define PLUGIN_EXT "so"
#endif


typedef struct Plugin
{
	GModule 	*module;
	gchar		*filename;		// plugin filename (/path/libname.so)
	PluginFields	fields;
	gulong		*signal_ids;		// signal IDs to disconnect when unloading
	gsize		signal_ids_len;

	PluginInfo*	(*info) ();	/* Returns plugin name, description */
	void	(*init) (GeanyData *data);	/* Called when the plugin is enabled */
	void	(*cleanup) ();		/* Called when the plugin is disabled or when Geany exits */
}
Plugin;

static GList *plugin_list = NULL;


static DocumentFuncs doc_funcs = {
	&document_new_file,
	&document_get_cur_idx,
	&document_get_current,
	&document_save_file,
	&document_open_file,
	&document_open_files,
	&document_remove
};

static ScintillaFuncs sci_funcs = {
	&scintilla_send_message,
	&sci_cmd,
	&sci_start_undo_action,
	&sci_end_undo_action,
	&sci_set_text,
	&sci_insert_text,
	&sci_get_text,
	&sci_get_length,
	&sci_get_current_position,
	&sci_set_current_position,
	&sci_get_col_from_position,
	&sci_get_line_from_position,
	&sci_get_position_from_line,
	&sci_replace_sel,
	&sci_get_selected_text,
	&sci_get_selected_text_length,
	&sci_get_selection_start,
	&sci_get_selection_end,
	&sci_get_selection_mode,
	&sci_set_selection_mode,
	&sci_set_selection_start,
	&sci_set_selection_end,
	&sci_get_text_range,
	&sci_get_line,
	&sci_get_line_length,
	&sci_get_line_count,
	&sci_get_line_is_visible,
	&sci_ensure_line_is_visible,
	&sci_scroll_caret,
	&sci_find_bracematch,
	&sci_get_style_at,
	&sci_get_char_at,
	&sci_get_zoom
};

static TemplateFuncs template_funcs = {
	&templates_get_template_fileheader
};

static UtilsFuncs utils_funcs = {
	&utils_str_equal,
	&utils_str_replace,
	&utils_get_file_list,
	&utils_write_file,
	&utils_get_locale_from_utf8,
	&utils_get_utf8_from_locale,
	&utils_remove_ext_from_filename
};

static UIUtilsFuncs uiutils_funcs = {
	&ui_dialog_vbox_new,
	&ui_frame_new_with_alignment
};

static DialogFuncs dialog_funcs = {
	&dialogs_show_question,
	&dialogs_show_msgbox
};

static SupportFuncs support_funcs = {
	&lookup_widget
};

static MsgWinFuncs msgwin_funcs = {
	&msgwin_status_add
};


static GeanyData geany_data = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	&doc_funcs,
	&sci_funcs,
	&template_funcs,
	&utils_funcs,
	&uiutils_funcs,
	&support_funcs,
	&dialog_funcs,
	&msgwin_funcs
};


static void
geany_data_init()
{
	geany_data.app = app;
	geany_data.tools_menu = lookup_widget(app->window, "tools1_menu");
	geany_data.doc_array = doc_array;
	geany_data.filetypes = filetypes;
	geany_data.editor_prefs = &editor_prefs;
}


/* Prevent the same plugin filename being loaded more than once.
 * Note: g_module_name always returns the .so name, even when Plugin::filename
 * is an .la file. */
static gboolean
plugin_loaded(GModule *module)
{
	gchar *basename_module, *basename_loaded;
	GList *item;

	basename_module = g_path_get_basename(g_module_name(module));
	for (item = plugin_list; item != NULL; item = g_list_next(item))
	{
		basename_loaded = g_path_get_basename(
			g_module_name(((Plugin*)item->data)->module));

		if (utils_str_equal(basename_module, basename_loaded))
		{
			g_free(basename_loaded);
			g_free(basename_module);
			return TRUE;
		}
		g_free(basename_loaded);
	}
	g_free(basename_module);
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


static void add_callbacks(Plugin *plugin, GeanyCallback *callbacks)
{
	GeanyCallback *cb;
	guint i, len = 0;

	while (TRUE)
	{
		cb = &callbacks[len];
		if (!cb->signal_name || !cb->callback)
			break;
		len++;
	}
	if (len == 0)
		return;

	plugin->signal_ids_len = len;
	plugin->signal_ids = g_new(gulong, len);

	for (i = 0; i < len; i++)
	{
		cb = &callbacks[i];

		plugin->signal_ids[i] = (cb->after) ?
			g_signal_connect_after(geany_object, cb->signal_name, cb->callback, cb->user_data) :
			g_signal_connect(geany_object, cb->signal_name, cb->callback, cb->user_data);
	}
}


static Plugin*
plugin_new(const gchar *fname)
{
	Plugin *plugin;
	GModule *module;
	PluginInfo* (*info)();
	PluginFields **plugin_fields;
	GeanyData **p_geany_data;
	GeanyCallback *callbacks;

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

	g_module_symbol(module, "geany_data", (void *) &p_geany_data);
	if (p_geany_data)
		*p_geany_data = &geany_data;
	g_module_symbol(module, "plugin_fields", (void *) &plugin_fields);
	if (plugin_fields)
		*plugin_fields = &plugin->fields;

	g_module_symbol(module, "init", (void *) &plugin->init);
	g_module_symbol(module, "cleanup", (void *) &plugin->cleanup);
	if (plugin->init != NULL && plugin->cleanup == NULL)
	{
		if (app->debug_mode)
			g_warning("Plugin '%s' has no cleanup() function - there may be memory leaks!",
				info()->name);
	}

	if (plugin->init)
		plugin->init(&geany_data);

	if (plugin->fields.flags & PLUGIN_IS_DOCUMENT_SENSITIVE)
	{
		gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) ? TRUE : FALSE;
		gtk_widget_set_sensitive(plugin->fields.menu_item, enable);
	}

	g_module_symbol(module, "geany_callbacks", (void *) &callbacks);
	if (callbacks)
		add_callbacks(plugin, callbacks);

	geany_debug("Loaded:   %s (%s)", fname,
		NVL(plugin->info()->name, "<Unknown>"));
	return plugin;
}


static void remove_callbacks(Plugin *plugin)
{
	guint i;

	if (plugin->signal_ids == NULL)
		return;

	for (i = 0; i < plugin->signal_ids_len; i++)
		g_signal_handler_disconnect(geany_object, plugin->signal_ids[i]);
	g_free(plugin->signal_ids);
}


static void
plugin_free(Plugin *plugin)
{
	g_return_if_fail(plugin);
	g_return_if_fail(plugin->module);

	if (plugin->cleanup)
		plugin->cleanup();

	if (! g_module_close(plugin->module))
		g_warning("%s: %s", plugin->filename, g_module_error());
	else
		geany_debug("Unloaded: %s", plugin->filename);

	remove_callbacks(plugin);
	g_free(plugin->filename);
	g_free(plugin);
}


static void
load_plugins(const gchar *path)
{
	GSList *list, *item;
	gchar *fname, *tmp;
	Plugin *plugin;

	list = utils_get_file_list(path, NULL, NULL);

	for (item = list; item != NULL; item = g_slist_next(item))
	{
		tmp = strrchr(item->data, '.');
		if (tmp == NULL || strcasecmp(tmp, "." PLUGIN_EXT) != 0)
			continue;

		fname = g_strconcat(path, G_DIR_SEPARATOR_S, item->data, NULL);
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


#ifdef G_OS_WIN32
static gchar *get_plugin_path()
{
	gchar *install_dir = g_win32_get_package_installation_directory("geany", NULL);
	gchar *path;

	path = g_strconcat(install_dir, "\\plugins", NULL);

	return path;
}
#endif


static void load_plugin_paths()
{
	gchar *path;

	path = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "plugins", NULL);
	// first load plugins in ~/.geany/plugins/, then in $prefix/lib/geany
	load_plugins(path);
#ifdef G_OS_WIN32
	g_free(path);
	path = get_plugin_path();
	load_plugins(path);
#else
	load_plugins(PACKAGE_LIB_DIR G_DIR_SEPARATOR_S "geany");
#endif

	g_free(path);
}


void plugins_init()
{
	GtkWidget *widget;

	geany_data_init();
	geany_object = geany_object_new();

	widget = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(geany_data.tools_menu), widget);

	load_plugin_paths();

	if (g_list_length(plugin_list) > 0)
		gtk_widget_show(widget);
}


void plugins_free()
{

	if (plugin_list != NULL)
	{
		g_list_foreach(plugin_list, (GFunc) plugin_free, NULL);
		g_list_free(plugin_list);
	}
	g_object_unref(geany_object);
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
