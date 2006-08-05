/*
 *      keyfile.c - this file is part of Geany, a fast and lightweight IDE
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

#include "support.h"
#include "keyfile.h"
#include "utils.h"
#include "document.h"
#include "sciwrappers.h"
#include "encodings.h"


static gchar *scribble_text = NULL;
static gchar *session_files[GEANY_SESSION_FILES];
static gint hpan_position;
static gint vpan_position;


static void generate_filetype_extensions(const gchar *output_dir);


void configuration_save()
{
	gint i = 0, j = 0, idx, max;
	gboolean config_exists;
	GKeyFile *config = g_key_file_new();
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	gchar *data;
	gchar *entry = g_malloc(14);
	gchar *fname = g_malloc0(256);
	gchar **recent_files = g_new0(gchar*, app->mru_length + 1);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	config_exists = g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL);

	if (!config_exists)
	{
		gchar *start_comm = g_strdup_printf(_("%s configuration file, edit as you need"), PACKAGE);
		g_key_file_set_comment(config, NULL, NULL, start_comm, NULL);
		g_free(start_comm);
	}

	// gets the text from the scribble textview
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lookup_widget(app->window, "textview_scribble")));
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	scribble_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	// store basic settings
	if (app->pref_main_save_winpos)
	{
		g_key_file_set_integer(config, PACKAGE, "treeview_position",
				gtk_paned_get_position(GTK_PANED(lookup_widget(app->window, "hpaned1"))));
		g_key_file_set_integer(config, PACKAGE, "msgwindow_position",
				gtk_paned_get_position(GTK_PANED(lookup_widget(app->window, "vpaned1"))));
	}

	g_key_file_set_integer(config, PACKAGE, "mru_length", app->mru_length);
	g_key_file_set_integer(config, PACKAGE, "long_line_type", app->long_line_type);
	g_key_file_set_integer(config, PACKAGE, "tab_pos_editor", app->tab_pos_editor);
	g_key_file_set_integer(config, PACKAGE, "tab_pos_msgwin", app->tab_pos_msgwin);
	g_key_file_set_integer(config, PACKAGE, "tab_pos_sidebar", app->tab_pos_sidebar);
	g_key_file_set_integer(config, PACKAGE, "long_line_column", app->long_line_column);
	g_key_file_set_string(config, PACKAGE, "long_line_color", app->long_line_color);
	g_key_file_set_boolean(config, PACKAGE, "beep_on_errors", app->beep_on_errors);
	g_key_file_set_boolean(config, PACKAGE, "sidebar_symbol_visible", app->sidebar_symbol_visible);
	g_key_file_set_boolean(config, PACKAGE, "sidebar_openfiles_visible", app->sidebar_openfiles_visible);
	g_key_file_set_boolean(config, PACKAGE, "sidebar_visible", app->sidebar_visible);
	g_key_file_set_boolean(config, PACKAGE, "msgwindow_visible", app->msgwindow_visible);
	g_key_file_set_boolean(config, PACKAGE, "use_folding", app->pref_editor_folding);
	g_key_file_set_boolean(config, PACKAGE, "use_auto_indention", app->pref_editor_use_auto_indention);
	g_key_file_set_boolean(config, PACKAGE, "use_indicators", app->pref_editor_use_indicators);
	g_key_file_set_boolean(config, PACKAGE, "show_indent_guide", app->pref_editor_show_indent_guide);
	g_key_file_set_boolean(config, PACKAGE, "show_white_space", app->pref_editor_show_white_space);
	g_key_file_set_boolean(config, PACKAGE, "show_markers_margin", app->show_markers_margin);
	g_key_file_set_boolean(config, PACKAGE, "show_linenumber_margin", app->show_linenumber_margin);
	g_key_file_set_boolean(config, PACKAGE, "line_breaking", app->pref_editor_line_breaking);
	g_key_file_set_boolean(config, PACKAGE, "show_line_endings", app->pref_editor_show_line_endings);
	g_key_file_set_boolean(config, PACKAGE, "fullscreen", app->fullscreen);
	g_key_file_set_boolean(config, PACKAGE, "tab_order_ltr", app->tab_order_ltr);
	g_key_file_set_boolean(config, PACKAGE, "brace_match_ltgt", app->brace_match_ltgt);
	g_key_file_set_boolean(config, PACKAGE, "switch_msgwin_pages", app->switch_msgwin_pages);
	g_key_file_set_boolean(config, PACKAGE, "auto_close_xml_tags", app->pref_editor_auto_close_xml_tags);
	g_key_file_set_boolean(config, PACKAGE, "auto_complete_constructs", app->pref_editor_auto_complete_constructs);
#ifdef HAVE_VTE
	g_key_file_set_boolean(config, PACKAGE, "load_vte", app->load_vte);
	g_key_file_set_comment(config, PACKAGE, "terminal_settings",
			_(" VTE settings: FONT;FOREGROUND;BACKGROUND;scrollback;type;scroll on keystroke;scroll on output;follow path of file"), NULL);
	g_key_file_set_string(config, PACKAGE, "terminal_settings", app->terminal_settings);
#endif
	g_key_file_set_string(config, PACKAGE, "editor_font", app->editor_font);
	g_key_file_set_string(config, PACKAGE, "tagbar_font", app->tagbar_font);
	g_key_file_set_string(config, PACKAGE, "msgwin_font", app->msgwin_font);
	g_key_file_set_string(config, PACKAGE, "scribble_text", scribble_text);
	if (app->pref_main_save_winpos && ! app->fullscreen)
	{
		gtk_window_get_position(GTK_WINDOW(app->window), &app->geometry[0], &app->geometry[1]);
		gtk_window_get_size(GTK_WINDOW(app->window), &app->geometry[2], &app->geometry[3]);
		g_key_file_set_integer_list(config, PACKAGE, "geometry", app->geometry, 4);
	}
	g_key_file_set_integer(config, PACKAGE, "pref_editor_tab_width", app->pref_editor_tab_width);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_confirm_exit", app->pref_main_confirm_exit);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_load_session", app->pref_main_load_session);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_save_winpos", app->pref_main_save_winpos);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show", app->toolbar_visible);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_search", app->pref_toolbar_show_search);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_goto", app->pref_toolbar_show_goto);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_zoom", app->pref_toolbar_show_zoom);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_undo", app->pref_toolbar_show_undo);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_compile", app->pref_toolbar_show_compile);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_colour", app->pref_toolbar_show_colour);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_fileops", app->pref_toolbar_show_fileops);
	g_key_file_set_integer(config, PACKAGE, "pref_toolbar_icon_style", app->toolbar_icon_style);
	g_key_file_set_integer(config, PACKAGE, "pref_toolbar_icon_size", app->toolbar_icon_size);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_new_line", app->pref_editor_new_line);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_replace_tabs", app->pref_editor_replace_tabs);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_trail_space", app->pref_editor_trail_space);
	g_key_file_set_string(config, PACKAGE, "pref_editor_default_encoding", encodings[app->pref_editor_default_encoding].charset);
	g_key_file_set_string(config, PACKAGE, "pref_template_developer", app->pref_template_developer);
	g_key_file_set_string(config, PACKAGE, "pref_template_company", app->pref_template_company);
	g_key_file_set_string(config, PACKAGE, "pref_template_mail", app->pref_template_mail);
	g_key_file_set_string(config, PACKAGE, "pref_template_initial", app->pref_template_initial);
	g_key_file_set_string(config, PACKAGE, "pref_template_version", app->pref_template_version);

	// store tools settings
	g_key_file_set_string(config, "tools", "make_cmd", app->tools_make_cmd ? app->tools_make_cmd : "");
	g_key_file_set_string(config, "tools", "term_cmd", app->tools_term_cmd ? app->tools_term_cmd : "");
	g_key_file_set_string(config, "tools", "browser_cmd", app->tools_browser_cmd ? app->tools_browser_cmd : "");
	g_key_file_set_string(config, "tools", "print_cmd", app->tools_print_cmd ? app->tools_print_cmd : "");
	g_key_file_set_string(config, "tools", "grep_cmd", app->tools_grep_cmd? app->tools_grep_cmd: "");

	for (i = 0; i < app->mru_length; i++)
	{
		if (! g_queue_is_empty(app->recent_queue))
		{
			// copy the values, this is necessary when this function is called from the
			// preferences dialog or when quitting is canceled to keep the queue intact
			recent_files[i] = g_strdup(g_queue_peek_nth(app->recent_queue, i));
		}
		else
		{
			recent_files[i] = NULL;
			break;
		}
	}
	// There is a bug in GTK2.6 g_key_file_set_string_list, we must NULL terminate.
	recent_files[app->mru_length] = NULL;
	g_key_file_set_string_list(config, "files", "recent_files",
				(const gchar**)recent_files, app->mru_length);
	g_strfreev(recent_files);

	// store the last 15(or what ever GEANY_SESSION_FILES is set to) filenames, to reopen the next time
	max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
	for(i = 0; (i < max) && (j < GEANY_SESSION_FILES); i++)
	{
		idx = document_get_n_idx(i);
		if (idx >= 0 && doc_list[idx].file_name)
		{
			g_snprintf(entry, 13, "FILE_NAME_%d", j);
			g_snprintf(fname, 255, "%d:%d:%s", sci_get_current_position(doc_list[idx].sci),
				doc_list[idx].file_type->uid, doc_list[idx].file_name);
			g_key_file_set_string(config, "files", entry, fname);
			j++;
		}
	}
	// if open filenames less than GEANY_SESSION_FILES, delete existing saved entries in the list
	for(i = j; i < GEANY_SESSION_FILES; i++)
	{
		g_snprintf(entry, 13, "FILE_NAME_%d", i);
		g_key_file_set_string(config, "files", entry, "");
	}

	// write the file
	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(configfile, data);
	g_free(data);

	g_key_file_free(config);
	g_free(configfile);
	g_free(entry);
	g_free(fname);
	g_free(scribble_text);
}


#define GEANY_GET_SETTING(propertyname, value, default_value) \
	if (g_object_class_find_property( \
		G_OBJECT_GET_CLASS(G_OBJECT(gtk_settings_get_default())), propertyname)) \
			g_object_get(G_OBJECT(gtk_settings_get_default()), propertyname, &value, \
				NULL); \
	else \
		value = default_value;

gboolean configuration_load()
{
	gboolean config_exists;
	guint i, geo_len;
	gint *geo = g_malloc(sizeof(gint) * 4);
	gsize len = 0;
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	gchar *entry = g_malloc(14);
	gchar *tmp_string, *tmp_string2;
	GKeyFile *config = g_key_file_new();
	GError *error = NULL;
	gchar **recent_files;

	config_exists = g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL);

	app->toolbar_visible = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show", TRUE);
	{
		GtkIconSize tb_iconsize;
		GtkToolbarStyle tb_style;
		GEANY_GET_SETTING("gtk-toolbar-style", tb_style, GTK_TOOLBAR_ICONS);
		GEANY_GET_SETTING("gtk-toolbar-icon-size", tb_iconsize,
			GTK_ICON_SIZE_LARGE_TOOLBAR);
		app->toolbar_icon_style = utils_get_setting_integer(config, PACKAGE, "pref_toolbar_icon_style", tb_style);
		app->toolbar_icon_size = utils_get_setting_integer(config, PACKAGE, "pref_toolbar_icon_size", tb_iconsize);
	}
	app->beep_on_errors = utils_get_setting_boolean(config, PACKAGE, "beep_on_errors", TRUE);
	app->mru_length = utils_get_setting_integer(config, PACKAGE, "mru_length", GEANY_DEFAULT_MRU_LENGHTH);
	app->long_line_type = utils_get_setting_integer(config, PACKAGE, "long_line_type", 0);
	app->long_line_color = utils_get_setting_string(config, PACKAGE, "long_line_color", "#C2EBC2");
	app->long_line_column = utils_get_setting_integer(config, PACKAGE, "long_line_column", 72);
	app->tab_pos_editor = utils_get_setting_integer(config, PACKAGE, "tab_pos_editor", GTK_POS_TOP);
	app->tab_pos_msgwin = utils_get_setting_integer(config, PACKAGE, "tab_pos_msgwin",GTK_POS_LEFT);
	app->tab_pos_sidebar = utils_get_setting_integer(config, PACKAGE, "tab_pos_sidebar", GTK_POS_TOP);
	app->sidebar_symbol_visible = utils_get_setting_boolean(config, PACKAGE, "sidebar_symbol_visible", TRUE);
	app->sidebar_openfiles_visible = utils_get_setting_boolean(config, PACKAGE, "sidebar_openfiles_visible", TRUE);
	app->sidebar_visible = utils_get_setting_boolean(config, PACKAGE, "sidebar_visible", TRUE);
	app->msgwindow_visible = utils_get_setting_boolean(config, PACKAGE, "msgwindow_visible", TRUE);
	app->pref_editor_line_breaking = utils_get_setting_boolean(config, PACKAGE, "line_breaking", FALSE); //default is off for better performance
	app->pref_editor_use_auto_indention = utils_get_setting_boolean(config, PACKAGE, "use_auto_indention", TRUE);
	app->pref_editor_use_indicators = utils_get_setting_boolean(config, PACKAGE, "use_indicators", TRUE);
	app->pref_editor_show_indent_guide = utils_get_setting_boolean(config, PACKAGE, "show_indent_guide", FALSE);
	app->pref_editor_show_white_space = utils_get_setting_boolean(config, PACKAGE, "show_white_space", FALSE);
	app->pref_editor_show_line_endings = utils_get_setting_boolean(config, PACKAGE, "show_line_endings", FALSE);
	app->pref_editor_auto_close_xml_tags = utils_get_setting_boolean(config, PACKAGE, "auto_close_xml_tags", TRUE);
	app->pref_editor_auto_complete_constructs = utils_get_setting_boolean(config, PACKAGE, "auto_complete_constructs", TRUE);
	app->pref_editor_folding = utils_get_setting_boolean(config, PACKAGE, "use_folding", TRUE);
	app->show_markers_margin = utils_get_setting_boolean(config, PACKAGE, "show_markers_margin", TRUE);
	app->show_linenumber_margin = utils_get_setting_boolean(config, PACKAGE, "show_linenumber_margin", TRUE);
	app->fullscreen = utils_get_setting_boolean(config, PACKAGE, "fullscreen", FALSE);
	app->tab_order_ltr = utils_get_setting_boolean(config, PACKAGE, "tab_order_ltr", FALSE);
	app->brace_match_ltgt = utils_get_setting_boolean(config, PACKAGE, "brace_match_ltgt", FALSE);
	app->switch_msgwin_pages = utils_get_setting_boolean(config, PACKAGE, "switch_msgwin_pages", FALSE);
	app->editor_font = utils_get_setting_string(config, PACKAGE, "editor_font", GEANY_DEFAULT_FONT_EDITOR);
	app->tagbar_font = utils_get_setting_string(config, PACKAGE, "tagbar_font", GEANY_DEFAULT_FONT_SYMBOL_LIST);
	app->msgwin_font = utils_get_setting_string(config, PACKAGE, "msgwin_font", GEANY_DEFAULT_FONT_MSG_WINDOW);
	scribble_text = utils_get_setting_string(config, PACKAGE, "scribble_text",
				_("Type here what you want, use it as a notice/scratch board"));

	geo = g_key_file_get_integer_list(config, PACKAGE, "geometry", &geo_len, &error);
	if (error)
	{
		app->geometry[0] = -1;
		g_error_free(error);
		error = NULL;
	}
	else
	{
		app->geometry[0] = geo[0];
		app->geometry[1] = geo[1];
		app->geometry[2] = geo[2];
		app->geometry[3] = geo[3];
	}
	hpan_position = utils_get_setting_integer(config, PACKAGE, "treeview_position", 156);
	vpan_position = utils_get_setting_integer(config, PACKAGE, "msgwindow_position", (geo) ?
				(GEANY_MSGWIN_HEIGHT + geo[3] - 440) :
				(GEANY_MSGWIN_HEIGHT + GEANY_WINDOW_DEFAULT_HEIGHT - 440));


	app->pref_editor_tab_width = utils_get_setting_integer(config, PACKAGE, "pref_editor_tab_width", 4);
	tmp_string = utils_get_setting_string(config, PACKAGE, "pref_editor_default_encoding",
											encodings[GEANY_ENCODING_UTF_8].charset);
	if (tmp_string)
	{
		const GeanyEncoding *enc = encodings_get_from_charset(tmp_string);
		if (enc != NULL)
			app->pref_editor_default_encoding = enc->idx;
		else
			app->pref_editor_default_encoding = GEANY_ENCODING_UTF_8;

		g_free(tmp_string);
	}
	app->pref_main_confirm_exit = utils_get_setting_boolean(config, PACKAGE, "pref_main_confirm_exit", TRUE);
	app->pref_main_load_session = utils_get_setting_boolean(config, PACKAGE, "pref_main_load_session", TRUE);
	app->pref_main_save_winpos = utils_get_setting_boolean(config, PACKAGE, "pref_main_save_winpos", TRUE);
	app->pref_toolbar_show_search = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_search", TRUE);
	app->pref_toolbar_show_goto = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_goto", TRUE);
	app->pref_toolbar_show_zoom = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_zoom", FALSE);
	app->pref_toolbar_show_compile = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_compile", TRUE);
	app->pref_toolbar_show_undo = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_undo", FALSE);
	app->pref_toolbar_show_colour = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_colour", TRUE);
	app->pref_toolbar_show_fileops = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_fileops", TRUE);
#ifdef HAVE_VTE
	app->load_vte = utils_get_setting_boolean(config, PACKAGE, "load_vte", TRUE);
	app->terminal_settings = utils_get_setting_string(config, PACKAGE, "terminal_settings",	"");
#endif
	app->pref_template_developer = utils_get_setting_string(config, PACKAGE, "pref_template_developer", g_get_real_name());
	app->pref_template_company = utils_get_setting_string(config, PACKAGE, "pref_template_company", "");
	tmp_string = utils_get_initials(app->pref_template_developer);
	app->pref_template_initial = utils_get_setting_string(config, PACKAGE, "pref_template_initial", tmp_string);
	g_free(tmp_string);

	app->pref_template_version = utils_get_setting_string(config, PACKAGE, "pref_template_version", "1.0");

	tmp_string2 = utils_get_hostname();
	tmp_string = g_strdup_printf("%s@%s", g_get_user_name(), tmp_string2);
	app->pref_template_mail = utils_get_setting_string(config, PACKAGE, "pref_template_mail", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	app->pref_editor_replace_tabs = utils_get_setting_boolean(config, PACKAGE, "pref_editor_replace_tabs", FALSE);
	app->pref_editor_new_line = utils_get_setting_boolean(config, PACKAGE, "pref_editor_new_line", TRUE);
	app->pref_editor_trail_space = utils_get_setting_boolean(config, PACKAGE, "pref_editor_trail_space", FALSE);

	tmp_string = g_find_program_in_path(GEANY_DEFAULT_TOOLS_MAKE);
	app->tools_make_cmd = utils_get_setting_string(config, "tools", "make_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string = g_find_program_in_path(GEANY_DEFAULT_TOOLS_TERMINAL);
	app->tools_term_cmd = utils_get_setting_string(config, "tools", "term_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string = g_find_program_in_path(GEANY_DEFAULT_TOOLS_BROWSER);
	app->tools_browser_cmd = utils_get_setting_string(config, "tools", "browser_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string2 = g_find_program_in_path(GEANY_DEFAULT_TOOLS_PRINTCMD);
	tmp_string = g_strconcat(tmp_string2, " %f", NULL);
	app->tools_print_cmd = utils_get_setting_string(config, "tools", "print_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	tmp_string = g_find_program_in_path(GEANY_DEFAULT_TOOLS_GREP);
	app->tools_grep_cmd = utils_get_setting_string(config, "tools", "grep_cmd", tmp_string);
	g_free(tmp_string);

	recent_files = g_key_file_get_string_list(config, "files", "recent_files", &len, NULL);
	if (recent_files != NULL)
	{
		for (i = 0; (i < len) && (i < app->mru_length); i++)
		{
			gchar *filename = g_strdup(recent_files[i]);
			g_queue_push_tail(app->recent_queue, filename);
		}
	}
	g_strfreev(recent_files);

	for(i = 0; i < GEANY_SESSION_FILES; i++)
	{
		g_snprintf(entry, 13, "FILE_NAME_%d", i);
		session_files[i] = g_key_file_get_string(config, "files", entry, &error);
		if (! session_files[i] || error)
		{
			g_error_free(error);
			error = NULL;
			session_files[i] = NULL;
		}
	}

	g_key_file_free(config);
	g_free(configfile);
	g_free(entry);
	g_free(geo);
	return TRUE;
}


gboolean configuration_open_files()
{
	gint i;
	guint x, pos, y, len;
	gchar *file, *locale_filename, **array;
	gboolean ret = FALSE;

	i = app->tab_order_ltr ? 0 : GEANY_SESSION_FILES - 1;
	while(TRUE)
	{
		if (session_files[i] && *session_files[i])
		{
			gint uid = -1;
			x = 0;
			y = 0;

			// yes it is :, it should be a ;, but now it is too late to change it
			array = g_strsplit(session_files[i], ":", 3);
			len = g_strv_length(array);

			// read position
			if (len > 0 && array[0]) pos = atoi(array[0]);
			else pos = 0;

			// read filetype (only if there are more than two fields, otherwise we have the old format)
			if (len > 2 && array[1])
			{
				uid = atoi(array[1]);
				file = array[2];
			}
			else file = array[1];

			// try to get the locale equivalent for the filename, fallback to filename if error
			locale_filename = g_locale_from_utf8(file, -1, NULL, NULL, NULL);
			if (locale_filename == NULL) locale_filename = g_strdup(file);

			if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK))
			{
				filetype *ft = filetypes_get_from_uid(uid);
				document_open_file(-1, locale_filename, pos, FALSE, ft, NULL);
				ret = TRUE;
			}
			g_free(locale_filename);
		}
		g_free(session_files[i]);

		if (app->tab_order_ltr)
		{
			i++;
			if (i >= GEANY_SESSION_FILES) break;
		}
		else
		{
			i--;
			if (i < 0) break;
		}
	}

	return ret;
}


/* set some settings which are already read from the config file, but need other things, like the
 * realisation of the main window */
void configuration_apply_settings()
{
	if (scribble_text)
	{	// update the scribble widget, because now it's realized
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(lookup_widget(app->window, "textview_scribble"))),
				scribble_text, -1);
	}
	g_free(scribble_text);

	// set the position of the hpaned and vpaned
	if (app->pref_main_save_winpos)
	{
		gtk_paned_set_position(GTK_PANED(lookup_widget(app->window, "hpaned1")), hpan_position);
		gtk_paned_set_position(GTK_PANED(lookup_widget(app->window, "vpaned1")), vpan_position);
	}

	// now the scintilla widget pages may need scrolling in view
	if (app->pref_main_load_session)
	{
		gint idx;
		guint tabnum = 0;

		while (tabnum < gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)))
		{
			idx = document_get_n_idx(tabnum);
			if (idx < 0) break;
			sci_scroll_caret(doc_list[idx].sci);
			tabnum++;
		}
	}

	// set fullscreen after initial draw so that returning to normal view is the right size.
	// fullscreen mode is disabled by default, so act only if it is true
	if (app->fullscreen)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_fullscreen1")), TRUE);
		app->fullscreen = TRUE;
		utils_set_fullscreen();
	}
}


/* Generate the config files in "data/" from defaults */
void configuration_generate_data_files()
{
	gchar *cur_dir, *gen_dir;

	cur_dir = g_get_current_dir();
	gen_dir = g_strconcat(cur_dir, G_DIR_SEPARATOR_S, "data", NULL);
	g_free(cur_dir);

	if (! g_file_test(gen_dir, G_FILE_TEST_IS_DIR))
	{
		g_print("%s does not exist!\n", gen_dir);
		return;
	}
	g_print("Generating system files in %s:\n", gen_dir);
	generate_filetype_extensions(gen_dir);
	g_free(gen_dir);
}


/* This will write the default settings for the system filetype_extensions.conf */
static void generate_filetype_extensions(const gchar *output_dir)
{
	guint i;
	gchar *configfile = g_strconcat(output_dir, G_DIR_SEPARATOR_S, "filetype_extensions.conf", NULL);
	gchar *data, *basename;
	GKeyFile *config;

	config = g_key_file_new();
	// add filetype keys
	for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		g_key_file_set_string_list(config, "Extensions", filetypes[i]->name,
			(const gchar**) filetypes[i]->pattern, g_strv_length(filetypes[i]->pattern));
	}
	// add comment
	g_key_file_set_comment(config, "Extensions", NULL,
		"Filetype extension configuration file for Geany\n"
		"Insert as many items as you want, seperate them with a \";\".\n"
		"See Geany's main documentation for details.", NULL);

	// write the file
	g_print("%s: ", __func__);
	data = g_key_file_to_data(config, NULL, NULL);
	basename = g_path_get_basename(configfile);

	if (utils_write_file(configfile, data) == 0)
		g_print("wrote file %s.\n", basename);
	else
		g_print("*** ERROR: error writing file %s\n", basename);
	g_free(basename);

	g_free(data);
	g_key_file_free(config);
}


void configuration_read_filetype_extensions()
{
	guint i;
	gsize len = 0;
	gchar *sysconfigfile = g_strconcat(app->datadir, G_DIR_SEPARATOR_S,
		"filetype_extensions.conf", NULL);
	gchar *userconfigfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S,
		"filetype_extensions.conf", NULL);
	gchar **list;
	GKeyFile *sysconfig = g_key_file_new();
	GKeyFile *userconfig = g_key_file_new();

	g_key_file_load_from_file(sysconfig, sysconfigfile, G_KEY_FILE_NONE, NULL);
	g_key_file_load_from_file(userconfig, userconfigfile, G_KEY_FILE_NONE, NULL);

	// read the keys
	for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		gboolean userset =
			g_key_file_has_key(userconfig, "Extensions", filetypes[i]->name, NULL);
		list = g_key_file_get_string_list(
			(userset) ? userconfig : sysconfig, "Extensions", filetypes[i]->name, &len, NULL);
		if (list && len > 0)
		{
			g_strfreev(filetypes[i]->pattern);
			filetypes[i]->pattern = list;
		}
		else g_strfreev(list);
	}

	g_key_file_free(sysconfig);
	g_key_file_free(userconfig);
}
