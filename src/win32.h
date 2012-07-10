/*
 *      win32.h - this file is part of Geany, a fast and lightweight IDE
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef G_OS_WIN32


void win32_show_pref_file_dialog(GtkEntry *item);

gchar *win32_show_file_dialog(GtkWindow *parent, const gchar *title, const gchar *initial_dir);

gboolean win32_show_document_open_dialog(GtkWindow *parent, const gchar *title, const gchar *initial_dir);

gchar *win32_show_document_save_as_dialog(GtkWindow *parent, const gchar *title,
										  const gchar *initial_file);

void win32_show_font_dialog(void);

void win32_show_color_dialog(const gchar *colour);

gboolean win32_message_dialog(GtkWidget *parent, GtkMessageType type, const gchar *msg);

void win32_open_browser(const gchar *uri);

gchar *win32_show_project_open_dialog(GtkWidget *parent, const gchar *title,
								      const gchar *initial_dir, gboolean allow_new_file,
								      gboolean project_file_filter);

gchar *win32_show_folder_dialog(GtkWidget *parent, const gchar *title, const gchar *initial_dir);

gint win32_check_write_permission(const gchar *dir);

void win32_init_debug_code(void);

void win32_set_working_directory(const gchar *dir);

gchar *win32_get_hostname(void);

gboolean win32_get_exit_status(GPid child_pid);

gboolean win32_spawn(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
					 gchar **std_out, gchar **std_err, gint *exit_status, GError **error);

gchar *win32_get_shortcut_target(const gchar *file_name);

gchar *win32_get_installation_dir(void);

gchar *win32_expand_environment_variables(const gchar *str);

#endif
