/*
 *      prefs.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#ifndef GEANY_PREFS_H
#define GEANY_PREFS_H 1

void prefs_init_dialog(void);

void on_prefs_button_clicked(GtkDialog *dialog, gint response, gpointer user_data);

void on_prefs_font_choosed(GtkFontButton *widget, gpointer user_data);

void on_prefs_color_choosed(GtkColorButton *widget, gpointer user_data);

void dialogs_show_prefs_dialog(void);

void on_prefs_tools_button_clicked(GtkButton *button, gpointer user_data);

#endif
