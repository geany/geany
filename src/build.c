/*
 *      build.c - this file is part of Geany, a fast and lightweight IDE
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
 * $Id$
 */


#include "geany.h"
#include "build.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#ifdef G_OS_UNIX
# include <sys/types.h>
# include <sys/wait.h>
# include <signal.h>
#endif

#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "dialogs.h"
#include "msgwindow.h"
#include "document.h"
#include "keybindings.h"


BuildInfo build_info = {GBO_COMPILE, 0, NULL, GEANY_FILETYPES_ALL, NULL};

static struct
{
	GPid pid;
	gint file_type_id;
} run_info = {0, GEANY_FILETYPES_ALL};

enum
{
	LATEX_CMD_TO_DVI,
	LATEX_CMD_TO_PDF,
	LATEX_CMD_VIEW_DVI,
	LATEX_CMD_VIEW_PDF
};

static BuildMenuItems default_menu_items =
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static BuildMenuItems latex_menu_items =
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};


static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data);
static gboolean build_create_shellscript(const gint idx, const gchar *fname, const gchar *cmd,
								gboolean autoclose);
static GPid build_spawn_cmd(gint idx, gchar **cmd);
static void on_make_target_dialog_response(GtkDialog *dialog, gint response, gpointer user_data);
static void on_make_target_entry_activate(GtkEntry *entry, gpointer user_data);
static void set_stop_button(gboolean stop);
static void build_exit_cb(GPid child_pid, gint status, gpointer user_data);
static void run_exit_cb(GPid child_pid, gint status, gpointer user_data);
static void free_pointers(gpointer first, ...);

#ifndef G_OS_WIN32
static void kill_process(GPid *pid);
#endif


void build_finalize()
{
	g_free(build_info.dir);
	g_free(build_info.custom_target);

	if (default_menu_items.menu != NULL && GTK_IS_WIDGET(default_menu_items.menu))
		gtk_widget_destroy(default_menu_items.menu);
	if (latex_menu_items.menu != NULL && GTK_IS_WIDGET(latex_menu_items.menu))
		gtk_widget_destroy(latex_menu_items.menu);
}


GPid build_compile_tex_file(gint idx, gint mode)
{
	gchar **argv;

	if (idx < 0 || doc_list[idx].file_name == NULL) return (GPid) 1;

	argv = g_new0(gchar*, 2);
	if (mode == LATEX_CMD_TO_DVI)
	{
		argv[0] = g_strdup(doc_list[idx].file_type->programs->compiler);
		build_info.type = GBO_COMPILE;
	}
	else
	{
		argv[0] = g_strdup(doc_list[idx].file_type->programs->linker);
		build_info.type = GBO_BUILD;
	}
	argv[1] = NULL;

	return build_spawn_cmd(idx, argv);
}


GPid build_view_tex_file(gint idx, gint mode)
{
	gchar **argv, **term_argv;
	gchar  *executable = NULL;
	gchar  *view_file = NULL;
	gchar  *locale_filename = NULL;
	gchar  *cmd_string = NULL;
	gchar  *locale_cmd_string = NULL;
	gchar  *locale_term_cmd;
	gchar  *script_name;
	gint	term_argv_len, i;
	GError *error = NULL;
	struct stat st;

	if (idx < 0 || doc_list[idx].file_name == NULL) return (GPid) 1;

	run_info.file_type_id = GEANY_FILETYPES_LATEX;

#ifdef G_OS_WIN32
	script_name = g_strdup("./geany_run_script.bat");
#else
	script_name = g_strdup("./geany_run_script.sh");
#endif

	executable = utils_remove_ext_from_filename(doc_list[idx].file_name);
	view_file = g_strconcat(executable, (mode == LATEX_CMD_VIEW_DVI) ? ".dvi" : ".pdf", NULL);

	// try convert in locale for stat()
	locale_filename = utils_get_locale_from_utf8(view_file);

	// check wether view_file exists
	if (stat(locale_filename, &st) != 0)
	{
		msgwin_status_add(_("Failed to view %s (make sure it is already compiled)"), view_file);
		free_pointers(executable, view_file, locale_filename, script_name);

		return (GPid) 1;
	}

	// replace %f and %e in the run_cmd string
	cmd_string = g_strdup((mode == LATEX_CMD_VIEW_DVI) ?
										g_strdup(doc_list[idx].file_type->programs->run_cmd) :
										g_strdup(doc_list[idx].file_type->programs->run_cmd2));
	cmd_string = utils_str_replace(cmd_string, "%f", view_file);
	cmd_string = utils_str_replace(cmd_string, "%e", executable);

	// try convert in locale
	locale_cmd_string = utils_get_locale_from_utf8(cmd_string);

	/* get the terminal path */
	locale_term_cmd = utils_get_locale_from_utf8(app->tools_term_cmd);
	// split the term_cmd, so arguments will work too
	term_argv = g_strsplit(locale_term_cmd, " ", -1);
	term_argv_len = g_strv_length(term_argv);

	// check that terminal exists (to prevent misleading error messages)
	if (term_argv[0] != NULL)
	{
		gchar *tmp = term_argv[0];
		// g_find_program_in_path checks tmp exists and is executable
		term_argv[0] = g_find_program_in_path(tmp);
		g_free(tmp);
	}
	if (term_argv[0] == NULL)
	{
		msgwin_status_add(
			_("Could not find terminal '%s' "
				"(check path for Terminal tool setting in Preferences)"), app->tools_term_cmd);

		free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										script_name, locale_term_cmd);
		g_strfreev(term_argv);
		return (GPid) 1;
	}

	// write a little shellscript to call the executable (similar to anjuta_launcher but "internal")
	// (script_name should be ok in UTF8 without converting in locale because it contains no umlauts)
	if (! build_create_shellscript(idx, script_name, locale_cmd_string, TRUE))
	{
		gchar *utf8_check_executable = utils_remove_ext_from_filename(doc_list[idx].file_name);
		msgwin_status_add(_("Failed to execute %s (start-script could not be created)"),
													utf8_check_executable);
		free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										utf8_check_executable, script_name, locale_term_cmd);
		g_strfreev(term_argv);
		return (GPid) 1;
	}

	argv = g_new0(gchar *, term_argv_len + 3);
	for (i = 0; i < term_argv_len; i++)
	{
		argv[i] = g_strdup(term_argv[i]);
	}
#ifdef G_OS_WIN32
	// command line arguments for cmd.exe
	argv[term_argv_len   ]  = g_strdup("/Q /C");
	argv[term_argv_len + 1] = g_path_get_basename(script_name);
#else
	argv[term_argv_len   ]  = g_strdup("-e");
	argv[term_argv_len + 1] = g_strdup(script_name);
#endif
	argv[term_argv_len + 2] = NULL;


	if (! g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &(run_info.pid), NULL, NULL, NULL, &error))
	{
		geany_debug("g_spawn_async_with_pipes() failed: %s", error->message);
		msgwin_status_add(_("Process failed (%s)"), error->message);

		free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										script_name, locale_term_cmd);
		g_strfreev(argv);
		g_strfreev(term_argv);
		g_error_free(error);
		error = NULL;
		return (GPid) 0;
	}

	if (run_info.pid > 0)
	{
		//setpgid(0, getppid());
		g_child_watch_add(run_info.pid, (GChildWatchFunc) run_exit_cb, NULL);
		build_menu_update(idx);
	}

	free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										script_name, locale_term_cmd);
	g_strfreev(argv);
	g_strfreev(term_argv);

	return run_info.pid;
}


static gchar *get_object_filename(gint idx)
{
	gchar *locale_filename, *short_file, *noext, *object_file;

	if (doc_list[idx].file_name == NULL) return NULL;

	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);

	short_file = g_path_get_basename(locale_filename);
	g_free(locale_filename);

	noext = utils_remove_ext_from_filename(short_file);
	g_free(short_file);

	object_file = g_strdup_printf("%s.o", noext);
	g_free(noext);

	return object_file;
}


GPid build_make_file(gint idx, gint build_opts)
{
	gchar **argv;

	if (idx < 0 || doc_list[idx].file_name == NULL) return (GPid) 1;

	argv = g_new0(gchar*, 3);
	argv[0] = g_strdup(app->tools_make_cmd);

	if (build_opts == GBO_MAKE_OBJECT)
	{
		build_info.type = GBO_MAKE_OBJECT;
		argv[1] = get_object_filename(idx);
		argv[2] = NULL;
	}
	else if (build_opts == GBO_MAKE_CUSTOM && build_info.custom_target)
	{	//cust-target
		build_info.type = GBO_MAKE_CUSTOM;
		argv[1] = g_strdup(build_info.custom_target);
		argv[2] = NULL;
	}
	else	// GBO_MAKE_ALL
	{
		build_info.type = GBO_MAKE_ALL;
		argv[1] = g_strdup("all");
		argv[2] = NULL;
	}

	return build_spawn_cmd(idx, argv);
}


GPid build_compile_file(gint idx)
{
	gchar **argv;

	if (idx < 0 || doc_list[idx].file_name == NULL) return (GPid) 1;

	argv = g_new0(gchar *, 2);
	argv[0] = g_strdup(doc_list[idx].file_type->programs->compiler);
	argv[1] = NULL;

	build_info.type = GBO_COMPILE;
	return build_spawn_cmd(idx, argv);
}


GPid build_link_file(gint idx)
{
	gchar **argv;
	gchar *executable = NULL;
	gchar *object_file, *locale_filename;
	struct stat st, st2;

	if (idx < 0 || doc_list[idx].file_name == NULL) return (GPid) 1;

	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);

	executable = utils_remove_ext_from_filename(locale_filename);
	object_file = g_strdup_printf("%s.o", executable);

	// check wether object file (file.o) exists
	if (stat(object_file, &st) == 0)
	{	// check wether src is newer than object file
		if (stat(locale_filename, &st2) == 0)
		{
			if (st2.st_mtime > st.st_mtime)
			{
				// set object_file to NULL, so the source file will be used for linking,
				// more precisely then we compile and link instead of just linking
				g_free(object_file);
				object_file = NULL;
			}
		}
		else
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
					_("Something very strange is occurred, could not stat %s (%s)."),
					doc_list[idx].file_name, g_strerror(errno));
		}
	}

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_D)
	{	// the dmd compiler needs -of instead of -o and it accepts no whitespace after -of
		gchar *tmp = g_path_get_basename(executable);

		argv = g_new0(gchar *, 3);
		argv[0] = g_strdup(doc_list[idx].file_type->programs->linker);
		argv[1] = g_strconcat("-of", tmp, NULL);
		argv[2] = NULL;

		g_free(tmp);
	}
	else
	{
		argv = g_new0(gchar *, 4);
		argv[0] = g_strdup(doc_list[idx].file_type->programs->linker);
		argv[1] = g_strdup("-o");
		argv[2] = g_path_get_basename(executable);
		argv[3] = NULL;
	}

	g_free(executable);
	g_free(object_file);
	g_free(locale_filename);

	build_info.type = GBO_BUILD;
	return build_spawn_cmd(idx, argv);
}


static GPid build_spawn_cmd(gint idx, gchar **cmd)
{
	GError  *error = NULL;
	gchar **argv;
	gchar	*working_dir;
	gchar	*utf8_working_dir;
	gchar	*cmd_string;
	gchar	*utf8_cmd_string;
	gchar	*locale_filename;
	gchar	*executable;
	gchar	*tmp;
	gint     stdout_fd;
	gint     stderr_fd;

	g_return_val_if_fail(idx >= 0 && doc_list[idx].is_valid, (GPid) 1);

	document_clear_indicators(idx);

	cmd_string = g_strjoinv(" ", cmd);
	g_strfreev(cmd);

	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);

	executable = utils_remove_ext_from_filename(locale_filename);

	// replace %f and %e in the command string
	tmp = g_path_get_basename(locale_filename);
	cmd_string = utils_str_replace(cmd_string, "%f", tmp);
	g_free(tmp);
	tmp = g_path_get_basename(executable);
	cmd_string = utils_str_replace(cmd_string, "%e", tmp);
	g_free(tmp);
	g_free(executable);

	utf8_cmd_string = utils_get_utf8_from_locale(cmd_string);

	argv = g_new0(gchar *, 4);
	argv[0] = g_strdup("/bin/sh");
	argv[1] = g_strdup("-c");
	argv[2] = cmd_string;
	argv[3] = NULL;

	working_dir = g_path_get_dirname(locale_filename);
	utf8_working_dir = g_path_get_dirname(doc_list[idx].file_name);
	gtk_list_store_clear(msgwindow.store_compiler);
	msgwin_compiler_add(COLOR_BLUE, _("%s (in directory: %s)"), utf8_cmd_string, utf8_working_dir);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);

	// set the build info for the message window
	g_free(build_info.dir);
	build_info.dir = g_strdup(working_dir);
	build_info.file_type_id = FILETYPE_ID(doc_list[idx].file_type);

	if (! g_spawn_async_with_pipes(working_dir, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &(build_info.pid), NULL, &stdout_fd, &stderr_fd, &error))
	{
		geany_debug("g_spawn_async_with_pipes() failed: %s", error->message);
		msgwin_status_add(_("Process failed (%s)"), error->message);
		g_strfreev(argv);
		g_error_free(error);
		g_free(working_dir);
		g_free(utf8_working_dir);
		g_free(utf8_cmd_string);
		g_free(locale_filename);
		error = NULL;
		return (GPid) 0;
	}

	if (build_info.pid > 0)
	{
		g_child_watch_add(build_info.pid, (GChildWatchFunc) build_exit_cb, NULL);
		build_menu_update(idx);
	}

	// use GIOChannels to monitor stdout and stderr
	utils_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
		build_iofunc, GINT_TO_POINTER(0));
	utils_set_up_io_channel(stderr_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
		build_iofunc, GINT_TO_POINTER(1));

	g_strfreev(argv);
	g_free(utf8_working_dir);
	g_free(utf8_cmd_string);
	g_free(working_dir);
	g_free(locale_filename);

	return build_info.pid;
}


GPid build_run_cmd(gint idx)
{
	GPid	 result_id;	// either child_pid or error id.
	GError	*error = NULL;
	gchar  **argv = NULL;
	gchar  **term_argv = NULL;
	gchar	*working_dir = NULL;
	gchar	*long_executable = NULL;
	gchar	*check_executable = NULL;
	gchar	*utf8_check_executable = NULL;
	gchar	*locale_filename = NULL;
	gchar	*locale_term_cmd = NULL;
	gchar	*cmd = NULL;
	gchar	*tmp = NULL;
	gchar	*executable = NULL;
	gchar	*script_name;
	guint    term_argv_len, i;
	struct stat st;

	if (! DOC_IDX_VALID(idx) || doc_list[idx].file_name == NULL) return (GPid) 1;

	run_info.file_type_id = FILETYPE_ID(doc_list[idx].file_type);

#ifdef G_OS_WIN32
	script_name = g_strdup("./geany_run_script.bat");
#else
	script_name = g_strdup("./geany_run_script.sh");
#endif

	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);

	long_executable = utils_remove_ext_from_filename(locale_filename);
#ifdef G_OS_WIN32
	long_executable = g_strconcat(long_executable, ".exe", NULL);
#endif

	// only check for existing executable, if executable is required by %e
	if (strstr(doc_list[idx].file_type->programs->run_cmd, "%e") != NULL)
	{
		// add .class extension for JAVA source files (only for stat)
		if (doc_list[idx].file_type->id == GEANY_FILETYPES_JAVA)
		{
#ifdef G_OS_WIN32
			// there is already the extension .exe, so first remove it and then add .class
			check_executable = utils_remove_ext_from_filename(long_executable);
			check_executable = g_strconcat(check_executable, ".class", NULL);
#else
			check_executable = g_strconcat(long_executable, ".class", NULL);
#endif
		}
		else
			check_executable = g_strdup(long_executable);

		// check whether executable exists
		if (stat(check_executable, &st) != 0)
		{
/// TODO why?
#ifndef G_OS_WIN32
			utf8_check_executable = g_strdup(check_executable);
#else
			utf8_check_executable = utils_remove_ext_from_filename(doc_list[idx].file_name);
#endif
			msgwin_status_add(_("Failed to execute %s (make sure it is already built)"),
														utf8_check_executable);
			result_id = (GPid) 1;
			goto free_strings;
		}
	}

	/* get the terminal path */
	locale_term_cmd = utils_get_locale_from_utf8(app->tools_term_cmd);
	// split the term_cmd, so arguments will work too
	term_argv = g_strsplit(locale_term_cmd, " ", -1);
	term_argv_len = g_strv_length(term_argv);

	// check that terminal exists (to prevent misleading error messages)
	if (term_argv[0] != NULL)
	{
		tmp = term_argv[0];
		// g_find_program_in_path checks tmp exists and is executable
		term_argv[0] = g_find_program_in_path(tmp);
		g_free(tmp);
	}
	if (term_argv[0] == NULL)
	{
		msgwin_status_add(
			_("Could not find terminal '%s' "
				"(check path for Terminal tool setting in Preferences)"), app->tools_term_cmd);
		result_id = (GPid) 1;
		goto free_strings;
	}

	executable = g_path_get_basename(long_executable);

	working_dir = g_path_get_dirname(locale_filename);
	if (chdir(working_dir) != 0)
	{
		gchar *utf8_working_dir = NULL;
		utf8_working_dir = utils_get_utf8_from_locale(working_dir);

		msgwin_status_add(_("Failed to change the working directory to %s"), working_dir);
		result_id = (GPid) 1;	// return 1, to prevent error handling of the caller
		g_free(utf8_working_dir);
		goto free_strings;
	}

	// replace %f and %e in the run_cmd string
	cmd = g_strdup(doc_list[idx].file_type->programs->run_cmd);
	tmp = g_path_get_basename(locale_filename);
	cmd = utils_str_replace(cmd, "%f", tmp);
	g_free(tmp);
	cmd = utils_str_replace(cmd, "%e", executable);

	// write a little shellscript to call the executable (similar to anjuta_launcher but "internal")
	// (script_name should be ok in UTF8 without converting in locale because it contains no umlauts)
	if (! build_create_shellscript(idx, script_name, cmd, FALSE))
	{
		utf8_check_executable = utils_remove_ext_from_filename(doc_list[idx].file_name);
		msgwin_status_add(_("Failed to execute %s (start-script could not be created)"),
													utf8_check_executable);
		result_id = (GPid) 1;
		goto free_strings;
	}

	argv = g_new0(gchar *, term_argv_len + 3);
	for (i = 0; i < term_argv_len; i++)
	{
		argv[i] = g_strdup(term_argv[i]);
	}
#ifdef G_OS_WIN32
	// command line arguments for cmd.exe
	argv[term_argv_len   ]  = g_strdup("/Q /C");
	argv[term_argv_len + 1] = g_path_get_basename(script_name);
#else
	argv[term_argv_len   ]  = g_strdup("-e");
	argv[term_argv_len + 1] = g_strdup(script_name);
#endif
	argv[term_argv_len + 2] = NULL;

	if (! g_spawn_async_with_pipes(working_dir, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &(run_info.pid), NULL, NULL, NULL, &error))
	{
		geany_debug("g_spawn_async_with_pipes() failed: %s", error->message);
		msgwin_status_add(_("Process failed (%s)"), error->message);
		unlink(script_name);
		g_error_free(error);
		error = NULL;
		result_id = (GPid) 0;
		goto free_strings;
	}

	result_id = run_info.pid; // g_spawn was successful, result is child process id
	if (run_info.pid > 0)
	{
		g_child_watch_add(run_info.pid, (GChildWatchFunc) run_exit_cb, NULL);
		build_menu_update(idx);
	}

	free_strings:
	/* free all non-NULL strings */
	g_strfreev(argv);
	g_strfreev(term_argv);
	g_free(working_dir);
	g_free(cmd);
	g_free(utf8_check_executable);
	g_free(locale_filename);
	g_free(locale_term_cmd);
	g_free(check_executable);
	g_free(long_executable);
	g_free(executable);
	g_free(script_name);

	return result_id;
}


static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		//GIOStatus s;
		gchar *msg;

		while (g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL) && msg)
		{
			//if (s != G_IO_STATUS_NORMAL && s != G_IO_STATUS_EOF) break;
			gint color;
			color = (GPOINTER_TO_INT(data)) ? COLOR_DARK_RED : COLOR_BLACK;
			g_strstrip(msg);

			if (app->pref_editor_use_indicators)
			{
				gchar *filename;
				gint line;
				msgwin_parse_compiler_error_line(msg, &filename, &line);
				if (line != -1 && filename != NULL)
				{
					gint idx = document_find_by_filename(filename, FALSE);
					document_set_indicator(idx, line - 1);	// will check valid idx
					color = COLOR_RED;	// error message parsed on the line
				}
				g_free(filename);
			}
			msgwin_compiler_add(color, msg);

			g_free(msg);
		}
	}
	if (cond & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		return FALSE;

	return TRUE;
}


#ifndef G_OS_WIN32
static void show_build_result_message(gboolean failure)
{
	gchar *msg;

	if (failure)
	{
		msg = _("Compilation failed.");
		msgwin_compiler_add(COLOR_DARK_RED, "%s", msg);
		// If msgwindow is hidden, user will want to display it to see the error
		if (! app->msgwindow_visible)
		{
			gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
			msgwin_show();
		}
		else
		if (gtk_notebook_get_current_page(GTK_NOTEBOOK(msgwindow.notebook)) != MSG_COMPILER)
			ui_set_statusbar("%s", msg);
	}
	else
	{
		msg = _("Compilation finished successfully.");
		msgwin_compiler_add(COLOR_BLUE, "%s", msg);
		if (! app->msgwindow_visible ||
			gtk_notebook_get_current_page(GTK_NOTEBOOK(msgwindow.notebook)) != MSG_COMPILER)
				ui_set_statusbar("%s", msg);
	}
}
#endif


static void build_exit_cb(GPid child_pid, gint status, gpointer user_data)
{
#ifdef G_OS_UNIX
	gboolean failure = FALSE;

	if (WIFEXITED(status))
	{
		if (WEXITSTATUS(status) != EXIT_SUCCESS)
			failure = TRUE;
	}
	else if (WIFSIGNALED(status))
	{
		// the terminating signal: WTERMSIG (status));
		failure = TRUE;
	}
	else
	{	// any other failure occured
		failure = TRUE;
	}
	show_build_result_message(failure);
#endif

	utils_beep();
	g_spawn_close_pid(child_pid);

	build_info.pid = 0;
	// enable build items again
	build_menu_update(-1);
}


static void run_exit_cb(GPid child_pid, gint status, gpointer user_data)
{
	g_spawn_close_pid(child_pid);

	run_info.pid = 0;
	// reset the stop button and menu item to the original meaning
	build_menu_update(-1);
}


static gboolean build_create_shellscript(const gint idx, const gchar *fname, const gchar *cmd,
											gboolean autoclose)
{
	FILE *fp;
	gchar *str;
#ifdef G_OS_WIN32
	gchar *tmp;
#endif

	fp = fopen(fname, "w");
	if (! fp) return FALSE;

#ifdef G_OS_WIN32
	tmp = g_path_get_basename(fname);
	str = g_strdup_printf("%s\n\n%s\ndel %s\n", cmd, (autoclose) ? "" : "pause", tmp);
	g_free(tmp);
#else
	str = g_strdup_printf(
		"#!/bin/sh\n\n%s\n\necho \"\n\n------------------\n(program exited with code: $?)\" \
		\n\necho \"Press return to continue\"\n%s\nunlink $0\n", cmd, (autoclose) ? "" :
		"#to be more compatible with shells like dash\ndummy_var=\"\"\nread dummy_var");
#endif

	fputs(str, fp);
	g_free(str);

#ifndef G_OS_WIN32
	if (chmod(fname, 0700) != 0)
	{
		unlink(fname);
		return FALSE;
	}
#endif
	fclose(fp);

	return TRUE;
}


#define GEANY_ADD_WIDGET_ACCEL(gkey, menuitem) \
	if (keys[(gkey)]->key != 0) \
		gtk_widget_add_accelerator(menuitem, "activate", accel_group, \
			keys[(gkey)]->key, keys[(gkey)]->mods, GTK_ACCEL_VISIBLE)

static void create_build_menu_gen(BuildMenuItems *menu_items)
{
	GtkWidget *menu, *item = NULL, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	menu = gtk_menu_new();

#ifndef G_OS_WIN32
	// compile the code
	item = gtk_image_menu_item_new_with_mnemonic(_("_Compile"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_COMPILE, item);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_compile_activate), NULL);
	menu_items->item_compile = item;

	// build the code
	item = gtk_image_menu_item_new_with_mnemonic(_("_Build"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Builds the current file (generate an executable file)"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_LINK, item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_build_activate), NULL);
	menu_items->item_link = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	// build the code with make all
	item = gtk_image_menu_item_new_with_mnemonic(_("_Make all"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKE, item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_ALL));
	menu_items->item_make_all = item;

	// build the code with make custom
	item = gtk_image_menu_item_new_with_mnemonic(_("Make custom _target"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOWNTARGET, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_CUSTOM));
	menu_items->item_make_custom = item;

	// build the code with make object
	item = gtk_image_menu_item_new_with_mnemonic(_("Make _object"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOBJECT, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file using the "
										   "make tool"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_OBJECT));
	menu_items->item_make_object = item;
#endif

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	// next error
	item = gtk_image_menu_item_new_with_mnemonic(_("_Next Error"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_NEXTERROR, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_next_error), NULL);
	menu_items->item_next_error = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	// execute the code
	item = gtk_image_menu_item_new_from_stock("gtk-execute", accel_group);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Run or view the current file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_RUN, item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_execute_activate), NULL);
	menu_items->item_exec = item;

	separator = gtk_separator_menu_item_new();
	gtk_widget_show(separator);
	gtk_container_add(GTK_CONTAINER(menu), separator);
	gtk_widget_set_sensitive(separator, FALSE);

	// arguments
	item = gtk_image_menu_item_new_with_mnemonic(_("_Set Includes and Arguments"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_OPTIONS, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Sets the includes and library paths for the compiler and "
				  "the program arguments for execution"), NULL);
	image = gtk_image_new_from_stock("gtk-preferences", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_arguments_activate), NULL);
	menu_items->item_set_args = item;

	menu_items->menu = menu;
	g_object_ref((gpointer)menu_items->menu);	// to hold it after removing
}


static void create_build_menu_tex(BuildMenuItems *menu_items)
{
	GtkWidget *menu, *item, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	menu = gtk_menu_new();

#ifndef G_OS_WIN32
	// DVI
	item = gtk_image_menu_item_new_with_mnemonic(_("LaTeX -> DVI"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a DVI file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_COMPILE, item);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(LATEX_CMD_TO_DVI));
	menu_items->item_compile = item;

	// PDF
	item = gtk_image_menu_item_new_with_mnemonic(_("LaTeX -> PDF"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a PDF file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_LINK, item);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(LATEX_CMD_TO_PDF));
	menu_items->item_link = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	// build the code with make all
	item = gtk_image_menu_item_new_with_mnemonic(_("_Make all"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKE, item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_ALL));
	menu_items->item_make_all = item;

	// build the code with make custom
	item = gtk_image_menu_item_new_with_mnemonic(_("Make custom _target"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOWNTARGET, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_CUSTOM));
	menu_items->item_make_custom = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
#endif

	// next error
	item = gtk_image_menu_item_new_with_mnemonic(_("_Next Error"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_NEXTERROR, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_next_error), NULL);
	menu_items->item_next_error = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	// DVI view
#define LATEX_VIEW_DVI_LABEL _("View DVI file") // used later again
	item = gtk_image_menu_item_new_with_mnemonic(LATEX_VIEW_DVI_LABEL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_RUN, item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles and view the current file"), NULL);
	image = gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate",
						G_CALLBACK(on_build_execute_activate), GINT_TO_POINTER(LATEX_CMD_VIEW_DVI));
	menu_items->item_exec = item;

	// PDF view
	item = gtk_image_menu_item_new_with_mnemonic(_("View PDF file"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_RUN2, item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles and view the current file"), NULL);
	image = gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate",
						G_CALLBACK(on_build_execute_activate), GINT_TO_POINTER(LATEX_CMD_VIEW_PDF));
	menu_items->item_exec2 = item;

	// separator
	separator = gtk_separator_menu_item_new();
	gtk_widget_show(separator);
	gtk_container_add(GTK_CONTAINER(menu), separator);
	gtk_widget_set_sensitive(separator, FALSE);

	// arguments
	item = gtk_image_menu_item_new_with_mnemonic(_("Set Arguments"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_OPTIONS, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Sets the program paths and arguments"), NULL);
	image = gtk_image_new_from_stock("gtk-preferences", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate",
		G_CALLBACK(on_build_arguments_activate), filetypes[GEANY_FILETYPES_LATEX]);
	menu_items->item_set_args = item;

	gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);

	menu_items->menu = menu;
	g_object_ref((gpointer)menu_items->menu);	// to hold it after removing
}


static gboolean is_c_header(const gchar *fname)
{
	gchar *ext = NULL;

	if (fname)
	{
		ext = strrchr(fname, '.');
	}
	return (ext == NULL) ? FALSE : (*(ext + 1) == 'h');	// match *.h*
}


/* Call this whenever build menu items need to be enabled/disabled.
 * Uses current document (if there is one) when idx == -1 */
void build_menu_update(gint idx)
{
	filetype *ft;
	gboolean have_path, can_build, can_make, can_run, can_set_args, have_errors;
	BuildMenuItems *menu_items;

	if (idx == -1)
		idx = document_get_cur_idx();
	if (idx == -1 ||
		(FILETYPE_ID(doc_list[idx].file_type) == GEANY_FILETYPES_ALL &&
			doc_list[idx].file_name == NULL))
	{
		gtk_widget_set_sensitive(lookup_widget(app->window, "menu_build1"), FALSE);
		gtk_menu_item_remove_submenu(GTK_MENU_ITEM(lookup_widget(app->window, "menu_build1")));
		gtk_widget_set_sensitive(app->compile_button, FALSE);
		gtk_widget_set_sensitive(app->run_button, FALSE);
		return;
	}
	else
		gtk_widget_set_sensitive(lookup_widget(app->window, "menu_build1"), TRUE);

	ft = doc_list[idx].file_type;
	g_return_if_fail(ft != NULL);

#ifdef G_OS_WIN32
	// disable compile and link under Windows until it is implemented
	ft->actions->can_compile = FALSE;
	ft->actions->can_link = FALSE;
#endif

	menu_items = build_get_menu_items(ft->id);
	/* Note: don't remove the submenu first because it can now cause an X hang if
	 * the menu is already open when called from build_exit_cb(). */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(lookup_widget(app->window, "menu_build1")),
		menu_items->menu);

	have_path = (doc_list[idx].file_name != NULL);

	can_make = have_path && build_info.pid <= (GPid) 1;

	// disable compile and link for C/C++ header files
	if (ft->id == GEANY_FILETYPES_C || ft->id == GEANY_FILETYPES_CPP)
		can_build = can_make && ! is_c_header(doc_list[idx].file_name);
	else
		can_build = can_make;

	if (menu_items->item_compile)
		gtk_widget_set_sensitive(menu_items->item_compile, can_build && ft->actions->can_compile);
	if (menu_items->item_link)
		gtk_widget_set_sensitive(menu_items->item_link, can_build && ft->actions->can_link);
	if (menu_items->item_make_all)
		gtk_widget_set_sensitive(menu_items->item_make_all, can_make);
	if (menu_items->item_make_custom)
		gtk_widget_set_sensitive(menu_items->item_make_custom, can_make);
	if (menu_items->item_make_object)
		gtk_widget_set_sensitive(menu_items->item_make_object, can_make);

	can_run = have_path && run_info.pid <= (GPid) 1;
	/* can_run only applies item_exec2
	 * item_exec is enabled for both run and stop commands */
	if (menu_items->item_exec)
		gtk_widget_set_sensitive(menu_items->item_exec, have_path && ft->actions->can_exec);
	if (menu_items->item_exec2)
		gtk_widget_set_sensitive(menu_items->item_exec2, can_run && ft->actions->can_exec);

	can_set_args =
		((ft->actions->can_compile ||
		ft->actions->can_link ||
		ft->actions->can_exec) &&
		FILETYPE_ID(ft) != GEANY_FILETYPES_ALL);
	if (menu_items->item_set_args)
		gtk_widget_set_sensitive(menu_items->item_set_args, can_set_args);

	gtk_widget_set_sensitive(app->compile_button, can_build && ft->actions->can_compile);
	gtk_widget_set_sensitive(app->run_button, have_path && ft->actions->can_exec);

	// show the stop command if a program is running, otherwise show run command
	set_stop_button(run_info.pid > (GPid) 1);

	have_errors = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(msgwindow.store_compiler),
		NULL) > 0;
	gtk_widget_set_sensitive(menu_items->item_next_error, have_errors);
}


// Call build_menu_update() instead of calling this directly.
static void set_stop_button(gboolean stop)
{
	GtkStockItem sitem;
	GtkWidget *menuitem =
		build_get_menu_items(run_info.file_type_id)->item_exec;

	if (stop && utils_str_equal(
		gtk_tool_button_get_stock_id(GTK_TOOL_BUTTON(app->run_button)), "gtk-stop")) return;
	if (! stop && utils_str_equal(
		gtk_tool_button_get_stock_id(GTK_TOOL_BUTTON(app->run_button)), "gtk-execute")) return;

	// use the run button also as stop button
	if (stop)
	{
		gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(app->run_button), "gtk-stop");

		if (menuitem != NULL)
		{
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
							gtk_image_new_from_stock("gtk-stop", GTK_ICON_SIZE_MENU));
			gtk_stock_lookup("gtk-stop", &sitem);
			gtk_label_set_text_with_mnemonic(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))),
						sitem.label);
		}
	}
	else
	{
		gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(app->run_button), "gtk-execute");

		if (menuitem != NULL)
		{
			// LaTeX hacks ;-(
			if (run_info.file_type_id == GEANY_FILETYPES_LATEX)
			{
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
							gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU));
				gtk_label_set_text_with_mnemonic(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))),
						LATEX_VIEW_DVI_LABEL);
			}
			else
			{
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menuitem),
							gtk_image_new_from_stock("gtk-execute", GTK_ICON_SIZE_MENU));

				gtk_stock_lookup("gtk-execute", &sitem);
				gtk_label_set_text_with_mnemonic(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))),
							sitem.label);
			}
		}
	}
}


/* Creates the relevant build menu if necessary.
 * If filetype_id is -1, the current filetype is used, or GEANY_FILETYPES_ALL */
BuildMenuItems *build_get_menu_items(gint filetype_id)
{
	BuildMenuItems *items;

	if (filetype_id == -1)
	{
		gint idx = document_get_cur_idx();
		filetype *ft = NULL;

		if (DOC_IDX_VALID(idx))
			ft = doc_list[idx].file_type;
		filetype_id = FILETYPE_ID(ft);
	}

	if (filetype_id == GEANY_FILETYPES_LATEX)
	{
		items = &latex_menu_items;
		if (items->menu == NULL)
			create_build_menu_tex(items);
	}
	else
	{
		items = &default_menu_items;
		if (items->menu == NULL)
			create_build_menu_gen(items);
	}
	return items;
}


void
on_build_compile_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx)) return;

	if (doc_list[idx].changed) document_save_file(idx, FALSE);

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_LATEX)
		build_compile_tex_file(idx, 0);
	else
		build_compile_file(idx);
}


void
on_build_tex_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (doc_list[idx].changed) document_save_file(idx, FALSE);

	switch (GPOINTER_TO_INT(user_data))
	{
		case LATEX_CMD_TO_DVI:
		case LATEX_CMD_TO_PDF:
			build_compile_tex_file(idx, GPOINTER_TO_INT(user_data)); break;
		case LATEX_CMD_VIEW_DVI:
		case LATEX_CMD_VIEW_PDF:
			build_view_tex_file(idx, GPOINTER_TO_INT(user_data)); break;
	}
}


void
on_build_build_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx)) return;

	if (doc_list[idx].changed) document_save_file(idx, FALSE);

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_LATEX)
		build_compile_tex_file(idx, 1);
	else
		build_link_file(idx);
}


void
on_build_make_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gint build_opts = GPOINTER_TO_INT(user_data);

	g_return_if_fail(DOC_IDX_VALID(idx) && doc_list[idx].file_name != NULL);

	switch (build_opts)
	{
		case GBO_MAKE_CUSTOM:
		{
			dialogs_show_input(_("Make custom target"),
				_("Enter custom options here, all entered text is passed to the make command."),
				build_info.custom_target,
				G_CALLBACK(on_make_target_dialog_response),
				G_CALLBACK(on_make_target_entry_activate));
			break;
		}

		case GBO_MAKE_OBJECT:
		// fall through
		case GBO_MAKE_ALL:
		{
			if (doc_list[idx].changed) document_save_file(idx, FALSE);

			build_make_file(idx, build_opts);
		}
	}
}


void
on_build_execute_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	// make the process "stopable"
	if (run_info.pid > (GPid) 1)
	{
		// on Windows there is no PID returned (resp. it is a handle), currently unsupported
#ifndef G_OS_WIN32
		kill_process(&run_info.pid);
#endif
		return;
	}

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_LATEX)
	{	// run LaTeX file
		if (build_view_tex_file(idx, GPOINTER_TO_INT(user_data)) == (GPid) 0)
		{
			msgwin_status_add(_("Failed to execute the view program"));
		}
	}
	else if (doc_list[idx].file_type->id == GEANY_FILETYPES_HTML)
	{	// run HTML file
		gchar *uri = g_strconcat("file:///", g_path_skip_root(doc_list[idx].file_name), NULL);
		utils_start_browser(uri);
		g_free(uri);
	}
	else
	{	// run everything else

		// save the file only if the run command uses it
		if (doc_list[idx].changed &&
			strstr(doc_list[idx].file_type->programs->run_cmd, "%f") != NULL)
				document_save_file(idx, FALSE);

		if (build_run_cmd(idx) == (GPid) 0)
		{
			msgwin_status_add(_("Failed to execute the terminal program"));
		}
	}
}


void
on_build_arguments_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (user_data && FILETYPE_ID((filetype*) user_data) == GEANY_FILETYPES_LATEX)
		dialogs_show_includes_arguments_tex();
	else
		dialogs_show_includes_arguments_gen();
}


static void
on_make_target_dialog_response         (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gint idx = document_get_cur_idx();

		if (doc_list[idx].changed) document_save_file(idx, FALSE);

		g_free(build_info.custom_target);
		build_info.custom_target = g_strdup(gtk_entry_get_text(GTK_ENTRY(user_data)));

		build_make_file(idx, GBO_MAKE_CUSTOM);
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


static void
on_make_target_entry_activate          (GtkEntry        *entry,
                                        gpointer         user_data)
{
	on_make_target_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_ACCEPT, entry);
}


#ifndef G_OS_WIN32
static void kill_process(GPid *pid)
{
	/* SIGQUIT is not the best signal to use because it causes a core dump (this should not
	 * perforce necessary for just killing a process). But we must use a signal which we can
	 * ignore because the main process get it too, it is declared to ignore in main.c. */

	gint resultpg, result;

	// sent SIGQUIT to all the processes to the processes' own process group
	result = kill(*pid, SIGQUIT);
	resultpg = killpg(0, SIGQUIT);

	if (result != 0 || resultpg != 0)
		msgwin_status_add(_("Process could not be stopped (%s)."), g_strerror(errno));
	else
	{
		*pid = 0;
		build_menu_update(-1);
	}
}
#endif


// frees all passed pointers if they are non-NULL, the first argument is nothing special,
// it will also be freed
static void free_pointers(gpointer first, ...)
{
	va_list a;
	gpointer sa;

    for (va_start(a, first);  (sa = va_arg(a, gpointer), sa!=NULL);)
    {
    	if (sa != NULL)
    		g_free(sa);
	}
	va_end(a);

    if (first != NULL)
    	g_free(first);
}


void
on_build_next_error                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (! ui_tree_view_find_next(GTK_TREE_VIEW(msgwindow.tree_compiler),
		msgwin_goto_compiler_file_line))
		ui_set_statusbar(_("No more build errors."));
}


