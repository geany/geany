/*
 *      plugins.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* Code to manage, load and unload plugins. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_PLUGINS

#include "plugins.h"

#include "app.h"
#include "dialogs.h"
#include "encodings.h"
#include "geanyobject.h"
#include "geanywraplabel.h"
#include "highlighting.h"
#include "keybindingsprivate.h"
#include "keyfile.h"
#include "main.h"
#include "msgwindow.h"
#include "navqueue.h"
#include "plugindata.h"
#include "pluginprivate.h"
#include "pluginutils.h"
#include "prefs.h"
#include "sciwrappers.h"
#include "stash.h"
#include "support.h"
#include "symbols.h"
#include "templates.h"
#include "toolbar.h"
#include "ui_utils.h"
#include "utils.h"
#include "win32.h"

#include "gtkcompat.h"

#include <string.h>


GList *active_plugin_list = NULL; /* list of only actually loaded plugins, always valid */


static gboolean want_plugins = FALSE;

/* list of all available, loadable plugins, only valid as long as the plugin manager dialog is
 * opened, afterwards it will be destroyed */
static GList *plugin_list = NULL;
static gchar **active_plugins_pref = NULL; 	/* list of plugin filenames to load at startup */
static GList *failed_plugins_list = NULL;	/* plugins the user wants active but can't be used */

static GtkWidget *menu_separator = NULL;

static gchar *get_plugin_path(void);
static void pm_show_dialog(GtkMenuItem *menuitem, gpointer user_data);

static GeanyData geany_data;


static void
geany_data_init(void)
{
	GeanyData gd = {
		app,
		&main_widgets,
		documents_array,
		filetypes_array,
		&prefs,
		&interface_prefs,
		&toolbar_prefs,
		&editor_prefs,
		&file_prefs,
		&search_prefs,
		&tool_prefs,
		&template_prefs,
		&build_info,
		filetypes_by_title
	};

	geany_data = gd;
}


/* Prevent the same plugin filename being loaded more than once.
 * Note: g_module_name always returns the .so name, even when Plugin::filename is a .la file. */
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
	/* Look also through the list of active plugins. This prevents problems when we have the same
	 * plugin in libdir/geany/ AND in configdir/plugins/ and the one in libdir/geany/ is loaded
	 * as active plugin. The plugin manager list would only take the one in configdir/geany/ and
	 * the plugin manager would list both plugins. Additionally, unloading the active plugin
	 * would cause a crash. */
	for (item = active_plugin_list; item != NULL; item = g_list_next(item))
	{
		basename_loaded = g_path_get_basename(g_module_name(((Plugin*)item->data)->module));

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


static Plugin *find_active_plugin_by_name(const gchar *filename)
{
	GList *item;

	g_return_val_if_fail(filename, FALSE);

	for (item = active_plugin_list; item != NULL; item = g_list_next(item))
	{
		if (utils_str_equal(filename, ((Plugin*)item->data)->filename))
			return item->data;
	}

	return NULL;
}


static gboolean
plugin_check_version(GModule *module)
{
	gint (*version_check)(gint) = NULL;

	g_module_symbol(module, "plugin_version_check", (void *) &version_check);

	if (G_UNLIKELY(! version_check))
	{
		geany_debug("Plugin \"%s\" has no plugin_version_check() function - ignoring plugin!",
				g_module_name(module));
		return FALSE;
	}
	else
	{
		gint result = version_check(GEANY_ABI_VERSION);

		if (result < 0)
		{
			msgwin_status_add(_("The plugin \"%s\" is not binary compatible with this "
				"release of Geany - please recompile it."), g_module_name(module));
			geany_debug("Plugin \"%s\" is not binary compatible with this "
				"release of Geany - recompile it.", g_module_name(module));
			return FALSE;
		}
		if (result > GEANY_API_VERSION)
		{
			geany_debug("Plugin \"%s\" requires a newer version of Geany (API >= v%d).",
				g_module_name(module), result);
			return FALSE;
		}
	}
	return TRUE;
}


static void add_callbacks(Plugin *plugin, PluginCallback *callbacks)
{
	PluginCallback *cb;
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

	for (i = 0; i < len; i++)
	{
		cb = &callbacks[i];

		plugin_signal_connect(&plugin->public, NULL, cb->signal_name, cb->after,
			cb->callback, cb->user_data);
	}
}


static void read_key_group(Plugin *plugin)
{
	GeanyKeyGroupInfo *p_key_info;
	GeanyKeyGroup **p_key_group;

	g_module_symbol(plugin->module, "plugin_key_group_info", (void *) &p_key_info);
	g_module_symbol(plugin->module, "plugin_key_group", (void *) &p_key_group);
	if (p_key_info && p_key_group)
	{
		GeanyKeyGroupInfo *key_info = p_key_info;

		if (*p_key_group)
			geany_debug("Ignoring plugin_key_group symbol for plugin '%s' - "
				"use plugin_set_key_group() instead to allocate keybindings dynamically.",
				plugin->info.name);
		else
		{
			if (key_info->count)
			{
				GeanyKeyGroup *key_group =
					plugin_set_key_group(&plugin->public, key_info->name, key_info->count, NULL);
				if (key_group)
					*p_key_group = key_group;
			}
			else
				geany_debug("Ignoring plugin_key_group_info symbol for plugin '%s' - "
					"count field is zero. Maybe use plugin_set_key_group() instead?",
					plugin->info.name);
		}
	}
	else if (p_key_info || p_key_group)
		geany_debug("Ignoring only one of plugin_key_group[_info] symbols defined for plugin '%s'. "
			"Maybe use plugin_set_key_group() instead?",
			plugin->info.name);
}


static gint cmp_plugin_names(gconstpointer a, gconstpointer b)
{
	const Plugin *pa = a;
	const Plugin *pb = b;

	return strcmp(pa->info.name, pb->info.name);
}


static void
plugin_load(Plugin *plugin)
{
	GeanyPlugin **p_geany_plugin;
	PluginCallback *callbacks;
	PluginInfo **p_info;
	PluginFields **plugin_fields;

	/* set these symbols before plugin_init() is called
	 * we don't set geany_data since it is set directly by plugin_new() */
	g_module_symbol(plugin->module, "geany_plugin", (void *) &p_geany_plugin);
	if (p_geany_plugin)
		*p_geany_plugin = &plugin->public;
	g_module_symbol(plugin->module, "plugin_info", (void *) &p_info);
	if (p_info)
		*p_info = &plugin->info;
	g_module_symbol(plugin->module, "plugin_fields", (void *) &plugin_fields);
	if (plugin_fields)
		*plugin_fields = &plugin->fields;
	read_key_group(plugin);

	/* start the plugin */
	g_return_if_fail(plugin->init);
	plugin->init(&geany_data);

	/* store some function pointers for later use */
	g_module_symbol(plugin->module, "plugin_configure", (void *) &plugin->configure);
	g_module_symbol(plugin->module, "plugin_configure_single", (void *) &plugin->configure_single);
	if (app->debug_mode && plugin->configure && plugin->configure_single)
		g_warning("Plugin '%s' implements plugin_configure_single() unnecessarily - "
			"only plugin_configure() will be used!",
			plugin->info.name);

	g_module_symbol(plugin->module, "plugin_help", (void *) &plugin->help);
	g_module_symbol(plugin->module, "plugin_cleanup", (void *) &plugin->cleanup);
	if (plugin->cleanup == NULL)
	{
		if (app->debug_mode)
			g_warning("Plugin '%s' has no plugin_cleanup() function - there may be memory leaks!",
				plugin->info.name);
	}

	/* now read any plugin-owned data that might have been set in plugin_init() */

	if (plugin->fields.flags & PLUGIN_IS_DOCUMENT_SENSITIVE)
	{
		ui_add_document_sensitive(plugin->fields.menu_item);
	}

	g_module_symbol(plugin->module, "plugin_callbacks", (void *) &callbacks);
	if (callbacks)
		add_callbacks(plugin, callbacks);

	/* remember which plugins are active.
	 * keep list sorted so tools menu items and plugin preference tabs are
	 * sorted by plugin name */
	active_plugin_list = g_list_insert_sorted(active_plugin_list, plugin, cmp_plugin_names);

	geany_debug("Loaded:   %s (%s)", plugin->filename,
		FALLBACK(plugin->info.name, "<Unknown>"));
}


/* Load and optionally init a plugin.
 * load_plugin decides whether the plugin's plugin_init() function should be called or not. If it is
 * called, the plugin will be started, if not the plugin will be read only (for the list of
 * available plugins in the plugin manager).
 * When add_to_list is set, the plugin will be added to the plugin manager's plugin_list. */
static Plugin*
plugin_new(const gchar *fname, gboolean load_plugin, gboolean add_to_list)
{
	Plugin *plugin;
	GModule *module;
	GeanyData **p_geany_data;
	void (*plugin_set_info)(PluginInfo*);

	g_return_val_if_fail(fname, NULL);
	g_return_val_if_fail(g_module_supported(), NULL);

	/* find the plugin in the list of already loaded, active plugins and use it, otherwise
	 * load the module */
	plugin = find_active_plugin_by_name(fname);
	if (plugin != NULL)
	{
		geany_debug("Plugin \"%s\" already loaded.", fname);
		if (add_to_list)
		{
			/* do not add to the list twice */
			if (g_list_find(plugin_list, plugin) != NULL)
				return NULL;

			plugin_list = g_list_prepend(plugin_list, plugin);
		}
		return plugin;
	}

	/* Don't use G_MODULE_BIND_LAZY otherwise we can get unresolved symbols at runtime,
	 * causing a segfault. Without that flag the module will safely fail to load.
	 * G_MODULE_BIND_LOCAL also helps find undefined symbols e.g. app when it would
	 * otherwise not be detected due to the shadowing of Geany's app variable.
	 * Also without G_MODULE_BIND_LOCAL calling public functions e.g. the old info()
	 * function from a plugin will be shadowed. */
	module = g_module_open(fname, G_MODULE_BIND_LOCAL);
	if (! module)
	{
		geany_debug("Can't load plugin: %s", g_module_error());
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

	g_module_symbol(module, "plugin_set_info", (void *) &plugin_set_info);
	if (plugin_set_info == NULL)
	{
		geany_debug("No plugin_set_info() defined for \"%s\" - ignoring plugin!", fname);

		if (! g_module_close(module))
			g_warning("%s: %s", fname, g_module_error());
		return NULL;
	}

	plugin = g_new0(Plugin, 1);

	/* set basic fields here to allow plugins to call Geany functions in set_info() */
	g_module_symbol(module, "geany_data", (void *) &p_geany_data);
	if (p_geany_data)
		*p_geany_data = &geany_data;

	/* read plugin name, etc. */
	plugin_set_info(&plugin->info);
	if (G_UNLIKELY(EMPTY(plugin->info.name)))
	{
		geany_debug("No plugin name set in plugin_set_info() for \"%s\" - ignoring plugin!",
			fname);

		if (! g_module_close(module))
			g_warning("%s: %s", fname, g_module_error());
		g_free(plugin);
		return NULL;
	}

	g_module_symbol(module, "plugin_init", (void *) &plugin->init);
	if (plugin->init == NULL)
	{
		geany_debug("Plugin '%s' has no plugin_init() function - ignoring plugin!",
			plugin->info.name);

		if (! g_module_close(module))
			g_warning("%s: %s", fname, g_module_error());
		g_free(plugin);
		return NULL;
	}
	/*geany_debug("Initializing plugin '%s'", plugin->info.name);*/

	plugin->filename = g_strdup(fname);
	plugin->module = module;
	plugin->public.info = &plugin->info;
	plugin->public.priv = plugin;

	if (load_plugin)
		plugin_load(plugin);

	if (add_to_list)
		plugin_list = g_list_prepend(plugin_list, plugin);

	return plugin;
}


static void on_object_weak_notify(gpointer data, GObject *old_ptr)
{
	Plugin *plugin = data;
	guint i = 0;

	g_return_if_fail(plugin && plugin->signal_ids);

	for (i = 0; i < plugin->signal_ids->len; i++)
	{
		SignalConnection *sc = &g_array_index(plugin->signal_ids, SignalConnection, i);

		if (sc->object == old_ptr)
		{
			g_array_remove_index_fast(plugin->signal_ids, i);
			/* we can break the loop right after finding the first match,
			 * because we will get one notification per connected signal */
			break;
		}
	}
}


/* add an object to watch for destruction, and release pointers to it when destroyed.
 * this should only be used by plugin_signal_connect() to add a watch on
 * the object lifetime and nuke out references to it in plugin->signal_ids */
void plugin_watch_object(Plugin *plugin, gpointer object)
{
	g_object_weak_ref(object, on_object_weak_notify, plugin);
}


static void remove_callbacks(Plugin *plugin)
{
	GArray *signal_ids = plugin->signal_ids;
	SignalConnection *sc;

	if (signal_ids == NULL)
		return;

	foreach_array(SignalConnection, sc, signal_ids)
	{
		g_signal_handler_disconnect(sc->object, sc->handler_id);
		g_object_weak_unref(sc->object, on_object_weak_notify, plugin);
	}

	g_array_free(signal_ids, TRUE);
}


static void remove_sources(Plugin *plugin)
{
	GList *item;

	item = plugin->sources;
	while (item != NULL)
	{
		GList *next = item->next; /* cache the next pointer because current item will be freed */

		g_source_destroy(item->data);
		item = next;
	}
	/* don't free the list here, it is allocated inside each source's data */
}


static gboolean is_active_plugin(Plugin *plugin)
{
	return (g_list_find(active_plugin_list, plugin) != NULL);
}


/* Clean up anything used by an active plugin  */
static void
plugin_cleanup(Plugin *plugin)
{
	GtkWidget *widget;

	if (plugin->cleanup)
		plugin->cleanup();

	remove_callbacks(plugin);
	remove_sources(plugin);

	if (plugin->key_group)
		keybindings_free_group(plugin->key_group);

	widget = plugin->toolbar_separator.widget;
	if (widget)
		gtk_widget_destroy(widget);

	geany_debug("Unloaded: %s", plugin->filename);
}


static void
plugin_free(Plugin *plugin)
{
	g_return_if_fail(plugin);
	g_return_if_fail(plugin->module);

	if (is_active_plugin(plugin))
		plugin_cleanup(plugin);

	active_plugin_list = g_list_remove(active_plugin_list, plugin);

	if (! g_module_close(plugin->module))
		g_warning("%s: %s", plugin->filename, g_module_error());

	plugin_list = g_list_remove(plugin_list, plugin);

	g_free(plugin->filename);
	g_free(plugin);
	plugin = NULL;
}


static gchar *get_custom_plugin_path(const gchar *plugin_path_config,
									 const gchar *plugin_path_system)
{
	gchar *plugin_path_custom;

	if (EMPTY(prefs.custom_plugin_path))
		return NULL;

	plugin_path_custom = utils_get_locale_from_utf8(prefs.custom_plugin_path);
	utils_tidy_path(plugin_path_custom);

	/* check whether the custom plugin path is one of the system or user plugin paths
	 * and abort if so */
	if (utils_str_equal(plugin_path_custom, plugin_path_config) ||
		utils_str_equal(plugin_path_custom, plugin_path_system))
	{
		g_free(plugin_path_custom);
		return NULL;
	}
	return plugin_path_custom;
}


/* all 3 paths Geany looks for plugins in can change (even system path on Windows)
 * so we need to check active plugins are in the right place before loading */
static gboolean check_plugin_path(const gchar *fname)
{
	gchar *plugin_path_config;
	gchar *plugin_path_system;
	gchar *plugin_path_custom;
	gboolean ret = FALSE;

	plugin_path_config = g_build_filename(app->configdir, "plugins", NULL);
	if (g_str_has_prefix(fname, plugin_path_config))
		ret = TRUE;

	plugin_path_system = get_plugin_path();
	if (g_str_has_prefix(fname, plugin_path_system))
		ret = TRUE;

	plugin_path_custom = get_custom_plugin_path(plugin_path_config, plugin_path_system);
	if (plugin_path_custom)
	{
		if (g_str_has_prefix(fname, plugin_path_custom))
			ret = TRUE;

		g_free(plugin_path_custom);
	}
	g_free(plugin_path_config);
	g_free(plugin_path_system);
	return ret;
}


/* load active plugins at startup */
static void
load_active_plugins(void)
{
	guint i, len;

	if (active_plugins_pref == NULL || (len = g_strv_length(active_plugins_pref)) == 0)
		return;

	for (i = 0; i < len; i++)
	{
		const gchar *fname = active_plugins_pref[i];

		if (!EMPTY(fname) && g_file_test(fname, G_FILE_TEST_EXISTS))
		{
			if (!check_plugin_path(fname) || plugin_new(fname, TRUE, FALSE) == NULL)
				failed_plugins_list = g_list_prepend(failed_plugins_list, g_strdup(fname));
		}
	}
}


static void
load_plugins_from_path(const gchar *path)
{
	GSList *list, *item;
	gchar *fname, *tmp;
	gint count = 0;

	list = utils_get_file_list(path, NULL, NULL);

	for (item = list; item != NULL; item = g_slist_next(item))
	{
		tmp = strrchr(item->data, '.');
		if (tmp == NULL || utils_str_casecmp(tmp, "." G_MODULE_SUFFIX) != 0)
			continue;

		fname = g_build_filename(path, item->data, NULL);
		if (plugin_new(fname, FALSE, TRUE))
			count++;
		g_free(fname);
	}

	g_slist_foreach(list, (GFunc) g_free, NULL);
	g_slist_free(list);

	if (count)
		geany_debug("Added %d plugin(s) in '%s'.", count, path);
}


static gchar *get_plugin_path(void)
{
	return g_strdup(utils_resource_dir(RESOURCE_DIR_PLUGIN));
}


/* Load (but don't initialize) all plugins for the Plugin Manager dialog */
static void load_all_plugins(void)
{
	gchar *plugin_path_config;
	gchar *plugin_path_system;
	gchar *plugin_path_custom;

	plugin_path_config = g_build_filename(app->configdir, "plugins", NULL);
	plugin_path_system = get_plugin_path();

	/* first load plugins in ~/.config/geany/plugins/ */
	load_plugins_from_path(plugin_path_config);

	/* load plugins from a custom path */
	plugin_path_custom = get_custom_plugin_path(plugin_path_config, plugin_path_system);
	if (plugin_path_custom)
	{
		load_plugins_from_path(plugin_path_custom);
		g_free(plugin_path_custom);
	}

	/* finally load plugins from $prefix/lib/geany */
	load_plugins_from_path(plugin_path_system);

	g_free(plugin_path_config);
	g_free(plugin_path_system);
}


static void on_tools_menu_show(GtkWidget *menu_item, G_GNUC_UNUSED gpointer user_data)
{
	GList *item, *list = gtk_container_get_children(GTK_CONTAINER(menu_item));
	guint i = 0;
	gboolean have_plugin_menu_items = FALSE;

	for (item = list; item != NULL; item = g_list_next(item))
	{
		if (item->data == menu_separator)
		{
			if (i < g_list_length(list) - 1)
			{
				have_plugin_menu_items = TRUE;
				break;
			}
		}
		i++;
	}
	g_list_free(list);

	ui_widget_show_hide(menu_separator, have_plugin_menu_items);
}


/* Calling this starts up plugin support */
void plugins_load_active(void)
{
	GtkWidget *widget;

	want_plugins = TRUE;

	geany_data_init();

	widget = gtk_separator_menu_item_new();
	gtk_widget_show(widget);
	gtk_container_add(GTK_CONTAINER(main_widgets.tools_menu), widget);

	widget = gtk_menu_item_new_with_mnemonic(_("_Plugin Manager"));
	gtk_container_add(GTK_CONTAINER(main_widgets.tools_menu), widget);
	gtk_widget_show(widget);
	g_signal_connect(widget, "activate", G_CALLBACK(pm_show_dialog), NULL);

	menu_separator = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(main_widgets.tools_menu), menu_separator);
	g_signal_connect(main_widgets.tools_menu, "show", G_CALLBACK(on_tools_menu_show), NULL);

	load_active_plugins();
}


/* Update the global active plugins list so it's up-to-date when configuration
 * is saved. Called in response to GeanyObject's "save-settings" signal. */
static void update_active_plugins_pref(void)
{
	gint i = 0;
	GList *list;
	gsize count;

	/* if plugins are disabled, don't clear list of active plugins */
	if (!want_plugins)
		return;

	count = g_list_length(active_plugin_list) + g_list_length(failed_plugins_list);

	g_strfreev(active_plugins_pref);

	if (count == 0)
	{
		active_plugins_pref = NULL;
		return;
	}

	active_plugins_pref = g_new0(gchar*, count + 1);

	for (list = g_list_first(active_plugin_list); list != NULL; list = list->next)
	{
		Plugin *plugin = list->data;

		active_plugins_pref[i] = g_strdup(plugin->filename);
		i++;
	}
	for (list = g_list_first(failed_plugins_list); list != NULL; list = list->next)
	{
		const gchar *fname = list->data;

		active_plugins_pref[i] = g_strdup(fname);
		i++;
	}
	active_plugins_pref[i] = NULL;
}


/* called even if plugin support is disabled */
void plugins_init(void)
{
	StashGroup *group;
	gchar *path;

	path = get_plugin_path();
	geany_debug("System plugin path: %s", path);
	g_free(path);

	group = stash_group_new("plugins");
	configuration_add_pref_group(group, TRUE);

	stash_group_add_toggle_button(group, &prefs.load_plugins,
		"load_plugins", TRUE, "check_plugins");
	stash_group_add_entry(group, &prefs.custom_plugin_path,
		"custom_plugin_path", "", "extra_plugin_path_entry");

	g_signal_connect(geany_object, "save-settings", G_CALLBACK(update_active_plugins_pref), NULL);
	stash_group_add_string_vector(group, &active_plugins_pref, "active_plugins", NULL);
}


/* called even if plugin support is disabled */
void plugins_finalize(void)
{
	if (failed_plugins_list != NULL)
	{
		g_list_foreach(failed_plugins_list, (GFunc) g_free,	NULL);
		g_list_free(failed_plugins_list);
	}
	if (active_plugin_list != NULL)
	{
		g_list_foreach(active_plugin_list, (GFunc) plugin_free,	NULL);
		g_list_free(active_plugin_list);
	}
	g_strfreev(active_plugins_pref);
}


/* Check whether there are any plugins loaded which provide a configure symbol */
gboolean plugins_have_preferences(void)
{
	GList *item;

	if (active_plugin_list == NULL)
		return FALSE;

	foreach_list(item, active_plugin_list)
	{
		Plugin *plugin = item->data;
		if (plugin->configure != NULL || plugin->configure_single != NULL)
			return TRUE;
	}

	return FALSE;
}


/* Plugin Manager */

enum
{
	PLUGIN_COLUMN_CHECK = 0,
	PLUGIN_COLUMN_PLUGIN,
	PLUGIN_N_COLUMNS,
	PM_BUTTON_KEYBINDINGS,
	PM_BUTTON_CONFIGURE,
	PM_BUTTON_HELP
};

typedef struct
{
	GtkWidget *dialog;
	GtkWidget *tree;
	GtkListStore *store;
	GtkWidget *filter_entry;
	GtkWidget *configure_button;
	GtkWidget *keybindings_button;
	GtkWidget *help_button;
	GtkWidget *popup_menu;
	GtkWidget *popup_configure_menu_item;
	GtkWidget *popup_keybindings_menu_item;
	GtkWidget *popup_help_menu_item;
}
PluginManagerWidgets;

static PluginManagerWidgets pm_widgets;


static void pm_update_buttons(Plugin *p)
{
	gboolean is_active = FALSE;
	gboolean has_configure = FALSE;
	gboolean has_help = FALSE;
	gboolean has_keybindings = FALSE;

	if (p != NULL)
	{
		is_active = is_active_plugin(p);
		has_configure = (p->configure || p->configure_single) && is_active;
		has_help = p->help != NULL && is_active;
		has_keybindings = p->key_group && p->key_group->plugin_key_count > 0 && is_active;
	}

	gtk_widget_set_sensitive(pm_widgets.configure_button, has_configure);
	gtk_widget_set_sensitive(pm_widgets.help_button, has_help);
	gtk_widget_set_sensitive(pm_widgets.keybindings_button, has_keybindings);

	gtk_widget_set_sensitive(pm_widgets.popup_configure_menu_item, has_configure);
	gtk_widget_set_sensitive(pm_widgets.popup_help_menu_item, has_help);
	gtk_widget_set_sensitive(pm_widgets.popup_keybindings_menu_item, has_keybindings);
}


static void pm_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	Plugin *p;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, PLUGIN_COLUMN_PLUGIN, &p, -1);

		if (p != NULL)
			pm_update_buttons(p);
	}
}


static void pm_plugin_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	gboolean old_state, state;
	gchar *file_name;
	GtkTreeIter iter;
	GtkTreeIter store_iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(pm_widgets.tree));
	Plugin *p;

	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(model, &iter,
		PLUGIN_COLUMN_CHECK, &old_state,
		PLUGIN_COLUMN_PLUGIN, &p, -1);

	/* no plugins item */
	if (p == NULL)
		return;

	gtk_tree_model_filter_convert_iter_to_child_iter(
		GTK_TREE_MODEL_FILTER(model), &store_iter, &iter);

	state = ! old_state; /* toggle the state */

	/* save the filename of the plugin */
	file_name = g_strdup(p->filename);

	/* unload plugin module */
	if (!state)
		/* save shortcuts (only need this group, but it doesn't take long) */
		keybindings_write_to_file();

	plugin_free(p);

	/* reload plugin module and initialize it if item is checked */
	p = plugin_new(file_name, state, TRUE);
	if (!p)
	{
		/* plugin file may no longer be on disk, or is now incompatible */
		gtk_list_store_remove(pm_widgets.store, &store_iter);
	}
	else
	{
		if (state)
			keybindings_load_keyfile();		/* load shortcuts */

		/* update model */
		gtk_list_store_set(pm_widgets.store, &store_iter,
			PLUGIN_COLUMN_CHECK, state,
			PLUGIN_COLUMN_PLUGIN, p, -1);

		/* set again the sensitiveness of the configure and help buttons */
		pm_update_buttons(p);
	}
	g_free(file_name);
}


static gboolean pm_treeview_query_tooltip(GtkWidget *widget, gint x, gint y,
		gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	Plugin *p = NULL;

	if (! gtk_tree_view_get_tooltip_context(GTK_TREE_VIEW(widget), &x, &y, keyboard_mode,
			&model, &path, &iter))
		return FALSE;

	gtk_tree_model_get(model, &iter, PLUGIN_COLUMN_PLUGIN, &p, -1);
	if (p != NULL)
	{
		gchar *markup;
		gchar *details;

		details = g_strdup_printf(_("Version:\t%s\nAuthor(s):\t%s\nFilename:\t%s"),
			p->info.version, p->info.author, p->filename);
		markup = g_markup_printf_escaped("<b>%s</b>\n%s\n<small><i>\n%s</i></small>",
			p->info.name, p->info.description, details);

		gtk_tooltip_set_markup(tooltip, markup);
		gtk_tree_view_set_tooltip_row(GTK_TREE_VIEW(widget), tooltip, path);

		g_free(details);
		g_free(markup);
	}
	gtk_tree_path_free(path);

	return p != NULL;
}


static void pm_treeview_text_cell_data_func(GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	Plugin *p;

	gtk_tree_model_get(model, iter, PLUGIN_COLUMN_PLUGIN, &p, -1);

	if (p == NULL)
		g_object_set(cell, "text", _("No plugins available."), NULL);
	else
	{
		gchar *markup = g_markup_printf_escaped("<b>%s</b>\n%s", p->info.name, p->info.description);

		g_object_set(cell, "markup", markup, NULL);
		g_free(markup);
	}
}


static gboolean pm_treeview_button_press_cb(GtkWidget *widget, GdkEventButton *event,
		G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_menu_popup(GTK_MENU(pm_widgets.popup_menu), NULL, NULL, NULL, NULL,
				event->button, event->time);
	}
	return FALSE;
}


static gint pm_tree_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
		gpointer user_data)
{
	Plugin *pa, *pb;

	gtk_tree_model_get(model, a, PLUGIN_COLUMN_PLUGIN, &pa, -1);
	gtk_tree_model_get(model, b, PLUGIN_COLUMN_PLUGIN, &pb, -1);

	if (pa && pb)
		return strcmp(pa->info.name, pb->info.name);
	else
		return pa - pb;
}


static gboolean pm_tree_search(const gchar *key, const gchar *haystack)
{
	gchar *normalized_string = NULL;
	gchar *normalized_key = NULL;
	gchar *case_normalized_string = NULL;
	gchar *case_normalized_key = NULL;
	gboolean matched = TRUE;

	normalized_string = g_utf8_normalize(haystack, -1, G_NORMALIZE_ALL);
	normalized_key = g_utf8_normalize(key, -1, G_NORMALIZE_ALL);

	if (normalized_string != NULL && normalized_key != NULL)
	{
		GString *stripped_key;
		gchar **subkey, **subkeys;

		case_normalized_string = g_utf8_casefold(normalized_string, -1);
		case_normalized_key = g_utf8_casefold(normalized_key, -1);
		stripped_key = g_string_new(case_normalized_key);
		do {} while (utils_string_replace_all(stripped_key, "  ", " "));
		subkeys = g_strsplit(stripped_key->str, " ", -1);
		g_string_free(stripped_key, TRUE);
		foreach_strv(subkey, subkeys)
		{
			if (strstr(case_normalized_string, *subkey) == NULL)
			{
				matched = FALSE;
				break;
			}
		}
		g_strfreev(subkeys);
	}

	g_free(normalized_key);
	g_free(normalized_string);
	g_free(case_normalized_key);
	g_free(case_normalized_string);

	return matched;
}


static gboolean pm_tree_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	Plugin *plugin;
	gboolean matched;
	const gchar *key;
	gchar *haystack, *filename;

	gtk_tree_model_get(model, iter, PLUGIN_COLUMN_PLUGIN, &plugin, -1);
	key = gtk_entry_get_text(GTK_ENTRY(pm_widgets.filter_entry));

	filename = g_path_get_basename(plugin->filename);
	haystack = g_strjoin(" ", plugin->info.name, plugin->info.description,
					plugin->info.author, filename, NULL);
	matched = pm_tree_search(key, haystack);
	g_free(haystack);
	g_free(filename);

	return matched;
}


static void on_pm_tree_filter_entry_changed_cb(GtkEntry *entry, gpointer user_data)
{
	GtkTreeModel *filter_model = gtk_tree_view_get_model(GTK_TREE_VIEW(pm_widgets.tree));
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter_model));
}


static void on_pm_tree_filter_entry_icon_release_cb(GtkEntry *entry, GtkEntryIconPosition icon_pos,
		GdkEvent *event, gpointer user_data)
{
	if (event->button.button == 1 && icon_pos == GTK_ENTRY_ICON_PRIMARY)
		on_pm_tree_filter_entry_changed_cb(entry, user_data);
}


static void pm_prepare_treeview(GtkWidget *tree, GtkListStore *store)
{
	GtkCellRenderer *text_renderer, *checkbox_renderer;
	GtkTreeViewColumn *column;
	GtkTreeModel *filter_model;
	GtkTreeIter iter;
	GList *list;
	GtkTreeSelection *sel;

	g_signal_connect(tree, "query-tooltip", G_CALLBACK(pm_treeview_query_tooltip), NULL);
	gtk_widget_set_has_tooltip(tree, TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	checkbox_renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Active"), checkbox_renderer, "active", PLUGIN_COLUMN_CHECK, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	g_signal_connect(checkbox_renderer, "toggled", G_CALLBACK(pm_plugin_toggled), NULL);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Plugin"), text_renderer, NULL);
	gtk_tree_view_column_set_cell_data_func(column, text_renderer,
		pm_treeview_text_cell_data_func, NULL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), PLUGIN_COLUMN_PLUGIN,
		pm_tree_sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(store), PLUGIN_COLUMN_PLUGIN, GTK_SORT_ASCENDING);

	/* selection handling */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
	g_signal_connect(sel, "changed", G_CALLBACK(pm_selection_changed), NULL);

	g_signal_connect(tree, "button-press-event", G_CALLBACK(pm_treeview_button_press_cb), NULL);

	list = g_list_first(plugin_list);
	if (list == NULL)
	{
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, PLUGIN_COLUMN_CHECK, FALSE,
				PLUGIN_COLUMN_PLUGIN, NULL, -1);
	}
	else
	{
		Plugin *p;
		for (; list != NULL; list = list->next)
		{
			p = list->data;

			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
				PLUGIN_COLUMN_CHECK, is_active_plugin(p),
				PLUGIN_COLUMN_PLUGIN, p,
				-1);
		}
	}
	/* filter */
	filter_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
	gtk_tree_model_filter_set_visible_func(
		GTK_TREE_MODEL_FILTER(filter_model), pm_tree_filter_func, NULL, NULL);

	/* set model to tree view */
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), filter_model);
	g_object_unref(store);
	g_object_unref(filter_model);
}


static void pm_on_plugin_button_clicked(G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	Plugin *p;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pm_widgets.tree));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, PLUGIN_COLUMN_PLUGIN, &p, -1);

		if (p != NULL)
		{
			if (GPOINTER_TO_INT(user_data) == PM_BUTTON_CONFIGURE)
				plugin_show_configure(&p->public);
			else if (GPOINTER_TO_INT(user_data) == PM_BUTTON_HELP && p->help != NULL)
				p->help();
			else if (GPOINTER_TO_INT(user_data) == PM_BUTTON_KEYBINDINGS && p->key_group && p->key_group->plugin_key_count > 0)
				keybindings_dialog_show_prefs_scroll(p->info.name);
		}
	}
}


static void
free_non_active_plugin(gpointer data, gpointer user_data)
{
	Plugin *plugin = data;

	/* don't do anything when closing the plugin manager and it is an active plugin */
	if (is_active_plugin(plugin))
		return;

	plugin_free(plugin);
}


/* Callback when plugin manager dialog closes, responses GTK_RESPONSE_CLOSE and
 * GTK_RESPONSE_DELETE_EVENT are treated the same. */
static void pm_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	switch (response)
	{
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_DELETE_EVENT:
			if (plugin_list != NULL)
			{
				/* remove all non-active plugins from the list */
				g_list_foreach(plugin_list, free_non_active_plugin, NULL);
				g_list_free(plugin_list);
				plugin_list = NULL;
			}
			gtk_widget_destroy(GTK_WIDGET(dialog));

			configuration_save();
			break;
		case PM_BUTTON_CONFIGURE:
		case PM_BUTTON_HELP:
		case PM_BUTTON_KEYBINDINGS:
			/* forward event to the generic handler */
			pm_on_plugin_button_clicked(NULL, GINT_TO_POINTER(response));
			break;
	}
}


static void pm_show_dialog(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *vbox, *vbox2, *swin, *label, *menu_item, *filter_entry;

	/* before showing the dialog, we need to create the list of available plugins */
	load_all_plugins();

	pm_widgets.dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(pm_widgets.dialog), _("Plugins"));
	gtk_window_set_transient_for(GTK_WINDOW(pm_widgets.dialog), GTK_WINDOW(main_widgets.window));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(pm_widgets.dialog), TRUE);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(pm_widgets.dialog));
	gtk_widget_set_name(pm_widgets.dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	gtk_window_set_default_size(GTK_WINDOW(pm_widgets.dialog), 500, 450);

	pm_widgets.help_button = gtk_dialog_add_button(
		GTK_DIALOG(pm_widgets.dialog), GTK_STOCK_HELP, PM_BUTTON_HELP);
	pm_widgets.configure_button = gtk_dialog_add_button(
		GTK_DIALOG(pm_widgets.dialog), GTK_STOCK_PREFERENCES, PM_BUTTON_CONFIGURE);
	pm_widgets.keybindings_button = gtk_dialog_add_button(
		GTK_DIALOG(pm_widgets.dialog), _("Keybindings"), PM_BUTTON_KEYBINDINGS);
	gtk_dialog_add_button(GTK_DIALOG(pm_widgets.dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_set_default_response(GTK_DIALOG(pm_widgets.dialog), GTK_RESPONSE_CLOSE);

	/* filter */
	pm_widgets.filter_entry = filter_entry = gtk_entry_new();
	gtk_entry_set_icon_from_stock(GTK_ENTRY(filter_entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
	ui_entry_add_clear_icon(GTK_ENTRY(filter_entry));
	g_signal_connect(filter_entry, "changed", G_CALLBACK(on_pm_tree_filter_entry_changed_cb), NULL);
	g_signal_connect(filter_entry, "icon-release",
		G_CALLBACK(on_pm_tree_filter_entry_icon_release_cb), NULL);

	/* prepare treeview */
	pm_widgets.tree = gtk_tree_view_new();
	pm_widgets.store = gtk_list_store_new(
		PLUGIN_N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_POINTER);
	pm_prepare_treeview(pm_widgets.tree, pm_widgets.store);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(swin), pm_widgets.tree);

	label = geany_wrap_label_new(_("Choose which plugins should be loaded at startup:"));

	/* plugin popup menu */
	pm_widgets.popup_menu = gtk_menu_new();

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	gtk_container_add(GTK_CONTAINER(pm_widgets.popup_menu), menu_item);
	g_signal_connect(menu_item, "activate",
			G_CALLBACK(pm_on_plugin_button_clicked), GINT_TO_POINTER(PM_BUTTON_CONFIGURE));
	pm_widgets.popup_configure_menu_item = menu_item;

	menu_item = gtk_image_menu_item_new_with_mnemonic(_("Keybindings"));
	gtk_container_add(GTK_CONTAINER(pm_widgets.popup_menu), menu_item);
	g_signal_connect(menu_item, "activate",
			G_CALLBACK(pm_on_plugin_button_clicked), GINT_TO_POINTER(PM_BUTTON_KEYBINDINGS));
	pm_widgets.popup_keybindings_menu_item = menu_item;

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP, NULL);
	gtk_container_add(GTK_CONTAINER(pm_widgets.popup_menu), menu_item);
	g_signal_connect(menu_item, "activate",
			G_CALLBACK(pm_on_plugin_button_clicked), GINT_TO_POINTER(PM_BUTTON_HELP));
	pm_widgets.popup_help_menu_item = menu_item;

	/* put it together */
	vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), filter_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), swin, TRUE, TRUE, 0);

	g_signal_connect(pm_widgets.dialog, "response", G_CALLBACK(pm_dialog_response), NULL);

	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show_all(pm_widgets.dialog);
	gtk_widget_show_all(pm_widgets.popup_menu);

	/* set initial plugin buttons state, pass NULL as no plugin is selected by default */
	pm_update_buttons(NULL);
	gtk_widget_grab_focus(pm_widgets.filter_entry);
}


#endif
