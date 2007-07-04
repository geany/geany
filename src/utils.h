/*
 *      utils.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */


#ifndef GEANY_UTILS_H
#define GEANY_UTILS_H 1

#if ! GLIB_CHECK_VERSION(2, 8, 0)
#define G_GNUC_NULL_TERMINATED
#endif

// Returns: TRUE if ptr points to a non-zero value.
#define NZV(ptr) \
	((ptr) && (ptr)[0])

/* Free's ptr (if not NULL), then assigns result to it.
 * result can be an expression using the 'old' value of ptr.
 * It prevents a memory leak compared with: ptr = func(ptr); */
#define setptr(ptr, result)\
	{\
		gpointer tmp = ptr;\
		ptr = result;\
		g_free(tmp);\
	}


void utils_start_browser(const gchar *uri);

/* taken from anjuta, to determine the EOL mode of the file */
gint utils_get_line_endings(gchar* buffer, glong size);

gboolean utils_isbrace(gchar c);

gboolean utils_is_opening_brace(gchar c);

gint utils_get_local_tag(gint idx, const gchar *qual_name);

gboolean utils_goto_file_line(const gchar *file, gboolean is_tm_filename, gint line);

gboolean utils_goto_line(gint idx, gint line);

gint utils_write_file(const gchar *filename, const gchar *text);

/**
 * (stolen from anjuta and modified)
 * Search backward through size bytes looking for a '<', then return the tag if any
 * @return The tag name
 */
gchar *utils_find_open_xml_tag(const gchar sel[], gint size, gboolean check_tag);

gboolean utils_check_disk_status(gint idx, gboolean force);

//gchar *utils_get_current_tag(gint idx, gint direction);
gint utils_get_current_function(gint idx, const gchar **tagname);

/* returns the end-of-line character(s) length of the specified editor */
gint utils_get_eol_char_len(gint idx);

/* returns the end-of-line character(s) of the specified editor */
gchar *utils_get_eol_char(gint idx);

/* mainly debug function, to get TRUE or FALSE as ascii from a gboolean */
gchar *utils_btoa(gboolean sbool);

gboolean utils_atob(const gchar *str);

void utils_wordcount(gchar *text, guint *chars, guint *lines, guint *words);

gboolean utils_is_absolute_path(const gchar *path);

gdouble utils_scale_round(gdouble val, gdouble factor);

/* (taken from libexo from os-cillation)
 * NULL-safe string comparison. Returns TRUE if both a and b are
 * NULL or if a and b refer to valid strings which are equal.
 */
gboolean utils_str_equal(const gchar *a, const gchar *b);

/* removes the extension from filename and return the result in
 * a newly allocated string */
gchar *utils_remove_ext_from_filename(const gchar *filename);


gchar utils_brace_opposite(gchar ch);

gchar *utils_get_hostname();

gint utils_make_settings_dir(const gchar *dir, const gchar *data_dir, const gchar *doc_dir);

gchar *utils_str_replace(gchar *haystack, const gchar *needle, const gchar *replacement);

gint utils_strpos(const gchar* haystack, const gchar * needle);

gchar *utils_get_date_time();

gchar *utils_get_date();

gchar *utils_get_initials(gchar *name);

gboolean utils_get_setting_boolean(GKeyFile *config, const gchar *section, const gchar *key, const gboolean default_value);

gint utils_get_setting_integer(GKeyFile *config, const gchar *section, const gchar *key, const gint default_value);

gchar *utils_get_setting_string(GKeyFile *config, const gchar *section, const gchar *key, const gchar *default_value);

void utils_switch_document(gint direction);

void utils_replace_filename(gint idx);

gchar *utils_get_hex_from_color(GdkColor *color);

gchar *utils_get_current_file_dir();

void utils_beep();

gchar *utils_make_human_readable_str(unsigned long long size, unsigned long block_size,
									 unsigned long display_unit);

/* utils_strtod() converts a string containing a hex colour ("0x00ff00") into an integer.
 * Basically, it is the same as strtod() would do, but it does not understand hex colour values,
 * before ANSI-C99. With with_route set, it takes strings of the format "#00ff00". */
gint utils_strtod(const gchar *source, gchar **end, gboolean with_route);

// returned string must be freed.
gchar *utils_get_current_time_string();

GIOChannel *utils_set_up_io_channel(gint fd, GIOCondition cond, gboolean nblock,
									GIOFunc func, gpointer data);

gchar **utils_read_file_in_array(const gchar *filename);

/* Contributed by Stefan Oltmanns, thanks.
 * Replaces \\, \r, \n, \t and \uXXX by their real counterparts */
gboolean utils_str_replace_escape(gchar *string);

/* Wraps a string in place, replacing a space with a newline character.
 * wrapstart is the minimum position to start wrapping or -1 for default */
gboolean utils_wrap_string(gchar *string, gint wrapstart);

/* Simple wrapper for g_locale_from_utf8; returns a copy of utf8_text on failure. */
gchar *utils_get_locale_from_utf8(const gchar *utf8_text);

/* Simple wrapper for g_locale_to_utf8; returns a copy of locale_text on failure. */
gchar *utils_get_utf8_from_locale(const gchar *locale_text);


/* Frees all passed pointers if they are *ALL* non-NULL.
 * Do not use if any pointers may be NULL.
 * The first argument is nothing special, it will also be freed.
 * The list must be ended with NULL. */
void utils_free_pointers(gpointer first, ...) G_GNUC_NULL_TERMINATED;

/* Creates a string array deep copy of a series of non-NULL strings.
 * The first argument is nothing special.
 * The list must be ended with NULL.
 * If first is NULL, NULL is returned. */
gchar **utils_strv_new(gchar *first, ...) G_GNUC_NULL_TERMINATED;


gint utils_mkdir(const gchar *path, gboolean create_parent_dirs);

/* Gets a sorted list of files in the specified directory. */
GSList *utils_get_file_list(const gchar *path, guint *length, GError **error);

#endif
