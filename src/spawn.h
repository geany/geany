/*
 *      spawn.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2013 Dimitar Toshkov Zhekov <dimitar(dot)zhekov(at)gmail(dot)com>
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


#ifndef GEANY_SPAWN_H
#define GEANY_SPAWN_H 1

#include <gtk/gtk.h>

gboolean spawn_check_command(const gchar *command_line, gboolean execute, GError **error);

gboolean spawn_async_with_pipes(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GPid *child_pid, gint *stdin_fd, gint *stdout_fd,
	gint *stderr_fd, GError **error);

gboolean spawn_kill_process(GPid pid, GError **error);

gboolean spawn_with_callbacks(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GtkWindow *block_window, gchar *stdin_text,
	void (*stdout_cb)(const gchar *text, gpointer user_data), gpointer stdout_data,
	void (*stderr_cb)(const gchar *text, gpointer user_data), gpointer stderr_data,
	void (*exit_cb)(gint status, gpointer user_data), gpointer exit_data, GPid *child_pid,
	GError **error);

gboolean spawn_with_capture(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GtkWindow *block_window, gchar *stdin_text,
	gchar **stdout_text, gchar **stderr_text, gint *exit_status, GError **error);

#endif
