/*
 *      search.c
 *
 *      Copyright 2006 Nick Treleaven <nick.treleaven@btinternet.com>
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
 * $Id$
 */

#include "geany.h"
#include "search.h"
#include "support.h"
#include "utils.h"
#include "msgwindow.h"

#include <unistd.h>

#ifdef G_OS_UNIX
# include <sys/types.h>
# include <sys/wait.h>
#endif


static gboolean search_read_io              (GIOChannel *source,
                                             GIOCondition condition,
                                             gpointer data);

static void search_close_pid(GPid child_pid, gint status, gpointer user_data);

static gchar **search_get_argv(const gchar **argv_prefix, const gchar *dir);

static GSList *search_get_file_list(const gchar *path, gint *length);


gboolean search_find_in_files(const gchar *search_text, const gchar *dir, fif_options opts)
{
	gchar **argv_prefix;
	gchar **grep_cmd_argv;
	gchar **argv;
	gchar grep_opts[] = "-nHI   ";
	GPid child_pid;
	gint stdout_fd, stdin_fd, grep_cmd_argv_len, i, grep_opts_len = 4;
	GError *error = NULL;
	gboolean ret = FALSE;

	if (! search_text || ! *search_text || ! dir) return TRUE;

	// first process the grep command (need to split it in a argv because it can be "grep -I")
	grep_cmd_argv = g_strsplit(app->tools_grep_cmd, " ", -1);
	grep_cmd_argv_len = g_strv_length(grep_cmd_argv);

	if (! g_file_test(grep_cmd_argv[0], G_FILE_TEST_IS_EXECUTABLE))
	{
		msgwin_status_add(_("Cannot execute grep tool '%s';"
			" check the path setting in Preferences."), app->tools_grep_cmd);
		return FALSE;
	}

	gtk_list_store_clear(msgwindow.store_msg);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_MESSAGE);

	if (! (opts & FIF_CASE_SENSITIVE))
		grep_opts[grep_opts_len++] = 'i';
	if (opts & FIF_USE_EREGEXP)
		grep_opts[grep_opts_len++] = 'E';
	if (opts & FIF_INVERT_MATCH)
		grep_opts[grep_opts_len++] = 'v';
	grep_opts[grep_opts_len] = '\0';

	// set grep command and options
	argv_prefix = g_new0(gchar*, grep_cmd_argv_len + 4);
	for (i = 0; i < grep_cmd_argv_len; i++)
	{
		argv_prefix[i] = g_strdup(grep_cmd_argv[i]);
	}
	argv_prefix[grep_cmd_argv_len   ] = g_strdup(grep_opts);
	argv_prefix[grep_cmd_argv_len + 1] = g_strdup("--");
	argv_prefix[grep_cmd_argv_len + 2] = g_strdup(search_text);
	argv_prefix[grep_cmd_argv_len + 3] = NULL;
	g_strfreev(grep_cmd_argv);

	// finally add the arguments(files to be searched)
	argv = search_get_argv((const gchar**)argv_prefix, dir);
	g_strfreev(argv_prefix);
	//geany_debug(g_strjoinv(" ", argv));
	if (argv == NULL) return FALSE;

	if (! g_spawn_async_with_pipes(dir, (gchar**)argv, NULL,
		G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD,
		NULL, NULL, &child_pid,
		&stdin_fd, &stdout_fd, NULL, &error))
	{
		geany_debug("%s: g_spawn_async_with_pipes() failed: %s", __func__, error->message);
		msgwin_status_add(_("Process failed (%s)"), error->message);
		g_error_free(error);
		ret = FALSE;
	}
	else
	{
		g_free(find_in_files_dir);
		find_in_files_dir = g_strdup(dir);
		utils_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
			search_read_io, NULL);
		g_child_watch_add(child_pid, search_close_pid, NULL);
		ret = TRUE;
	}
	g_strfreev(argv);
	return ret;
}


/* Creates an argument vector of strings, copying argv_prefix[] values for
 * the first arguments, then followed by filenames found in dir.
 * Returns NULL if no files were found, otherwise returned vector should be fully freed. */
static gchar **search_get_argv(const gchar **argv_prefix, const gchar *dir)
{
	guint prefix_len, list_len, i, j;
	gchar **argv;
	GSList *list, *item;
	g_return_val_if_fail(dir != NULL, NULL);

	prefix_len = g_strv_length((gchar**)argv_prefix);
	list = search_get_file_list(dir, &list_len);
	if (list == NULL) return NULL;

	argv = g_new(gchar*, prefix_len + list_len + 1);

	for (i = 0; i < prefix_len; i++)
		argv[i] = g_strdup(argv_prefix[i]);

	item = list;
	for (j = 0; j < list_len; j++)
	{
		argv[i++] = item->data;
		item = g_slist_next(item);
	}
	argv[i] = NULL;

	g_slist_free(list);
	return argv;
}


/* Gets a list of files in the current directory, or NULL if no files found.
 * The list and the data in the list should be freed after use.
 * *length is set to the number of non-NULL data items in the list. */
static GSList *search_get_file_list(const gchar *path, gint *length)
{
	GError *error = NULL;
	GSList *list = NULL;
	gint len = 0;
	const gchar *filename;
	GDir *dir;
	g_return_val_if_fail(path != NULL, NULL);

	dir = g_dir_open(path, 0, &error);
	if (error)
	{
		msgwin_status_add(_("Could not open directory (%s)"), error->message);
		g_error_free(error);
		*length = 0;
		return NULL;
	}
	do
	{
		filename = g_dir_read_name(dir);
		list = g_slist_append(list, g_strdup(filename));
		len++;
	} while (filename != NULL);

	g_dir_close(dir);
	len--; //subtract 1 for the last null data element.
	*length = len;
	if (len == 0)
	{
		g_slist_free(list);
		return NULL;
	}
	return list;
}


static gboolean search_read_io              (GIOChannel *source,
                                             GIOCondition condition,
                                             gpointer data)
{
	if (condition & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg;

		while (g_io_channel_read_line(source, &msg, NULL, NULL, NULL) && msg)
		{
			msgwin_msg_add(-1, -1, g_strstrip(msg));
			g_free(msg);
		}
	}
	if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		return FALSE;

	return TRUE;
}


static void search_close_pid(GPid child_pid, gint status, gpointer user_data)
{
#ifdef G_OS_UNIX
	gchar *msg = _("Search failed.");

	if (WIFEXITED(status))
	{
		switch (WEXITSTATUS(status))
		{
			case 0: msg = _("Search completed."); break;
			case 1: msg = _("No matches found."); break;
			default: break;
		}
	}

	msgwin_msg_add(-1, -1, msg);
#endif
	utils_beep();
	g_spawn_close_pid(child_pid);
}


