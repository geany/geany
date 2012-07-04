/*
 *      prefs.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * @file prefs.h
 * Preferences dialog.
 */

/*
 * Preferences dialog support functions.
 * New 'simple' prefs should use Stash code in keyfile.c - init_pref_groups().
 */

#include <stdlib.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "geany.h"

#include "prefs.h"
#include "support.h"
#include "dialogs.h"
#include "ui_utils.h"
#include "utils.h"
#include "sciwrappers.h"
#include "document.h"
#include "documentprivate.h"
#include "msgwindow.h"
#include "keyfile.h"
#include "keybindings.h"
#include "encodings.h"
#include "project.h"
#include "editor.h"
#include "main.h"
#include "sidebar.h"
#include "printing.h"
#include "geanywraplabel.h"
#include "templates.h"
#include "search.h"
#include "toolbar.h"
#include "tools.h"
#include "stash.h"
#include "keyfile.h"
#include "filetypes.h"
#include "win32.h"

#ifdef HAVE_VTE
# include "vte.h"
#endif


GeanyPrefs prefs;
GeanyToolPrefs tool_prefs;


/* keybinding globals, should be put in a struct */
static GtkTreeIter g_iter;
static GtkTreeStore *store = NULL;
static GtkTreeView *tree = NULL;
static GtkWidget *dialog_label;
static gboolean edited = FALSE;

static GtkTreeView *various_treeview = NULL;

static GeanyKeyBinding *kb_index(guint gidx, guint kid);
static void kb_cell_edited_cb(GtkCellRendererText *cellrenderertext, gchar *path, gchar *new_text, gpointer user_data);
static gboolean kb_grab_key_dialog_key_press_cb(GtkWidget *dialog, GdkEventKey *event, gpointer user_data);
static void kb_grab_key_dialog_response_cb(GtkWidget *dialog, gint response, gpointer user_data);
static gboolean kb_find_duplicate(GtkWidget *parent, GtkTreeIter *old_iter,
		guint key, GdkModifierType mods, const gchar *shortcut);
static void on_toolbar_show_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_show_notebook_tabs_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_enable_plugins_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_use_folding_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_open_encoding_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_sidebar_visible_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_prefs_print_radio_button_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_prefs_print_page_header_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void open_preferences_help(void);


typedef enum PrefCallbackAction
{
	PREF_DISPLAY,
	PREF_UPDATE
}
PrefCallbackAction;


/* Synchronize Stash settings with widgets (see keyfile.c - init_pref_groups()). */
static void prefs_action(PrefCallbackAction action)
{
	StashGroup *group;
	guint i;

	foreach_ptr_array(group, i, pref_groups)
	{
		switch (action)
		{
			case PREF_DISPLAY:
				stash_group_display(group, ui_widgets.prefs_dialog);
				break;
			case PREF_UPDATE:
				stash_group_update(group, ui_widgets.prefs_dialog);
				break;
		}
	}

	switch (action)
	{
		case PREF_DISPLAY:
			stash_tree_display(various_treeview);
			break;
		case PREF_UPDATE:
			stash_tree_update(various_treeview);
			break;
	}
}


enum
{
	KB_TREE_ACTION,
	KB_TREE_SHORTCUT,
	KB_TREE_INDEX,
	KB_TREE_EDITABLE,
	KB_TREE_WEIGHT,
	KB_TREE_COLUMNS
};


static void kb_tree_view_change_button_clicked_cb(GtkWidget *button, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gchar *name;

	selection = gtk_tree_view_get_selection(tree);
	if (gtk_tree_selection_get_selected(selection, &model, &g_iter))
	{
		if (gtk_tree_model_iter_has_child(model, &g_iter))
		{	/* double click on a section to expand or collapse it */
			GtkTreePath *path = gtk_tree_model_get_path(model, &g_iter);

			if (gtk_tree_view_row_expanded(tree, path))
				gtk_tree_view_collapse_row(tree, path);
			else
				gtk_tree_view_expand_row(tree, path, FALSE);

			gtk_tree_path_free(path);
			return;
		}

		gtk_tree_model_get(model, &g_iter, KB_TREE_ACTION, &name, -1);
		if (name != NULL)
		{
			GtkWidget *dialog;
			GtkWidget *label;
			gchar *str;

			dialog = gtk_dialog_new_with_buttons(_("Grab Key"), GTK_WINDOW(ui_widgets.prefs_dialog),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

			str = g_strdup_printf(
					_("Press the combination of the keys you want to use for \"%s\"."), name);
			label = gtk_label_new(str);
			gtk_misc_set_padding(GTK_MISC(label), 5, 10);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

			dialog_label = gtk_label_new("");
			gtk_misc_set_padding(GTK_MISC(dialog_label), 5, 10);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), dialog_label);

			g_signal_connect(dialog, "key-press-event",
								G_CALLBACK(kb_grab_key_dialog_key_press_cb), NULL);
			g_signal_connect(dialog, "response", G_CALLBACK(kb_grab_key_dialog_response_cb), NULL);

			gtk_widget_show_all(dialog);
			g_free(str);
			g_free(name);
		}
	}
}


static void kb_expand_collapse_cb(GtkWidget *item, gpointer user_data)
{
	if (user_data != NULL)
		gtk_tree_view_expand_all(tree);
	else
		gtk_tree_view_collapse_all(tree);
}


static void kb_show_popup_menu(GtkWidget *widget, GdkEventButton *event)
{
	GtkWidget *item;
	static GtkWidget *menu = NULL;
	guint button;
	guint32 event_time;

	if (menu == NULL)
	{
		menu = gtk_menu_new();

		item = ui_image_menu_item_new(GTK_STOCK_ADD, _("_Expand All"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(kb_expand_collapse_cb), GINT_TO_POINTER(TRUE));

		item = ui_image_menu_item_new(GTK_STOCK_REMOVE, _("_Collapse All"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(kb_expand_collapse_cb), NULL);

		gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);
	}

	if (event != NULL)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time();
	}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button, event_time);
}


static gboolean kb_popup_menu_cb(GtkWidget *widget, gpointer data)
{
	kb_show_popup_menu(widget, NULL);
	return TRUE;
}


static gboolean kb_tree_view_button_press_event_cb(GtkWidget *widget, GdkEventButton *event,
												   gpointer user_data)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
	{
		kb_show_popup_menu(widget, event);
		return TRUE;
	}
	else if (event->type == GDK_2BUTTON_PRESS)
	{
		kb_tree_view_change_button_clicked_cb(NULL, NULL);
		return TRUE;
	}
	return FALSE;
}


static void kb_init_tree(void)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	tree = GTK_TREE_VIEW(ui_lookup_widget(ui_widgets.prefs_dialog, "treeview7"));

	store = gtk_tree_store_new(KB_TREE_COLUMNS,
		G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_object_unref(store);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Action"), renderer,
		"text", KB_TREE_ACTION, "weight", KB_TREE_WEIGHT, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Shortcut"), renderer,
		"text", KB_TREE_SHORTCUT, "editable", KB_TREE_EDITABLE,
		"weight", KB_TREE_WEIGHT, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	/* set policy settings for the scrolled window around the treeview again, because glade
	 * doesn't keep the settings */
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(ui_lookup_widget(ui_widgets.prefs_dialog, "scrolledwindow8")),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	g_signal_connect(renderer, "edited", G_CALLBACK(kb_cell_edited_cb), NULL);
	g_signal_connect(tree, "button-press-event", G_CALLBACK(kb_tree_view_button_press_event_cb), NULL);
	g_signal_connect(tree, "popup-menu", G_CALLBACK(kb_popup_menu_cb), NULL);
	g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "button2"), "clicked",
				G_CALLBACK(kb_tree_view_change_button_clicked_cb), NULL);
}


static void kb_set_shortcut(GtkTreeStore *store, GtkTreeIter *iter,
		guint key, GdkModifierType mods)
{
	gchar *key_string = gtk_accelerator_name(key, mods);
	GtkTreeIter parent;
	guint kid, gid;
	GeanyKeyBinding *kb;
	gboolean bold;

	gtk_tree_store_set(store, iter, KB_TREE_SHORTCUT, key_string, -1);
	g_free(key_string);

	gtk_tree_model_get(GTK_TREE_MODEL(store), iter, KB_TREE_INDEX, &kid, -1);
	gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &parent, iter);
	gtk_tree_model_get(GTK_TREE_MODEL(store), &parent, KB_TREE_INDEX, &gid, -1);
	kb = kb_index(gid, kid);
	bold = key != kb->default_key || mods != kb->default_mods;
	gtk_tree_store_set(store, iter, KB_TREE_WEIGHT,
		bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL, -1);
}


static void kb_init(void)
{
	GtkTreeIter parent, iter;
	gsize g, i;
	gchar *label;
	GeanyKeyGroup *group;
	GeanyKeyBinding *kb;

	if (store == NULL)
		kb_init_tree();

	foreach_ptr_array(group, g, keybinding_groups)
	{
		gtk_tree_store_append(store, &parent, NULL);
		gtk_tree_store_set(store, &parent, KB_TREE_ACTION, group->label,
			KB_TREE_INDEX, g, -1);

		foreach_ptr_array(kb, i, group->key_items)
		{
			label = keybindings_get_label(kb);
			gtk_tree_store_append(store, &iter, &parent);
			gtk_tree_store_set(store, &iter, KB_TREE_ACTION, label,
				KB_TREE_EDITABLE, TRUE, KB_TREE_INDEX, kb->id, -1);
			kb_set_shortcut(store, &iter, kb->key, kb->mods);
			g_free(label);
		}
	}
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
}


/* note: new 'simple' prefs should use Stash code in keyfile.c */
static void prefs_init_dialog(void)
{
	GtkWidget *widget;
	GdkColor *color;

	/* Synchronize with Stash settings */
	prefs_action(PREF_DISPLAY);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "label_project_indent_warning");
	ui_widget_show_hide(widget, app->project != NULL);

	/* General settings */
	/* startup */
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_load_session");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.load_session);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_project_session");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), project_prefs.project_session);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_project_file_in_basedir");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), project_prefs.project_file_in_basedir);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_save_win_pos");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.save_winpos);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_ask_for_quit");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.confirm_exit);

	/* behaviour */
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_beep");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.beep_on_errors);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_switch_pages");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.switch_to_status);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_suppress_status_msgs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.suppress_status_messages);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_auto_focus");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.auto_focus);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_native_windows_dialogs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		interface_prefs.use_native_windows_dialogs);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_contextaction");
	gtk_entry_set_text(GTK_ENTRY(widget), tool_prefs.context_action_cmd);

	project_setup_prefs();	/* project files path */


	/* Interface settings */
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_sidebar_visible");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), ui_prefs.sidebar_visible);
	on_sidebar_visible_toggled(GTK_TOGGLE_BUTTON(widget), NULL);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_list_symbol");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), interface_prefs.sidebar_symbol_visible);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), interface_prefs.sidebar_openfiles_visible);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "tagbar_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), interface_prefs.tagbar_font);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "msgwin_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), interface_prefs.msgwin_font);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "editor_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), interface_prefs.editor_font);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_long_line");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.long_line_column);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_long_line");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.long_line_enabled);

	switch (editor_prefs.long_line_type)
	{
		case 0: widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_line"); break;
		case 1: widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_background"); break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	color = g_new0(GdkColor, 1);
	gdk_color_parse(editor_prefs.long_line_color, color);
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "long_line_color");
	gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), color);
	g_free(color);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), interface_prefs.show_notebook_tabs);
	/* disable following setting if notebook tabs are hidden */
	on_show_notebook_tabs_toggled(GTK_TOGGLE_BUTTON(
					ui_lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs")), NULL);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_show_tab_cross");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), file_prefs.show_tab_cross);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_tab_editor");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), interface_prefs.tab_pos_editor);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_tab_msgwin");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), interface_prefs.tab_pos_msgwin);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_tab_sidebar");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), interface_prefs.tab_pos_sidebar);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_statusbar_visible");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), interface_prefs.statusbar_visible);


	/* Toolbar settings */
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), toolbar_prefs.visible);
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_in_menu");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), toolbar_prefs.append_to_menu);

	switch (toolbar_prefs.icon_style)
	{
		case 0: widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_image"); break;
		case 1: widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_text"); break;
		default: widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_imagetext"); break;
	}
	if (toolbar_prefs.use_gtk_default_style)
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_style_default");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	switch (toolbar_prefs.icon_size)
	{
		case GTK_ICON_SIZE_LARGE_TOOLBAR:
				widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_large"); break;
		case GTK_ICON_SIZE_SMALL_TOOLBAR:
				widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_small"); break;
		default: widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_verysmall"); break;
	}
	if (toolbar_prefs.use_gtk_default_icon)
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_icon_default");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	/* disable elements if toolbar is hidden */
	on_toolbar_show_toggled(GTK_TOGGLE_BUTTON(
					ui_lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show")), NULL);


	/* Files settings */
	if (file_prefs.tab_order_ltr)
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_tab_right");
	else
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_tab_left");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_tab_beside");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), file_prefs.tab_order_beside);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_new_encoding");
	/* luckily the index of the combo box items match the index of the encodings array */
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), file_prefs.default_new_encoding);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_open_encoding");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			(file_prefs.default_open_encoding >= 0) ? TRUE : FALSE);
	on_open_encoding_toggled(GTK_TOGGLE_BUTTON(widget), NULL);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_open_encoding");
	if (file_prefs.default_open_encoding >= 0)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), file_prefs.default_open_encoding);
	}
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), GEANY_ENCODING_UTF_8);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_eol");
	if (file_prefs.default_eol_character >= 0 && file_prefs.default_eol_character < 3)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), file_prefs.default_eol_character);
	}
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), GEANY_DEFAULT_EOL_CHARACTER);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_trailing_spaces");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), file_prefs.strip_trailing_spaces);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_new_line");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), file_prefs.final_new_line);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_ensure_convert_new_lines");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), file_prefs.ensure_convert_new_lines);

	/* Editor settings */
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_toggle_mark");
	gtk_entry_set_text(GTK_ENTRY(widget), editor_prefs.comment_toggle_mark);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_replace_tabs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), file_prefs.replace_tabs);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_indent");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_indent_guide);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_white_space");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_white_space);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_line_end");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_line_endings);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_line_numbers");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_linenumber_margin);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_markers_margin");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_markers_margin);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_scroll_stop_at_last_line");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.scroll_stop_at_last_line);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_line_wrapping");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.line_wrapping);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_complete_snippets");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.complete_snippets);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_xmltag");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.auto_close_xml_tags);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_folding");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.folding);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_unfold_children");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.unfold_all_children);
	on_use_folding_toggled(GTK_TOGGLE_BUTTON(
					ui_lookup_widget(ui_widgets.prefs_dialog, "check_folding")), NULL);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_disable_dnd");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.disable_dnd);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_smart_home");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.smart_home_key);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_newline_strip");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.newline_strip);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_indicators");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.use_indicators);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_auto_multiline");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.auto_continue_multiline);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_symbol_auto_completion");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.auto_complete_symbols);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_symbollistheight");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.symbolcompletion_max_height);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_symbol_complete_chars");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.symbolcompletion_min_chars);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_line_break");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.line_break_column);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_parenthesis");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		(editor_prefs.autoclose_chars & GEANY_AC_PARENTHESIS));

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_cbracket");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		(editor_prefs.autoclose_chars & GEANY_AC_CBRACKET));

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_sbracket");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		(editor_prefs.autoclose_chars & GEANY_AC_SBRACKET));

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_squote");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		(editor_prefs.autoclose_chars & GEANY_AC_SQUOTE));

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_dquote");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
		(editor_prefs.autoclose_chars & GEANY_AC_DQUOTE));

	/* Tools Settings */

	if (tool_prefs.term_cmd)
		gtk_entry_set_text(GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_com_term")), tool_prefs.term_cmd);

	if (tool_prefs.browser_cmd)
		gtk_entry_set_text(GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_browser")), tool_prefs.browser_cmd);

	if (tool_prefs.grep_cmd)
		gtk_entry_set_text(GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_grep")), tool_prefs.grep_cmd);


	/* Template settings */
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_developer");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.developer);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_company");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.company);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_mail");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.mail);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_initial");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.initials);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_version");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.version);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_year");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.year_format);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_date");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.date_format);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_datetime");
	gtk_entry_set_text(GTK_ENTRY(widget), template_prefs.datetime_format);


	/* Keybindings */
	kb_init();

	/* Printing */
	{
		GtkWidget *widget_gtk = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_print_gtk");
		if (printing_prefs.use_gtk_printing)
			widget = widget_gtk;
		else
			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_print_external");

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

		on_prefs_print_radio_button_toggled(GTK_TOGGLE_BUTTON(widget_gtk), NULL);
	}
	if (printing_prefs.external_print_cmd)
		gtk_entry_set_text(
			GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_print_external_cmd")),
			printing_prefs.external_print_cmd);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_linenumbers");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.print_line_numbers);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_pagenumbers");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.print_page_numbers);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_pageheader");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.print_page_header);
	on_prefs_print_page_header_toggled(GTK_TOGGLE_BUTTON(widget), NULL);

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_basename");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.page_header_basename);

	if (printing_prefs.page_header_datefmt)
		gtk_entry_set_text(
			GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_print_dateformat")),
			printing_prefs.page_header_datefmt);


#ifndef HAVE_PLUGINS
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "check_plugins"), FALSE);
#endif
	on_enable_plugins_toggled(GTK_TOGGLE_BUTTON( ui_lookup_widget(ui_widgets.prefs_dialog, "check_plugins")), NULL);

#ifdef HAVE_VTE
	/* make VTE switch visible only when VTE is compiled in, it is hidden by default */
	widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_vte");
	gtk_widget_show(widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vte_info.load_vte);

	/* VTE settings */
	if (vte_info.have_vte)
	{
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "font_term");
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), vc->font);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "color_fore");
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), vc->colour_fore);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "color_back");
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), vc->colour_back);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_scrollback");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), vc->scrollback_lines);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_shell");
		gtk_entry_set_text(GTK_ENTRY(widget), vc->shell);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_scroll_key");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->scroll_on_key);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_scroll_out");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->scroll_on_out);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_enable_bash_keys");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->enable_bash_keys);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_ignore_menu_key");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->ignore_menu_bar_accel);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_follow_path");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->follow_path);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_run_in_vte");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->run_in_vte);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_skip_script");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->skip_run_script);

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_cursor_blinks");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->cursor_blinks);
	}
#endif
}


/* note: uses group index, not group id, unlike keybindings_lookup_item(). */
static GeanyKeyBinding *kb_index(guint gidx, guint kid)
{
	GeanyKeyGroup *group;

	group = g_ptr_array_index(keybinding_groups, gidx);
	return keybindings_get_item(group, kid);
}


/* read the treeview shortcut fields into keybindings */
static void kb_update(void)
{
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	GtkTreeIter child, parent;
	guint gid = 0;

	/* get first parent */
	if (! gtk_tree_model_iter_children(model, &parent, NULL))
		return;

	/* foreach parent */
	while (TRUE)
	{
		/* get first child */
		if (! gtk_tree_model_iter_children(model, &child, &parent))
			return;

		/* foreach child */
		while (TRUE)
		{
			guint kid;
			gchar *str;
			guint key;
			GdkModifierType mods;
			GeanyKeyBinding *kb;

			gtk_tree_model_get(model, &child, KB_TREE_INDEX, &kid, KB_TREE_SHORTCUT, &str, -1);
			gtk_accelerator_parse(str, &key, &mods);
			g_free(str);
			kb = kb_index(gid, kid);
			if (kb->key != key || kb->mods != mods)
				keybindings_update_combo(kb, key, mods);

			if (! gtk_tree_model_iter_next(model, &child))
				break;
		}
		if (! gtk_tree_model_iter_next(model, &parent))
			return;
		gid++;
	}
}


/*
 * callbacks
 */
/* note: new 'simple' prefs should use Stash code in keyfile.c */
static void
on_prefs_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GtkWidget *widget;
		guint i;
		gboolean autoclose_brackets[5];
		gboolean old_invert_all = interface_prefs.highlighting_invert_all;
		gboolean old_sidebar_pos = interface_prefs.sidebar_pos;
		GeanyDocument *doc = document_get_current();

		/* Synchronize Stash settings */
		prefs_action(PREF_UPDATE);

		if (interface_prefs.highlighting_invert_all != old_invert_all)
			filetypes_reload();

		if (interface_prefs.sidebar_pos != old_sidebar_pos)
			ui_swap_sidebar_pos();

		/* General settings */
		/* startup */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_load_session");
		prefs.load_session = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_project_session");
		project_prefs.project_session = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_project_file_in_basedir");
		project_prefs.project_file_in_basedir = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_save_win_pos");
		prefs.save_winpos = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_ask_for_quit");
		prefs.confirm_exit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		/* behaviour */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_beep");
		prefs.beep_on_errors = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_switch_pages");
		prefs.switch_to_status = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_suppress_status_msgs");
		prefs.suppress_status_messages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_auto_focus");
		prefs.auto_focus = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_native_windows_dialogs");
		interface_prefs.use_native_windows_dialogs =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_contextaction");
		g_free(tool_prefs.context_action_cmd);
		tool_prefs.context_action_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		project_apply_prefs();	/* project file path */


		/* Interface settings */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_sidebar_visible");
		ui_prefs.sidebar_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_list_symbol");
		interface_prefs.sidebar_symbol_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles");
		interface_prefs.sidebar_openfiles_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_long_line");
		editor_prefs.long_line_enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_line");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			editor_prefs.long_line_type = 0;
		else
			/* now only the "background" radio remains */
			editor_prefs.long_line_type = 1;

		if (editor_prefs.long_line_column == 0)
			editor_prefs.long_line_enabled = FALSE;

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs");
		interface_prefs.show_notebook_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_show_tab_cross");
		file_prefs.show_tab_cross = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_tab_editor");
		interface_prefs.tab_pos_editor = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_tab_msgwin");
		interface_prefs.tab_pos_msgwin = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_tab_sidebar");
		interface_prefs.tab_pos_sidebar = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_statusbar_visible");
		interface_prefs.statusbar_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));


		/* Toolbar settings */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show");
		toolbar_prefs.visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_in_menu");
		toolbar_prefs.append_to_menu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_style_default");
		toolbar_prefs.use_gtk_default_style = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		if (! toolbar_prefs.use_gtk_default_style)
		{
			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_imagetext");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
				toolbar_prefs.icon_style = 2;
			else
			{
				widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_image");
				if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
					toolbar_prefs.icon_style = 0;
				else
					/* now only the text only radio remains, so set text only */
					toolbar_prefs.icon_style = 1;
			}
		}

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_icon_default");
		toolbar_prefs.use_gtk_default_icon = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		if (! toolbar_prefs.use_gtk_default_icon)
		{	toolbar_prefs.icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_large");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
				toolbar_prefs.icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
			else
			{
				widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_small");
				if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
					toolbar_prefs.icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;
				else
					toolbar_prefs.icon_size = GTK_ICON_SIZE_MENU;
			}
		}

		/* Files settings */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_tab_right");
		file_prefs.tab_order_ltr = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_tab_beside");
		file_prefs.tab_order_beside = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_new_encoding");
		file_prefs.default_new_encoding = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_open_encoding");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_open_encoding");
			file_prefs.default_open_encoding = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
		}
		else
			file_prefs.default_open_encoding = -1;

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "combo_eol");
		file_prefs.default_eol_character = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_trailing_spaces");
		file_prefs.strip_trailing_spaces = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_new_line");
		file_prefs.final_new_line = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_ensure_convert_new_lines");
		file_prefs.ensure_convert_new_lines = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_replace_tabs");
		file_prefs.replace_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));


		/* Editor settings */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_toggle_mark");
		SETPTR(editor_prefs.comment_toggle_mark,
			gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_long_line");
		/* note: use stash for new code - it updates spin buttons itself */
		gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
		editor_prefs.long_line_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_folding");
		editor_prefs.folding = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		ui_update_fold_items();

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_unfold_children");
		editor_prefs.unfold_all_children = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_indent");
		editor_prefs.show_indent_guide = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_white_space");
		editor_prefs.show_white_space = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_line_end");
		editor_prefs.show_line_endings = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_line_numbers");
		editor_prefs.show_linenumber_margin = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_markers_margin");
		editor_prefs.show_markers_margin = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_scroll_stop_at_last_line");
		editor_prefs.scroll_stop_at_last_line = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_line_wrapping");
		editor_prefs.line_wrapping = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_complete_snippets");
		editor_prefs.complete_snippets = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_xmltag");
		editor_prefs.auto_close_xml_tags = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_indicators");
		editor_prefs.use_indicators = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_disable_dnd");
		editor_prefs.disable_dnd = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_smart_home");
		editor_prefs.smart_home_key = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_newline_strip");
		editor_prefs.newline_strip = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_auto_multiline");
		editor_prefs.auto_continue_multiline = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_symbol_auto_completion");
		editor_prefs.auto_complete_symbols = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_symbol_complete_chars");
		gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
		editor_prefs.symbolcompletion_min_chars = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_symbollistheight");
		gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
		editor_prefs.symbolcompletion_max_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_line_break");
		gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
		editor_prefs.line_break_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_parenthesis");
		autoclose_brackets[0] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_cbracket");
		autoclose_brackets[1] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_sbracket");
		autoclose_brackets[2] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_squote");
		autoclose_brackets[3] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_autoclose_dquote");
		autoclose_brackets[4] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		editor_prefs.autoclose_chars =
		  (autoclose_brackets[0] ? GEANY_AC_PARENTHESIS : 0u)
		| (autoclose_brackets[1] ? GEANY_AC_CBRACKET : 0u)
		| (autoclose_brackets[2] ? GEANY_AC_SBRACKET : 0u)
		| (autoclose_brackets[3] ? GEANY_AC_SQUOTE : 0u)
		| (autoclose_brackets[4] ? GEANY_AC_DQUOTE : 0u);

		/* Tools Settings */

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_com_term");
		g_free(tool_prefs.term_cmd);
		tool_prefs.term_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_browser");
		g_free(tool_prefs.browser_cmd);
		tool_prefs.browser_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_grep");
		g_free(tool_prefs.grep_cmd);
		tool_prefs.grep_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


		/* Template settings */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_developer");
		g_free(template_prefs.developer);
		template_prefs.developer = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_company");
		g_free(template_prefs.company);
		template_prefs.company = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_mail");
		g_free(template_prefs.mail);
		template_prefs.mail = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_initial");
		g_free(template_prefs.initials);
		template_prefs.initials = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_version");
		g_free(template_prefs.version);
		template_prefs.version = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_year");
		g_free(template_prefs.year_format);
		template_prefs.year_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_date");
		g_free(template_prefs.date_format);
		template_prefs.date_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_template_datetime");
		g_free(template_prefs.datetime_format);
		template_prefs.datetime_format = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


		/* Keybindings */
		if (edited)
		{
			kb_update();
			tools_create_insert_custom_command_menu_items();
			keybindings_write_to_file();
		}

		/* Printing */
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "radio_print_gtk");
		printing_prefs.use_gtk_printing = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_print_external_cmd");
		g_free(printing_prefs.external_print_cmd);
		printing_prefs.external_print_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_linenumbers");
		printing_prefs.print_line_numbers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_pagenumbers");
		printing_prefs.print_page_numbers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_pageheader");
		printing_prefs.print_page_header = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_basename");
		printing_prefs.page_header_basename = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_print_dateformat");
		g_free(printing_prefs.page_header_datefmt);
		printing_prefs.page_header_datefmt = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


#ifdef HAVE_VTE
		widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_vte");
		vte_info.load_vte = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		/* VTE settings */
		if (vte_info.have_vte)
		{
			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "spin_scrollback");
			gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
			vc->scrollback_lines = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "entry_shell");
			g_free(vc->shell);
			vc->shell = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_scroll_key");
			vc->scroll_on_key = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_scroll_out");
			vc->scroll_on_out = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_enable_bash_keys");
			vc->enable_bash_keys = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_ignore_menu_key");
			vc->ignore_menu_bar_accel = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_follow_path");
			vc->follow_path = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_run_in_vte");
			vc->run_in_vte = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_skip_script");
			vc->skip_run_script = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = ui_lookup_widget(ui_widgets.prefs_dialog, "check_cursor_blinks");
			vc->cursor_blinks = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			vte_apply_user_settings();
		}
#endif

		/* apply the changes made */
		ui_statusbar_showhide(interface_prefs.statusbar_visible);
		sidebar_openfiles_update_all(); /* to update if full path setting has changed */
		toolbar_apply_settings();
		toolbar_update_ui();
		toolbar_show_hide();
		ui_sidebar_show_hide();
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(main_widgets.notebook), interface_prefs.show_notebook_tabs);

		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(main_widgets.notebook), interface_prefs.tab_pos_editor);
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(msgwindow.notebook), interface_prefs.tab_pos_msgwin);
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(main_widgets.sidebar_notebook), interface_prefs.tab_pos_sidebar);

		/* re-colourise all open documents, if tab width or long line settings have changed */
		for (i = 0; i < documents_array->len; i++)
		{
			if (documents[i]->is_valid)
			{
				editor_apply_update_prefs(documents[i]->editor);
				if (! editor_prefs.folding)
					editor_unfold_all(documents[i]->editor);
			}
		}
		ui_document_show_hide(NULL);
		ui_update_view_editor_menu_items();

		/* various preferences */
		ui_save_buttons_toggle((doc != NULL) ? doc->changed : FALSE);
		msgwin_show_hide_tabs();
		ui_update_statusbar(doc, -1);

		/* store all settings */
		configuration_save();
	}

	if (response == GTK_RESPONSE_HELP)
	{
		open_preferences_help();
	}
	else if (response != GTK_RESPONSE_APPLY)
	{
		gtk_tree_store_clear(store);
		gtk_widget_hide(GTK_WIDGET(dialog));
	}
}


static void on_color_button_choose_cb(GtkColorButton *widget, gpointer user_data)
{
	GdkColor color;

	gtk_color_button_get_color(widget, &color);
	SETPTR(editor_prefs.long_line_color, utils_get_hex_from_color(&color));
}


static void on_prefs_font_choosed(GtkFontButton *widget, gpointer user_data)
{
	const gchar *fontbtn = gtk_font_button_get_font_name(widget);
	guint i;

	switch (GPOINTER_TO_INT(user_data))
	{
		case 1:
		{
			if (strcmp(fontbtn, interface_prefs.tagbar_font) == 0)
				break;

			SETPTR(interface_prefs.tagbar_font, g_strdup(fontbtn));
			for (i = 0; i < documents_array->len; i++)
			{
				GeanyDocument *doc = documents[i];

				if (documents[i]->is_valid && GTK_IS_WIDGET(doc->priv->tag_tree))
					ui_widget_modify_font_from_string(doc->priv->tag_tree,
						interface_prefs.tagbar_font);
			}
			if (GTK_IS_WIDGET(tv.default_tag_tree))
				ui_widget_modify_font_from_string(tv.default_tag_tree, interface_prefs.tagbar_font);
			ui_widget_modify_font_from_string(tv.tree_openfiles, interface_prefs.tagbar_font);
			break;
		}
		case 2:
		{
			if (strcmp(fontbtn, interface_prefs.msgwin_font) == 0)
				break;
			SETPTR(interface_prefs.msgwin_font, g_strdup(fontbtn));
			ui_widget_modify_font_from_string(msgwindow.tree_compiler, interface_prefs.msgwin_font);
			ui_widget_modify_font_from_string(msgwindow.tree_msg, interface_prefs.msgwin_font);
			ui_widget_modify_font_from_string(msgwindow.tree_status, interface_prefs.msgwin_font);
			ui_widget_modify_font_from_string(msgwindow.scribble, interface_prefs.msgwin_font);
			break;
		}
		case 3:
		{
			ui_set_editor_font(fontbtn);
			break;
		}
	}
}


static void kb_change_iter_shortcut(GtkTreeIter *iter, const gchar *new_text)
{
	guint lkey;
	GdkModifierType lmods;

	gtk_accelerator_parse(new_text, &lkey, &lmods);

	if (kb_find_duplicate(ui_widgets.prefs_dialog, iter, lkey, lmods, new_text))
		return;

	/* set the values here, because of the above check, setting it in
	 * gtk_accelerator_parse would return a wrong key combination if it is duplicate */
	kb_set_shortcut(store, iter, lkey, lmods);

	edited = TRUE;
}


static void kb_cell_edited_cb(GtkCellRendererText *cellrenderertext,
		gchar *path, gchar *new_text, gpointer user_data)
{
	if (path != NULL && new_text != NULL)
	{
		GtkTreeIter iter;

		gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store), &iter, path);
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter))
			return;	/* ignore group items */

		kb_change_iter_shortcut(&iter, new_text);
	}
}


static gboolean kb_grab_key_dialog_key_press_cb(GtkWidget *dialog, GdkEventKey *event, gpointer user_data)
{
	gchar *str;
	guint state;

	state = event->state & gtk_accelerator_get_default_mod_mask();

	if (event->keyval == GDK_Escape)
		return FALSE;	/* close the dialog, don't allow escape when detecting keybindings. */

	str = gtk_accelerator_name(event->keyval, state);

	gtk_label_set_text(GTK_LABEL(dialog_label), str);
	g_free(str);

	return TRUE;
}


static void kb_grab_key_dialog_response_cb(GtkWidget *dialog, gint response, G_GNUC_UNUSED gpointer iter)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		const gchar *new_text = gtk_label_get_text(GTK_LABEL(dialog_label));

		kb_change_iter_shortcut(&g_iter, new_text);
	}
	gtk_widget_destroy(dialog);
}


/* test if the entered key combination is already used
 * returns true if cancelling duplicate */
static gboolean kb_find_duplicate(GtkWidget *parent, GtkTreeIter *old_iter,
		guint key, GdkModifierType mods, const gchar *shortcut)
{
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	GtkTreeIter parent_iter;
	gchar *kb_str;
	guint kb_key;
	GdkModifierType kb_mods;

	/* allow duplicate if there is no key combination */
	if (key == 0 && mods == 0)
		return FALSE;

	/* don't check if the new keybinding is the same as the old one */
	gtk_tree_model_get(model, old_iter, KB_TREE_SHORTCUT, &kb_str, -1);
	if (kb_str)
	{
		gtk_accelerator_parse(kb_str, &kb_key, &kb_mods);
		g_free(kb_str);
		if (key == kb_key && mods == kb_mods)
			return FALSE;
	}

	if (! gtk_tree_model_get_iter_first(model, &parent_iter))
		return FALSE;
	do	/* foreach top level */
	{
		GtkTreeIter iter;

		if (! gtk_tree_model_iter_children(model, &iter, &parent_iter))
			continue;
		do	/* foreach children */
		{
			gtk_tree_model_get(model, &iter, KB_TREE_SHORTCUT, &kb_str, -1);
			if (! kb_str)
				continue;

			gtk_accelerator_parse(kb_str, &kb_key, &kb_mods);
			g_free(kb_str);
			/* search another item with the same key and modifiers */
			if (kb_key == key && kb_mods == mods)
			{
				gchar *label;
				gint ret;

				gtk_tree_model_get(model, &iter, KB_TREE_ACTION, &label, -1);
				ret = dialogs_show_prompt(parent,
					_("_Allow"), GTK_RESPONSE_APPLY,
					GTK_STOCK_CANCEL, GTK_RESPONSE_NO,
					_("_Override"), GTK_RESPONSE_YES,
					_("Override that keybinding?"),
					_("The combination '%s' is already used for \"%s\"."),
					shortcut, label);

				g_free(label);

				if (ret == GTK_RESPONSE_YES)
				{
					kb_set_shortcut(store, &iter, 0, 0);	/* clear shortcut */
					/* carry on looking for other duplicates if overriding */
					continue;
				}
				return ret == GTK_RESPONSE_NO;
			}
		}
		while (gtk_tree_model_iter_next(model, &iter));
	}
	while (gtk_tree_model_iter_next(model, &parent_iter));

	return FALSE;
}


static void on_toolbar_show_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(
		ui_lookup_widget(ui_widgets.prefs_dialog, "frame_toolbar_style"), sens);
	gtk_widget_set_sensitive(
		ui_lookup_widget(ui_widgets.prefs_dialog, "frame_toolbar_icon"), sens);
	gtk_widget_set_sensitive(
		ui_lookup_widget(ui_widgets.prefs_dialog, "button_customize_toolbar"), sens);
	gtk_widget_set_sensitive(
		ui_lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_in_menu"), sens);
}


static void on_show_notebook_tabs_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	/* tab placement only enabled when tabs are visible */
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "combo_tab_editor"), sens);
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "check_show_tab_cross"), sens);
}


static void on_use_folding_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "check_unfold_children"), sens);
}


static void on_enable_plugins_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "extra_plugin_path_entry"), sens);
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "extra_plugin_path_button"), sens);
}


static void on_open_encoding_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "eventbox3"), sens);
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "label_open_encoding"), sens);
}


static void on_sidebar_visible_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles"), sens);
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "check_list_symbol"), sens);
}


static void on_prefs_print_radio_button_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "vbox29"), sens);
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "hbox9"), ! sens);
}


static void on_prefs_print_page_header_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_basename"), sens);
	gtk_widget_set_sensitive(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_print_dateformat"), sens);
}


static void open_preferences_help(void)
{
	gchar *uri;
	const gchar *label, *suffix = NULL;
	GtkNotebook *notebook = GTK_NOTEBOOK(
		ui_lookup_widget(ui_widgets.prefs_dialog, "notebook2"));
	gint page_nr = gtk_notebook_get_current_page(notebook);
	GtkWidget *page = gtk_notebook_get_nth_page(notebook, page_nr);

	label = gtk_notebook_get_tab_label_text(notebook, page);

	/* TODO Find a better way to map the current notebook page to the
	 * corresponding chapter in the documentation, comparing translatable
	 * strings is easy to break. Maybe attach an identifying string to the
	 * tab label object. */
	if (utils_str_equal(label, _("General")))
		suffix = "#general-startup-preferences";
	else if (utils_str_equal(label, _("Interface")))
		suffix = "#interface-preferences";
	else if (utils_str_equal(label, _("Toolbar")))
		suffix = "#toolbar-preferences";
	else if (utils_str_equal(label, _("Editor")))
		suffix = "#editor-features-preferences";
	else if (utils_str_equal(label, _("Files")))
		suffix = "#files-preferences";
	else if (utils_str_equal(label, _("Tools")))
		suffix = "#tools-preferences";
	else if (utils_str_equal(label, _("Templates")))
		suffix = "#template-preferences";
	else if (utils_str_equal(label, _("Keybindings")))
		suffix = "#keybinding-preferences";
	else if (utils_str_equal(label, _("Printing")))
		suffix = "#printing-preferences";
	else if (utils_str_equal(label, _("Various")))
		suffix = "#various-preferences";
	else if (utils_str_equal(label, _("Terminal")))
		suffix = "#terminal-vte-preferences";

	uri = utils_get_help_url(suffix);
	utils_open_browser(uri);
	g_free(uri);
}


static gboolean prefs_dialog_key_press_response_cb(GtkWidget *dialog, GdkEventKey *event,
												   gpointer data)
{
	GeanyKeyBinding *kb = keybindings_lookup_item(GEANY_KEY_GROUP_HELP, GEANY_KEYS_HELP_HELP);

	if (keybindings_check_event(event, kb))
	{
		open_preferences_help();
		return TRUE;
	}
	return FALSE;
}


static void list_store_append_text(GtkListStore *list, const gchar *text)
{
	GtkTreeIter iter;

	gtk_list_store_append(list, &iter);
	gtk_list_store_set(list, &iter, 0, text, -1);
}


void prefs_show_dialog(void)
{
	if (ui_widgets.prefs_dialog == NULL)
	{
		GtkListStore *encoding_list, *eol_list;
		GtkWidget *label;
		guint i;
		gchar *encoding_string;
		GdkPixbuf *pb;

		ui_widgets.prefs_dialog = create_prefs_dialog();
		gtk_widget_set_name(ui_widgets.prefs_dialog, "GeanyPrefsDialog");
		gtk_window_set_transient_for(GTK_WINDOW(ui_widgets.prefs_dialog), GTK_WINDOW(main_widgets.window));
		pb = ui_new_pixbuf_from_inline(GEANY_IMAGE_LOGO);
		gtk_window_set_icon(GTK_WINDOW(ui_widgets.prefs_dialog), pb);
		g_object_unref(pb);	/* free our reference */

		/* init the file encoding combo boxes */
		encoding_list = ui_builder_get_object("encoding_list");
		for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
		{
			encoding_string = encodings_to_string(&encodings[i]);
			list_store_append_text(encoding_list, encoding_string);
			g_free(encoding_string);
		}

		/* init the eol character combo box */
		eol_list = ui_builder_get_object("eol_list");
		list_store_append_text(eol_list, utils_get_eol_name(SC_EOL_CRLF));
		list_store_append_text(eol_list, utils_get_eol_name(SC_EOL_CR));
		list_store_append_text(eol_list, utils_get_eol_name(SC_EOL_LF));

		/* add manually GeanyWrapLabels because they can't be added with Glade */
		/* page Tools */
		label = geany_wrap_label_new(_("Enter tool paths below. Tools you do not need can be left blank."));
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(ui_lookup_widget(ui_widgets.prefs_dialog, "vbox33")),
			label, FALSE, TRUE, 5);
		/* page Templates */
		label = geany_wrap_label_new(_("Set the information to be used in templates. See the documentation for details."));
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(ui_lookup_widget(ui_widgets.prefs_dialog, "vbox31")),
			label, FALSE, TRUE, 5);
		/* page Keybindings */
		label = geany_wrap_label_new(_("Here you can change keyboard shortcuts for various actions. Select one and press the Change button to enter a new shortcut, or double click on an action to edit the string representation of the shortcut directly."));
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(ui_lookup_widget(ui_widgets.prefs_dialog, "vbox32")),
			label, FALSE, TRUE, 5);
		/* page Editor->Indentation */
		label = geany_wrap_label_new(_("<i>Warning: these settings are overridden by the current project. See <b>Project->Properties</b>.</i>"));
		gtk_widget_show(label);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_misc_set_padding(GTK_MISC(label), 6, 0);
		gtk_box_pack_start(GTK_BOX(ui_lookup_widget(ui_widgets.prefs_dialog,
			"label_project_indent_warning")), label, FALSE, TRUE, 5);

		/* add the clear icon to GtkEntry widgets in the dialog */
		{
			const gchar *names[] = {
				"startup_path_entry",
				"project_file_path_entry",
				"extra_plugin_path_entry",
				"entry_toggle_mark",
			/*	"entry_com_make", */
				"entry_com_term",
				"entry_browser",
				"entry_grep",
				"entry_contextaction",
				"entry_template_developer",
				"entry_template_initial",
				"entry_template_mail",
				"entry_template_company",
				"entry_template_version",
				"entry_template_year",
				"entry_template_date",
				"entry_template_datetime",
				"entry_print_external_cmd",
				"entry_print_dateformat"};
			const gchar **name;

			foreach_c_array(name, names, G_N_ELEMENTS(names))
				ui_entry_add_clear_icon(GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, *name)));
		}

		/* page Various */
		various_treeview = GTK_TREE_VIEW(ui_lookup_widget(ui_widgets.prefs_dialog,
			"various_treeview"));
		stash_tree_setup(pref_groups, various_treeview);

#ifdef HAVE_VTE
		vte_append_preferences_tab();
#endif

#ifndef G_OS_WIN32
		gtk_widget_hide(ui_lookup_widget(ui_widgets.prefs_dialog, "check_native_windows_dialogs"));
#endif
		ui_setup_open_button_callback(ui_lookup_widget(ui_widgets.prefs_dialog, "startup_path_button"), NULL,
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "startup_path_entry")));
		ui_setup_open_button_callback(ui_lookup_widget(ui_widgets.prefs_dialog, "extra_plugin_path_button"), NULL,
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "extra_plugin_path_entry")));

		g_signal_connect(ui_widgets.prefs_dialog, "response",
			G_CALLBACK(on_prefs_dialog_response), NULL);
		g_signal_connect(ui_widgets.prefs_dialog, "delete-event",
			G_CALLBACK(gtk_widget_hide_on_delete), NULL);

		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "tagbar_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(1));
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "msgwin_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(2));
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "editor_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(3));
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "long_line_color"),
				"color-set", G_CALLBACK(on_color_button_choose_cb), NULL);
		/* file chooser buttons in the tools tab */
		ui_setup_open_button_callback(ui_lookup_widget(ui_widgets.prefs_dialog, "button_term"),
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_com_term")));
		ui_setup_open_button_callback(ui_lookup_widget(ui_widgets.prefs_dialog, "button_browser"),
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_browser")));
		ui_setup_open_button_callback(ui_lookup_widget(ui_widgets.prefs_dialog, "button_grep"),
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_grep")));

		/* tools commands */
		ui_setup_open_button_callback(ui_lookup_widget(ui_widgets.prefs_dialog, "button_contextaction"),
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_contextaction")));

		/* printing */
		ui_setup_open_button_callback(ui_lookup_widget(ui_widgets.prefs_dialog, "button_print_external_cmd"),
			NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_ENTRY(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_print_external_cmd")));

		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "radio_print_gtk"),
			"toggled", G_CALLBACK(on_prefs_print_radio_button_toggled), NULL);
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "check_print_pageheader"),
			"toggled", G_CALLBACK(on_prefs_print_page_header_toggled), NULL);

		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "check_plugins"),
				"toggled", G_CALLBACK(on_enable_plugins_toggled), NULL);
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show"),
				"toggled", G_CALLBACK(on_toolbar_show_toggled), NULL);
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs"),
				"toggled", G_CALLBACK(on_show_notebook_tabs_toggled), NULL);
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "check_folding"),
				"toggled", G_CALLBACK(on_use_folding_toggled), NULL);
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "check_open_encoding"),
				"toggled", G_CALLBACK(on_open_encoding_toggled), NULL);
		g_signal_connect(ui_lookup_widget(ui_widgets.prefs_dialog, "check_sidebar_visible"),
				"toggled", G_CALLBACK(on_sidebar_visible_toggled), NULL);

		g_signal_connect(ui_widgets.prefs_dialog,
				"key-press-event", G_CALLBACK(prefs_dialog_key_press_response_cb), NULL);
	}

	prefs_init_dialog();
	gtk_window_present(GTK_WINDOW(ui_widgets.prefs_dialog));
}
