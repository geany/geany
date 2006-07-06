/*
 *      prefs.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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


#include <stdlib.h>
#include <string.h>

#include "geany.h"

#include "prefs.h"
#include "support.h"
#include "dialogs.h"
#include "utils.h"
#include "msgwindow.h"
#include "sciwrappers.h"
#include "document.h"
#include "keyfile.h"
#include "keybindings.h"

#ifdef HAVE_VTE
# include "vte.h"
#endif


gint old_tab_width;
gint old_long_line_column;
gchar *old_long_line_color;
gchar *dialog_key_name;
static GtkListStore *store = NULL;
static GtkTreeView *tree = NULL;
GtkWidget *dialog_label;
static gboolean edited = FALSE;

static gboolean on_prefs_tree_view_button_press_event(
						GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void on_cell_edited(GtkCellRendererText *cellrenderertext, gchar *path, gchar *new_text, gpointer user_data);
static gboolean on_keytype_dialog_response(GtkWidget *dialog, GdkEventKey *event, gpointer user_data);
static void on_dialog_response(GtkWidget *dialog, gint response, gpointer user_data);
static gboolean find_duplicate(guint idx, guint key, GdkModifierType mods, const gchar *action);


void prefs_init_dialog(void)
{
	GtkWidget *widget;
	GdkColor *color;
	GtkTreeIter iter;
	guint i;
	gchar *key_string;

	// General settings
	widget = lookup_widget(app->prefs_dialog, "spin_mru");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), app->mru_length);

	widget = lookup_widget(app->prefs_dialog, "check_load_session");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_main_load_session);

	widget = lookup_widget(app->prefs_dialog, "check_save_win_pos");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_main_save_winpos);

	widget = lookup_widget(app->prefs_dialog, "check_beep");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->beep_on_errors);

	widget = lookup_widget(app->prefs_dialog, "check_switch_pages");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->switch_msgwin_pages);

	widget = lookup_widget(app->prefs_dialog, "check_ask_for_quit");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_main_confirm_exit);

	if (app->tab_order_ltr)
		widget = lookup_widget(app->prefs_dialog, "radio_tab_right");
	else
		widget = lookup_widget(app->prefs_dialog, "radio_tab_left");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);


	// Interface settings
	widget = lookup_widget(app->prefs_dialog, "check_toolbar_search");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_main_show_search);

	widget = lookup_widget(app->prefs_dialog, "check_toolbar_goto");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_main_show_goto);

	widget = lookup_widget(app->prefs_dialog, "check_list_symbol");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->treeview_symbol_visible);

	widget = lookup_widget(app->prefs_dialog, "check_list_openfiles");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->treeview_openfiles_visible);

	widget = lookup_widget(app->prefs_dialog, "tagbar_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), app->tagbar_font);

	widget = lookup_widget(app->prefs_dialog, "msgwin_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), app->msgwin_font);

	widget = lookup_widget(app->prefs_dialog, "editor_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), app->editor_font);

	widget = lookup_widget(app->prefs_dialog, "spin_long_line");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), app->long_line_column);
	old_long_line_column = app->long_line_column;

	switch (app->long_line_type)
	{
		case 0: widget = lookup_widget(app->prefs_dialog, "radio_long_line_line"); break;
		case 1: widget = lookup_widget(app->prefs_dialog, "radio_long_line_background"); break;
		default: widget = lookup_widget(app->prefs_dialog, "radio_long_line_disabled"); break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	old_long_line_color = g_strdup(app->long_line_color);

	color = g_new0(GdkColor, 1);
	gdk_color_parse(app->long_line_color, color);
	widget = lookup_widget(app->prefs_dialog, "long_line_color");
	gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), color);
	g_free(color);

	widget = lookup_widget(app->prefs_dialog, "combo_tab_editor");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), app->tab_pos_editor);

	widget = lookup_widget(app->prefs_dialog, "combo_tab_msgwin");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), app->tab_pos_msgwin);

	widget = lookup_widget(app->prefs_dialog, "combo_tab_sidebar");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), app->tab_pos_sidebar);


	// Editor settings
	widget = lookup_widget(app->prefs_dialog, "spin_tab_width");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), app->pref_editor_tab_width);
	old_tab_width = app->pref_editor_tab_width;

	widget = lookup_widget(app->prefs_dialog, "combo_encoding");
	// luckily the index of the combo box items match the index of the encodings array
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), app->pref_editor_default_encoding);

	widget = lookup_widget(app->prefs_dialog, "check_trailing_spaces");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_trail_space);

	widget = lookup_widget(app->prefs_dialog, "check_new_line");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_new_line);

	widget = lookup_widget(app->prefs_dialog, "check_replace_tabs");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_replace_tabs);

	widget = lookup_widget(app->prefs_dialog, "check_indent");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_show_indent_guide);

	widget = lookup_widget(app->prefs_dialog, "check_white_space");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_show_white_space);

	widget = lookup_widget(app->prefs_dialog, "check_line_end");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_show_line_endings);

	widget = lookup_widget(app->prefs_dialog, "check_xmltag");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_auto_close_xml_tags);

	widget = lookup_widget(app->prefs_dialog, "check_auto_complete");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_auto_complete_constructs);

	widget = lookup_widget(app->prefs_dialog, "check_folding");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_folding);

	widget = lookup_widget(app->prefs_dialog, "check_indicators");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_use_indicators);


	// Tools Settings
#ifdef GEANY_WIN32
        // hide related Terminal path setting
        gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "label11"), FALSE);
        gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "entry_com_make"), FALSE);
        gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "button_make"), FALSE);

        // hide related Terminal path setting
        gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "label97"), FALSE);
        gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "entry_com_term"), FALSE);
        gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "button_term"), FALSE);
#else
        if (app->tools_make_cmd)
                gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_make")), app->tools_make_cmd);

        if (app->tools_term_cmd)
                gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_term")), app->tools_term_cmd);
#endif
	if (app->tools_browser_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_browser")), app->tools_browser_cmd);

	if (app->tools_browser_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_print")), app->tools_print_cmd);


	// Template settings
	widget = lookup_widget(app->prefs_dialog, "entry_template_developer");
	gtk_entry_set_text(GTK_ENTRY(widget), app->pref_template_developer);

	widget = lookup_widget(app->prefs_dialog, "entry_template_company");
	gtk_entry_set_text(GTK_ENTRY(widget), app->pref_template_company);

	widget = lookup_widget(app->prefs_dialog, "entry_template_mail");
	gtk_entry_set_text(GTK_ENTRY(widget), app->pref_template_mail);

	widget = lookup_widget(app->prefs_dialog, "entry_template_initial");
	gtk_entry_set_text(GTK_ENTRY(widget), app->pref_template_initial);

	widget = lookup_widget(app->prefs_dialog, "entry_template_version");
	gtk_entry_set_text(GTK_ENTRY(widget), app->pref_template_version);


	// Keybindings
	if (store == NULL)
	{
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;

		tree = GTK_TREE_VIEW(lookup_widget(app->prefs_dialog, "treeview7"));
		//g_object_set(tree, "vertical-separator", 6, NULL);

		store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(_("Action"), renderer, "text", 0, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

		renderer = gtk_cell_renderer_text_new();
		g_object_set(renderer, "editable", TRUE, NULL);
		column = gtk_tree_view_column_new_with_attributes(_("Shortcut"), renderer, "text", 1, NULL);
		gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

		// set policy settings for the scollwedwindow around the treeview again, because glade
		// doesn't keep the settings
		gtk_scrolled_window_set_policy(
				GTK_SCROLLED_WINDOW(lookup_widget(app->prefs_dialog, "scrolledwindow8")),
				GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

		g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(on_cell_edited), NULL);
		g_signal_connect(G_OBJECT(tree), "button-press-event",
					G_CALLBACK(on_prefs_tree_view_button_press_event), NULL);
		g_signal_connect(G_OBJECT(lookup_widget(app->prefs_dialog, "button2")), "button-press-event",
					G_CALLBACK(on_prefs_tree_view_button_press_event), NULL);
	}

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		key_string = gtk_accelerator_name(keys[i]->key, keys[i]->mods);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, keys[i]->label, 1, key_string, -1);
		g_free(key_string);
	}


#ifdef HAVE_VTE
	// make VTE switch visible only when VTE is compiled in, it is hidden by default
	widget = lookup_widget(app->prefs_dialog, "check_vte");
	gtk_widget_show(widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->load_vte);

	// VTE settings
	if (app->have_vte)
	{
		widget = lookup_widget(app->prefs_dialog, "font_term");
		gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), vc->font);

		widget = lookup_widget(app->prefs_dialog, "color_fore");
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), vc->color_fore);

		widget = lookup_widget(app->prefs_dialog, "color_back");
		gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), vc->color_back);

		widget = lookup_widget(app->prefs_dialog, "spin_scrollback");
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), vc->scrollback_lines);

		widget = lookup_widget(app->prefs_dialog, "entry_emulation");
		gtk_entry_set_text(GTK_ENTRY(widget), vc->emulation);

		widget = lookup_widget(app->prefs_dialog, "check_scroll_key");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->scroll_on_key);

		widget = lookup_widget(app->prefs_dialog, "check_scroll_out");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->scroll_on_out);

		widget = lookup_widget(app->prefs_dialog, "check_follow_path");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), vc->follow_path);
	}
#endif
}

//gtk_notebook_set_tab_pos

/*
 * callbacks
 */
void on_prefs_button_clicked(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_OK)
	{
		GtkWidget *widget;
		gint i;

		// General settings
		widget = lookup_widget(app->prefs_dialog, "spin_mru");
		app->mru_length = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_load_session");
		app->pref_main_load_session = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_save_win_pos");
		app->pref_main_save_winpos = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_beep");
		app->beep_on_errors = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_ask_for_quit");
		app->pref_main_confirm_exit = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_switch_pages");
		app->switch_msgwin_pages = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "radio_tab_right");
		app->tab_order_ltr = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));


		// Interface settings
		widget = lookup_widget(app->prefs_dialog, "check_toolbar_search");
		app->pref_main_show_search = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_toolbar_goto");
		app->pref_main_show_goto = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_list_symbol");
		app->treeview_symbol_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_list_openfiles");
		app->treeview_openfiles_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "radio_long_line_line");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) app->long_line_type = 0;
		else
		{
			widget = lookup_widget(app->prefs_dialog, "radio_long_line_background");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) app->long_line_type = 1;
			else
			{	// now only the disabled radio remains, so disable it
				app->long_line_type = 2;
			}
		}
		if (app->long_line_column == 0) app->long_line_type = 2;

		widget = lookup_widget(app->prefs_dialog, "combo_tab_editor");
		app->tab_pos_editor = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(app->prefs_dialog, "combo_tab_msgwin");
		app->tab_pos_msgwin = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(app->prefs_dialog, "combo_tab_sidebar");
		app->tab_pos_sidebar = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));


		// Editor settings
		widget = lookup_widget(app->prefs_dialog, "spin_tab_width");
		app->pref_editor_tab_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "combo_encoding");
		app->pref_editor_default_encoding = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));

		widget = lookup_widget(app->prefs_dialog, "check_trailing_spaces");
		app->pref_editor_trail_space = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_new_line");
		app->pref_editor_new_line = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_replace_tabs");
		app->pref_editor_replace_tabs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "spin_long_line");
		app->long_line_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_folding");
		app->pref_editor_folding = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
		utils_update_fold_items();

		widget = lookup_widget(app->prefs_dialog, "check_indent");
		app->pref_editor_show_indent_guide = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_white_space");
		app->pref_editor_show_white_space = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_line_end");
		app->pref_editor_show_line_endings = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_xmltag");
		app->pref_editor_auto_close_xml_tags = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_auto_complete");
		app->pref_editor_auto_complete_constructs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_indicators");
		app->pref_editor_use_indicators = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));


		// Tools Settings
		widget = lookup_widget(app->prefs_dialog, "entry_com_make");
		g_free(app->tools_make_cmd);
		app->tools_make_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_com_term");
		g_free(app->tools_term_cmd);
		app->tools_term_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_browser");
		g_free(app->tools_browser_cmd);
		app->tools_browser_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_print");
		g_free(app->tools_print_cmd);
		app->tools_print_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


		// Template settings
		widget = lookup_widget(app->prefs_dialog, "entry_template_developer");
		g_free(app->pref_template_developer);
		app->pref_template_developer = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_template_company");
		g_free(app->pref_template_company);
		app->pref_template_company = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_template_mail");
		g_free(app->pref_template_mail);
		app->pref_template_mail = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_template_initial");
		g_free(app->pref_template_initial);
		app->pref_template_initial = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_template_version");
		g_free(app->pref_template_version);
		app->pref_template_version = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));


		// Keybindings
		if (edited) keybindings_write_to_file();

#ifdef HAVE_VTE
		widget = lookup_widget(app->prefs_dialog, "check_vte");
		app->load_vte = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		// VTE settings
		if (app->have_vte)
		{
			gchar *hex_color_back, *hex_color_fore;

			widget = lookup_widget(app->prefs_dialog, "spin_scrollback");
			vc->scrollback_lines = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

			widget = lookup_widget(app->prefs_dialog, "entry_emulation");
			g_free(vc->emulation);
			vc->emulation = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

			widget = lookup_widget(app->prefs_dialog, "check_scroll_key");
			vc->scroll_on_key = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = lookup_widget(app->prefs_dialog, "check_scroll_out");
			vc->scroll_on_out = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			widget = lookup_widget(app->prefs_dialog, "check_follow_path");
			vc->follow_path = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

			g_free(app->terminal_settings);
			hex_color_fore = utils_get_hex_from_color(vc->color_fore);
			hex_color_back = utils_get_hex_from_color(vc->color_back);
			app->terminal_settings = g_strdup_printf("%s;%s;%s;%d;%s;%s;%s;%s", vc->font,
					hex_color_fore, hex_color_back,
					vc->scrollback_lines, vc->emulation,
					utils_btoa(vc->scroll_on_key), utils_btoa(vc->scroll_on_out),
					utils_btoa(vc->follow_path));

			vte_apply_user_settings();
			g_free(hex_color_fore);
			g_free(hex_color_back);
		}
#endif

		// apply the changes made
		utils_widget_show_hide(lookup_widget(app->window, "entry1"), app->pref_main_show_search);
		utils_widget_show_hide(lookup_widget(app->window, "toolbutton18"), app->pref_main_show_search);
		utils_widget_show_hide(lookup_widget(app->window, "separatortoolitem4"), app->pref_main_show_search);
		utils_widget_show_hide(lookup_widget(app->window, "entry_goto_line"), app->pref_main_show_goto);
		utils_widget_show_hide(lookup_widget(app->window, "toolbutton25"), app->pref_main_show_goto);
		utils_widget_show_hide(lookup_widget(app->window, "separatortoolitem5"), app->pref_main_show_goto);
		utils_treeviews_showhide();

		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->notebook), app->tab_pos_editor);
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(msgwindow.notebook), app->tab_pos_msgwin);
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->treeview_notebook), app->tab_pos_sidebar);

		// re-colourise all open documents, if tab width or long line settings have changed
		for (i = 0; i < GEANY_MAX_OPEN_FILES; i++)
		{
			if (doc_list[i].is_valid)
			{
				sci_set_tab_width(doc_list[i].sci, app->pref_editor_tab_width);
				sci_set_mark_long_lines(doc_list[i].sci, app->long_line_type,
										app->long_line_column, app->long_line_color);
				sci_set_visible_eols(doc_list[i].sci, app->pref_editor_show_line_endings);
				sci_set_indentionguides(doc_list[i].sci, app->pref_editor_show_indent_guide);
				sci_set_visible_white_spaces(doc_list[i].sci, app->pref_editor_show_white_space);
				sci_set_folding_margin_visible(doc_list[i].sci, app->pref_editor_folding);
				if (! app->pref_editor_folding) document_unfold_all(i);
			}
		}
		old_tab_width = app->pref_editor_tab_width;
		old_long_line_column = app->long_line_column;
		g_free(old_long_line_color);
		old_long_line_color = g_strdup(app->long_line_color);

		// store all settings
		configuration_save();
	}
	gtk_list_store_clear(store);
	gtk_widget_hide(GTK_WIDGET(dialog));
}


gboolean on_prefs_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	gtk_widget_hide(widget);

	return TRUE;
}


void on_prefs_color_choosed(GtkColorButton *widget, gpointer user_data)
{
	GdkColor color;

	switch (GPOINTER_TO_INT(user_data))
	{
		case 1:
		{
			gtk_color_button_get_color(widget, &color);
			app->long_line_color = utils_get_hex_from_color(&color);
			break;
		}
#ifdef HAVE_VTE
		case 2:
		{
			g_free(vc->color_fore);
			vc->color_fore = g_new0(GdkColor, 1);
			gtk_color_button_get_color(widget, vc->color_fore);
			break;
		}
		case 3:
		{
			g_free(vc->color_back);
			vc->color_back = g_new0(GdkColor, 1);
			gtk_color_button_get_color(widget, vc->color_back);
			break;
		}
#endif
	}
}


void on_prefs_font_choosed(GtkFontButton *widget, gpointer user_data)
{
	const gchar *fontbtn = gtk_font_button_get_font_name(widget);
	gint i;

	switch (GPOINTER_TO_INT(user_data))
	{
		case 1:
		{
			if (strcmp(fontbtn, app->tagbar_font) == 0) break;
			g_free(app->tagbar_font);
			app->tagbar_font = g_strdup(fontbtn);
			for (i = 0; i < GEANY_MAX_OPEN_FILES; i++)
			{
				if (doc_list[i].is_valid && GTK_IS_WIDGET(doc_list[i].tag_tree))
					gtk_widget_modify_font(doc_list[i].tag_tree,
						pango_font_description_from_string(app->tagbar_font));
			}
			if (GTK_IS_WIDGET(app->default_tag_tree))
				gtk_widget_modify_font(app->default_tag_tree,
					pango_font_description_from_string(app->tagbar_font));
			gtk_widget_modify_font(lookup_widget(app->window, "entry1"),
				pango_font_description_from_string(app->tagbar_font));
			break;
		}
		case 2:
		{
			if (strcmp(fontbtn, app->msgwin_font) == 0) break;
			g_free(app->msgwin_font);
			app->msgwin_font = g_strdup(fontbtn);
			gtk_widget_modify_font(msgwindow.tree_compiler,
				pango_font_description_from_string(app->msgwin_font));
			gtk_widget_modify_font(msgwindow.tree_msg,
				pango_font_description_from_string(app->msgwin_font));
			gtk_widget_modify_font(msgwindow.tree_status,
				pango_font_description_from_string(app->msgwin_font));
			break;
		}
		case 3:
		{
			utils_set_editor_font(fontbtn);
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



static gboolean on_prefs_tree_view_button_press_event(
						GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gchar *name;

	// discard click events in the tree unless it is a double click
	if (widget == (GtkWidget*)tree && event->type != GDK_2BUTTON_PRESS) return FALSE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 0, &name, -1);
		if (name != NULL)
		{
			GtkWidget *dialog;
			GtkWidget *label;
			gchar *str;

			dialog = gtk_dialog_new_with_buttons(_("Grab key"), GTK_WINDOW(app->prefs_dialog),
					GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
					GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

			str = g_strdup_printf(_("Type the combination of the keys you want to use for \"%s\""), name);
			label = gtk_label_new(str);
			gtk_misc_set_padding(GTK_MISC(label), 5, 10);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

			dialog_label = gtk_label_new("");
			gtk_misc_set_padding(GTK_MISC(dialog_label), 5, 10);
			gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), dialog_label);

			g_signal_connect(G_OBJECT(dialog), "key-press-event", G_CALLBACK(on_keytype_dialog_response), NULL);
			g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(on_dialog_response), NULL);
			g_signal_connect(G_OBJECT(dialog), "close", G_CALLBACK(gtk_widget_destroy), NULL);

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
		gchar *test;
		GtkTreeIter iter;

		// get the index of the shortcut
		idx = strtol(path, &test, 10);
		if (test == path) return;

		gtk_accelerator_parse(new_text, &lkey, &lmods);

		if (find_duplicate(idx, lkey, lmods, new_text)) return;

		// set the values here, because of the above check, setting it in gtk_accelerator_parse would
		// return a wrong key combination if it is duplicate
		keys[idx]->key = lkey;
		keys[idx]->mods = lmods;

		gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store), &iter, path);
		gtk_list_store_set(store, &iter, 1, new_text, -1);

		edited = TRUE;
	}
}


static gboolean on_keytype_dialog_response(GtkWidget *dialog, GdkEventKey *event, gpointer user_data)
{
	gchar *str;

	// ignore numlock key, not necessary but nice
	if (event->state & GDK_MOD2_MASK) event->state -= GDK_MOD2_MASK;

	str = gtk_accelerator_name(event->keyval, event->state);

	gtk_label_set_text(GTK_LABEL(dialog_label), str);
	g_free(str);

	return TRUE;
}


static void on_dialog_response(GtkWidget *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		GtkTreeIter iter;
		guint idx;
		guint lkey;
		GdkModifierType lmods;
		gchar path[3];

		for (idx = 0; idx < GEANY_MAX_KEYS; idx++)
		{
			if (utils_strcmp(dialog_key_name, keys[idx]->label)) break;
		}

		gtk_accelerator_parse(gtk_label_get_text(GTK_LABEL(dialog_label)), &lkey, &lmods);

		if (find_duplicate(idx, lkey, lmods, gtk_label_get_text(GTK_LABEL(dialog_label)))) return;

		// set the values here, because of the above check, setting it in gtk_accelerator_parse would
		// return a wrong key combination if it is duplicate
		keys[idx]->key = lkey;
		keys[idx]->mods = lmods;

		// generate the path, it is exactly the index
		g_snprintf(path, 3, "%d", idx);
		gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store), &iter, path);
		gtk_list_store_set(store, &iter, 1, gtk_label_get_text(GTK_LABEL(dialog_label)), -1);
		g_free(dialog_key_name);
		dialog_key_name = NULL;

		edited = TRUE;
	}
	gtk_widget_destroy(dialog);
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
			dialogs_show_error(
				_("The combination '%s' is already used for \"%s\". Please choose another one."),
				action, keys[i]->label);
			return TRUE;
		}
	}
	return FALSE;
}


