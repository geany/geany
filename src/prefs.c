/*
 *      prefs.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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


#include "geany.h"

#include "prefs.h"
#include "support.h"
#include "utils.h"
#include "msgwindow.h"
#include "sciwrappers.h"
#ifdef HAVE_VTE
# include "vte.h"
#endif


gint old_tab_width;
gint old_long_line_column;
gchar *old_long_line_color;


void prefs_init_dialog(void)
{
	GtkWidget *widget;
	GdkColor *color;

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

	widget = lookup_widget(app->prefs_dialog, "check_toolbar_search");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_main_show_search);

	widget = lookup_widget(app->prefs_dialog, "check_list_symbol");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->treeview_symbol_visible);

	widget = lookup_widget(app->prefs_dialog, "check_list_openfiles");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->treeview_openfiles_visible);

	widget = lookup_widget(app->prefs_dialog, "tagbar_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), app->tagbar_font);

	widget = lookup_widget(app->prefs_dialog, "msgwin_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), app->msgwin_font);


	// Editor settings
	widget = lookup_widget(app->prefs_dialog, "spin_tab_width");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), app->pref_editor_tab_width);
	old_tab_width = app->pref_editor_tab_width;

	widget = lookup_widget(app->prefs_dialog, "check_trailing_spaces");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_trail_space);

	widget = lookup_widget(app->prefs_dialog, "check_new_line");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), app->pref_editor_new_line);

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

	widget = lookup_widget(app->prefs_dialog, "editor_font");
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(widget), app->editor_font);

	widget = lookup_widget(app->prefs_dialog, "spin_long_line");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), app->long_line_column);
	old_long_line_column = app->long_line_column;

	old_long_line_color = g_strdup(app->long_line_color);

	color = g_new(GdkColor, 1);
	gdk_color_parse(app->long_line_color, color);
	widget = lookup_widget(app->prefs_dialog, "long_line_color");
	gtk_color_button_set_color(GTK_COLOR_BUTTON(widget), color);
	g_free(color);

	// Tools Settings
	if (app->build_c_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_c")), app->build_c_cmd);

	if (app->build_cpp_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_cpp")), app->build_cpp_cmd);

	if (app->build_java_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_java")), app->build_java_cmd);

	if (app->build_javac_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_javac")), app->build_javac_cmd);

	if (app->build_fpc_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_pascal")), app->build_fpc_cmd);

	if (app->build_make_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_make")), app->build_make_cmd);

	if (app->build_term_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_com_term")), app->build_term_cmd);

	if (app->build_browser_cmd)
		gtk_entry_set_text(GTK_ENTRY(lookup_widget(app->prefs_dialog, "entry_browser")), app->build_browser_cmd);


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


#ifdef HAVE_VTE
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

		gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "font_term"), TRUE);
		gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "color_fore"), TRUE);
		gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "color_back"), TRUE);
		gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "spin_scrollback"), TRUE);
		gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "entry_emulation"), TRUE);
		gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "check_scroll_key"), TRUE);
		gtk_widget_set_sensitive(lookup_widget(app->prefs_dialog, "check_scroll_out"), TRUE);
	}
#endif
}



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

		widget = lookup_widget(app->prefs_dialog, "check_toolbar_search");
		app->pref_main_show_search = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_list_symbol");
		app->treeview_symbol_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_list_openfiles");
		app->treeview_openfiles_visible = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));


		// Editor settings
		widget = lookup_widget(app->prefs_dialog, "spin_tab_width");
		app->pref_editor_tab_width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_trailing_spaces");
		app->pref_editor_trail_space = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "check_new_line");
		app->pref_editor_new_line = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

		widget = lookup_widget(app->prefs_dialog, "spin_long_line");
		app->long_line_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));


		// Tools Settings
		widget = lookup_widget(app->prefs_dialog, "entry_com_c");
		g_free(app->build_c_cmd);
		app->build_c_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_com_cpp");
		g_free(app->build_cpp_cmd);
		app->build_cpp_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_com_java");
		g_free(app->build_java_cmd);
		app->build_java_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_com_javac");
		g_free(app->build_javac_cmd);
		app->build_javac_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_com_pascal");
		g_free(app->build_fpc_cmd);
		app->build_fpc_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_com_make");
		g_free(app->build_make_cmd);
		app->build_make_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_com_term");
		g_free(app->build_term_cmd);
		app->build_term_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

		widget = lookup_widget(app->prefs_dialog, "entry_browser");
		g_free(app->build_browser_cmd);
		app->build_browser_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));

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


#ifdef HAVE_VTE
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

			g_free(app->terminal_settings);
			hex_color_fore = utils_get_hex_from_color(vc->color_fore);
			hex_color_back = utils_get_hex_from_color(vc->color_back);
			app->terminal_settings = g_strdup_printf("%s;%s;%s;%d;%s;%s;%s", vc->font,
					hex_color_fore, hex_color_back,
					vc->scrollback_lines, vc->emulation,
					utils_btoa(vc->scroll_on_key), utils_btoa(vc->scroll_on_out));

			vte_apply_user_settings();
			g_free(hex_color_fore);
			g_free(hex_color_back);
		}
#endif

		// apply the changes made
		utils_widget_show_hide(lookup_widget(app->window, "entry1"), app->pref_main_show_search);
		utils_widget_show_hide(lookup_widget(app->window, "toolbutton18"), app->pref_main_show_search);
		utils_widget_show_hide(lookup_widget(app->window, "separatortoolitem4"), app->pref_main_show_search);
		utils_treeviews_showhide();
		for (i = 0; i < GEANY_MAX_OPEN_FILES; i++)
		{
			if (doc_list[i].is_valid && GTK_IS_WIDGET(doc_list[i].tag_tree))
				gtk_widget_modify_font(doc_list[i].tag_tree, pango_font_description_from_string(app->tagbar_font));
		}
		if (GTK_IS_WIDGET(app->default_tag_tree))
		gtk_widget_modify_font(app->default_tag_tree, pango_font_description_from_string(app->tagbar_font));
		gtk_widget_modify_font(lookup_widget(app->window, "entry1"), pango_font_description_from_string(app->tagbar_font));
		gtk_widget_modify_font(msgwindow.tree_compiler, pango_font_description_from_string(app->msgwin_font));
		gtk_widget_modify_font(msgwindow.tree_msg, pango_font_description_from_string(app->msgwin_font));
		gtk_widget_modify_font(msgwindow.tree_status, pango_font_description_from_string(app->msgwin_font));

		utils_set_font();

		// re-colorize all open documents, if tab width or long line settings have changed
/*		if ((app->pref_editor_tab_width != old_tab_width) ||
			(! utils_strcmp(app->long_line_color, old_long_line_color)) ||
			(app->long_line_column != old_long_line_column))
		{
			gint i;
*/			for (i = 0; i < GEANY_MAX_OPEN_FILES; i++)
			{
				if (doc_list[i].is_valid)
				{
					sci_set_tab_width(doc_list[i].sci, app->pref_editor_tab_width);
					sci_set_mark_long_lines(doc_list[i].sci, app->long_line_column, app->long_line_color);
					sci_set_visible_eols(doc_list[i].sci, app->pref_editor_show_line_endings);
					sci_set_indentionguides(doc_list[i].sci, app->pref_editor_show_indent_guide);
					sci_set_visible_white_spaces(doc_list[i].sci, app->pref_editor_show_white_space);
				}
//			}
			old_tab_width = app->pref_editor_tab_width;
			old_long_line_column = app->long_line_column;
			g_free(old_long_line_color);
			old_long_line_color = g_strdup(app->long_line_color);
		}

	}
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
			vc->color_fore = g_new(GdkColor, 1);
			gtk_color_button_get_color(widget, vc->color_fore);
			break;
		}
		case 3:
		{
			g_free(vc->color_back);
			vc->color_back = g_new(GdkColor, 1);
			gtk_color_button_get_color(widget, vc->color_back);
			break;
		}
#endif
	}
}


void on_prefs_font_choosed(GtkFontButton *widget, gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data))
	{
		case 1:
		{
			g_free(app->tagbar_font);
			app->tagbar_font = g_strdup(gtk_font_button_get_font_name(widget));
			break;
		}
		case 2:
		{
			g_free(app->msgwin_font);
			app->msgwin_font = g_strdup(gtk_font_button_get_font_name(widget));
			break;
		}
		case 3:
		{
			g_free(app->editor_font);
			app->editor_font = g_strdup(gtk_font_button_get_font_name(widget));
			break;
		}
#ifdef HAVE_VTE
		case 4:
		{
			// VTE settings
			g_free(vc->font);
			vc->font = g_strdup(gtk_font_button_get_font_name(widget));
			break;
		}
#endif
	}
}

