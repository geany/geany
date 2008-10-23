/*
 *      dialogs.h - this file is part of Geany, a fast and lightweight IDE
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
 *  @file dialogs.h
 *  File related dialogs, miscellaneous dialogs, font dialog.
 **/


#ifndef GEANY_DIALOGS_H
#define GEANY_DIALOGS_H 1

typedef void (*InputCallback)(const gchar *);


void dialogs_show_open_file(void);

gboolean dialogs_show_save_as(void);

gboolean dialogs_show_unsaved_file(GeanyDocument *doc);

void dialogs_show_open_font(void);

void dialogs_show_word_count(void);

void dialogs_show_color(gchar *colour);

GtkWidget *dialogs_show_input(const gchar *title, const gchar *label_text,
	const gchar *default_text, gboolean persistent, InputCallback input_cb);

gboolean dialogs_show_input_numeric(const gchar *title, const gchar *label_text,
									gdouble *value, gdouble min, gdouble max, gdouble step);

void dialogs_show_file_properties(GeanyDocument *doc);

gboolean dialogs_show_question(const gchar *text, ...) G_GNUC_PRINTF (1, 2);

gboolean dialogs_show_question_full(GtkWidget *parent, const gchar *yes_btn, const gchar *no_btn,
	const gchar *extra_text, const gchar *main_text, ...) G_GNUC_PRINTF (5, 6);

void dialogs_show_msgbox(gint type, const gchar *text, ...) G_GNUC_PRINTF (2, 3);

void dialogs_show_msgbox_with_secondary(gint type, const gchar *text, const gchar *secondary);

#endif
