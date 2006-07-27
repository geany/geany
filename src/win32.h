/*
 *      win32.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */


#ifdef G_OS_WIN32


void win32_show_pref_file_dialog(GtkEntry *item);

void win32_show_file_dialog(gboolean file_open);

void win32_show_font_dialog(void);

void win32_show_color_dialog(const gchar *colour);

/* Creates a native Windows message box of the given type and returns always TRUE
 * or FALSE representing th pressed Yes or No button.
 * If type is not GTK_MESSAGE_QUESTION, it returns always TRUE. */
gboolean win32_message_dialog(GtkMessageType type, const gchar *msg);

/* Special dialog to ask for an action when closing an unsaved file */
gint win32_message_dialog_unsaved(const gchar *msg);

/* Just a simple wrapper function to open a browser window */
void win32_open_browser(const gchar *uri);

#endif
