/*
 *      utils.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#ifndef GEANY_UTILS_H
#define GEANY_UTILS_H 1


void utils_start_browser(gchar *uri);

void utils_update_statusbar(gint idx);

void utils_set_buttons_state(gboolean enable);

void utils_update_popup_reundo_items(gint idx);

void utils_update_popup_copy_items(gint idx);

void utils_update_insert_include_item(gint idx, gint item);

void utils_update_menu_copy_items(gint idx);

void utils_update_popup_goto_items(gboolean enable);

void utils_save_buttons_toggle(gboolean enable);

void utils_close_buttons_toggle(void);

/* taken from anjuta, to determine the EOL mode of the file */
gint utils_get_line_endings(gchar* buffer, glong size);

gboolean utils_isbrace(gchar c);

/* This sets the window title according to the current filename. */
void utils_set_window_title(gint index);

void utils_set_font(void);

const GList *utils_get_tag_list(gint idx, guint tag_types);

gint utils_get_local_tag(gint idx, const gchar *qual_name);

gboolean utils_goto_workspace_tag(const gchar *file, gint line);

gboolean utils_goto_line(gint idx, gint line);

GdkPixbuf *utils_new_pixbuf_from_inline(gint img, gboolean small_img);

GtkWidget *utils_new_image_from_inline(gint img, gboolean small_img);

gint utils_write_file(const gchar *filename, const gchar *text);

void utils_show_indention_guides(void);

void utils_show_white_space(void);

void utils_line_breaking(void);

void utils_show_markers_margin(void);

void utils_show_line_endings(void);

void utils_set_fullscreen(void);

GtkFileFilter *utils_create_file_filter(filetype *ft);

void utils_update_tag_list(gint idx, gboolean update);

gchar *utils_convert_to_utf8(const gchar *buffer, gsize size, gchar **used_encoding);

gchar *utils_convert_to_utf8_simple(const gchar *str);

gchar *utils_convert_to_utf8_from_charset(const gchar *buffer, gsize size, const gchar *charset);

/**
 * (stolen from anjuta and modified)
 * Search backward through size bytes looking for a '<', then return the tag if any
 * @return The tag name
 */
gchar *utils_find_open_xml_tag(const gchar sel[], gint size, gboolean check_tag);

gboolean utils_check_disk_status(gint idx, const gboolean bForce);

//gchar *utils_get_current_tag(gint idx, gint direction);
gint utils_get_current_tag(gint idx, gchar **tagname);

void utils_find_current_word(ScintillaObject *sci, gint pos, gchar *word);

/* returns the end-of-line character(s) length of the specified editor */
gint utils_get_eol_char_len(gint idx);

/* returns the end-of-line character(s) of the specified editor */
gchar *utils_get_eol_char(gint idx);

/* mainly debug function, to get TRUE or FALSE as ascii from a gboolean */
gchar *utils_btoa(gboolean sbool);

gboolean utils_atob(const gchar *str);

/* replaces all occurrences of c in source with d,
 * source must be null-terminated */
void utils_replace_c(gchar*, gchar, gchar);

void utils_wordcount(gchar *text, guint *chars, guint *lines, guint *words);

gboolean utils_is_absolute_path(const gchar *path);

void utils_ensure_final_newline(gint idx);

void utils_strip_trailing_spaces(gint idx);

gdouble utils_scale_round(gdouble val, gdouble factor);

void utils_widget_show_hide(GtkWidget *widget, gboolean show);

void utils_build_show_hide(gint);

/* (taken from libexo from os-cillation)
 * NULL-safe string comparison. Returns TRUE if both a and b are
 * NULL or if a and b refer to valid strings which are equal.
 */
gboolean utils_strcmp(const gchar *a, const gchar *b);

/* removes the extension from filename and return the result in
 * a newly allocated string */
gchar *utils_remove_ext_from_filename(const gchar *filename);


/* Finds a corresponding matching brace to the given pos
 * (this is taken from Scintilla Editor.cxx,
 * fit to work with sci_cb_close_block) */
gint utils_brace_match(ScintillaObject *sci, gint pos);


gchar utils_brace_opposite(gchar ch);

void utils_replace_tabs(gint idx);

GList *utils_glist_from_string(const gchar *string);

void utils_glist_strings_free(GList * list);

gchar *utils_get_hostname(void);

gint utils_make_settings_dir(void);

gchar *utils_str_replace(gchar *haystack, const gchar *needle, const gchar *replacement);

gint utils_strpos(const gchar* haystack, const gchar * needle);

gchar *utils_get_date_time(void);

gchar *utils_get_date(void);

void utils_create_insert_menu_items(void);

gchar *utils_get_initials(gchar *name);

void utils_update_toolbar_icons(GtkIconSize size);

void utils_free_ptr_array(gchar *array[], gint len);

void utils_update_recent_menu(void);

gboolean utils_get_setting_boolean(GKeyFile *config, const gchar *section, const gchar *key, const gboolean default_value);

gint utils_get_setting_integer(GKeyFile *config, const gchar *section, const gchar *key, const gint default_value);

gchar *utils_get_setting_string(GKeyFile *config, const gchar *section, const gchar *key, const gchar *default_value);

void utils_switch_document(gint direction);

void utils_replace_filename(gint idx);

gint utils_compare_symbol(const GeanySymbol *a, const GeanySymbol *b);

gchar *utils_get_hex_from_color(GdkColor *color);

gint utils_get_int_from_hexcolor(const gchar *hex);

void utils_treeviews_showhide(void);

gchar *utils_get_current_file_dir();

void utils_beep(void);

#endif
