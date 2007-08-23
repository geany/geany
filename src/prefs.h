/*
 *      prefs.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 */


#ifndef GEANY_PREFS_H
#define GEANY_PREFS_H 1

/* Preferences dialog settings.
 * (See also EditorPrefs in editor.h).
 * Remember to increment abi_version in plugindata.h when changing items. */
typedef struct GeanyPrefs
{
	/* general */
	gboolean		load_session;
	gboolean		load_plugins;
	gboolean		save_winpos;
	gboolean		confirm_exit;
	gboolean		beep_on_errors;
	gboolean		suppress_search_dialogs;
	gboolean		suppress_status_messages;
	gboolean		switch_msgwin_pages;
	gboolean		auto_focus;
	gchar			*default_open_path;

	/* interface */
	gboolean		sidebar_symbol_visible;
	gboolean		sidebar_openfiles_visible;
	gchar			*editor_font;
	gchar			*tagbar_font;
	gchar			*msgwin_font;
	gboolean		show_notebook_tabs;
	gint			tab_pos_editor;
	gint			tab_pos_msgwin;
	gint			tab_pos_sidebar;
	gboolean		statusbar_visible;

	/* toolbar */
	gboolean		toolbar_visible;
	gboolean		toolbar_show_search;
	gboolean		toolbar_show_goto;
	gboolean		toolbar_show_undo;
	gboolean		toolbar_show_navigation;
	gboolean		toolbar_show_compile;
	gboolean		toolbar_show_zoom;
	gboolean		toolbar_show_colour;
	gboolean		toolbar_show_fileops;
	gboolean		toolbar_show_quit;
	GtkIconSize		toolbar_icon_size;
	gint			toolbar_icon_style;

	/* files */
	gboolean		tab_order_ltr;
	guint			mru_length;

	/* tools */
	gchar			*tools_browser_cmd;
	gchar			*tools_make_cmd;
	gchar			*tools_term_cmd;
	gchar			*tools_print_cmd;
	gchar			*tools_grep_cmd;
	gchar			*context_action_cmd;

	/* templates */
	gchar			*template_developer;
	gchar			*template_company;
	gchar			*template_mail;
	gchar			*template_initial;
	gchar			*template_version;
}
GeanyPrefs;

extern GeanyPrefs prefs;


void prefs_init_dialog(void);

void prefs_show_dialog(void);

void on_prefs_font_choosed(GtkFontButton *widget, gpointer user_data);

void on_prefs_color_choosed(GtkColorButton *widget, gpointer user_data);

void on_prefs_tools_button_clicked(GtkButton *button, gpointer user_data);

#endif
