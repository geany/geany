/*
 *      toolbar.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
#include "geanyobject.h"
#include "geanymenubuttonaction.h"
#include "geanyentryaction.h"



GeanyToolbarPrefs toolbar_prefs;
static GtkUIManager *uim;
static GtkActionGroup *group;


/* Available toolbar actions
 * Fields: name, stock_id, label, accelerator, tooltip, callback */
const GtkActionEntry ui_entries[] = {
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


GtkWidget *toolbar_get_widget_child_by_name(const gchar *name)
{
	GtkWidget *widget = toolbar_get_widget_by_name(name);

	if (widget != NULL)
		return gtk_bin_get_child(GTK_BIN(widget));
	else
		return widget;
}


GtkAction *toolbar_get_action_by_name(const gchar *name)
{
	g_return_val_if_fail(name != NULL, NULL);

	return gtk_action_group_get_action(group, name);
}


static void on_document_save(G_GNUC_UNUSED GObject *object, GeanyDocument *doc,
							 G_GNUC_UNUSED gpointer data)
{
	g_return_if_fail(NZV(doc->real_path));

	if (utils_str_equal(doc->real_path, utils_build_path(app->configdir, "ui_toolbar.xml", NULL)))
	{
		dialogs_show_msgbox(GTK_MESSAGE_INFO,
		_("For all changes you make in this file to take effect, you need to restart Geany."));
	}
}


void toolbar_add_config_file_menu_item(void)
{
	ui_add_config_file_menu_item(
		utils_build_path(app->configdir, "ui_toolbar.xml", NULL), NULL, NULL);
	g_signal_connect(geany_object, "document-save", G_CALLBACK(on_document_save), NULL);
}


GtkWidget *toolbar_init(void)
{
	GtkWidget *toolbar;
	GtkBox *box;
	GtkAction *action_new;
	GtkAction *action_open;
	GtkAction *action_searchentry;
	GtkAction *action_gotoentry;
	GError *error = NULL;
	const gchar *filename;

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

	action_searchentry = geany_entry_action_new(
		"SearchEntry", _("Search"), _("Find the entered text in the current file"), FALSE);
	g_signal_connect(action_searchentry, "entry-activate",
		G_CALLBACK(on_toolbar_search_entry_changed), NULL);
	g_signal_connect(action_searchentry, "entry-changed",
		G_CALLBACK(on_toolbar_search_entry_changed), NULL);
	gtk_action_group_add_action(group, action_searchentry);

	action_gotoentry = geany_entry_action_new(
		"GotoEntry", _("Goto"), _("Jump to the entered line number."), TRUE);
	g_signal_connect(action_gotoentry, "entry-activate",
		G_CALLBACK(on_toolbutton_goto_entry_activate), NULL);
	gtk_action_group_add_action(group, action_gotoentry);

	gtk_ui_manager_insert_action_group(uim, group, 0);

	/* Load the toolbar UI XML file from disk (first from config dir, then try data dir) */
	filename = utils_build_path(app->configdir, "ui_toolbar.xml", NULL);
	if (! gtk_ui_manager_add_ui_from_file(uim, filename, &error))
	{
		if (error->code != G_FILE_ERROR_NOENT)
			geany_debug("Loading user toolbar UI definition failed (%s).", error->message);
		g_error_free(error);
		error = NULL;

		filename = utils_build_path(app->datadir, "ui_toolbar.xml", NULL);
		if (! gtk_ui_manager_add_ui_from_file(uim, filename, &error))
		{
			geany_debug(
				"UI creation failed, using internal fallback definition. Error message: %s",
				error->message);
			g_error_free(error);
			/* finally load the internally defined markup as fallback */
			gtk_ui_manager_add_ui_from_string(uim, toolbar_markup, -1, NULL);
		}
	}

	/* Add the toolbar widget to the main UI */
	toolbar = gtk_ui_manager_get_widget(uim, "/ui/GeanyToolbar");
	box = GTK_BOX(lookup_widget(main_widgets.window, "vbox1"));
	gtk_box_pack_start(box, toolbar, FALSE, FALSE, 0);
	gtk_box_reorder_child(box, toolbar, 1);

	/* Set some pointers */
	ui_widgets.new_file_menu = geany_menu_button_action_get_menu(
		GEANY_MENU_BUTTON_ACTION(action_new));
	ui_widgets.recent_files_menu_toolbar = geany_menu_button_action_get_menu(
		GEANY_MENU_BUTTON_ACTION(action_open));

	g_signal_connect(toolbar, "key-press-event", G_CALLBACK(on_escape_key_press_event), NULL);

	return toolbar;
}


/*  Returns the position for adding new toolbar items. The returned position can be used
 *  to add new toolbar items with @c gtk_toolbar_insert(). The toolbar object can be accessed
 *  with @a geany->main_widgets->toolbar.
 *  The position is always the last one before the Quit button (if it is shown).
 *
 *  @return The position for new toolbar items or @c -1 if an error occurred.
 */
gint toolbar_get_insert_position(void)
{
	GtkWidget *quit = toolbar_get_widget_by_name("Quit");
	gint pos = gtk_toolbar_get_item_index(GTK_TOOLBAR(main_widgets.toolbar), GTK_TOOL_ITEM(quit));

	return pos;
}


static void auto_separator_update(GeanyAutoSeparator *autosep)
{
	g_return_if_fail(autosep->ref_count >= 0);

	if (autosep->widget)
		ui_widget_show_hide(autosep->widget, autosep->ref_count > 0);
}


static void on_auto_separator_item_show_hide(GtkWidget *widget, gpointer user_data)
{
	GeanyAutoSeparator *autosep = user_data;

	if (GTK_WIDGET_VISIBLE(widget))
		autosep->ref_count++;
	else
		autosep->ref_count--;

	auto_separator_update(autosep);
}


static void on_auto_separator_item_destroy(GtkWidget *widget, gpointer user_data)
{
	GeanyAutoSeparator *autosep = user_data;

	/* GTK_WIDGET_VISIBLE won't work now the widget is being destroyed,
	 * so assume widget was visible */
	autosep->ref_count--;
	autosep->ref_count = MAX(autosep->ref_count, 0);
	auto_separator_update(autosep);
}


/* Show the separator widget if @a item or another is visible. */
/* Note: This would be neater taking a widget argument, setting a "visible-count"
 * property, and using reference counting to keep the widget alive whilst its visible group
 * is alive. */
void ui_auto_separator_add_ref(GeanyAutoSeparator *autosep, GtkWidget *item)
{
	/* set widget ptr NULL when widget destroyed */
	if (autosep->ref_count == 0)
		g_signal_connect(autosep->widget, "destroy",
			G_CALLBACK(gtk_widget_destroyed), &autosep->widget);

	if (GTK_WIDGET_VISIBLE(item))
	{
		autosep->ref_count++;
		auto_separator_update(autosep);
	}
	g_signal_connect(item, "show", G_CALLBACK(on_auto_separator_item_show_hide), autosep);
	g_signal_connect(item, "hide", G_CALLBACK(on_auto_separator_item_show_hide), autosep);
	g_signal_connect(item, "destroy", G_CALLBACK(on_auto_separator_item_destroy), autosep);
}


void toolbar_finalize(void)
{
    /* unref'ing the GtkUIManager object will destroy all its widgets unless they were ref'ed */
	g_object_unref(uim);
    g_object_unref(group);
}
