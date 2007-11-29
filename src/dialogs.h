/*
 *      dialogs.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_DIALOGS_H
#define GEANY_DIALOGS_H 1

/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file();

/* This shows the file selection dialog to save a file. */
gboolean dialogs_show_save_as();

gboolean dialogs_show_unsaved_file(gint idx);

/* This shows the font selection dialog to choose a font. */
void dialogs_show_open_font();

void dialogs_show_word_count();

void dialogs_show_color(gchar *colour);

void dialogs_show_input(const gchar *title, const gchar *label_text, const gchar *default_text,
						GCallback cb_dialog, GCallback cb_entry);

void dialogs_show_goto_line();

void dialogs_show_file_properties(gint idx);

gboolean dialogs_show_question(const gchar *text, ...) G_GNUC_PRINTF (1, 2);

/* extra_text can be NULL; otherwise it is displayed below main_text. */
gboolean dialogs_show_question_full(GtkWidget *parent, const gchar *yes_btn, const gchar *no_btn,
	const gchar *extra_text, const gchar *main_text, ...) G_GNUC_PRINTF (5, 6);

void dialogs_show_msgbox(gint type, const gchar *text, ...) G_GNUC_PRINTF (2, 3);

void dialogs_show_msgbox_with_secondary(gint type, const gchar *text, const gchar *secondary);

#endif
