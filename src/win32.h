/*
 *      win32.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 The Geany contributors
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

#ifndef GEANY_WIN32_H
#define GEANY_WIN32_H 1

#include "document.h"

#include "gtkcompat.h"


#ifdef G_OS_WIN32

G_BEGIN_DECLS

gchar *win32_show_file_dialog(GtkWindow *parent, const gchar *title, const gchar *initial_dir);

gboolean win32_message_dialog(GtkWidget *parent, GtkMessageType type, const gchar *msg);

void win32_open_browser(const gchar *uri);


gchar *win32_show_folder_dialog(GtkWidget *parent, const gchar *title, const gchar *initial_dir);

gint win32_check_write_permission(const gchar *dir);

void win32_init_debug_code(void);

void win32_set_working_directory(const gchar *dir);

gchar *win32_get_shortcut_target(const gchar *file_name);

gchar *win32_get_installation_dir(void);

gchar *win32_expand_environment_variables(const gchar *str);

gchar *win32_get_user_config_dir(void);

G_END_DECLS

#endif /* G_OS_WIN32 */

#endif /* GEANY_WIN32_H */
