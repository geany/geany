/*
 *      plugins.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#if defined(HAVE_PLUGINS) && !defined(GEANY_DISABLE_DEPRECATED)

#include "app.h"
#include "build.h"
#include "dialogs.h"
#include "document.h"
#include "editor.h"
#include "encodings.h"
#include "filetypes.h"
#include "geany.h"
#include "highlighting.h"
#include "keybindings.h"
#include "main.h"
#include "msgwindow.h"
#include "navqueue.h"
#include "pluginutils.h"
#include "plugindata.h"
#include "prefs.h"
#include "project.h"
#include "sciwrappers.h"
#include "search.h"
#include "stash.h"
#include "support.h"
#include "symbols.h"
#include "templates.h"
#include "toolbar.h"
#include "ui_utils.h"
#include "utils.h"

/* deprecated definition of geanyfunctions. This is purely to maintain ABI compatibility to
 * ABI level <= 69 (Aug. 2014). Remove at some point */

struct
{
	void	(*plugin_add_toolbar_item)(GeanyPlugin *plugin, GtkToolItem *item);
	void	(*plugin_module_make_resident) (GeanyPlugin *plugin);
	void	(*plugin_signal_connect) (GeanyPlugin *plugin,
		GObject *object, const gchar *signal_name, gboolean after,
		GCallback callback, gpointer user_data);
	GeanyKeyGroup* (*plugin_set_key_group)(GeanyPlugin *plugin,
		const gchar *section_name, gsize count, GeanyKeyGroupCallback callback);
	void	(*plugin_show_configure)(GeanyPlugin *plugin);
	guint	(*plugin_timeout_add) (GeanyPlugin *plugin, guint interval, GSourceFunc function,
		gpointer data);
	guint	(*plugin_timeout_add_seconds) (GeanyPlugin *plugin, guint interval,
		GSourceFunc function, gpointer data);
	guint	(*plugin_idle_add) (GeanyPlugin *plugin, GSourceFunc function, gpointer data);
	void	(*plugin_builder_connect_signals) (GeanyPlugin *plugin, GtkBuilder *builder, gpointer user_data);
} plugin_funcs = {
	&plugin_add_toolbar_item,
	&plugin_module_make_resident,
	&plugin_signal_connect,
	&plugin_set_key_group,
	&plugin_show_configure,
	&plugin_timeout_add,
	&plugin_timeout_add_seconds,
	&plugin_idle_add,
	&plugin_builder_connect_signals
};

/* See document.h */
struct
{
	struct GeanyDocument*	(*document_new_file) (const gchar *utf8_filename, struct GeanyFiletype *ft,
			const gchar *text);
	struct GeanyDocument*	(*document_get_current) (void);
	struct GeanyDocument*	(*document_get_from_page) (guint page_num);
	struct GeanyDocument*	(*document_find_by_filename) (const gchar *utf8_filename);
	struct GeanyDocument*	(*document_find_by_real_path) (const gchar *realname);
	gboolean				(*document_save_file) (struct GeanyDocument *doc, gboolean force);
	struct GeanyDocument*	(*document_open_file) (const gchar *locale_filename, gboolean readonly,
			struct GeanyFiletype *ft, const gchar *forced_enc);
	void		(*document_open_files) (const GSList *filenames, gboolean readonly,
			struct GeanyFiletype *ft, const gchar *forced_enc);
	gboolean	(*document_remove_page) (guint page_num);
	gboolean	(*document_reload_force) (struct GeanyDocument *doc, const gchar *forced_enc);
	void		(*document_set_encoding) (struct GeanyDocument *doc, const gchar *new_encoding);
	void		(*document_set_text_changed) (struct GeanyDocument *doc, gboolean changed);
	void		(*document_set_filetype) (struct GeanyDocument *doc, struct GeanyFiletype *type);
	gboolean	(*document_close) (struct GeanyDocument *doc);
	struct GeanyDocument*	(*document_index)(gint idx);
	gboolean	(*document_save_file_as) (struct GeanyDocument *doc, const gchar *utf8_fname);
	void		(*document_rename_file) (struct GeanyDocument *doc, const gchar *new_filename);
	const GdkColor*	(*document_get_status_color) (struct GeanyDocument *doc);
	gchar*		(*document_get_basename_for_display) (struct GeanyDocument *doc, gint length);
	gint		(*document_get_notebook_page) (struct GeanyDocument *doc);
	gint		(*document_compare_by_display_name) (gconstpointer a, gconstpointer b);
	gint		(*document_compare_by_tab_order) (gconstpointer a, gconstpointer b);
	gint		(*document_compare_by_tab_order_reverse) (gconstpointer a, gconstpointer b);
	GeanyDocument*	(*document_find_by_id)(guint id);
} doc_funcs = {
	&document_new_file,
	&document_get_current,
	&document_get_from_page,
	&document_find_by_filename,
	&document_find_by_real_path,
	&document_save_file,
	&document_open_file,
	&document_open_files,
	&document_remove_page,
	&document_reload_force,
	&document_set_encoding,
	&document_set_text_changed,
	&document_set_filetype,
	&document_close,
	&document_index,
	&document_save_file_as,
	&document_rename_file,
	&document_get_status_color,
	&document_get_basename_for_display,
	&document_get_notebook_page,
	&document_compare_by_display_name,
	&document_compare_by_tab_order,
	&document_compare_by_tab_order_reverse,
	&document_find_by_id
};


struct
{
	const GeanyIndentPrefs* (*editor_get_indent_prefs)(GeanyEditor *editor);
	ScintillaObject* (*editor_create_widget)(GeanyEditor *editor);

	void	(*editor_indicator_set_on_range) (GeanyEditor *editor, gint indic, gint start, gint end);
	void	(*editor_indicator_set_on_line) (GeanyEditor *editor, gint indic, gint line);
	void	(*editor_indicator_clear) (GeanyEditor *editor, gint indic);

	void	(*editor_set_indent_type)(GeanyEditor *editor, GeanyIndentType type);
	gchar*	(*editor_get_word_at_pos) (GeanyEditor *editor, gint pos, const gchar *wordchars);

	const gchar*	(*editor_get_eol_char_name) (GeanyEditor *editor);
	gint			(*editor_get_eol_char_len) (GeanyEditor *editor);
	const gchar*	(*editor_get_eol_char) (GeanyEditor *editor);

	void	(*editor_insert_text_block) (GeanyEditor *editor, const gchar *text,
			 gint insert_pos, gint cursor_index, gint newline_indent_size,
			 gboolean replace_newlines);

	gint	(*editor_get_eol_char_mode) (GeanyEditor *editor);
	gboolean (*editor_goto_pos) (GeanyEditor *editor, gint pos, gboolean mark);

	const gchar* (*editor_find_snippet) (GeanyEditor *editor, const gchar *snippet_name);
	void	(*editor_insert_snippet) (GeanyEditor *editor, gint pos, const gchar *snippet);
} editor_funcs = {
	&editor_get_indent_prefs,
	&editor_create_widget,
	&editor_indicator_set_on_range,
	&editor_indicator_set_on_line,
	&editor_indicator_clear,
	&editor_set_indent_type,
	&editor_get_word_at_pos,
	&editor_get_eol_char_name,
	&editor_get_eol_char_len,
	&editor_get_eol_char,
	&editor_insert_text_block,
	&editor_get_eol_char_mode,
	&editor_goto_pos,
	&editor_find_snippet,
	&editor_insert_snippet
};


struct
{
	long int	(*scintilla_send_message) (struct _ScintillaObject *sci, unsigned int iMessage,
			long unsigned int wParam, long int lParam);
	GtkWidget*	(*scintilla_new)(void);
}  scintilla_funcs = {
	&scintilla_send_message,
	&scintilla_new
};

struct
{
	long int (*sci_send_message) (struct _ScintillaObject *sci, unsigned int iMessage,
			long unsigned int wParam, long int lParam);
	void	(*sci_send_command) (struct _ScintillaObject *sci, gint cmd);

	void	(*sci_start_undo_action) (struct _ScintillaObject *sci);
	void	(*sci_end_undo_action) (struct _ScintillaObject *sci);
	void	(*sci_set_text) (struct _ScintillaObject *sci, const gchar *text);
	void	(*sci_insert_text) (struct _ScintillaObject *sci, gint pos, const gchar *text);
	void	(*sci_get_text) (struct _ScintillaObject *sci, gint len, gchar *text);
	gint	(*sci_get_length) (struct _ScintillaObject *sci);
	gint	(*sci_get_current_position) (struct _ScintillaObject *sci);
	void	(*sci_set_current_position) (struct _ScintillaObject *sci, gint position,
			 gboolean scroll_to_caret);
	gint	(*sci_get_col_from_position) (struct _ScintillaObject *sci, gint position);
	gint	(*sci_get_line_from_position) (struct _ScintillaObject *sci, gint position);
	gint	(*sci_get_position_from_line) (struct _ScintillaObject *sci, gint line);
	void	(*sci_replace_sel) (struct _ScintillaObject *sci, const gchar *text);
	void	(*sci_get_selected_text) (struct _ScintillaObject *sci, gchar *text);
	gint	(*sci_get_selected_text_length) (struct _ScintillaObject *sci);
	gint	(*sci_get_selection_start) (struct _ScintillaObject *sci);
	gint	(*sci_get_selection_end) (struct _ScintillaObject *sci);
	gint	(*sci_get_selection_mode) (struct _ScintillaObject *sci);
	void	(*sci_set_selection_mode) (struct _ScintillaObject *sci, gint mode);
	void	(*sci_set_selection_start) (struct _ScintillaObject *sci, gint position);
	void	(*sci_set_selection_end) (struct _ScintillaObject *sci, gint position);
	void	(*sci_get_text_range) (struct _ScintillaObject *sci, gint start, gint end, gchar *text);
	gchar*	(*sci_get_line) (struct _ScintillaObject *sci, gint line_num);
	gint	(*sci_get_line_length) (struct _ScintillaObject *sci, gint line);
	gint	(*sci_get_line_count) (struct _ScintillaObject *sci);
	gboolean (*sci_get_line_is_visible) (struct _ScintillaObject *sci, gint line);
	void	(*sci_ensure_line_is_visible) (struct _ScintillaObject *sci, gint line);
	void	(*sci_scroll_caret) (struct _ScintillaObject *sci);
	gint	(*sci_find_matching_brace) (struct _ScintillaObject *sci, gint pos);
	gint	(*sci_get_style_at) (struct _ScintillaObject *sci, gint position);
	gchar	(*sci_get_char_at) (struct _ScintillaObject *sci, gint pos);
	gint	(*sci_get_current_line) (struct _ScintillaObject *sci);
	gboolean (*sci_has_selection) (struct _ScintillaObject *sci);
	gint	(*sci_get_tab_width) (struct _ScintillaObject *sci);
	void	(*sci_indicator_clear) (struct _ScintillaObject *sci, gint start, gint end);
	void	(*sci_indicator_set) (struct _ScintillaObject *sci, gint indic);
	gchar*	(*sci_get_contents) (struct _ScintillaObject *sci, gint len);
	gchar*	(*sci_get_contents_range) (struct _ScintillaObject *sci, gint start, gint end);
	gchar*	(*sci_get_selection_contents) (struct _ScintillaObject *sci);
	void	(*sci_set_font) (struct _ScintillaObject *sci, gint style, const gchar *font, gint size);
	gint	(*sci_get_line_end_position) (struct _ScintillaObject *sci, gint line);
	void	(*sci_set_target_start) (struct _ScintillaObject *sci, gint start);
	void	(*sci_set_target_end) (struct _ScintillaObject *sci, gint end);
	gint	(*sci_replace_target) (struct _ScintillaObject *sci, const gchar *text, gboolean regex);
	void	(*sci_set_marker_at_line) (struct _ScintillaObject *sci, gint line_number, gint marker);
	void	(*sci_delete_marker_at_line) (struct _ScintillaObject *sci, gint line_number, gint marker);
	gboolean (*sci_is_marker_set_at_line) (struct _ScintillaObject *sci, gint line, gint marker);
	void 	(*sci_goto_line) (struct _ScintillaObject *sci, gint line, gboolean unfold);
	gint	(*sci_find_text) (struct _ScintillaObject *sci, gint flags, struct Sci_TextToFind *ttf);
	void	(*sci_set_line_indentation) (struct _ScintillaObject *sci, gint line, gint indent);
	gint	(*sci_get_line_indentation) (struct _ScintillaObject *sci, gint line);
	gint	(*sci_get_lexer) (struct _ScintillaObject *sci);
} sci_funcs = {
	&scintilla_send_message,
	&sci_send_command,
	&sci_start_undo_action,
	&sci_end_undo_action,
	&sci_set_text,
	&sci_insert_text,
	&sci_get_text,
	&sci_get_length,
	&sci_get_current_position,
	&sci_set_current_position,
	&sci_get_col_from_position,
	&sci_get_line_from_position,
	&sci_get_position_from_line,
	&sci_replace_sel,
	&sci_get_selected_text,
	&sci_get_selected_text_length,
	&sci_get_selection_start,
	&sci_get_selection_end,
	&sci_get_selection_mode,
	&sci_set_selection_mode,
	&sci_set_selection_start,
	&sci_set_selection_end,
	&sci_get_text_range,
	&sci_get_line,
	&sci_get_line_length,
	&sci_get_line_count,
	&sci_get_line_is_visible,
	&sci_ensure_line_is_visible,
	&sci_scroll_caret,
	&sci_find_matching_brace,
	&sci_get_style_at,
	&sci_get_char_at,
	&sci_get_current_line,
	&sci_has_selection,
	&sci_get_tab_width,
	&sci_indicator_clear,
	&sci_indicator_set,
	&sci_get_contents,
	&sci_get_contents_range,
	&sci_get_selection_contents,
	&sci_set_font,
	&sci_get_line_end_position,
	&sci_set_target_start,
	&sci_set_target_end,
	&sci_replace_target,
	&sci_set_marker_at_line,
	&sci_delete_marker_at_line,
	&sci_is_marker_set_at_line,
	&sci_goto_line,
	&sci_find_text,
	&sci_set_line_indentation,
	&sci_get_line_indentation,
	&sci_get_lexer
};

/* See templates.h */
struct
{
	gchar*		(*templates_get_template_fileheader) (gint filetype_idx, const gchar *fname);
} template_funcs = {
	&templates_get_template_fileheader
};


struct
{
	gboolean	(*utils_str_equal) (const gchar *a, const gchar *b);
	guint		(*utils_string_replace_all) (GString *haystack, const gchar *needle,
				 const gchar *replacement);
	GSList*		(*utils_get_file_list) (const gchar *path, guint *length, GError **error);
	gint		(*utils_write_file) (const gchar *filename, const gchar *text);
	gchar*		(*utils_get_locale_from_utf8) (const gchar *utf8_text);
	gchar*		(*utils_get_utf8_from_locale) (const gchar *locale_text);
	gchar*		(*utils_remove_ext_from_filename) (const gchar *filename);
	gint		(*utils_mkdir) (const gchar *path, gboolean create_parent_dirs);
	gboolean	(*utils_get_setting_boolean) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gboolean default_value);
	gint		(*utils_get_setting_integer) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gint default_value);
	gchar*		(*utils_get_setting_string) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gchar *default_value);
	gboolean	(*utils_spawn_sync) (const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
				 GSpawnChildSetupFunc child_setup, gpointer user_data, gchar **std_out,
				 gchar **std_err, gint *exit_status, GError **error);
	gboolean	(*utils_spawn_async) (const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
				 GSpawnChildSetupFunc child_setup, gpointer user_data, GPid *child_pid,
				 GError **error);
	gint		(*utils_str_casecmp) (const gchar *s1, const gchar *s2);
	gchar*		(*utils_get_date_time) (const gchar *format, time_t *time_to_use);
	void		(*utils_open_browser) (const gchar *uri);
	guint		(*utils_string_replace_first) (GString *haystack, const gchar *needle,
				 const gchar *replace);
	gchar*		(*utils_str_middle_truncate) (const gchar *string, guint truncate_length);
	gchar*		(*utils_str_remove_chars) (gchar *string, const gchar *chars);
	GSList*		(*utils_get_file_list_full)(const gchar *path, gboolean full_path, gboolean sort,
				GError **error);
	gchar**		(*utils_copy_environment)(const gchar **exclude_vars, const gchar *first_varname, ...);
	gchar*		(*utils_find_open_xml_tag) (const gchar sel[], gint size);
	const gchar*	(*utils_find_open_xml_tag_pos) (const gchar sel[], gint size);
} utils_funcs = {
	&utils_str_equal,
	&utils_string_replace_all,
	&utils_get_file_list,
	&utils_write_file,
	&utils_get_locale_from_utf8,
	&utils_get_utf8_from_locale,
	&utils_remove_ext_from_filename,
	&utils_mkdir,
	&utils_get_setting_boolean,
	&utils_get_setting_integer,
	&utils_get_setting_string,
	&utils_spawn_sync,
	&utils_spawn_async,
	&utils_str_casecmp,
	&utils_get_date_time,
	&utils_open_browser,
	&utils_string_replace_first,
	&utils_str_middle_truncate,
	&utils_str_remove_chars,
	&utils_get_file_list_full,
	&utils_copy_environment,
	&utils_find_open_xml_tag,
	&utils_find_open_xml_tag_pos
};


struct
{
	GtkWidget*	(*ui_dialog_vbox_new) (GtkDialog *dialog);
	GtkWidget*	(*ui_frame_new_with_alignment) (const gchar *label_text, GtkWidget **alignment);
	void		(*ui_set_statusbar) (gboolean log, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
	void		(*ui_table_add_row) (GtkTable *table, gint row, ...) G_GNUC_NULL_TERMINATED;
	GtkWidget*	(*ui_path_box_new) (const gchar *title, GtkFileChooserAction action, GtkEntry *entry);
	GtkWidget*	(*ui_button_new_with_image) (const gchar *stock_id, const gchar *text);
	void		(*ui_add_document_sensitive) (GtkWidget *widget);
	void		(*ui_widget_set_tooltip_text) (GtkWidget *widget, const gchar *text);
	GtkWidget*	(*ui_image_menu_item_new) (const gchar *stock_id, const gchar *label);
	GtkWidget*	(*ui_lookup_widget) (GtkWidget *widget, const gchar *widget_name);
	void		(*ui_progress_bar_start) (const gchar *text);
	void		(*ui_progress_bar_stop) (void);
	void		(*ui_entry_add_clear_icon) (GtkEntry *entry);
	void		(*ui_menu_add_document_items) (GtkMenu *menu, struct GeanyDocument *active,
				GCallback callback);
	void		(*ui_widget_modify_font_from_string) (GtkWidget *widget, const gchar *str);
	gboolean	(*ui_is_keyval_enter_or_return) (guint keyval);
	gint		(*ui_get_gtk_settings_integer) (const gchar *property_name, gint default_value);
	void		(*ui_combo_box_add_to_history) (GtkComboBoxText *combo_entry,
				const gchar *text, gint history_len);
	void		(*ui_menu_add_document_items_sorted) (GtkMenu *menu, struct GeanyDocument *active,
				GCallback callback, GCompareFunc compare_func);
	const gchar* (*ui_lookup_stock_label)(const gchar *stock_id);
} uiutils_funcs = {
	&ui_dialog_vbox_new,
	&ui_frame_new_with_alignment,
	&ui_set_statusbar,
	&ui_table_add_row,
	&ui_path_box_new,
	&ui_button_new_with_image,
	&ui_add_document_sensitive,
	&ui_widget_set_tooltip_text,
	&ui_image_menu_item_new,
	&ui_lookup_widget,
	&ui_progress_bar_start,
	&ui_progress_bar_stop,
	&ui_entry_add_clear_icon,
	&ui_menu_add_document_items,
	&ui_widget_modify_font_from_string,
	&ui_is_keyval_enter_or_return,
	&ui_get_gtk_settings_integer,
	&ui_combo_box_add_to_history,
	&ui_menu_add_document_items_sorted,
	&ui_lookup_stock_label
};


struct
{
	gboolean	(*dialogs_show_question) (const gchar *text, ...) G_GNUC_PRINTF (1, 2);
	void		(*dialogs_show_msgbox) (GtkMessageType type, const gchar *text, ...) G_GNUC_PRINTF (2, 3);
	gboolean	(*dialogs_show_save_as) (void);
	gboolean	(*dialogs_show_input_numeric) (const gchar *title, const gchar *label_text,
				 gdouble *value, gdouble min, gdouble max, gdouble step);
	gchar*		(*dialogs_show_input)(const gchar *title, GtkWindow *parent, const gchar *label_text,
				const gchar *default_text);
} dialog_funcs = {
	&dialogs_show_question,
	&dialogs_show_msgbox,
	&dialogs_show_save_as,
	&dialogs_show_input_numeric,
	&dialogs_show_input
};

/* deprecated */
struct
{
	GtkWidget*	(*support_lookup_widget) (GtkWidget *widget, const gchar *widget_name);
} support_funcs = {
	&ui_lookup_widget
};


struct
{
	/* status_add() does not set the status bar - use ui->set_statusbar() instead. */
	void		(*msgwin_status_add) (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
	void		(*msgwin_compiler_add) (gint msg_color, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
	void		(*msgwin_msg_add) (gint msg_color, gint line, struct GeanyDocument *doc,
				 const gchar *format, ...) G_GNUC_PRINTF (4, 5);
	void		(*msgwin_clear_tab) (gint tabnum);
	void		(*msgwin_switch_tab) (gint tabnum, gboolean show);
	void		(*msgwin_set_messages_dir) (const gchar *messages_dir);
} msgwin_funcs = {
	&msgwin_status_add,
	&msgwin_compiler_add,
	&msgwin_msg_add,
	&msgwin_clear_tab,
	&msgwin_switch_tab,
	&msgwin_set_messages_dir
};


struct
{
	gchar*			(*encodings_convert_to_utf8) (const gchar *buffer, gssize size, gchar **used_encoding);
	gchar* 			(*encodings_convert_to_utf8_from_charset) (const gchar *buffer, gssize size,
													 const gchar *charset, gboolean fast);
	const gchar*	(*encodings_get_charset_from_index) (gint idx);
} encoding_funcs = {
	&encodings_convert_to_utf8,
	&encodings_convert_to_utf8_from_charset,
	&encodings_get_charset_from_index
};


struct
{
	void		(*keybindings_send_command) (guint group_id, guint key_id);
	GeanyKeyBinding* (*keybindings_set_item) (GeanyKeyGroup *group, gsize key_id,
					GeanyKeyCallback callback, guint key, GdkModifierType mod,
					const gchar *name, const gchar *label, GtkWidget *menu_item);
	GeanyKeyBinding* (*keybindings_get_item)(GeanyKeyGroup *group, gsize key_id);
} keybindings_funcs = {
	&keybindings_send_command,
	&keybindings_set_item,
	&keybindings_get_item
};


struct
{
	gchar*			(*tm_get_real_path) (const gchar *file_name);
	TMWorkObject*	(*tm_source_file_new) (const char *file_name, gboolean update, const char *name);
	gboolean		(*tm_workspace_add_object) (TMWorkObject *work_object);
	gboolean		(*tm_source_file_update) (TMWorkObject *source_file, gboolean force,
					 gboolean recurse, gboolean update_parent);
	void			(*tm_work_object_free) (gpointer work_object);
	gboolean		(*tm_workspace_remove_object) (TMWorkObject *w, gboolean do_free, gboolean update);
} tagmanager_funcs = {
	&tm_get_real_path,
	&tm_source_file_new,
	&tm_workspace_add_object,
	&tm_source_file_update,
	&tm_work_object_free,
	&tm_workspace_remove_object
};


struct
{
	void		(*search_show_find_in_files_dialog) (const gchar *dir);
} search_funcs = {
	&search_show_find_in_files_dialog
};


struct
{
	const struct GeanyLexerStyle* (*highlighting_get_style) (gint ft_id, gint style_id);
	void		(*highlighting_set_styles) (struct _ScintillaObject *sci, struct GeanyFiletype *ft);
	gboolean	(*highlighting_is_string_style) (gint lexer, gint style);
	gboolean	(*highlighting_is_comment_style) (gint lexer, gint style);
	gboolean	(*highlighting_is_code_style) (gint lexer, gint style);
} highlighting_funcs = {
	&highlighting_get_style,
	&highlighting_set_styles,
	&highlighting_is_string_style,
	&highlighting_is_comment_style,
	&highlighting_is_code_style
};


struct
{
	GeanyFiletype*	(*filetypes_detect_from_file) (const gchar *utf8_filename);
	GeanyFiletype*	(*filetypes_lookup_by_name) (const gchar *name);
	GeanyFiletype*	(*filetypes_index)(gint idx);
	const gchar*	(*filetypes_get_display_name)(GeanyFiletype *ft);
	const GSList*	(*filetypes_get_sorted_by_name)(void);
	/* Remember to convert any filetype_id arguments to GeanyFiletype pointers in any
	 * appended functions */
} filetype_funcs = {
	&filetypes_detect_from_file,
	&filetypes_lookup_by_name,
	&filetypes_index,
	&filetypes_get_display_name,
	&filetypes_get_sorted_by_name
};


struct
{
	gboolean		(*navqueue_goto_line) (struct GeanyDocument *old_doc, struct GeanyDocument *new_doc,
					 gint line);
} navqueue_funcs = {
	&navqueue_goto_line
};


struct
{
	void		(*main_reload_configuration) (void);
	void		(*main_locale_init) (const gchar *locale_dir, const gchar *package);
	gboolean	(*main_is_realized) (void);
} main_funcs = {
	&main_reload_configuration,
	&main_locale_init,
	&main_is_realized
};


struct
{
	StashGroup *(*stash_group_new)(const gchar *name);
	void (*stash_group_add_boolean)(StashGroup *group, gboolean *setting,
			const gchar *key_name, gboolean default_value);
	void (*stash_group_add_integer)(StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value);
	void (*stash_group_add_string)(StashGroup *group, gchar **setting,
			const gchar *key_name, const gchar *default_value);
	void (*stash_group_add_string_vector)(StashGroup *group, gchar ***setting,
			const gchar *key_name, const gchar **default_value);
	void (*stash_group_load_from_key_file)(StashGroup *group, GKeyFile *keyfile);
	void (*stash_group_save_to_key_file)(StashGroup *group, GKeyFile *keyfile);
	void (*stash_group_free)(StashGroup *group);
	gboolean (*stash_group_load_from_file)(StashGroup *group, const gchar *filename);
	gint (*stash_group_save_to_file)(StashGroup *group, const gchar *filename,
			GKeyFileFlags flags);
	void (*stash_group_add_toggle_button)(StashGroup *group, gboolean *setting,
			const gchar *key_name, gboolean default_value, gconstpointer widget_id);
	void (*stash_group_add_radio_buttons)(StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value,
			gconstpointer widget_id, gint enum_id, ...) G_GNUC_NULL_TERMINATED;
	void (*stash_group_add_spin_button_integer)(StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value, gconstpointer widget_id);
	void (*stash_group_add_combo_box)(StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value, gconstpointer widget_id);
	void (*stash_group_add_combo_box_entry)(StashGroup *group, gchar **setting,
			const gchar *key_name, const gchar *default_value, gconstpointer widget_id);
	void (*stash_group_add_entry)(StashGroup *group, gchar **setting,
			const gchar *key_name, const gchar *default_value, gconstpointer widget_id);
	void (*stash_group_add_widget_property)(StashGroup *group, gpointer setting,
			const gchar *key_name, gpointer default_value, gconstpointer widget_id,
			const gchar *property_name, GType type);
	void (*stash_group_display)(StashGroup *group, GtkWidget *owner);
	void (*stash_group_update)(StashGroup *group, GtkWidget *owner);
	void (*stash_group_free_settings)(StashGroup *group);
} stash_funcs = {
	&stash_group_new,
	&stash_group_add_boolean,
	&stash_group_add_integer,
	&stash_group_add_string,
	&stash_group_add_string_vector,
	&stash_group_load_from_key_file,
	&stash_group_save_to_key_file,
	&stash_group_free,
	&stash_group_load_from_file,
	&stash_group_save_to_file,
	&stash_group_add_toggle_button,
	&stash_group_add_radio_buttons,
	&stash_group_add_spin_button_integer,
	&stash_group_add_combo_box,
	&stash_group_add_combo_box_entry,
	&stash_group_add_entry,
	&stash_group_add_widget_property,
	&stash_group_display,
	&stash_group_update,
	&stash_group_free_settings
};


struct
{
	const gchar*	(*symbols_get_context_separator)(gint ft_id);
} symbols_funcs = {
	&symbols_get_context_separator
};

struct
{
	void (*build_activate_menu_item)(const GeanyBuildGroup grp, const guint cmd);
	const gchar *(*build_get_current_menu_item)(const GeanyBuildGroup grp, const guint cmd,
			const GeanyBuildCmdEntries field);
	void (*build_remove_menu_item)(const GeanyBuildSource src, const GeanyBuildGroup grp,
			const gint cmd);
	void (*build_set_menu_item)(const GeanyBuildSource src, const GeanyBuildGroup grp,
			const guint cmd, const GeanyBuildCmdEntries field, const gchar *value);
	guint (*build_get_group_count)(const GeanyBuildGroup grp);
} build_funcs = {
	&build_activate_menu_item,
	&build_get_current_menu_item,
	&build_remove_menu_item,
	&build_set_menu_item,
	&build_get_group_count
};

void *geany_functions[] = {
	&doc_funcs,
	&sci_funcs,
	&template_funcs,
	&utils_funcs,
	&uiutils_funcs,
	&support_funcs,
	&dialog_funcs,
	&msgwin_funcs,
	&encoding_funcs,
	&keybindings_funcs,
	&tagmanager_funcs,
	&search_funcs,
	&highlighting_funcs,
	&filetype_funcs,
	&navqueue_funcs,
	&editor_funcs,
	&main_funcs,
	&plugin_funcs,
	&scintilla_funcs,
	&msgwin_funcs,
	&stash_funcs,
	&symbols_funcs,
	&build_funcs
};

#endif /* HAVE_PLUGINS && !GEANY_DISABLE_DEPRECATED */
