/*
 *      build.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/*
 * Build commands and menu items.
 */

#include "geany.h"
#include "build.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <glib/gstdio.h>

#ifdef G_OS_UNIX
# include <sys/types.h>
# include <sys/wait.h>
# include <signal.h>
#else
# include <windows.h>
#endif

#include "prefs.h"
#include "support.h"
#include "document.h"
#include "utils.h"
#include "ui_utils.h"
#include "dialogs.h"
#include "msgwindow.h"
#include "filetypes.h"
#include "keybindings.h"
#include "vte.h"
#include "project.h"
#include "editor.h"
#include "win32.h"


GeanyBuildInfo build_info = {GBO_COMPILE, 0, NULL, GEANY_FILETYPES_NONE, NULL};

static gchar *current_dir_entered = NULL;

static struct
{
	GPid pid;
	gint file_type_id;
} run_info = {0, GEANY_FILETYPES_NONE};

#ifdef G_OS_WIN32
static const gchar RUN_SCRIPT_CMD[] = "geany_run_script.bat";
#else
static const gchar RUN_SCRIPT_CMD[] = "./geany_run_script.sh";
#endif

enum
{
	LATEX_CMD_TO_DVI,
	LATEX_CMD_TO_PDF,
	LATEX_CMD_VIEW_DVI,
	LATEX_CMD_VIEW_PDF
};

static BuildMenuItems default_menu_items =
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static BuildMenuItems latex_menu_items =
	{NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};


static struct
{
	GtkWidget	*run_button;
	GtkWidget	*compile_button;
}
widgets;


static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data);
static gboolean build_create_shellscript(const gchar *fname, const gchar *cmd, gboolean autoclose);
static GPid build_spawn_cmd(GeanyDocument *doc, const gchar *cmd, const gchar *dir);
static void set_stop_button(gboolean stop);
static void build_exit_cb(GPid child_pid, gint status, gpointer user_data);
static void run_exit_cb(GPid child_pid, gint status, gpointer user_data);
static void on_build_arguments_activate(GtkMenuItem *menuitem, gpointer user_data);

static void on_build_compile_activate(GtkMenuItem *menuitem, gpointer user_data);
static void on_build_tex_activate(GtkMenuItem *menuitem, gpointer user_data);
static void on_build_build_activate(GtkMenuItem *menuitem, gpointer user_data);
static void on_build_make_activate(GtkMenuItem *menuitem, gpointer user_data);
static void on_build_execute_activate(GtkMenuItem *menuitem, gpointer user_data);
static void on_build_next_error(GtkMenuItem *menuitem, gpointer user_data);
static void on_build_previous_error(GtkMenuItem *menuitem, gpointer user_data);
static void kill_process(GPid *pid);


void build_finalize()
{
	g_free(build_info.dir);
	g_free(build_info.custom_target);

	if (default_menu_items.menu != NULL && GTK_IS_WIDGET(default_menu_items.menu))
		gtk_widget_destroy(default_menu_items.menu);
	if (latex_menu_items.menu != NULL && GTK_IS_WIDGET(latex_menu_items.menu))
		gtk_widget_destroy(latex_menu_items.menu);
}


static GPid build_compile_tex_file(GeanyDocument *doc, gint mode)
{
	const gchar *cmd = NULL;

	if (doc == NULL || doc->file_name == NULL)
		return (GPid) 1;

	if (mode == LATEX_CMD_TO_DVI)
	{
		cmd = doc->file_type->programs->compiler;
		build_info.type = GBO_COMPILE;
	}
	else
	{
		cmd = doc->file_type->programs->linker;
		build_info.type = GBO_BUILD;
	}

	return build_spawn_cmd(doc, cmd, NULL);
}


static GPid build_view_tex_file(GeanyDocument *doc, gint mode)
{
	gchar **argv, **term_argv;
	gchar  *executable = NULL;
	gchar  *view_file = NULL;
	gchar  *locale_filename = NULL;
	gchar  *cmd_string = NULL;
	gchar  *locale_cmd_string = NULL;
	gchar  *locale_term_cmd;
	gchar  *script_name;
	gchar  *working_dir;
	gint	term_argv_len, i;
	GError *error = NULL;
	struct stat st;

	if (doc == NULL || doc->file_name == NULL)
		return (GPid) 1;

	run_info.file_type_id = GEANY_FILETYPES_LATEX;

	executable = utils_remove_ext_from_filename(doc->file_name);
	view_file = g_strconcat(executable, (mode == LATEX_CMD_VIEW_DVI) ? ".dvi" : ".pdf", NULL);

	/* try convert in locale for stat() */
	locale_filename = utils_get_locale_from_utf8(view_file);

	/* check whether view_file exists */
	if (g_stat(locale_filename, &st) != 0)
	{
		ui_set_statusbar(TRUE, _("Failed to view %s (make sure it is already compiled)"), view_file);
		utils_free_pointers(executable, view_file, locale_filename, NULL);

		return (GPid) 1;
	}

	/* replace %f and %e in the run_cmd string */
	cmd_string = g_strdup((mode == LATEX_CMD_VIEW_DVI) ?
										g_strdup(doc->file_type->programs->run_cmd) :
										g_strdup(doc->file_type->programs->run_cmd2));
	cmd_string = utils_str_replace(cmd_string, "%f", view_file);
	cmd_string = utils_str_replace(cmd_string, "%e", executable);

	/* try convert in locale */
	locale_cmd_string = utils_get_locale_from_utf8(cmd_string);

	/* get the terminal path */
	locale_term_cmd = utils_get_locale_from_utf8(tool_prefs.term_cmd);
	/* split the term_cmd, so arguments will work too */
	term_argv = g_strsplit(locale_term_cmd, " ", -1);
	term_argv_len = g_strv_length(term_argv);

	/* check that terminal exists (to prevent misleading error messages) */
	if (term_argv[0] != NULL)
	{
		gchar *tmp = term_argv[0];
		/* g_find_program_in_path checks tmp exists and is executable */
		term_argv[0] = g_find_program_in_path(tmp);
		g_free(tmp);
	}
	if (term_argv[0] == NULL)
	{
		ui_set_statusbar(TRUE,
			_("Could not find terminal \"%s\" "
				"(check path for Terminal tool setting in Preferences)"), tool_prefs.term_cmd);

		utils_free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										locale_term_cmd, NULL);
		g_strfreev(term_argv);
		return (GPid) 1;
	}

	/* RUN_SCRIPT_CMD should be ok in UTF8 without converting in locale because
	 * it contains no umlauts */
	working_dir = g_path_get_dirname(locale_filename); /** TODO do we need project support here? */
	script_name = g_build_filename(working_dir, RUN_SCRIPT_CMD, NULL);
	if (! build_create_shellscript(script_name, locale_cmd_string, TRUE))
	{
		ui_set_statusbar(TRUE, _("Failed to execute \"%s\" (start-script could not be created)"),
													executable);
		utils_free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										locale_term_cmd, working_dir, NULL);
		g_strfreev(term_argv);
		return (GPid) 1;
	}
	g_free(working_dir);

	argv = g_new0(gchar *, term_argv_len + 3);
	for (i = 0; i < term_argv_len; i++)
	{
		argv[i] = g_strdup(term_argv[i]);
	}
#ifdef G_OS_WIN32
		/* command line arguments only for cmd.exe */
		if (strstr(argv[0], "cmd.exe") != NULL)
		{
			argv[term_argv_len   ]  = g_strdup("/Q /C");
			argv[term_argv_len + 1] = script_name;
		}
		else
		{
			argv[term_argv_len    ] = script_name;
			argv[term_argv_len + 1] = NULL;
		}
#else
	argv[term_argv_len   ]  = g_strdup("-e");
	argv[term_argv_len + 1] = script_name;
#endif
	argv[term_argv_len + 2] = NULL;


	if (! g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &(run_info.pid), &error))
	{
		geany_debug("g_spawn_async() failed: %s", error->message);
		ui_set_statusbar(TRUE, _("Process failed (%s)"), error->message);

		utils_free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										locale_term_cmd, NULL);
		g_strfreev(argv);
		g_strfreev(term_argv);
		g_error_free(error);
		error = NULL;
		return (GPid) 0;
	}

	if (run_info.pid > 0)
	{
		/*setpgid(0, getppid());*/
		g_child_watch_add(run_info.pid, (GChildWatchFunc) run_exit_cb, NULL);
		build_menu_update(doc);
	}

	utils_free_pointers(executable, view_file, locale_filename, cmd_string, locale_cmd_string,
										locale_term_cmd, NULL);
	g_strfreev(argv);
	g_strfreev(term_argv);

	return run_info.pid;
}


/* get curfile.o in locale encoding from document::file_name */
static gchar *get_object_filename(GeanyDocument *doc)
{
	gchar *locale_filename, *short_file, *noext, *object_file;

	if (doc->file_name == NULL) return NULL;

	locale_filename = utils_get_locale_from_utf8(doc->file_name);

	short_file = g_path_get_basename(locale_filename);
	g_free(locale_filename);

	noext = utils_remove_ext_from_filename(short_file);
	g_free(short_file);

	object_file = g_strdup_printf("%s.o", noext);
	g_free(noext);

	return object_file;
}


static GPid build_make_file(GeanyDocument *doc, gint build_opts)
{
	GString *cmdstr;
	gchar *dir = NULL;
	gchar *part1=NULL;
	gchar *part2=NULL;
	GPid pid;

	if (doc == NULL || doc->file_name == NULL)
		return (GPid) 1;

    part1 = tool_prefs.make_cmd;
    
	if (build_opts == GBO_MAKE_OBJECT)
	{
		build_info.type = build_opts;
		if( app->project!=NULL && app->project->build_3_cmd != NULL )
            part1 = app->project->build_3_cmd;
        else  part2 = "\"%e.o\"";
	}
	else if (build_opts == GBO_MAKE_CUSTOM && build_info.custom_target)
	{
		build_info.type = GBO_MAKE_CUSTOM;
		if( app->project != NULL && app->project->build_2_cmd != NULL )
		    part1 = app->project->build_2_cmd;
        else part2 = build_info.custom_target;
		dir = project_get_make_dir();
	}
	else	/* GBO_MAKE_ALL */
	{
		build_info.type = GBO_MAKE_ALL;
		if( app->project != NULL && app->project->build_1_cmd != NULL )
		    part1 = app->project->build_1_cmd;
        else part2 = "all";
		dir = project_get_make_dir();
	}
	
	cmdstr = g_string_new(part1);
	if( part2!=NULL )
	{
	    g_string_append_c(cmdstr, ' ');
	    g_string_append( cmdstr, part2 );
    }

	pid = build_spawn_cmd(doc, cmdstr->str, dir); /* if dir is NULL, idx filename is used */
	g_free(dir);
	g_string_free(cmdstr, TRUE);
	return pid;
}


static GPid build_compile_file(GeanyDocument *doc)
{
	if (doc == NULL || doc->file_name == NULL)
		return (GPid) 1;

	build_info.type = GBO_COMPILE;
	return build_spawn_cmd(doc, doc->file_type->programs->compiler, NULL);
}


static GPid build_link_file(GeanyDocument *doc)
{
	if (doc == NULL || doc->file_name == NULL)
		return (GPid) 1;

	build_info.type = GBO_BUILD;
	return build_spawn_cmd(doc, doc->file_type->programs->linker, NULL);
}


/* If linking, clear all error indicators in all documents.
 * Otherwise, just clear error indicators in document idx. */
static void clear_errors(GeanyDocument *doc)
{
	switch (build_info.type)
	{
		case GBO_COMPILE:
		case GBO_MAKE_OBJECT:
			g_return_if_fail(doc);
			editor_clear_indicators(doc->editor);
			break;

		case GBO_BUILD:
		case GBO_MAKE_ALL:
		case GBO_MAKE_CUSTOM:
		{
			guint i;

			for (i = 0; i < documents_array->len; i++)
			{
				if (documents[i]->is_valid)
					editor_clear_indicators(documents[i]->editor);
			}
			break;
		}
	}
}


#ifdef G_OS_WIN32
/* cmd is a command line separated with spaces, first element will be escaped with double quotes
 * and a newly allocated string will be returned */
static gchar *quote_executable(const gchar *cmd)
{
	gchar **fields;
	gchar *result;

	if (! NZV(cmd))
		return NULL;

	fields = g_strsplit(cmd, " ", 2);
	if (fields == NULL || g_strv_length(fields) != 2)
		return g_strdup(cmd);

	result = g_strconcat("\"", fields[0], "\" ", fields[1], NULL);

	g_strfreev(fields);
	return result;
}
#endif


/* dir is the UTF-8 working directory to run cmd in. It can be NULL to use the
 * idx document directory */
static GPid build_spawn_cmd(GeanyDocument *doc, const gchar *cmd, const gchar *dir)
{
	GError  *error = NULL;
	gchar  **argv;
	gchar	*working_dir;
	gchar	*utf8_working_dir;
	gchar	*cmd_string;
	gchar	*utf8_cmd_string;
	gchar	*locale_filename;
	gchar	*executable;
	gchar	*tmp;
	gint     stdout_fd;
	gint     stderr_fd;

	g_return_val_if_fail(doc != NULL, (GPid) 1);
	clear_errors(doc);
	setptr(current_dir_entered, NULL);

	locale_filename = utils_get_locale_from_utf8(doc->file_name);
	executable = utils_remove_ext_from_filename(locale_filename);

	cmd_string = g_strdup(cmd);
	/* replace %f and %e in the command string */
	tmp = g_path_get_basename(locale_filename);
	cmd_string = utils_str_replace(cmd_string, "%f", tmp);
	g_free(tmp);
	tmp = g_path_get_basename(executable);
	cmd_string = utils_str_replace(cmd_string, "%e", tmp);
	g_free(tmp);
	g_free(executable);

#ifdef G_OS_WIN32
	/* due to g_shell_parse_argv() we need to enclose the command(first element) of cmd_string with
	 * "" if the command contains a full path(i.e. backslashes) otherwise the backslashes will be
	 * eaten by g_shell_parse_argv(). */
	setptr(cmd_string, quote_executable(cmd_string));
	if (! g_shell_parse_argv(cmd_string, NULL, &argv, NULL))
		/* if automatic parsing failed, fall back to simple, unsafe argv creation */
		argv = g_strsplit(cmd_string, " ", 0);
#else
	argv = g_new0(gchar *, 4);
	argv[0] = g_strdup("/bin/sh");
	argv[1] = g_strdup("-c");
	argv[2] = cmd_string;
	argv[3] = NULL;
#endif

	utf8_cmd_string = utils_get_utf8_from_locale(cmd_string);
	utf8_working_dir = (dir != NULL) ? g_strdup(dir) :
		g_path_get_dirname(doc->file_name);
	working_dir = utils_get_locale_from_utf8(utf8_working_dir);

	gtk_list_store_clear(msgwindow.store_compiler);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
	msgwin_compiler_add_fmt(COLOR_BLUE, _("%s (in directory: %s)"), utf8_cmd_string, utf8_working_dir);
	g_free(utf8_working_dir);
	g_free(utf8_cmd_string);

	/* set the build info for the message window */
	g_free(build_info.dir);
	build_info.dir = g_strdup(working_dir);
	build_info.file_type_id = FILETYPE_ID(doc->file_type);

	if (! g_spawn_async_with_pipes(working_dir, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &(build_info.pid), NULL, &stdout_fd, &stderr_fd, &error))
	{
		geany_debug("g_spawn_async_with_pipes() failed: %s", error->message);
		ui_set_statusbar(TRUE, _("Process failed (%s)"), error->message);
		g_strfreev(argv);
		g_error_free(error);
		g_free(working_dir);
		g_free(locale_filename);
		error = NULL;
		return (GPid) 0;
	}

	if (build_info.pid > 0)
	{
		g_child_watch_add(build_info.pid, (GChildWatchFunc) build_exit_cb, NULL);
		build_menu_update(doc);
	}

	/* use GIOChannels to monitor stdout and stderr */
	utils_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
		TRUE, build_iofunc, GINT_TO_POINTER(0));
	utils_set_up_io_channel(stderr_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
		TRUE, build_iofunc, GINT_TO_POINTER(1));

	g_strfreev(argv);
	g_free(working_dir);
	g_free(locale_filename);

	return build_info.pid;
}


/* Checks if the executable file corresponding to document idx exists.
 * Returns the name part of the filename, without extension.
 * Returns NULL if executable file doesn't exist. */
static gchar *get_build_executable(const gchar *locale_filename, gboolean check_exists,
		filetype_id ft_id)
{
	gchar *long_executable = NULL;
	struct stat st;

	long_executable = utils_remove_ext_from_filename(locale_filename);
#ifdef G_OS_WIN32
	setptr(long_executable, g_strconcat(long_executable, ".exe", NULL));
#endif

	if (check_exists)
	{
		gchar *check_executable = NULL;

		/* add .class extension for JAVA source files (only for stat) */
		if (ft_id == GEANY_FILETYPES_JAVA)
		{
#ifdef G_OS_WIN32
			gchar *tmp;
			/* there is already the extension .exe, so first remove it and then add .class */
			tmp = utils_remove_ext_from_filename(long_executable);
			check_executable = g_strconcat(tmp, ".class", NULL);
			/* store the filename without "exe" extension for Java, tmp will be freed by setptr */
			setptr(long_executable, tmp);
#else
			check_executable = g_strconcat(long_executable, ".class", NULL);
#endif
		}
		else
			check_executable = g_strdup(long_executable);

		/* check for filename extension and abort if filename doesn't have one */
		if (utils_str_equal(locale_filename, check_executable))
		{
			ui_set_statusbar(TRUE, _("Command stopped because the current file has no extension."));
			utils_beep();
			g_free(check_executable);
			return NULL;
		}

		/* check whether executable exists */
		if (g_stat(check_executable, &st) != 0)
		{
			gchar *utf8_check_executable = utils_get_utf8_from_locale(check_executable);

			ui_set_statusbar(TRUE, _("Failed to execute \"%s\" (make sure it is already built)"),
														utf8_check_executable);
			g_free(utf8_check_executable);
			g_free(check_executable);
			return NULL;
		}
		g_free(check_executable);
	}

	/* remove path */
	setptr(long_executable, g_path_get_basename(long_executable));
	return long_executable;
}


/* Returns: NULL if there was an error, or the working directory the script was created in.
 * vte_cmd_nonscript is the location of a string which is filled with the command to be used
 * when vc->skip_run_script is set, otherwise it will be set to NULL */
static gchar *prepare_run_script(GeanyDocument *doc, gchar **vte_cmd_nonscript)
{
	gchar	*locale_filename = NULL;
	gboolean have_project;
	GeanyProject *project = app->project;
	GeanyFiletype *ft = doc->file_type;
	gboolean check_exists;
	gchar	*cmd = NULL;
	gchar	*executable = NULL;
	gchar	*working_dir = NULL;
	gboolean autoclose = FALSE;
	gboolean result = FALSE;
	gchar	*tmp;

	if (vte_cmd_nonscript != NULL)
		*vte_cmd_nonscript = NULL;

	locale_filename = utils_get_locale_from_utf8(doc->file_name);

	have_project = (project != NULL && NZV(project->run_cmd));
	cmd = (have_project) ?
		project->run_cmd :
		ft->programs->run_cmd;

	/* only check for existing executable, if executable is required by %e */
	check_exists = (strstr(cmd, "%e") != NULL);
	executable = get_build_executable(locale_filename, check_exists, FILETYPE_ID(ft));
	if (executable == NULL)
	{
		g_free(locale_filename);
		return NULL;
	}

	if (have_project)
	{
		gchar *project_base_path = project_get_base_path();
		working_dir = utils_get_locale_from_utf8(project_base_path);
		g_free(project_base_path);
	}
	else
		working_dir = g_path_get_dirname(locale_filename);

	/* only test whether working dir exists, don't change it or else Windows support will break
	 * (gspawn-win32-helper.exe is used by GLib and must be in $PATH which means current working
	 *  dir where geany.exe was started from, so we can't change it) */
	if (! g_file_test(working_dir, G_FILE_TEST_EXISTS) ||
		! g_file_test(working_dir, G_FILE_TEST_IS_DIR))
	{
		gchar *utf8_working_dir = utils_get_utf8_from_locale(working_dir);

		ui_set_statusbar(TRUE, _("Failed to change the working directory to \"%s\""), utf8_working_dir);
		utils_free_pointers(utf8_working_dir, working_dir, executable, locale_filename, NULL);
		return NULL;
	}

	/* replace %f and %e in the run_cmd string */
	cmd = g_strdup(cmd);
	tmp = g_path_get_basename(locale_filename);
	cmd = utils_str_replace(cmd, "%f", tmp);
	g_free(tmp);
	cmd = utils_str_replace(cmd, "%e", executable);

#ifdef HAVE_VTE
	if (vte_info.load_vte && vc != NULL && vc->run_in_vte)
	{
		if (vc->skip_run_script)
		{
			if (vte_cmd_nonscript != NULL)
				*vte_cmd_nonscript = cmd;

			utils_free_pointers(executable, locale_filename, NULL);
			return working_dir;
		}
		else
			/* don't wait for user input at the end of script when we are running in VTE */
			autoclose = TRUE;
	}
#endif

	/* RUN_SCRIPT_CMD should be ok in UTF8 without converting in locale because it
	 * contains no umlauts */
	tmp = g_build_filename(working_dir, RUN_SCRIPT_CMD, NULL);
	result = build_create_shellscript(tmp, cmd, autoclose);
	if (! result)
	{
		gchar *utf8_cmd = utils_get_utf8_from_locale(cmd);

		ui_set_statusbar(TRUE, _("Failed to execute \"%s\" (start-script could not be created)"),
			utf8_cmd);
		g_free(utf8_cmd);
	}

	utils_free_pointers(tmp, cmd, executable, locale_filename, NULL);

	if (result)
		return working_dir;

	g_free(working_dir);
	return NULL;
}


static GPid build_run_cmd(GeanyDocument *doc)
{
	gchar	*working_dir;
	gchar	*vte_cmd_nonscript = NULL;
	GError	*error = NULL;

	if (doc == NULL || doc->file_name == NULL)
		return (GPid) 0;

	working_dir = prepare_run_script(doc, &vte_cmd_nonscript);
	if (working_dir == NULL)
		return (GPid) 0;

	run_info.file_type_id = FILETYPE_ID(doc->file_type);

#ifdef HAVE_VTE
	if (vte_info.load_vte && vc != NULL && vc->run_in_vte)
	{
		GeanyProject *project = app->project;
		gchar *vte_cmd;

		if (vc->skip_run_script)
		{
			setptr(vte_cmd_nonscript, utils_get_utf8_from_locale(vte_cmd_nonscript));
			vte_cmd = g_strconcat(vte_cmd_nonscript, "\n", NULL);
			g_free(vte_cmd_nonscript);
		}
		else
			vte_cmd = g_strconcat("\n/bin/sh ", RUN_SCRIPT_CMD, "\n", NULL);

		/* change into current directory if it is not done by default or we have a project and
		 * project run command(working_dir is already set accordingly) */
		if (! vc->follow_path || (project != NULL && NZV(project->run_cmd)))
		{
			/* we need to convert the working_dir back to UTF-8 because the VTE expects it */
			gchar *utf8_working_dir = utils_get_utf8_from_locale(working_dir);
			vte_cwd(utf8_working_dir, TRUE);
			g_free(utf8_working_dir);
		}
		if (! vte_send_cmd(vte_cmd))
			ui_set_statusbar(FALSE,
		_("Could not execute the file in the VTE because it probably contains a command."));

		/* show the VTE */
		gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_VTE);
		gtk_widget_grab_focus(vc->vte);
		msgwin_show_hide(TRUE);

		run_info.pid = 1;

		g_free(vte_cmd);
	}
	else
#endif
	{
		gchar	*locale_term_cmd = NULL;
		gchar  **term_argv = NULL;
		guint    term_argv_len, i;
		gchar  **argv = NULL;

		/* get the terminal path */
		locale_term_cmd = utils_get_locale_from_utf8(tool_prefs.term_cmd);
		/* split the term_cmd, so arguments will work too */
		term_argv = g_strsplit(locale_term_cmd, " ", -1);
		term_argv_len = g_strv_length(term_argv);

		/* check that terminal exists (to prevent misleading error messages) */
		if (term_argv[0] != NULL)
		{
			gchar *tmp = term_argv[0];
			/* g_find_program_in_path checks whether tmp exists and is executable */
			term_argv[0] = g_find_program_in_path(tmp);
			g_free(tmp);
		}
		if (term_argv[0] == NULL)
		{
			ui_set_statusbar(TRUE,
				_("Could not find terminal \"%s\" "
					"(check path for Terminal tool setting in Preferences)"), tool_prefs.term_cmd);
			run_info.pid = (GPid) 1;
			goto free_strings;
		}

		argv = g_new0(gchar *, term_argv_len + 3);
		for (i = 0; i < term_argv_len; i++)
		{
			argv[i] = g_strdup(term_argv[i]);
		}
#ifdef G_OS_WIN32
		/* command line arguments only for cmd.exe */
		if (strstr(argv[0], "cmd.exe") != NULL)
		{
			argv[term_argv_len   ]  = g_strdup("/Q /C");
			argv[term_argv_len + 1] = g_strdup(RUN_SCRIPT_CMD);
		}
		else
		{
			argv[term_argv_len    ] = g_strdup(RUN_SCRIPT_CMD);
			argv[term_argv_len + 1] = NULL;
		}
#else
		argv[term_argv_len   ]  = g_strdup("-e");
		argv[term_argv_len + 1] = g_strconcat("/bin/sh ", RUN_SCRIPT_CMD, NULL);
#endif
		argv[term_argv_len + 2] = NULL;

		if (! g_spawn_async(working_dir, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
							NULL, NULL, &(run_info.pid), &error))
		{
			geany_debug("g_spawn_async() failed: %s", error->message);
			ui_set_statusbar(TRUE, _("Process failed (%s)"), error->message);
			g_unlink(RUN_SCRIPT_CMD);
			g_error_free(error);
			error = NULL;
			run_info.pid = (GPid) 0;
		}

		if (run_info.pid > 0)
		{
			g_child_watch_add(run_info.pid, (GChildWatchFunc) run_exit_cb, NULL);
			build_menu_update(doc);
		}
		free_strings:
		g_strfreev(argv);
		g_strfreev(term_argv);
		g_free(locale_term_cmd);
	}

	g_free(working_dir);
	return run_info.pid;
}


static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		/*GIOStatus s;*/
		gchar *msg;

		while (g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL) && msg)
		{
			/*if (s != G_IO_STATUS_NORMAL && s != G_IO_STATUS_EOF) break;*/
			gint color;
			gchar *tmp;

			color = (GPOINTER_TO_INT(data)) ? COLOR_DARK_RED : COLOR_BLACK;
			g_strstrip(msg);

			if (build_parse_make_dir(msg, &tmp))
			{
				setptr(current_dir_entered, tmp);
			}

			if (editor_prefs.use_indicators)
			{
				gchar *filename;
				gint line;

				msgwin_parse_compiler_error_line(msg, current_dir_entered,
					&filename, &line);
				if (line != -1 && filename != NULL)
				{
					GeanyDocument *doc = document_find_by_filename(filename);

					if (doc)
						editor_set_indicator_on_line(doc->editor, line - 1);
					color = COLOR_RED;	/* error message parsed on the line */
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


gboolean build_parse_make_dir(const gchar *string, gchar **prefix)
{
	const gchar *pos;

	*prefix = NULL;

	if (string == NULL)
		return FALSE;

	if ((pos = strstr(string, "Entering directory")) != NULL)
	{
		gsize len;
		gchar *input;

		/* get the start of the path */
		pos = strstr(string, "/");

		if (pos == NULL)
			return FALSE;

		input = g_strdup(pos);

		/* kill the ' at the end of the path */
		len = strlen(input);
		input[len - 1] = '\0';
		input = g_realloc(input, len);	/* shorten by 1 */
		*prefix = input;

		return TRUE;
	}

	return FALSE;
}


static void show_build_result_message(gboolean failure)
{
	gchar *msg;

	if (failure)
	{
		msg = _("Compilation failed.");
		msgwin_compiler_add(COLOR_DARK_RED, msg);
		/* If msgwindow is hidden, user will want to display it to see the error */
		if (! ui_prefs.msgwindow_visible)
		{
			gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
			msgwin_show_hide(TRUE);
		}
		else
		if (gtk_notebook_get_current_page(GTK_NOTEBOOK(msgwindow.notebook)) != MSG_COMPILER)
			ui_set_statusbar(FALSE, "%s", msg);
	}
	else
	{
		msg = _("Compilation finished successfully.");
		msgwin_compiler_add(COLOR_BLUE, msg);
		if (! ui_prefs.msgwindow_visible ||
			gtk_notebook_get_current_page(GTK_NOTEBOOK(msgwindow.notebook)) != MSG_COMPILER)
				ui_set_statusbar(FALSE, "%s", msg);
	}
}


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
		/* the terminating signal: WTERMSIG (status)); */
		failure = TRUE;
	}
	else
	{	/* any other failure occured */
		failure = TRUE;
	}
	show_build_result_message(failure);
#else
	show_build_result_message(! win32_get_exit_status(child_pid));
#endif

	utils_beep();
	g_spawn_close_pid(child_pid);

	build_info.pid = 0;
	/* enable build items again */
	build_menu_update(NULL);
}


static void run_exit_cb(GPid child_pid, gint status, gpointer user_data)
{
	g_spawn_close_pid(child_pid);

	run_info.pid = 0;
	/* reset the stop button and menu item to the original meaning */
	build_menu_update(NULL);
}


/* write a little shellscript to call the executable (similar to anjuta_launcher but "internal")
 * fname is the full file name (including path) for the script to create */
static gboolean build_create_shellscript(const gchar *fname, const gchar *cmd, gboolean autoclose)
{
	FILE *fp;
	gchar *str;

	fp = g_fopen(fname, "w");
	if (! fp) return FALSE;
#ifdef G_OS_WIN32
	str = g_strdup_printf("%s\n\n%s\ndel %s\n", cmd, (autoclose) ? "" : "pause", fname);
#else
	str = g_strdup_printf(
		"#!/bin/sh\n\n%s\n\necho \"\n\n------------------\n(program exited with code: $?)\" \
		\n\n%s\nrm $0\n", cmd, (autoclose) ? "" :
		"\necho \"Press return to continue\"\n#to be more compatible with shells like dash\ndummy_var=\"\"\nread dummy_var");
#endif

	fputs(str, fp);
	g_free(str);

	fclose(fp);

	return TRUE;
}


/* note: copied from keybindings.c.
 * Perhaps the separate Tex menu could be merged with the default build menu?
 * Then this could be done with Glade and set the accels in keybindings.c. */
static void add_menu_accel(GeanyKeyGroup *group, guint kb_id,
	GtkAccelGroup *accel_group, GtkWidget *menuitem)
{
	GeanyKeyBinding *kb = &group->keys[kb_id];

	if (kb->key != 0)
		gtk_widget_add_accelerator(menuitem, "activate", accel_group,
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
}


#define GEANY_ADD_WIDGET_ACCEL(kb_id, menuitem) \
	add_menu_accel(group, kb_id, accel_group, menuitem)

static void create_build_menu_gen(BuildMenuItems *menu_items)
{
	GtkWidget *menu, *item = NULL, *image, *separator;
	struct GeanyProject *proj;
	gchar *tiptext;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(main_widgets.window, "tooltips"));
	GeanyKeyGroup *group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_BUILD);

	menu = gtk_menu_new();

	/* compile the code */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Compile"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_COMPILE, item);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_compile_activate), NULL);
	menu_items->item_compile = item;

	/* build the code */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Build"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Builds the current file (generate an executable file)"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_LINK, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_build_activate), NULL);
	menu_items->item_link = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	/* build the code with make all or prefs or project command*/
	proj = app->project;
	if( proj!=NULL && proj->build_1_label!=NULL )
	{
	    item = gtk_image_menu_item_new_with_mnemonic( proj->build_1_label );
	    tiptext = NULL; /* user label so no tip needed */
	}/* else if prefs */
	else
	{
	    item = gtk_image_menu_item_new_with_mnemonic(_("_Make All"));
	    tiptext = _("Builds the current file with the make tool and the default target");
    }
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	if( tiptext != NULL )gtk_tooltips_set_tip(tooltips, item, tiptext, NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKE, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_ALL));
	menu_items->item_make_all = item;

	/* build the code with make custom */
	if( proj!=NULL && proj->build_2_label!=NULL )
	{
	    item = gtk_image_menu_item_new_with_mnemonic( proj->build_2_label );
	    tiptext = _("Dialog contents appended to this command");
	}/* else if prefs */
	else
	{
	    item = gtk_image_menu_item_new_with_mnemonic(_("Make Custom _Target"));
	    tiptext = _("Builds the current file with the make tool and the specified target");
    }
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOWNTARGET, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, tiptext, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_CUSTOM));
	menu_items->item_make_custom = item;

	/* build the code with make object */
	if( proj!=NULL && proj->build_2_label!=NULL )
	{
	    item = gtk_image_menu_item_new_with_mnemonic( proj->build_2_label );
	    tiptext = NULL;
	}/* else if prefs */
	else
	{
	    item = gtk_image_menu_item_new_with_mnemonic(_("Make _Object"));
	    tiptext = _("Compiles the current file using the make tool");
    }
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOBJECT, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, tiptext, NULL);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_OBJECT));
	menu_items->item_make_object = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	/* next error */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Next Error"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_NEXTERROR, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_next_error), NULL);
	menu_items->item_next_error = item;

	item = gtk_image_menu_item_new_with_mnemonic(_("_Previous Error"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_PREVIOUSERROR, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_previous_error), NULL);
	menu_items->item_previous_error = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	/* execute the code */
	item = gtk_image_menu_item_new_from_stock("gtk-execute", accel_group);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Run or view the current file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_RUN, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_execute_activate), NULL);
	menu_items->item_exec = item;

	separator = gtk_separator_menu_item_new();
	gtk_widget_show(separator);
	gtk_container_add(GTK_CONTAINER(menu), separator);
	gtk_widget_set_sensitive(separator, FALSE);

	/* arguments */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Set Build Menu Commands"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_OPTIONS, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Sets the includes and library paths for the compiler and "
				  "the program arguments for execution"), NULL);
	image = gtk_image_new_from_stock("gtk-preferences", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_arguments_activate), NULL);
	menu_items->item_set_args = item;

    if( menu_items->menu ) g_object_unref( (gpointer)menu_items->menu ); /* free it */
	menu_items->menu = menu;
	g_object_ref((gpointer)menu_items->menu);	/* to hold it after removing */
}

/* externally callable build default menu for when projects change menu */

void build_default_menu()
{
    create_build_menu_gen( &default_menu_items );
};

static void create_build_menu_tex(BuildMenuItems *menu_items)
{
	GtkWidget *menu, *item, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(main_widgets.window, "tooltips"));
	GeanyKeyGroup *group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_BUILD);

	menu = gtk_menu_new();

	/* DVI */
	item = gtk_image_menu_item_new_with_mnemonic(_("LaTeX -> _DVI"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a DVI file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_COMPILE, item);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(item, "activate",
				G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(LATEX_CMD_TO_DVI));
	menu_items->item_compile = item;

	/* PDF */
	item = gtk_image_menu_item_new_with_mnemonic(_("LaTeX -> _PDF"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a PDF file"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_LINK, item);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(item, "activate",
				G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(LATEX_CMD_TO_PDF));
	menu_items->item_link = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	/* build the code with make all */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Make All"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKE, item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_ALL));
	menu_items->item_make_all = item;

	/* build the code with make custom */
	item = gtk_image_menu_item_new_with_mnemonic(_("Make Custom _Target"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOWNTARGET, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_make_activate),
		GINT_TO_POINTER(GBO_MAKE_CUSTOM));
	menu_items->item_make_custom = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	/* next error */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Next Error"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_NEXTERROR, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_next_error), NULL);
	menu_items->item_next_error = item;

	item = gtk_image_menu_item_new_with_mnemonic(_("_Previous Error"));
	gtk_widget_show(item);
	/*GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_, item);*/
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_build_previous_error), NULL);
	menu_items->item_previous_error = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	/* DVI view */
#define LATEX_VIEW_DVI_LABEL _("_View DVI File") /* used later again */
	item = gtk_image_menu_item_new_with_mnemonic(LATEX_VIEW_DVI_LABEL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_RUN, item);
	gtk_tooltips_set_tip(tooltips, item, _("Compile and view the current file"), NULL);
	image = gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(item, "activate",
					G_CALLBACK(on_build_execute_activate), GINT_TO_POINTER(LATEX_CMD_VIEW_DVI));
	menu_items->item_exec = item;

	/* PDF view */
	item = gtk_image_menu_item_new_with_mnemonic(_("V_iew PDF File"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_RUN2, item);
	gtk_tooltips_set_tip(tooltips, item, _("Compile and view the current file"), NULL);
	image = gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(item, "activate",
					G_CALLBACK(on_build_execute_activate), GINT_TO_POINTER(LATEX_CMD_VIEW_PDF));
	menu_items->item_exec2 = item;

	/* separator */
	separator = gtk_separator_menu_item_new();
	gtk_widget_show(separator);
	gtk_container_add(GTK_CONTAINER(menu), separator);
	gtk_widget_set_sensitive(separator, FALSE);

	/* arguments */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Set Arguments"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_OPTIONS, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Sets the program paths and arguments"), NULL);
	image = gtk_image_new_from_stock("gtk-preferences", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect(item, "activate",
		G_CALLBACK(on_build_arguments_activate), filetypes[GEANY_FILETYPES_LATEX]);
	menu_items->item_set_args = item;

	gtk_window_add_accel_group(GTK_WINDOW(main_widgets.window), accel_group);

	menu_items->menu = menu;
	g_object_ref((gpointer)menu_items->menu);	/* to hold it after removing */
}


static void
on_includes_arguments_tex_dialog_response  (GtkDialog *dialog,
                                            gint response,
                                            gpointer user_data)
{
	GeanyFiletype *ft = user_data;
	g_return_if_fail(ft != NULL);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		const gchar *newstr;
		struct build_programs *programs = ft->programs;

		newstr = gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry1")));
		if (! utils_str_equal(newstr, programs->compiler))
		{
			if (programs->compiler) g_free(programs->compiler);
			programs->compiler = g_strdup(newstr);
			programs->modified = TRUE;
		}
		newstr = gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry2")));
		if (! utils_str_equal(newstr, programs->linker))
		{
			if (programs->linker) g_free(programs->linker);
			programs->linker = g_strdup(newstr);
			programs->modified = TRUE;
		}
		newstr = gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry3")));
		if (! utils_str_equal(newstr, programs->run_cmd))
		{
			if (programs->run_cmd) g_free(programs->run_cmd);
			programs->run_cmd = g_strdup(newstr);
			programs->modified = TRUE;
		}
		newstr = gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry4")));
		if (! utils_str_equal(newstr, programs->run_cmd2))
		{
			if (programs->run_cmd2) g_free(programs->run_cmd2);
			programs->run_cmd2 = g_strdup(newstr);
			programs->modified = TRUE;
		}
	}
}


static void show_includes_arguments_tex(void)
{
	GtkWidget *dialog, *label, *entries[4], *vbox, *table;
	GeanyDocument *doc = document_get_current();
	gint response;
	GeanyFiletype *ft = NULL;

	if (doc != NULL)
		ft = doc->file_type;
	g_return_if_fail(ft != NULL);

	dialog = gtk_dialog_new_with_buttons(_("Set Arguments"), GTK_WINDOW(main_widgets.window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");

	label = gtk_label_new(_("Set programs and options for compiling and viewing (La)TeX files."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	/* LaTeX -> DVI args */
	if (ft->programs->compiler != NULL && ft->actions->can_compile)
	{
		label = gtk_label_new(_("DVI creation:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[0] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
		if (ft->programs->compiler)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[0]), ft->programs->compiler);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[0], 1, 2, 0, 1);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	}

	/* LaTeX -> PDF args */
	if (ft->programs->linker != NULL && ft->actions->can_link)
	{
		label = gtk_label_new(_("PDF creation:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[1] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
		if (ft->programs->linker)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[1]), ft->programs->linker);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[1], 1, 2, 1, 2);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	}

	/* View LaTeX -> DVI args */
	if (ft->programs->run_cmd != NULL)
	{
		label = gtk_label_new(_("DVI preview:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[2] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
		if (ft->programs->run_cmd)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[2]), ft->programs->run_cmd);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[2], 1, 2, 2, 3);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry3",
					gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);
	}

	/* View LaTeX -> PDF args */
	if (ft->programs->run_cmd2 != NULL)
	{
		label = gtk_label_new(_("PDF preview:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[3] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[3]), 30);
		if (ft->programs->run_cmd2)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[3]), ft->programs->run_cmd2);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[3], 1, 2, 3, 4);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry4",
					gtk_widget_ref(entries[3]), (GDestroyNotify)gtk_widget_unref);
	}

	label = gtk_label_new(_("%f will be replaced by the current filename, e.g. test_file.c\n"
							"%e will be replaced by the filename without extension, e.g. test_file"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	gtk_widget_show_all(dialog);
	/* run modally to prevent user changing idx filetype */
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	/* call the callback manually */
	on_includes_arguments_tex_dialog_response(GTK_DIALOG(dialog), response, ft);

	gtk_widget_destroy(dialog);
}


static void
on_includes_arguments_dialog_response  (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	GeanyFiletype *ft = user_data;

	g_return_if_fail(ft != NULL);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		const gchar *newstr;
		struct build_programs *programs = ft->programs;

		if (ft->actions->can_compile)
		{
			newstr = gtk_entry_get_text(
					GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry1")));
			if (! utils_str_equal(newstr, programs->compiler))
			{
				if (programs->compiler) g_free(programs->compiler);
				programs->compiler = g_strdup(newstr);
				programs->modified = TRUE;
			}
		}
		if (ft->actions->can_link)
		{
			newstr = gtk_entry_get_text(
					GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry2")));
			if (! utils_str_equal(newstr, programs->linker))
			{
				if (programs->linker) g_free(programs->linker);
				programs->linker = g_strdup(newstr);
				programs->modified = TRUE;
			}
		}
		if (ft->actions->can_exec)
		{
			newstr = gtk_entry_get_text(
					GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry3")));
			if (! utils_str_equal(newstr, programs->run_cmd))
			{
				if (programs->run_cmd) g_free(programs->run_cmd);
				programs->run_cmd = g_strdup(newstr);
				programs->modified = TRUE;
			}
		}
		if( app->project!=NULL )
		{
		    struct GeanyProject *proj = app->project;
		    
		    newstr = gtk_entry_get_text( GTK_ENTRY( lookup_widget( GTK_WIDGET(dialog), "build_1_label" ) ) );
		    if( !utils_str_equal( newstr, proj->build_1_label ) )
		    {
		        if( proj->build_1_label ) g_free( proj->build_1_label );
		        proj->build_1_label = g_strdup(newstr);
            }
		    newstr = gtk_entry_get_text( GTK_ENTRY( lookup_widget( GTK_WIDGET(dialog), "build_1_cmd" ) ) );
		    if( !utils_str_equal( newstr, proj->build_1_cmd ) )
		    {
		        if( proj->build_1_cmd ) g_free( proj->build_1_cmd );
		        proj->build_1_cmd = g_strdup(newstr);
            }
		    newstr = gtk_entry_get_text( GTK_ENTRY( lookup_widget( GTK_WIDGET(dialog), "build_2_label" ) ) );
		    if( !utils_str_equal( newstr, proj->build_2_label ) )
		    {
		        if( proj->build_2_label ) g_free( proj->build_2_label );
		        proj->build_2_label = g_strdup(newstr);
            }
		    newstr = gtk_entry_get_text( GTK_ENTRY( lookup_widget( GTK_WIDGET(dialog), "build_2_cmd" ) ) );
		    if( !utils_str_equal( newstr, proj->build_2_cmd ) )
		    {
		        if( proj->build_2_cmd ) g_free( proj->build_2_cmd );
		        proj->build_2_cmd = g_strdup(newstr);
            }
		    newstr = gtk_entry_get_text( GTK_ENTRY( lookup_widget( GTK_WIDGET(dialog), "build_3_label" ) ) );
		    if( !utils_str_equal( newstr, proj->build_3_label ) )
		    {
		        if( proj->build_3_label ) g_free( proj->build_3_label );
		        proj->build_3_label = g_strdup(newstr);
            }
		    newstr = gtk_entry_get_text( GTK_ENTRY( lookup_widget( GTK_WIDGET(dialog), "build_3_cmd" ) ) );
		    if( !utils_str_equal( newstr, proj->build_3_cmd ) )
		    {
		        if( proj->build_3_cmd ) g_free( proj->build_3_cmd );
		        proj->build_3_cmd = g_strdup(newstr);
            }
        }
	}
}


static void show_includes_arguments_gen(void)
{
	GtkWidget *dialog, *label, *entries[3], *vbox, *build_entry;
	GtkWidget *ft_table = NULL;
	GtkWidget *pr_table = NULL;
	gint row = 0;
	gint response;
	GeanyDocument *doc = document_get_current();
	GeanyFiletype *ft = NULL;

	if (doc != NULL)
		ft = doc->file_type;
	g_return_if_fail(ft != NULL);

	dialog = gtk_dialog_new_with_buttons(_("Build Menu Commands"), GTK_WINDOW(main_widgets.window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");

	label = gtk_label_new(_("Set the commands for the build menu."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	if (ft->actions->can_compile || ft->actions->can_link || ft->actions->can_exec)
	{
		GtkWidget *align, *frame;
		/* in-dialog heading for the file type part of the build commands dialog */
		gchar *frame_title = g_strdup_printf(_("%s commands"), ft->title);

		frame = ui_frame_new_with_alignment(frame_title, &align);
		gtk_container_add(GTK_CONTAINER(vbox), frame);
		g_free(frame_title);

		ft_table = gtk_table_new(3, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(ft_table), 6);
		gtk_container_add(GTK_CONTAINER(align), ft_table);
		row = 0;
	}

	/* include-args */
	if (ft->actions->can_compile)
	{
		label = gtk_label_new(_("Compile:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(ft_table), label, 0, 1, row, row + 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[0] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
		if (ft->programs->compiler)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[0]), ft->programs->compiler);
		}
		gtk_table_attach_defaults(GTK_TABLE(ft_table), entries[0], 1, 2, row, row + 1);
		row++;

		g_object_set_data_full(G_OBJECT(dialog), "includes_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	}

	/* lib-args */
	if (ft->actions->can_link)
	{
		label = gtk_label_new(_("Build:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(ft_table), label, 0, 1, row, row + 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[1] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
		if (ft->programs->linker)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[1]), ft->programs->linker);
		}
		gtk_table_attach_defaults(GTK_TABLE(ft_table), entries[1], 1, 2, row, row + 1);
		row++;

		g_object_set_data_full(G_OBJECT(dialog), "includes_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	}

	/* program-args */
	if (ft->actions->can_exec)
	{
		label = gtk_label_new(_("Execute:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(ft_table), label, 0, 1, row, row + 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[2] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
		if (ft->programs->run_cmd)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[2]), ft->programs->run_cmd);
		}
		gtk_table_attach_defaults(GTK_TABLE(ft_table), entries[2], 1, 2, row, row + 1);
		row++;

		g_object_set_data_full(G_OBJECT(dialog), "includes_entry3",
						gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);

		/* disable the run command if there is a valid project run command set */
		if (app->project && NZV(app->project->run_cmd))
			gtk_widget_set_sensitive(entries[2], FALSE);
	}

    /* see if need project based command fields */
    
    if( app->project!=NULL )
    {
 		GtkWidget *align, *frame;
		/* in-dialog heading for the project part of the build commands dialog */
		gchar *frame_title = g_strdup_printf(_("%s build menu commands"), "project");
        struct GeanyProject *proj = app->project;
        
		frame = ui_frame_new_with_alignment(frame_title, &align);
		gtk_container_add(GTK_CONTAINER(vbox), frame);
		g_free(frame_title);

		pr_table = gtk_table_new(3, 3, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(pr_table), 6);
		gtk_container_add(GTK_CONTAINER(align), pr_table);
		
		/* label and cmd 1 */
		build_entry = gtk_entry_new();
		gtk_entry_set_width_chars( GTK_ENTRY(build_entry), 10 );
		if( proj->build_1_label!=NULL )
		{
		    gtk_entry_set_text( GTK_ENTRY(build_entry), proj->build_1_label );
        }
        gtk_table_attach_defaults( GTK_TABLE(pr_table), build_entry, 0, 1, 0, 1 );
		g_object_set_data_full( G_OBJECT(dialog), "build_1_label",
					gtk_widget_ref(build_entry), (GDestroyNotify)gtk_widget_unref );
		build_entry = gtk_entry_new();
		gtk_entry_set_width_chars( GTK_ENTRY(build_entry), 30 );
		if( proj->build_1_cmd!=NULL )
		{
		    gtk_entry_set_text( GTK_ENTRY(build_entry), proj->build_1_cmd );
        }
        gtk_table_attach_defaults( GTK_TABLE(pr_table), build_entry, 1, 3, 0, 1 );
		g_object_set_data_full(G_OBJECT(dialog), "build_1_cmd",
					gtk_widget_ref(build_entry), (GDestroyNotify)gtk_widget_unref);

		/* label and cmd 2 */
		build_entry = gtk_entry_new();
		gtk_entry_set_width_chars( GTK_ENTRY(build_entry), 10 );
		if( proj->build_2_label!=NULL )
		{
		    gtk_entry_set_text( GTK_ENTRY(build_entry), proj->build_2_label );
        }
        gtk_table_attach_defaults( GTK_TABLE(pr_table), build_entry, 0, 1, 1, 2 );
		g_object_set_data_full( G_OBJECT(dialog), "build_2_label",
					gtk_widget_ref(build_entry), (GDestroyNotify)gtk_widget_unref );
		build_entry = gtk_entry_new();
		gtk_entry_set_width_chars( GTK_ENTRY(build_entry), 30 );
		if( proj->build_2_cmd!=NULL )
		{
		    gtk_entry_set_text( GTK_ENTRY(build_entry), proj->build_2_cmd );
        }
        gtk_table_attach_defaults( GTK_TABLE(pr_table), build_entry, 1, 3, 1, 2 );
		g_object_set_data_full(G_OBJECT(dialog), "build_2_cmd",
					gtk_widget_ref(build_entry), (GDestroyNotify)gtk_widget_unref);

		/* label and cmd 3 */
		build_entry = gtk_entry_new();
		gtk_entry_set_width_chars( GTK_ENTRY(build_entry), 10 );
		if( proj->build_3_label!=NULL )
		{
		    gtk_entry_set_text( GTK_ENTRY(build_entry), proj->build_3_label );
        }
        gtk_table_attach_defaults( GTK_TABLE(pr_table), build_entry, 0, 1, 2, 3 );
		g_object_set_data_full( G_OBJECT(dialog), "build_3_label",
					gtk_widget_ref(build_entry), (GDestroyNotify)gtk_widget_unref );
		build_entry = gtk_entry_new();
		gtk_entry_set_width_chars( GTK_ENTRY(build_entry), 30 );
		if( proj->build_3_cmd!=NULL )
		{
		    gtk_entry_set_text( GTK_ENTRY(build_entry), proj->build_3_cmd );
        }
        gtk_table_attach_defaults( GTK_TABLE(pr_table), build_entry, 1, 3, 2, 3 );
		g_object_set_data_full(G_OBJECT(dialog), "build_3_cmd",
					gtk_widget_ref(build_entry), (GDestroyNotify)gtk_widget_unref);
		
    }
    
	label = gtk_label_new(_("%f will be replaced by the current filename, e.g. test_file.c\n"
							"%e will be replaced by the filename without extension, e.g. test_file"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	gtk_widget_show_all(dialog);
	/* run modally to prevent user changing idx filetype */
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	/* call the callback manually */
	on_includes_arguments_dialog_response(GTK_DIALOG(dialog), response, ft);

	gtk_widget_destroy(dialog);
}


static void
on_build_arguments_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (user_data && FILETYPE_ID((GeanyFiletype*) user_data) == GEANY_FILETYPES_LATEX)
		show_includes_arguments_tex();
	else
		show_includes_arguments_gen();
}


static gboolean is_c_header(const gchar *fname)
{
	gchar *ext = NULL;

	if (fname)
	{
		ext = strrchr(fname, '.');
	}
	return (ext == NULL) ? FALSE : (*(ext + 1) == 'h');	/* match *.h* */
}


/* Call this whenever build menu items need to be enabled/disabled.
 * Uses current document (if there is one) when idx == -1 */
void build_menu_update(GeanyDocument *doc)
{
	GeanyFiletype *ft;
	gboolean have_path, can_build, can_make, can_run, can_stop, can_set_args, have_errors;
	BuildMenuItems *menu_items;

	if (doc == NULL)
		doc = document_get_current();
	if (doc == NULL ||
		(FILETYPE_ID(doc->file_type) == GEANY_FILETYPES_NONE &&	doc->file_name == NULL))
	{
		gtk_widget_set_sensitive(lookup_widget(main_widgets.window, "menu_build1"), FALSE);
		gtk_menu_item_remove_submenu(GTK_MENU_ITEM(lookup_widget(main_widgets.window, "menu_build1")));
		gtk_widget_set_sensitive(widgets.compile_button, FALSE);
		gtk_widget_set_sensitive(widgets.run_button, FALSE);
		return;
	}
	else
		gtk_widget_set_sensitive(lookup_widget(main_widgets.window, "menu_build1"), TRUE);

	ft = doc->file_type;
	g_return_if_fail(ft != NULL);

	menu_items = build_get_menu_items(ft->id);
	/* Note: don't remove the submenu first because it can now cause an X hang if
	 * the menu is already open when called from build_exit_cb(). */
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(lookup_widget(main_widgets.window, "menu_build1")),
		menu_items->menu);

	have_path = (doc->file_name != NULL);

	can_make = have_path && build_info.pid <= (GPid) 1;

	/* disable compile and link for C/C++ header files */
	if (ft->id == GEANY_FILETYPES_C || ft->id == GEANY_FILETYPES_CPP)
		can_build = can_make && ! is_c_header(doc->file_name);
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

	if (app->project && NZV(app->project->run_cmd))
		can_run = have_path;	/* for now run is disabled for all untitled files */
	else
		can_run = have_path && ft->actions->can_exec;

	can_stop = run_info.pid > (GPid) 1;
	can_run &= ! can_stop;

	/* item_exec is enabled for both run and stop commands */
	if (menu_items->item_exec)
		gtk_widget_set_sensitive(menu_items->item_exec, can_run || can_stop);
	/* item_exec2 is disabled if there's a running process already */
	if (menu_items->item_exec2)
		gtk_widget_set_sensitive(menu_items->item_exec2, can_run);

	can_set_args =
		((ft->actions->can_compile ||
		ft->actions->can_link ||
		ft->actions->can_exec) &&
		FILETYPE_ID(ft) != GEANY_FILETYPES_NONE);
	if (menu_items->item_set_args)
		gtk_widget_set_sensitive(menu_items->item_set_args, can_set_args);

	gtk_widget_set_sensitive(widgets.compile_button, can_build && ft->actions->can_compile);
	gtk_widget_set_sensitive(widgets.run_button, can_run || can_stop);

	/* show the stop command if a program is running, otherwise show run command */
	set_stop_button(can_stop);

	/* simply enable next error command if the compiler window has any items */
	have_errors = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(msgwindow.store_compiler),
		NULL) > 0;
	if (menu_items->item_next_error)
		gtk_widget_set_sensitive(menu_items->item_next_error, have_errors);
	if (menu_items->item_previous_error)
		gtk_widget_set_sensitive(menu_items->item_previous_error, have_errors);
}


/* Call build_menu_update() instead of calling this directly. */
static void set_stop_button(gboolean stop)
{
	GtkStockItem sitem;
	GtkWidget *menuitem =
		build_get_menu_items(run_info.file_type_id)->item_exec;

	if (stop && utils_str_equal(
		gtk_tool_button_get_stock_id(GTK_TOOL_BUTTON(widgets.run_button)), "gtk-stop")) return;
	if (! stop && utils_str_equal(
		gtk_tool_button_get_stock_id(GTK_TOOL_BUTTON(widgets.run_button)), "gtk-execute")) return;

	/* use the run button also as stop button */
	if (stop)
	{
		gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(widgets.run_button), "gtk-stop");

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
		gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(widgets.run_button), "gtk-execute");

		if (menuitem != NULL)
		{
			/* LaTeX hacks ;-( */
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
 * If filetype_idx is -1, the current filetype is used, or GEANY_FILETYPES_NONE */
BuildMenuItems *build_get_menu_items(gint filetype_idx)
{
	BuildMenuItems *items;

	if (filetype_idx == -1)
	{
		GeanyDocument *doc = document_get_current();
		GeanyFiletype *ft = NULL;

		if (doc != NULL)
			ft = doc->file_type;
		filetype_idx = FILETYPE_ID(ft);
	}

	if (filetype_idx == GEANY_FILETYPES_LATEX)
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


static void
on_build_compile_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GeanyDocument *doc = document_get_current();

	if (doc == NULL)
		return;

	if (doc->changed)
		document_save_file(doc, FALSE);

	if (FILETYPE_ID(doc->file_type) == GEANY_FILETYPES_LATEX)
		build_compile_tex_file(doc, 0);
	else
		build_compile_file(doc);
}


static void
on_build_tex_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GeanyDocument *doc = document_get_current();

	if (doc == NULL)
		return;

	if (doc->changed)
		document_save_file(doc, FALSE);

	switch (GPOINTER_TO_INT(user_data))
	{
		case LATEX_CMD_TO_DVI:
		case LATEX_CMD_TO_PDF:
			build_compile_tex_file(doc, GPOINTER_TO_INT(user_data)); break;
		case LATEX_CMD_VIEW_DVI:
		case LATEX_CMD_VIEW_PDF:
			build_view_tex_file(doc, GPOINTER_TO_INT(user_data)); break;
	}
}


static void
on_build_build_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GeanyDocument *doc = document_get_current();

	if (doc == NULL)
		return;

	if (doc->changed)
		document_save_file(doc, FALSE);

	if (FILETYPE_ID(doc->file_type) == GEANY_FILETYPES_LATEX)
		build_compile_tex_file(doc, 1);
	else
		build_link_file(doc);
}


static void
on_make_custom_input_response(const gchar *input)
{
	GeanyDocument *doc = document_get_current();

	if (doc->changed)
		document_save_file(doc, FALSE);

	setptr(build_info.custom_target, g_strdup(input));

	build_make_file(doc, GBO_MAKE_CUSTOM);
}


static void
show_make_custom(void)
{
	static GtkWidget *dialog = NULL;	/* keep dialog for combo history */

	if (! dialog)
		dialog = dialogs_show_input(_("Make Custom Target"),
			_("Enter custom options here, all entered text is passed to the make command."),
			build_info.custom_target, TRUE, &on_make_custom_input_response);
	else
	{
		gtk_widget_show(dialog);
	}
}


static void
on_build_make_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GeanyDocument *doc = document_get_current();
	gint build_opts = GPOINTER_TO_INT(user_data);

	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	switch (build_opts)
	{
		case GBO_MAKE_CUSTOM:
		{
			show_make_custom();
			break;
		}

		case GBO_MAKE_OBJECT:
		/* fall through */
		case GBO_MAKE_ALL:
		{
			if (doc->changed)
				document_save_file(doc, FALSE);

			build_make_file(doc, build_opts);
		}
	}
}


static gboolean use_html_builtin(GeanyDocument *doc, GeanyFiletype *ft)
{
	gboolean use_builtin = FALSE;
	if (ft->id == GEANY_FILETYPES_HTML)
	{
		/* we have a project, check its run_cmd */
		if (app->project != NULL)
		{
			if (utils_str_equal(app->project->run_cmd, "builtin"))
				use_builtin = TRUE;
		}
		/* no project, check for filetype run_cmd */
		else if (ft->actions->can_exec && utils_str_equal(ft->programs->run_cmd, "builtin"))
			use_builtin = TRUE;
	}

	if (use_builtin)
	{
		gchar *uri = g_strconcat("file:///", g_path_skip_root(doc->file_name), NULL);
		utils_start_browser(uri);
		g_free(uri);

		return TRUE;
	}
	return FALSE;
}


static void
on_build_execute_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GeanyDocument *doc = document_get_current();
	filetype_id ft_id;
	GeanyFiletype *ft;

	if (doc == NULL)
		return;

	/* make the process "stopable" */
	if (run_info.pid > (GPid) 1)
	{
		kill_process(&run_info.pid);
		return;
	}

	ft_id = FILETYPE_ID(doc->file_type);
	ft = filetypes[ft_id];
	if (ft_id == GEANY_FILETYPES_LATEX)
	{	/* run LaTeX file */
		if (build_view_tex_file(doc, GPOINTER_TO_INT(user_data)) == (GPid) 0)
		{
			ui_set_statusbar(TRUE, _("Failed to execute the view program"));
		}
	}
	/* use_html_builtin() checks for HTML builtin request and returns FALSE if not */
	else if (! use_html_builtin(doc, ft))
	{	/* run everything else */

		/* save the file only if the run command uses it */
		if (doc->changed &&
			NZV(ft->programs->run_cmd) &&	/* can happen when project is open */
			strstr(ft->programs->run_cmd, "%f") != NULL)
				document_save_file(doc, FALSE);

		build_run_cmd(doc);
	}
}


static void kill_process(GPid *pid)
{
	/* Unix: SIGQUIT is not the best signal to use because it causes a core dump (this should not
	 * perforce necessary for just killing a process). But we must use a signal which we can
	 * ignore because the main process get it too, it is declared to ignore in main.c. */

	gint result;

#ifdef G_OS_WIN32
	g_return_if_fail(*pid != NULL);
	result = TerminateProcess(*pid, 0);
	/* TerminateProcess() returns TRUE on success, for the check below we have to convert
	 * it to FALSE (and vice versa) */
	result = ! result;
#else
	g_return_if_fail(*pid > 1);
	result = kill(*pid, SIGQUIT);
#endif

	if (result != 0)
		ui_set_statusbar(TRUE, _("Process could not be stopped (%s)."), g_strerror(errno));
	else
	{
		*pid = 0;
		build_menu_update(NULL);
	}
}


static void
on_build_next_error                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (ui_tree_view_find_next(GTK_TREE_VIEW(msgwindow.tree_compiler),
		msgwin_goto_compiler_file_line))
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
	}
	else
		ui_set_statusbar(FALSE, _("No more build errors."));
}


static void
on_build_previous_error                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (ui_tree_view_find_previous(GTK_TREE_VIEW(msgwindow.tree_compiler),
		msgwin_goto_compiler_file_line))
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
	}
	else
		ui_set_statusbar(FALSE, _("No more build errors."));
}


void build_init()
{
	widgets.compile_button = lookup_widget(main_widgets.window, "toolbutton_compile");
	widgets.run_button = lookup_widget(main_widgets.window, "toolbutton_run");
}
