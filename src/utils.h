/*
 *      utils.h - this file is part of Geany, a fast and lightweight IDE
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

/**
 *  @file: utils.h
 *  General utility functions, non-GTK related.
 */

#ifndef GEANY_UTILS_H
#define GEANY_UTILS_H 1

G_BEGIN_DECLS

#include <time.h>


/** Returns TRUE if @a ptr points to a non-zero value. */
#define NZV(ptr) \
	((ptr) && (ptr)[0])

/** Assigns @a result to @a ptr, then frees the old value.
 * @a result can be an expression using the 'old' value of @a ptr.
 * E.g. @code SETPTR(str, g_strndup(str, 5)); @endcode
 **/
#define SETPTR(ptr, result) \
	do setptr(ptr, result) while (0)

/** @deprecated 2011/11/15 - use SETPTR() instead. */
#define setptr(ptr, result) \
	{\
		gpointer setptr_tmp = ptr;\
		ptr = result;\
		g_free(setptr_tmp);\
	}

/** Duplicates a string on the stack using @c g_alloca().
 * Like glibc's @c strdupa(), but portable.
 * @note You must include @c string.h yourself.
 * @warning Don't use excessively or for long strings otherwise there may be stack exhaustion -
 *          see the GLib docs for @c g_alloca(). */
#define utils_strdupa(str) \
	strcpy(g_alloca(strlen(str) + 1), str)

/* Get a keyfile setting, using the home keyfile if the key exists,
 * otherwise system keyfile. */
#define utils_get_setting(type, home, sys, group, key, default_val)\
	(g_key_file_has_key(home, group, key, NULL)) ?\
		utils_get_setting_##type(home, group, key, default_val) :\
		utils_get_setting_##type(sys, group, key, default_val)

/* ignore the case of filenames and paths under WIN32, causes errors if not */
#ifdef G_OS_WIN32
#define utils_filenamecmp(a, b)	utils_str_casecmp((a), (b))
#else
#define utils_filenamecmp(a, b)	strcmp((a), (b))
#endif


/** Iterates all the items in @a array using pointers.
 * @param item pointer to an item in @a array.
 * @param array C array to traverse.
 * @param len Length of the array. */
#define foreach_c_array(item, array, len) \
	for (item = array; item < &array[len]; item++)

/** Iterates all items in @a array.
 * @param type Type of @a item.
 * @param item pointer to item in @a array.
 * @param array @c GArray to traverse. */
#define foreach_array(type, item, array) \
	foreach_c_array(item, ((type*)(gpointer)array->data), array->len)

/** Iterates all the pointers in @a ptr_array.
 * @param item pointer in @a ptr_array.
 * @param idx @c guint index into @a ptr_array.
 * @param ptr_array @c GPtrArray to traverse. */
#define foreach_ptr_array(item, idx, ptr_array) \
	for (idx = 0, item = ((ptr_array)->len > 0 ? g_ptr_array_index((ptr_array), 0) : NULL); \
		idx < (ptr_array)->len; ++idx, item = g_ptr_array_index((ptr_array), idx))

/** Iterates all the nodes in @a list.
 * @param node should be a (@c GList*).
 * @param list @c GList to traverse. */
#define foreach_list(node, list) \
	for (node = list; node != NULL; node = node->next)

/** Iterates all the nodes in @a list.
 * @param node should be a (@c GSList*).
 * @param list @c GSList to traverse. */
#define foreach_slist(node, list) \
	foreach_list(node, list)

/** Iterates through each unsorted filename in a @c GDir.
 * @param filename (@c const @c gchar*) locale-encoded filename, without path. Do not modify or free.
 * @param dir @c GDir created with @c g_dir_open(). Call @c g_dir_close() afterwards.
 * @see utils_get_file_list() if you want a sorted list.
 * @since Geany 0.19. */
#define foreach_dir(filename, dir)\
	for ((filename) = g_dir_read_name(dir); (filename) != NULL; (filename) = g_dir_read_name(dir))

/** Iterates through each character in @a string.
 * @param char_ptr Pointer to character.
 * @param string String to traverse.
 * @warning Doesn't include null terminating character.
 * @since Geany 0.19. */
#define foreach_str(char_ptr, string) \
	for (char_ptr = string; *char_ptr; char_ptr++)

/** Iterates a null-terminated string vector.
 * @param str_ptr @c gchar** pointer to string element.
 * @param strv Can be @c NULL.
 * @since Geany 0.20 */
#define foreach_strv(str_ptr, strv)\
	if (!(strv)) {} else foreach_str(str_ptr, strv)

/** Iterates from 0 to @a size.
 * @param i Integer.
 * @param size Number of iterations.
 * @since Geany 0.20. */
#define foreach_range(i, size) \
	for (i = 0; i < size; i++)


void utils_open_browser(const gchar *uri);

gint utils_get_line_endings(const gchar* buffer, gsize size);

gboolean utils_isbrace(gchar c, gboolean include_angles);

gboolean utils_is_opening_brace(gchar c, gboolean include_angles);

gint utils_write_file(const gchar *filename, const gchar *text);

gchar *utils_find_open_xml_tag(const gchar sel[], gint size);

const gchar *utils_find_open_xml_tag_pos(const gchar sel[], gint size);

gboolean utils_is_short_html_tag(const gchar *tag_name);

void utils_ensure_same_eol_characters(GString *string, gint target_eol_mode);

const gchar *utils_get_eol_char(gint eol_mode);

const gchar *utils_get_eol_name(gint eol_mode);

gboolean utils_atob(const gchar *str);

void utils_tidy_path(gchar *filename);

gboolean utils_is_absolute_path(const gchar *path);

const gchar *utils_path_skip_root(const gchar *path);

gdouble utils_scale_round(gdouble val, gdouble factor);

gboolean utils_str_equal(const gchar *a, const gchar *b);

gchar *utils_remove_ext_from_filename(const gchar *filename);

gchar utils_brace_opposite(gchar ch);

gchar *utils_get_hostname(void);

gint utils_string_find(GString *haystack, gint start, gint end, const gchar *needle);

gint utils_string_replace(GString *str, gint pos, gint len, const gchar *replace);

guint utils_string_replace_all(GString *haystack, const gchar *needle, const gchar *replace);

guint utils_string_replace_first(GString *haystack, const gchar *needle, const gchar *replace);

guint utils_string_regex_replace_all(GString *haystack, GRegex *regex,
		guint match_num, const gchar *replace, gboolean literal);

void utils_str_replace_all(gchar **haystack, const gchar *needle, const gchar *replacement);

gint utils_strpos(const gchar* haystack, const gchar *needle);

gchar *utils_get_date_time(const gchar *format, time_t *time_to_use);

gchar *utils_get_initials(const gchar *name);

gboolean utils_get_setting_boolean(GKeyFile *config, const gchar *section, const gchar *key, const gboolean default_value);

gint utils_get_setting_integer(GKeyFile *config, const gchar *section, const gchar *key, const gint default_value);

gchar *utils_get_setting_string(GKeyFile *config, const gchar *section, const gchar *key, const gchar *default_value);

gchar *utils_get_hex_from_color(GdkColor *color);

guint utils_invert_color(guint color);

const gchar *utils_get_default_dir_utf8(void);

gchar *utils_get_current_file_dir_utf8(void);

void utils_beep(void);

gchar *utils_make_human_readable_str(guint64 size, gulong block_size,
									 gulong display_unit);

gint utils_strtod(const gchar *source, gchar **end, gboolean with_route);

gchar *utils_get_current_time_string(void);

GIOChannel *utils_set_up_io_channel(gint fd, GIOCondition cond, gboolean nblock,
									GIOFunc func, gpointer data);

gchar **utils_read_file_in_array(const gchar *filename);

gboolean utils_str_replace_escape(gchar *string, gboolean keep_backslash);

gboolean utils_wrap_string(gchar *string, gint wrapstart);

gchar *utils_get_locale_from_utf8(const gchar *utf8_text);

gchar *utils_get_utf8_from_locale(const gchar *locale_text);

void utils_free_pointers(gsize arg_count, ...) G_GNUC_NULL_TERMINATED;

gchar **utils_strv_new(const gchar *first, ...) G_GNUC_NULL_TERMINATED;

gchar **utils_strv_join(gchar **first, gchar **second) G_GNUC_WARN_UNUSED_RESULT;

gint utils_mkdir(const gchar *path, gboolean create_parent_dirs);

GSList *utils_get_file_list(const gchar *path, guint *length, GError **error);

GSList *utils_get_file_list_full(const gchar *path, gboolean full_path, gboolean sort, GError **error);

GSList *utils_get_config_files(const gchar *subdir);

gchar *utils_get_help_url(const gchar *suffix);

gboolean utils_str_has_upper(const gchar *str);

gint utils_is_file_writable(const gchar *locale_filename);


gboolean utils_spawn_sync(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
						  GSpawnChildSetupFunc child_setup, gpointer user_data, gchar **std_out,
						  gchar **std_err, gint *exit_status, GError **error);

gboolean utils_spawn_async(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
						   GSpawnChildSetupFunc child_setup, gpointer user_data, GPid *child_pid,
						   GError **error);

gint utils_str_casecmp(const gchar *s1, const gchar *s2);

gchar *utils_get_path_from_uri(const gchar *uri);

gboolean utils_is_uri(const gchar *uri);

gboolean utils_is_remote_path(const gchar *path);

gchar *utils_str_middle_truncate(const gchar *string, guint truncate_length);

gchar *utils_str_remove_chars(gchar *string, const gchar *chars);

gchar **utils_copy_environment(const gchar **exclude_vars, const gchar *first_varname, ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif
