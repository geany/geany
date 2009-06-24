/*
 *      toolbar.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *
 * $Id$
 */


/** @file toolbar.c
 * Utility functions to create the toolbar.
 */

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



GeanyToolbarPrefs toolbar_prefs;
static GtkUIManager *uim;
static GtkActionGroup *group;
static GSList *plugin_items = NULL;

/* Available toolbar actions
 * Fields: name, stock_id, label, accelerator, tooltip, callback */
const GtkActionEntry ui_entries[] = {
	/* custom actions defined in toolbar_init(): "New", "Open", "SearchEntry", "GotoEntry", "Build" */
	{ "Save", GTK_STOCK_SAVE, NULL, NULL, N_("Save the current file"), G_CALLBACK(on_toolbutton_save_clicked) },
	{ "SaveAll", GEANY_STOCK_SAVE_ALL, N_("Save All"), NULL, N_("Save all open files"), G_CALLBACK(on_save_all1_activate) },
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
const guint ui_entries_n = G_N_ELEMENTS(ui_entries);


/* fallback UI definition */
const gchar *toolbar_markup =
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


static GtkWidget *toolbar_reload(void)
{
	gint i;
	GSList *l;
	GtkWidget *entry;
	GError *error = NULL;
	const gchar *filename;
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

	/* Load the toolbar UI XML file from disk (first from config dir, then try data dir) */
	filename = utils_build_path(app->configdir, "ui_toolbar.xml", NULL);
	merge_id = gtk_ui_manager_add_ui_from_file(uim, filename, &error);
	if (merge_id == 0)
	{
		if (error->code != G_FILE_ERROR_NOENT)
			geany_debug("Loading user toolbar UI definition failed (%s).", error->message);
		g_error_free(error);
		error = NULL;

		filename = utils_build_path(app->datadir, "ui_toolbar.xml", NULL);
		merge_id = gtk_ui_manager_add_ui_from_file(uim, filename, &error);
		if (merge_id == 0)
		{
			geany_debug(
				"UI creation failed, using internal fallback definition. Error message: %s",
				error->message);
			g_error_free(error);
			/* finally load the internally defined markup as fallback */
			merge_id = gtk_ui_manager_add_ui_from_string(uim, toolbar_markup, -1, NULL);
		}
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

		ui_document_buttons_update();
		ui_save_buttons_toggle(doc->changed); /* update save all */
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


static void document_save_cb(G_GNUC_UNUSED GObject *object, GeanyDocument *doc,
							 G_GNUC_UNUSED gpointer data)
{
	g_return_if_fail(NZV(doc->real_path));

	if (utils_str_equal(doc->real_path, utils_build_path(app->configdir, "ui_toolbar.xml", NULL)))
	{
		main_widgets.toolbar = toolbar_reload();
		ui_set_statusbar(FALSE, _("Toolbar reloaded."));
	}
}


void toolbar_add_config_file_menu_item(void)
{
	ui_add_config_file_menu_item(
		utils_build_path(app->configdir, "ui_toolbar.xml", NULL), NULL, NULL);
	g_signal_connect(geany_object, "document-save", G_CALLBACK(document_save_cb), NULL);
}


GtkWidget *toolbar_init(void)
{
	GtkWidget *toolbar;
	GtkAction *action_new;
	GtkAction *action_open;
	GtkAction *action_build;
	GtkAction *action_searchentry;
	GtkAction *action_gotoentry;

	uim = gtk_ui_manager_new();
	group = gtk_action_group_new("GeanyToolbar");

	gtk_action_group_set_translation_domain(group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions(group, ui_entries, ui_entries_n, NULL);

	/* Create our custom actions */
	action_new = geany_menu_button_action_new("New", NULL, _("Create a new file"), GTK_STOCK_NEW);
	g_signal_connect(action_new, "button-clicked", G_CALLBACK(on_toolbutton_new_clicked), NULL);
	gtk_action_group_add_action(group, action_new);

	action_open = geany_menu_button_action_new(
		"Open", NULL, _("Open an existing file"), GTK_STOCK_OPEN);
	g_signal_connect(action_open, "button-clicked", G_CALLBACK(on_toolbutton_open_clicked), NULL);
	gtk_action_group_add_action(group, action_open);

	action_build = geany_menu_button_action_new(
		"Build", NULL, _("Build the current file"), GEANY_STOCK_BUILD);
	g_signal_connect(action_build, "button-clicked",
		G_CALLBACK(build_toolbutton_build_clicked), NULL);
	gtk_action_group_add_action(group, action_build);

	action_searchentry = geany_entry_action_new(
		"SearchEntry", _("Search"), _("Find the entered text in the current file"), FALSE);
	g_signal_connect(action_searchentry, "entry-activate",
		G_CALLBACK(on_toolbar_search_entry_changed), GINT_TO_POINTER(FALSE));
	g_signal_connect(action_searchentry, "entry-changed",
		G_CALLBACK(on_toolbar_search_entry_changed), GINT_TO_POINTER(TRUE));
	gtk_action_group_add_action(group, action_searchentry);

	action_gotoentry = geany_entry_action_new(
		"GotoEntry", _("Goto"), _("Jump to the entered line number"), TRUE);
	g_signal_connect(action_gotoentry, "entry-activate",
		G_CALLBACK(on_toolbutton_goto_entry_activate), NULL);
	gtk_action_group_add_action(group, action_gotoentry);

	gtk_ui_manager_insert_action_group(uim, group, 0);

	toolbar = toolbar_reload();

	return toolbar;
}


void toolbar_update_ui(void)
{
	static GtkWidget *hbox_menubar = NULL;
	static GtkWidget *menubar = NULL;
	static GtkWidget *menubar_toolbar_separator = NULL;
	GtkWidget *parent;

	if (menubar == NULL)
	{	/* cache widget pointers */
		hbox_menubar = ui_lookup_widget(main_widgets.window, "hbox_menubar");
		menubar = ui_lookup_widget(main_widgets.window, "menubar1");

		menubar_toolbar_separator = GTK_WIDGET(gtk_separator_tool_item_new());
		gtk_toolbar_insert(GTK_TOOLBAR(main_widgets.toolbar),
			GTK_TOOL_ITEM(menubar_toolbar_separator), 0);
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
	/* the separator between the menubar and the toolbar */
	ui_widget_show_hide(menubar_toolbar_separator, toolbar_prefs.append_to_menu);
	/* we need to adjust the packing flags for the menubar to expand it if it is alone in the
	 * hbox and not expand it if the toolbar is appended */
	gtk_box_set_child_packing(GTK_BOX(hbox_menubar), menubar,
		! toolbar_prefs.append_to_menu, ! toolbar_prefs.append_to_menu, 0, GTK_PACK_START);
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
	g_object_unref(geany_menu_button_action_get_menu(
					GEANY_MENU_BUTTON_ACTION(toolbar_get_action_by_name("Open"))));

	/* unref'ing the GtkUIManager object will destroy all its widgets unless they were ref'ed */
	g_object_unref(uim);
	g_object_unref(group);

	g_slist_free(plugin_items);
}


void toolbar_apply_settings(void)
{
	/* sets the icon style of the toolbar */
	switch (toolbar_prefs.icon_style)
	{
		case GTK_TOOLBAR_BOTH:
		{
			/*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "images_and_text1")), TRUE);*/
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(ui_widgets.toolbar_menu, "images_and_text2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_ICONS:
		{
			/*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "images_only1")), TRUE);*/
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(ui_widgets.toolbar_menu, "images_only2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_TEXT:
		{
			/*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "text_only1")), TRUE);*/
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(ui_widgets.toolbar_menu, "text_only2")), TRUE);
			break;
		}
	}
	gtk_toolbar_set_style(GTK_TOOLBAR(main_widgets.toolbar), toolbar_prefs.icon_style);

	/* sets the icon size of the toolbar, use user preferences (.gtkrc) if not set */
	if (toolbar_prefs.icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR ||
		toolbar_prefs.icon_size == GTK_ICON_SIZE_LARGE_TOOLBAR ||
		toolbar_prefs.icon_size == GTK_ICON_SIZE_MENU)
	{
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(main_widgets.toolbar), toolbar_prefs.icon_size);
	}
}
