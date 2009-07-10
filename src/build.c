/*
 *      build.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
#include "toolbar.h"
#include "geanymenubuttonaction.h"


GeanyBuildInfo build_info = {GBG_FT, 0, 0, NULL, GEANY_FILETYPES_NONE, NULL, 0};

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


/* pack group (<8) and command (<32) into a user_data pointer */
#define GRP_CMD_TO_POINTER(grp, cmd) GINT_TO_POINTER((((grp)&7)<<5)|((cmd)&0x1f))
#define GBO_TO_POINTER(gbo) (GRP_CMD_TO_POINTER(GBO_TO_GBG(gbo), GBO_TO_CMD(gbo)))
#define GPOINTER_TO_CMD(gptr) (GPOINTER_TO_INT(gptr)&0x1f)
#define GPOINTER_TO_GRP(gptr) ((GPOINTER_TO_INT(gptr)&0xe0)>>5)

static gpointer last_toolbutton_action = GBO_TO_POINTER(GBO_BUILD);

static BuildMenuItems menu_items = {NULL}; /* only matters that menu is NULL */

static struct
{
	GtkWidget	*run_button;
	GtkWidget	*compile_button;
	GtkWidget	*build_button;
	GtkAction	*build_action;

	GtkWidget	*toolitem_build;
	GtkWidget	*toolitem_make_all;
	GtkWidget	*toolitem_make_custom;
	GtkWidget	*toolitem_make_object;
	GtkWidget	*toolitem_set_args;
}
widgets;

static gint build_groups_count[GBG_COUNT] = { 3, 4, 1 };
static gint build_items_count = 8;

#ifndef G_OS_WIN32
static void build_exit_cb(GPid child_pid, gint status, gpointer user_data);
static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data);
#endif
static gboolean build_create_shellscript(const gchar *fname, const gchar *cmd, gboolean autoclose);
static GPid build_spawn_cmd(GeanyDocument *doc, const gchar *cmd, const gchar *dir);
static void set_stop_button(gboolean stop);
static void run_exit_cb(GPid child_pid, gint status, gpointer user_data);
static void on_set_build_commands_activate(GtkWidget *w, gpointer u);
static void on_build_next_error(GtkWidget *menuitem, gpointer user_data);
static void on_build_previous_error(GtkWidget *menuitem, gpointer user_data);
static void kill_process(GPid *pid);
static void show_build_result_message(gboolean failure);
static void process_build_output_line(const gchar *line, gint color);
static void show_build_commands_dialog(void);


void build_finalize()
{
	g_free(build_info.dir);
	g_free(build_info.custom_target);

	if (menu_items.menu != NULL && GTK_IS_WIDGET(menu_items.menu))
		gtk_widget_destroy(menu_items.menu);
/*	if (latex_menu_items.menu != NULL && GTK_IS_WIDGET(latex_menu_items.menu))
		gtk_widget_destroy(latex_menu_items.menu); */
}

/* note: copied from keybindings.c, may be able to go away */
static void add_menu_accel(GeanyKeyGroup *group, guint kb_id,
	GtkAccelGroup *accel_group, GtkWidget *menuitem)
{
	GeanyKeyBinding *kb = &group->keys[kb_id];

	if (kb->key != 0)
		gtk_widget_add_accelerator(menuitem, "activate", accel_group,
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
}

/*-----------------------------------------------------
 * 
 * Execute commands and handle results
 * 
 *-----------------------------------------------------*/

/* the various groups of commands not in the filetype struct */
GeanyBuildCommand *ft_def=NULL, *non_ft_proj=NULL, *non_ft_pref=NULL, *non_ft_def=NULL, *exec_proj=NULL, *exec_pref=NULL, *exec_def=NULL;

#define return_cmd_if(src, cmds) if (cmds!=NULL && cmds[cmdindex].exists && below>src)\
									{*fr=src; return &(cmds[cmdindex]);}
#define return_ft_cmd_if(src, cmds) if (ft!=NULL && ft->cmds!=NULL && ft->cmds[cmdindex].exists && below>src)\
										{*fr=src; return &(ft->cmds[cmdindex]);}

/* get the next lowest command taking priority into account */
static GeanyBuildCommand *get_next_build_cmd(GeanyDocument *doc, gint cmdgrp, gint cmdindex, gint below, gint *from)
{
	GeanyBuildSource	 srcindex;
	GeanyFiletype		*ft=NULL;
	gint				 sink, *fr = &sink;
	
	if (cmdgrp>=GBG_COUNT)return NULL;
	if (from!=NULL)fr=from;
	if (doc==NULL)doc=document_get_current();
	if (doc!=NULL)ft = doc->file_type;
	switch(cmdgrp)
	{
		case GBG_FT: /* order proj ft, home ft, ft, defft */
			if (ft!=NULL)
			{
				return_ft_cmd_if(BCS_PROJ_FT, projfilecmds);
				return_ft_cmd_if(BCS_PREF, homefilecmds);
				return_ft_cmd_if(BCS_FT, filecmds);
			}
			return_cmd_if(BCS_DEF, ft_def);
			break;
		case GBG_NON_FT: /* order proj, pref, def */
			return_cmd_if(BCS_PROJ, non_ft_proj);
			return_cmd_if(BCS_PREF, non_ft_pref);
			return_ft_cmd_if(BCS_FT, ftdefcmds);
			return_cmd_if(BCS_DEF, non_ft_def);
			break;
		case GBG_EXEC: /* order proj, proj ft, pref, home ft, ft, def */
			return_cmd_if(BCS_PROJ, exec_proj);
			return_ft_cmd_if(BCS_PROJ_FT, projexeccmds);
			return_cmd_if(BCS_PREF, exec_pref);
			return_ft_cmd_if(BCS_FT, homeexeccmds);
			return_ft_cmd_if(BCS_FT, execcmds);
			return_cmd_if(BCS_DEF, exec_def);
			break;
		default:
			break;
	}
	return NULL;
}

/* shortcut to start looking at the top */
static GeanyBuildCommand *get_build_cmd(GeanyDocument *doc, gint grp, gint index, gint *from)
{
	return get_next_build_cmd(doc, grp, index, BCS_COUNT, from);
}

/* remove the specified command, cmd<0 remove whole group */
void remove_command(GeanyBuildSource src, GeanyBuildGroup grp, gint cmd)
{
	GeanyBuildCommand *bc;
	gint i;
	GeanyDocument *doc;
	GeanyFiletype *ft;
	
	switch(grp)
	{
		case GBG_FT:
			if ((doc=document_get_current())==NULL)return;
			if ((ft=doc->file_type)==NULL)return;
			switch(src)
			{
				case BCS_DEF:     bc=ft->ftdefcmds; break;
				case BCS_FT:      bc=ft->filecmds; break;
				case BCS_HOME_FT: bc=ft->homefilecmds; break;
				case BCS_PREF:    bc=ft->homefilecmds; break;
				case BCS_PROJ:    bc=ft->projfilecmds; break;
				case BCS_PROJ_FT: bc=ft->projfilecmds; break;
				default: return;
			}
			break;
		case GBG_NON_FT:
			switch(src)
			{
				case BCS_DEF:     bc=non_ft_def; break;
				case BCS_PREF:    bc=non_ft_pref; break;
				case BCS_PROJ:    bc=non_ft_proj; break;
				default: return;
			}
			break;
		case GBG_EXEC:
			if ((doc=document_get_current())==NULL)return;
			if ((ft=doc->file_type)==NULL)return;
			switch(src)
			{
				case BCS_DEF:     bc=exec_def; break;
				case BCS_FT:      bc=ft->execcmds; break;
				case BCS_HOME_FT: bc=ft->homeexeccmds; break;
				case BCS_PREF:    bc=exec_pref; break;
				case BCS_PROJ:    bc=exec_proj; break;
				case BCS_PROJ_FT: bc=ft->projexeccmds; break;
				default: return;
				
			}
			break;
		default:
			return;
	}
	if (bc==NULL)return;
	if (cmd<0)
		for (i=0; i<build_groups_count[grp]; ++i)
			bc[i].exists=FALSE;
	else
		bc[cmd].exists=FALSE;
}

/* Clear all error indicators in all documents. */
static void clear_errors(GeanyDocument *doc)
{
	guint i;

	for (i = 0; i < documents_array->len; i++)
	{
		if (documents[i]->is_valid)
			editor_indicator_clear_errors(documents[i]->editor);
	}
}


#ifdef G_OS_WIN32
static void parse_build_output(const gchar **output, gint status)
{
	guint x, i, len;
	gchar *line, **lines;

	for (x = 0; x < 2; x++)
	{
		if (NZV(output[x]))
		{
			lines = g_strsplit_set(output[x], "\r\n", -1);
			len = g_strv_length(lines);

			for (i = 0; i < len; i++)
			{
				if (NZV(lines[i]))
				{
					line = lines[i];
					while (*line != '\0')
					{	/* replace any conrol characters in the output */
						if (*line < 32)
							*line = 32;
						line++;
					}
					process_build_output_line(lines[i], COLOR_BLACK);
				}
			}
			g_strfreev(lines);
		}
	}

	show_build_result_message(status != 0);
	utils_beep();

	build_info.pid = 0;
	/* enable build items again */
	build_menu_update(NULL);
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
#ifdef G_OS_WIN32
	gchar	*output[2];
	gint	 status;
#else
	gint     stdout_fd;
	gint     stderr_fd;
#endif

	g_return_val_if_fail(doc!=NULL || dir!=NULL, (GPid) 1);

	if (doc!=NULL)clear_errors(doc);
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
	msgwin_compiler_add(COLOR_BLUE, _("%s (in directory: %s)"), utf8_cmd_string, utf8_working_dir);
	g_free(utf8_working_dir);
	g_free(utf8_cmd_string);

	/* set the build info for the message window */
	g_free(build_info.dir);
	build_info.dir = g_strdup(working_dir);
	build_info.file_type_id = (doc==NULL)?GEANY_FILETYPES_NONE : FILETYPE_ID(doc->file_type);
	build_info.message_count = 0;

#ifdef G_OS_WIN32
	if (! utils_spawn_sync(working_dir, argv, NULL, G_SPAWN_SEARCH_PATH,
			NULL, NULL, &output[0], &output[1], &status, &error))
#else
	if (! g_spawn_async_with_pipes(working_dir, argv, NULL,
			G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
			&(build_info.pid), NULL, &stdout_fd, &stderr_fd, &error))
#endif
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

#ifdef G_OS_WIN32
	parse_build_output((const gchar**) output, status);
	g_free(output[0]);
	g_free(output[1]);
#else
	if (build_info.pid > 0)
	{
		g_child_watch_add(build_info.pid, (GChildWatchFunc) build_exit_cb, NULL);
		build_menu_update(doc);
		ui_progress_bar_start(NULL);
	}

	/* use GIOChannels to monitor stdout and stderr */
	utils_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
		TRUE, build_iofunc, GINT_TO_POINTER(0));
	utils_set_up_io_channel(stderr_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
		TRUE, build_iofunc, GINT_TO_POINTER(1));
#endif

	g_strfreev(argv);
	g_free(working_dir);
	g_free(locale_filename);

	return build_info.pid;
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
	cmd = get_build_cmd(doc, GBG_EXEC, 0, NULL)->command;
	/* TODO fix all this stuff */

	if (strstr(cmd, "%e") != NULL)
	{
		executable = utils_remove_ext_from_filename(locale_filename);
		setptr(executable, g_path_get_basename(executable));
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
		utils_free_pointers(4, utf8_working_dir, working_dir, executable, locale_filename, NULL);
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

			utils_free_pointers(2, executable, locale_filename, NULL);
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

	utils_free_pointers(4, tmp, cmd, executable, locale_filename, NULL);

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
		{
			ui_set_statusbar(FALSE,
		_("Could not execute the file in the VTE because it probably contains a command."));
			geany_debug("Could not execute the file in the VTE because it probably contains a command.");
		}

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


static void process_build_output_line(const gchar *str, gint color)
{
	gchar *msg, *tmp;

	msg = g_strdup(str);

	g_strchomp(msg);

	if (! NZV(msg))
		return;

	if (editor_prefs.use_indicators && build_info.message_count < GEANY_BUILD_ERR_HIGHLIGHT_MAX)
	{
		gchar *filename;
		gint line;

		build_info.message_count++;

		if (build_parse_make_dir(msg, &tmp))
		{
			setptr(current_dir_entered, tmp);
		}
		msgwin_parse_compiler_error_line(msg, current_dir_entered, &filename, &line);
		if (line != -1 && filename != NULL)
		{
			GeanyDocument *doc = document_find_by_filename(filename);

			if (doc)
			{
				if (line > 0) /* some compilers, like pdflatex report errors on line 0 */
					line--;   /* so only adjust the line number if it is greater than 0 */
				editor_indicator_set_on_line(doc->editor, GEANY_INDICATOR_ERROR, line);
			}
			color = COLOR_RED;	/* error message parsed on the line */
		}
		g_free(filename);
	}
	msgwin_compiler_add_string(color, msg);

	g_free(msg);
}


#ifndef G_OS_WIN32
static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg;

		while (g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL) && msg)
		{
			gint color = (GPOINTER_TO_INT(data)) ? COLOR_DARK_RED : COLOR_BLACK;

			process_build_output_line(msg, color);
 			g_free(msg);
		}
	}
	if (cond & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		return FALSE;

	return TRUE;
}
#endif


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

	if (strstr(string, "Leaving directory") != NULL)
	{
		*prefix = NULL;
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
		msgwin_compiler_add_string(COLOR_DARK_RED, msg);
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
		msgwin_compiler_add_string(COLOR_BLUE, msg);
		if (! ui_prefs.msgwindow_visible ||
			gtk_notebook_get_current_page(GTK_NOTEBOOK(msgwindow.notebook)) != MSG_COMPILER)
				ui_set_statusbar(FALSE, "%s", msg);
	}
}


#ifndef G_OS_WIN32
static void build_exit_cb(GPid child_pid, gint status, gpointer user_data)
{
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

	utils_beep();
	g_spawn_close_pid(child_pid);

	build_info.pid = 0;
	/* enable build items again */
	build_menu_update(NULL);
	ui_progress_bar_stop();
}
#endif


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
	if (! fp)
		return FALSE;
#ifdef G_OS_WIN32
	str = g_strdup_printf("%s\n\n%s\ndel \"%%0\"\n\npause\n", cmd, (autoclose) ? "" : "pause");
#else
	str = g_strdup_printf(
		"#!/bin/sh\n\nrm $0\n\n%s\n\necho \"\n\n------------------\n(program exited with code: $?)\" \
		\n\n%s\n", cmd, (autoclose) ? "" :
		"\necho \"Press return to continue\"\n#to be more compatible with shells like dash\ndummy_var=\"\"\nread dummy_var");
#endif

	fputs(str, fp);
	g_free(str);

	fclose(fp);

	return TRUE;
}

typedef void callback(GtkWidget *w, gpointer u);

/* run the command catenating cmd_cat if present */
static void build_command(GeanyDocument *doc, GeanyBuildGroup grp, gint cmd, gchar *cmd_cat)
{
	gchar *dir;
	gchar *full_command;
	GeanyBuildCommand *buildcmd = get_build_cmd(doc, grp, cmd, NULL);
	
	if (buildcmd==NULL)return;
	if (cmd_cat != NULL)
	{
		if (buildcmd->command != NULL)
			full_command = g_strconcat(buildcmd->command, cmd_cat, NULL);
		else
			full_command = g_strdup(cmd_cat);
	}
	else
		full_command = buildcmd->command;
	if (grp == GBG_FT)
	{
		dir=NULL; /* allways run in doc dir */
	}
	else
	{
		dir = NULL;
		if (buildcmd->run_in_base_dir)
			dir =  project_get_make_dir();
	}
	build_info.grp = grp;
	build_info.cmd = cmd;
	build_spawn_cmd(doc, full_command, dir);
	g_free(dir);
	if (cmd_cat != NULL) g_free(full_command);
	build_menu_update(doc);

}


/*----------------------------------------------------------------
 * 
 * Create build menu and handle callbacks (&toolbar callbacks)
 * 
 *----------------------------------------------------------------*/

static void on_make_custom_input_response(const gchar *input)
{
	GeanyDocument *doc = document_get_current();
	setptr(build_info.custom_target, g_strdup(input));
	build_command(doc, GBO_TO_GBG(GBO_MAKE_CUSTOM), GBO_TO_CMD(GBO_MAKE_CUSTOM), build_info.custom_target);
}

static void on_build_menu_item(GtkWidget *w, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	filetype_id ft_id;
	GeanyFiletype *ft;
	gint grp=GPOINTER_TO_GRP(user_data);
	gint cmd=GPOINTER_TO_CMD(user_data);
	
	if (doc && doc->changed)
		document_save_file(doc, FALSE);
	if (grp == GBG_NON_FT && cmd == GBO_TO_CMD(GBO_MAKE_CUSTOM))
	{
		static GtkWidget *dialog = NULL;	/* keep dialog for combo history */

		if (! dialog)
			dialog = dialogs_show_input(_("Custom Text"),
				_("Enter custom text here, all entered text is appended to the command."),
				build_info.custom_target, TRUE, &on_make_custom_input_response);
		else
		{
			gtk_widget_show(dialog);
		}
		return;
	}
	else if (grp == GBG_EXEC && cmd == GBO_TO_CMD(GBO_EXEC))
	{
		if (run_info.pid > (GPid) 1)
		{
			kill_process(&run_info.pid);
			return;
		}
		GeanyBuildCommand *bc = get_build_cmd(doc, grp, cmd, NULL);
		if (bc!=NULL && strcmp(bc->command, "builtin")==0)
		{
			if (doc==NULL)return;
			gchar *uri = g_strconcat("file:///", g_path_skip_root(doc->file_name), NULL);
			utils_open_browser(uri);
			g_free(uri);

		}
		else
			build_run_cmd(doc);
	}
	else
		build_command(doc, grp, cmd, NULL);
};

/* group codes for menu items other than the known commands
 * value order is important, see the following table for use */

/* the rest in each group */
#define MENU_FT_REST     (GBG_COUNT+GBG_FT)
#define MENU_NON_FT_REST (GBG_COUNT+GBG_NON_FT)
#define MENU_EXEC_REST   (GBG_COUNT+GBG_EXEC)
/* the separator */
#define MENU_SEPARATOR   (2*GBG_COUNT)
/* the fixed items */
#define MENU_NEXT_ERROR  (MENU_SEPARATOR+1)
#define MENU_PREV_ERROR  (MENU_NEXT_ERROR+1)
#define MENU_COMMANDS    (MENU_PREV_ERROR+1)
#define MENU_DONE        (MENU_COMMANDS+1)


static struct build_menu_item_spec {
	const gchar	*stock_id;
	const gint	 key_binding;
	const gint	 build_grp, build_cmd;
	const gchar	*fix_label;
	callback *cb;
} build_menu_specs[] = { 
		{ GTK_STOCK_CONVERT, GEANY_KEYS_BUILD_COMPILE, GBO_TO_GBG(GBO_COMPILE), GBO_TO_CMD(GBO_COMPILE),  NULL, on_build_menu_item },
		{ GEANY_STOCK_BUILD, GEANY_KEYS_BUILD_LINK,    GBO_TO_GBG(GBO_BUILD),   GBO_TO_CMD(GBO_BUILD),    NULL, on_build_menu_item },
		{ NULL,              -1,                       MENU_FT_REST,            GBO_TO_CMD(GBO_BUILD)+1,  NULL, on_build_menu_item },
		{ NULL,              -1,                       MENU_SEPARATOR,          GBF_SEP_1,                NULL, NULL },
		{ NULL,              GEANY_KEYS_BUILD_MAKE,    GBO_TO_GBG(GBO_MAKE_ALL),  GBO_TO_CMD(GBO_MAKE_ALL), NULL, on_build_menu_item },
		{ NULL, GEANY_KEYS_BUILD_MAKEOWNTARGET,        GBO_TO_GBG(GBO_MAKE_CUSTOM),GBO_TO_CMD(GBO_MAKE_CUSTOM), NULL, on_build_menu_item },
		{ NULL, GEANY_KEYS_BUILD_MAKEOBJECT,           GBO_TO_GBG(GBO_MAKE_OBJECT),GBO_TO_CMD(GBO_MAKE_OBJECT), NULL, on_build_menu_item },
		{ NULL,              -1,                       MENU_NON_FT_REST, GBO_TO_CMD(GBO_MAKE_OBJECT)+1,  NULL, on_build_menu_item },
		{ NULL,              -1,                       MENU_SEPARATOR,          GBF_SEP_2,                 NULL, NULL },
		{ NULL, GEANY_KEYS_BUILD_NEXTERROR,            MENU_NEXT_ERROR,         GBF_NEXT_ERROR,            N_("_Next Error"), on_build_next_error },
		{ NULL, GEANY_KEYS_BUILD_PREVIOUSERROR,        MENU_PREV_ERROR,         GBF_PREV_ERROR,            N_("_Previous Error"), on_build_previous_error },
		{ NULL,              -1,                       MENU_SEPARATOR,          GBF_SEP_3,                 NULL, NULL },
		{ GTK_STOCK_EXECUTE, GEANY_KEYS_BUILD_RUN,     GBO_TO_GBG(GBO_EXEC),    GBO_TO_CMD(GBO_EXEC),      NULL, on_build_menu_item },
		{ NULL,              -1,                       MENU_EXEC_REST,          GBO_TO_CMD(GBO_EXEC)+1,    NULL, NULL },
		{ NULL,              -1,                       MENU_SEPARATOR,          GBF_SEP_4,                 NULL, NULL },
		{ GTK_STOCK_PREFERENCES, GEANY_KEYS_BUILD_OPTIONS, MENU_COMMANDS,       GBF_COMMANDS,              N_("_Set Build Commands"), on_set_build_commands_activate },
		{ NULL,              -1,                       MENU_DONE,               0,                         NULL, NULL }
};

static void create_build_menu_item(GtkWidget *menu, GeanyKeyGroup *group, GtkAccelGroup *ag, 
							struct build_menu_item_spec *bs, gchar *lbl, gint grp, gint cmd)
{
	GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(lbl);
	if (bs->stock_id!=NULL)
	{
		GtkWidget *image = gtk_image_new_from_stock(bs->stock_id, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
		
	}
	gtk_widget_show(item);
	if (bs->key_binding>0)
		add_menu_accel(group, bs->key_binding, ag, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	if (bs->cb!=NULL)
	{
		g_signal_connect(item, "activate", G_CALLBACK(bs->cb), GRP_CMD_TO_POINTER(grp,cmd));
	}
	menu_items.menu_item[grp][cmd] = item;
}

static void create_build_menu(BuildMenuItems *menu_items)
{
	GtkWidget *menu;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GeanyKeyGroup *keygroup = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_BUILD);
	gint i,j;

	menu = gtk_menu_new();
	menu_items->menu_item[GBG_FT] = g_new0(GtkWidget*, build_groups_count[GBG_FT]);
	menu_items->menu_item[GBG_NON_FT] = g_new0(GtkWidget*, build_groups_count[GBG_NON_FT]);
	menu_items->menu_item[GBG_EXEC] = g_new0(GtkWidget*, build_groups_count[GBG_EXEC]);
	menu_items->menu_item[GBG_FIXED] = g_new0(GtkWidget*, GBF_COUNT);
	
	for (i=0; build_menu_specs[i].build_grp != MENU_DONE; ++i)
	{
		struct build_menu_item_spec *bs = &(build_menu_specs[i]);
		if (bs->build_grp == MENU_SEPARATOR)
		{
			GtkWidget *item = gtk_separator_menu_item_new();
			gtk_widget_show(item);
			gtk_container_add(GTK_CONTAINER(menu), item);
			menu_items->menu_item[GBG_FIXED][bs->build_cmd] = item;
		}
		else if (bs->fix_label!=NULL)
		{
			create_build_menu_item(menu, keygroup, accel_group, bs, gettext(bs->fix_label), GBG_FIXED, bs->build_cmd);
		}
		else if (bs->build_grp >= MENU_FT_REST && bs->build_grp <= MENU_SEPARATOR)
		{
			gint grp = bs->build_grp-GBG_COUNT;
			for (j=bs->build_cmd; j<build_groups_count[grp]; ++j)
			{
				GeanyBuildCommand *bc = get_build_cmd(NULL, grp, j, NULL);
				gchar *lbl = (bc==NULL)?"":bc->label;
				create_build_menu_item(menu, keygroup, accel_group, bs, lbl, grp, j);
			}
		}
		else
		{
			GeanyBuildCommand *bc = get_build_cmd(NULL, bs->build_grp, bs->build_cmd, NULL);
			gchar *lbl = (bc==NULL)?"":bc->label;
			create_build_menu_item(menu, keygroup, accel_group, bs, lbl, bs->build_grp, bs->build_cmd);
		}
	}
	menu_items->menu = menu;
	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_build1")), menu);
}

/* portability to various GTK versions needs checking
 * conforms to description of gtk_accel_label as child of menu item */
static void geany_menu_item_set_label(GtkWidget *w, gchar *label)
{
	GtkWidget *c=gtk_bin_get_child(GTK_BIN(w));
	gtk_label_set_text_with_mnemonic(GTK_LABEL(c), label);
}

/* Call this whenever build menu items need to be enabled/disabled.
 * Uses current document (if there is one) when idx == -1 */
void build_menu_update(GeanyDocument *doc)
{
	gint i, cmdcount, cmd, grp;
	gboolean vis=FALSE;
	gboolean got_cmd[ MENU_DONE ];
	gboolean have_path, build_running, exec_running, have_errors, cmd_sensitivity;
	GeanyBuildCommand *bc;
	
	if (menu_items.menu==NULL)
		create_build_menu(&menu_items);
	if (doc == NULL)
		doc = document_get_current();
	have_path = doc!=NULL && doc->file_name != NULL;
	build_running =  build_info.pid > (GPid) 1;
	exec_running = run_info.pid > (GPid) 1;
	have_errors = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(msgwindow.store_compiler), NULL) > 0;
	for (i=0; build_menu_specs[i].build_grp != MENU_DONE; ++i)
	{
		struct build_menu_item_spec *bs = &(build_menu_specs[i]);
		switch(bs->build_grp)
		{
			case MENU_SEPARATOR:
				if (vis==TRUE)
				{
					gtk_widget_show_all(menu_items.menu_item[GBG_FIXED][bs->build_cmd]);
					vis=FALSE;
				}
				else
					gtk_widget_hide_all(menu_items.menu_item[GBG_FIXED][bs->build_cmd]);
				break;
			case MENU_NEXT_ERROR:
			case MENU_PREV_ERROR:
				gtk_widget_set_sensitive(menu_items.menu_item[GBG_FIXED][bs->build_cmd], have_errors);
				vis |= TRUE;
				break;
			case MENU_COMMANDS:
				vis |= TRUE;
				break;
			default: /* all configurable commands */
				if (bs->build_grp >=GBG_COUNT)
				{
					grp = bs->build_grp-GBG_COUNT;
					cmdcount = build_groups_count[grp];
				}
				else
				{
					grp = bs->build_grp;
					cmdcount = bs->build_cmd+1;
				}
				for (cmd=bs->build_cmd; cmd<cmdcount; ++cmd)
				{
					GtkWidget *menu_item = menu_items.menu_item[grp][cmd];
					bc = get_build_cmd(doc, grp, cmd, NULL);
					if (grp < GBG_EXEC)
					{
						cmd_sensitivity = 
							(grp == GBG_FT && bc!=NULL && have_path && ! build_running) ||
							(grp == GBG_NON_FT && bc!=NULL && ! build_running);
						gtk_widget_set_sensitive(menu_item, cmd_sensitivity);
						if (bc!=NULL && bc->label!=NULL && strlen(bc->label)>0)
						{
							geany_menu_item_set_label(menu_item, bc->label);
							gtk_widget_show_all(menu_item);
							vis |= TRUE;
						}
						else
							gtk_widget_hide_all(menu_item);
					}
					else
					{
						GtkWidget *image;
						cmd_sensitivity = bc!=NULL || exec_running;
						gtk_widget_set_sensitive(menu_item, cmd_sensitivity);
						if (!exec_running)
						{
							image = gtk_image_new_from_stock(bs->stock_id, GTK_ICON_SIZE_MENU);
						}
						else
						{
							image = gtk_image_new_from_stock(GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
						}
						gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
						if (bc!=NULL && bc->label!=NULL && strlen(bc->label)>0)
						{
							geany_menu_item_set_label(menu_item, bc->label);
							gtk_widget_show_all(menu_item);
							vis |= TRUE;
						}
						else
							gtk_widget_hide_all(menu_item);
					}
				}
		}
	}
	ui_widget_set_sensitive(widgets.compile_button, get_build_cmd(doc, GBG_FT, GBO_TO_CMD(GBO_COMPILE), NULL)!=NULL && have_path && ! build_running);
	ui_widget_set_sensitive(widgets.build_button, get_build_cmd(doc, GBG_FT, GBO_TO_CMD(GBO_BUILD), NULL)!=NULL && have_path && ! build_running);
	if (widgets.run_button!=NULL)
	{
		if (exec_running)
			gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(widgets.run_button), GTK_STOCK_STOP);
		else
			gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(widgets.run_button), GTK_STOCK_EXECUTE);
	}
	ui_widget_set_sensitive(widgets.run_button, get_build_cmd(doc, GBG_EXEC, GBO_TO_CMD(GBO_EXEC), NULL)!=NULL || exec_running);
}

static void on_set_build_commands_activate(GtkWidget *w, gpointer u)
{
	show_build_commands_dialog();
}

static void
on_toolbutton_build_activate(GtkWidget *menuitem, gpointer user_data)
{
	last_toolbutton_action = user_data;
	g_object_set(widgets.build_action, "tooltip", _("Build the current file"), NULL);
	on_build_menu_item(menuitem, user_data);
}


static void
on_toolbutton_make_activate  (GtkWidget *menuitem, gpointer user_data)
{
	gchar *msg;
	gint grp,cmd;

	last_toolbutton_action = user_data;
	grp = GPOINTER_TO_GRP(user_data); cmd = GPOINTER_TO_CMD(user_data);
	if ( last_toolbutton_action==GBO_TO_POINTER(GBO_MAKE_ALL))
			msg = _("Build the current file with Make and the default target");
	else if (last_toolbutton_action==GBO_TO_POINTER(GBO_MAKE_CUSTOM))
			msg = _("Build the current file with Make and the specified target");
	else if (last_toolbutton_action==GBO_TO_POINTER(GBO_MAKE_OBJECT))
			msg = _("Compile the current file with Make");
	else
			msg = NULL;
	g_object_set(widgets.build_action, "tooltip", msg, NULL);
	on_build_menu_item(menuitem, user_data);
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
on_build_next_error                    (GtkWidget     *menuitem,
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
on_build_previous_error                (GtkWidget     *menuitem,
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

void build_toolbutton_build_clicked(GtkAction *action, gpointer user_data)
{
	if (last_toolbutton_action == GBO_TO_POINTER(GBO_BUILD))
	{
		on_build_menu_item(NULL, user_data);
	}
	else
	{
		on_build_menu_item(NULL, last_toolbutton_action);
	}
}

/*------------------------------------------------------
 * 
 * Create and handle the build menu configuration dialog
 * 
 *-------------------------------------------------------*/

typedef struct RowWidgets {
	GtkWidget *label, *command, *dir;
	GeanyBuildSource src, dst;
	GeanyBuildCommand *cmdsrc;
	gint grp,cmd;
	gboolean cleared;
}RowWidgets;

static void on_clear_dialog_row( GtkWidget *unused, gpointer user_data )
{
	RowWidgets *r = (RowWidgets*)user_data;
	gint src;
	GeanyBuildCommand *bc = get_next_build_cmd(NULL, r->grp, r->cmd, r->dst, &src);
	if(bc!=NULL)
	{
		printf("clear here %d, %d, %d\n", r->dst, r->src, src);
		r->cmdsrc = bc;
		r->src = src;
		gtk_entry_set_text(GTK_ENTRY(r->label), bc->label!=NULL?bc->label:"");
		gtk_entry_set_text(GTK_ENTRY(r->command), bc->command!=NULL?bc->command:"");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r->dir), bc->run_in_base_dir);
	}
	else
	{
		printf("clear there\n");
		gtk_entry_set_text(GTK_ENTRY(r->label), "");
		gtk_entry_set_text(GTK_ENTRY(r->command), "");
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r->dir), FALSE);
	}
	r->cleared = TRUE;
}

static RowWidgets *build_add_dialog_row(GeanyDocument *doc, GtkTable *table, gint row,
				GeanyBuildSource dst, gint grp, gint cmd, gboolean dir)
{
	GtkWidget *label, *check, *clear, *clearicon;
	RowWidgets *roww;
	gchar 	*labeltxt, *cmdtxt;
	GeanyBuildCommand *bc;
	gint src;
	gboolean ribd; /* run in base directory */
	
	if (grp == GBO_TO_GBG(GBO_MAKE_CUSTOM) && cmd == GBO_TO_CMD(GBO_MAKE_CUSTOM)){ labeltxt = g_strdup_printf("%d:*", cmd+1); }
	else labeltxt = g_strdup_printf("%d:", cmd+1);
	label = gtk_label_new(labeltxt);
	g_free(labeltxt);
	gtk_table_attach(table, label, 0, 1, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	roww = g_new0(RowWidgets, 1);
	roww->src = BCS_COUNT;
	roww->grp = grp;
	roww->cmd = cmd;
	roww->dst = dst;
	roww->label = gtk_entry_new();
	gtk_table_attach(table, roww->label, 1, 2, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	roww->command = gtk_entry_new();
	gtk_table_attach(table, roww->command, 2, 3, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	if (dir)
	{
		check = gtk_check_button_new();
		gtk_table_attach(table, check, 3, 4, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	}
	else check = NULL;
	roww->dir = check;
	clearicon = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_SMALL_TOOLBAR);
	clear = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(clear), clearicon);
	g_signal_connect((gpointer)clear, "clicked", G_CALLBACK(on_clear_dialog_row), (gpointer)roww);
	gtk_table_attach(table, clear, 4, 5, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	roww->cmdsrc = bc = get_build_cmd(doc, grp, cmd, &src);
	if (bc!=NULL)
	{
		if ((labeltxt = bc->label)==NULL)labeltxt="";
		if ((cmdtxt = bc->command)==NULL)cmdtxt="";
		ribd = bc->run_in_base_dir;
		roww->src = src;
	}
	else
	{
		labeltxt = cmdtxt = "";
		ribd = FALSE;
	}
	gtk_entry_set_text(GTK_ENTRY(roww->label), labeltxt);
	gtk_entry_set_text(GTK_ENTRY(roww->command), cmdtxt);
	if (dir)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), ribd);
	if (src>dst || (grp==GBG_FT && (doc==NULL || doc->file_type==NULL)))
	{
		gtk_widget_set_sensitive(roww->label, FALSE);
		gtk_widget_set_sensitive(roww->command, FALSE);
		gtk_widget_set_sensitive(check, FALSE);
		gtk_widget_set_sensitive(clear, FALSE);
	}
	return roww;
}

static gchar *colheads[] = { N_("Item"), N_("Label"), N_("Command*"), N_("Base*"), N_("Clear"), NULL };

typedef struct TableFields {
	RowWidgets 	**rows;
	GtkWidget	*regex;
} TableFields;

GtkWidget *build_commands_table(GeanyDocument *doc, GeanyBuildSource dst, TableData *table_data, GeanyFiletype *ft)
{
	GtkWidget		*label, *sep, *regex, *clearicon, *clear;
	TableFields		*fields;
	GtkTable		*table;
	gchar			**ch, *txt;
	gint			 col, row, cmdindex, cmd;
	
	table = GTK_TABLE(gtk_table_new(build_items_count+12, 5, FALSE));
	fields = g_new0(TableFields, 1);
	fields->rows = g_new0(RowWidgets *, build_items_count);
	for (ch= colheads, col=0; *ch!=NULL; ch++, col++)
	{
		label = gtk_label_new(gettext(*ch));
		gtk_table_attach(table, label, col, col+1, 0, 1,
							GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	}
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, 6, 1, 2, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	if (ft!=NULL){
		txt = g_strdup_printf(_("%s commands"), ft->title);
	} else
		txt = g_strdup(_("No Filetype"));
	label = gtk_label_new(txt);
	g_free(txt);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, 6, 2, 3, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	for (row=3, cmdindex=0, cmd=0; cmd<build_groups_count[GBG_FT]; ++row, ++cmdindex, ++cmd)
		fields->rows[cmdindex] = build_add_dialog_row(doc, table, row, dst, GBG_FT, cmd, FALSE);
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, 6, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	++row;
	label = gtk_label_new(_("Non Filetype Comamnds"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, 6, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	for (++row, cmd=0; cmd<build_groups_count[GBG_NON_FT]; ++row,++cmdindex, ++cmd)
		fields->rows[cmdindex] = build_add_dialog_row(doc, table, row, dst, GBG_NON_FT, cmd, TRUE);
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, 6, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	++row;
	label = gtk_label_new(_("Execute Comamnds"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, 6, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	for (++row, cmd=0; cmd<build_groups_count[GBG_EXEC]; ++row,++cmdindex, ++cmd)
		fields->rows[cmdindex] = build_add_dialog_row(doc, table, row, dst, GBG_EXEC, cmd, TRUE);
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, 6, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	++row;
	label = gtk_label_new(_("Error Regular Expression"));
	gtk_table_attach(table, label, 0, 2, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	fields->regex = gtk_entry_new();
	gtk_table_attach(table, fields->regex, 2, 4, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	clearicon = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_SMALL_TOOLBAR);
	clear = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(clear), clearicon);
	/* TODO clear callback */
	gtk_table_attach(table, clear, 4, 5, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	++row;
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, 6, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
	++row;
	label = gtk_label_new(_(
				"* Notes:\n"
				"  In commands, %f is replaced by filename and\n"
				"   %e is replaced by filename without extension\n"
				"  Base executes command in base directory\n"
				"  Non-filetype menu Item 2 opens a dialog\n"
				"   and appends the reponse to the command"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, 6, row, row+1, GTK_FILL, GTK_FILL | GTK_EXPAND, 0, 0);
/*	printf("%d extra rows in dialog\n", row-build_items_count); */
	++row;
	*table_data = fields;
	return GTK_WIDGET(table);
}

/* string compare where null pointers match null or 0 length strings */
static int stcmp(const gchar *a, const gchar *b)
{
	if (a==NULL && b==NULL) return 0;
	if (a==NULL && b!=NULL) return strlen(b);
	if (a!=NULL && b==NULL) return strlen(a);
	return strcmp(a, b);
}

void free_build_fields(TableData table_data)
{
	gint cmdindex;
	for (cmdindex=0; cmdindex<build_items_count; ++cmdindex)
		g_free(table_data->rows[cmdindex]);
	g_free(table_data->rows);
	g_free(table_data);
}

/* sets of commands to get from the build dialog, NULL gets filetype pointer later */
static GeanyBuildCommand **proj_cmds[GBG_COUNT]={ NULL, &non_ft_proj, &exec_proj }; /* indexed by GBG */
static GeanyBuildCommand **pref_cmds[GBG_COUNT]={ NULL, &non_ft_pref, &exec_pref };

static gboolean read_row(GeanyBuildCommand ***dstcmds, TableData table_data, gint drow, gint grp, gint cmd)
{
	gchar			*label, *command;
	gboolean		 dir;
	gboolean		 changed = FALSE;
	GeanyBuildSource src;

	src = table_data->rows[drow]->src;
	label = g_strdup(gtk_entry_get_text(GTK_ENTRY(table_data->rows[drow]->label)));
	command = g_strdup(gtk_entry_get_text(GTK_ENTRY(table_data->rows[drow]->command)));
	dir = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(table_data->rows[drow]->dir));
	if (table_data->rows[drow]->cleared)
	{
		if (dstcmds[grp]!=NULL)
		{
			if (*(dstcmds[grp])==NULL)*(dstcmds[grp])=g_new0(GeanyBuildCommand, build_groups_count[grp]);
			(*(dstcmds[grp]))[cmd].exists=FALSE;
			(*(dstcmds[grp]))[cmd].changed=TRUE;
			changed=TRUE;
		}
	}
	if ((table_data->rows[drow]->cmdsrc==NULL &&                          /* originally there was no content */
		  (strlen(label)>0 || strlen(command)>0  || dir)) ||              /* and now there is some  or */
		(table_data->rows[drow]->cmdsrc!=NULL &&                          /* originally there was content and */
		  (stcmp(label, table_data->rows[drow]->cmdsrc->label)!=0 ||      /* label is different or */
		    stcmp(command, table_data->rows[drow]->cmdsrc->command)!=0 || /* command is different or */
		    dir != table_data->rows[drow]->cmdsrc->run_in_base_dir)))     /* runinbasedir is different */
	{
		if (dstcmds[grp]!=NULL)
		{
			if (*(dstcmds[grp])==NULL)
				*(dstcmds[grp]) = g_new0(GeanyBuildCommand, build_groups_count[grp]);
			setptr((*(dstcmds[grp]))[cmd].label, label);
			setptr((*(dstcmds[grp]))[cmd].command, command);
			(*(dstcmds[grp]))[cmd].run_in_base_dir = dir;
			(*(dstcmds[grp]))[cmd].exists = TRUE;
			(*(dstcmds[grp]))[cmd].changed=TRUE;
			changed = TRUE;
		}
	}
	else
	{
		g_free(label); g_free(command);
	}
	return changed;
}

gboolean read_build_commands(GeanyBuildCommand ***dstcmds, TableData table_data, gint response)
{
	gint			 cmdindex, grp, cmd;
	gboolean		 changed = FALSE;
	
	if (response == GTK_RESPONSE_ACCEPT)
	{
		for (cmdindex=0, cmd=0; cmd<build_groups_count[GBG_FT]; ++cmdindex, ++cmd)
			changed |= read_row(dstcmds, table_data, cmdindex, GBG_FT, cmd);
		for (cmd=0; cmd<build_groups_count[GBG_NON_FT]; ++cmdindex, ++cmd)
			changed |= read_row(dstcmds, table_data, cmdindex, GBG_NON_FT, cmd);
		for (cmd=0; cmd<build_groups_count[GBG_EXEC]; ++cmdindex, ++cmd)
			changed |= read_row(dstcmds, table_data, cmdindex, GBG_EXEC, cmd);
		/* regex */
	}
	return changed;
}

static void show_build_commands_dialog()
{
	GtkWidget		*dialog, *table;
	GeanyDocument	*doc = document_get_current();
	GeanyFiletype	*ft = NULL;
	gchar			*title = _("Set Build Commands");
	gint			 cmdindex, response;
	TableData		 table_data;

	if (doc != NULL)
		ft = doc->file_type;
	if (ft!=NULL)pref_cmds[GBG_FT]= &(ft->homefilecmds);
	else pref_cmds[GBG_FT] = NULL;
	dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(main_widgets.window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	table = build_commands_table(doc, BCS_PREF, &table_data, ft);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0); 
	gtk_widget_show_all(dialog);
	/* run modally to prevent user changing idx filetype */
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	read_build_commands(pref_cmds, table_data, response);
	build_menu_update(doc);
	free_build_fields(table_data);
	gtk_widget_destroy(dialog);
}

/* Creates the relevant build menu if necessary. */
BuildMenuItems *build_get_menu_items(gint filetype_idx)
{
	BuildMenuItems *items;

	items = &menu_items;
	if (items->menu == NULL) create_build_menu(items);
	return items;
}

/*----------------------------------------------------------
 * 
 * Load and store configuration
 * 
 * ---------------------------------------------------------*/

static const gchar *build_grp_name = "build-menu";

/* config format for build-menu group is prefix_gg_nn_xx=value
 * where gg = FT, NF, EX for the command group
 *       nn = 2 digit command number
 *       xx = LB for label, CM for command and BD for basedir */
static const gchar *groups[GBG_COUNT] = { "FT", "NF", "EX" };
static const gchar *fixedkey="xx_xx_xx";

#define set_key_grp(key,grp) (key[prefixlen+0]=grp[0], key[prefixlen+1]=grp[1])
#define set_key_cmd(key,cmd) (key[prefixlen+3]=cmd[0], key[prefixlen+4]=cmd[1])
#define set_key_fld(key,fld) (key[prefixlen+6]=fld[0], key[prefixlen+7]=fld[1])

static void load_build_menu_grp(GKeyFile *config, GeanyBuildCommand **dst, gint grp, gchar *prefix, gboolean loc)
{
	gint cmd, prefixlen; /* NOTE prefixlen used in macros above */
	GeanyBuildCommand *dstcmd;
	gchar *key;
	static gchar cmdbuf[3]="  ";
	
	if (*dst==NULL)*dst = g_new0(GeanyBuildCommand, build_groups_count[grp]);
	dstcmd = *dst;
	prefixlen = prefix==NULL?0:strlen(prefix);
	key = g_strconcat(prefix==NULL?"":prefix, fixedkey, NULL);
	for (cmd=0; cmd<build_groups_count[grp]; ++cmd)
	{
		gchar *label;
		if (cmd<0 || cmd>=100)return; /* ensure no buffer overflow */
		sprintf(cmdbuf, "%02d", cmd);
		set_key_grp(key, groups[grp]);
		set_key_cmd(key, cmdbuf);
		set_key_fld(key, "LB");
		if (loc)
			label = g_key_file_get_locale_string(config, build_grp_name, key, NULL, NULL);
		else
			label = g_key_file_get_string(config, build_grp_name, key, NULL);
		if (label!=NULL)
		{
			dstcmd[cmd].exists = TRUE;
			setptr(dstcmd[cmd].label, label);
			set_key_fld(key,"CM");
			setptr(dstcmd[cmd].command, g_key_file_get_string(config, build_grp_name, key, NULL));
			set_key_fld(key,"BD");
			dstcmd[cmd].run_in_base_dir = g_key_file_get_boolean(config, build_grp_name, key, NULL);
		}
		else dstcmd[cmd].exists = FALSE;
	}
	g_free(key);
}

/* for the specified source load new format build menu items or try to make some sense of 
 * old format setings, not done perfectly but better than ignoring them */
void load_build_menu(GKeyFile *config, GeanyBuildSource src, gpointer p)
{
/*	gint grp;*/
	GeanyFiletype 	*ft;
	GeanyProject  	*pj;
	gchar 			**ftlist;

	if (g_key_file_has_group(config, build_grp_name))
	{
		switch(src)
		{
			case BCS_FT:
				ft = (GeanyFiletype*)p;
				if (ft==NULL)return;
				load_build_menu_grp(config, &(ft->filecmds), GBG_FT, NULL, TRUE);
				load_build_menu_grp(config, &(ft->ftdefcmds), GBG_NON_FT, NULL, TRUE);
				load_build_menu_grp(config, &(ft->execcmds), GBG_EXEC, NULL, TRUE);
				break;
			case BCS_HOME_FT:
				ft = (GeanyFiletype*)p;
				if (ft==NULL)return;
				load_build_menu_grp(config, &(ft->homefilecmds), GBG_FT, NULL, FALSE);
				load_build_menu_grp(config, &(ft->homeexeccmds), GBG_EXEC, NULL, FALSE);
				break;
			case BCS_PREF:
				load_build_menu_grp(config, &non_ft_pref, GBG_NON_FT, NULL, FALSE);
				load_build_menu_grp(config, &exec_pref, GBG_EXEC, NULL, FALSE);
				break;
			case BCS_PROJ:
				load_build_menu_grp(config, &non_ft_proj, GBG_NON_FT, NULL, FALSE);
				load_build_menu_grp(config, &exec_proj, GBG_EXEC, NULL, FALSE);
				pj = (GeanyProject*)p;
				if (p==NULL)return;
				ftlist = g_key_file_get_string_list(config, build_grp_name, "filetypes", NULL, NULL);
				if (ftlist!=NULL)
				{
					gchar **ftname;
					GeanyFiletype *ft;
					if (pj->build_filetypes_list==NULL) pj->build_filetypes_list = g_ptr_array_new();
					g_ptr_array_set_size(pj->build_filetypes_list, 0);
					for (ftname=ftlist; ftname!=NULL; ++ftname)
					{
						ft=filetypes_lookup_by_name(*ftname);
						if (ft!=NULL)
						{
							g_ptr_array_add(pj->build_filetypes_list, ft);
							load_build_menu_grp(config, &(ft->projfilecmds), GBG_FT, *ftname, FALSE);
							load_build_menu_grp(config, &(ft->projexeccmds), GBG_EXEC, *ftname, FALSE);
						}
					}
					g_free(ftlist);
				}
				break;
			default: /* defaults don't load from config, see build_init */
				break;
		}
	}
	else
	{
		gchar *value;
		gboolean bvalue;
		gint cmd;
		switch(src)
		{
			case BCS_FT:
				ft = (GeanyFiletype*)p;
				if (ft->filecmds==NULL)ft->filecmds = g_new0(GeanyBuildCommand, build_groups_count[GBG_FT]);
				value = g_key_file_get_string(config, "build_settings", "compiler", NULL);
				if (value != NULL)
				{
					ft->filecmds[GBO_TO_CMD(GBO_COMPILE)].exists = TRUE;
					ft->filecmds[GBO_TO_CMD(GBO_COMPILE)].label = g_strdup(_("_Compile"));
					ft->filecmds[GBO_TO_CMD(GBO_COMPILE)].command = value;
				}
				value = g_key_file_get_string(config, "build_settings", "linker", NULL);
				if (value != NULL)
				{
					ft->filecmds[GBO_TO_CMD(GBO_BUILD)].exists = TRUE;
					ft->filecmds[GBO_TO_CMD(GBO_BUILD)].label = g_strdup(_("_Build"));
					ft->filecmds[GBO_TO_CMD(GBO_BUILD)].command = value;
				}
				if (ft->execcmds==NULL)ft->execcmds = g_new0(GeanyBuildCommand, build_groups_count[GBG_EXEC]);
				value = g_key_file_get_string(config, "build_settings", "run_cmd", NULL);
				if (value != NULL)
				{
					ft->execcmds[GBO_TO_CMD(GBO_EXEC)].exists = TRUE;
					ft->execcmds[GBO_TO_CMD(GBO_EXEC)].label = g_strdup(_("_Execute"));
					ft->execcmds[GBO_TO_CMD(GBO_EXEC)].command = value;
				}
				break;
			case BCS_PROJ:
				if (non_ft_proj==NULL)non_ft_proj = g_new0(GeanyBuildCommand, build_groups_count[GBG_NON_FT]);
				bvalue = g_key_file_get_boolean(config, "project", "make_in_base_path", NULL);
				non_ft_proj[GBO_TO_CMD(GBO_MAKE_ALL)].run_in_base_dir = bvalue;
				non_ft_proj[GBO_TO_CMD(GBO_MAKE_CUSTOM)].run_in_base_dir = bvalue;
				non_ft_proj[GBO_TO_CMD(GBO_MAKE_OBJECT)].run_in_base_dir = bvalue;
				for (cmd=GBO_TO_CMD(GBO_MAKE_OBJECT)+1; cmd<build_groups_count[GBG_NON_FT]; ++cmd)
					non_ft_proj[cmd].run_in_base_dir = bvalue;
				value = g_key_file_get_string(config, "project", "run_cmd", NULL);
				if (value !=NULL)
				{
					if (exec_proj==NULL)exec_proj = g_new0(GeanyBuildCommand, build_groups_count[GBG_EXEC]);
					exec_proj[GBO_TO_CMD(GBO_EXEC)].exists = TRUE;
					exec_proj[GBO_TO_CMD(GBO_EXEC)].label = g_strdup(_("Execute"));
					exec_proj[GBO_TO_CMD(GBO_EXEC)].command = value;
				}
				break;
			case BCS_PREF:
				if (non_ft_pref==NULL)non_ft_pref = g_new0(GeanyBuildCommand, build_groups_count[GBG_NON_FT]);
				value = g_key_file_get_string(config, "tools", "make_cmd", NULL);
				if (value!=NULL)
				{
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_ALL)].exists = TRUE;
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_ALL)].label = g_strdup(_("_Make All"));
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_ALL)].command = value;
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_CUSTOM)].exists = TRUE;
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_CUSTOM)].label = g_strdup(_("Make Custom _Target"));
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_CUSTOM)].command = g_strdup_printf("%s ",value);
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_OBJECT)].exists = TRUE;
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_OBJECT)].label = g_strdup(_("Make _Object"));
					non_ft_pref[GBO_TO_CMD(GBO_MAKE_OBJECT)].command = g_strdup_printf("%s %%e.o",value);
				}
				break;
			default:
				break;
		}
	}
}

static void save_build_menu_grp(GKeyFile *config, GeanyBuildCommand *src, gint grp, gchar *prefix)
{
	gint cmd, prefixlen; /* NOTE prefixlen used in macros above */
	gchar *key;
	
	if (src==NULL)return;
	prefixlen = prefix==NULL?0:strlen(prefix);
	key = g_strconcat(prefix==NULL?"":prefix, fixedkey, NULL);
	for (cmd=0; cmd<build_groups_count[grp]; ++cmd)
	{
		if (src[cmd].changed)
		{
			static gchar cmdbuf[3]="  ";
			if (cmd<0 || cmd>=100)return; /* ensure no buffer overflow */
			sprintf(cmdbuf, "%02d", cmd);
			set_key_grp(key, groups[grp]);
			set_key_cmd(key, cmdbuf);
			set_key_fld(key, "LB");
			if (src[cmd].exists)
			{
				g_key_file_set_string(config, build_grp_name, key, src[cmd].label);
				set_key_fld(key,"CM");
				g_key_file_set_string(config, build_grp_name, key, src[cmd].command);
				set_key_fld(key,"BD");
				g_key_file_set_boolean(config, build_grp_name, key, src[cmd].run_in_base_dir);
			}
			else
			{
				g_key_file_remove_key(config, build_grp_name, key, NULL);
				set_key_fld(key,"CM");
				g_key_file_remove_key(config, build_grp_name, key, NULL);
				set_key_fld(key,"BD");
				g_key_file_remove_key(config, build_grp_name, key, NULL);
			}
		}
	}
	g_free(key);
	
}

static void foreach_project_filetype(gpointer data, gpointer user_data)
{
	GeanyFiletype *ft = (GeanyFiletype*)data;
	GKeyFile *config = (GKeyFile*)user_data;
	save_build_menu_grp(config, ft->projfilecmds, GBG_FT, ft->name);
	save_build_menu_grp(config, ft->projexeccmds, GBG_EXEC, ft->name);
}

void save_build_menu(GKeyFile *config, gpointer ptr, GeanyBuildSource src)
{
	GeanyFiletype *ft;
	GeanyProject  *pj;
	switch(src)
	{
		case BCS_HOME_FT:
			ft = (GeanyFiletype*)ptr;
			if (ft==NULL)return;
			save_build_menu_grp(config, ft->homefilecmds, GBG_FT, NULL);
			save_build_menu_grp(config, ft->homeexeccmds, GBG_EXEC, NULL);
			break;
		case BCS_PREF:
			save_build_menu_grp(config, non_ft_pref, GBG_NON_FT, NULL);
			save_build_menu_grp(config, exec_pref, GBG_EXEC, NULL);
			break;
		case BCS_PROJ:
			pj = (GeanyProject*)ptr;
			save_build_menu_grp(config, non_ft_proj, GBG_NON_FT, NULL);
			save_build_menu_grp(config, exec_proj, GBG_EXEC, NULL);
			g_ptr_array_foreach(pj->build_filetypes_list, foreach_project_filetype, (gpointer)config);
			break;
		default: /* defaults and BCS_FT can't save */
			break;
	}
}

void set_build_grp_count(GeanyBuildGroup grp, guint count)
{
	gint i, sum;
	if (count>build_groups_count[grp])
		build_groups_count[grp]=count;
	for (i=0, sum=0; i<GBG_COUNT; ++i)sum+=build_groups_count[i];
	build_items_count = sum;
}

static struct {
	gchar *label,*command;
	gboolean dir;
	GeanyBuildCommand **ptr;
	gint index;
} default_cmds[] = { 
	{N_("_Make"), "make", FALSE, &non_ft_def, GBO_TO_CMD(GBO_MAKE_ALL)}, 
	{N_("Make Custom _Target"), "make ", FALSE, &non_ft_def, GBO_TO_CMD(GBO_MAKE_CUSTOM)}, 
	{N_("Make _Object"), "make %e.o", FALSE, &non_ft_def, GBO_TO_CMD(GBO_MAKE_OBJECT)},
	{N_("_Execute"), "./%e", FALSE, &exec_def, GBO_TO_CMD(GBO_EXEC)},
	{NULL, NULL, FALSE, NULL, 0 }
};

void build_init()
{
	GtkWidget *item;
	GtkWidget *toolmenu;
	gint cmdindex, defindex;

	ft_def = g_new0(GeanyBuildCommand, build_groups_count[GBG_FT]);
	non_ft_def = g_new0(GeanyBuildCommand, build_groups_count[GBG_NON_FT]);
	exec_def = g_new0(GeanyBuildCommand, build_groups_count[GBG_EXEC]);
	for (cmdindex=0; default_cmds[cmdindex].label!=NULL; ++cmdindex)
	{
		GeanyBuildCommand *cmd = &((*(default_cmds[cmdindex].ptr))[ default_cmds[cmdindex].index ]);
		cmd->exists = TRUE;
		cmd->label = g_strdup(default_cmds[cmdindex].label);
		cmd->command = g_strdup(default_cmds[cmdindex].command);
		cmd->run_in_base_dir = default_cmds[cmdindex].dir;
	}

	widgets.build_action = toolbar_get_action_by_name("Build");
	toolmenu = geany_menu_button_action_get_menu(GEANY_MENU_BUTTON_ACTION(widgets.build_action));

	if (toolmenu != NULL)
	{
		/* build the code */
		item = ui_image_menu_item_new(GEANY_STOCK_BUILD, _("_Build"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(toolmenu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_build_activate), NULL);
		widgets.toolitem_build = item;

		item = gtk_separator_menu_item_new();
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(toolmenu), item);

		/* build the code with make all */
		item = gtk_image_menu_item_new_with_mnemonic(_("_Make All"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(toolmenu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_make_activate),
			GINT_TO_POINTER(GBO_MAKE_ALL));
		widgets.toolitem_make_all = item;

		/* build the code with make custom */
		item = gtk_image_menu_item_new_with_mnemonic(_("Make Custom _Target"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(toolmenu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_make_activate),
			GINT_TO_POINTER(GBO_MAKE_CUSTOM));
		widgets.toolitem_make_custom = item;

		/* build the code with make object */
		item = gtk_image_menu_item_new_with_mnemonic(_("Make _Object"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(toolmenu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_make_activate),
			GINT_TO_POINTER(GBO_MAKE_OBJECT));
		widgets.toolitem_make_object = item;

		item = gtk_separator_menu_item_new();
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(toolmenu), item);

		/* arguments */
		item = ui_image_menu_item_new(GTK_STOCK_PREFERENCES, _("_Set Build Commands"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(toolmenu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_set_build_commands_activate), NULL);
		widgets.toolitem_set_args = item;
	}

	widgets.compile_button = toolbar_get_widget_by_name("Compile");
	widgets.run_button = toolbar_get_widget_by_name("Run");
	widgets.build_button = toolbar_get_widget_by_name("Build");
}


