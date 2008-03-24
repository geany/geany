/*
 *      plugins.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

#include "prefix.h"
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
#include "prefs.h"
#include "geanyobject.h"
#include "geanywraplabel.h"
#include "build.h"
#include "encodings.h"
#include "search.h"
#include "highlighting.h"
#include "keybindings.h"


#ifdef G_OS_WIN32
# define PLUGIN_EXT "dll"
#else
# define PLUGIN_EXT "so"
#endif


typedef struct Plugin
{
	GModule 	*module;
	gchar		*filename;				/* plugin filename (/path/libname.so) */
	PluginFields	fields;
	gulong		*signal_ids;			/* signal IDs to disconnect when unloading */
	gsize		signal_ids_len;
	KeyBindingGroup	*key_group;

	PluginInfo*	(*info) (void);			/* Returns plugin name, description */
	void	(*init) (GeanyData *data);	/* Called when the plugin is enabled */
	void	(*configure) (GtkWidget *parent);	/* plugin configure dialog, optionally */
	void	(*cleanup) (void);			/* Called when the plugin is disabled or when Geany exits */
}
Plugin;


/* list of all available, loadable plugins, only valid as long as the plugin manager dialog is
 * opened, afterwards it will be destroyed */
static GList *plugin_list = NULL;
static GList *active_plugin_list = NULL; /* list of only actually loaded plugins, always valid */
static GtkWidget *separator = NULL;
static void pm_show_dialog(GtkMenuItem *menuitem, gpointer user_data);


enum
{
	PLUGIN_FREE_NON_ACTIVE,
	PLUGIN_FREE_ALL
};


static DocumentFuncs doc_funcs = {
	&document_new_file,
	&document_get_cur_idx,
	&document_get_n_idx,
	&document_find_by_filename,
	&document_get_current,
	&document_save_file,
	&document_open_file,
	&document_open_files,
	&document_remove,
	&document_reload_file,
	&document_set_encoding,
	&document_set_text_changed,
	&document_set_filetype
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
	&sci_get_current_line,
};

static TemplateFuncs template_funcs = {
	&templates_get_template_fileheader
};

static UtilsFuncs utils_funcs = {
	&utils_str_equal,
	&utils_string_replace_all,
	&utils_get_file_list,
	&utils_write_file,
	&utils_get_locale_from_utf8,
	&utils_get_utf8_from_locale,
	&utils_remove_ext_from_filename,
	&utils_mkdir,
	&utils_get_setting_boolean,
	&utils_get_setting_integer,
	&utils_get_setting_string,
	&utils_spawn_sync,
	&utils_spawn_async
};

static UIUtilsFuncs uiutils_funcs = {
	&ui_dialog_vbox_new,
	&ui_frame_new_with_alignment,
	&ui_set_statusbar,
	&ui_table_add_row,
	&ui_path_box_new,
	&ui_button_new_with_image,
};

static DialogFuncs dialog_funcs = {
	&dialogs_show_question,
	&dialogs_show_msgbox,
	&dialogs_show_save_as
};

static SupportFuncs support_funcs = {
	&lookup_widget
};

static MsgWinFuncs msgwin_funcs = {
	&msgwin_status_add,
	&msgwin_compiler_add_fmt
};

static EncodingFuncs encoding_funcs = {
	&encodings_convert_to_utf8,
	&encodings_convert_to_utf8_from_charset,
	&encodings_get_charset_from_index
};

static KeybindingFuncs keybindings_funcs = {
	&keybindings_send_command,
	&keybindings_set_item
};

static TagManagerFuncs tagmanager_funcs = {
	&tm_get_real_path,
	&tm_source_file_new,
	&tm_workspace_add_object,
	&tm_source_file_update,
	&tm_work_object_free,
	&tm_workspace_remove_object
};

static SearchFuncs search_funcs = {
	&search_show_find_in_files_dialog
};

static HighlightingFuncs highlighting_funcs = {
	&highlighting_get_style
};


static FiletypeFuncs filetype_funcs = {
	&filetypes_detect_from_filename,
	&filetypes_get_from_uid
};


static GeanyData geany_data = {
	NULL,
	NULL,
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
	&msgwin_funcs,
	&encoding_funcs,
	&keybindings_funcs,
	&tagmanager_funcs,
	&search_funcs,
	&highlighting_funcs,
	&filetype_funcs
};


static void
geany_data_init(void)
{
	geany_data.app = app;
	geany_data.tools_menu = lookup_widget(app->window, "tools1_menu");
	geany_data.doc_array = doc_array;
	geany_data.filetypes = filetypes;
	geany_data.prefs = &prefs;
	geany_data.editor_prefs = &editor_prefs;
	geany_data.build_info = &build_info;
}


/* Prevent the same plugin filename being loaded more than once.
 * Note: g_module_name always returns the .so name, even when Plugin::filename is an .la file. */
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


static void
add_kb_group(Plugin *plugin)
{
	g_return_if_fail(NZV(plugin->key_group->name));
	g_return_if_fail(! g_str_equal(plugin->key_group->name, keybindings_keyfile_group_name));

	if (plugin->key_group->count == 0)
	{
		plugin->key_group = NULL;	/* Ignore the group */
		return;
	}

	plugin->key_group->label = plugin->info()->name;

	g_ptr_array_add(keybinding_groups, plugin->key_group);
}


static void
plugin_init(Plugin *plugin)
{
	GeanyCallback *callbacks;

	if (plugin->init)
		plugin->init(&geany_data);

	if (plugin->fields.flags & PLUGIN_IS_DOCUMENT_SENSITIVE)
	{
		gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) ? TRUE : FALSE;
		gtk_widget_set_sensitive(plugin->fields.menu_item, enable);
	}

	g_module_symbol(plugin->module, "geany_callbacks", (void *) &callbacks);
	if (callbacks)
		add_callbacks(plugin, callbacks);

	g_module_symbol(plugin->module, "plugin_key_group",
		(void *) &plugin->key_group);
	if (plugin->key_group)
		add_kb_group(plugin);

	active_plugin_list = g_list_append(active_plugin_list, plugin);

	geany_debug("Loaded:   %s (%s)", plugin->filename,
		NVL(plugin->info()->name, "<Unknown>"));
}


/* Load and init a plugin.
 * init_plugin decides whether the plugin's init() function should be called or not. If it is
 * called, the plugin will be started, if not the plugin will be read only (for the list of
 * available plugins in the plugin manager).
 * When add_to_list is set, the plugin will be added to the plugin manager's plugin_list. */
static Plugin*
plugin_new(const gchar *fname, gboolean init_plugin, gboolean add_to_list)
{
	Plugin *plugin;
	GModule *module;
	PluginInfo* (*info)(void);
	PluginFields **plugin_fields;
	GeanyData **p_geany_data;

	g_return_val_if_fail(fname, NULL);
	g_return_val_if_fail(g_module_supported(), NULL);

	/* find the plugin in the list of already loaded, active plugins and use it, otherwise
	 * load the module */
	plugin = find_active_plugin_by_name(fname);
	if (plugin != NULL)
	{
		geany_debug("Plugin \"%s\" already loaded.", fname);
		if (add_to_list)
			plugin_list = g_list_append(plugin_list, plugin);
		return plugin;
	}

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
	geany_debug("Initializing plugin '%s'", info()->name);

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
	g_module_symbol(module, "configure", (void *) &plugin->configure);
	g_module_symbol(module, "cleanup", (void *) &plugin->cleanup);
	if (plugin->init != NULL && plugin->cleanup == NULL)
	{
		if (app->debug_mode)
			g_warning("Plugin '%s' has no cleanup() function - there may be memory leaks!",
				info()->name);
	}

	if (init_plugin)
		plugin_init(plugin);

	if (add_to_list)
		plugin_list = g_list_append(plugin_list, plugin);

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


static gboolean is_active_plugin(Plugin *plugin)
{
	return (g_list_find(active_plugin_list, plugin) != NULL);
}


static void
plugin_unload(Plugin *plugin)
{

	if (is_active_plugin(plugin) && plugin->cleanup)
		plugin->cleanup();

	remove_callbacks(plugin);

	if (plugin->key_group)
		g_ptr_array_remove_fast(keybinding_groups, plugin->key_group);

	active_plugin_list = g_list_remove(active_plugin_list, plugin);
	geany_debug("Unloaded: %s", plugin->filename);
}


static void
plugin_free(Plugin *plugin, gpointer data)
{
	g_return_if_fail(plugin);
	g_return_if_fail(plugin->module);

	/* don't do anything when closing the plugin manager and it is an active plugin */
	if (GPOINTER_TO_INT(data) == PLUGIN_FREE_NON_ACTIVE && is_active_plugin(plugin))
		return;

	plugin_unload(plugin);

	if (plugin->module != NULL && ! g_module_close(plugin->module))
		g_warning("%s: %s", plugin->filename, g_module_error());

	plugin_list = g_list_remove(plugin_list, plugin);

	g_free(plugin->filename);
	g_free(plugin);
	plugin = NULL;
}


/* load active plugins at startup */
static void
load_active_plugins()
{
	guint i, len;

	if (app->active_plugins == NULL || (len = g_strv_length(app->active_plugins)) == 0)
		return;

	for (i = 0; i < len; i++)
	{
		if (NZV(app->active_plugins[i]))
			plugin_new(app->active_plugins[i], TRUE, FALSE);
	}
}


static void
load_plugins_from_path(const gchar *path)
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
		plugin = plugin_new(fname, FALSE, TRUE);
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


/* Load (but don't initialize) all plugins for the Plugin Manager dialog */
static void load_all_plugins(void)
{
	gchar *path;

	path = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "plugins", NULL);
	/* first load plugins in ~/.geany/plugins/, then in $prefix/lib/geany */
	load_plugins_from_path(path);
	g_free(path);
#ifdef G_OS_WIN32
	path = get_plugin_path();
#else
	path = g_strconcat(GEANY_LIBDIR, G_DIR_SEPARATOR_S "geany", NULL);
#endif
	load_plugins_from_path(path);

	g_free(path);
}


void plugins_init()
{
	GtkWidget *widget;

	geany_data_init();
	geany_object = geany_object_new();

	widget = gtk_menu_item_new_with_mnemonic(_("_Plugin Manager"));
	gtk_container_add(GTK_CONTAINER (geany_data.tools_menu), widget);
	gtk_widget_show(widget);
	g_signal_connect((gpointer) widget, "activate", G_CALLBACK(pm_show_dialog), NULL);

	separator = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(geany_data.tools_menu), separator);

	load_active_plugins();

	plugins_update_tools_menu();
}


void plugins_create_active_list()
{
	gint i = 0;
	GList *list;

	g_strfreev(app->active_plugins);

	if (active_plugin_list == NULL)
	{
		app->active_plugins = NULL;
		return;
	}

	app->active_plugins = g_new0(gchar*, g_list_length(active_plugin_list) + 1);
	for (list = g_list_first(active_plugin_list); list != NULL; list = list->next)
	{
		app->active_plugins[i] = g_strdup(((Plugin*)list->data)->filename);
		i++;
	}
	app->active_plugins[i] = NULL;
}


void plugins_free()
{
	if (active_plugin_list != NULL)
	{
		g_list_foreach(active_plugin_list, (GFunc) plugin_free,	GINT_TO_POINTER(PLUGIN_FREE_ALL));
		g_list_free(active_plugin_list);
	}

	g_object_unref(geany_object);
	geany_object = NULL; /* to mark the object as invalid for any code which tries to emit signals */
}


void plugins_update_document_sensitive(gboolean enabled)
{
	GList *item;

	for (item = active_plugin_list; item != NULL; item = g_list_next(item))
	{
		Plugin *plugin = item->data;

		if (plugin->fields.flags & PLUGIN_IS_DOCUMENT_SENSITIVE)
			gtk_widget_set_sensitive(plugin->fields.menu_item, enabled);
	}
}


static gint
plugin_has_menu(Plugin *a, Plugin *b)
{
	if (a->fields.menu_item != NULL)
		return 0;

	return 1;
}


void plugins_update_tools_menu()
{
	gboolean found;

	if (separator == NULL)
		return;

	found = (g_list_find_custom(active_plugin_list, NULL, (GCompareFunc) plugin_has_menu) != NULL);
	ui_widget_show_hide(separator, found);
}


/* Plugin Manager */

enum
{
	PLUGIN_COLUMN_CHECK = 0,
	PLUGIN_COLUMN_NAME,
	PLUGIN_COLUMN_FILE,
	PLUGIN_COLUMN_PLUGIN,
	PLUGIN_N_COLUMNS
};

typedef struct
{
	GtkWidget *dialog;
	GtkWidget *tree;
	GtkListStore *store;
	GtkWidget *description_label;
	GtkWidget *configure_button;
} PluginManagerWidgets;
static PluginManagerWidgets pm_widgets;


void pm_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	Plugin *p;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, PLUGIN_COLUMN_PLUGIN, &p, -1);

		if (p != NULL)
		{
			gchar *text;
			PluginInfo *pi;

			pi = p->info();
			text = g_strdup_printf(
				_("Plugin: %s %s\nDescription: %s\nAuthor(s): %s"),
				pi->name, pi->version, pi->description, pi->author);

			geany_wrap_label_set_text(GTK_LABEL(pm_widgets.description_label), text);
			g_free(text);

			gtk_widget_set_sensitive(pm_widgets.configure_button,
				p->configure != NULL && g_list_find(active_plugin_list, p) != NULL);
		}
	}
}


static void pm_plugin_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	gboolean old_state, state;
	gchar *file_name;
	GtkTreeIter iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	Plugin *p;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(pm_widgets.store), &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(pm_widgets.store), &iter,
		PLUGIN_COLUMN_CHECK, &old_state, PLUGIN_COLUMN_PLUGIN, &p, -1);
	if (p == NULL)
		return;
	state = ! old_state; /* toggle the state */

	/* save the filename of the plugin */
	file_name = g_strdup(p->filename);

	/* unload plugin module */
	if (!state)
		keybindings_write_to_file();	/* save shortcuts (only need this group, but it doesn't take long) */
	plugin_free(p, GINT_TO_POINTER(PLUGIN_FREE_ALL));

	/* reload plugin module and initialize it if item is checked */
	p = plugin_new(file_name, state, TRUE);
	if (state)
		keybindings_load_keyfile();		/* load shortcuts */

	/* update model */
	gtk_list_store_set(pm_widgets.store, &iter,
		PLUGIN_COLUMN_CHECK, state,
		PLUGIN_COLUMN_PLUGIN, p, -1);

	g_free(file_name);

	/* set again the sensitiveness of the configure button */
	gtk_widget_set_sensitive(pm_widgets.configure_button,
		p->configure != NULL && is_active_plugin(p));
}


static void pm_prepare_treeview(GtkWidget *tree, GtkListStore *store)
{
	GtkCellRenderer *text_renderer, *checkbox_renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter iter;
	GList *list;
	GtkTreeSelection *sel;

	checkbox_renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes(
		_("Active"), checkbox_renderer, "active", PLUGIN_COLUMN_CHECK, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	g_signal_connect((gpointer) checkbox_renderer, "toggled", G_CALLBACK(pm_plugin_toggled), NULL);

	text_renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
		_("Plugin"), text_renderer, "text", PLUGIN_COLUMN_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    column = gtk_tree_view_column_new_with_attributes(
		_("File"), text_renderer, "text", PLUGIN_COLUMN_FILE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(store), PLUGIN_COLUMN_NAME, GTK_SORT_ASCENDING);

	/* selection handling */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
	g_signal_connect((gpointer) sel, "changed", G_CALLBACK(pm_selection_changed), NULL);

	list = g_list_first(plugin_list);
	if (list == NULL)
	{
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, PLUGIN_COLUMN_CHECK, FALSE,
				PLUGIN_COLUMN_NAME, _("No plugins available."),
				PLUGIN_COLUMN_FILE, "", PLUGIN_COLUMN_PLUGIN, NULL, -1);
	}
	else
	{
		for (; list != NULL; list = list->next)
		{
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter,
				PLUGIN_COLUMN_CHECK, is_active_plugin(list->data),
				PLUGIN_COLUMN_NAME, ((Plugin*)list->data)->info()->name,
				PLUGIN_COLUMN_FILE, ((Plugin*)list->data)->filename,
				PLUGIN_COLUMN_PLUGIN, list->data,
				-1);
		}
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
}


void pm_on_configure_button_clicked(GtkButton *button, gpointer user_data)
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
			p->configure(pm_widgets.dialog);
		}
	}
}


static void pm_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (plugin_list != NULL)
	{
		/* remove all non-active plugins from the list */
		g_list_foreach(plugin_list, (GFunc) plugin_free, GINT_TO_POINTER(PLUGIN_FREE_NON_ACTIVE));
		g_list_free(plugin_list);
		plugin_list = NULL;
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


static void pm_show_dialog(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *vbox, *vbox2, *label_vbox, *hbox, *swin, *label, *label2;

	/* before showing the dialog, we need to create the list of available plugins */
	load_all_plugins();

	pm_widgets.dialog = gtk_dialog_new_with_buttons(_("Plugins"), GTK_WINDOW(app->window),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_STOCK_OK, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(pm_widgets.dialog));
	gtk_widget_set_name(pm_widgets.dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	gtk_window_set_default_size(GTK_WINDOW(pm_widgets.dialog), 400, 350);

	pm_widgets.tree = gtk_tree_view_new();
	pm_widgets.store = gtk_list_store_new(
		PLUGIN_N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	pm_prepare_treeview(pm_widgets.tree, pm_widgets.store);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(swin), pm_widgets.tree);

	label = gtk_label_new(_("Below is a list of available plugins. "
		"Select the plugins which should be loaded when Geany is started."));

	pm_widgets.configure_button = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);
	gtk_widget_set_sensitive(pm_widgets.configure_button, FALSE);
	g_signal_connect((gpointer) pm_widgets.configure_button, "clicked",
		G_CALLBACK(pm_on_configure_button_clicked), NULL);

	label2 = gtk_label_new(_("<b>Plugin details:</b>"));
	gtk_label_set_use_markup(GTK_LABEL(label2), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);
	pm_widgets.description_label = geany_wrap_label_new("");

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label2, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), pm_widgets.configure_button, FALSE, FALSE, 0);

	label_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(label_vbox), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(label_vbox), pm_widgets.description_label, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox2), swin, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), label_vbox, FALSE, FALSE, 0);

	g_signal_connect((gpointer) pm_widgets.dialog, "response",
		G_CALLBACK(pm_dialog_response), NULL);

	gtk_container_add(GTK_CONTAINER(vbox), vbox2);
	gtk_widget_show_all(pm_widgets.dialog);
}

#endif
