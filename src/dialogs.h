/*
 *      dialogs.h - this file is part of Geany, a fast and lightweight IDE
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

/**
 *  @file dialogs.h
 *  File related dialogs, miscellaneous dialogs, font dialog.
 **/


#ifndef GEANY_DIALOGS_H
#define GEANY_DIALOGS_H 1

#include "document.h"

#include "gtkcompat.h"

G_BEGIN_DECLS

gboolean dialogs_show_question(const gchar *text, ...) G_GNUC_PRINTF (1, 2);

void dialogs_show_msgbox(GtkMessageType type, const gchar *text, ...) G_GNUC_PRINTF (2, 3);

gboolean dialogs_show_save_as(void);

gboolean dialogs_show_input_numeric(const gchar *title, const gchar *label_text,
	gdouble *value, gdouble min, gdouble max, gdouble step);

gchar *dialogs_show_input(const gchar *title, GtkWindow *parent,
	const gchar *label_text, const gchar *default_text);


#ifdef GEANY_PRIVATE

typedef void (*GeanyInputCallback)(const gchar *text, gpointer data);


void dialogs_show_open_file(void);

gboolean dialogs_show_unsaved_file(GeanyDocument *doc);

void dialogs_show_open_font(void);

void dialogs_show_word_count(void);

void dialogs_show_color(gchar *colour);

gchar *dialogs_show_input_goto_line(const gchar *title, GtkWindow *parent,
	const gchar *label_text, const gchar *default_text);

GtkWidget *dialogs_show_input_persistent(const gchar *title, GtkWindow *parent,
	const gchar *label_text, const gchar *default_text, GeanyInputCallback input_cb, gpointer input_cb_data);

void dialogs_show_file_properties(GeanyDocument *doc);

gboolean dialogs_show_question_full(GtkWidget *parent, const gchar *yes_btn, const gchar *no_btn,
	const gchar *extra_text, const gchar *main_text, ...) G_GNUC_PRINTF (5, 6);

gint dialogs_show_prompt(GtkWidget *parent,
		const gchar *btn_1, GtkResponseType response_1,
		const gchar *btn_2, GtkResponseType response_2,
		const gchar *btn_3, GtkResponseType response_3,
		const gchar *extra_text, const gchar *main_text, ...) G_GNUC_PRINTF (9, 10);

void dialogs_show_msgbox_with_secondary(GtkMessageType type, const gchar *text, const gchar *secondary);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_DIALOGS_H */
