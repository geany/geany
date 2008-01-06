/*
 *      prefs.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *
 * $Id$
 */

/*
 * Preferences dialog support functions.
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
#include "msgwindow.h"
#include "sciwrappers.h"
#include "document.h"
#include "keyfile.h"
#include "keybindings.h"
#include "interface.h"
#include "encodings.h"
#include "project.h"
#include "editor.h"
#include "main.h"
#include "treeviews.h"
#include "printing.h"

#ifdef HAVE_VTE
# include "vte.h"
#endif

#ifdef G_OS_WIN32
# include "win32.h"
#endif


GeanyPrefs prefs;

static gchar *dialog_key_name;
static GtkTreeIter g_iter;
static GtkTreeStore *store = NULL;
static GtkTreeView *tree = NULL;
static GtkWidget *dialog_label;
static gboolean edited = FALSE;

static gboolean on_tree_view_button_press_event(
						GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void on_cell_edited(GtkCellRendererText *cellrenderertext, gchar *path, gchar *new_text, gpointer user_data);
static gboolean on_keytype_dialog_response(GtkWidget *dialog, GdkEventKey *event, gpointer user_data);
static void on_dialog_response(GtkWidget *dialog, gint response, gpointer user_data);
static gboolean find_duplicate(guint idx, guint key, GdkModifierType mods, const gchar *action);
static void on_toolbar_show_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_show_notebook_tabs_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_use_folding_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_open_encoding_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_openfiles_visible_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_prefs_print_radio_button_toggled(GtkToggleButton *togglebutton, gpointer user_data);
static void on_prefs_print_page_header_toggled(GtkToggleButton *togglebutton, gpointer user_data);


enum
{
	KB_TREE_ACTION,
	KB_TREE_SHORTCUT,
	KB_TREE_INDEX,
};

static void init_kb_tree()
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	tree = GTK_TREE_VIEW(lookup_widget(ui_widgets.prefs_dialog, "treeview7"));
	//g_object_set(tree, "vertical-separator", 6, NULL);

	store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Action"), renderer, "text", KB_TREE_ACTION, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Shortcut"), renderer, "text", KB_TREE_SHORTCUT, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	// set policy settings for the scollwedwindow around the treeview again, because glade
	// doesn't keep the settings
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(lookup_widget(ui_widgets.prefs_dialog, "scrolledwindow8")),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(on_cell_edited), NULL);
	g_signal_connect(G_OBJECT(tree), "button-press-event",
				G_CALLBACK(on_tree_view_button_press_event), NULL);
	g_signal_connect(G_OBJECT(lookup_widget(ui_widgets.prefs_dialog, "button2")), "clicked",
				G_CALLBACK(on_tree_view_button_press_event), NULL);
}


static void init_keybindings()
{
	GtkTreeIter parent, iter;
	gint i;
	gchar *key_string;

	if (store == NULL)
		init_kb_tree();

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		if (keys[i]->section != NULL)
		{
			gtk_tree_store_append(store, &parent, NULL);
			gtk_tree_store_set(store, &parent, KB_TREE_ACTION, keys[i]->section, -1);
		}

		key_string = gtk_accelerator_name(keys[i]->key, keys[i]->mods);
		gtk_tree_store_append(store, &iter, &parent);
		gtk_tree_store_set(store, &iter, KB_TREE_ACTION, keys[i]->label,
			KB_TREE_SHORTCUT, key_string, KB_TREE_INDEX, i, -1);
		g_free(key_string);
	}
	gtk_tree_view_expand_all(GTK_TREE_VIEW(tree));
}


void prefs_init_dialog(void)
{
	GtkWidget *widget;
	GdkColor *color;

	// General settings
	widget = lookup_widget(ui_widgets.prefs_dialog, "spin_mru");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), prefs.mru_length);

	// startup
	widget = lookup_widget(ui_widgets.prefs_dialog, "check_load_session");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.load_session);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_save_win_pos");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.save_winpos);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_plugins");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.load_plugins);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_ask_for_quit");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.confirm_exit);

	// behaviour
	widget = lookup_widget(ui_widgets.prefs_dialog, "check_beep");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.beep_on_errors);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_switch_pages");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.switch_msgwin_pages);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_suppress_status_msgs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.suppress_status_messages);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_auto_focus");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.auto_focus);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_ask_suppress_search_dialogs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.suppress_search_dialogs);

	widget = lookup_widget(ui_widgets.prefs_dialog, "entry_contextaction");
	gtk_entry_set_text(GTK_ENTRY(widget), prefs.context_action_cmd);

	widget = lookup_widget(ui_widgets.prefs_dialog, "startup_path_entry");
	gtk_entry_set_text(GTK_ENTRY(widget), prefs.default_open_path);

	project_setup_prefs();	// project files path


	// Interface settings
	widget = lookup_widget(ui_widgets.prefs_dialog, "check_list_symbol");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.sidebar_symbol_visible);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.sidebar_openfiles_visible);
	on_openfiles_visible_toggled(GTK_TOGGLE_BUTTON(widget), NULL);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles_fullpath");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.sidebar_openfiles_fullpath);

	widget = lookup_widget(ui_widgets.prefs_dialog, "tagbar_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), prefs.tagbar_font);

	widget = lookup_widget(ui_widgets.prefs_dialog, "msgwin_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), prefs.msgwin_font);

	widget = lookup_widget(ui_widgets.prefs_dialog, "editor_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), prefs.editor_font);

	widget = lookup_widget(ui_widgets.prefs_dialog, "spin_long_line");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.long_line_column);

	switch (editor_prefs.long_line_type)
	{
		case 0: widget = lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_line"); break;
		case 1: widget = lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_background"); break;
		default: widget = lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_disabled"); break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	color = g_new0(GdkColor, 1);
	gdk_color_parse(editor_prefs.long_line_color, color);
	widget = lookup_widget(ui_widgets.prefs_dialog, "long_line_color");
	gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), color);
	g_free(color);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.show_notebook_tabs);
	// disable following setting if notebook tabs are hidden
	on_show_notebook_tabs_toggled(GTK_TOGGLE_BUTTON(
					lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs")), NULL);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_show_tab_cross");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.show_tab_cross);

	widget = lookup_widget(ui_widgets.prefs_dialog, "combo_tab_editor");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), prefs.tab_pos_editor);

	widget = lookup_widget(ui_widgets.prefs_dialog, "combo_tab_msgwin");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), prefs.tab_pos_msgwin);

	widget = lookup_widget(ui_widgets.prefs_dialog, "combo_tab_sidebar");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), prefs.tab_pos_sidebar);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_statusbar_visible");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.statusbar_visible);


	// Toolbar settings
	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_visible);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_search");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_search);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_goto");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_goto);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_compile");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_compile);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_zoom");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_zoom);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_indent");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_indent);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_undo");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_undo);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_navigation");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_navigation);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_colour");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_colour);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_fileops");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_fileops);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_quit");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.toolbar_show_quit);


	switch (prefs.toolbar_icon_style)
	{
		case 0: widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_image"); break;
		case 1: widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_text"); break;
		default: widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_imagetext"); break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);


	switch (prefs.toolbar_icon_size)
	{
		case GTK_ICON_SIZE_LARGE_TOOLBAR:
				widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_large"); break;
		default: widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_small"); break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	// disable elements if toolbar is hidden
	on_toolbar_show_toggled(GTK_TOGGLE_BUTTON(
					lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show")), NULL);


	// Files settings
	if (prefs.tab_order_ltr)
		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_tab_right");
	else
		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_tab_left");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);


	// Editor settings
	widget = lookup_widget(ui_widgets.prefs_dialog, "spin_tab_width");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.tab_width);

	widget = lookup_widget(ui_widgets.prefs_dialog, "combo_new_encoding");
	// luckily the index of the combo box items match the index of the encodings array
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), prefs.default_new_encoding);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_open_encoding");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			(prefs.default_open_encoding >= 0) ? TRUE : FALSE);
	on_open_encoding_toggled(GTK_TOGGLE_BUTTON(widget), NULL);

	widget = lookup_widget(ui_widgets.prefs_dialog, "combo_open_encoding");
	if (prefs.default_open_encoding >= 0)
	{
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), prefs.default_open_encoding);
	}
	else
		gtk_combo_box_set_active(GTK_COMBO_BOX(widget), GEANY_ENCODING_UTF_8);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_trailing_spaces");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.strip_trailing_spaces);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_new_line");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.final_new_line);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_replace_tabs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), prefs.replace_tabs);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_indent");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_indent_guide);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_white_space");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_white_space);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_line_end");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.show_line_endings);

	widget = lookup_widget(ui_widgets.prefs_dialog, "combo_auto_indent_mode");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), editor_prefs.indent_mode);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_detect_indent");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.detect_tab_mode);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_line_wrapping");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.line_wrapping);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_complete_snippets");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.complete_snippets);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_xmltag");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.auto_close_xml_tags);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_folding");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.folding);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_unfold_children");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.unfold_all_children);
	on_use_folding_toggled(GTK_TOGGLE_BUTTON(
					lookup_widget(ui_widgets.prefs_dialog, "check_folding")), NULL);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_disable_dnd");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.disable_dnd);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_smart_home");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.smart_home_key);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_newline_strip");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.newline_strip);

	if (editor_prefs.use_tabs)
		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_indent_tabs");
	else
		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_indent_spaces");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_indicators");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.use_indicators);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_symbol_auto_completion");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), editor_prefs.auto_complete_symbols);

	widget = lookup_widget(ui_widgets.prefs_dialog, "spin_symbollistheight");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.symbolcompletion_max_height);

	widget = lookup_widget(ui_widgets.prefs_dialog, "spin_symbol_complete_chars");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), editor_prefs.symbolcompletion_min_chars);


	// Tools Settings
	if (prefs.tools_make_cmd)
			gtk_entry_set_text(GTK_ENTRY(lookup_widget(ui_widgets.prefs_dialog, "entry_com_make")), prefs.tools_make_cmd);

    if (prefs.tools_term_cmd)
            gtk_entry_set_text(GTK_ENTRY(lookup_widget(ui_widgets.prefs_dialog, "entry_com_term")), prefs.tools_term_cmd);

	if (prefs.tools_browser_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(ui_widgets.prefs_dialog, "entry_browser")), prefs.tools_browser_cmd);

	if (prefs.tools_grep_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(ui_widgets.prefs_dialog, "entry_grep")), prefs.tools_grep_cmd);


	// Template settings
	widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_developer");
	gtk_entry_set_text(GTK_ENTRY(widget), prefs.template_developer);

	widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_company");
	gtk_entry_set_text(GTK_ENTRY(widget), prefs.template_company);

	widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_mail");
	gtk_entry_set_text(GTK_ENTRY(widget), prefs.template_mail);

	widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_initial");
	gtk_entry_set_text(GTK_ENTRY(widget), prefs.template_initial);

	widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_version");
	gtk_entry_set_text(GTK_ENTRY(widget), prefs.template_version);


	// Keybindings
	init_keybindings();

	// Printing
	{
		GtkWidget *widget_gtk = lookup_widget(ui_widgets.prefs_dialog, "radio_print_gtk");
		if (printing_prefs.use_gtk_printing)
			widget = widget_gtk;
		else
			widget = lookup_widget(ui_widgets.prefs_dialog, "radio_print_external");

#if GTK_CHECK_VERSION(2, 10, 0)
		if (gtk_check_version(2, 10, 0) != NULL)
#endif
		{
			gtk_widget_set_sensitive(widget_gtk, FALSE); // disable the whole option block
			widget = lookup_widget(ui_widgets.prefs_dialog, "radio_print_external");
		}
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

		on_prefs_print_radio_button_toggled(GTK_TOGGLE_BUTTON(widget_gtk), NULL);
	}
	if (printing_prefs.external_print_cmd)
		gtk_entry_set_text(
			GTK_ENTRY(lookup_widget(ui_widgets.prefs_dialog, "entry_print_external_cmd")),
			printing_prefs.external_print_cmd);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_linenumbers");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.print_line_numbers);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_pagenumbers");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.print_page_numbers);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_pageheader");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.print_page_header);
	on_prefs_print_page_header_toggled(GTK_TOGGLE_BUTTON(widget), NULL);

	widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_basename");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), printing_prefs.page_header_basename);

	if (printing_prefs.page_header_datefmt)
		gtk_entry_set_text(
			GTK_ENTRY(lookup_widget(ui_widgets.prefs_dialog, "entry_print_dateformat")),
			printing_prefs.page_header_datefmt);


#ifndef HAVE_PLUGINS
	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "check_plugins"), FALSE);
#endif

#ifdef HAVE_VTE
	// make VTE switch visible only when VTE is compiled in, it is hidden by default
	widget = lookup_widget(ui_widgets.prefs_dialog, "check_vte");
	gtk_widget_show(widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vte_info.load_vte);

	// VTE settings
	if (vte_info.have_vte)
	{
		widget = lookup_widget(ui_widgets.prefs_dialog, "font_term");
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), vc->font);

		widget = lookup_widget(ui_widgets.prefs_dialog, "color_fore");
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), vc->colour_fore);

		widget = lookup_widget(ui_widgets.prefs_dialog, "color_back");
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), vc->colour_back);

		widget = lookup_widget(ui_widgets.prefs_dialog, "spin_scrollback");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), vc->scrollback_lines);

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_emulation");
		gtk_entry_set_text(GTK_ENTRY(widget), vc->emulation);

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_shell");
		gtk_entry_set_text(GTK_ENTRY(widget), vc->shell);

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_scroll_key");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->scroll_on_key);

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_scroll_out");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->scroll_on_out);

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_ignore_menu_key");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->ignore_menu_bar_accel);

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_follow_path");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->follow_path);

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_run_in_vte");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->run_in_vte);

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_skip_script");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->skip_run_script);
	}
#endif
}


/*
 * callbacks
 */
static void
on_prefs_button_clicked(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GtkWidget *widget;
		guint i;

		// General settings
		widget = lookup_widget(ui_widgets.prefs_dialog, "spin_mru");
		prefs.mru_length = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		// startup
		widget = lookup_widget(ui_widgets.prefs_dialog, "check_load_session");
		prefs.load_session = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_save_win_pos");
		prefs.save_winpos = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_plugins");
		prefs.load_plugins = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_ask_for_quit");
		prefs.confirm_exit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		// behaviour
		widget = lookup_widget(ui_widgets.prefs_dialog, "check_beep");
		prefs.beep_on_errors = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_ask_suppress_search_dialogs");
		prefs.suppress_search_dialogs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_switch_pages");
		prefs.switch_msgwin_pages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_suppress_status_msgs");
		prefs.suppress_status_messages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_auto_focus");
		prefs.auto_focus = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_contextaction");
		g_free(prefs.context_action_cmd);
		prefs.context_action_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "startup_path_entry");
		g_free(prefs.default_open_path);
		prefs.default_open_path = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		project_apply_prefs();	// project file path


		// Interface settings
		widget = lookup_widget(ui_widgets.prefs_dialog, "check_list_symbol");
		prefs.sidebar_symbol_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles");
		prefs.sidebar_openfiles_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles_fullpath");
		prefs.sidebar_openfiles_fullpath = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_line");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) editor_prefs.long_line_type = 0;
		else
		{
			widget = lookup_widget(ui_widgets.prefs_dialog, "radio_long_line_background");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) editor_prefs.long_line_type = 1;
			else
			{	// now only the disabled radio remains, so disable it
				editor_prefs.long_line_type = 2;
			}
		}
		if (editor_prefs.long_line_column == 0) editor_prefs.long_line_type = 2;

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs");
		prefs.show_notebook_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_show_tab_cross");
		prefs.show_tab_cross = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "combo_tab_editor");
		prefs.tab_pos_editor = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "combo_tab_msgwin");
		prefs.tab_pos_msgwin = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "combo_tab_sidebar");
		prefs.tab_pos_sidebar = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_statusbar_visible");
		prefs.statusbar_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));


		// Toolbar settings
		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show");
		prefs.toolbar_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_search");
		prefs.toolbar_show_search = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_goto");
		prefs.toolbar_show_goto = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_zoom");
		prefs.toolbar_show_zoom = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_indent");
		prefs.toolbar_show_indent = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_undo");
		prefs.toolbar_show_undo = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_navigation");
		prefs.toolbar_show_navigation = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_compile");
		prefs.toolbar_show_compile = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_colour");
		prefs.toolbar_show_colour = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_fileops");
		prefs.toolbar_show_fileops = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_quit");
		prefs.toolbar_show_quit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_imagetext");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) prefs.toolbar_icon_style = 2;
		else
		{
			widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_image");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
				prefs.toolbar_icon_style = 0;
			else
				// now only the text only radio remains, so set text only
				prefs.toolbar_icon_style = 1;
		}


		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_toolbar_large");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			prefs.toolbar_icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
		else
			prefs.toolbar_icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;


		// Files settings
		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_tab_right");
		prefs.tab_order_ltr = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));


		// Editor settings
		widget = lookup_widget(ui_widgets.prefs_dialog, "spin_tab_width");
		editor_prefs.tab_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "combo_new_encoding");
		prefs.default_new_encoding = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_open_encoding");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
			widget = lookup_widget(ui_widgets.prefs_dialog, "combo_open_encoding");
			prefs.default_open_encoding = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
		}
		else
			prefs.default_open_encoding = -1;

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_trailing_spaces");
		prefs.strip_trailing_spaces = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_new_line");
		prefs.final_new_line = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_replace_tabs");
		prefs.replace_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "spin_long_line");
		editor_prefs.long_line_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_folding");
		editor_prefs.folding = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		ui_update_fold_items();

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_unfold_children");
		editor_prefs.unfold_all_children = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_indent");
		editor_prefs.show_indent_guide = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_white_space");
		editor_prefs.show_white_space = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_line_end");
		editor_prefs.show_line_endings = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "combo_auto_indent_mode");
		editor_prefs.indent_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_line_wrapping");
		editor_prefs.line_wrapping = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_complete_snippets");
		editor_prefs.complete_snippets = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_xmltag");
		editor_prefs.auto_close_xml_tags = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_indicators");
		editor_prefs.use_indicators = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_disable_dnd");
		editor_prefs.disable_dnd = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_smart_home");
		editor_prefs.smart_home_key = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_newline_strip");
		editor_prefs.newline_strip = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_indent_tabs");
		{
			gboolean use_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			// override each document setting only if the default has changed
			if (editor_prefs.use_tabs != use_tabs)
			{
				editor_prefs.use_tabs = use_tabs;
				for (i = 0; i < doc_array->len; i++)
				{
					if (doc_list[i].is_valid)
						document_set_use_tabs(i, editor_prefs.use_tabs);
				}
			}
		}

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_detect_indent");
		editor_prefs.detect_tab_mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_symbol_auto_completion");
		editor_prefs.auto_complete_symbols = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "spin_symbol_complete_chars");
		editor_prefs.symbolcompletion_min_chars = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "spin_symbollistheight");
		editor_prefs.symbolcompletion_max_height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));


		// Tools Settings
		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_com_make");
		g_free(prefs.tools_make_cmd);
		prefs.tools_make_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_com_term");
		g_free(prefs.tools_term_cmd);
		prefs.tools_term_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_browser");
		g_free(prefs.tools_browser_cmd);
		prefs.tools_browser_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_grep");
		g_free(prefs.tools_grep_cmd);
		prefs.tools_grep_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


		// Template settings
		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_developer");
		g_free(prefs.template_developer);
		prefs.template_developer = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_company");
		g_free(prefs.template_company);
		prefs.template_company = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_mail");
		g_free(prefs.template_mail);
		prefs.template_mail = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_initial");
		g_free(prefs.template_initial);
		prefs.template_initial = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_template_version");
		g_free(prefs.template_version);
		prefs.template_version = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


		// Keybindings
		if (edited) keybindings_write_to_file();


		// Printing
		widget = lookup_widget(ui_widgets.prefs_dialog, "radio_print_gtk");
		printing_prefs.use_gtk_printing = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_print_external_cmd");
		g_free(printing_prefs.external_print_cmd);
		printing_prefs.external_print_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_linenumbers");
		printing_prefs.print_line_numbers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_pagenumbers");
		printing_prefs.print_page_numbers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_pageheader");
		printing_prefs.print_page_header = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "check_print_basename");
		printing_prefs.page_header_basename = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(ui_widgets.prefs_dialog, "entry_print_dateformat");
		g_free(printing_prefs.page_header_datefmt);
		printing_prefs.page_header_datefmt = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


#ifdef HAVE_VTE
		widget = lookup_widget(ui_widgets.prefs_dialog, "check_vte");
		vte_info.load_vte = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		// VTE settings
		if (vte_info.have_vte)
		{
			widget = lookup_widget(ui_widgets.prefs_dialog, "spin_scrollback");
			vc->scrollback_lines = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

			widget = lookup_widget(ui_widgets.prefs_dialog, "entry_emulation");
			g_free(vc->emulation);
			vc->emulation = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

			widget = lookup_widget(ui_widgets.prefs_dialog, "entry_shell");
			g_free(vc->shell);
			vc->shell = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

			widget = lookup_widget(ui_widgets.prefs_dialog, "check_scroll_key");
			vc->scroll_on_key = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = lookup_widget(ui_widgets.prefs_dialog, "check_scroll_out");
			vc->scroll_on_out = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = lookup_widget(ui_widgets.prefs_dialog, "check_ignore_menu_key");
			vc->ignore_menu_bar_accel = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = lookup_widget(ui_widgets.prefs_dialog, "check_follow_path");
			vc->follow_path = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = lookup_widget(ui_widgets.prefs_dialog, "check_run_in_vte");
			vc->run_in_vte = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = lookup_widget(ui_widgets.prefs_dialog, "check_skip_script");
			vc->skip_run_script = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			vte_apply_user_settings();
		}
#endif

		// apply the changes made
		ui_statusbar_showhide(prefs.statusbar_visible);
		treeviews_openfiles_update_all(); // to update if full path setting has changed
		ui_update_toolbar_items();
		ui_update_toolbar_icons(prefs.toolbar_icon_size);
		gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), prefs.toolbar_icon_style);
		ui_treeviews_show_hide(FALSE);
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(app->notebook), prefs.show_notebook_tabs);

		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->notebook), prefs.tab_pos_editor);
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(msgwindow.notebook), prefs.tab_pos_msgwin);
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->treeview_notebook), prefs.tab_pos_sidebar);

		// re-colourise all open documents, if tab width or long line settings have changed
		for (i = 0; i < doc_array->len; i++)
		{
			if (doc_list[i].is_valid)
			{
				document_apply_update_prefs(i);
				if (! editor_prefs.folding)
					document_unfold_all(i);
			}
		}
		ui_document_show_hide(-1);

		// store all settings
		configuration_save();
	}

	if (response != GTK_RESPONSE_APPLY)
	{
		gtk_tree_store_clear(store);
		gtk_widget_hide(GTK_WIDGET(dialog));
	}
}


void on_prefs_color_choosed(GtkColorButton *widget, gpointer user_data)
{
	GdkColor color;

	switch (GPOINTER_TO_INT(user_data))
	{
		case 1:
		{
			gtk_color_button_get_color(widget, &color);
			editor_prefs.long_line_color = utils_get_hex_from_color(&color);
			break;
		}
#ifdef HAVE_VTE
		case 2:
		{
			g_free(vc->colour_fore);
			vc->colour_fore = g_new0(GdkColor, 1);
			gtk_color_button_get_color(widget, vc->colour_fore);
			break;
		}
		case 3:
		{
			g_free(vc->colour_back);
			vc->colour_back = g_new0(GdkColor, 1);
			gtk_color_button_get_color(widget, vc->colour_back);
			break;
		}
#endif
	}
}


void on_prefs_font_choosed(GtkFontButton *widget, gpointer user_data)
{
	const gchar *fontbtn = gtk_font_button_get_font_name(widget);
	guint i;

	switch (GPOINTER_TO_INT(user_data))
	{
		case 1:
		{
			if (strcmp(fontbtn, prefs.tagbar_font) == 0) break;
			g_free(prefs.tagbar_font);
			prefs.tagbar_font = g_strdup(fontbtn);
			for (i = 0; i < doc_array->len; i++)
			{
				if (doc_list[i].is_valid && GTK_IS_WIDGET(doc_list[i].tag_tree))
					ui_widget_modify_font_from_string(doc_list[i].tag_tree,
						prefs.tagbar_font);
			}
			if (GTK_IS_WIDGET(tv.default_tag_tree))
				ui_widget_modify_font_from_string(tv.default_tag_tree, prefs.tagbar_font);
			ui_widget_modify_font_from_string(lookup_widget(app->window, "entry1"),
				prefs.tagbar_font);
			break;
		}
		case 2:
		{
			if (strcmp(fontbtn, prefs.msgwin_font) == 0) break;
			g_free(prefs.msgwin_font);
			prefs.msgwin_font = g_strdup(fontbtn);
			ui_widget_modify_font_from_string(msgwindow.tree_compiler, prefs.msgwin_font);
			ui_widget_modify_font_from_string(msgwindow.tree_msg, prefs.msgwin_font);
			ui_widget_modify_font_from_string(msgwindow.tree_status, prefs.msgwin_font);
			break;
		}
		case 3:
		{
			ui_set_editor_font(fontbtn);
			break;
		}
#ifdef HAVE_VTE
		case 4:
		{
			// VTE settings
			if (strcmp(fontbtn, vc->font) == 0) break;
			g_free(vc->font);
			vc->font = g_strdup(gtk_font_button_get_font_name(widget));
			vte_apply_user_settings();
			break;
		}
#endif
	}
}


static gboolean on_tree_view_button_press_event(
						GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gchar *name;

	// discard click events in the tree unless it is a double click
	if (widget == (GtkWidget*)tree && event->type != GDK_2BUTTON_PRESS)
		return FALSE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	if (gtk_tree_selection_get_selected(selection, &model, &g_iter))
	{
		if (gtk_tree_model_iter_has_child(model, &g_iter))
		{	// double click on a section to expand or collapse it
			GtkTreePath *path = gtk_tree_model_get_path(model, &g_iter);

			if (gtk_tree_view_row_expanded(tree, path))
				gtk_tree_view_collapse_row(tree, path);
			else
				gtk_tree_view_expand_row(tree, path, FALSE);

			gtk_tree_path_free(path);
			return TRUE;
		}

		gtk_tree_model_get(model, &g_iter, 0, &name, -1);
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
					_("Type the combination of the keys you want to use for \"%s\""), name);
			label = gtk_label_new(str);
			gtk_misc_set_padding(GTK_MISC(label), 5, 10);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

			dialog_label = gtk_label_new("");
			gtk_misc_set_padding(GTK_MISC(dialog_label), 5, 10);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), dialog_label);

			g_signal_connect(G_OBJECT(dialog), "key-press-event",
								G_CALLBACK(on_keytype_dialog_response), NULL);
			g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(on_dialog_response), NULL);

			// copy name to global variable to hold it, will be freed in on_dialog_response()
			dialog_key_name = g_strdup(name);

			gtk_widget_show_all(dialog);
			g_free(str);
			g_free(name);
		}
	}
	return TRUE;
}


static void on_cell_edited(GtkCellRendererText *cellrenderertext, gchar *path, gchar *new_text, gpointer user_data)
{
	if (path != NULL && new_text != NULL)
	{
		guint idx;
		guint lkey;
		GdkModifierType lmods;
		GtkTreeIter iter;

		gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store), &iter, path);
		if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter))
			return;

		gtk_accelerator_parse(new_text, &lkey, &lmods);

		// get index
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 2, &idx, -1);

		if (find_duplicate(idx, lkey, lmods, new_text))
			return;

		// set the values here, because of the above check, setting it in gtk_accelerator_parse would
		// return a wrong key combination if it is duplicate
		keys[idx]->key = lkey;
		keys[idx]->mods = lmods;

		gtk_tree_store_set(store, &iter, 1, new_text, -1);

		edited = TRUE;
	}
}


static gboolean on_keytype_dialog_response(GtkWidget *dialog, GdkEventKey *event, gpointer user_data)
{
	gchar *str;

	if (event->state == 0 && event->keyval == GDK_Escape)
		return FALSE;	// close the dialog, don't allow escape when detecting keybindings.

	// ignore numlock key, not necessary but nice
	if (event->state & GDK_MOD2_MASK) event->state -= GDK_MOD2_MASK;

	str = gtk_accelerator_name(event->keyval, event->state);

	gtk_label_set_text(GTK_LABEL(dialog_label), str);
	g_free(str);

	return TRUE;
}


static void on_dialog_response(GtkWidget *dialog, gint response, gpointer iter)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		guint idx;
		guint lkey;
		GdkModifierType lmods;

		// get index
		gtk_tree_model_get(GTK_TREE_MODEL(store), &g_iter, 2, &idx, -1);

		gtk_accelerator_parse(gtk_label_get_text(GTK_LABEL(dialog_label)), &lkey, &lmods);

		if (find_duplicate(idx, lkey, lmods, gtk_label_get_text(GTK_LABEL(dialog_label))))
			return;

		// set the values here, because of the above check, setting it in gtk_accelerator_parse would
		// return a wrong key combination if it is duplicate
		keys[idx]->key = lkey;
		keys[idx]->mods = lmods;

		gtk_tree_store_set(store, &g_iter,
				1, gtk_label_get_text(GTK_LABEL(dialog_label)), -1);

		g_free(dialog_key_name);
		dialog_key_name = NULL;

		edited = TRUE;
	}
	gtk_widget_destroy(dialog);
}


static gboolean find_iter(guint i, GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	guint idx;
	GtkTreeIter parent;

	if (! gtk_tree_model_get_iter_first(model, &parent))
		return FALSE;	// no items

	while (TRUE)
	{
		if (! gtk_tree_model_iter_children(model, iter, &parent))
			return FALSE;

		while (TRUE)
		{
			gtk_tree_model_get(model, iter, 2, &idx, -1);
			if (idx == i)
				return TRUE;
			if (! gtk_tree_model_iter_next(model, iter))
				break;
		}
		if (! gtk_tree_model_iter_next(model, &parent))
			return FALSE;
	}
}


// test if the entered key combination is already used
static gboolean find_duplicate(guint idx, guint key, GdkModifierType mods, const gchar *action)
{
	guint i;

	// allow duplicate if there is no key combination
	if (key == 0 && mods == 0) return FALSE;

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		// search another item with the same key, but take not the key we are searching for(-> idx)
		if (keys[i]->key == key && keys[i]->mods == mods
			&& ! (keys[i]->key == keys[idx]->key && keys[i]->mods == keys[idx]->mods))
		{
			if (dialogs_show_question_full(app->window, _("_Override"), GTK_STOCK_CANCEL,
				_("Override that keybinding?"),
				_("The combination '%s' is already used for \"%s\"."),
				action, keys[i]->label))
			{
				GtkTreeIter iter;

				keys[i]->key = 0;
				keys[i]->mods = 0;
				if (find_iter(i, &iter))
					gtk_tree_store_set(store, &iter, 1, NULL, -1);	// clear item
				continue;
			}
			return TRUE;
		}
	}
	return FALSE;
}


static void on_toolbar_show_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "frame11"), sens);
	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "frame13"), sens);
}


static void on_show_notebook_tabs_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	// tab placement only enabled when tabs are visible
	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "combo_tab_editor"), sens);
	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "check_show_tab_cross"), sens);
}


static void on_use_folding_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "check_unfold_children"), sens);
}


static void on_open_encoding_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "eventbox3"), sens);
	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "label_open_encoding"), sens);
}


static void on_openfiles_visible_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles_fullpath"), sens);
}


static void on_prefs_print_radio_button_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "vbox29"), sens);
	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "hbox9"), ! sens);
}


static void on_prefs_print_page_header_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gboolean sens = gtk_toggle_button_get_active(togglebutton);

	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "check_print_basename"), sens);
	gtk_widget_set_sensitive(lookup_widget(ui_widgets.prefs_dialog, "entry_print_dateformat"), sens);
}


void prefs_show_dialog(void)
{
	if (ui_widgets.prefs_dialog == NULL)
	{
		GtkWidget *combo_new, *combo_open;
		guint i;
		gchar *encoding_string;

		ui_widgets.prefs_dialog = create_prefs_dialog();
		gtk_widget_set_name(ui_widgets.prefs_dialog, "GeanyPrefsDialog");
		gtk_window_set_transient_for(GTK_WINDOW(ui_widgets.prefs_dialog), GTK_WINDOW(app->window));

		// init the default file encoding combo box
		combo_new = lookup_widget(ui_widgets.prefs_dialog, "combo_new_encoding");
		combo_open = lookup_widget(ui_widgets.prefs_dialog, "combo_open_encoding");
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo_new), 3);
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo_open), 3);
		for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
		{
			encoding_string = encodings_to_string(&encodings[i]);
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo_new), encoding_string);
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo_open), encoding_string);
			g_free(encoding_string);
		}

#ifdef HAVE_VTE
		vte_append_preferences_tab();
#endif

		ui_setup_open_button_callback(lookup_widget(ui_widgets.prefs_dialog, "startup_path_button"), NULL,
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_ENTRY(lookup_widget(ui_widgets.prefs_dialog, "startup_path_entry")));

		g_signal_connect((gpointer) ui_widgets.prefs_dialog, "response",
			G_CALLBACK(on_prefs_button_clicked), NULL);
		g_signal_connect((gpointer) ui_widgets.prefs_dialog, "delete_event",
			G_CALLBACK(gtk_widget_hide_on_delete), NULL);

		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "tagbar_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(1));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "msgwin_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(2));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "editor_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(3));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "long_line_color"),
				"color-set", G_CALLBACK(on_prefs_color_choosed), GINT_TO_POINTER(1));
		// file chooser buttons in the tools tab
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "button_make"),
				"clicked", G_CALLBACK(on_prefs_tools_button_clicked), lookup_widget(ui_widgets.prefs_dialog, "entry_com_make"));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "button_term"),
				"clicked", G_CALLBACK(on_prefs_tools_button_clicked), lookup_widget(ui_widgets.prefs_dialog, "entry_com_term"));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "button_browser"),
				"clicked", G_CALLBACK(on_prefs_tools_button_clicked), lookup_widget(ui_widgets.prefs_dialog, "entry_browser"));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "button_grep"),
				"clicked", G_CALLBACK(on_prefs_tools_button_clicked), lookup_widget(ui_widgets.prefs_dialog, "entry_grep"));

		// tools commands
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "button_contextaction"),
			"clicked", G_CALLBACK(on_prefs_tools_button_clicked), lookup_widget(ui_widgets.prefs_dialog, "entry_contextaction"));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "button_contextaction"),
			"clicked", G_CALLBACK(on_prefs_tools_button_clicked), lookup_widget(ui_widgets.prefs_dialog, "entry_contextaction"));

		// printing
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "button_print_external_cmd"),
			"clicked", G_CALLBACK(on_prefs_tools_button_clicked), lookup_widget(ui_widgets.prefs_dialog, "entry_print_external_cmd"));
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "radio_print_gtk"),
			"toggled", G_CALLBACK(on_prefs_print_radio_button_toggled), NULL);
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "check_print_pageheader"),
			"toggled", G_CALLBACK(on_prefs_print_page_header_toggled), NULL);

		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "check_toolbar_show"),
				"toggled", G_CALLBACK(on_toolbar_show_toggled), NULL);
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "check_show_notebook_tabs"),
				"toggled", G_CALLBACK(on_show_notebook_tabs_toggled), NULL);
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "check_folding"),
				"toggled", G_CALLBACK(on_use_folding_toggled), NULL);
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "check_open_encoding"),
				"toggled", G_CALLBACK(on_open_encoding_toggled), NULL);
		g_signal_connect((gpointer) lookup_widget(ui_widgets.prefs_dialog, "check_list_openfiles"),
				"toggled", G_CALLBACK(on_openfiles_visible_toggled), NULL);
	}

	prefs_init_dialog();
	gtk_widget_show(ui_widgets.prefs_dialog);
}


void
on_prefs_tools_button_clicked           (GtkButton       *button,
                                        gpointer         item)
{
#ifdef G_OS_WIN32
	win32_show_pref_file_dialog(item);
#else
	GtkWidget *dialog;
	gchar *filename, *tmp, **field;

	// initialize the dialog
	dialog = gtk_file_chooser_dialog_new(_("Open File"), GTK_WINDOW(ui_widgets.prefs_dialog),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	// cut the options from the command line
	field = g_strsplit(gtk_entry_get_text(GTK_ENTRY(item)), " ", 2);
	if (field[0])
	{
		filename = g_find_program_in_path(field[0]);
		if (filename)
		{
			gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), filename);
			g_free(filename);
		}
	}

	// run it
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		tmp = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (g_strv_length(field) > 1)
			filename = g_strconcat(tmp, " ", field[1], NULL);
		else
		{
			filename = tmp;
			tmp = NULL;
		}
		gtk_entry_set_text(GTK_ENTRY(item), filename);
		g_free(filename);
		g_free(tmp);
	}

	g_strfreev(field);
	gtk_widget_destroy(dialog);
#endif
}


