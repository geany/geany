/*
 *      utils.h - this file is part of Geany, a fast and lightweight IDE
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

/**
 *  @file: utils.h
 *  General utility functions, non-GTK related.
 */

#ifndef GEANY_UTILS_H
#define GEANY_UTILS_H 1

/** Returns: TRUE if @a ptr points to a non-zero value. */
#define NZV(ptr) \
	((ptr) && (ptr)[0])

/**
 *  Free's @a ptr (if not @c NULL), then assigns @a result to it.
 *  @a result can be an expression using the 'old' value of @a ptr.
 *  It prevents a memory leak compared with: @code ptr = func(ptr); @endcode
 **/
#define setptr(ptr, result) \
	{\
		gpointer setptr_tmp = ptr;\
		ptr = result;\
		g_free(setptr_tmp);\
	}


void utils_start_browser(const gchar *uri);

gint utils_get_line_endings(const gchar* buffer, glong size);

gboolean utils_isbrace(gchar c, gboolean include_angles);

gboolean utils_is_opening_brace(gchar c, gboolean include_angles);

gint utils_write_file(const gchar *filename, const gchar *text);

gchar *utils_find_open_xml_tag(const gchar sel[], gint size, gboolean check_tag);

const gchar *utils_get_eol_name(gint eol_mode);

gboolean utils_atob(const gchar *str);

gboolean utils_is_absolute_path(const gchar *path);

gdouble utils_scale_round(gdouble val, gdouble factor);

gboolean utils_str_equal(const gchar *a, const gchar *b);

gchar *utils_remove_ext_from_filename(const gchar *filename);

gchar utils_brace_opposite(gchar ch);

gchar *utils_get_hostname(void);

gboolean utils_string_replace_all(GString *str, const gchar *needle, const gchar *replace);

gchar *utils_str_replace(gchar *haystack, const gchar *needle, const gchar *replacement);

gint utils_strpos(const gchar* haystack, const gchar *needle);

gchar *utils_get_date_time(const gchar *format, time_t *time_to_use);

gchar *utils_get_initials(const gchar *name);

gboolean utils_get_setting_boolean(GKeyFile *config, const gchar *section, const gchar *key, const gboolean default_value);

gint utils_get_setting_integer(GKeyFile *config, const gchar *section, const gchar *key, const gint default_value);

gchar *utils_get_setting_string(GKeyFile *config, const gchar *section, const gchar *key, const gchar *default_value);

gchar *utils_get_hex_from_color(GdkColor *color);

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

gboolean utils_str_replace_escape(gchar *string);

gboolean utils_wrap_string(gchar *string, gint wrapstart);

gchar *utils_get_locale_from_utf8(const gchar *utf8_text);

gchar *utils_get_utf8_from_locale(const gchar *locale_text);

void utils_free_pointers(gpointer first, ...) G_GNUC_NULL_TERMINATED;

gchar **utils_strv_new(const gchar *first, ...) G_GNUC_NULL_TERMINATED;

gint utils_mkdir(const gchar *path, gboolean create_parent_dirs);

GSList *utils_get_file_list(const gchar *path, guint *length, GError **error);

gboolean utils_str_has_upper(const gchar *str);

gint utils_is_file_writeable(const gchar *locale_filename);


gboolean utils_spawn_sync(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
						  GSpawnChildSetupFunc child_setup, gpointer user_data, gchar **std_out,
						  gchar **std_err, gint *exit_status, GError **error);

gboolean utils_spawn_async(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
						   GSpawnChildSetupFunc child_setup, gpointer user_data, GPid *child_pid,
						   GError **error);

gint utils_str_casecmp(const gchar *s1, const gchar *s2);

#endif
