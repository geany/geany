/*
 *      keyfile.c - this file is part of Geany, a fast and lightweight IDE
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
 * geany.conf preferences file loading and saving.
 */

/*
 * Session file format:
 * filename_xx=pos;filetype UID;read only;encoding idx;use_tabs;auto_indent;line_wrapping;filename
 */

#include <stdlib.h>
#include <string.h>

#include "geany.h"

#ifdef HAVE_VTE
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "support.h"
#include "keyfile.h"
#include "prefs.h"
#include "ui_utils.h"
#include "utils.h"
#include "document.h"
#include "filetypes.h"
#include "sciwrappers.h"
#include "encodings.h"
#include "vte.h"
#include "main.h"
#include "msgwindow.h"
#include "search.h"
#include "project.h"
#include "editor.h"
#include "printing.h"
#include "plugins.h"


static gchar *scribble_text = NULL;
static GPtrArray *session_files = NULL;
static gint session_notebook_page;
static gint hpan_position;
static gint vpan_position;


static void save_recent_files(GKeyFile *config)
{
	gchar **recent_files = g_new0(gchar*, prefs.mru_length + 1);
	guint i;

	for (i = 0; i < prefs.mru_length; i++)
	{
		if (! g_queue_is_empty(ui_prefs.recent_queue))
		{
			/* copy the values, this is necessary when this function is called from the
			 * preferences dialog or when quitting is canceled to keep the queue intact */
			recent_files[i] = g_strdup(g_queue_peek_nth(ui_prefs.recent_queue, i));
		}
		else
		{
			recent_files[i] = NULL;
			break;
		}
	}
	/* There is a bug in GTK 2.6 g_key_file_set_string_list, we must NULL terminate. */
	recent_files[prefs.mru_length] = NULL;
	g_key_file_set_string_list(config, "files", "recent_files",
				(const gchar**)recent_files, prefs.mru_length);
	g_strfreev(recent_files);
}


void configuration_save_session_files(GKeyFile *config)
{
	gint idx, npage;
	gchar *tmp;
	gchar entry[14];
	guint i = 0, j = 0, max;

	npage = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
	g_key_file_set_integer(config, "files", "current_page", npage);

	/* store the filenames to reopen them the next time */
	max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
	for (i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		if (idx >= 0 && doc_list[idx].file_name)
		{
			gchar *fname;
			filetype *ft = doc_list[idx].file_type;

			if (ft == NULL)	/* can happen when saving a new file when quitting */
				ft = filetypes[GEANY_FILETYPES_ALL];
			g_snprintf(entry, 13, "FILE_NAME_%d", j);
			fname = g_strdup_printf("%d;%d;%d;%d;%d;%d;%d;%s;",
				sci_get_current_position(doc_list[idx].sci),
				ft->uid,
				doc_list[idx].readonly,
				encodings_get_idx_from_charset(doc_list[idx].encoding),
				doc_list[idx].use_tabs,
				doc_list[idx].auto_indent,
				doc_list[idx].line_wrapping,
				doc_list[idx].file_name);
			g_key_file_set_string(config, "files", entry, fname);
			g_free(fname);
			j++;
		}
	}
	/* if open filenames less than saved session files, delete existing entries in the list */
	i = j;
	while (TRUE)
	{
		g_snprintf(entry, 13, "FILE_NAME_%d", i);
		tmp = g_key_file_get_string(config, "files", entry, NULL);
		if (tmp == NULL)
		{
			break;
		}
		else
		{
			g_key_file_remove_key(config, "files", entry, NULL);
			g_free(tmp);
			i++;
		}
	}
}


#ifdef HAVE_PLUGINS
static void save_plugin_prefs(GKeyFile *config)
{
	g_key_file_set_boolean(config, "plugins", "load_plugins", prefs.load_plugins);
	plugins_create_active_list();
	if (app->active_plugins != NULL)
		g_key_file_set_string_list(config, "plugins", "active_plugins",
			(const gchar**)app->active_plugins, g_strv_length(app->active_plugins));
	else
	{
		/* use an empty dummy array to override maybe exisiting value */
		const gchar *dummy[] = { "" };
		g_key_file_set_string_list(config, "plugins", "active_plugins", dummy, 1);
	}
}
#endif


static void save_dialog_prefs(GKeyFile *config)
{
	/* Some of the key names are not consistent, but this is for backwards compatibility */

	/* general */
	g_key_file_set_boolean(config, PACKAGE, "pref_main_load_session", prefs.load_session);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_project_session", project_prefs.project_session);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_save_winpos", prefs.save_winpos);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_confirm_exit", prefs.confirm_exit);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_suppress_search_dialogs", prefs.suppress_search_dialogs);
	g_key_file_set_boolean(config, PACKAGE, "pref_main_suppress_status_messages", prefs.suppress_status_messages);
	g_key_file_set_boolean(config, PACKAGE, "switch_msgwin_pages", prefs.switch_msgwin_pages);
	g_key_file_set_boolean(config, PACKAGE, "beep_on_errors", prefs.beep_on_errors);
	g_key_file_set_boolean(config, PACKAGE, "auto_focus", prefs.auto_focus);
	g_key_file_set_string(config, PACKAGE, "default_open_path", prefs.default_open_path);

	/* interface */
	g_key_file_set_boolean(config, PACKAGE, "sidebar_symbol_visible", prefs.sidebar_symbol_visible);
	g_key_file_set_boolean(config, PACKAGE, "sidebar_openfiles_visible", prefs.sidebar_openfiles_visible);
	g_key_file_set_boolean(config, PACKAGE, "sidebar_openfiles_fullpath", prefs.sidebar_openfiles_fullpath);
	g_key_file_set_string(config, PACKAGE, "editor_font", prefs.editor_font);
	g_key_file_set_string(config, PACKAGE, "tagbar_font", prefs.tagbar_font);
	g_key_file_set_string(config, PACKAGE, "msgwin_font", prefs.msgwin_font);
	g_key_file_set_boolean(config, PACKAGE, "show_notebook_tabs", prefs.show_notebook_tabs);
	g_key_file_set_boolean(config, PACKAGE, "show_tab_cross", prefs.show_tab_cross);
	g_key_file_set_boolean(config, PACKAGE, "tab_order_ltr", prefs.tab_order_ltr);
	g_key_file_set_integer(config, PACKAGE, "tab_pos_editor", prefs.tab_pos_editor);
	g_key_file_set_integer(config, PACKAGE, "tab_pos_msgwin", prefs.tab_pos_msgwin);
	g_key_file_set_integer(config, PACKAGE, "tab_pos_sidebar", prefs.tab_pos_sidebar);

	/* display */
	g_key_file_set_boolean(config, PACKAGE, "show_indent_guide", editor_prefs.show_indent_guide);
	g_key_file_set_boolean(config, PACKAGE, "show_white_space", editor_prefs.show_white_space);
	g_key_file_set_boolean(config, PACKAGE, "show_line_endings", editor_prefs.show_line_endings);
	g_key_file_set_boolean(config, PACKAGE, "show_markers_margin", editor_prefs.show_markers_margin);
	g_key_file_set_boolean(config, PACKAGE, "show_linenumber_margin", editor_prefs.show_linenumber_margin);
	g_key_file_set_integer(config, PACKAGE, "long_line_type", editor_prefs.long_line_type);
	g_key_file_set_integer(config, PACKAGE, "long_line_column", editor_prefs.long_line_column);
	g_key_file_set_string(config, PACKAGE, "long_line_color", editor_prefs.long_line_color);

	/* editor */
	g_key_file_set_integer(config, PACKAGE, "symbolcompletion_max_height", editor_prefs.symbolcompletion_max_height);
	g_key_file_set_integer(config, PACKAGE, "symbolcompletion_min_chars", editor_prefs.symbolcompletion_min_chars);
	g_key_file_set_boolean(config, PACKAGE, "use_folding", editor_prefs.folding);
	g_key_file_set_boolean(config, PACKAGE, "unfold_all_children", editor_prefs.unfold_all_children);
	g_key_file_set_integer(config, PACKAGE, "indent_mode", editor_prefs.indent_mode);
	g_key_file_set_integer(config, PACKAGE, "check_detect_indent", editor_prefs.detect_tab_mode);
	g_key_file_set_boolean(config, PACKAGE, "use_indicators", editor_prefs.use_indicators);
	g_key_file_set_boolean(config, PACKAGE, "line_breaking", editor_prefs.line_wrapping);
	g_key_file_set_boolean(config, PACKAGE, "auto_close_xml_tags", editor_prefs.auto_close_xml_tags);
	g_key_file_set_boolean(config, PACKAGE, "complete_snippets", editor_prefs.complete_snippets);
	g_key_file_set_boolean(config, PACKAGE, "auto_complete_symbols", editor_prefs.auto_complete_symbols);
	g_key_file_set_integer(config, PACKAGE, "pref_editor_tab_width", editor_prefs.tab_width);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_use_tabs", editor_prefs.use_tabs);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_disable_dnd", editor_prefs.disable_dnd);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_smart_home_key", editor_prefs.smart_home_key);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_newline_strip", editor_prefs.newline_strip);

	/* files */
	g_key_file_set_string(config, PACKAGE, "pref_editor_default_new_encoding", encodings[prefs.default_new_encoding].charset);
	if (prefs.default_open_encoding == -1)
		g_key_file_set_string(config, PACKAGE, "pref_editor_default_open_encoding", "none");
	else
		g_key_file_set_string(config, PACKAGE, "pref_editor_default_open_encoding", encodings[prefs.default_open_encoding].charset);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_new_line", prefs.final_new_line);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_replace_tabs", prefs.replace_tabs);
	g_key_file_set_boolean(config, PACKAGE, "pref_editor_trail_space", prefs.strip_trailing_spaces);
	g_key_file_set_integer(config, PACKAGE, "mru_length", prefs.mru_length);

	/* toolbar */
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show", prefs.toolbar_visible);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_search", prefs.toolbar_show_search);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_goto", prefs.toolbar_show_goto);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_zoom", prefs.toolbar_show_zoom);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_indent", prefs.toolbar_show_indent);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_undo", prefs.toolbar_show_undo);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_navigation", prefs.toolbar_show_navigation);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_compile", prefs.toolbar_show_compile);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_colour", prefs.toolbar_show_colour);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_fileops", prefs.toolbar_show_fileops);
	g_key_file_set_boolean(config, PACKAGE, "pref_toolbar_show_quit", prefs.toolbar_show_quit);
	g_key_file_set_integer(config, PACKAGE, "pref_toolbar_icon_style", prefs.toolbar_icon_style);
	g_key_file_set_integer(config, PACKAGE, "pref_toolbar_icon_size", prefs.toolbar_icon_size);

	/* templates */
	g_key_file_set_string(config, PACKAGE, "pref_template_developer", prefs.template_developer);
	g_key_file_set_string(config, PACKAGE, "pref_template_company", prefs.template_company);
	g_key_file_set_string(config, PACKAGE, "pref_template_mail", prefs.template_mail);
	g_key_file_set_string(config, PACKAGE, "pref_template_initial", prefs.template_initial);
	g_key_file_set_string(config, PACKAGE, "pref_template_version", prefs.template_version);

	/* tools settings */
	g_key_file_set_string(config, "tools", "make_cmd", prefs.tools_make_cmd ? prefs.tools_make_cmd : "");
	g_key_file_set_string(config, "tools", "term_cmd", prefs.tools_term_cmd ? prefs.tools_term_cmd : "");
	g_key_file_set_string(config, "tools", "browser_cmd", prefs.tools_browser_cmd ? prefs.tools_browser_cmd : "");
	g_key_file_set_string(config, "tools", "grep_cmd", prefs.tools_grep_cmd ? prefs.tools_grep_cmd : "");
	g_key_file_set_string(config, PACKAGE, "context_action_cmd", prefs.context_action_cmd);

	/* printing */
	g_key_file_set_string(config, "printing", "print_cmd", printing_prefs.external_print_cmd ? printing_prefs.external_print_cmd : "");
	g_key_file_set_boolean(config, "printing", "use_gtk_printing", printing_prefs.use_gtk_printing);
	g_key_file_set_boolean(config, "printing", "print_line_numbers", printing_prefs.print_line_numbers);
	g_key_file_set_boolean(config, "printing", "print_page_numbers", printing_prefs.print_page_numbers);
	g_key_file_set_boolean(config, "printing", "print_page_header", printing_prefs.print_page_header);
	g_key_file_set_boolean(config, "printing", "page_header_basename", printing_prefs.page_header_basename);
	g_key_file_set_string(config, "printing", "page_header_datefmt", printing_prefs.page_header_datefmt);

	/* VTE */
#ifdef HAVE_VTE
	g_key_file_set_boolean(config, "VTE", "load_vte", vte_info.load_vte);
	if (vte_info.load_vte && vc != NULL)
	{
		gchar *tmp_string;

		g_key_file_set_string(config, "VTE", "emulation", vc->emulation);
		g_key_file_set_string(config, "VTE", "font", vc->font);
		g_key_file_set_boolean(config, "VTE", "scroll_on_key", vc->scroll_on_key);
		g_key_file_set_boolean(config, "VTE", "scroll_on_out", vc->scroll_on_out);
		g_key_file_set_boolean(config, "VTE", "enable_bash_keys", vc->enable_bash_keys);
		g_key_file_set_boolean(config, "VTE", "ignore_menu_bar_accel", vc->ignore_menu_bar_accel);
		g_key_file_set_boolean(config, "VTE", "follow_path", vc->follow_path);
		g_key_file_set_boolean(config, "VTE", "run_in_vte", vc->run_in_vte);
		g_key_file_set_boolean(config, "VTE", "skip_run_script", vc->skip_run_script);
		g_key_file_set_integer(config, "VTE", "scrollback_lines", vc->scrollback_lines);
		g_key_file_set_string(config, "VTE", "font", vc->font);
		g_key_file_set_string(config, "VTE", "shell", vc->shell);
		tmp_string = utils_get_hex_from_color(vc->colour_fore);
		g_key_file_set_string(config, "VTE", "colour_fore", tmp_string);
		g_free(tmp_string);
		tmp_string = utils_get_hex_from_color(vc->colour_back);
		g_key_file_set_string(config, "VTE", "colour_back", tmp_string);
		g_free(tmp_string);
		vte_get_working_directory();	/* refresh vte_info.dir */
		g_key_file_set_string(config, "VTE", "last_dir", vte_info.dir);
	}
#endif
}


static void save_ui_prefs(GKeyFile *config)
{
	g_key_file_set_boolean(config, PACKAGE, "sidebar_visible", ui_prefs.sidebar_visible);
	g_key_file_set_boolean(config, PACKAGE, "statusbar_visible", prefs.statusbar_visible);
	g_key_file_set_boolean(config, PACKAGE, "msgwindow_visible", ui_prefs.msgwindow_visible);
	g_key_file_set_boolean(config, PACKAGE, "fullscreen", ui_prefs.fullscreen);

	/* get the text from the scribble textview */
	{
		GtkTextBuffer *buffer;
		GtkTextIter start, end;

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lookup_widget(app->window, "textview_scribble")));
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		scribble_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
		g_key_file_set_string(config, PACKAGE, "scribble_text", scribble_text);
		g_free(scribble_text);
	}

	if (prefs.save_winpos)
	{
		g_key_file_set_integer(config, PACKAGE, "treeview_position",
				gtk_paned_get_position(GTK_PANED(lookup_widget(app->window, "hpaned1"))));
		g_key_file_set_integer(config, PACKAGE, "msgwindow_position",
				gtk_paned_get_position(GTK_PANED(lookup_widget(app->window, "vpaned1"))));
	}

	if (prefs.save_winpos)
	{
		gtk_window_get_position(GTK_WINDOW(app->window), &ui_prefs.geometry[0], &ui_prefs.geometry[1]);
		gtk_window_get_size(GTK_WINDOW(app->window), &ui_prefs.geometry[2], &ui_prefs.geometry[3]);
		if (gdk_window_get_state(app->window->window) & GDK_WINDOW_STATE_MAXIMIZED)
			ui_prefs.geometry[4] = 1;
		else
			ui_prefs.geometry[4] = 0;

		g_key_file_set_integer_list(config, PACKAGE, "geometry", ui_prefs.geometry, 5);
	}

	g_key_file_set_string(config, PACKAGE, "custom_date_format", ui_prefs.custom_date_format);
	if (ui_prefs.custom_commands != NULL)
	{
		g_key_file_set_string_list(config, PACKAGE, "custom_commands",
				(const gchar**) ui_prefs.custom_commands, g_strv_length(ui_prefs.custom_commands));
	}

	/* search */
	g_key_file_set_string(config, "search", "fif_extra_options", search_prefs.fif_extra_options ? search_prefs.fif_extra_options : "");
}


/* hidden prefs (don't overwrite them so users can edit them manually) */
#define write_hidden_pref_boolean(conf, pkg, key, val) \
		if (! g_key_file_has_key((conf), (pkg), (key), NULL)) \
			g_key_file_set_boolean((conf), (pkg), (key), (val));

static void save_hidden_prefs(GKeyFile *config)
{
	write_hidden_pref_boolean(config, PACKAGE, "show_editor_scrollbars", editor_prefs.show_scrollbars);
	write_hidden_pref_boolean(config, PACKAGE, "use_tab_to_indent", editor_prefs.use_tab_to_indent);
	write_hidden_pref_boolean(config, PACKAGE, "brace_match_ltgt", editor_prefs.brace_match_ltgt);
	write_hidden_pref_boolean(config, PACKAGE, "use_gtk_word_boundaries", editor_prefs.use_gtk_word_boundaries);
	write_hidden_pref_boolean(config, PACKAGE, "complete_snippets_whilst_editing", editor_prefs.complete_snippets_whilst_editing);
	write_hidden_pref_boolean(config, PACKAGE, "scroll_stop_at_last_line", editor_prefs.scroll_stop_at_last_line);
#if GTK_CHECK_VERSION(2, 12, 0)
	write_hidden_pref_boolean(config, PACKAGE, "show_symbol_list_expanders", prefs.show_symbol_list_expanders);
#endif
}


void configuration_save(void)
{
	GKeyFile *config = g_key_file_new();
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	gchar *data;

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);

	save_dialog_prefs(config);
#ifdef HAVE_PLUGINS
	save_plugin_prefs(config);
#endif
	save_hidden_prefs(config);
	save_ui_prefs(config);
	project_save_prefs(config);	/* save project filename, etc. */
	save_recent_files(config);
	if (cl_options.load_session)
		configuration_save_session_files(config);

	/* write the file */
	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(configfile, data);
	g_free(data);

	g_key_file_free(config);
	g_free(configfile);
}

/*
 * Load session list from the given keyfile, and store it in the global
 * session_files variable for later file loading
 * */
void configuration_load_session_files(GKeyFile *config)
{
	gchar **recent_files;
	guint i;
	gsize len = 0;
	gboolean have_session_files;
	gchar entry[14];
	gchar **tmp_array;
	GError *error = NULL;

	session_notebook_page = utils_get_setting_integer(config, "files", "current_page", -1);

	recent_files = g_key_file_get_string_list(config, "files", "recent_files", &len, NULL);
	if (recent_files != NULL)
	{
		for (i = 0; (i < len) && (i < prefs.mru_length); i++)
		{
			gchar *filename = g_strdup(recent_files[i]);
			g_queue_push_tail(ui_prefs.recent_queue, filename);
		}
	}
	g_strfreev(recent_files);

	/* the project may load another list than the main setting */
	if (session_files != NULL)
		g_ptr_array_free(session_files, TRUE);

	session_files = g_ptr_array_new();
	have_session_files = TRUE;
	i = 0;
	while (have_session_files)
	{
		g_snprintf(entry, 13, "FILE_NAME_%d", i);
		tmp_array = g_key_file_get_string_list(config, "files", entry, NULL, &error);
		if (! tmp_array || error)
		{
			g_error_free(error);
			error = NULL;
			have_session_files = FALSE;
		}
		g_ptr_array_add(session_files, tmp_array);
		i++;
	}
}


#define GEANY_GET_SETTING(propertyname, value, default_value) \
	if (g_object_class_find_property( \
		G_OBJECT_GET_CLASS(G_OBJECT(gtk_settings_get_default())), propertyname)) \
			g_object_get(G_OBJECT(gtk_settings_get_default()), propertyname, &value, \
				NULL); \
	else \
		value = default_value;

static void load_dialog_prefs(GKeyFile *config)
{
	gchar *tmp_string, *tmp_string2;
	const gchar *default_charset = NULL;

	/* general */
	prefs.confirm_exit = utils_get_setting_boolean(config, PACKAGE, "pref_main_confirm_exit", FALSE);
	prefs.suppress_search_dialogs = utils_get_setting_boolean(config, PACKAGE, "pref_main_suppress_search_dialogs", FALSE);
	prefs.suppress_status_messages = utils_get_setting_boolean(config, PACKAGE, "pref_main_suppress_status_messages", FALSE);
	prefs.load_session = utils_get_setting_boolean(config, PACKAGE, "pref_main_load_session", TRUE);
	project_prefs.project_session = utils_get_setting_boolean(config, PACKAGE, "pref_main_project_session", TRUE);
	prefs.save_winpos = utils_get_setting_boolean(config, PACKAGE, "pref_main_save_winpos", TRUE);
	prefs.beep_on_errors = utils_get_setting_boolean(config, PACKAGE, "beep_on_errors", TRUE);
	prefs.switch_msgwin_pages = utils_get_setting_boolean(config, PACKAGE, "switch_msgwin_pages", FALSE);
	prefs.auto_focus = utils_get_setting_boolean(config, PACKAGE, "auto_focus", FALSE);
	prefs.default_open_path = utils_get_setting_string(config, PACKAGE, "default_open_path", "");

	/* interface */
	prefs.tab_pos_editor = utils_get_setting_integer(config, PACKAGE, "tab_pos_editor", GTK_POS_TOP);
	prefs.tab_pos_msgwin = utils_get_setting_integer(config, PACKAGE, "tab_pos_msgwin",GTK_POS_LEFT);
	prefs.tab_pos_sidebar = utils_get_setting_integer(config, PACKAGE, "tab_pos_sidebar", GTK_POS_TOP);
	prefs.sidebar_symbol_visible = utils_get_setting_boolean(config, PACKAGE, "sidebar_symbol_visible", TRUE);
	prefs.sidebar_openfiles_visible = utils_get_setting_boolean(config, PACKAGE, "sidebar_openfiles_visible", TRUE);
	prefs.sidebar_openfiles_fullpath = utils_get_setting_boolean(config, PACKAGE, "sidebar_openfiles_fullpath", FALSE);
	prefs.statusbar_visible = utils_get_setting_boolean(config, PACKAGE, "statusbar_visible", TRUE);
	prefs.tab_order_ltr = utils_get_setting_boolean(config, PACKAGE, "tab_order_ltr", TRUE);
	prefs.show_notebook_tabs = utils_get_setting_boolean(config, PACKAGE, "show_notebook_tabs", TRUE);
	prefs.show_tab_cross = utils_get_setting_boolean(config, PACKAGE, "show_tab_cross", TRUE);
	prefs.editor_font = utils_get_setting_string(config, PACKAGE, "editor_font", GEANY_DEFAULT_FONT_EDITOR);
	prefs.tagbar_font = utils_get_setting_string(config, PACKAGE, "tagbar_font", GEANY_DEFAULT_FONT_SYMBOL_LIST);
	prefs.msgwin_font = utils_get_setting_string(config, PACKAGE, "msgwin_font", GEANY_DEFAULT_FONT_MSG_WINDOW);
#if GTK_CHECK_VERSION(2, 12, 0)
	prefs.show_symbol_list_expanders = utils_get_setting_boolean(config, PACKAGE, "show_symbol_list_expanders", TRUE);
#endif

	/* display, editor */
	editor_prefs.long_line_type = utils_get_setting_integer(config, PACKAGE, "long_line_type", 0);
	editor_prefs.long_line_color = utils_get_setting_string(config, PACKAGE, "long_line_color", "#C2EBC2");
	editor_prefs.long_line_column = utils_get_setting_integer(config, PACKAGE, "long_line_column", 72);
	editor_prefs.symbolcompletion_min_chars = utils_get_setting_integer(config, PACKAGE, "symbolcompletion_min_chars", GEANY_MIN_SYMBOLLIST_CHARS);
	editor_prefs.symbolcompletion_max_height = utils_get_setting_integer(config, PACKAGE, "symbolcompletion_max_height", GEANY_MAX_SYMBOLLIST_HEIGHT);
	editor_prefs.line_wrapping = utils_get_setting_boolean(config, PACKAGE, "line_breaking", FALSE); /* default is off for better performance */
	editor_prefs.indent_mode = utils_get_setting_integer(config, PACKAGE, "indent_mode", INDENT_CURRENTCHARS);
	editor_prefs.detect_tab_mode = utils_get_setting_integer(config, PACKAGE, "check_detect_indent", FALSE);
	editor_prefs.use_tab_to_indent = utils_get_setting_boolean(config, PACKAGE, "use_tab_to_indent", FALSE);
	editor_prefs.use_indicators = utils_get_setting_boolean(config, PACKAGE, "use_indicators", TRUE);
	editor_prefs.show_indent_guide = utils_get_setting_boolean(config, PACKAGE, "show_indent_guide", FALSE);
	editor_prefs.show_white_space = utils_get_setting_boolean(config, PACKAGE, "show_white_space", FALSE);
	editor_prefs.show_line_endings = utils_get_setting_boolean(config, PACKAGE, "show_line_endings", FALSE);
	editor_prefs.auto_close_xml_tags = utils_get_setting_boolean(config, PACKAGE, "auto_close_xml_tags", TRUE);
	editor_prefs.complete_snippets = utils_get_setting_boolean(config, PACKAGE, "complete_snippets", TRUE);
	editor_prefs.auto_complete_symbols = utils_get_setting_boolean(config, PACKAGE, "auto_complete_symbols", TRUE);
	editor_prefs.folding = utils_get_setting_boolean(config, PACKAGE, "use_folding", TRUE);
	editor_prefs.unfold_all_children = utils_get_setting_boolean(config, PACKAGE, "unfold_all_children", FALSE);
	editor_prefs.show_scrollbars = utils_get_setting_boolean(config, PACKAGE, "show_editor_scrollbars", TRUE);
	editor_prefs.show_markers_margin = utils_get_setting_boolean(config, PACKAGE, "show_markers_margin", TRUE);
	editor_prefs.show_linenumber_margin = utils_get_setting_boolean(config, PACKAGE, "show_linenumber_margin", TRUE);
	editor_prefs.brace_match_ltgt = utils_get_setting_boolean(config, PACKAGE, "brace_match_ltgt", FALSE);
	editor_prefs.tab_width = utils_get_setting_integer(config, PACKAGE, "pref_editor_tab_width", 4);
	editor_prefs.use_tabs = utils_get_setting_boolean(config, PACKAGE, "pref_editor_use_tabs", TRUE);
	editor_prefs.disable_dnd = utils_get_setting_boolean(config, PACKAGE, "pref_editor_disable_dnd", FALSE);
	editor_prefs.smart_home_key = utils_get_setting_boolean(config, PACKAGE, "pref_editor_smart_home_key", TRUE);
	editor_prefs.newline_strip = utils_get_setting_boolean(config, PACKAGE, "pref_editor_newline_strip", FALSE);
	editor_prefs.use_gtk_word_boundaries = utils_get_setting_boolean(config, PACKAGE, "use_gtk_word_boundaries", TRUE);
	editor_prefs.complete_snippets_whilst_editing = utils_get_setting_boolean(config, PACKAGE, "complete_snippets_whilst_editing", FALSE);
	editor_prefs.scroll_stop_at_last_line = utils_get_setting_boolean(config, PACKAGE, "scroll_stop_at_last_line", FALSE);

	/* Files
	 * use current locale encoding as default for new files (should be UTF-8 in most cases) */
	g_get_charset(&default_charset);
	tmp_string = utils_get_setting_string(config, PACKAGE, "pref_editor_default_new_encoding",
		default_charset);
	if (tmp_string)
	{
		const GeanyEncoding *enc = encodings_get_from_charset(tmp_string);
		if (enc != NULL)
			prefs.default_new_encoding = enc->idx;
		else
			prefs.default_new_encoding = GEANY_ENCODING_UTF_8;

		g_free(tmp_string);
	}
	tmp_string = utils_get_setting_string(config, PACKAGE, "pref_editor_default_open_encoding",
		"none");
	if (tmp_string)
	{
		const GeanyEncoding *enc = encodings_get_from_charset(tmp_string);
		if (enc != NULL)
			prefs.default_open_encoding = enc->idx;
		else
			prefs.default_open_encoding = -1;

		g_free(tmp_string);
	}
	prefs.replace_tabs = utils_get_setting_boolean(config, PACKAGE, "pref_editor_replace_tabs", FALSE);
	prefs.final_new_line = utils_get_setting_boolean(config, PACKAGE, "pref_editor_new_line", TRUE);
	prefs.strip_trailing_spaces = utils_get_setting_boolean(config, PACKAGE, "pref_editor_trail_space", FALSE);
	prefs.mru_length = utils_get_setting_integer(config, PACKAGE, "mru_length", GEANY_DEFAULT_MRU_LENGTH);

	/* toolbar */
	prefs.toolbar_visible = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show", TRUE);
	prefs.toolbar_show_search = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_search", TRUE);
	prefs.toolbar_show_goto = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_goto", TRUE);
	prefs.toolbar_show_zoom = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_zoom", FALSE);
	prefs.toolbar_show_indent = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_indent", FALSE);
	prefs.toolbar_show_compile = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_compile", TRUE);
	prefs.toolbar_show_undo = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_undo", FALSE);
	prefs.toolbar_show_navigation = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_navigation", TRUE);
	prefs.toolbar_show_colour = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_colour", TRUE);
	prefs.toolbar_show_fileops = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_fileops", TRUE);
	prefs.toolbar_show_quit = utils_get_setting_boolean(config, PACKAGE, "pref_toolbar_show_quit", TRUE);
	{
		GtkIconSize tb_iconsize;
		GtkToolbarStyle tb_style;
		GEANY_GET_SETTING("gtk-toolbar-style", tb_style, GTK_TOOLBAR_ICONS);
		GEANY_GET_SETTING("gtk-toolbar-icon-size", tb_iconsize, GTK_ICON_SIZE_LARGE_TOOLBAR);
		prefs.toolbar_icon_style = utils_get_setting_integer(config, PACKAGE, "pref_toolbar_icon_style", tb_style);
		prefs.toolbar_icon_size = utils_get_setting_integer(config, PACKAGE, "pref_toolbar_icon_size", tb_iconsize);
	}

	/* VTE */
#ifdef HAVE_VTE
	vte_info.load_vte = utils_get_setting_boolean(config, "VTE", "load_vte", TRUE);
	if (vte_info.load_vte)
	{
		struct passwd *pw = getpwuid(getuid());
		gchar *shell = (pw != NULL) ? pw->pw_shell : "/bin/sh";

		vc = g_new0(VteConfig, 1);
		vte_info.dir = utils_get_setting_string(config, "VTE", "last_dir", NULL);
		if ((vte_info.dir == NULL || utils_str_equal(vte_info.dir, "")) && pw != NULL)
			/* last dir is not set, fallback to user's home directory */
			vte_info.dir = g_strdup(pw->pw_dir);
		else if (vte_info.dir == NULL && pw == NULL)
			/* fallback to root */
			vte_info.dir = g_strdup("/");

		vc->emulation = utils_get_setting_string(config, "VTE", "emulation", "xterm");
		vc->shell = utils_get_setting_string(config, "VTE", "shell", shell);
		vc->font = utils_get_setting_string(config, "VTE", "font", "Monospace 10");
		vc->scroll_on_key = utils_get_setting_boolean(config, "VTE", "scroll_on_key", TRUE);
		vc->scroll_on_out = utils_get_setting_boolean(config, "VTE", "scroll_on_out", TRUE);
		vc->enable_bash_keys = utils_get_setting_boolean(config, "VTE", "enable_bash_keys", FALSE);
		vc->ignore_menu_bar_accel = utils_get_setting_boolean(config, "VTE", "ignore_menu_bar_accel", FALSE);
		vc->follow_path = utils_get_setting_boolean(config, "VTE", "follow_path", FALSE);
		vc->run_in_vte = utils_get_setting_boolean(config, "VTE", "run_in_vte", FALSE);
		vc->skip_run_script = utils_get_setting_boolean(config, "VTE", "skip_run_script", FALSE);
		vc->scrollback_lines = utils_get_setting_integer(config, "VTE", "scrollback_lines", 500);
		vc->colour_fore = g_new0(GdkColor, 1);
		vc->colour_back = g_new0(GdkColor, 1);
		tmp_string = utils_get_setting_string(config, "VTE", "colour_fore", "#ffffff");
		gdk_color_parse(tmp_string, vc->colour_fore);
		g_free(tmp_string);
		tmp_string = utils_get_setting_string(config, "VTE", "colour_back", "#000000");
		gdk_color_parse(tmp_string, vc->colour_back);
		g_free(tmp_string);
	}
#endif
	/* templates */
	prefs.template_developer = utils_get_setting_string(config, PACKAGE, "pref_template_developer", g_get_real_name());
	prefs.template_company = utils_get_setting_string(config, PACKAGE, "pref_template_company", "");
	tmp_string = utils_get_initials(prefs.template_developer);
	prefs.template_initial = utils_get_setting_string(config, PACKAGE, "pref_template_initial", tmp_string);
	g_free(tmp_string);

	prefs.template_version = utils_get_setting_string(config, PACKAGE, "pref_template_version", "1.0");

	tmp_string2 = utils_get_hostname();
	tmp_string = g_strdup_printf("%s@%s", g_get_user_name(), tmp_string2);
	prefs.template_mail = utils_get_setting_string(config, PACKAGE, "pref_template_mail", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	/* tools */
	prefs.tools_make_cmd = utils_get_setting_string(config, "tools", "make_cmd", GEANY_DEFAULT_TOOLS_MAKE);
	prefs.tools_term_cmd = utils_get_setting_string(config, "tools", "term_cmd", GEANY_DEFAULT_TOOLS_TERMINAL);
	prefs.tools_browser_cmd = utils_get_setting_string(config, "tools", "browser_cmd", GEANY_DEFAULT_TOOLS_BROWSER);
	prefs.tools_grep_cmd = utils_get_setting_string(config, "tools", "grep_cmd", GEANY_DEFAULT_TOOLS_GREP);

	prefs.context_action_cmd = utils_get_setting_string(config, PACKAGE, "context_action_cmd", "");

	/* printing */
	tmp_string2 = g_find_program_in_path(GEANY_DEFAULT_TOOLS_PRINTCMD);
#ifdef G_OS_WIN32
	/* single quote paths on Win32 for g_spawn_command_line_async */
	tmp_string = g_strconcat("'", tmp_string2, "' '%f'", NULL);
#else
	tmp_string = g_strconcat(tmp_string2, " %f", NULL);
#endif
	printing_prefs.external_print_cmd = utils_get_setting_string(config, "printing", "print_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

#if GTK_CHECK_VERSION(2, 10, 0)
	printing_prefs.use_gtk_printing = utils_get_setting_boolean(config, "printing", "use_gtk_printing", TRUE);
#else
	printing_prefs.use_gtk_printing = utils_get_setting_boolean(config, "printing", "use_gtk_printing", FALSE);
#endif
	printing_prefs.print_line_numbers = utils_get_setting_boolean(config, "printing", "print_line_numbers", TRUE);
	printing_prefs.print_page_numbers = utils_get_setting_boolean(config, "printing", "print_page_numbers", TRUE);
	printing_prefs.print_page_header = utils_get_setting_boolean(config, "printing", "print_page_header", TRUE);
	printing_prefs.page_header_basename = utils_get_setting_boolean(config, "printing", "page_header_basename", FALSE);
	printing_prefs.page_header_datefmt = utils_get_setting_string(config, "printing", "page_header_datefmt", "%c");
}


#ifdef HAVE_PLUGINS
static void load_plugin_prefs(GKeyFile *config)
{
	prefs.load_plugins = utils_get_setting_boolean(config, "plugins", "load_plugins", TRUE);
	app->active_plugins = g_key_file_get_string_list(config, "plugins", "active_plugins", NULL, NULL);
}
#endif


static void load_ui_prefs(GKeyFile *config)
{
	gint *geo;
	GError *error = NULL;

	ui_prefs.sidebar_visible = utils_get_setting_boolean(config, PACKAGE, "sidebar_visible", TRUE);
	ui_prefs.msgwindow_visible = utils_get_setting_boolean(config, PACKAGE, "msgwindow_visible", TRUE);
	ui_prefs.fullscreen = utils_get_setting_boolean(config, PACKAGE, "fullscreen", FALSE);
	ui_prefs.custom_date_format = utils_get_setting_string(config, PACKAGE, "custom_date_format", "");
	ui_prefs.custom_commands = g_key_file_get_string_list(config, PACKAGE, "custom_commands", NULL, NULL);

	scribble_text = utils_get_setting_string(config, PACKAGE, "scribble_text",
				_("Type here what you want, use it as a notice/scratch board"));

	geo = g_key_file_get_integer_list(config, PACKAGE, "geometry", NULL, &error);
	if (error)
	{
		ui_prefs.geometry[0] = -1;
		g_error_free(error);
		error = NULL;
	}
	else
	{
		gint i;

		ui_prefs.geometry[0] = geo[0];
		ui_prefs.geometry[1] = geo[1];
		ui_prefs.geometry[2] = geo[2];
		ui_prefs.geometry[3] = geo[3];
		ui_prefs.geometry[4] = geo[4];

		/* don't use insane values but when main windows was maximized last time, pos might be
		 * negative at least on Windows for some reason */
		if (ui_prefs.geometry[4] != 1)
		{
			for (i = 0; i < 4; i++)
			{
				if (ui_prefs.geometry[i] < -1)
					ui_prefs.geometry[i] = -1;
			}
		}
	}
	hpan_position = utils_get_setting_integer(config, PACKAGE, "treeview_position", 156);
	vpan_position = utils_get_setting_integer(config, PACKAGE, "msgwindow_position", (geo) ?
				(GEANY_MSGWIN_HEIGHT + geo[3] - 440) :
				(GEANY_MSGWIN_HEIGHT + GEANY_WINDOW_DEFAULT_HEIGHT - 440));

	g_free(geo);

	/* search */
	search_prefs.fif_extra_options = utils_get_setting_string(config, "search", "fif_extra_options", "");
}


/*
 * Save current session in default configuration file
 */
void configuration_save_default_session(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	gchar *data;
	GKeyFile *config = g_key_file_new();

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);

	if (cl_options.load_session)
		configuration_save_session_files(config);

	/* write the file */
	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(configfile, data);
	g_free(data);

	g_key_file_free(config);
	g_free(configfile);
}


/*
 * Only reload the session part of the default configuration
 */
void configuration_reload_default_session(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	GKeyFile *config = g_key_file_new();

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);

	configuration_load_session_files(config);

	g_key_file_free(config);
	g_free(configfile);
}


gboolean configuration_load(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "geany.conf", NULL);
	GKeyFile *config = g_key_file_new();

	if (! g_file_test(configfile, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
	{	/* config file does not (yet) exist, so try to load a global config file which may be */
		/* created by distributors */
		geany_debug("No config file found, try to use global configuration.");
		setptr(configfile, g_strconcat(app->datadir, G_DIR_SEPARATOR_S "geany.conf", NULL));
	}

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);

	load_dialog_prefs(config);
#ifdef HAVE_PLUGINS
	load_plugin_prefs(config);
#endif
	load_ui_prefs(config);
	project_load_prefs(config);
	configuration_load_session_files(config);

	g_key_file_free(config);
	g_free(configfile);
	return TRUE;
}


/* Open session files
 * Note: notebook page switch handler and adding to recent files list is always disabled
 * for all files opened within this function */
gboolean configuration_open_files(void)
{
	gint i;
	guint pos;
	gboolean ret = FALSE, failure = FALSE;

	/* necessary to set it to TRUE for project session support */
	main_status.opening_session_files = TRUE;

	document_delay_colourise();

	i = prefs.tab_order_ltr ? 0 : (session_files->len - 1);
	while (TRUE)
	{
		gchar **tmp = g_ptr_array_index(session_files, i);

		if (tmp != NULL && g_strv_length(tmp) == 8)
		{
			gchar *locale_filename;
			gint ft_uid, enc_idx;
			gboolean ro, use_tabs, auto_indent, line_wrapping;

			pos = atoi(tmp[0]);
			ft_uid = atoi(tmp[1]);
			ro = atoi(tmp[2]);
			enc_idx = atoi(tmp[3]);
			use_tabs = atoi(tmp[4]);
			auto_indent = atoi(tmp[5]);
			line_wrapping = atoi(tmp[6]);
			/* try to get the locale equivalent for the filename */
			locale_filename = utils_get_locale_from_utf8(tmp[7]);

			if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
			{
				filetype *ft = filetypes_get_from_uid(ft_uid);
				gint new_idx = document_open_file_full(
					-1, locale_filename, pos, ro, ft,
					(enc_idx >= 0 && enc_idx < GEANY_ENCODINGS_MAX) ?
						encodings[enc_idx].charset : NULL);

				if (DOC_IDX_VALID(new_idx))
				{
					document_set_use_tabs(new_idx, use_tabs);
					document_set_line_wrapping(new_idx, line_wrapping);
					doc_list[new_idx].auto_indent = auto_indent;
					ret = TRUE;
				}
			}
			else
			{
				failure = TRUE;
				geany_debug("Could not find file '%s'.", tmp[7]);
			}

			g_free(locale_filename);
		}
		g_strfreev(tmp);

		if (prefs.tab_order_ltr)
		{
			i++;
			if (i >= (gint)session_files->len) break;
		}
		else
		{
			i--;
			if (i < 0) break;
		}
	}
	document_colourise_new();

	g_ptr_array_free(session_files, TRUE);
	session_files = NULL;

	if (failure)
		ui_set_statusbar(TRUE, _("Failed to load one or more session files."));
	else if (session_notebook_page >= 0)
	{
		/* exlicitly allow notebook switch page callback to be called for window title,
		 * encoding settings and so other things */
		main_status.opening_session_files = FALSE;
		/** TODO if session_notebook_page is equal to the current notebook tab(the last opened)
		 ** the notebook switch page callback isn't triggered and e.g. menu items are not updated */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), session_notebook_page);
	}
	main_status.opening_session_files = FALSE;
	return ret;
}


/* set some settings which are already read from the config file, but need other things, like the
 * realisation of the main window */
void configuration_apply_settings(void)
{
	if (scribble_text)
	{	/* update the scribble widget, because now it's realized */
		gtk_text_buffer_set_text(
				gtk_text_view_get_buffer(GTK_TEXT_VIEW(lookup_widget(app->window, "textview_scribble"))),
				scribble_text, -1);
	}
	g_free(scribble_text);

	/* set the position of the hpaned and vpaned */
	if (prefs.save_winpos)
	{
		gtk_paned_set_position(GTK_PANED(lookup_widget(app->window, "hpaned1")), hpan_position);
		gtk_paned_set_position(GTK_PANED(lookup_widget(app->window, "vpaned1")), vpan_position);
	}

	/* set fullscreen after initial draw so that returning to normal view is the right size.
	 * fullscreen mode is disabled by default, so act only if it is true */
	if (ui_prefs.fullscreen)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_fullscreen1")), TRUE);
		ui_prefs.fullscreen = TRUE;
		ui_set_fullscreen();
	}
}


#ifdef GEANY_DEBUG
/* Geany data file generation is only available with a debug build of Geany. */

static void generate_filetype_extensions(const gchar *output_dir);


/* Generate the config files in "data/" from defaults */
void configuration_generate_data_files(void)
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
	/* currently only filetype extensions are auto-generated. */
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
	g_key_file_set_comment(config, NULL, NULL,
		"*** This file generated by: geany --generate-data-files ***", NULL);
	/* add filetype keys */
	for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		g_key_file_set_string_list(config, "Extensions", filetypes[i]->name,
			(const gchar**) filetypes[i]->pattern, g_strv_length(filetypes[i]->pattern));
	}
	/* add comment */
	g_key_file_set_comment(config, "Extensions", NULL,
		"Filetype extension configuration file for Geany\n"
		"Insert as many items as you want, seperate them with a \";\".\n"
		"See Geany's main documentation for details.", NULL);

	/* write the file */
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

#endif


void configuration_read_filetype_extensions(void)
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

	/* read the keys */
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

	g_free(sysconfigfile);
	g_free(userconfigfile);
	g_key_file_free(sysconfig);
	g_key_file_free(userconfig);
}


void configuration_read_snippets(void)
{
	gsize i, j, len = 0, len_keys = 0;
	gchar *sysconfigfile, *userconfigfile;
	gchar **groups_user, **groups_sys;
	gchar **keys_user, **keys_sys;
	gchar *value;
	GKeyFile *sysconfig = g_key_file_new();
	GKeyFile *userconfig = g_key_file_new();
	GHashTable *tmp;

	sysconfigfile = g_strconcat(app->datadir, G_DIR_SEPARATOR_S, "snippets.conf", NULL);
	userconfigfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "snippets.conf", NULL);

	/* check for old autocomplete.conf files (backwards compatibility) */
	if (! g_file_test(userconfigfile, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
		setptr(userconfigfile,
			g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "autocomplete.conf", NULL));

	/* load the actual config files */
	g_key_file_load_from_file(sysconfig, sysconfigfile, G_KEY_FILE_NONE, NULL);
	g_key_file_load_from_file(userconfig, userconfigfile, G_KEY_FILE_NONE, NULL);

	/* keys are strings, values are GHashTables, so use g_free and g_hash_table_destroy */
	editor_prefs.snippets =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_hash_table_destroy);

	/* first read all globally defined auto completions */
	groups_sys = g_key_file_get_groups(sysconfig, &len);
	for (i = 0; i < len; i++)
	{
		keys_sys = g_key_file_get_keys(sysconfig, groups_sys[i], &len_keys, NULL);
		/* create new hash table for the read section (=> filetype) */
		tmp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(editor_prefs.snippets, g_strdup(groups_sys[i]), tmp);

		for (j = 0; j < len_keys; j++)
		{
			g_hash_table_insert(tmp, g_strdup(keys_sys[j]),
						utils_get_setting_string(sysconfig, groups_sys[i], keys_sys[j], ""));
		}
		g_strfreev(keys_sys);
	}

	/* now read defined completions in user's configuration directory and add / replace them */
	groups_user = g_key_file_get_groups(userconfig, &len);
	for (i = 0; i < len; i++)
	{
		keys_user = g_key_file_get_keys(userconfig, groups_user[i], &len_keys, NULL);

		tmp = g_hash_table_lookup(editor_prefs.snippets, groups_user[i]);
		if (tmp == NULL)
		{	/* new key found, create hash table */
			tmp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
			g_hash_table_insert(editor_prefs.snippets, g_strdup(groups_user[i]), tmp);
		}
		for (j = 0; j < len_keys; j++)
		{
			value = g_hash_table_lookup(tmp, keys_user[j]);
			if (value == NULL)
			{	/* value = NULL means the key doesn't yet exist, so insert */
				g_hash_table_insert(tmp, g_strdup(keys_user[j]),
						utils_get_setting_string(userconfig, groups_user[i], keys_user[j], ""));
			}
			else
			{	/* old key and value will be freed by destroy function (g_free) */
				g_hash_table_replace(tmp, g_strdup(keys_user[j]),
						utils_get_setting_string(userconfig, groups_user[i], keys_user[j], ""));
			}
		}
		g_strfreev(keys_user);
	}

	g_free(sysconfigfile);
	g_free(userconfigfile);
	g_strfreev(groups_sys);
	g_strfreev(groups_user);
	g_key_file_free(sysconfig);
	g_key_file_free(userconfig);
}
