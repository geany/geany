/*
 *      msgwindow.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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


#ifndef GEANY_MSGWINDOW_H
#define GEANY_MSGWINDOW_H 1


enum
{
	COLOR_RED,
	COLOR_DARK_RED,
	COLOR_BLACK,
	COLOR_BLUE
};



typedef struct
{
	GtkListStore	*store_status;
	GtkListStore	*store_msg;
	GtkListStore	*store_compiler;
	GtkWidget		*tree_compiler;
	GtkWidget		*tree_status;
	GtkWidget		*tree_msg;
	GtkWidget		*popup_status_menu;
	GtkWidget		*popup_msg_menu;
	GtkWidget		*popup_compiler_menu;
	GtkWidget		*notebook;
	gchar			*find_in_files_dir;
} MessageWindow;

extern MessageWindow msgwindow;


void msgwin_init();

void msgwin_finalize();

void msgwin_prepare_msg_tree_view(void);

void msgwin_prepare_status_tree_view(void);

void msgwin_prepare_compiler_tree_view(void);

void msgwin_show();

void msgwin_msg_add(gint line, gint idx, gchar *string);

void msgwin_compiler_add(gint msg_color, gboolean scroll, gchar const *format, ...);

void msgwin_status_add(gchar const *format, ...);

GtkWidget *msgwin_create_message_popup_menu(gint type);

gboolean msgwin_goto_compiler_file_line();

/* try to parse the file and line number where the error occured described in line
 * and when something useful is found, it stores the line number in *line and the
 * relevant file with the error in filename.
 * *line will be -1 if no error was found in string.
 * filename must be freed unless it is NULL. */
void msgwin_parse_compiler_error_line(const gchar *string, gchar **filename, gint *line);

gboolean msgwin_goto_messages_file_line();

#endif
