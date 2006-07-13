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
	COLOR_BLACK,
	COLOR_BLUE
};

enum
{
	MSG_STATUS = 0,
	MSG_COMPILER,
	MSG_MESSAGE,
	MSG_SCRATCH,
	MSG_VTE
};


typedef struct msgwin
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
} msgwin;

msgwin msgwindow;


void msgwin_prepare_msg_tree_view(void);

void msgwin_prepare_status_tree_view(void);

void msgwin_prepare_compiler_tree_view(void);

void msgwin_msg_add(gint line, gint idx, gchar *string);

void msgwin_compiler_add(gint msg_color, gboolean scroll, gchar const *format, ...);

void msgwin_status_add(gchar const *format, ...);

GtkWidget *msgwin_create_message_popup_menu(gint type);

gboolean msgwin_goto_compiler_file_line();

gboolean msgwin_goto_messages_file_line();

#endif
