/*
 *      dialogs.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_DIALOGS_H
#define GEANY_DIALOGS_H 1

/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file(void);

/* This shows the file selection dialog to save a file. */
void dialogs_show_save_as();

void dialogs_show_info(const gchar *text, ...);

void dialogs_show_error(const gchar *text, ...);

gboolean dialogs_show_unsaved_file(gint idx);

/* This shows the font selection dialog to choose a font. */
void dialogs_show_open_font(void);

void dialogs_show_word_count(void);

void dialogs_show_color(gchar *colour);

GtkWidget *dialogs_create_build_menu_gen(gint idx);

GtkWidget *dialogs_create_build_menu_tex(gint idx);

void dialogs_show_make_target(void);

void dialogs_show_goto_line(void);

void dialogs_show_includes_arguments_gen(void);

void dialogs_show_includes_arguments_tex(void);

void dialogs_create_recent_menu(void);

GtkWidget *dialogs_add_file_open_extra_widget(void);

void dialogs_show_file_properties(gint idx);

gboolean dialogs_show_question(const gchar *text, ...);

void dialogs_show_keyboard_shortcuts(void);

#endif
