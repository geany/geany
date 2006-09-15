/*
 *      ui_utils.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
 *      Copyright 2006 Nick Treleaven <nick.treleaven@btinternet.com>
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

#ifndef GEANY_UI_UTILS_H
#define GEANY_UI_UTILS_H 1

/* allow_override is TRUE if text can be ignored when another message has been set
 * that didn't use allow_override and has not timed out. */
void ui_set_statusbar(const gchar *text, gboolean allow_override);

void ui_update_statusbar(gint idx, gint pos);


/* This sets the window title according to the current filename. */
void ui_set_window_title(gint index);

void ui_set_editor_font(const gchar *font_name);

void ui_set_fullscreen();


void ui_update_tag_list(gint idx, gboolean update);


void ui_update_popup_reundo_items(gint idx);

void ui_update_popup_copy_items(gint idx);

void ui_update_popup_goto_items(gboolean enable);


void ui_update_menu_copy_items(gint idx);

void ui_update_insert_include_item(gint idx, gint item);

void ui_update_fold_items();


void ui_create_insert_menu_items();

void ui_create_insert_date_menu_items();


void ui_save_buttons_toggle(gboolean enable);

void ui_close_buttons_toggle();


void ui_widget_show_hide(GtkWidget *widget, gboolean show);

void ui_build_show_hide(gint);

void ui_treeviews_show_hide(gboolean force);

void ui_document_show_hide(gint idx);


void ui_update_toolbar_icons(GtkIconSize size);

void ui_update_toolbar_items();


GdkPixbuf *ui_new_pixbuf_from_inline(gint img, gboolean small_img);

GtkWidget *ui_new_image_from_inline(gint img, gboolean small_img);


void ui_create_recent_menu();

void ui_add_recent_file(const gchar *utf8_filename);


void ui_show_markers_margin();

void ui_show_linenumber_margin();


GtkContainer *ui_frame_new(GtkContainer *parent, const gchar *label_text);

#endif
