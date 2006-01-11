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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#include "geany.h"

#ifndef GEANY_DIALOGS_H
#define GEANY_DIALOGS_H 1

typedef struct
{
	GtkWidget		*menu_c;
	GtkWidget		*menu_tex;
	GtkWidget		*menu_misc;
} build_menus;
build_menus dialogs_build_menus;


/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file (void);

/* This shows the file selection dialog to save a file. */
void dialogs_show_save_as ();

void dialogs_show_file_open_error(void);

gboolean dialogs_show_not_found(const gchar *text);

void dialogs_show_info(const gchar *text, ...);

void dialogs_show_error(const gchar *text, ...);

gboolean dialogs_show_reload_warning(const gchar *text);

gboolean dialogs_show_confirm_exit(void);

gboolean dialogs_show_unsaved_file(gint idx);

/* This shows the font selection dialog to choose a font. */
void dialogs_show_open_font(void);

void dialogs_show_about(void);

void dialogs_show_word_count(void);

void dialogs_show_color(void);

GtkWidget *dialogs_create_build_menu_gen(gboolean link, gboolean execute);

GtkWidget *dialogs_create_build_menu_tex(void);

void dialogs_show_make_target(void);

void dialogs_show_find(void);

void dialogs_show_replace(void);

void dialogs_show_goto_line(void);

void dialogs_show_includes_arguments_gen(gboolean link);

void dialogs_show_includes_arguments_tex(void);

void dialogs_create_recent_menu(void);

GtkWidget *dialogs_add_file_open_extra_widget(void);

gboolean dialogs_show_mkcfgdir_error(gint error_nr);

#endif
