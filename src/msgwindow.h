/*
 *      msgwindow.h - this file is part of Geany, a fast and lightweight IDE
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

enum
{
	MSG_STATUS = 0, // force it to start at 0 to keep in sync with the notebook page numbers
	MSG_COMPILER,
	MSG_MESSAGE,
	MSG_SCRATCH,
	MSG_VTE,
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

void msgwin_show_hide(gboolean show);

void msgwin_msg_add(gint line, gint idx, const gchar *string);

void msgwin_compiler_add(gint msg_color, const gchar *format, ...)
		G_GNUC_PRINTF (2, 3);

void msgwin_status_add(const gchar *format, ...) G_GNUC_PRINTF (1, 2);

void msgwin_menu_add_common_items(GtkMenu *menu);

gboolean msgwin_goto_compiler_file_line();

void msgwin_parse_compiler_error_line(const gchar *string, const gchar *dir, gchar **filename, gint *line);

gboolean msgwin_goto_messages_file_line();

#endif
