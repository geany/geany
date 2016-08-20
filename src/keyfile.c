/*
 *      keyfile.c - this file is part of Geany, a fast and lightweight IDE
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * geany.conf preferences file loading and saving.
 */

/*
 * Session file format:
 * filename_xx=pos;filetype UID;read only;Eencoding;use_tabs;auto_indent;line_wrapping;filename
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "keyfile.h"

#include "app.h"
#include "build.h"
#include "document.h"
#include "encodings.h"
#include "encodingsprivate.h"
#include "filetypes.h"
#include "geanyobject.h"
#include "main.h"
#include "msgwindow.h"
#include "prefs.h"
#include "printing.h"
#include "project.h"
#include "sciwrappers.h"
#include "stash.h"
#include "support.h"
#include "symbols.h"
#include "templates.h"
#include "toolbar.h"
#include "ui_utils.h"
#include "utils.h"
#include "vte.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_VTE
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif


/* some default settings which are used at the very first start of Geany to fill
 * the configuration file */
#define GEANY_MAX_SYMBOLLIST_HEIGHT		10
#define GEANY_MIN_SYMBOLLIST_CHARS		4
#define GEANY_MSGWIN_HEIGHT				208
#define GEANY_DISK_CHECK_TIMEOUT		30
#define GEANY_DEFAULT_TOOLS_MAKE		"make"
#ifdef G_OS_WIN32
#define GEANY_DEFAULT_TOOLS_TERMINAL	"cmd.exe /Q /C %c"
#elif defined(__APPLE__)
#define GEANY_DEFAULT_TOOLS_TERMINAL	"open -a terminal %c"
#else
#define GEANY_DEFAULT_TOOLS_TERMINAL	"xterm -e \"/bin/sh %c\""
#endif
#ifdef __APPLE__
#define GEANY_DEFAULT_TOOLS_BROWSER		"open -a safari"
#define GEANY_DEFAULT_FONT_SYMBOL_LIST	"Helvetica Medium 12"
#define GEANY_DEFAULT_FONT_MSG_WINDOW	"Menlo Medium 12"
#define GEANY_DEFAULT_FONT_EDITOR		"Menlo Medium 12"
#else
#define GEANY_DEFAULT_TOOLS_BROWSER		"firefox"
#define GEANY_DEFAULT_FONT_SYMBOL_LIST	"Sans 9"
#define GEANY_DEFAULT_FONT_MSG_WINDOW	"Monospace 9"
#define GEANY_DEFAULT_FONT_EDITOR		"Monospace 10"
#endif
#define GEANY_DEFAULT_TOOLS_PRINTCMD	"lpr"
#define GEANY_DEFAULT_TOOLS_GREP		"grep"
#define GEANY_DEFAULT_MRU_LENGTH		10
#define GEANY_TOGGLE_MARK				"~ "
#define GEANY_MAX_AUTOCOMPLETE_WORDS	30
#define GEANY_MAX_SYMBOLS_UPDATE_FREQ	250
#define GEANY_DEFAULT_FILETYPE_REGEX    "-\\*-\\s*([^\\s]+)\\s*-\\*-"


static gchar *scribble_text = NULL;
static gint scribble_pos = -1;
static GPtrArray *session_files = NULL;
static gint session_notebook_page;
static gint hpan_position;
static gint vpan_position;
static const gchar atomic_file_saving_key[] = "use_atomic_file_saving";

static GPtrArray *keyfile_groups = NULL;

GPtrArray *pref_groups = NULL;

static struct
{
	gint number_ft_menu_items;
	gint number_non_ft_menu_items;
	gint number_exec_menu_items;
}
build_menu_prefs;


/* The group will be free'd on quitting.
 * @param for_prefs_dialog is whether the group also has Prefs dialog items. */
void configuration_add_pref_group(struct StashGroup *group, gboolean for_prefs_dialog)
{
	g_ptr_array_add(keyfile_groups, group);

	if (for_prefs_dialog)
		g_ptr_array_add(pref_groups, group);
}


/* The group will be free'd on quitting. */
void configuration_add_various_pref_group(struct StashGroup *group)
{
	configuration_add_pref_group(group, TRUE);
	stash_group_set_various(group, TRUE);
}


static void init_pref_groups(void)
{
	StashGroup *group;

	group = stash_group_new(PACKAGE);
	configuration_add_pref_group(group, TRUE);
	stash_group_add_entry(group, &prefs.default_open_path,
		"default_open_path", "", "startup_path_entry");

	stash_group_add_toggle_button(group, &file_prefs.cmdline_new_files,
		"cmdline_new_files", TRUE, "check_cmdline_new_files");

	stash_group_add_toggle_button(group, &interface_prefs.notebook_double_click_hides_widgets,
		"notebook_double_click_hides_widgets", FALSE, "check_double_click_hides_widgets");
	stash_group_add_toggle_button(group, &file_prefs.tab_close_switch_to_mru,
		"tab_close_switch_to_mru", FALSE, "check_tab_close_switch_to_mru");
	stash_group_add_radio_buttons(group, &interface_prefs.sidebar_pos,
		"sidebar_pos", GTK_POS_LEFT,
		"radio_sidebar_left", GTK_POS_LEFT,
		"radio_sidebar_right", GTK_POS_RIGHT,
		NULL);
	stash_group_add_radio_buttons(group, &interface_prefs.symbols_sort_mode,
		"symbols_sort_mode", SYMBOLS_SORT_BY_NAME,
		"radio_symbols_sort_by_name", SYMBOLS_SORT_BY_NAME,
		"radio_symbols_sort_by_appearance", SYMBOLS_SORT_BY_APPEARANCE,
		NULL);
	stash_group_add_radio_buttons(group, &interface_prefs.msgwin_orientation,
		"msgwin_orientation", GTK_ORIENTATION_VERTICAL,
		"radio_msgwin_vertical", GTK_ORIENTATION_VERTICAL,
		"radio_msgwin_horizontal", GTK_ORIENTATION_HORIZONTAL,
		NULL);

	/* startup */
	stash_group_add_toggle_button(group, &prefs.load_session,
		"pref_main_load_session", TRUE, "check_load_session");
	stash_group_add_toggle_button(group, &project_prefs.project_session,
		"pref_main_project_session", TRUE, "check_project_session");
	stash_group_add_toggle_button(group, &project_prefs.project_file_in_basedir,
		"pref_main_project_file_in_basedir", FALSE, "check_project_file_in_basedir");
	stash_group_add_toggle_button(group, &prefs.save_winpos,
		"pref_main_save_winpos", TRUE, "check_save_win_pos");

	/* behaviour */
	stash_group_add_toggle_button(group, &prefs.confirm_exit,
		"pref_main_confirm_exit", FALSE, "check_ask_for_quit");
	stash_group_add_toggle_button(group, &prefs.suppress_status_messages,
		"pref_main_suppress_status_messages", FALSE, "check_suppress_status_msgs");
	stash_group_add_toggle_button(group, &prefs.switch_to_status,
		"switch_msgwin_pages", FALSE, "check_switch_pages");
	stash_group_add_toggle_button(group, &prefs.beep_on_errors,
		"beep_on_errors", TRUE, "check_beep");
	stash_group_add_toggle_button(group, &prefs.auto_focus,
		"auto_focus", FALSE, "check_auto_focus");

	/* interface */
	stash_group_add_radio_buttons(group, &file_prefs.tab_order_ltr,
		"tab_order_ltr", TRUE,
		"radio_tab_right", TRUE,
		"radio_tab_left", FALSE,
		NULL);
	stash_group_add_toggle_button(group, &file_prefs.tab_order_beside,
		"tab_order_beside", FALSE, "check_tab_beside");
	stash_group_add_toggle_button(group, &interface_prefs.show_notebook_tabs,
		"show_notebook_tabs", TRUE, "check_show_notebook_tabs");
	stash_group_add_toggle_button(group, &file_prefs.show_tab_cross,
		"show_tab_cross", TRUE, "check_show_tab_cross");

	stash_group_add_combo_box(group, &interface_prefs.tab_pos_editor,
		"tab_pos_editor", GTK_POS_TOP, "combo_tab_editor");
	stash_group_add_combo_box(group, &interface_prefs.tab_pos_msgwin,
		"tab_pos_msgwin", GTK_POS_LEFT, "combo_tab_msgwin");
	stash_group_add_combo_box(group, &interface_prefs.tab_pos_sidebar,
		"tab_pos_sidebar", GTK_POS_TOP, "combo_tab_sidebar");

	stash_group_add_toggle_button(group, &interface_prefs.use_native_windows_dialogs,
		"use_native_windows_dialogs", FALSE, "check_native_windows_dialogs");

	stash_group_add_toggle_button(group, &ui_prefs.sidebar_visible,
		"sidebar_visible", TRUE, "check_sidebar_visible");
	stash_group_add_toggle_button(group, &interface_prefs.sidebar_symbol_visible,
		"sidebar_symbol_visible", TRUE, "check_list_symbol");
	stash_group_add_toggle_button(group, &interface_prefs.sidebar_openfiles_visible,
		"sidebar_openfiles_visible", TRUE, "check_list_openfiles");
	stash_group_add_toggle_button(group, &interface_prefs.statusbar_visible,
		"statusbar_visible", TRUE, "check_statusbar_visible");
	stash_group_add_boolean(group, &ui_prefs.msgwindow_visible,
		"msgwindow_visible", TRUE);

	stash_group_add_toggle_button(group, &editor_prefs.long_line_enabled,
		"long_line_enabled", TRUE, "check_long_line");
	stash_group_add_radio_buttons(group, &editor_prefs.long_line_type,
		"long_line_type", 0,
		"radio_long_line_line", 0,
		"radio_long_line_background", 1,
		NULL);
	stash_group_add_spin_button_integer(group, &editor_prefs.long_line_column,
		"long_line_column", 72, "spin_long_line");
	stash_group_add_string(group, &editor_prefs.long_line_color, "long_line_color", "#C2EBC2");

	/* fonts */
	stash_group_add_string(group, &interface_prefs.tagbar_font,
		"tagbar_font", GEANY_DEFAULT_FONT_SYMBOL_LIST);
	stash_group_add_string(group, &interface_prefs.msgwin_font,
		"msgwin_font", GEANY_DEFAULT_FONT_MSG_WINDOW);
	stash_group_add_string(group, &interface_prefs.editor_font,
		"editor_font", GEANY_DEFAULT_FONT_EDITOR);

	/* tools (in legacy section PACKAGE rather than "tools") */
	stash_group_add_entry(group, &tool_prefs.context_action_cmd,
			"context_action_cmd", "", "entry_contextaction");

	/* editor display */
	stash_group_add_toggle_button(group, &interface_prefs.highlighting_invert_all,
		"highlighting_invert_all", FALSE, "check_highlighting_invert");

	stash_group_add_toggle_button(group, &search_prefs.use_current_word,
		"pref_main_search_use_current_word", TRUE, "check_search_use_current_word");

	/* editor */
	stash_group_add_toggle_button(group, &editor_prefs.indentation->detect_type,
		"check_detect_indent", FALSE, "check_detect_indent_type");
	stash_group_add_toggle_button(group, &editor_prefs.indentation->detect_width,
		"detect_indent_width", FALSE, "check_detect_indent_width");
	stash_group_add_toggle_button(group, &editor_prefs.use_tab_to_indent,
		"use_tab_to_indent", TRUE, "check_tab_key_indents");
	stash_group_add_spin_button_integer(group, &editor_prefs.indentation->width,
		"pref_editor_tab_width", 4, "spin_indent_width");
	stash_group_add_combo_box(group, (gint*)(void*)&editor_prefs.indentation->auto_indent_mode,
		"indent_mode", GEANY_AUTOINDENT_CURRENTCHARS, "combo_auto_indent_mode");
	stash_group_add_radio_buttons(group, (gint*)(void*)&editor_prefs.indentation->type,
		"indent_type", GEANY_INDENT_TYPE_TABS,
		"radio_indent_spaces", GEANY_INDENT_TYPE_SPACES,
		"radio_indent_tabs", GEANY_INDENT_TYPE_TABS,
		"radio_indent_both", GEANY_INDENT_TYPE_BOTH,
		NULL);
	stash_group_add_radio_buttons(group, (gint*)(void*)&editor_prefs.show_virtual_space,
		"virtualspace", GEANY_VIRTUAL_SPACE_SELECTION,
		"radio_virtualspace_disabled", GEANY_VIRTUAL_SPACE_DISABLED,
		"radio_virtualspace_selection", GEANY_VIRTUAL_SPACE_SELECTION,
		"radio_virtualspace_always", GEANY_VIRTUAL_SPACE_ALWAYS,
		NULL);
	stash_group_add_toggle_button(group, &editor_prefs.auto_complete_symbols,
		"auto_complete_symbols", TRUE, "check_symbol_auto_completion");
	stash_group_add_toggle_button(group, &editor_prefs.autocomplete_doc_words,
		"autocomplete_doc_words", FALSE, "check_autocomplete_doc_words");
	stash_group_add_toggle_button(group, &editor_prefs.completion_drops_rest_of_word,
		"completion_drops_rest_of_word", FALSE, "check_completion_drops_rest_of_word");
	stash_group_add_spin_button_integer(group, (gint*)&editor_prefs.autocompletion_max_entries,
		"autocompletion_max_entries", GEANY_MAX_AUTOCOMPLETE_WORDS,
		"spin_autocompletion_max_entries");
	stash_group_add_spin_button_integer(group, (gint*)&editor_prefs.autocompletion_update_freq,
		"autocompletion_update_freq", GEANY_MAX_SYMBOLS_UPDATE_FREQ, "spin_symbol_update_freq");
	stash_group_add_spin_button_integer(group, &editor_prefs.symbolcompletion_min_chars,
		"symbolcompletion_min_chars", GEANY_MIN_SYMBOLLIST_CHARS, "spin_symbol_complete_chars");
	stash_group_add_spin_button_integer(group, &editor_prefs.symbolcompletion_max_height,
		"symbolcompletion_max_height", GEANY_MAX_SYMBOLLIST_HEIGHT, "spin_symbollistheight");
	stash_group_add_toggle_button(group, &editor_prefs.auto_close_xml_tags,
		"auto_close_xml_tags", TRUE, "check_xmltag");
	stash_group_add_toggle_button(group, &editor_prefs.complete_snippets,
		"complete_snippets", TRUE, "check_complete_snippets");
	stash_group_add_toggle_button(group, &editor_prefs.auto_continue_multiline,
		"auto_continue_multiline", TRUE, "check_auto_multiline");
	stash_group_add_integer(group, (gint*)&editor_prefs.autoclose_chars, "autoclose_chars", 0);
	stash_group_add_string(group, &editor_prefs.color_scheme,
		"color_scheme", NULL);
	stash_group_add_toggle_button(group, &editor_prefs.line_wrapping,
		"line_wrapping", FALSE, "check_line_wrapping"); /* default is off for better performance */
	stash_group_add_toggle_button(group, &editor_prefs.use_indicators,
		"use_indicators", TRUE, "check_indicators");
	stash_group_add_toggle_button(group, &editor_prefs.show_indent_guide,
		"show_indent_guide", FALSE, "check_indent");
	stash_group_add_toggle_button(group, &editor_prefs.show_white_space,
		"show_white_space", FALSE, "check_white_space");
	stash_group_add_toggle_button(group, &editor_prefs.show_line_endings,
		"show_line_endings", FALSE, "check_line_end");
	stash_group_add_toggle_button(group, &editor_prefs.show_markers_margin,
		"show_markers_margin", TRUE, "check_markers_margin");
	stash_group_add_toggle_button(group, &editor_prefs.show_linenumber_margin,
		"show_linenumber_margin", TRUE, "check_line_numbers");
	stash_group_add_toggle_button(group, &editor_prefs.scroll_stop_at_last_line,
		"scroll_stop_at_last_line", TRUE, "check_scroll_stop_at_last_line");
	stash_group_add_toggle_button(group, &editor_prefs.folding,
		"use_folding", TRUE, "check_folding");
	stash_group_add_toggle_button(group, &editor_prefs.unfold_all_children,
		"unfold_all_children", FALSE, "check_unfold_children");
	stash_group_add_toggle_button(group, &editor_prefs.disable_dnd,
		"pref_editor_disable_dnd", FALSE, "check_disable_dnd");
	stash_group_add_toggle_button(group, &editor_prefs.smart_home_key,
		"pref_editor_smart_home_key", TRUE, "check_smart_home");
	stash_group_add_toggle_button(group, &editor_prefs.newline_strip,
		"pref_editor_newline_strip", FALSE, "check_newline_strip");
	stash_group_add_spin_button_integer(group, &editor_prefs.line_break_column,
		"line_break_column", 72, "spin_line_break");
	stash_group_add_entry(group, &editor_prefs.comment_toggle_mark,
		"comment_toggle_mark", GEANY_TOGGLE_MARK, "entry_toggle_mark");

	/* files */
	stash_group_add_spin_button_integer(group, (gint*)&file_prefs.mru_length,
		"mru_length", GEANY_DEFAULT_MRU_LENGTH, "spin_mru");
	stash_group_add_spin_button_integer(group, &file_prefs.disk_check_timeout,
		"disk_check_timeout", GEANY_DISK_CHECK_TIMEOUT, "spin_disk_check");

	stash_group_add_combo_box(group, &file_prefs.default_eol_character,
		"default_eol_character", GEANY_DEFAULT_EOL_CHARACTER, "combo_eol");
	stash_group_add_toggle_button(group, &file_prefs.replace_tabs,
		"pref_editor_replace_tabs", FALSE, "check_replace_tabs");
	stash_group_add_toggle_button(group, &file_prefs.ensure_convert_new_lines,
		"pref_editor_ensure_convert_line_endings", FALSE, "check_ensure_convert_new_lines");
	stash_group_add_toggle_button(group, &file_prefs.final_new_line,
		"pref_editor_new_line", TRUE, "check_new_line");
	stash_group_add_toggle_button(group, &file_prefs.strip_trailing_spaces,
		"pref_editor_trail_space", FALSE, "check_trailing_spaces");

	/* toolbar */
	stash_group_add_toggle_button(group, &toolbar_prefs.visible,
		"pref_toolbar_show", TRUE, "check_toolbar_show");
	stash_group_add_toggle_button(group, &toolbar_prefs.append_to_menu,
		"pref_toolbar_append_to_menu", FALSE, "check_toolbar_in_menu");
	/* the UI settings for this is tricky: they are radio buttons spread between 2 settings.
	 * We put the default last, so if checked it has precedence over the radio items */
	stash_group_add_radio_buttons(group, (gint*)&toolbar_prefs.icon_style,
		"pref_toolbar_icon_style", GTK_TOOLBAR_ICONS,
		"radio_toolbar_style_default", GTK_TOOLBAR_ICONS /* whatever, overridden below -- last one wins */,
		"radio_toolbar_image", GTK_TOOLBAR_ICONS,
		"radio_toolbar_text", GTK_TOOLBAR_TEXT,
		"radio_toolbar_imagetext", GTK_TOOLBAR_BOTH,
		NULL);
	stash_group_add_toggle_button(group, &toolbar_prefs.use_gtk_default_style,
		"pref_toolbar_use_gtk_default_style", TRUE, "radio_toolbar_style_default");
	/* same as above */
	stash_group_add_radio_buttons(group, (gint*)&toolbar_prefs.icon_size,
		"pref_toolbar_icon_size", GTK_ICON_SIZE_LARGE_TOOLBAR,
		"radio_toolbar_icon_default", GTK_ICON_SIZE_LARGE_TOOLBAR /* whatever, overridden below -- last one wins */,
		"radio_toolbar_large", GTK_ICON_SIZE_LARGE_TOOLBAR,
		"radio_toolbar_small", GTK_ICON_SIZE_SMALL_TOOLBAR,
		"radio_toolbar_verysmall", GTK_ICON_SIZE_MENU,
		NULL);
	stash_group_add_toggle_button(group, &toolbar_prefs.use_gtk_default_icon,
		"pref_toolbar_use_gtk_default_icon", TRUE, "radio_toolbar_icon_default");

	/* various geany prefs */
	group = stash_group_new(PACKAGE);
	configuration_add_various_pref_group(group);

	stash_group_add_boolean(group, &editor_prefs.show_scrollbars,
		"show_editor_scrollbars", TRUE);
	stash_group_add_boolean(group, &editor_prefs.brace_match_ltgt,
		"brace_match_ltgt", FALSE);
	stash_group_add_boolean(group, &editor_prefs.use_gtk_word_boundaries,
		"use_gtk_word_boundaries", TRUE);
	stash_group_add_boolean(group, &editor_prefs.complete_snippets_whilst_editing,
		"complete_snippets_whilst_editing", FALSE);
	stash_group_add_boolean(group, &file_prefs.use_safe_file_saving,
		atomic_file_saving_key, FALSE);
	stash_group_add_boolean(group, &file_prefs.gio_unsafe_save_backup,
		"gio_unsafe_save_backup", FALSE);
	stash_group_add_boolean(group, &file_prefs.use_gio_unsafe_file_saving,
		"use_gio_unsafe_file_saving", TRUE);
	stash_group_add_boolean(group, &file_prefs.keep_edit_history_on_reload,
		"keep_edit_history_on_reload", TRUE);
	stash_group_add_boolean(group, &file_prefs.show_keep_edit_history_on_reload_msg,
		"show_keep_edit_history_on_reload_msg", TRUE);
	/* for backwards-compatibility */
	stash_group_add_integer(group, &editor_prefs.indentation->hard_tab_width,
		"indent_hard_tab_width", 8);
	stash_group_add_integer(group, (gint*)&search_prefs.find_selection_type,
		"find_selection_type", GEANY_FIND_SEL_CURRENT_WORD);
	stash_group_add_string(group, &file_prefs.extract_filetype_regex,
		"extract_filetype_regex", GEANY_DEFAULT_FILETYPE_REGEX);
	stash_group_add_boolean(group, &search_prefs.replace_and_find_by_default,
		"replace_and_find_by_default", TRUE);

	/* Note: Interface-related various prefs are in ui_init_prefs() */

	/* various build-menu prefs */
	group = stash_group_new("build-menu");
	configuration_add_various_pref_group(group);

	stash_group_add_integer(group, &build_menu_prefs.number_ft_menu_items,
		"number_ft_menu_items", 0);
	stash_group_add_integer(group, &build_menu_prefs.number_non_ft_menu_items,
		"number_non_ft_menu_items", 0);
	stash_group_add_integer(group, &build_menu_prefs.number_exec_menu_items,
		"number_exec_menu_items", 0);
}


typedef enum SettingAction
{
	SETTING_READ,
	SETTING_WRITE
}
SettingAction;

static void settings_action(GKeyFile *config, SettingAction action)
{
	guint i;
	StashGroup *group;

	foreach_ptr_array(group, i, keyfile_groups)
	{
		switch (action)
		{
			case SETTING_READ:
				stash_group_load_from_key_file(group, config); break;
			case SETTING_WRITE:
				stash_group_save_to_key_file(group, config); break;
		}
	}
}


static void save_recent_files(GKeyFile *config, GQueue *queue, gchar const *key)
{
	gchar **recent_files = g_new0(gchar*, file_prefs.mru_length + 1);
	guint i;

	for (i = 0; i < file_prefs.mru_length; i++)
	{
		if (! g_queue_is_empty(queue))
		{
			/* copy the values, this is necessary when this function is called from the
			 * preferences dialog or when quitting is canceled to keep the queue intact */
			recent_files[i] = g_strdup(g_queue_peek_nth(queue, i));
		}
		else
		{
			recent_files[i] = NULL;
			break;
		}
	}
	/* There is a bug in GTK 2.6 g_key_file_set_string_list, we must NULL terminate. */
	recent_files[file_prefs.mru_length] = NULL;
	g_key_file_set_string_list(config, "files", key,
				(const gchar**)recent_files, file_prefs.mru_length);
	g_strfreev(recent_files);
}


static gchar *get_session_file_string(GeanyDocument *doc)
{
	gchar *fname;
	gchar *locale_filename;
	gchar *escaped_filename;
	GeanyFiletype *ft = doc->file_type;

	if (ft == NULL) /* can happen when saving a new file when quitting */
		ft = filetypes[GEANY_FILETYPES_NONE];

	locale_filename = utils_get_locale_from_utf8(doc->file_name);
	escaped_filename = g_uri_escape_string(locale_filename, NULL, TRUE);

	fname = g_strdup_printf("%d;%s;%d;E%s;%d;%d;%d;%s;%d;%d",
		sci_get_current_position(doc->editor->sci),
		ft->name,
		doc->readonly,
		doc->encoding,
		doc->editor->indent_type,
		doc->editor->auto_indent,
		doc->editor->line_wrapping,
		escaped_filename,
		doc->editor->line_breaking,
		doc->editor->indent_width);
	g_free(escaped_filename);
	g_free(locale_filename);
	return fname;
}


static void remove_session_files(GKeyFile *config)
{
	gchar **ptr;
	gchar **keys = g_key_file_get_keys(config, "files", NULL, NULL);

	foreach_strv(ptr, keys)
	{
		if (g_str_has_prefix(*ptr, "FILE_NAME_"))
			g_key_file_remove_key(config, "files", *ptr, NULL);
	}
	g_strfreev(keys);
}


void configuration_save_session_files(GKeyFile *config)
{
	gint npage;
	gchar entry[16];
	guint i = 0, j = 0, max;

	npage = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.notebook));
	g_key_file_set_integer(config, "files", "current_page", npage);

	// clear existing entries first as they might not all be overwritten
	remove_session_files(config);

	/* store the filenames in the notebook tab order to reopen them the next time */
	max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	for (i = 0; i < max; i++)
	{
		GeanyDocument *doc = document_get_from_page(i);

		if (doc != NULL && doc->real_path != NULL)
		{
			gchar *fname;

			g_snprintf(entry, sizeof(entry), "FILE_NAME_%d", j);
			fname = get_session_file_string(doc);
			g_key_file_set_string(config, "files", entry, fname);
			g_free(fname);
			j++;
		}
	}

#ifdef HAVE_VTE
	if (vte_info.have_vte)
	{
		vte_get_working_directory();	/* refresh vte_info.dir */
		g_key_file_set_string(config, "VTE", "last_dir", vte_info.dir);
	}
#endif
}


static void save_dialog_prefs(GKeyFile *config)
{
	/* new settings should be added in init_pref_groups() */
	settings_action(config, SETTING_WRITE);

	/* Some of the key names are not consistent, but this is for backwards compatibility */

	/* files */
	g_key_file_set_string(config, PACKAGE, "pref_editor_default_new_encoding", encodings[file_prefs.default_new_encoding].charset);
	if (file_prefs.default_open_encoding == -1)
		g_key_file_set_string(config, PACKAGE, "pref_editor_default_open_encoding", "none");
	else
		g_key_file_set_string(config, PACKAGE, "pref_editor_default_open_encoding", encodings[file_prefs.default_open_encoding].charset);

	/* templates */
	g_key_file_set_string(config, PACKAGE, "pref_template_developer", template_prefs.developer);
	g_key_file_set_string(config, PACKAGE, "pref_template_company", template_prefs.company);
	g_key_file_set_string(config, PACKAGE, "pref_template_mail", template_prefs.mail);
	g_key_file_set_string(config, PACKAGE, "pref_template_initial", template_prefs.initials);
	g_key_file_set_string(config, PACKAGE, "pref_template_version", template_prefs.version);
	g_key_file_set_string(config, PACKAGE, "pref_template_year", template_prefs.year_format);
	g_key_file_set_string(config, PACKAGE, "pref_template_date", template_prefs.date_format);
	g_key_file_set_string(config, PACKAGE, "pref_template_datetime", template_prefs.datetime_format);

	/* tools settings */
	g_key_file_set_string(config, "tools", "terminal_cmd", tool_prefs.term_cmd ? tool_prefs.term_cmd : "");
	g_key_file_set_string(config, "tools", "browser_cmd", tool_prefs.browser_cmd ? tool_prefs.browser_cmd : "");
	g_key_file_set_string(config, "tools", "grep_cmd", tool_prefs.grep_cmd ? tool_prefs.grep_cmd : "");

	/* build menu */
	build_save_menu(config, NULL, GEANY_BCS_PREF);

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
	if (vte_info.have_vte)
	{
		gchar *tmp_string;

		if (!g_key_file_has_key(config, "VTE", "emulation", NULL))	/* hidden */
			g_key_file_set_string(config, "VTE", "emulation", vc->emulation);
		tmp_string = utils_get_hex_from_color(&vc->colour_fore);
		g_key_file_set_string(config, "VTE", "colour_fore", tmp_string);
		g_free(tmp_string);
		tmp_string = utils_get_hex_from_color(&vc->colour_back);
		g_key_file_set_string(config, "VTE", "colour_back", tmp_string);
		g_free(tmp_string);
	}
#endif
}


static void save_ui_prefs(GKeyFile *config)
{
	g_key_file_set_boolean(config, PACKAGE, "fullscreen", ui_prefs.fullscreen);

	/* get the text from the scribble textview */
	{
		GtkTextBuffer *buffer;
		GtkTextIter start, end, iter;
		GtkTextMark *mark;

		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(msgwindow.scribble));
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		scribble_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
		g_key_file_set_string(config, PACKAGE, "scribble_text", scribble_text);
		g_free(scribble_text);

		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
		scribble_pos = gtk_text_iter_get_offset(&iter);
		g_key_file_set_integer(config, PACKAGE, "scribble_pos", scribble_pos);
	}

	if (prefs.save_winpos)
	{
		GdkWindowState wstate;

		g_key_file_set_integer(config, PACKAGE, "treeview_position",
				gtk_paned_get_position(GTK_PANED(ui_lookup_widget(main_widgets.window, "hpaned1"))));
		g_key_file_set_integer(config, PACKAGE, "msgwindow_position",
				gtk_paned_get_position(GTK_PANED(ui_lookup_widget(main_widgets.window, "vpaned1"))));

		gtk_window_get_position(GTK_WINDOW(main_widgets.window), &ui_prefs.geometry[0], &ui_prefs.geometry[1]);
		gtk_window_get_size(GTK_WINDOW(main_widgets.window), &ui_prefs.geometry[2], &ui_prefs.geometry[3]);
		wstate = gdk_window_get_state(gtk_widget_get_window(main_widgets.window));
		ui_prefs.geometry[4] = (wstate & GDK_WINDOW_STATE_MAXIMIZED) ? 1 : 0;
		g_key_file_set_integer_list(config, PACKAGE, "geometry", ui_prefs.geometry, 5);
	}

	g_key_file_set_string(config, PACKAGE, "custom_date_format", ui_prefs.custom_date_format);
	if (ui_prefs.custom_commands != NULL)
	{
		g_key_file_set_string_list(config, PACKAGE, "custom_commands",
				(const gchar**) ui_prefs.custom_commands, g_strv_length(ui_prefs.custom_commands));
		g_key_file_set_string_list(config, PACKAGE, "custom_commands_labels",
				(const gchar**) ui_prefs.custom_commands_labels, g_strv_length(ui_prefs.custom_commands_labels));
	}
}


void configuration_save(void)
{
	GKeyFile *config = g_key_file_new();
	gchar *configfile = g_build_filename(app->configdir, "geany.conf", NULL);
	gchar *data;

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);

	/* this signal can be used e.g. to prepare any settings before Stash code reads them below */
	g_signal_emit_by_name(geany_object, "save-settings", config);

	save_dialog_prefs(config);
	save_ui_prefs(config);
	project_save_prefs(config);	/* save project filename, etc. */
	save_recent_files(config, ui_prefs.recent_queue, "recent_files");
	save_recent_files(config, ui_prefs.recent_projects_queue, "recent_projects");

	if (cl_options.load_session)
		configuration_save_session_files(config);
#ifdef HAVE_VTE
	else if (vte_info.have_vte)
	{
		vte_get_working_directory();	/* refresh vte_info.dir */
		g_key_file_set_string(config, "VTE", "last_dir", vte_info.dir);
	}
#endif

	/* write the file */
	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(configfile, data);
	g_free(data);

	g_key_file_free(config);
	g_free(configfile);
}


static void load_recent_files(GKeyFile *config, GQueue *queue, const gchar *key)
{
	gchar **recent_files;
	gsize i, len = 0;

	recent_files = g_key_file_get_string_list(config, "files", key, &len, NULL);
	if (recent_files != NULL)
	{
		for (i = 0; (i < len) && (i < file_prefs.mru_length); i++)
		{
			gchar *filename = g_strdup(recent_files[i]);
			g_queue_push_tail(queue, filename);
		}
		g_strfreev(recent_files);
	}
}


/*
 * Load session list from the given keyfile, and store it in the global
 * session_files variable for later file loading
 * */
void configuration_load_session_files(GKeyFile *config, gboolean read_recent_files)
{
	guint i;
	gboolean have_session_files;
	gchar entry[16];
	gchar **tmp_array;
	GError *error = NULL;

	session_notebook_page = utils_get_setting_integer(config, "files", "current_page", -1);

	if (read_recent_files)
	{
		load_recent_files(config, ui_prefs.recent_queue, "recent_files");
		load_recent_files(config, ui_prefs.recent_projects_queue, "recent_projects");
	}

	/* the project may load another list than the main setting */
	if (session_files != NULL)
	{
		foreach_ptr_array(tmp_array, i, session_files)
			g_strfreev(tmp_array);
		g_ptr_array_free(session_files, TRUE);
	}

	session_files = g_ptr_array_new();
	have_session_files = TRUE;
	i = 0;
	while (have_session_files)
	{
		g_snprintf(entry, sizeof(entry), "FILE_NAME_%d", i);
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

#ifdef HAVE_VTE
	/* BUG: after loading project at startup, closing project doesn't restore old VTE path */
	if (vte_info.have_vte)
	{
		gchar *tmp_string = utils_get_setting_string(config, "VTE", "last_dir", NULL);
		vte_cwd(tmp_string,TRUE);
		g_free(tmp_string);
	}
#endif
}


#ifdef HAVE_VTE
static void get_setting_color(GKeyFile *config, const gchar *section, const gchar *key,
		GdkColor *color, const gchar *default_color)
{
	gchar *str = utils_get_setting_string(config, section, key, NULL);
	if (str == NULL || ! utils_parse_color(str, color))
		utils_parse_color(default_color, color);
	g_free(str);
}
#endif


/* note: new settings should be added in init_pref_groups() */
static void load_dialog_prefs(GKeyFile *config)
{
	gchar *tmp_string, *tmp_string2;
	const gchar *default_charset = NULL;
	gchar *cmd;

	/* compatibility with Geany 0.20 */
	if (!g_key_file_has_key(config, PACKAGE, atomic_file_saving_key, NULL))
	{
		g_key_file_set_boolean(config, PACKAGE, atomic_file_saving_key,
			utils_get_setting_boolean(config, PACKAGE, "use_safe_file_saving", FALSE));
	}

	/* compatibility with Geany 0.21 */
	{
		gboolean suppress_search_dialogs = utils_get_setting_boolean(config, PACKAGE, "pref_main_suppress_search_dialogs", FALSE);

		if (!g_key_file_has_key(config, "search", "pref_search_always_wrap", NULL))
			g_key_file_set_boolean(config, "search", "pref_search_always_wrap", suppress_search_dialogs);

		if (!g_key_file_has_key(config, "search", "pref_search_hide_find_dialog", NULL))
			g_key_file_set_boolean(config, "search", "pref_search_hide_find_dialog", suppress_search_dialogs);
	}

	/* Files
	 * use current locale encoding as default for new files (should be UTF-8 in most cases) */
	g_get_charset(&default_charset);
	tmp_string = utils_get_setting_string(config, PACKAGE, "pref_editor_default_new_encoding",
		default_charset);
	if (tmp_string)
	{
		const GeanyEncoding *enc = encodings_get_from_charset(tmp_string);
		if (enc != NULL)
			file_prefs.default_new_encoding = enc->idx;
		else
			file_prefs.default_new_encoding = GEANY_ENCODING_UTF_8;

		g_free(tmp_string);
	}
	tmp_string = utils_get_setting_string(config, PACKAGE, "pref_editor_default_open_encoding",
		"none");
	if (tmp_string)
	{
		const GeanyEncoding *enc = encodings_get_from_charset(tmp_string);
		if (enc != NULL)
			file_prefs.default_open_encoding = enc->idx;
		else
			file_prefs.default_open_encoding = -1;

		g_free(tmp_string);
	}

	/* VTE */
#ifdef HAVE_VTE
	vte_info.load_vte = utils_get_setting_boolean(config, "VTE", "load_vte", TRUE);
	if (vte_info.load_vte && vte_info.have_vte /* not disabled on the cmdline */)
	{
		StashGroup *group;
		struct passwd *pw = getpwuid(getuid());
		const gchar *shell = (pw != NULL) ? pw->pw_shell : "/bin/sh";

#ifdef __APPLE__
		/* Geany is started using launchd on OS X and we don't get any environment variables
		 * so PS1 isn't defined. Start as a login shell to read the corresponding config files. */
		if (strcmp(shell, "/bin/bash") == 0)
			shell = "/bin/bash -l";
#endif

		vc = g_new0(VteConfig, 1);

		group = stash_group_new("VTE");
		configuration_add_pref_group(group, TRUE);

		stash_group_add_entry(group, &vc->image,
			"image", "", "entry_image");
		stash_group_add_entry(group, &vc->shell,
			"shell", shell, "entry_shell");
		stash_group_add_string(group, &vc->font,
			"font", GEANY_DEFAULT_FONT_EDITOR);
		stash_group_add_toggle_button(group, &vc->scroll_on_key,
			"scroll_on_key", TRUE, "check_scroll_key");
		stash_group_add_toggle_button(group, &vc->scroll_on_out,
			"scroll_on_out", TRUE, "check_scroll_out");
		stash_group_add_toggle_button(group, &vc->enable_bash_keys,
			"enable_bash_keys", TRUE, "check_enable_bash_keys");
		stash_group_add_toggle_button(group, &vc->ignore_menu_bar_accel,
			"ignore_menu_bar_accel", FALSE, "check_ignore_menu_key");
		stash_group_add_toggle_button(group, &vc->follow_path,
			"follow_path", FALSE, "check_follow_path");
		stash_group_add_toggle_button(group, &vc->run_in_vte,
			"run_in_vte", FALSE, "check_run_in_vte");
		stash_group_add_toggle_button(group, &vc->skip_run_script,
			"skip_run_script", FALSE, "check_skip_script");
		stash_group_add_toggle_button(group, &vc->cursor_blinks,
			"cursor_blinks", FALSE, "check_cursor_blinks");
		stash_group_add_spin_button_integer(group, &vc->scrollback_lines,
			"scrollback_lines", 500, "spin_scrollback");

		/* specially handled settings */
		vte_info.dir = utils_get_setting_string(config, "VTE", "last_dir", NULL);
		if ((vte_info.dir == NULL || utils_str_equal(vte_info.dir, "")) && pw != NULL)
			/* last dir is not set, fallback to user's home directory */
			SETPTR(vte_info.dir, g_strdup(pw->pw_dir));
		else if (vte_info.dir == NULL && pw == NULL)
			/* fallback to root */
			vte_info.dir = g_strdup("/");

		vc->emulation = utils_get_setting_string(config, "VTE", "emulation", "xterm");
		get_setting_color(config, "VTE", "colour_fore", &vc->colour_fore, "#ffffff");
		get_setting_color(config, "VTE", "colour_back", &vc->colour_back, "#000000");

		/* various VTE prefs.
		 * this can't be done in init_pref_groups() because we need to know the value of
		 * vte_info.load_vte, and `vc` to be initialized */
		group = stash_group_new("VTE");
		configuration_add_various_pref_group(group);

		stash_group_add_string(group, &vc->send_cmd_prefix, "send_cmd_prefix", "");
		stash_group_add_boolean(group, &vc->send_selection_unsafe, "send_selection_unsafe", FALSE);
	}
#endif
	/* templates */
	template_prefs.developer = utils_get_setting_string(config, PACKAGE, "pref_template_developer", g_get_real_name());
	template_prefs.company = utils_get_setting_string(config, PACKAGE, "pref_template_company", "");
	tmp_string = utils_get_initials(template_prefs.developer);
	template_prefs.initials = utils_get_setting_string(config, PACKAGE, "pref_template_initial", tmp_string);
	g_free(tmp_string);

	template_prefs.version = utils_get_setting_string(config, PACKAGE, "pref_template_version", "1.0");

	tmp_string = g_strdup_printf("%s@%s", g_get_user_name(), g_get_host_name());
	template_prefs.mail = utils_get_setting_string(config, PACKAGE, "pref_template_mail", tmp_string);
	g_free(tmp_string);
	template_prefs.year_format = utils_get_setting_string(config, PACKAGE, "pref_template_year", GEANY_TEMPLATES_FORMAT_YEAR);
	template_prefs.date_format = utils_get_setting_string(config, PACKAGE, "pref_template_date", GEANY_TEMPLATES_FORMAT_DATE);
	template_prefs.datetime_format = utils_get_setting_string(config, PACKAGE, "pref_template_datetime", GEANY_TEMPLATES_FORMAT_DATETIME);

	/* tools */
	cmd = utils_get_setting_string(config, "tools", "terminal_cmd", "");
	if (EMPTY(cmd))
	{
		SETPTR(cmd, utils_get_setting_string(config, "tools", "term_cmd", ""));
		if (!EMPTY(cmd))
		{
			tmp_string = cmd;
#ifdef G_OS_WIN32
			if (strstr(cmd, "cmd.exe"))
				cmd = g_strconcat(cmd, " /Q /C %c", NULL);
			else
				cmd = g_strconcat(cmd, " %c", NULL);
#else
			cmd = g_strconcat(cmd, " -e \"/bin/sh %c\"", NULL);
#endif
			g_free(tmp_string);
		}
		else
			SETPTR(cmd, g_strdup(GEANY_DEFAULT_TOOLS_TERMINAL));
	}
	tool_prefs.term_cmd = cmd;
	tool_prefs.browser_cmd = utils_get_setting_string(config, "tools", "browser_cmd", GEANY_DEFAULT_TOOLS_BROWSER);
	tool_prefs.grep_cmd = utils_get_setting_string(config, "tools", "grep_cmd", GEANY_DEFAULT_TOOLS_GREP);

	/* printing */
	tmp_string2 = g_find_program_in_path(GEANY_DEFAULT_TOOLS_PRINTCMD);

	if (!EMPTY(tmp_string2))
	{
	#ifdef G_OS_WIN32
		tmp_string = g_strconcat(GEANY_DEFAULT_TOOLS_PRINTCMD, " \"%f\"", NULL);
	#else
		tmp_string = g_strconcat(GEANY_DEFAULT_TOOLS_PRINTCMD, " '%f'", NULL);
	#endif
	}
	else
	{
		tmp_string = g_strdup("");
	}

	printing_prefs.external_print_cmd = utils_get_setting_string(config, "printing", "print_cmd", tmp_string);
	g_free(tmp_string);
	g_free(tmp_string2);

	printing_prefs.use_gtk_printing = utils_get_setting_boolean(config, "printing", "use_gtk_printing", TRUE);
	printing_prefs.print_line_numbers = utils_get_setting_boolean(config, "printing", "print_line_numbers", TRUE);
	printing_prefs.print_page_numbers = utils_get_setting_boolean(config, "printing", "print_page_numbers", TRUE);
	printing_prefs.print_page_header = utils_get_setting_boolean(config, "printing", "print_page_header", TRUE);
	printing_prefs.page_header_basename = utils_get_setting_boolean(config, "printing", "page_header_basename", FALSE);
	printing_prefs.page_header_datefmt = utils_get_setting_string(config, "printing", "page_header_datefmt", "%c");

	/* read stash prefs */
	settings_action(config, SETTING_READ);

	/* display, editor backward compatibility */
	if (editor_prefs.long_line_type == 2)
	{
		editor_prefs.long_line_type = 0;
		editor_prefs.long_line_enabled = FALSE;
	}

	/* build menu
	 * after stash prefs as it uses some of them */
	build_set_group_count(GEANY_GBG_FT, build_menu_prefs.number_ft_menu_items);
	build_set_group_count(GEANY_GBG_NON_FT, build_menu_prefs.number_non_ft_menu_items);
	build_set_group_count(GEANY_GBG_EXEC, build_menu_prefs.number_exec_menu_items);
	build_load_menu(config, GEANY_BCS_PREF, NULL);
}


static void load_ui_prefs(GKeyFile *config)
{
	gint *geo;
	gsize geo_len;

	ui_prefs.fullscreen = utils_get_setting_boolean(config, PACKAGE, "fullscreen", FALSE);
	ui_prefs.custom_date_format = utils_get_setting_string(config, PACKAGE, "custom_date_format", "");
	ui_prefs.custom_commands = g_key_file_get_string_list(config, PACKAGE, "custom_commands", NULL, NULL);
	ui_prefs.custom_commands_labels = g_key_file_get_string_list(config, PACKAGE, "custom_commands_labels", NULL, NULL);

	/* sanitize custom commands labels */
	if (ui_prefs.custom_commands || ui_prefs.custom_commands_labels)
	{
		guint i;
		guint cc_len = ui_prefs.custom_commands ? g_strv_length(ui_prefs.custom_commands) : 0;
		guint cc_labels_len = ui_prefs.custom_commands_labels ? g_strv_length(ui_prefs.custom_commands_labels) : 0;

		/* not enough items, resize and fill */
		if (cc_labels_len < cc_len)
		{
			ui_prefs.custom_commands_labels = g_realloc(ui_prefs.custom_commands_labels,
					(cc_len + 1) * sizeof *ui_prefs.custom_commands_labels);
			for (i = cc_labels_len; i < cc_len; i++)
				ui_prefs.custom_commands_labels[i] = g_strdup("");
			ui_prefs.custom_commands_labels[cc_len] = NULL;
		}
		/* too many items, cut off */
		else if (cc_labels_len > cc_len)
		{
			for (i = cc_len; i < cc_labels_len; i++)
			{
				g_free(ui_prefs.custom_commands_labels[i]);
				ui_prefs.custom_commands_labels[i] = NULL;
			}
		}
	}

	scribble_text = utils_get_setting_string(config, PACKAGE, "scribble_text",
				_("Type here what you want, use it as a notice/scratch board"));
	scribble_pos = utils_get_setting_integer(config, PACKAGE, "scribble_pos", -1);

	geo = g_key_file_get_integer_list(config, PACKAGE, "geometry", &geo_len, NULL);
	if (! geo || geo_len < 5)
	{
		ui_prefs.geometry[0] = -1;
		ui_prefs.geometry[1] = -1;
		ui_prefs.geometry[2] = -1;
		ui_prefs.geometry[3] = -1;
		ui_prefs.geometry[4] = 0;
	}
	else
	{
		/* don't use insane values but when main windows was maximized last time, pos might be
		 * negative (due to differences in root window and window decorations) */
		/* quitting when minimized can make pos -32000, -32000 on Windows! */
		ui_prefs.geometry[0] = MAX(-1, geo[0]);
		ui_prefs.geometry[1] = MAX(-1, geo[1]);
		ui_prefs.geometry[2] = MAX(-1, geo[2]);
		ui_prefs.geometry[3] = MAX(-1, geo[3]);
		ui_prefs.geometry[4] = geo[4] != 0;
	}
	hpan_position = utils_get_setting_integer(config, PACKAGE, "treeview_position", 156);
	vpan_position = utils_get_setting_integer(config, PACKAGE, "msgwindow_position", (geo) ?
				(GEANY_MSGWIN_HEIGHT + geo[3] - 440) :
				(GEANY_MSGWIN_HEIGHT + GEANY_WINDOW_DEFAULT_HEIGHT - 440));

	g_free(geo);
}


/*
 * Save current session in default configuration file
 */
void configuration_save_default_session(void)
{
	gchar *configfile = g_build_filename(app->configdir, "geany.conf", NULL);
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


void configuration_clear_default_session(void)
{
	gchar *configfile = g_build_filename(app->configdir, "geany.conf", NULL);
	gchar *data;
	GKeyFile *config = g_key_file_new();

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);

	if (cl_options.load_session)
		remove_session_files(config);

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
	gchar *configfile = g_build_filename(app->configdir, "geany.conf", NULL);
	GKeyFile *config = g_key_file_new();

	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);
	g_free(configfile);

	configuration_load_session_files(config, FALSE);

	g_key_file_free(config);
}


gboolean configuration_load(void)
{
	gchar *configfile = g_build_filename(app->configdir, "geany.conf", NULL);
	GKeyFile *config = g_key_file_new();

	if (! g_file_test(configfile, G_FILE_TEST_IS_REGULAR))
	{	/* config file does not (yet) exist, so try to load a global config file which may be */
		/* created by distributors */
		geany_debug("No user config file found, trying to use global configuration.");
		SETPTR(configfile, g_build_filename(app->datadir, "geany.conf", NULL));
	}
	g_key_file_load_from_file(config, configfile, G_KEY_FILE_NONE, NULL);
	g_free(configfile);

	load_dialog_prefs(config);
	load_ui_prefs(config);
	project_load_prefs(config);
	configuration_load_session_files(config, TRUE);

	/* this signal can be used e.g. to delay building UI elements until settings have been read */
	g_signal_emit_by_name(geany_object, "load-settings", config);

	g_key_file_free(config);
	return TRUE;
}


static gboolean open_session_file(gchar **tmp, guint len)
{
	guint pos;
	const gchar *ft_name;
	gchar *locale_filename;
	gchar *unescaped_filename;
	const gchar *encoding;
	gint  indent_type;
	gboolean ro, auto_indent, line_wrapping;
	/** TODO when we have a global pref for line breaking, use its value */
	gboolean line_breaking = FALSE;
	gboolean ret = FALSE;

	pos = atoi(tmp[0]);
	ft_name = tmp[1];
	ro = atoi(tmp[2]);
	if (isdigit(tmp[3][0]))
	{
		encoding = encodings_get_charset_from_index(atoi(tmp[3]));
	}
	else
	{
		encoding = &(tmp[3][1]);
	}
	indent_type = atoi(tmp[4]);
	auto_indent = atoi(tmp[5]);
	line_wrapping = atoi(tmp[6]);
	/* try to get the locale equivalent for the filename */
	unescaped_filename = g_uri_unescape_string(tmp[7], NULL);
	locale_filename = utils_get_locale_from_utf8(unescaped_filename);

	if (len > 8)
		line_breaking = atoi(tmp[8]);

	if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR))
	{
		GeanyFiletype *ft = filetypes_lookup_by_name(ft_name);
		GeanyDocument *doc = document_open_file_full(
			NULL, locale_filename, pos, ro, ft, encoding);

		if (doc)
		{
			gint indent_width = doc->editor->indent_width;

			if (len > 9)
				indent_width = atoi(tmp[9]);
			editor_set_indent(doc->editor, indent_type, indent_width);
			editor_set_line_wrapping(doc->editor, line_wrapping);
			doc->editor->line_breaking = line_breaking;
			doc->editor->auto_indent = auto_indent;
			ret = TRUE;
		}
	}
	else
	{
		geany_debug("Could not find file '%s'.", tmp[7]);
	}

	g_free(locale_filename);
	g_free(unescaped_filename);
	return ret;
}


/* Open session files
 * Note: notebook page switch handler and adding to recent files list is always disabled
 * for all files opened within this function */
void configuration_open_files(void)
{
	gint i;
	gboolean failure = FALSE;

	/* necessary to set it to TRUE for project session support */
	main_status.opening_session_files = TRUE;

	i = file_prefs.tab_order_ltr ? 0 : (session_files->len - 1);
	while (TRUE)
	{
		gchar **tmp = g_ptr_array_index(session_files, i);
		guint len;

		if (tmp != NULL && (len = g_strv_length(tmp)) >= 8)
		{
			if (! open_session_file(tmp, len))
				failure = TRUE;
		}
		g_strfreev(tmp);

		if (file_prefs.tab_order_ltr)
		{
			i++;
			if (i >= (gint)session_files->len)
				break;
		}
		else
		{
			i--;
			if (i < 0)
				break;
		}
	}

	g_ptr_array_free(session_files, TRUE);
	session_files = NULL;

	if (failure)
		ui_set_statusbar(TRUE, _("Failed to load one or more session files."));
	else
	{
		/* explicitly trigger a notebook page switch after unsetting main_status.opening_session_files
		 * for callbacks to run (and update window title, encoding settings, and so on) */
		gint n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
		gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.notebook));
		gint target_page = session_notebook_page >= 0 ? session_notebook_page : cur_page;

		/* if target page is current page, switch to another page first to really trigger an event */
		if (target_page == cur_page && n_pages > 0)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), (cur_page + 1) % n_pages);

		main_status.opening_session_files = FALSE;
		gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), target_page);
	}
	main_status.opening_session_files = FALSE;
}


/* set some settings which are already read from the config file, but need other things, like the
 * realisation of the main window */
void configuration_apply_settings(void)
{
	if (scribble_text)
	{	/* update the scribble widget, because now it's realized */
		GtkTextIter iter;
		GtkTextBuffer *buffer =
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(msgwindow.scribble));

		gtk_text_buffer_set_text(buffer, scribble_text, -1);
		gtk_text_buffer_get_iter_at_offset(buffer, &iter, scribble_pos);
		gtk_text_buffer_place_cursor(buffer, &iter);
	}
	g_free(scribble_text);

	/* set the position of the hpaned and vpaned */
	if (prefs.save_winpos)
	{
		gtk_paned_set_position(GTK_PANED(ui_lookup_widget(main_widgets.window, "hpaned1")), hpan_position);
		gtk_paned_set_position(GTK_PANED(ui_lookup_widget(main_widgets.window, "vpaned1")), vpan_position);
	}

	/* set fullscreen after initial draw so that returning to normal view is the right size.
	 * fullscreen mode is disabled by default, so act only if it is true */
	if (ui_prefs.fullscreen)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_fullscreen1")), TRUE);
		ui_prefs.fullscreen = TRUE;
		ui_set_fullscreen();
	}

	msgwin_show_hide_tabs();
}


void configuration_init(void)
{
	keyfile_groups = g_ptr_array_new();
	pref_groups = g_ptr_array_new();
	init_pref_groups();
}


void configuration_finalize(void)
{
	guint i;
	StashGroup *group;

	foreach_ptr_array(group, i, keyfile_groups)
		stash_group_free(group);

	g_ptr_array_free(keyfile_groups, TRUE);
	g_ptr_array_free(pref_groups, TRUE);
}
