/*
 *      toolbar.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * @file toolbar.h
 * Toolbar (prefs).
 */
/* Utility functions to create the toolbar */

#include "geany.h"
#include "support.h"
#include "ui_utils.h"
#include "toolbar.h"
#include "callbacks.h"
#include "utils.h"
#include "dialogs.h"
#include "document.h"
#include "build.h"
#include "main.h"
#include "geanymenubuttonaction.h"
#include "geanyentryaction.h"

#include <string.h>
#include <glib/gstdio.h>


GeanyToolbarPrefs toolbar_prefs;
static GtkUIManager *uim;
static GtkActionGroup *group;
static GSList *plugin_items = NULL;

/* Available toolbar actions
 * Fields: name, stock_id, label, accelerator, tooltip, callback */
static const GtkActionEntry ui_entries[] = {
	/* custom actions defined in toolbar_init(): "New", "Open", "SearchEntry", "GotoEntry", "Build" */
	{ "Save", GTK_STOCK_SAVE, NULL, NULL, N_("Save the current file"), G_CALLBACK(on_toolbutton_save_clicked) },
	{ "SaveAs", GTK_STOCK_SAVE_AS, NULL, NULL, N_("Save as"), G_CALLBACK(on_save_as1_activate) },
	{ "SaveAll", GEANY_STOCK_SAVE_ALL, NULL, NULL, N_("Save all open files"), G_CALLBACK(on_save_all1_activate) },
	{ "Reload", GTK_STOCK_REVERT_TO_SAVED, NULL, NULL, N_("Reload the current file from disk"), G_CALLBACK(on_toolbutton_reload_clicked) },
	{ "Close", GTK_STOCK_CLOSE, NULL, NULL, N_("Close the current file"), G_CALLBACK(on_toolbutton_close_clicked) },
	{ "CloseAll", GEANY_STOCK_CLOSE_ALL, NULL, NULL, N_("Close all open files"), G_CALLBACK(on_toolbutton_close_all_clicked) },
	{ "Cut", GTK_STOCK_CUT, NULL, NULL, N_("Cut the current selection"), G_CALLBACK(on_cut1_activate) },
	{ "Copy", GTK_STOCK_COPY, NULL, NULL, N_("Copy the current selection"), G_CALLBACK(on_copy1_activate) },
	{ "Paste", GTK_STOCK_PASTE, NULL, NULL, N_("Paste the contents of the clipboard"), G_CALLBACK(on_paste1_activate) },
	{ "Delete", GTK_STOCK_DELETE, NULL, NULL, N_("Delete the current selection"), G_CALLBACK(on_delete1_activate) },
	{ "Undo", GTK_STOCK_UNDO, NULL, NULL, N_("Undo the last modification"), G_CALLBACK(on_undo1_activate) },
	{ "Redo", GTK_STOCK_REDO, NULL, NULL, N_("Redo the last modification"), G_CALLBACK(on_redo1_activate) },
	{ "NavBack", GTK_STOCK_GO_BACK, NULL, NULL, N_("Navigate back a location"), G_CALLBACK(on_back_activate) },
	{ "NavFor", GTK_STOCK_GO_FORWARD, NULL, NULL, N_("Navigate forward a location"), G_CALLBACK(on_forward_activate) },
	{ "Compile", GTK_STOCK_CONVERT, N_("Compile"), NULL, N_("Compile the current file"), G_CALLBACK(on_toolbutton_compile_clicked) },
	{ "Run", GTK_STOCK_EXECUTE, NULL, NULL, N_("Run or view the current file"), G_CALLBACK(on_toolbutton_run_clicked) },
	{ "Color", GTK_STOCK_SELECT_COLOR, N_("Color Chooser"), NULL, N_("Open a color chooser dialog, to interactively pick colors from a palette"), G_CALLBACK(on_show_color_chooser1_activate) },
	{ "ZoomIn", GTK_STOCK_ZOOM_IN, NULL, NULL, N_("Zoom in the text"), G_CALLBACK(on_zoom_in1_activate) },
	{ "ZoomOut", GTK_STOCK_ZOOM_OUT, NULL, NULL, N_("Zoom out the text"), G_CALLBACK(on_zoom_out1_activate) },
	{ "UnIndent", GTK_STOCK_UNINDENT, NULL, NULL, N_("Decrease indentation"), G_CALLBACK(on_menu_decrease_indent1_activate) },
	{ "Indent", GTK_STOCK_INDENT, NULL, NULL, N_("Increase indentation"), G_CALLBACK(on_menu_increase_indent1_activate) },
	{ "Search", GTK_STOCK_FIND, NULL, NULL, N_("Find the entered text in the current file"), G_CALLBACK(on_toolbutton_search_clicked) },
	{ "Goto", GTK_STOCK_JUMP_TO, NULL, NULL, N_("Jump to the entered line number"), G_CALLBACK(on_toolbutton_goto_clicked) },
	{ "Preferences", GTK_STOCK_PREFERENCES, NULL, NULL, N_("Show the preferences dialog"), G_CALLBACK(on_toolbutton_preferences_clicked) },
	{ "Quit", GTK_STOCK_QUIT, NULL, NULL, N_("Quit Geany"), G_CALLBACK(on_toolbutton_quit_clicked) },
	{ "Print", GTK_STOCK_PRINT, NULL, NULL, N_("Print document"), G_CALLBACK(on_print1_activate) },
	{ "Replace", GTK_STOCK_FIND_AND_REPLACE, NULL, NULL, N_("Replace text in the current document"), G_CALLBACK(on_replace1_activate) }
};
static const guint ui_entries_n = G_N_ELEMENTS(ui_entries);


/* fallback UI definition */
static const gchar *toolbar_markup =
"<ui>"
	"<toolbar name='GeanyToolbar'>"
	"<toolitem action='New'/>"
	"<toolitem action='Open'/>"
	"<toolitem action='Save'/>"
	"<toolitem action='SaveAll'/>"
	"<separator/>"
	"<toolitem action='Reload'/>"
	"<toolitem action='Close'/>"
	"<separator/>"
	"<toolitem action='NavBack'/>"
	"<toolitem action='NavFor'/>"
	"<separator/>"
	"<toolitem action='Compile'/>"
	"<toolitem action='Build'/>"
	"<toolitem action='Run'/>"
	"<separator/>"
	"<toolitem action='Color'/>"
	"<separator/>"
	"<toolitem action='SearchEntry'/>"
	"<toolitem action='Search'/>"
	"<separator/>"
	"<toolitem action='GotoEntry'/>"
	"<toolitem action='Goto'/>"
	"<separator/>"
	"<toolitem action='Quit'/>"
	"</toolbar>"
"</ui>";


/* Note: The returned widget pointer is only valid until the toolbar is reloaded. So, either
 * update the widget pointer in this case (i.e. request it again) or better use
 * toolbar_get_action_by_name() instead. The action objects will remain the same even when the
 * toolbar is reloaded. */
GtkWidget *toolbar_get_widget_by_name(const gchar *name)
{
	GtkWidget *widget;
	gchar *path;

	g_return_val_if_fail(name != NULL, NULL);

	path = g_strconcat("/ui/GeanyToolbar/", name, NULL);
	widget = gtk_ui_manager_get_widget(uim, path);

	g_free(path);
	return widget;
}


/* Note: The returned widget pointer is only valid until the toolbar is reloaded. See
 * toolbar_get_widget_by_name for details(). */
GtkWidget *toolbar_get_widget_child_by_name(const gchar *name)
{
	GtkWidget *widget = toolbar_get_widget_by_name(name);

	if (G_LIKELY(widget != NULL))
		return gtk_bin_get_child(GTK_BIN(widget));
	else
		return NULL;
}


GtkAction *toolbar_get_action_by_name(const gchar *name)
{
	g_return_val_if_fail(name != NULL, NULL);

	return gtk_action_group_get_action(group, name);
}


static void toolbar_item_destroy_cb(GtkWidget *widget, G_GNUC_UNUSED gpointer data)
{
	plugin_items = g_slist_remove(plugin_items, widget);
}


void toolbar_item_ref(GtkToolItem *item)
{
	g_return_if_fail(item != NULL);

	plugin_items = g_slist_append(plugin_items, item);
	g_signal_connect(item, "destroy", G_CALLBACK(toolbar_item_destroy_cb), NULL);
}


static GtkWidget *toolbar_reload(const gchar *markup)
{
	gint i;
	GSList *l;
	GtkWidget *entry;
	GError *error = NULL;
	gchar *filename;
	static guint merge_id = 0;
	GtkWidget *toolbar_new_file_menu = NULL;
	GtkWidget *toolbar_recent_files_menu = NULL;
	GtkWidget *toolbar_build_menu = NULL;

	/* Cleanup old toolbar */
	if (merge_id > 0)
	{
		/* ref plugins toolbar items to keep them after we destroyed the toolbar */
		foreach_slist(l, plugin_items)
		{
			g_object_ref(l->data);
			gtk_container_remove(GTK_CONTAINER(main_widgets.toolbar), GTK_WIDGET(l->data));
		}
		/* ref and hold the submenus of the New, Open and Build toolbar items */
		toolbar_new_file_menu = geany_menu_button_action_get_menu(
					GEANY_MENU_BUTTON_ACTION(gtk_action_group_get_action(group, "New")));
		g_object_ref(toolbar_new_file_menu);
		toolbar_recent_files_menu = geany_menu_button_action_get_menu(
					GEANY_MENU_BUTTON_ACTION(gtk_action_group_get_action(group, "Open")));
		g_object_ref(toolbar_recent_files_menu);
		toolbar_build_menu = geany_menu_button_action_get_menu(
					GEANY_MENU_BUTTON_ACTION(gtk_action_group_get_action(group, "Build")));
		g_object_ref(toolbar_build_menu);

		/* Get rid of it! */
		gtk_widget_destroy(main_widgets.toolbar);

		gtk_ui_manager_remove_ui(uim, merge_id);
		gtk_ui_manager_ensure_update(uim);
	}

	if (markup != NULL)
	{
		merge_id = gtk_ui_manager_add_ui_from_string(uim, markup, -1, &error);
	}
	else
	{
		/* Load the toolbar UI XML file from disk (first from config dir, then try data dir) */
		filename = g_build_filename(app->configdir, "ui_toolbar.xml", NULL);
		merge_id = gtk_ui_manager_add_ui_from_file(uim, filename, &error);
		if (merge_id == 0)
		{
			if (! g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
				geany_debug("Loading user toolbar UI definition failed (%s).", error->message);
			g_error_free(error);
			error = NULL;

			SETPTR(filename, g_build_filename(app->datadir, "ui_toolbar.xml", NULL));
			merge_id = gtk_ui_manager_add_ui_from_file(uim, filename, &error);
		}
		g_free(filename);
	}
	if (error != NULL)
	{
		geany_debug("UI creation failed, using internal fallback definition. Error message: %s",
			error->message);
		g_error_free(error);
		/* finally load the internally defined markup as fallback */
		merge_id = gtk_ui_manager_add_ui_from_string(uim, toolbar_markup, -1, NULL);
	}
	main_widgets.toolbar = gtk_ui_manager_get_widget(uim, "/ui/GeanyToolbar");
	ui_init_toolbar_widgets();

	/* add the toolbar again to the main window */
	if (toolbar_prefs.append_to_menu)
	{
		GtkWidget *hbox_menubar = ui_lookup_widget(main_widgets.window, "hbox_menubar");
		gtk_box_pack_start(GTK_BOX(hbox_menubar), main_widgets.toolbar, TRUE, TRUE, 0);
		gtk_box_reorder_child(GTK_BOX(hbox_menubar), main_widgets.toolbar, 1);
	}
	else
	{
		GtkWidget *box = ui_lookup_widget(main_widgets.window, "vbox1");

		gtk_box_pack_start(GTK_BOX(box), main_widgets.toolbar, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(box), main_widgets.toolbar, 1);
	}
	gtk_widget_show(main_widgets.toolbar);

	/* re-add und unref the plugin toolbar items */
	i = toolbar_get_insert_position();
	foreach_slist(l, plugin_items)
	{
		gtk_toolbar_insert(GTK_TOOLBAR(main_widgets.toolbar), l->data, i);
		g_object_unref(l->data);
		i++;
	}
	/* re-add und unref the submenus of menu toolbar items */
	if (toolbar_new_file_menu != NULL)
	{
		geany_menu_button_action_set_menu(GEANY_MENU_BUTTON_ACTION(
			gtk_action_group_get_action(group, "New")), toolbar_new_file_menu);
		g_object_unref(toolbar_new_file_menu);
	}
	if (toolbar_recent_files_menu != NULL)
	{
		geany_menu_button_action_set_menu(GEANY_MENU_BUTTON_ACTION(
			gtk_action_group_get_action(group, "Open")), toolbar_recent_files_menu);
		g_object_unref(toolbar_recent_files_menu);
	}
	if (toolbar_build_menu != NULL)
	{
		geany_menu_button_action_set_menu(GEANY_MENU_BUTTON_ACTION(
			gtk_action_group_get_action(group, "Build")), toolbar_build_menu);
		g_object_unref(toolbar_build_menu);
	}

	/* update button states */
	if (main_status.main_window_realized)
	{
		GeanyDocument *doc = document_get_current();
		gboolean doc_changed = (doc != NULL) ? doc->changed : FALSE;

		ui_document_buttons_update();
		ui_save_buttons_toggle(doc_changed); /* update save all */
		ui_update_popup_reundo_items(doc);

		toolbar_apply_settings();
	}

	/* Signals */
	g_signal_connect(main_widgets.toolbar, "button-press-event",
		G_CALLBACK(toolbar_popup_menu), NULL);
	g_signal_connect(main_widgets.toolbar, "key-press-event",
		G_CALLBACK(on_escape_key_press_event), NULL);

	/* We don't need to disconnect those signals as this is done automatically when the entry
	 * widgets are destroyed, happens when the toolbar itself is destroyed. */
	entry = toolbar_get_widget_child_by_name("SearchEntry");
	if (entry != NULL)
		g_signal_connect(entry, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	entry = toolbar_get_widget_child_by_name("GotoEntry");
	if (entry != NULL)
		g_signal_connect(entry, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);

	return main_widgets.toolbar;
}


static void toolbar_notify_style_cb(GObject *object, GParamSpec *arg1, gpointer data)
{
	const gchar *arg_name = g_param_spec_get_name(arg1);
	gint value;

	if (toolbar_prefs.use_gtk_default_style && utils_str_equal(arg_name, "gtk-toolbar-style"))
	{
		value = ui_get_gtk_settings_integer(arg_name, toolbar_prefs.icon_style);
		gtk_toolbar_set_style(GTK_TOOLBAR(main_widgets.toolbar), value);
	}
	else if (toolbar_prefs.use_gtk_default_icon && utils_str_equal(arg_name, "gtk-toolbar-size"))
	{
		value = ui_get_gtk_settings_integer(arg_name, toolbar_prefs.icon_size);
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(main_widgets.toolbar), value);
	}
}


GtkWidget *toolbar_init(void)
{
	GtkWidget *toolbar;
	GtkAction *action_new;
	GtkAction *action_open;
	GtkAction *action_build;
	GtkAction *action_searchentry;
	GtkAction *action_gotoentry;
	GtkSettings *gtk_settings;

	uim = gtk_ui_manager_new();
	group = gtk_action_group_new("GeanyToolbar");

	gtk_action_group_set_translation_domain(group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions(group, ui_entries, ui_entries_n, NULL);

	/* Create our custom actions */
	action_new = geany_menu_button_action_new(
		"New", NULL,
		_("Create a new file"),
		_("Create a new file from a template"),
		GTK_STOCK_NEW);
	g_signal_connect(action_new, "button-clicked", G_CALLBACK(on_toolbutton_new_clicked), NULL);
	gtk_action_group_add_action(group, action_new);

	action_open = geany_menu_button_action_new(
		"Open", NULL,
		_("Open an existing file"),
		_("Open a recent file"),
		GTK_STOCK_OPEN);
	g_signal_connect(action_open, "button-clicked", G_CALLBACK(on_toolbutton_open_clicked), NULL);
	gtk_action_group_add_action(group, action_open);

	action_build = geany_menu_button_action_new(
		"Build", NULL,
		_("Build the current file"),
		_("Choose more build actions"),
		GEANY_STOCK_BUILD);
	g_signal_connect(action_build, "button-clicked",
		G_CALLBACK(build_toolbutton_build_clicked), NULL);
	gtk_action_group_add_action(group, action_build);

	action_searchentry = geany_entry_action_new(
		"SearchEntry", _("Search Field"), _("Find the entered text in the current file"), FALSE);
	g_signal_connect(action_searchentry, "entry-activate",
		G_CALLBACK(on_toolbar_search_entry_activate), GINT_TO_POINTER(FALSE));
	g_signal_connect(action_searchentry, "entry-activate-backward",
		G_CALLBACK(on_toolbar_search_entry_activate), GINT_TO_POINTER(TRUE));
	g_signal_connect(action_searchentry, "entry-changed",
		G_CALLBACK(on_toolbar_search_entry_changed), NULL);
	gtk_action_group_add_action(group, action_searchentry);

	action_gotoentry = geany_entry_action_new(
		"GotoEntry", _("Goto Field"), _("Jump to the entered line number"), TRUE);
	g_signal_connect(action_gotoentry, "entry-activate",
		G_CALLBACK(on_toolbutton_goto_entry_activate), NULL);
	gtk_action_group_add_action(group, action_gotoentry);

	gtk_ui_manager_insert_action_group(uim, group, 0);

	toolbar = toolbar_reload(NULL);

	gtk_settings = gtk_widget_get_settings(GTK_WIDGET(toolbar));
	if (gtk_settings != NULL)
	{
		g_signal_connect(gtk_settings, "notify::gtk-toolbar-style",
			G_CALLBACK(toolbar_notify_style_cb), NULL);
	}

	return toolbar;
}


void toolbar_update_ui(void)
{
	static GtkWidget *hbox_menubar = NULL;
	static GtkWidget *menubar = NULL;
	GtkWidget *menubar_toolbar_separator = NULL;
	GtkWidget *parent;
	GtkToolItem *first_item;

	if (menubar == NULL)
	{	/* cache widget pointers */
		hbox_menubar = ui_lookup_widget(main_widgets.window, "hbox_menubar");
		menubar = ui_lookup_widget(main_widgets.window, "menubar1");
	}
	/* the separator between the menubar and the toolbar */
	first_item = gtk_toolbar_get_nth_item(GTK_TOOLBAR(main_widgets.toolbar), 0);
	if (first_item != NULL && GTK_IS_SEPARATOR_TOOL_ITEM(first_item))
	{
		gtk_widget_destroy(GTK_WIDGET(first_item));
	}

	parent = gtk_widget_get_parent(main_widgets.toolbar);

	if (toolbar_prefs.append_to_menu)
	{
		if (parent != NULL)
		{
			if (parent != hbox_menubar)
			{	/* here we manually 'reparent' the toolbar, gtk_widget_reparent() doesn't
				 * like to do it */
				g_object_ref(main_widgets.toolbar);

				gtk_container_remove(GTK_CONTAINER(parent), main_widgets.toolbar);
				gtk_box_pack_start(GTK_BOX(hbox_menubar), main_widgets.toolbar, TRUE, TRUE, 0);
				gtk_box_reorder_child(GTK_BOX(hbox_menubar), main_widgets.toolbar, 1);

				g_object_unref(main_widgets.toolbar);
			}
		}
		else
			gtk_box_pack_start(GTK_BOX(hbox_menubar), main_widgets.toolbar, TRUE, TRUE, 0);

		/* the separator between the menubar and the toolbar */
		menubar_toolbar_separator = GTK_WIDGET(gtk_separator_tool_item_new());
		gtk_widget_show(menubar_toolbar_separator);
		gtk_toolbar_insert(GTK_TOOLBAR(main_widgets.toolbar),
			GTK_TOOL_ITEM(menubar_toolbar_separator), 0);
	}
	else
	{
		GtkWidget *box = ui_lookup_widget(main_widgets.window, "vbox1");

		if (parent != NULL)
		{
			if (parent != box)
			{
				g_object_ref(main_widgets.toolbar);

				gtk_container_remove(GTK_CONTAINER(parent), main_widgets.toolbar);
				gtk_box_pack_start(GTK_BOX(box), main_widgets.toolbar, FALSE, FALSE, 0);
				gtk_box_reorder_child(GTK_BOX(box), main_widgets.toolbar, 1);

				g_object_unref(main_widgets.toolbar);
			}
		}
		else
		{
			gtk_box_pack_start(GTK_BOX(box), main_widgets.toolbar, FALSE, FALSE, 0);
			gtk_box_reorder_child(GTK_BOX(box), main_widgets.toolbar, 1);
		}
	}
	/* we need to adjust the packing flags for the menubar to expand it if it is alone in the
	 * hbox and not expand it if the toolbar is appended */
	gtk_box_set_child_packing(GTK_BOX(hbox_menubar), menubar,
		! (toolbar_prefs.visible && toolbar_prefs.append_to_menu), TRUE, 0, GTK_PACK_START);
}


/*  Returns the position for adding new toolbar items. The returned position can be used
 *  to add new toolbar items with @c gtk_toolbar_insert(). The toolbar object can be accessed
 *  with @a geany->main_widgets->toolbar.
 *  The position is always the last one before the Quit button or the very last position if the
 *  Quit button is not the last toolbar item.
 *
 *  @return The position for new toolbar items.
 */
gint toolbar_get_insert_position(void)
{
	GtkWidget *quit = toolbar_get_widget_by_name("Quit");
	gint quit_pos = -1, pos;

	if (quit != NULL)
		quit_pos = gtk_toolbar_get_item_index(GTK_TOOLBAR(main_widgets.toolbar), GTK_TOOL_ITEM(quit));

	pos = gtk_toolbar_get_n_items(GTK_TOOLBAR(main_widgets.toolbar));
	if (quit_pos == (pos - 1))
	{
		/* if the toolbar item before the quit button is a separator, insert new items before */
		if (GTK_IS_SEPARATOR_TOOL_ITEM(gtk_toolbar_get_nth_item(
			GTK_TOOLBAR(main_widgets.toolbar), quit_pos - 1)))
		{
			return quit_pos - 1;
		}
		/* else return the position of the quit button to insert new items before */
		return quit_pos;
	}

	return pos;
}


void toolbar_finalize(void)
{
	GeanyMenubuttonAction *open_action = GEANY_MENU_BUTTON_ACTION(toolbar_get_action_by_name("Open"));
	g_object_unref(geany_menu_button_action_get_menu(open_action));
	geany_menu_button_action_set_menu(open_action, NULL);

	/* unref'ing the GtkUIManager object will destroy all its widgets unless they were ref'ed */
	g_object_unref(uim);
	g_object_unref(group);

	g_slist_free(plugin_items);
}


void toolbar_show_hide(void)
{
	ignore_callback = TRUE;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
		ui_lookup_widget(main_widgets.window, "menu_show_toolbar1")), toolbar_prefs.visible);
	ui_widget_show_hide(main_widgets.toolbar, toolbar_prefs.visible);
	ignore_callback = FALSE;
}


/* sets the icon style of the toolbar */
static void toolbar_set_icon_style(void)
{
	gint icon_style;

	icon_style = toolbar_prefs.icon_style;

	if (toolbar_prefs.use_gtk_default_style)
		icon_style = ui_get_gtk_settings_integer("gtk-toolbar-style", toolbar_prefs.icon_style);

	gtk_toolbar_set_style(GTK_TOOLBAR(main_widgets.toolbar), icon_style);
}


/* sets the icon size of the toolbar */
static void toolbar_set_icon_size(void)
{
	gint icon_size;

	icon_size = toolbar_prefs.icon_size;

	if (toolbar_prefs.use_gtk_default_icon)
		icon_size = ui_get_gtk_settings_integer("gtk-toolbar-icon-size", toolbar_prefs.icon_size);

	gtk_toolbar_set_icon_size(GTK_TOOLBAR(main_widgets.toolbar), icon_size);
}


void toolbar_apply_settings(void)
{
	toolbar_set_icon_style();
	toolbar_set_icon_size();
}


#define TB_EDITOR_SEPARATOR _("Separator")
#define TB_EDITOR_SEPARATOR_LABEL _("--- Separator ---")
typedef struct
{
	GtkWidget *dialog;

	GtkTreeView *tree_available;
	GtkTreeView *tree_used;

	GtkListStore *store_available;
	GtkListStore *store_used;

	GtkTreePath *last_drag_path;
	GtkTreeViewDropPosition last_drag_pos;

	GtkWidget *drag_source;
} TBEditorWidget;

static const GtkTargetEntry tb_editor_dnd_targets[] =
{
	{ "GEANY_TB_EDITOR_ROW", 0, 0 }
};
static const gint tb_editor_dnd_targets_len = G_N_ELEMENTS(tb_editor_dnd_targets);

enum
{
	TB_EDITOR_COL_ACTION,
	TB_EDITOR_COL_LABEL,
	TB_EDITOR_COL_ICON,
	TB_EDITOR_COLS_MAX
};

static void tb_editor_handler_start_element(GMarkupParseContext *context, const gchar *element_name,
											const gchar **attribute_names,
											const gchar **attribute_values, gpointer data,
											GError **error)
{
	gint i;
	GSList **actions = data;

	/* This is very basic parsing, stripped down any error checking, requires a valid UI markup. */
	if (utils_str_equal(element_name, "separator"))
		*actions = g_slist_append(*actions, g_strdup(TB_EDITOR_SEPARATOR));

	for (i = 0; attribute_names[i] != NULL; i++)
	{
		if (utils_str_equal(attribute_names[i], "action"))
		{
			*actions = g_slist_append(*actions, g_strdup(attribute_values[i]));
		}
	}
}


static const GMarkupParser tb_editor_xml_parser =
{
	tb_editor_handler_start_element, NULL, NULL, NULL, NULL
};


static GSList *tb_editor_parse_ui(const gchar *buffer, gssize length, GError **error)
{
	GMarkupParseContext *context;
	GSList *list = NULL;

	context = g_markup_parse_context_new(&tb_editor_xml_parser, 0, &list, NULL);
	g_markup_parse_context_parse(context, buffer, length, error);
	g_markup_parse_context_free(context);

	return list;
}


static void tb_editor_set_item_values(const gchar *name, GtkListStore *store, GtkTreeIter *iter)
{
	gchar *icon = NULL;
	gchar *label = NULL;
	gchar *label_clean = NULL;
	GtkAction *action;

	action = gtk_action_group_get_action(group, name);
	if (action == NULL)
	{
		if (utils_str_equal(name, TB_EDITOR_SEPARATOR))
			label_clean = g_strdup(TB_EDITOR_SEPARATOR_LABEL);
		else
			return;
	}
	else
	{
		g_object_get(action, "icon-name", &icon, NULL);
		if (icon == NULL)
			g_object_get(action, "stock-id", &icon, NULL);

		g_object_get(action, "label", &label, NULL);
		if (label != NULL)
			label_clean = utils_str_remove_chars(g_strdup(label), "_");
	}

	gtk_list_store_set(store, iter,
		TB_EDITOR_COL_ACTION, name,
		TB_EDITOR_COL_LABEL, label_clean,
		TB_EDITOR_COL_ICON, icon,
		-1);

	g_free(icon);
	g_free(label);
	g_free(label_clean);
}


static void tb_editor_scroll_to_iter(GtkTreeView *treeview, GtkTreeIter *iter)
{
	GtkTreePath *path = gtk_tree_model_get_path(gtk_tree_view_get_model(treeview), iter);
	gtk_tree_view_scroll_to_cell(treeview, path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free(path);
}


static void tb_editor_free_path(TBEditorWidget *tbw)
{
	if (tbw->last_drag_path != NULL)
	{
		gtk_tree_path_free(tbw->last_drag_path);
		tbw->last_drag_path = NULL;
	}
}


static void tb_editor_btn_remove_clicked_cb(GtkWidget *button, TBEditorWidget *tbw)
{
	GtkTreeModel *model_used;
	GtkTreeSelection *selection_used;
	GtkTreeIter iter_used, iter_new;
	gchar *action_name;

	selection_used = gtk_tree_view_get_selection(tbw->tree_used);
	if (gtk_tree_selection_get_selected(selection_used, &model_used, &iter_used))
	{
		gtk_tree_model_get(model_used, &iter_used, TB_EDITOR_COL_ACTION, &action_name, -1);
		if (gtk_list_store_remove(tbw->store_used, &iter_used))
			gtk_tree_selection_select_iter(selection_used, &iter_used);

		if (! utils_str_equal(action_name, TB_EDITOR_SEPARATOR))
		{
			gtk_list_store_append(tbw->store_available, &iter_new);
			tb_editor_set_item_values(action_name, tbw->store_available, &iter_new);
			tb_editor_scroll_to_iter(tbw->tree_available, &iter_new);
		}

		g_free(action_name);
	}
}


static void tb_editor_btn_add_clicked_cb(GtkWidget *button, TBEditorWidget *tbw)
{
	GtkTreeModel *model_available;
	GtkTreeSelection *selection_available, *selection_used;
	GtkTreeIter iter_available, iter_new, iter_selected;
	gchar *action_name;

	selection_available = gtk_tree_view_get_selection(tbw->tree_available);
	if (gtk_tree_selection_get_selected(selection_available, &model_available, &iter_available))
	{
		gtk_tree_model_get(model_available, &iter_available,
			TB_EDITOR_COL_ACTION, &action_name, -1);
		if (! utils_str_equal(action_name, TB_EDITOR_SEPARATOR))
		{
			if (gtk_list_store_remove(tbw->store_available, &iter_available))
				gtk_tree_selection_select_iter(selection_available, &iter_available);
		}

		selection_used = gtk_tree_view_get_selection(tbw->tree_used);
		if (gtk_tree_selection_get_selected(selection_used, NULL, &iter_selected))
			gtk_list_store_insert_before(tbw->store_used, &iter_new, &iter_selected);
		else
			gtk_list_store_append(tbw->store_used, &iter_new);

		tb_editor_set_item_values(action_name, tbw->store_used, &iter_new);
		tb_editor_scroll_to_iter(tbw->tree_used, &iter_new);

		g_free(action_name);
	}
}


static gboolean tb_editor_drag_motion_cb(GtkWidget *widget, GdkDragContext *drag_context,
										 gint x, gint y, guint ltime, TBEditorWidget *tbw)
{
	if (tbw->last_drag_path != NULL)
		gtk_tree_path_free(tbw->last_drag_path);
	gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(widget),
		&(tbw->last_drag_path), &(tbw->last_drag_pos));

	return FALSE;
}


static void tb_editor_drag_data_get_cb(GtkWidget *widget, GdkDragContext *context,
									   GtkSelectionData *data, guint info, guint ltime,
									   TBEditorWidget *tbw)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GdkAtom atom;
	gchar *name;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
	if (! gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	gtk_tree_model_get(model, &iter, TB_EDITOR_COL_ACTION, &name, -1);
	if (G_UNLIKELY(! NZV(name)))
		return;

	atom = gdk_atom_intern(tb_editor_dnd_targets[0].target, FALSE);
	gtk_selection_data_set(data, atom, 8, (guchar*) name, strlen(name));

	g_free(name);

	tbw->drag_source = widget;
}


static void tb_editor_drag_data_rcvd_cb(GtkWidget *widget, GdkDragContext *context,
										gint x, gint y, GtkSelectionData *data, guint info,
										guint ltime, TBEditorWidget *tbw)
{
	GtkTreeView *tree = GTK_TREE_VIEW(widget);
	gboolean del = FALSE;

	if (data->length >= 0 && data->format == 8)
	{
		gboolean is_sep;
		gchar *text = NULL;

		text = (gchar*) data->data;
		is_sep = utils_str_equal(text, TB_EDITOR_SEPARATOR);
		/* If the source of the action is equal to the target, we do just re-order and so need
		 * to delete the separator to get it moved, not just copied. */
		if (is_sep && widget == tbw->drag_source)
			is_sep = FALSE;

		if (tree != tbw->tree_available || ! is_sep)
		{
			GtkTreeIter iter, iter_before, *iter_before_ptr;
			GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(tree));

			if (tbw->last_drag_path != NULL)
			{
				gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter_before, tbw->last_drag_path);

				if (gtk_list_store_iter_is_valid(store, &iter_before))
					iter_before_ptr = &iter_before;
				else
					iter_before_ptr = NULL;

				if (tbw->last_drag_pos == GTK_TREE_VIEW_DROP_BEFORE ||
					tbw->last_drag_pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)
					gtk_list_store_insert_before(store, &iter, iter_before_ptr);
				else
					gtk_list_store_insert_after(store, &iter, iter_before_ptr);
			}
			else
				gtk_list_store_append(store, &iter);

			tb_editor_set_item_values(text, store, &iter);
			tb_editor_scroll_to_iter(tree, &iter);
		}
		if (tree != tbw->tree_used || ! is_sep)
			del = TRUE;
	}

	tbw->drag_source = NULL; /* reset the value just to be sure */
	tb_editor_free_path(tbw);
	gtk_drag_finish(context, TRUE, del, ltime);
}


static gboolean tb_editor_foreach_used(GtkTreeModel *model, GtkTreePath *path,
									   GtkTreeIter *iter, gpointer data)
{
	gchar *action_name;

	gtk_tree_model_get(model, iter, TB_EDITOR_COL_ACTION, &action_name, -1);

	if (utils_str_equal(action_name, TB_EDITOR_SEPARATOR))
		g_string_append_printf(data, "\t\t<separator/>\n");
	else if (G_LIKELY(NZV(action_name)))
		g_string_append_printf(data, "\t\t<toolitem action='%s' />\n", action_name);

	g_free(action_name);
	return FALSE;
}


static void tb_editor_write_markup(TBEditorWidget *tbw)
{
	/* <ui> must be the first tag, otherwise gtk_ui_manager_add_ui_from_string() will fail. */
	const gchar *template = "<ui>\n<!--\n\
This is Geany's toolbar UI definition.\nThe DTD can be found at \n\
http://library.gnome.org/devel/gtk/stable/GtkUIManager.html#GtkUIManager.description.\n\n\
You can re-order all items and freely add and remove available actions.\n\
You cannot add new actions which are not listed in the documentation.\n\
Everything you add or change must be inside the /ui/toolbar/ path.\n\n\
For changes to take effect, you need to restart Geany. Alternatively you can use the toolbar\n\
editor in Geany.\n\n\
A list of available actions can be found in the documentation included with Geany or\n\
at http://www.geany.org/manual/current/index.html#customizing-the-toolbar.\n-->\n\
\t<toolbar name='GeanyToolbar'>\n";
	gchar *filename;
	GString *str = g_string_new(template);

	gtk_tree_model_foreach(GTK_TREE_MODEL(tbw->store_used), tb_editor_foreach_used, str);

	g_string_append(str, "\n\t</toolbar>\n</ui>\n");

	toolbar_reload(str->str);

	filename = g_build_filename(app->configdir, "ui_toolbar.xml", NULL);
	utils_write_file(filename, str->str);
	g_free(filename);

	g_string_free(str, TRUE);
}


static void tb_editor_available_items_changed_cb(GtkTreeModel *model, GtkTreePath *arg1,
												 GtkTreeIter *arg2, TBEditorWidget *tbw)
{
	tb_editor_write_markup(tbw);
}


static void tb_editor_available_items_deleted_cb(GtkTreeModel *model, GtkTreePath *arg1,
												 TBEditorWidget *tbw)
{
	tb_editor_write_markup(tbw);
}


static TBEditorWidget *tb_editor_create_dialog(GtkWindow *parent)
{
	GtkWidget *dialog, *vbox, *hbox, *vbox_buttons, *button_add, *button_remove;
	GtkWidget *swin_available, *swin_used, *tree_available, *tree_used, *label;
	GtkCellRenderer *text_renderer, *icon_renderer;
	GtkTreeViewColumn *column;
	TBEditorWidget *tbw = g_new(TBEditorWidget, 1);

	if (parent == NULL)
		parent = GTK_WINDOW(main_widgets.window);

	dialog = gtk_dialog_new_with_buttons(_("Customize Toolbar"),
				parent,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 400);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	tbw->store_available = gtk_list_store_new(TB_EDITOR_COLS_MAX,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	tbw->store_used = gtk_list_store_new(TB_EDITOR_COLS_MAX,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	label = gtk_label_new(
		_("Select items to be displayed on the toolbar. Items can be reordered by drag and drop."));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

	tree_available = gtk_tree_view_new();
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree_available), GTK_TREE_MODEL(tbw->store_available));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_available), TRUE);
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(tbw->store_available), TB_EDITOR_COL_LABEL, GTK_SORT_ASCENDING);

	icon_renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
		NULL, icon_renderer, "stock-id", TB_EDITOR_COL_ICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_available), column);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Available Items"), text_renderer, "text", TB_EDITOR_COL_LABEL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_available), column);

	swin_available = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin_available),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin_available), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(swin_available), tree_available);

	tree_used = gtk_tree_view_new();
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree_used), GTK_TREE_MODEL(tbw->store_used));
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree_used), TRUE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tree_used), TRUE);

	icon_renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes(
		NULL, icon_renderer, "stock-id", TB_EDITOR_COL_ICON, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_used), column);

	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Displayed Items"), text_renderer, "text", TB_EDITOR_COL_LABEL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_used), column);

	swin_used = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin_used),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin_used), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(swin_used), tree_used);

	/* drag'n'drop */
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tree_available), GDK_BUTTON1_MASK,
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(tree_available),
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	g_signal_connect(tree_available, "drag-data-get",
		G_CALLBACK(tb_editor_drag_data_get_cb), tbw);
	g_signal_connect(tree_available, "drag-data-received",
		G_CALLBACK(tb_editor_drag_data_rcvd_cb), tbw);
	g_signal_connect(tree_available, "drag-motion",
		G_CALLBACK(tb_editor_drag_motion_cb), tbw);

	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(tree_used), GDK_BUTTON1_MASK,
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(tree_used),
		tb_editor_dnd_targets, tb_editor_dnd_targets_len, GDK_ACTION_MOVE);
	g_signal_connect(tree_used, "drag-data-get",
		G_CALLBACK(tb_editor_drag_data_get_cb), tbw);
	g_signal_connect(tree_used, "drag-data-received",
		G_CALLBACK(tb_editor_drag_data_rcvd_cb), tbw);
	g_signal_connect(tree_used, "drag-motion",
		G_CALLBACK(tb_editor_drag_motion_cb), tbw);


	button_add = ui_button_new_with_image(GTK_STOCK_GO_FORWARD, NULL);
	button_remove = ui_button_new_with_image(GTK_STOCK_GO_BACK, NULL);
	g_signal_connect(button_add, "clicked", G_CALLBACK(tb_editor_btn_add_clicked_cb), tbw);
	g_signal_connect(button_remove, "clicked", G_CALLBACK(tb_editor_btn_remove_clicked_cb), tbw);

	vbox_buttons = gtk_vbox_new(FALSE, 6);
	/* FIXME this is a little hack'ish, any better ideas? */
	gtk_box_pack_start(GTK_BOX(vbox_buttons), gtk_label_new(""), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_buttons), button_add, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_buttons), button_remove, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_buttons), gtk_label_new(""), TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), swin_available, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox_buttons, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), swin_used, TRUE, TRUE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);

	g_object_unref(tbw->store_available);
	g_object_unref(tbw->store_used);

	tbw->dialog = dialog;
	tbw->tree_available = GTK_TREE_VIEW(tree_available);
	tbw->tree_used = GTK_TREE_VIEW(tree_used);

	tbw->last_drag_path = NULL;

	return tbw;
}


void toolbar_configure(GtkWindow *parent)
{
	gchar *markup;
	const gchar *name;
	GSList *sl, *used_items;
	GList *l, *all_items;
	GtkTreeIter iter;
	GtkTreePath *path;
	TBEditorWidget *tbw;

	/* read the current active toolbar items */
	markup = gtk_ui_manager_get_ui(uim);
	used_items = tb_editor_parse_ui(markup, -1, NULL);
	g_free(markup);

	/* get all available actions */
	all_items = gtk_action_group_list_actions(group);

	/* create the GUI */
	tbw = tb_editor_create_dialog(parent);

	/* fill the stores */
	gtk_list_store_insert_with_values(tbw->store_available, NULL, -1,
		TB_EDITOR_COL_ACTION, TB_EDITOR_SEPARATOR,
		TB_EDITOR_COL_LABEL, TB_EDITOR_SEPARATOR_LABEL,
		-1);
	foreach_list(l, all_items)
	{
		name = gtk_action_get_name(l->data);
		if (g_slist_find_custom(used_items, name, (GCompareFunc) strcmp) == NULL)
		{
			gtk_list_store_append(tbw->store_available, &iter);
			tb_editor_set_item_values(name, tbw->store_available, &iter);
		}
	}
	foreach_slist(sl, used_items)
	{
		gtk_list_store_append(tbw->store_used, &iter);
		tb_editor_set_item_values(sl->data, tbw->store_used, &iter);
	}
	/* select first item */
	path = gtk_tree_path_new_from_string("0");
	gtk_tree_selection_select_path(gtk_tree_view_get_selection(tbw->tree_used), path);
	gtk_tree_path_free(path);

	/* connect the changed signals after populating the store */
	g_signal_connect(tbw->store_used, "row-changed",
		G_CALLBACK(tb_editor_available_items_changed_cb), tbw);
	g_signal_connect(tbw->store_used, "row-deleted",
		G_CALLBACK(tb_editor_available_items_deleted_cb), tbw);

	/* run it */
	gtk_dialog_run(GTK_DIALOG(tbw->dialog));

	gtk_widget_destroy(tbw->dialog);

	g_slist_foreach(used_items, (GFunc) g_free, NULL);
	g_slist_free(used_items);
	g_list_free(all_items);
	tb_editor_free_path(tbw);
	g_free(tbw);
}
