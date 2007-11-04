/*
 *      win32.h - this file is part of Geany, a fast and lightweight IDE
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
 *  $Id$
 */


#ifdef G_OS_WIN32


void win32_show_pref_file_dialog(GtkEntry *item);

gboolean win32_show_file_dialog(gboolean file_open);

void win32_show_font_dialog(void);

void win32_show_color_dialog(const gchar *colour);

/* Creates a native Windows message box of the given type and returns always TRUE
 * or FALSE representing th pressed Yes or No button.
 * If type is not GTK_MESSAGE_QUESTION, it returns always TRUE. */
gboolean win32_message_dialog(GtkWidget *parent, GtkMessageType type, const gchar *msg);

/* Special dialog to ask for an action when closing an unsaved file */
gint win32_message_dialog_unsaved(const gchar *msg);

/* Just a simple wrapper function to open a browser window */
void win32_open_browser(const gchar *uri);

/* Shows a file open dialog.
 * If allow_new_file is set, the file to be opened doesn't have to exist.
 * The selected file name is returned. */
gchar *win32_show_project_open_dialog(const gchar *title, const gchar *initial_dir, gboolean allow_new_file);

/* Shows a folder selection dialog.
 * The selected folder name is returned. */
gchar *win32_show_project_folder_dialog(const gchar *title, const gchar *initial_dir);

gint win32_check_write_permission(const gchar *dir);

#endif
