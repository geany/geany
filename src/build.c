/*
 *      build.c - this file is part of Geany, a fast and lightweight IDE
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


#include "build.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifdef G_OS_UNIX
# include <sys/types.h>
# include <sys/wait.h>
#endif

#include "support.h"
#include "utils.h"
#include "dialogs.h"
#include "msgwindow.h"


GPid build_make_c_file(gint idx, gboolean cust_target)
{
	gchar  **argv;

	argv = g_new(gchar *, 3);
	if (cust_target && app->build_make_custopt)
	{
		//cust-target
		argv[0] = g_strdup(app->build_make_cmd);
		argv[1] = g_strdup(app->build_make_custopt);
		argv[2] = NULL;
	}
	else
	{
		argv[0] = g_strdup(app->build_make_cmd);
		argv[1] = g_strdup("all");
		argv[2] = NULL;
	}

	return build_spawn_cmd(idx, argv);
}


GPid build_compile_c_file(gint idx)
{
	gchar  **argv;

	argv = g_new(gchar *, 5);
	argv[0] = g_strdup(app->build_c_cmd);
	argv[1] = g_strdup("-c");
	argv[2] = g_path_get_basename(doc_list[idx].file_name);
	argv[3] = g_strdup(app->build_args_inc);
	argv[4] = NULL;

	return build_spawn_cmd(idx, argv);
}


GPid build_link_c_file(gint idx)
{
	gchar **argv;
	gchar *executable = g_malloc0(strlen(doc_list[idx].file_name));
	gchar *object_file;
	gchar *last_dot = strrchr(doc_list[idx].file_name, '.');
	gint i = 0;
	struct stat st, st2;

	while ((doc_list[idx].file_name + i) != last_dot)
	{
		executable[i] = doc_list[idx].file_name[i];
		i++;
	}
	object_file = g_strdup_printf("%s.o", executable);

	// check wether object file (file.o) exists
	if (stat(object_file, &st) != 0)
	{
		g_free(object_file);
		object_file = NULL;
	}
	else
	{	// check wether src is newer than object file
		if (stat(doc_list[idx].file_name, &st2) == 0)
		{
			if (st2.st_mtime > st.st_mtime)
			{
				g_free(object_file);
				object_file = NULL;
			}
		}
		else
		{
			dialogs_show_error("Something very strange is occured, could not stat %s (%s)",
					doc_list[idx].file_name, strerror(errno));
		}
	}


	argv = g_new(gchar *, 6);
	argv[0] = g_strdup(app->build_c_cmd);
	argv[1] = g_strdup("-o");
	argv[2] = g_path_get_basename(executable);
	argv[3] = g_path_get_basename((object_file) ? object_file : doc_list[idx].file_name);
	argv[4] = g_strdup(app->build_args_libs);
	argv[5] = NULL;

	g_free(executable);
	g_free(object_file);

	return build_spawn_cmd(idx, argv);
}


GPid build_compile_cpp_file(gint idx)
{
	gchar  **argv;

	argv = g_new(gchar *, 5);
	argv[0] = g_strdup(app->build_cpp_cmd);
	argv[1] = g_strdup("-c");
	argv[2] = g_path_get_basename(doc_list[idx].file_name);
	argv[3] = g_strdup(app->build_args_inc);
	argv[4] = NULL;

	return build_spawn_cmd(idx, argv);
}


GPid build_link_cpp_file(gint idx)
{
	gchar **argv;
	gchar *executable = g_malloc0(strlen(doc_list[idx].file_name));
	gchar *object_file;
	gchar *last_dot = strrchr(doc_list[idx].file_name, '.');
	gint i = 0;
	struct stat st, st2;

	while ((doc_list[idx].file_name + i) != last_dot)
	{
		executable[i] = doc_list[idx].file_name[i];
		i++;
	}
	object_file = g_strdup_printf("%s.o", executable);

	// check wether object file (file.o) exists
	if (stat(object_file, &st) != 0)
	{
		g_free(object_file);
		object_file = NULL;
	}
	else
	{	// check wether src is newer than object file
		if (stat(doc_list[idx].file_name, &st2) == 0)
		{
			if (st2.st_mtime > st.st_mtime)
			{
				g_free(object_file);
				object_file = NULL;
			}
		}
		else
		{
			dialogs_show_error("Something very strange is occured, could not stat %s (%s)",
					doc_list[idx].file_name, strerror(errno));
		}
	}

	argv = g_new(gchar *, 6);
	argv[0] = g_strdup(app->build_cpp_cmd);
	argv[1] = g_strdup("-o");
	argv[2] = g_path_get_basename(executable);
	argv[3] = g_path_get_basename((object_file) ? object_file : doc_list[idx].file_name);
	argv[4] = g_strdup(app->build_args_libs);
	argv[5] = NULL;

	g_free(executable);
	g_free(object_file);

	return build_spawn_cmd(idx, argv);
}


GPid build_compile_pascal_file(gint idx)
{
	gchar  **argv;

	argv = g_new(gchar *, 4);
	argv[0] = g_strdup(app->build_fpc_cmd);
	argv[1] = g_path_get_basename(doc_list[idx].file_name);
	argv[2] = g_strdup(app->build_args_inc);
	argv[3] = NULL;

	return build_spawn_cmd(idx, argv);
}


GPid build_compile_java_file(gint idx)
{
	gchar  **argv;

	argv = g_new(gchar *, 4);
	argv[0] = g_strdup(app->build_javac_cmd);
	argv[1] = g_path_get_basename(doc_list[idx].file_name);
	argv[2] = g_strdup(app->build_args_inc);
	argv[3] = NULL;

	return build_spawn_cmd(idx, argv);
}




GPid build_spawn_cmd(gint idx, gchar **cmd)
{
	GError  *error = NULL;
	gchar **argv;
	gchar	*working_dir;
	gchar	*cmd_string;
	GPid     child_pid;
	gint     stdout_fd;
	gint     stderr_fd;

	cmd_string = g_strjoinv(" ", cmd);

	argv = g_new(gchar *, 4);
	argv[0] = g_strdup("/bin/sh");
	argv[1] = g_strdup("-c");
	argv[2] = cmd_string;
	argv[3] = NULL;

	working_dir = g_path_get_dirname(doc_list[idx].file_name);
	gtk_list_store_clear(msgwindow.store_compiler);
	msgwin_compiler_add(COLOR_BLUE, FALSE, _("%s (in directory: %s)"), cmd_string, working_dir);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);

	if (! g_spawn_async_with_pipes(working_dir, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &child_pid, NULL, &stdout_fd, &stderr_fd, &error))
	{
		g_warning("g_spawn_async_with_pipes() failed: %s", error->message);
		msgwin_status_add(_("Process failed (%s)"), error->message);
		g_strfreev(argv);
		g_error_free(error);
		error = NULL;
		return (GPid) 0;
	}

	// use GIOChannels to monitor stdout and stderr
	build_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL, build_iofunc, GINT_TO_POINTER(0));
	build_set_up_io_channel(stderr_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL, build_iofunc, GINT_TO_POINTER(1));

	g_strfreev(argv);
	g_strfreev(cmd);
	g_free(working_dir);

	return child_pid;
}


GPid build_run_cmd(gint idx)
{
	GPid	 child_pid;
	GError	*error = NULL;
	gchar  **argv;
	gchar	*working_dir;
	gchar	*long_executable;
	gchar	*check_executable;
	gchar	*executable = g_malloc0(strlen(doc_list[idx].file_name));
	gchar	*script_name = g_strdup("./geany_run_script.sh");
	struct stat st;

	/* removes the filetype extension from the filename
	 * this fails if the file has no extension, but even though a filetype,
	 * but in this case the build menu is disabled */
	long_executable = utils_remove_ext_from_filename(doc_list[idx].file_name);

	// add .class extension for JAVA source files
	if (doc_list[idx].file_type->id == GEANY_FILETYPES_JAVA)
		check_executable = g_strconcat(long_executable, ".class", NULL);
	else check_executable = g_strdup(long_executable);

	// check wether executable exists
	if (stat(check_executable, &st) != 0)
	{
		msgwin_status_add(_("Failed to execute %s (make sure it is already built)"), check_executable);
		g_free(check_executable);
		g_free(long_executable);
		return (GPid) 1;
	}

	executable = g_path_get_basename(long_executable);
	g_free(check_executable);
	g_free(long_executable);

	working_dir = g_path_get_dirname(doc_list[idx].file_name);
	if (chdir(working_dir) != 0)
	{
		msgwin_status_add(_("Failed to change the working directory to %s"), working_dir);
		g_free(working_dir);
		g_free(executable);
		return (GPid) 1;	// return 1, to prevent error handling of the caller
	}

	// write a little shellscript to call the executable (similar to anjuta_launcher but "internal")
	if (! build_create_shellscript(idx, script_name, executable, app->build_args_prog))
	{
		msgwin_status_add(_("Failed to execute %s (start-script could not be created)"), executable);
		g_free(working_dir);
		g_free(executable);
		g_free(script_name);
		return (GPid) 1;
	}

	argv = g_new(gchar *, 4);
	argv[0] = g_strdup(app->build_term_cmd);
	argv[1] = g_strdup("-e");
	argv[2] = g_strdup(script_name);
	argv[3] = NULL;

	if (! g_spawn_async_with_pipes(working_dir, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &child_pid, NULL, NULL, NULL, &error))
	{
		g_warning("g_spawn_async_with_pipes() failed: %s", error->message);
		msgwin_status_add(_("Process failed (%s)"), error->message);
		unlink(script_name);
		g_free(script_name);
		g_free(working_dir);
		g_strfreev(argv);
		g_error_free(error);
		error = NULL;
		return (GPid) 0;
	}

	/* check if the script is really deleted, this doesn't work because of g_spawn_ASYNC_with_pipes
	   anyone knows a solution? */
/*	if (stat(script_name, &st) == 0)
	{
		g_warning("The run script did not deleted itself.");
		unlink(script_name);
	}
*/
	g_free(script_name);
	g_strfreev(argv);
	g_free(working_dir);

	return child_pid;
}


gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		//GIOStatus s;
		gchar *msg;

		while (g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL) && msg)
		{
			//if (s != G_IO_STATUS_NORMAL && s != G_IO_STATUS_EOF) break;
			if (GPOINTER_TO_INT(data))
			{
				msgwin_compiler_add(COLOR_RED, FALSE, g_strstrip(msg));
			}
			else
			{
				msgwin_compiler_add(COLOR_BLACK, FALSE, g_strstrip(msg));
			}

			g_free(msg);
		}
	}
	if (cond & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		return FALSE;

	return TRUE;
}


GIOChannel *build_set_up_io_channel (gint fd, GIOCondition cond, GIOFunc func, gpointer data)
{
	GIOChannel *ioc;
	GError *error = NULL;
	const gchar *encoding;

	ioc = g_io_channel_unix_new(fd);

	g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);
	if (! g_get_charset(&encoding))
	{	// hope this works reliably
		g_io_channel_set_encoding(ioc, encoding, &error);
		if (error)
		{
			g_warning("compile: %s", error->message);
			g_error_free(error);
			return ioc;
		}
	}
	// "auto-close" ;-)
	g_io_channel_set_close_on_unref(ioc, TRUE);

	g_io_add_watch(ioc, cond, func, data);
	g_io_channel_unref(ioc);

	return ioc;
}


void build_exit_cb (GPid child_pid, gint status, gpointer user_data)
{
#ifdef G_OS_UNIX
	gboolean failure = FALSE;

	if (WIFEXITED (status))
	{
		if (WEXITSTATUS (status) != EXIT_SUCCESS)
			failure = TRUE;
	}
	else if (WIFSIGNALED (status))
	{
		// the terminating signal: WTERMSIG (status));
		failure = TRUE;
	}
	else
	{	// any other failure occured
		failure = TRUE;
	}


	if (failure)
	{
		msgwin_compiler_add(COLOR_BLUE, TRUE, _("compilation finished unsuccessful"));
	}
	else
	{
		msgwin_compiler_add(COLOR_BLUE, TRUE, _("compilation finished successful"));
	}

#endif
	if (app->beep_on_errors) gdk_beep();
	gtk_widget_set_sensitive(app->compile_button, TRUE);
	g_spawn_close_pid(child_pid);
}


gboolean build_create_shellscript(const gint idx, const gchar *fname, const gchar *exec, const gchar *args)
{
	FILE *fp;
	gchar *str, *java_cmd, *new_args, **tmp_args = NULL;

	fp = fopen(fname, "w");
	if (! fp) return FALSE;

	if (args != NULL)
	{	// enclose all args in ""
		gint i;
		gchar *tmp;

		tmp_args = g_strsplit(args, " ", -1);
		for (i = 0; ; i++)
		{
			if (tmp_args[i] == NULL) break;
			tmp = g_strdup(tmp_args[i]);
			g_free(tmp_args[i]);
			tmp_args[i] = g_strconcat("\"", tmp, "\"", NULL);
			g_free(tmp);
		}
		new_args = g_strjoinv(" ", tmp_args);
	}
	else new_args = g_strdup("");

	java_cmd = g_strconcat(app->build_java_cmd, " ", NULL);
	str = g_strdup_printf(
		"#!/bin/sh\n\n%s%s %s\n\necho \"\n\n------------------\n(program exited with code: $?)\" \
		\n\necho \"Press return to continue\"\nread\nunlink $0\n",
		(doc_list[idx].file_type->id == GEANY_FILETYPES_JAVA) ? java_cmd : "./",
		exec, new_args);
	fputs(str, fp);
	g_free(java_cmd);
	g_free(str);
	g_free(new_args);
	if (new_args != NULL) g_strfreev(tmp_args);

	if (chmod(fname, 0700) != 0)
	{
		unlink(fname);
		return FALSE;
	}
	fclose(fp);

	return TRUE;
}




#if 0
void build_c_file(gint idx)
{
	gint gcc_err, gcc_out, len, status;
	//gchar *argv[] = { "/bin/sh", "-c", "gcc", "-c", doc_list[idx].file_name, NULL };
	gchar *argv[] = { "gcc", "-Wall -c", doc_list[idx].file_name, NULL };
	GError *error = NULL;
	GPid pid;
	GIOChannel *c;
	gchar *msg;
	const gchar *encoding;

	msgwin_treeview_clear(msgwindow.store_compiler);
	msgwin_compiler_add(FALSE, g_strjoinv(" ", argv));
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);

	g_spawn_async_with_pipes(g_path_get_dirname(doc_list[idx].file_name), argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &pid,
					NULL, NULL, &gcc_err, &error);
	if (error)
	{
		g_warning("compile: %s", error->message);
		g_error_free(error);
		error = NULL;
		return;
	}

	c = g_io_channel_unix_new(gcc_err);
	//g_io_channel_set_flags(c, G_IO_FLAG_NONBLOCK, NULL);
	if (! g_get_charset(&encoding))
	{	// hope this works reliably
		g_io_channel_set_encoding(c, encoding, &error);
		if (error)
		{
			g_warning("compile: %s", error->message);
			g_error_free(error);
			return;
		}
	}

	while (g_io_channel_read_line(c, &msg, &len, NULL, NULL) == G_IO_STATUS_NORMAL)
	{
		msgwin_compiler_add(TRUE, g_strstrip(msg));
		g_free(msg);
	}

	g_io_channel_unref(c);
	g_io_channel_shutdown(c, FALSE, NULL);
	g_spawn_close_pid(pid);

	close(gcc_err);
}
#endif
