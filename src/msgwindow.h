/*
 *      msgwindow.h - this file is part of Geany, a fast and lightweight IDE
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

#ifndef GEANY_MSGWINDOW_H
#define GEANY_MSGWINDOW_H 1

#include "document.h"

#include "gtkcompat.h"


G_BEGIN_DECLS

/**
 * Various colors for use in the compiler and messages treeviews when adding messages.
 **/
enum MsgColors
{
	COLOR_RED,		/**< Color red */
	COLOR_DARK_RED,	/**< Color dark red */
	COLOR_BLACK,	/**< Color black */
	COLOR_BLUE		/**< Color blue */
};

/** Indices of the notebooks in the messages window. */
typedef enum
{
	/* force it to start at 0 to keep in sync with the notebook page numbers */
	MSG_STATUS = 0,	/**< Index of the status message tab */
	MSG_COMPILER,	/**< Index of the compiler tab */
	MSG_MESSAGE,	/**< Index of the messages tab */
	MSG_SCRATCH,	/**< Index of the scratch tab */
	MSG_VTE			/**< Index of the VTE tab */
} MessageWindowTabNum;


void msgwin_status_add(const gchar *format, ...) G_GNUC_PRINTF (1, 2);

void msgwin_compiler_add(gint msg_color, const gchar *format, ...) G_GNUC_PRINTF (2, 3);

void msgwin_msg_add(gint msg_color, gint line, GeanyDocument *doc, const gchar *format, ...)
			G_GNUC_PRINTF (4, 5);

void msgwin_clear_tab(gint tabnum);

void msgwin_switch_tab(gint tabnum, gboolean show);

void msgwin_set_messages_dir(const gchar *messages_dir);


#ifdef GEANY_PRIVATE

typedef struct
{
	GtkListStore	*store_status;
	GtkListStore	*store_msg;
	GtkListStore	*store_compiler;
	GtkWidget		*tree_compiler;
	GtkWidget		*tree_status;
	GtkWidget		*tree_msg;
	GtkWidget		*scribble;
	GtkWidget		*popup_status_menu;
	GtkWidget		*popup_msg_menu;
	GtkWidget		*popup_compiler_menu;
	GtkWidget		*notebook;
	gchar			*messages_dir;
} MessageWindow;

extern MessageWindow msgwindow;


void msgwin_init(void);

void msgwin_finalize(void);

void msgwin_show_hide(gboolean show);

void msgwin_msg_add_string(gint msg_color, gint line, GeanyDocument *doc, const gchar *string);

void msgwin_compiler_add_string(gint msg_color, const gchar *msg);

void msgwin_show_hide_tabs(void);


void msgwin_menu_add_common_items(GtkMenu *menu);

gboolean msgwin_goto_compiler_file_line(gboolean focus_editor);

void msgwin_parse_compiler_error_line(const gchar *string, const gchar *dir,
									  gchar **filename, gint *line);

gboolean msgwin_goto_messages_file_line(gboolean focus_editor);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_MSGWINDOW_H */
