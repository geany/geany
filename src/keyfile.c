/*
 *      keyfile.c - this file is part of Geany, a fast and lightweight IDE
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

#include <stdlib.h>
#include <string.h>

#include "geany.h"

#include "support.h"
#include "keyfile.h"
#include "utils.h"
#include "document.h"
#include "sciwrappers.h"


static gchar *scribble_text = NULL;
static gchar *session_files[GEANY_SESSION_FILES];
static gint hpan_position;
static gint vpan_position;


void configuration_save(void)
{
	gint i = 0, j = 0, idx, max;
	gboolean config_exists;
	GKeyFile *config = g_key_file_new();
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	gchar *entry = g_malloc(14);
	gchar *fname = g_malloc0(256);
	gchar **recent_files = g_new(gchar*, app->mru_length);
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	config_exists = g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL);

	if (!config_exists)
	{
		gchar *start_comm = g_malloc(100);
		g_snprintf(start_comm, 100, _("%s configuration file, edit as you need"), PACKAGE);
		g_key_file_set_comment(config, NULL, NULL, start_comm, NULL);
		g_free(start_comm);
	}

	// gets the text from the scribble textview
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lookup_widget(app->window, "textview_scribble")));
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	scribble_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	// store basic settings
	g_key_file_set_boolean(config, PACKAGE, "toolbar_visible", app->toolbar_visible);
	g_key_file_set_integer(config, PACKAGE, "toolbar_icon_style", app->toolbar_icon_style);
	if (app->pref_main_save_winpos)
	{
		g_key_file_set_integer(config, PACKAGE, "treeview_position",
				gtk_paned_get_position(GTK_PANED(lookup_widget(app->window, "hpaned1"))));
		g_key_file_set_integer(config, PACKAGE, "msgwindow_position",
				gtk_paned_get_position(GTK_PANED(lookup_widget(app->window, "vpaned1"))));
	}

	g_key_file_set_integer(config, PACKAGE, "mru_length", app->mru_length);
	g_key_file_set_integer(config, PACKAGE, "long_line_column", app->long_line_column);
	g_key_file_set_string(config, PACKAGE, "long_line_color", app->long_line_color);
	g_key_file_set_boolean(config, PACKAGE, "beep_on_errors", app->beep_on_errors);
	g_key_file_set_boolean(config, PACKAGE, "treeview_symbol_visible", app->treeview_symbol_visible);
	g_key_file_set_boolean(config, PACKAGE, "treeview_openfiles_visible", app->treeview_openfiles_visible);
	g_key_file_set_boolean(config, PACKAGE, "msgwindow_visible", app->msgwindow_visible);
	g_key_file_set_boolean(config, PACKAGE, "use_auto_indention", app->use_auto_indention);
	g_key_file_set_boolean(config, PACKAGE, "show_indent_guide", app->show_indent_guide);
	g_key_file_set_boolean(config, PACKAGE, "show_white_space", app->show_white_space);
	g_key_file_set_boolean(config, PACKAGE, "show_markers_margin", app->show_markers_margin);
	//g_key_file_set_boolean(config, PACKAGE, "line_breaking", app->line_breaking);
	g_key_file_set_boolean(config, PACKAGE, "show_line_endings", app->show_line_endings);
	g_key_file_set_boolean(config, PACKAGE, "fullscreen", app->fullscreen);
	g_key_file_set_boolean(config, PACKAGE, "switch_msgwin_pages", app->switch_msgwin_pages);
	g_key_file_set_boolean(config, PACKAGE, "auto_close_xml_tags", app->auto_close_xml_tags);
	g_key_file_set_boolean(config, PACKAGE, "auto_complete_constructs", app->auto_complete_constructs);
#ifdef HAVE_VTE
	g_key_file_set_comment(config, PACKAGE, "terminal_settings",
			_(" VTE settings: FONT;FOREGROUND;BACKGROUND;scrollback;type;scroll on keystroke;scroll on output"), NULL);
	g_key_file_set_string(config, PACKAGE, "terminal_settings", app->terminal_settings);
#endif
	g_key_file_set_string(config, PACKAGE, "editor_font", app->editor_font);
	g_key_file_set_string(config, PACKAGE, "tagbar_font", app->tagbar_font);
	g_key_file_set_string(config, PACKAGE, "msgwin_font", app->msgwin_font);
	g_key_file_set_string(config, PACKAGE, "scribble_text", scribble_text);
	if (app->pref_main_save_winpos)
	{
		gtk_window_get_position(GTK_WINDOW(app->window), &app->geometry[0], &app->geometry[1]);
		gtk_window_get_size(GTK_WINDOW(app->window), &app->geometry[2], &app->geometry[3]);
		g_key_file_set_integer_list(config, PACKAGE, "geometry", app->geometry, 4);
	}
	g_key_file_set_integer(config, PACKAGE, "pref_editor_tab_width", app->pref_editor_tab_width);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_confirm_exit", app->pref_main_confirm_exit);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_load_session", app->pref_main_load_session);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_save_winpos", app->pref_main_save_winpos);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_show_search", app->pref_main_show_search);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_new_line", app->pref_editor_new_line);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_trail_space", app->pref_editor_trail_space);
	g_key_file_set_string(config, PACKAGE, "pref_template_developer", app->pref_template_developer);
	g_key_file_set_string(config, PACKAGE, "pref_template_company", app->pref_template_company);
	g_key_file_set_string(config, PACKAGE, "pref_template_mail", app->pref_template_mail);
	g_key_file_set_string(config, PACKAGE, "pref_template_initial", app->pref_template_initial);
	g_key_file_set_string(config, PACKAGE, "pref_template_version", app->pref_template_version);

	// store build settings
	g_key_file_set_string(config, "build", "build_c_cmd", app->build_c_cmd ? app->build_c_cmd : "");
	g_key_file_set_string(config, "build", "build_cpp_cmd", app->build_cpp_cmd ? app->build_cpp_cmd : "");
	g_key_file_set_string(config, "build", "build_java_cmd", app->build_java_cmd ? app->build_java_cmd : "");
	g_key_file_set_string(config, "build", "build_javac_cmd", app->build_javac_cmd ? app->build_javac_cmd : "");
	g_key_file_set_string(config, "build", "build_fpc_cmd", app->build_fpc_cmd ? app->build_fpc_cmd : "");
	g_key_file_set_string(config, "build", "build_tex_dvi_cmd", app->build_tex_dvi_cmd ? app->build_tex_dvi_cmd : "");
	g_key_file_set_string(config, "build", "build_tex_pdf_cmd", app->build_tex_pdf_cmd ? app->build_tex_pdf_cmd : "");
	g_key_file_set_string(config, "build", "build_tex_view_dvi_cmd", app->build_tex_view_dvi_cmd ? app->build_tex_view_dvi_cmd : "");
	g_key_file_set_string(config, "build", "build_tex_view_pdf_cmd", app->build_tex_view_pdf_cmd ? app->build_tex_view_pdf_cmd : "");
	g_key_file_set_string(config, "build", "build_make_cmd", app->build_make_cmd ? app->build_make_cmd : "");
	g_key_file_set_string(config, "build", "build_term_cmd", app->build_term_cmd ? app->build_term_cmd : "");
	g_key_file_set_string(config, "build", "build_browser_cmd", app->build_browser_cmd ? app->build_browser_cmd : "");

	for (i = 0; i < app->mru_length; i++)
	{
		if (! g_queue_is_empty(app->recent_queue))
		{
			recent_files[i] = g_queue_pop_head(app->recent_queue);
		}
		else
		{
			recent_files[i] = NULL;
		}

	}
	g_key_file_set_string_list(config, "files", "recent_files",
				(const gchar**)recent_files, app->mru_length);

	// store the last 15(or what ever GEANY_SESSION_FILES is set to) filenames, to reopen the next time
	max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
	for(i = 0; (i < max) && (j < GEANY_SESSION_FILES); i++)
	{
		idx = document_get_n_idx(i);
		if (idx >= 0 && doc_list[idx].file_name)
		{
			g_snprintf(entry, 13, "FILE_NAME_%d", j);
			g_snprintf(fname, 255, "%d:%s", sci_get_current_position(doc_list[idx].sci), doc_list[idx].file_name);
			g_key_file_set_string(config, "files", entry, fname);
			j++;
		}
	}
	// if open tabs less than GEANY_SESSION_FILES, delete existing saved entries in the list
	if (max < GEANY_SESSION_FILES)
	{
		for(i = max; i < GEANY_SESSION_FILES; i++)
		{
			g_snprintf(entry, 13, "FILE_NAME_%d", i);
			g_key_file_set_string(config, "files", entry, "");
		}
	}

	// write the file
	utils_write_file(configfile, g_key_file_to_data(config, NULL, NULL));

	utils_free_ptr_array(recent_files, app->mru_length);
	g_free(recent_files);
	g_key_file_free(config);
	g_free(configfile);
	g_free(entry);
	g_free(fname);
	g_free(scribble_text);
}


gboolean configuration_load(void)
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

	config_exists = g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL);

	app->toolbar_visible = utils_get_setting_boolean(config, PACKAGE, "toolbar_visible", TRUE);
	app->beep_on_errors = utils_get_setting_boolean(config, PACKAGE, "beep_on_errors", TRUE);
	app->mru_length = utils_get_setting_integer(config, PACKAGE, "mru_length", 10);
	app->toolbar_icon_style = utils_get_setting_integer(config, PACKAGE, "toolbar_icon_style", GTK_TOOLBAR_ICONS);
	app->toolbar_icon_size = utils_get_setting_integer(config, PACKAGE, "toolbar_icon_size", 2);
	app->long_line_color = utils_get_setting_string(config, PACKAGE, "long_line_color", "#C2EBC2");
	app->long_line_column = utils_get_setting_integer(config, PACKAGE, "long_line_column", 72);
	app->treeview_symbol_visible = utils_get_setting_boolean(config, PACKAGE, "treeview_symbol_visible", TRUE);
	app->treeview_openfiles_visible = utils_get_setting_boolean(config, PACKAGE, "treeview_openfiles_visible", TRUE);
	app->msgwindow_visible = utils_get_setting_boolean(config, PACKAGE, "msgwindow_visible", TRUE);
	app->use_auto_indention = utils_get_setting_boolean(config, PACKAGE, "use_auto_indention", TRUE);
	app->show_indent_guide = utils_get_setting_boolean(config, PACKAGE, "show_indent_guide", FALSE);
	app->show_white_space = utils_get_setting_boolean(config, PACKAGE, "show_white_space", FALSE);
	app->show_markers_margin = utils_get_setting_boolean(config, PACKAGE, "show_markers_margin", TRUE);
	app->show_line_endings = utils_get_setting_boolean(config, PACKAGE, "show_line_endings", FALSE);
	//app->line_breaking = utils_get_setting_boolean(config, PACKAGE, "line_breaking", TRUE);
	app->fullscreen = utils_get_setting_boolean(config, PACKAGE, "fullscreen", FALSE);
	app->switch_msgwin_pages = utils_get_setting_boolean(config, PACKAGE, "switch_msgwin_pages", TRUE);
	app->auto_close_xml_tags = utils_get_setting_boolean(config, PACKAGE, "auto_close_xml_tags", TRUE);
	app->auto_complete_constructs = utils_get_setting_boolean(config, PACKAGE, "auto_complete_constructs", TRUE);
	app->editor_font = utils_get_setting_string(config, PACKAGE, "editor_font", "Courier New 9");
	app->tagbar_font = utils_get_setting_string(config, PACKAGE, "tagbar_font", "Cursor 8");
	app->msgwin_font = utils_get_setting_string(config, PACKAGE, "msgwin_font", "Cursor 8");
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
	hpan_position = utils_get_setting_integer(config, PACKAGE, "treeview_position", -1);
	vpan_position = utils_get_setting_integer(config, PACKAGE, "msgwindow_position",
				(geo) ? (geo[3] - GEANY_MSGWIN_HEIGHT) : -1);


	app->pref_editor_tab_width = utils_get_setting_integer(config, PACKAGE, "pref_editor_tab_width", 4);
	app->pref_main_confirm_exit = utils_get_setting_boolean(config, PACKAGE, "pref_main_confirm_exit", TRUE);
	app->pref_main_load_session = utils_get_setting_boolean(config, PACKAGE, "pref_main_load_session", TRUE);
	app->pref_main_save_winpos = utils_get_setting_boolean(config, PACKAGE, "pref_main_save_winpos", TRUE);
	app->pref_main_show_search = utils_get_setting_boolean(config, PACKAGE, "pref_main_show_search", TRUE);
	app->pref_template_developer = utils_get_setting_string(config, PACKAGE, "pref_template_developer", g_get_real_name());
	app->pref_template_company = utils_get_setting_string(config, PACKAGE, "pref_template_company", "");
#ifdef HAVE_VTE
	app->terminal_settings = utils_get_setting_string(config, PACKAGE, "terminal_settings",	"");
#endif
	tmp_string = utils_get_initials(app->pref_template_developer);
	app->pref_template_initial = utils_get_setting_string(config, PACKAGE, "pref_template_initial", tmp_string);
	g_free(tmp_string);

	app->pref_template_version = utils_get_setting_string(config, PACKAGE, "pref_template_version", "1.0");

	tmp_string2 = utils_get_hostname();
	tmp_string = g_strdup_printf("%s@%s", g_get_user_name(), tmp_string2);
	app->pref_template_mail = utils_get_setting_string(config, PACKAGE, "pref_template_mail", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	app->pref_editor_new_line = utils_get_setting_boolean(config, PACKAGE, "pref_editor_new_line", TRUE);
	app->pref_editor_trail_space = utils_get_setting_boolean(config, PACKAGE, "pref_editor_trail_space", TRUE);

	tmp_string2 = g_find_program_in_path("gcc");
	tmp_string = g_strconcat(tmp_string2, " -Wall", NULL);
	app->build_c_cmd = utils_get_setting_string(config, "build", "build_c_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	tmp_string2 = g_find_program_in_path("g++");
	tmp_string = g_strconcat(tmp_string2, " -Wall", NULL);
	app->build_cpp_cmd = utils_get_setting_string(config, "build", "build_cpp_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	tmp_string = g_find_program_in_path("java");
	app->build_java_cmd = utils_get_setting_string(config, "build", "build_java_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string2 = g_find_program_in_path("javac");
	tmp_string = g_strconcat(tmp_string2, " -verbose", NULL);
	app->build_javac_cmd = utils_get_setting_string(config, "build", "build_javac_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	tmp_string = g_find_program_in_path("fpc");
	app->build_fpc_cmd = utils_get_setting_string(config, "build", "build_fpc_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string2 = g_find_program_in_path("latex");
	tmp_string = g_strconcat(tmp_string2, " -interaction=nonstopmode", NULL);
	app->build_tex_dvi_cmd = utils_get_setting_string(config, "build", "build_tex_dvi_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	tmp_string2 = g_find_program_in_path("pdflatex");
	tmp_string = g_strconcat(tmp_string2, " -interaction=nonstopmode", NULL);
	app->build_tex_pdf_cmd = utils_get_setting_string(config, "build", "build_tex_pdf_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	tmp_string = g_find_program_in_path("xdvi");
	app->build_tex_view_dvi_cmd = utils_get_setting_string(config, "build", "build_tex_view_dvi_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string = g_find_program_in_path("xpdf");
	app->build_tex_view_pdf_cmd = utils_get_setting_string(config, "build", "build_tex_view_pdf_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string = g_find_program_in_path("make");
	app->build_make_cmd = utils_get_setting_string(config, "build", "build_make_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string = g_find_program_in_path("term");
	app->build_term_cmd = utils_get_setting_string(config, "build", "build_term_cmd", tmp_string);
	g_free(tmp_string);

	tmp_string = g_find_program_in_path("mozilla");
	app->build_browser_cmd = utils_get_setting_string(config, "build", "build_browser_cmd", tmp_string);
	g_free(tmp_string);

	app->recent_files = g_key_file_get_string_list(config, "files", "recent_files", &len, NULL);
	if (app->recent_files != NULL)
	{
		for (i = 0; (i < len) && (i < app->mru_length); i++)
		{
			g_queue_push_head(app->recent_queue, app->recent_files[i]);
			app->recent_files[i] = NULL;
		}
	}
	//geany_debug("%d", g_strv_length(app->recent_files));

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

	utils_free_ptr_array(app->recent_files, len);
	g_key_file_free(config);
	g_free(configfile);
	g_free(entry);
	g_free(geo);
	return TRUE;
}


gboolean configuration_open_files(void)
{
	gint i;
	guint x, pos;
	gchar *file, spos[7];
	gboolean ret = FALSE;

	for(i = GEANY_SESSION_FILES - 1; i >= 0 ; i--)
	{
		if (session_files[i] && strlen(session_files[i]))
		{
			x = 0;
			file = strchr(session_files[i], ':') + 1;
			while (session_files[i][x] != ':')
			{
				spos[x] = session_files[i][x];
				x++;
			}
			spos[x] = '\0';
			pos = atoi(spos);

			if (g_file_test(file, G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK))
			{
				document_open_file(-1, file, pos, FALSE);
				ret = TRUE;
			}
		}
		g_free(session_files[i]);
	}

	if (scribble_text)
	{	// update the scribble widget, because now it's realized
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(lookup_widget(app->window, "textview_scribble"))),
				scribble_text, -1);
	}
	g_free(scribble_text);

	// set the position of the hpaned
	if (app->pref_main_save_winpos)
	{
		gtk_paned_set_position(GTK_PANED(lookup_widget(app->window, "hpaned1")), hpan_position);
		gtk_paned_set_position(GTK_PANED(lookup_widget(app->window, "vpaned1")), vpan_position);
	}


	return ret;
}

