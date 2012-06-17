/*
 *      build.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2009 Lex Trotman <elextr(at)gmail(dot)com>
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

/*
 * Build commands and menu items.
 */
/* TODO: tidy code:
 * Use intermediate pointers for common subexpressions.
 * Replace defines with enums.
 * Other TODOs in code. */

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

/* g_spawn_async_with_pipes doesn't work on Windows */
#ifdef G_OS_WIN32
#define SYNC_SPAWN
#endif

/* Number of editor indicators to draw - limited as this can affect performance */
#define GEANY_BUILD_ERR_HIGHLIGHT_MAX 50


GeanyBuildInfo build_info = {GEANY_GBG_FT, 0, 0, NULL, GEANY_FILETYPES_NONE, NULL, 0};

static gchar *current_dir_entered = NULL;

typedef struct RunInfo
{
	GPid pid;
	gint file_type_id;
} RunInfo;

static RunInfo *run_info;

#ifdef G_OS_WIN32
static const gchar RUN_SCRIPT_CMD[] = "geany_run_script.bat";
#else
static const gchar RUN_SCRIPT_CMD[] = "./geany_run_script.sh";
#endif

/* pack group (<8) and command (<32) into a user_data pointer */
#define GRP_CMD_TO_POINTER(grp, cmd) GUINT_TO_POINTER((((grp)&7) << 5) | ((cmd)&0x1f))
#define GBO_TO_POINTER(gbo) (GRP_CMD_TO_POINTER(GBO_TO_GBG(gbo), GBO_TO_CMD(gbo)))
#define GPOINTER_TO_CMD(gptr) (GPOINTER_TO_UINT(gptr)&0x1f)
#define GPOINTER_TO_GRP(gptr) ((GPOINTER_TO_UINT(gptr)&0xe0) >> 5)

static gpointer last_toolbutton_action = GBO_TO_POINTER(GEANY_GBO_BUILD);

static BuildMenuItems menu_items = {NULL, {NULL, NULL, NULL, NULL}};

static struct
{
	GtkAction	*run_action;
	GtkAction	*compile_action;
	GtkAction	*build_action;
	GtkWidget	*toolmenu;

	GtkWidget	*toolitem_build;
	GtkWidget	*toolitem_make_all;
	GtkWidget	*toolitem_make_custom;
	GtkWidget	*toolitem_make_object;
	GtkWidget	*toolitem_set_args;
}
widgets;

static guint build_groups_count[GEANY_GBG_COUNT] = { 3, 4, 2 };
static guint build_items_count = 9;

#ifndef SYNC_SPAWN
static void build_exit_cb(GPid child_pid, gint status, gpointer user_data);
static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data);
#endif
static gboolean build_create_shellscript(const gchar *fname, const gchar *cmd, gboolean autoclose, GError **error);
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
static void on_build_menu_item(GtkWidget *w, gpointer user_data);

void build_finalize(void)
{
	g_free(build_info.dir);
	g_free(build_info.custom_target);

	if (menu_items.menu != NULL && GTK_IS_WIDGET(menu_items.menu))
		gtk_widget_destroy(menu_items.menu);
}


/* note: copied from keybindings.c, may be able to go away */
static void add_menu_accel(GeanyKeyGroup *group, guint kb_id,
	GtkAccelGroup *accel_group, GtkWidget *menuitem)
{
	GeanyKeyBinding *kb = keybindings_get_item(group, kb_id);

	if (kb->key != 0)
		gtk_widget_add_accelerator(menuitem, "activate", accel_group,
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
}


/* convenience routines to access parts of GeanyBuildCommand */
static gchar *id_to_str(GeanyBuildCommand *bc, gint id)
{
	switch (id)
	{
		case GEANY_BC_LABEL:
			return bc->label;
		case GEANY_BC_COMMAND:
			return bc->command;
		case GEANY_BC_WORKING_DIR:
			return bc->working_dir;
	}
	g_assert(0);
	return NULL;
}


static void set_command(GeanyBuildCommand *bc, gint id, gchar *str)
{
	switch (id)
	{
		case GEANY_BC_LABEL:
			SETPTR(bc->label, str);
			break;
		case GEANY_BC_COMMAND:
			SETPTR(bc->command, str);
			break;
		case GEANY_BC_WORKING_DIR:
			SETPTR(bc->working_dir, str);
			break;
		default:
			g_assert(0);
	}
}


static const gchar *config_keys[GEANY_BC_CMDENTRIES_COUNT] = {
	"LB", /* label */
	"CM", /* command */
	"WD"  /* working directory */
};

/*-----------------------------------------------------
 *
 * Execute commands and handle results
 *
 *-----------------------------------------------------*/

/* the various groups of commands not in the filetype struct */
static GeanyBuildCommand *ft_def = NULL;
static GeanyBuildCommand *non_ft_proj = NULL;
static GeanyBuildCommand *non_ft_pref = NULL;
static GeanyBuildCommand *non_ft_def = NULL;
static GeanyBuildCommand *exec_proj = NULL;
static GeanyBuildCommand *exec_pref = NULL;
static GeanyBuildCommand *exec_def = NULL;
/* and the regexen not in the filetype structure */
static gchar *regex_pref = NULL;
/* project non-fileregex string */
static gchar *regex_proj = NULL;

/* control if build commands are printed by get_build_cmd, for debug purposes only*/
#ifndef PRINTBUILDCMDS
#define PRINTBUILDCMDS FALSE
#endif
static gboolean printbuildcmds = PRINTBUILDCMDS;


/* for debug only, print the commands structures in priority order */
static void printfcmds(void)
{
#if 0
	GeanyBuildCommand **cl[GEANY_GBG_COUNT][GEANY_BCS_COUNT] = {
		/* GEANY_BCS_DEF, GEANY_BCS_FT, GEANY_BCS_HOME_FT, GEANY_BCS_PREF,
		 * GEANY_BCS_FT_PROJ, GEANY_BCS_PROJ */
		{ &ft_def, NULL, NULL, NULL, NULL, NULL },
		{ &non_ft_def, NULL, NULL, &non_ft_pref, NULL, &non_ft_proj },
		{ &exec_def, NULL, NULL, &exec_pref, NULL, &exec_proj }
	};
	GeanyFiletype *ft = NULL;
	GeanyDocument *doc;
	gint i, j, k, l, m;
	enum GeanyBuildCmdEntries n;
	gint cc[GEANY_BCS_COUNT];
	gchar c;

	doc = document_get_current();
	if (doc != NULL)
		ft = doc->file_type;
	if (ft != NULL)
	{
		printf("filetype %s\n",ft->name);
		cl[GEANY_GBG_FT][GEANY_BCS_FT] = &(ft->filecmds);
		cl[GEANY_GBG_FT][GEANY_BCS_HOME_FT] = &(ft->homefilecmds);
		cl[GEANY_GBG_FT][GEANY_BCS_PROJ] = &(ft->projfilecmds);
		cl[GEANY_GBG_NON_FT][GEANY_BCS_FT] = &(ft->ftdefcmds);
		cl[GEANY_GBG_EXEC][GEANY_BCS_FT] = &(ft->execcmds);
		cl[GEANY_GBG_EXEC][GEANY_BCS_HOME_FT] = &(ft->homeexeccmds);
		cl[GEANY_GBG_EXEC][GEANY_BCS_PROJ_FT] = &(ft->projexeccmds);
	}
	for (i = 0; i < GEANY_BCS_COUNT; ++i)
	{
		m = 1;
		for (j = 0; j < GEANY_GBG_COUNT; ++j)
		{
			for (k = 0; k < build_groups_count[j]; ++k)
				if (cl[j][i] != NULL && *(cl[j][i]) != NULL && (*(cl[j][i]))[k].exists)
				{
					for (n = 0; n < GEANY_BC_CMDENTRIES_COUNT; n++)
					{
						if ((*(cl[j][i]))[k].entries[n] != NULL &&
							(l = strlen((*(cl[j][i]))[k].entries[n])) > m)
						{
							m = l;
						}
					}
				}
		}
		cc[i] = m;
	}
	for (i = 0; i < GEANY_GBG_COUNT; ++i)
	{
		for (k = 0; k < build_groups_count[i]; ++k)
		{
			for (l = 0; l < 2; ++l)
			{
				c = ' ';
				for (j = 0; j < GEANY_BCS_COUNT; ++j)
				{
					if (cl[i][j] != NULL && *(cl[i][j]) != NULL && (*(cl[i][j]))[k].exists)
					{
						for (n = 0; n < GEANY_BC_CMDENTRIES_COUNT; n++)
						{
							if ((*(cl[i][j]))[k].entries[i] != NULL)
								printf("%c %*.*s",c,cc[j],cc[j],(*(cl[i][j]))[k].entries[i]);
							else
								printf("%c %*.*s",c,cc[j],cc[j]," ");
						}
					}
					else
						printf("%c %*.*s",c,cc[j],cc[j]," ");
					c = ',';
				}
				printf("\n");
			}
		}
		printf("\n");
	}
#endif
}


/* macros to save typing and make the logic visible */
#define return_cmd_if(src, cmds)\
	if (cmds != NULL && cmds[cmdindex].exists && below>src)\
	{ \
		*fr=src; \
		if (printbuildcmds) \
			printf("cmd[%d,%d]=%d\n",cmdgrp,cmdindex,src); \
		return &(cmds[cmdindex]); \
	}

#define return_ft_cmd_if(src, cmds)\
	if (ft != NULL && ft->cmds != NULL \
		&& ft->cmds[cmdindex].exists && below>src)\
		{ \
			*fr=src; \
			if (printbuildcmds) \
				printf("cmd[%d,%d]=%d\n",cmdgrp,cmdindex,src); \
			return &(ft->cmds[cmdindex]); \
		}


/* get the next lowest command taking priority into account */
static GeanyBuildCommand *get_next_build_cmd(GeanyDocument *doc, guint cmdgrp, guint cmdindex,
											 guint below, guint *from)
{
	/* Note: parameter below used in macros above */
	GeanyFiletype *ft = NULL;
	guint sink, *fr = &sink;

	if (printbuildcmds)
		printfcmds();
	if (cmdgrp >= GEANY_GBG_COUNT)
		return NULL;
	if (from != NULL)
		fr = from;
	if (doc == NULL)
		doc = document_get_current();
	if (doc != NULL)
		ft = doc->file_type;

	switch (cmdgrp)
	{
		case GEANY_GBG_FT: /* order proj ft, home ft, ft, defft */
			if (ft != NULL)
			{
				return_ft_cmd_if(GEANY_BCS_PROJ, projfilecmds);
				return_ft_cmd_if(GEANY_BCS_PREF, homefilecmds);
				return_ft_cmd_if(GEANY_BCS_FT, filecmds);
			}
			return_cmd_if(GEANY_BCS_DEF, ft_def);
			break;
		case GEANY_GBG_NON_FT: /* order proj, pref, def */
			return_cmd_if(GEANY_BCS_PROJ, non_ft_proj);
			return_cmd_if(GEANY_BCS_PREF, non_ft_pref);
			return_ft_cmd_if(GEANY_BCS_FT, ftdefcmds);
			return_cmd_if(GEANY_BCS_DEF, non_ft_def);
			break;
		case GEANY_GBG_EXEC: /* order proj, proj ft, pref, home ft, ft, def */
			return_cmd_if(GEANY_BCS_PROJ, exec_proj);
			return_ft_cmd_if(GEANY_BCS_PROJ_FT, projexeccmds);
			return_cmd_if(GEANY_BCS_PREF, exec_pref);
			return_ft_cmd_if(GEANY_BCS_FT, homeexeccmds);
			return_ft_cmd_if(GEANY_BCS_FT, execcmds);
			return_cmd_if(GEANY_BCS_DEF, exec_def);
			break;
		default:
			break;
	}
	return NULL;
}


/* shortcut to start looking at the top */
static GeanyBuildCommand *get_build_cmd(GeanyDocument *doc, guint grp, guint cmdindex, guint *from)
{
	return get_next_build_cmd(doc, grp, cmdindex, GEANY_BCS_COUNT, from);
}


#define return_nonblank_regex(src, ptr)\
	if (NZV(ptr)) \
		{ *fr = (src); return &(ptr); }


/* like get_build_cmd, but for regexen, used by filetypes */
gchar **build_get_regex(GeanyBuildGroup grp, GeanyFiletype *ft, guint *from)
{
	guint sink, *fr = &sink;

	if (from != NULL)
		fr = from;
	if (grp == GEANY_GBG_FT)
	{
		if (ft == NULL)
		{
			GeanyDocument *doc = document_get_current();
			if (doc != NULL)
				ft = doc->file_type;
		}
		if (ft == NULL)
			return NULL;
		return_nonblank_regex(GEANY_BCS_PROJ, ft->projerror_regex_string);
		return_nonblank_regex(GEANY_BCS_HOME_FT, ft->homeerror_regex_string);
		return_nonblank_regex(GEANY_BCS_FT, ft->error_regex_string);
	}
	else if (grp == GEANY_GBG_NON_FT)
	{
		return_nonblank_regex(GEANY_BCS_PROJ, regex_proj);
		return_nonblank_regex(GEANY_BCS_PREF, regex_pref);
	}
	return NULL;
}


static GeanyBuildCommand **get_build_group_pointer(const GeanyBuildSource src, const GeanyBuildGroup grp)
{
	GeanyDocument *doc;
	GeanyFiletype *ft = NULL;

	switch (grp)
	{
		case GEANY_GBG_FT:
			if ((doc = document_get_current()) == NULL)
				return NULL;
			if ((ft = doc->file_type) == NULL)
				return NULL;
			switch (src)
			{
				case GEANY_BCS_DEF:	 return &(ft->ftdefcmds);
				case GEANY_BCS_FT:	  return &(ft->filecmds);
				case GEANY_BCS_HOME_FT: return &(ft->homefilecmds);
				case GEANY_BCS_PREF:	return &(ft->homefilecmds);
				case GEANY_BCS_PROJ:	return &(ft->projfilecmds);
				default: return NULL;
			}
			break;
		case GEANY_GBG_NON_FT:
			switch (src)
			{
				case GEANY_BCS_DEF:	 return &(non_ft_def);
				case GEANY_BCS_PREF:	return &(non_ft_pref);
				case GEANY_BCS_PROJ:	return &(non_ft_proj);
				default: return NULL;
			}
			break;
		case GEANY_GBG_EXEC:
			if ((doc = document_get_current()) != NULL)
				ft = doc->file_type;
			switch (src)
			{
				case GEANY_BCS_DEF:	 return &(exec_def);
				case GEANY_BCS_FT:	  return ft ? &(ft->execcmds): NULL;
				case GEANY_BCS_HOME_FT: return ft ? &(ft->homeexeccmds): NULL;
				case GEANY_BCS_PROJ_FT: return ft ? &(ft->projexeccmds): NULL;
				case GEANY_BCS_PREF:	return &(exec_pref);
				case GEANY_BCS_PROJ:	return &(exec_proj);
				default: return NULL;
			}
			break;
		default:
			return NULL;
	}
}


/* get pointer to the command group array */
static GeanyBuildCommand *get_build_group(const GeanyBuildSource src, const GeanyBuildGroup grp)
{
	GeanyBuildCommand **g = get_build_group_pointer(src, grp);
	if (g == NULL) return NULL;
	return *g;
};


/** Remove the specified Build menu item.
 *
 * Makes the specified menu item configuration no longer exist. This
 * is different to setting fields to blank because the menu item
 * will be deleted from the configuration file on saving
 * (except the system filetypes settings @see Build Menu Configuration
 * section of the Manual).
 *
 * @param src the source of the menu item to remove.
 * @param grp the group of the command to remove.
 * @param cmd the index (from 0) of the command within the group. A negative
 *        value will remove the whole group.
 *
 * If any parameter is out of range does nothing.
 *
 * Updates the menu.
 * 
 **/
void build_remove_menu_item(const GeanyBuildSource src, const GeanyBuildGroup grp, const gint cmd)
{
	GeanyBuildCommand *bc;
	guint i;

	bc = get_build_group(src, grp);
	if (bc == NULL)
		return;
	if (cmd < 0)
	{
		for (i = 0; i < build_groups_count[grp]; ++i)
			bc[i].exists = FALSE;
	}
	else if ((guint) cmd < build_groups_count[grp])
		bc[cmd].exists = FALSE;
}


/* * Get the @a GeanyBuildCommand structure for the specified Build menu item.
 *
 * Get the command for any menu item specified by @a src, @a grp and @a cmd even if it is
 * hidden by higher priority commands.
 *
 * @param src the source of the specified menu item.
 * @param grp the group of the specified menu item.
 * @param cmd the index of the command within the group.
 *
 * @return a pointer to the @a GeanyBuildCommand structure or @a NULL if it doesn't exist.
 *         This is a pointer to an internal structure and must not be freed.
 *
 * @see build_menu_update
 **/
GeanyBuildCommand *build_get_menu_item(GeanyBuildSource src, GeanyBuildGroup grp, guint cmd)
{
	GeanyBuildCommand *bc;

	g_return_val_if_fail(src < GEANY_BCS_COUNT, NULL);
	g_return_val_if_fail(grp < GEANY_GBG_COUNT, NULL);
	g_return_val_if_fail(cmd < build_groups_count[grp], NULL);

	bc = get_build_group(src, grp);
	if (bc == NULL)
		return NULL;
	return &(bc[cmd]);
}


/** Get the string for the menu item field.
 *
 * Get the current highest priority command specified by @a grp and @a cmd.  This is the one
 * that the menu item will use if activated.
 *
 * @param grp the group of the specified menu item.
 * @param cmd the index of the command within the group.
 * @param fld the field to return
 *
 * @return a pointer to the constant string or @a NULL if it doesn't exist.
 *         This is a pointer to an internal structure and must not be freed.
 *
 **/
const gchar *build_get_current_menu_item(const GeanyBuildGroup grp, const guint cmd, 
                                         const GeanyBuildCmdEntries fld)
{
	GeanyBuildCommand *c;
	gchar *str = NULL;
	
	g_return_val_if_fail(grp < GEANY_GBG_COUNT, NULL);
	g_return_val_if_fail(fld < GEANY_BC_CMDENTRIES_COUNT, NULL);
	g_return_val_if_fail(cmd < build_groups_count[grp], NULL);

	c = get_build_cmd(NULL, grp, cmd, NULL);
	if (c == NULL) return NULL;
	switch (fld)
	{
		case GEANY_BC_COMMAND:
			str = c->command;
			break;
		case GEANY_BC_LABEL:
			str = c->label;
			break;
		case GEANY_BC_WORKING_DIR:
			str = c->working_dir;
			break;
		default:
			break;
	}
	return str;
};

/** Set the string for the menu item field.
 *
 * Set the specified field of the command specified by @a src, @a grp and @a cmd.
 *
 * @param src the source of the menu item 
 * @param grp the group of the specified menu item.
 * @param cmd the index of the menu item within the group.
 * @param fld the field in the menu item command to set
 * @param val the value to set the field to, is copied
 *
 **/
 
void build_set_menu_item(const GeanyBuildSource src, const GeanyBuildGroup grp, 
                         const guint cmd, const GeanyBuildCmdEntries fld, const gchar *val)
{
	GeanyBuildCommand **g;
	
	g_return_if_fail(src < GEANY_BCS_COUNT);
	g_return_if_fail(grp < GEANY_GBG_COUNT);
	g_return_if_fail(fld < GEANY_BC_CMDENTRIES_COUNT);
	g_return_if_fail(cmd < build_groups_count[grp]);

	g = get_build_group_pointer(src, grp);
	if (g == NULL) return;
	if (*g == NULL )
	{
		*g = g_new0(GeanyBuildCommand, build_groups_count[grp]);
	}
	switch (fld)
	{
		case GEANY_BC_COMMAND:
			SETPTR((*g)[cmd].command, g_strdup(val));
			(*g)[cmd].exists = TRUE;
			break;
		case GEANY_BC_LABEL:
			SETPTR((*g)[cmd].label, g_strdup(val));
			(*g)[cmd].exists = TRUE;
			break;
		case GEANY_BC_WORKING_DIR:
			SETPTR((*g)[cmd].working_dir, g_strdup(val));
			(*g)[cmd].exists = TRUE;
			break;
		default:
			break;
	}
	build_menu_update(NULL);
};

/** Set the string for the menu item field.
 *
 * Set the specified field of the command specified by @a src, @a grp and @a cmd.
 *
 * @param grp the group of the specified menu item.
 * @param cmd the index of the command within the group.
 *
 **/

void build_activate_menu_item(const GeanyBuildGroup grp, const guint cmd)
{
	on_build_menu_item(NULL, GRP_CMD_TO_POINTER(grp, cmd));
};


/* Clear all error indicators in all documents. */
static void clear_all_errors(void)
{
	guint i;

	foreach_document(i)
	{
		editor_indicator_clear_errors(documents[i]->editor);
	}
}


#ifdef SYNC_SPAWN
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
					{	/* replace any control characters in the output */
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


/* Replaces occurences of %e and %p with the appropriate filenames,
 * %d and %p replacements should be in UTF8 */
static gchar *build_replace_placeholder(const GeanyDocument *doc, const gchar *src)
{
	GString *stack;
	gchar *filename = NULL;
	gchar *replacement;
	gchar *executable = NULL;
	gchar *ret_str; /* to be freed when not in use anymore */

	stack = g_string_new(src);
	if (doc != NULL && doc->file_name != NULL)
	{
		filename = utils_get_utf8_from_locale(doc->file_name);

		/* replace %f with the filename (including extension) */
		replacement = g_path_get_basename(filename);
		utils_string_replace_all(stack, "%f", replacement);
		g_free(replacement);

		/* replace %d with the absolute path of the dir of the current file */
		replacement = g_path_get_dirname(filename);
		utils_string_replace_all(stack, "%d", replacement);
		g_free(replacement);

		/* replace %e with the filename (excluding extension) */
		executable = utils_remove_ext_from_filename(filename);
		replacement = g_path_get_basename(executable);
		utils_string_replace_all(stack, "%e", replacement);
		g_free(replacement);
	}

	/* replace %p with the current project's (absolute) base directory */
	replacement = NULL; /* prevent double free if no replacement found */
	if (app->project)
	{
		replacement = project_get_base_path();
	}
	else if (strstr(stack->str, "%p"))
	{   /* fall back to %d */
		ui_set_statusbar(FALSE, _("failed to substitute %%p, no project active"));
		if (doc != NULL && filename != NULL)
			replacement = g_path_get_dirname(filename);
	}

	utils_string_replace_all(stack, "%p", replacement);
	g_free(replacement);

	ret_str = utils_get_utf8_from_locale(stack->str);
	g_free(executable);
	g_free(filename);
	g_string_free(stack, TRUE);

	return ret_str; /* don't forget to free src also if needed */
}


/* dir is the UTF-8 working directory to run cmd in. It can be NULL to use the
 * idx document directory */
static GPid build_spawn_cmd(GeanyDocument *doc, const gchar *cmd, const gchar *dir)
{
	GError *error = NULL;
	gchar **argv;
	gchar *working_dir;
	gchar *utf8_working_dir;
	gchar *cmd_string;
	gchar *utf8_cmd_string;
#ifdef SYNC_SPAWN
	gchar *output[2];
	gint status;
#else
	gint stdout_fd;
	gint stderr_fd;
#endif

	if (!((doc != NULL && NZV(doc->file_name)) || NZV(dir)))
	{
		geany_debug("Failed to run command with no working directory");
		ui_set_statusbar(TRUE, _("Process failed, no working directory"));
		return (GPid) 1;
	}

	clear_all_errors();
	SETPTR(current_dir_entered, NULL);

	cmd_string = g_strdup(cmd);

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
	utf8_working_dir = NZV(dir) ? g_strdup(dir) : g_path_get_dirname(doc->file_name);
	working_dir = utils_get_locale_from_utf8(utf8_working_dir);

	gtk_list_store_clear(msgwindow.store_compiler);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
	msgwin_compiler_add(COLOR_BLUE, _("%s (in directory: %s)"), utf8_cmd_string, utf8_working_dir);
	g_free(utf8_working_dir);
	g_free(utf8_cmd_string);

	/* set the build info for the message window */
	g_free(build_info.dir);
	build_info.dir = g_strdup(working_dir);
	build_info.file_type_id = (doc == NULL) ? GEANY_FILETYPES_NONE : doc->file_type->id;
	build_info.message_count = 0;

#ifdef SYNC_SPAWN
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
		error = NULL;
		return (GPid) 0;
	}

#ifdef SYNC_SPAWN
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
	utils_set_up_io_channel(stdout_fd, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
		TRUE, build_iofunc, GINT_TO_POINTER(0));
	utils_set_up_io_channel(stderr_fd, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
		TRUE, build_iofunc, GINT_TO_POINTER(1));
#endif

	g_strfreev(argv);
	g_free(working_dir);

	return build_info.pid;
}


/* Returns: NULL if there was an error, or the working directory the script was created in.
 * vte_cmd_nonscript is the location of a string which is filled with the command to be used
 * when vc->skip_run_script is set, otherwise it will be set to NULL */
static gchar *prepare_run_script(GeanyDocument *doc, gchar **vte_cmd_nonscript, guint cmdindex)
{
	gchar *locale_filename = NULL;
	GeanyBuildCommand *cmd = NULL;
	gchar *executable = NULL;
	gchar *working_dir = NULL;
	const gchar *cmd_working_dir;
	gboolean autoclose = FALSE;
	gboolean result = FALSE;
	gchar *tmp;
	gchar *cmd_string;
	GError *error = NULL;

	if (vte_cmd_nonscript != NULL)
		*vte_cmd_nonscript = NULL;

	locale_filename = utils_get_locale_from_utf8(doc->file_name);

	cmd = get_build_cmd(doc, GEANY_GBG_EXEC, cmdindex, NULL);

	cmd_string = build_replace_placeholder(doc, cmd->command);
	cmd_working_dir =  cmd->working_dir;
	if (! NZV(cmd_working_dir))
		cmd_working_dir = "%d";
	working_dir = build_replace_placeholder(doc, cmd_working_dir); /* in utf-8 */

	/* only test whether working dir exists, don't change it or else Windows support will break
	 * (gspawn-win32-helper.exe is used by GLib and must be in $PATH which means current working
	 *  dir where geany.exe was started from, so we can't change it) */
	if (!NZV(working_dir) || ! g_file_test(working_dir, G_FILE_TEST_EXISTS) ||
		! g_file_test(working_dir, G_FILE_TEST_IS_DIR))
	{
		ui_set_statusbar(TRUE, _("Failed to change the working directory to \"%s\""),
				NZV(working_dir) ? working_dir : "<NULL>" );
		utils_free_pointers(2, cmd_string, working_dir, NULL);
		return NULL;
	}

#ifdef HAVE_VTE
	if (vte_info.have_vte && vc->run_in_vte)
	{
		if (vc->skip_run_script)
		{
			if (vte_cmd_nonscript != NULL)
				*vte_cmd_nonscript = cmd_string;

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
	result = build_create_shellscript(tmp, cmd_string, autoclose, &error);
	if (! result)
	{
		ui_set_statusbar(TRUE, _("Failed to execute \"%s\" (start-script could not be created: %s)"),
			NZV(cmd_string) ? cmd_string : NULL, error->message);
		g_error_free(error);
	}

	utils_free_pointers(4, cmd_string, tmp, executable, locale_filename, NULL);

	if (result)
		return working_dir;

	g_free(working_dir);
	return NULL;
}


static GPid build_run_cmd(GeanyDocument *doc, guint cmdindex)
{
	gchar *working_dir;
	gchar *vte_cmd_nonscript = NULL;
	GError *error = NULL;

	if (doc == NULL || doc->file_name == NULL)
		return (GPid) 0;

	working_dir = prepare_run_script(doc, &vte_cmd_nonscript, cmdindex);
	if (working_dir == NULL)
		return (GPid) 0;

	run_info[cmdindex].file_type_id = doc->file_type->id;

#ifdef HAVE_VTE
	if (vte_info.have_vte && vc->run_in_vte)
	{
		gchar *vte_cmd;

		if (vc->skip_run_script)
		{
			SETPTR(vte_cmd_nonscript, utils_get_utf8_from_locale(vte_cmd_nonscript));
			vte_cmd = g_strconcat(vte_cmd_nonscript, "\n", NULL);
			g_free(vte_cmd_nonscript);
		}
		else
			vte_cmd = g_strconcat("\n/bin/sh ", RUN_SCRIPT_CMD, "\n", NULL);

		/* change into current directory if it is not done by default */
		if (! vc->follow_path)
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

		run_info[cmdindex].pid = 1;

		g_free(vte_cmd);
	}
	else
#endif
	{
		gchar *locale_term_cmd = NULL;
		gchar **term_argv = NULL;
		guint term_argv_len, i;
		gchar **argv = NULL;

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
			run_info[cmdindex].pid = (GPid) 1;
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
			argv[term_argv_len] = g_strdup("/Q /C");
			argv[term_argv_len + 1] = g_strdup(RUN_SCRIPT_CMD);
		}
		else
		{
			argv[term_argv_len] = g_strdup(RUN_SCRIPT_CMD);
			argv[term_argv_len + 1] = NULL;
		}
#else
		argv[term_argv_len   ]  = g_strdup("-e");
		argv[term_argv_len + 1] = g_strconcat("/bin/sh ", RUN_SCRIPT_CMD, NULL);
#endif
		argv[term_argv_len + 2] = NULL;

		if (! g_spawn_async(working_dir, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
							NULL, NULL, &(run_info[cmdindex].pid), &error))
		{
			geany_debug("g_spawn_async() failed: %s", error->message);
			ui_set_statusbar(TRUE, _("Process failed (%s)"), error->message);
			g_unlink(RUN_SCRIPT_CMD);
			g_error_free(error);
			error = NULL;
			run_info[cmdindex].pid = (GPid) 0;
		}

		if (run_info[cmdindex].pid != 0)
		{
			g_child_watch_add(run_info[cmdindex].pid, (GChildWatchFunc) run_exit_cb,
								(gpointer)&(run_info[cmdindex]));
			build_menu_update(doc);
		}
		free_strings:
		g_strfreev(argv);
		g_strfreev(term_argv);
		g_free(locale_term_cmd);
	}

	g_free(working_dir);
	return run_info[cmdindex].pid;
}


static void process_build_output_line(const gchar *str, gint color)
{
	gchar *msg, *tmp;
	gchar *filename;
	gint line;

	msg = g_strdup(str);

	g_strchomp(msg);

	if (! NZV(msg))
	{
		g_free(msg);
		return;
	}

	if (build_parse_make_dir(msg, &tmp))
	{
		SETPTR(current_dir_entered, tmp);
	}
	msgwin_parse_compiler_error_line(msg, current_dir_entered, &filename, &line);

	if (line != -1 && filename != NULL)
	{
		GeanyDocument *doc = document_find_by_filename(filename);

		/* limit number of indicators */
		if (doc && editor_prefs.use_indicators &&
			build_info.message_count < GEANY_BUILD_ERR_HIGHLIGHT_MAX)
		{
			if (line > 0) /* some compilers, like pdflatex report errors on line 0 */
				line--;   /* so only adjust the line number if it is greater than 0 */
			editor_indicator_set_on_line(doc->editor, GEANY_INDICATOR_ERROR, line);
		}
		build_info.message_count++;
		color = COLOR_RED;	/* error message parsed on the line */
	}
	g_free(filename);

	msgwin_compiler_add_string(color, msg);
	g_free(msg);
}


#ifndef SYNC_SPAWN
static gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg;
		GIOStatus st;

		while ((st = g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL)) == G_IO_STATUS_NORMAL && msg)
		{
			gint color = (GPOINTER_TO_INT(data)) ? COLOR_DARK_RED : COLOR_BLACK;

			process_build_output_line(msg, color);
 			g_free(msg);
		}
		if (st == G_IO_STATUS_ERROR || st == G_IO_STATUS_EOF) return FALSE;
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
		msgwin_compiler_add_string(COLOR_BLUE, msg);
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


#ifndef SYNC_SPAWN
static void build_exit_cb(GPid child_pid, gint status, gpointer user_data)
{
	gboolean failure = FALSE;

#ifdef G_OS_WIN32
	failure = status;
#else
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
#endif
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
	RunInfo *run_info_data = user_data;

	g_spawn_close_pid(child_pid);

	run_info_data->pid = 0;
	/* reset the stop button and menu item to the original meaning */
	build_menu_update(NULL);
}


static void set_file_error_from_errno(GError **error, gint err, const gchar *prefix)
{
	g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(err), "%s%s%s",
		prefix ? prefix : "", prefix ? ": " : "", g_strerror(err));
}


/* write a little shellscript to call the executable (similar to anjuta_launcher but "internal")
 * fname is the full file name (including path) for the script to create */
static gboolean build_create_shellscript(const gchar *fname, const gchar *cmd, gboolean autoclose, GError **error)
{
	FILE *fp;
	gchar *str;
	gboolean success = TRUE;
#ifdef G_OS_WIN32
	gchar *expanded_cmd;
#endif

	fp = g_fopen(fname, "w");
	if (! fp)
	{
		set_file_error_from_errno(error, errno, "Failed to create file");
		return FALSE;
	}
#ifdef G_OS_WIN32
	/* Expand environment variables like %blah%. */
	expanded_cmd = win32_expand_environment_variables(cmd);
	str = g_strdup_printf("%s\n\n%s\ndel \"%%0\"\n\npause\n", expanded_cmd, (autoclose) ? "" : "pause");
	g_free(expanded_cmd);
#else
	str = g_strdup_printf(
		"#!/bin/sh\n\nrm $0\n\n%s\n\necho \"\n\n------------------\n(program exited with code: $?)\" \
		\n\n%s\n", cmd, (autoclose) ? "" :
		"\necho \"Press return to continue\"\n#to be more compatible with shells like "
			"dash\ndummy_var=\"\"\nread dummy_var");
#endif

	if (fputs(str, fp) < 0)
	{
		set_file_error_from_errno(error, errno, "Failed to write file");
		success = FALSE;
	}
	g_free(str);

	if (fclose(fp) != 0)
	{
		if (error && ! *error) /* don't set error twice */
			set_file_error_from_errno(error, errno, "Failed to close file");
		success = FALSE;
	}

	return success;
}


typedef void Callback(GtkWidget *w, gpointer u);

/* run the command catenating cmd_cat if present */
static void build_command(GeanyDocument *doc, GeanyBuildGroup grp, guint cmd, gchar *cmd_cat)
{
	gchar *dir;
	gchar *full_command, *subs_command;
	GeanyBuildCommand *buildcmd = get_build_cmd(doc, grp, cmd, NULL);
	gchar *cmdstr;

	if (buildcmd == NULL)
		return;

	cmdstr = buildcmd->command;

	if (cmd_cat != NULL)
	{
		if (cmdstr != NULL)
			full_command = g_strconcat(cmdstr, cmd_cat, NULL);
		else
			full_command = g_strdup(cmd_cat);
	}
	else
		full_command = cmdstr;

	dir = build_replace_placeholder(doc, buildcmd->working_dir);
	subs_command = build_replace_placeholder(doc, full_command);
	build_info.grp = grp;
	build_info.cmd = cmd;
	build_spawn_cmd(doc, subs_command, dir);
	g_free(subs_command);
	g_free(dir);
	if (cmd_cat != NULL)
		g_free(full_command);
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

	SETPTR(build_info.custom_target, g_strdup(input));
	build_command(doc, GBO_TO_GBG(GEANY_GBO_CUSTOM), GBO_TO_CMD(GEANY_GBO_CUSTOM),
					build_info.custom_target);
}


static void on_build_menu_item(GtkWidget *w, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	GeanyBuildCommand *bc;
	guint grp = GPOINTER_TO_GRP(user_data);
	guint cmd = GPOINTER_TO_CMD(user_data);

	if (doc && doc->changed)
	{
		if (!document_save_file(doc, FALSE))
			return;
	}
	g_signal_emit_by_name(geany_object, "build-start");

	if (grp == GEANY_GBG_NON_FT && cmd == GBO_TO_CMD(GEANY_GBO_CUSTOM))
	{
		static GtkWidget *dialog = NULL; /* keep dialog for combo history */

		if (! dialog)
		{
			dialog = dialogs_show_input_persistent(_("Custom Text"), GTK_WINDOW(main_widgets.window),
				_("Enter custom text here, all entered text is appended to the command."),
				build_info.custom_target, &on_make_custom_input_response);
		}
		else
		{
			gtk_widget_show(dialog);
		}
		return;
	}
	else if (grp == GEANY_GBG_EXEC)
	{
		if (run_info[cmd].pid > (GPid) 1)
		{
			kill_process(&run_info[cmd].pid);
			return;
		}
		bc = get_build_cmd(doc, grp, cmd, NULL);
		if (bc != NULL && strcmp(bc->command, "builtin") == 0)
		{
			gchar *uri;
			if (doc == NULL)
				return;
			uri = g_strconcat("file:///", g_path_skip_root(doc->file_name), NULL);
			utils_open_browser(uri);
			g_free(uri);

		}
		else
			build_run_cmd(doc, cmd);
	}
	else
		build_command(doc, grp, cmd, NULL);
}


/* group codes for menu items other than the known commands
 * value order is important, see the following table for use */

/* the rest in each group */
#define MENU_FT_REST	 (GEANY_GBG_COUNT + GEANY_GBG_FT)
#define MENU_NON_FT_REST (GEANY_GBG_COUNT + GEANY_GBG_NON_FT)
#define MENU_EXEC_REST   (GEANY_GBG_COUNT + GEANY_GBG_EXEC)
/* the separator */
#define MENU_SEPARATOR   (2*GEANY_GBG_COUNT)
/* the fixed items */
#define MENU_NEXT_ERROR  (MENU_SEPARATOR + 1)
#define MENU_PREV_ERROR  (MENU_NEXT_ERROR + 1)
#define MENU_COMMANDS	(MENU_PREV_ERROR + 1)
#define MENU_DONE		(MENU_COMMANDS + 1)


static struct BuildMenuItemSpec {
	const gchar	*stock_id;
	const gint	 key_binding;
	const guint	 build_grp;
	const guint	 build_cmd;
	const gchar	*fix_label;
	Callback *cb;
} build_menu_specs[] = {
	{GTK_STOCK_CONVERT, GEANY_KEYS_BUILD_COMPILE, GBO_TO_GBG(GEANY_GBO_COMPILE),
		GBO_TO_CMD(GEANY_GBO_COMPILE), NULL, on_build_menu_item},
	{GEANY_STOCK_BUILD, GEANY_KEYS_BUILD_LINK, GBO_TO_GBG(GEANY_GBO_BUILD),
		GBO_TO_CMD(GEANY_GBO_BUILD), NULL, on_build_menu_item},
	{NULL, -1, MENU_FT_REST,
		GBO_TO_CMD(GEANY_GBO_BUILD) + 1, NULL, on_build_menu_item},
	{NULL, -1, MENU_SEPARATOR,
		GBF_SEP_1, NULL, NULL},
	{NULL, GEANY_KEYS_BUILD_MAKE, GBO_TO_GBG(GEANY_GBO_MAKE_ALL),
		GBO_TO_CMD(GEANY_GBO_MAKE_ALL), NULL, on_build_menu_item},
	{NULL, GEANY_KEYS_BUILD_MAKEOWNTARGET, GBO_TO_GBG(GEANY_GBO_CUSTOM),
		GBO_TO_CMD(GEANY_GBO_CUSTOM), NULL, on_build_menu_item},
	{NULL, GEANY_KEYS_BUILD_MAKEOBJECT, GBO_TO_GBG(GEANY_GBO_MAKE_OBJECT),
		GBO_TO_CMD(GEANY_GBO_MAKE_OBJECT), NULL, on_build_menu_item},
	{NULL, -1, MENU_NON_FT_REST,
		GBO_TO_CMD(GEANY_GBO_MAKE_OBJECT) + 1, NULL, on_build_menu_item},
	{NULL, -1, MENU_SEPARATOR,
		GBF_SEP_2, NULL, NULL},
	{GTK_STOCK_GO_DOWN, GEANY_KEYS_BUILD_NEXTERROR, MENU_NEXT_ERROR,
		GBF_NEXT_ERROR, N_("_Next Error"), on_build_next_error},
	{GTK_STOCK_GO_UP, GEANY_KEYS_BUILD_PREVIOUSERROR, MENU_PREV_ERROR,
		GBF_PREV_ERROR, N_("_Previous Error"), on_build_previous_error},
	{NULL, -1, MENU_SEPARATOR,
		GBF_SEP_3, NULL, NULL},
	{GTK_STOCK_EXECUTE, GEANY_KEYS_BUILD_RUN, GBO_TO_GBG(GEANY_GBO_EXEC),
		GBO_TO_CMD(GEANY_GBO_EXEC), NULL, on_build_menu_item},
	{NULL, -1, MENU_EXEC_REST,
		GBO_TO_CMD(GEANY_GBO_EXEC) + 1, NULL, on_build_menu_item},
	{NULL, -1, MENU_SEPARATOR,
		GBF_SEP_4, NULL, NULL},
	{GTK_STOCK_PREFERENCES, GEANY_KEYS_BUILD_OPTIONS, MENU_COMMANDS,
		GBF_COMMANDS, N_("_Set Build Commands"), on_set_build_commands_activate},
	{NULL, -1, MENU_DONE,
		0, NULL, NULL}
};


static void create_build_menu_item(GtkWidget *menu, GeanyKeyGroup *group, GtkAccelGroup *ag,
							struct BuildMenuItemSpec *bs, const gchar *lbl, guint grp, guint cmd)
{
	GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(lbl);

	if (bs->stock_id != NULL)
	{
		GtkWidget *image = gtk_image_new_from_stock(bs->stock_id, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	}
	gtk_widget_show(item);
	if (bs->key_binding >= 0)
		add_menu_accel(group, (guint) bs->key_binding, ag, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	if (bs->cb != NULL)
	{
		g_signal_connect(item, "activate", G_CALLBACK(bs->cb), GRP_CMD_TO_POINTER(grp,cmd));
	}
	menu_items.menu_item[grp][cmd] = item;
}


static void create_build_menu(BuildMenuItems *build_menu_items)
{
	GtkWidget *menu;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GeanyKeyGroup *keygroup = keybindings_get_core_group(GEANY_KEY_GROUP_BUILD);
	guint i, j;

	menu = gtk_menu_new();
	build_menu_items->menu_item[GEANY_GBG_FT] = g_new0(GtkWidget*, build_groups_count[GEANY_GBG_FT]);
	build_menu_items->menu_item[GEANY_GBG_NON_FT] = g_new0(GtkWidget*, build_groups_count[GEANY_GBG_NON_FT]);
	build_menu_items->menu_item[GEANY_GBG_EXEC] = g_new0(GtkWidget*, build_groups_count[GEANY_GBG_EXEC]);
	build_menu_items->menu_item[GBG_FIXED] = g_new0(GtkWidget*, GBF_COUNT);

	for (i = 0; build_menu_specs[i].build_grp != MENU_DONE; ++i)
	{
		struct BuildMenuItemSpec *bs = &(build_menu_specs[i]);
		if (bs->build_grp == MENU_SEPARATOR)
		{
			GtkWidget *item = gtk_separator_menu_item_new();
			gtk_widget_show(item);
			gtk_container_add(GTK_CONTAINER(menu), item);
			build_menu_items->menu_item[GBG_FIXED][bs->build_cmd] = item;
		}
		else if (bs->fix_label != NULL)
		{
			create_build_menu_item(menu, keygroup, accel_group, bs, _(bs->fix_label),
									GBG_FIXED, bs->build_cmd);
		}
		else if (bs->build_grp >= MENU_FT_REST && bs->build_grp <= MENU_SEPARATOR)
		{
			guint grp = bs->build_grp - GEANY_GBG_COUNT;
			for (j = bs->build_cmd; j < build_groups_count[grp]; ++j)
			{
				GeanyBuildCommand *bc = get_build_cmd(NULL, grp, j, NULL);
				const gchar *lbl = (bc == NULL) ? "" : bc->label;
				create_build_menu_item(menu, keygroup, accel_group, bs, lbl, grp, j);
			}
		}
		else
		{
			GeanyBuildCommand *bc = get_build_cmd(NULL, bs->build_grp, bs->build_cmd, NULL);
			const gchar *lbl = (bc == NULL) ? "" : bc->label;
			create_build_menu_item(menu, keygroup, accel_group, bs, lbl, bs->build_grp, bs->build_cmd);
		}
	}
	build_menu_items->menu = menu;
	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_build1")), menu);
}


/* portability to various GTK versions needs checking
 * conforms to description of gtk_accel_label as child of menu item
 * NB 2.16 adds set_label but not yet set_label_mnemonic */
static void geany_menu_item_set_label(GtkWidget *w, const gchar *label)
{
	GtkWidget *c = gtk_bin_get_child(GTK_BIN(w));

	gtk_label_set_text_with_mnemonic(GTK_LABEL(c), label);
}


/* * Update the build menu to reflect changes in configuration or status.
 *
 * Sets the labels and number of visible items to match the highest
 * priority configured commands.  Also sets sensitivity if build commands are
 * running and switches executes to stop when commands are running.
 *
 * @param doc The current document, if available, to save looking it up.
 *        If @c NULL it will be looked up.
 *
 * Call this after modifying any fields of a GeanyBuildCommand structure.
 *
 * @see Build Menu Configuration section of the Manual.
 *
 **/
void build_menu_update(GeanyDocument *doc)
{
	guint i, cmdcount, cmd, grp;
	gboolean vis = FALSE;
	gboolean have_path, build_running, exec_running, have_errors, cmd_sensitivity;
	gboolean can_compile, can_build, can_make, run_sensitivity = FALSE, run_running = FALSE;
	GeanyBuildCommand *bc;

	if (menu_items.menu == NULL)
		create_build_menu(&menu_items);
	if (doc == NULL)
		doc = document_get_current();
	have_path = doc != NULL && doc->file_name != NULL;
	build_running =  build_info.pid > (GPid) 1;
	have_errors = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(msgwindow.store_compiler), NULL) > 0;
	for (i = 0; build_menu_specs[i].build_grp != MENU_DONE; ++i)
	{
		struct BuildMenuItemSpec *bs = &(build_menu_specs[i]);
		switch (bs->build_grp)
		{
			case MENU_SEPARATOR:
				if (vis == TRUE)
				{
					gtk_widget_show_all(menu_items.menu_item[GBG_FIXED][bs->build_cmd]);
					vis = FALSE;
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
				if (bs->build_grp >= GEANY_GBG_COUNT)
				{
					grp = bs->build_grp - GEANY_GBG_COUNT;
					cmdcount = build_groups_count[grp];
				}
				else
				{
					grp = bs->build_grp;
					cmdcount = bs->build_cmd + 1;
				}
				for (cmd = bs->build_cmd; cmd < cmdcount; ++cmd)
				{
					GtkWidget *menu_item = menu_items.menu_item[grp][cmd];
					const gchar *label;
					bc = get_build_cmd(doc, grp, cmd, NULL);
					if (bc)
						label = bc->label;
					else
						label = NULL;

					if (grp < GEANY_GBG_EXEC)
					{
						cmd_sensitivity =
							(grp == GEANY_GBG_FT && bc != NULL && have_path && ! build_running) ||
							(grp == GEANY_GBG_NON_FT && bc != NULL && ! build_running);
						gtk_widget_set_sensitive(menu_item, cmd_sensitivity);
						if (bc != NULL && NZV(label))
						{
							geany_menu_item_set_label(menu_item, label);
							gtk_widget_show_all(menu_item);
							vis |= TRUE;
						}
						else
							gtk_widget_hide_all(menu_item);
					}
					else
					{
						GtkWidget *image;
						exec_running = run_info[cmd].pid > (GPid) 1;
						cmd_sensitivity = (bc != NULL) || exec_running;
						gtk_widget_set_sensitive(menu_item, cmd_sensitivity);
						if (cmd == GBO_TO_CMD(GEANY_GBO_EXEC))
							run_sensitivity = cmd_sensitivity;
						if (! exec_running)
						{
							image = gtk_image_new_from_stock(bs->stock_id, GTK_ICON_SIZE_MENU);
						}
						else
						{
							image = gtk_image_new_from_stock(GTK_STOCK_STOP, GTK_ICON_SIZE_MENU);
						}
						if (cmd == GBO_TO_CMD(GEANY_GBO_EXEC))
							run_running = exec_running;
						gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
						if (bc != NULL && NZV(label))
						{
							geany_menu_item_set_label(menu_item, label);
							gtk_widget_show_all(menu_item);
							vis |= TRUE;
						}
						else
							gtk_widget_hide_all(menu_item);
					}
				}
		}
	}

	run_sensitivity &= (doc != NULL);
	can_build = get_build_cmd(doc, GEANY_GBG_FT, GBO_TO_CMD(GEANY_GBO_BUILD), NULL) != NULL
					&& have_path && ! build_running;
	if (widgets.toolitem_build != NULL)
		gtk_widget_set_sensitive(widgets.toolitem_build, can_build);
	can_make = FALSE;
	if (widgets.toolitem_make_all != NULL)
		gtk_widget_set_sensitive(widgets.toolitem_make_all,
			(can_make |= get_build_cmd(doc, GEANY_GBG_FT, GBO_TO_CMD(GEANY_GBO_MAKE_ALL), NULL) != NULL
							&& ! build_running));
	if (widgets.toolitem_make_custom != NULL)
		gtk_widget_set_sensitive(widgets.toolitem_make_custom,
			(can_make |= get_build_cmd(doc, GEANY_GBG_FT, GBO_TO_CMD(GEANY_GBO_CUSTOM), NULL) != NULL
							&& ! build_running));
	if (widgets.toolitem_make_object != NULL)
		gtk_widget_set_sensitive(widgets.toolitem_make_object,
			(can_make |= get_build_cmd(doc, GEANY_GBG_FT, GBO_TO_CMD(GEANY_GBO_MAKE_OBJECT), NULL) != NULL
							&& ! build_running));
	if (widgets.toolitem_set_args != NULL)
		gtk_widget_set_sensitive(widgets.toolitem_set_args, TRUE);

	can_compile = get_build_cmd(doc, GEANY_GBG_FT, GBO_TO_CMD(GEANY_GBO_COMPILE), NULL) != NULL
					&& have_path && ! build_running;
	gtk_action_set_sensitive(widgets.compile_action, can_compile);
	gtk_action_set_sensitive(widgets.build_action, can_make);
	gtk_action_set_sensitive(widgets.run_action, run_sensitivity);

	/* show the stop command if a program is running from execute 0 , otherwise show run command */
	set_stop_button(run_running);

}


/* Call build_menu_update() instead of calling this directly. */
static void set_stop_button(gboolean stop)
{
	const gchar *button_stock_id = NULL;
	GtkToolButton *run_button;

	run_button = GTK_TOOL_BUTTON(toolbar_get_widget_by_name("Run"));
	if (run_button != NULL)
		button_stock_id = gtk_tool_button_get_stock_id(run_button);

	if (stop && utils_str_equal(button_stock_id, GTK_STOCK_STOP))
		return;
	if (! stop && utils_str_equal(button_stock_id, GTK_STOCK_EXECUTE))
		return;

	 /* use the run button also as stop button  */
	if (stop)
	{
		if (run_button != NULL)
			gtk_tool_button_set_stock_id(run_button, GTK_STOCK_STOP);
	}
	else
	{
		if (run_button != NULL)
			gtk_tool_button_set_stock_id(run_button, GTK_STOCK_EXECUTE);
	}
}


static void on_set_build_commands_activate(GtkWidget *w, gpointer u)
{
	/* For now, just show the project dialog */
	if (app->project)
		project_build_properties();
	else
		show_build_commands_dialog();
}


static void on_toolbutton_build_activate(GtkWidget *menuitem, gpointer user_data)
{
	last_toolbutton_action = user_data;
	g_object_set(widgets.build_action, "tooltip", _("Build the current file"), NULL);
	on_build_menu_item(menuitem, user_data);
}


static void on_toolbutton_make_activate(GtkWidget *menuitem, gpointer user_data)
{
	gchar *msg;

	last_toolbutton_action = user_data;
	if (last_toolbutton_action == GBO_TO_POINTER(GEANY_GBO_MAKE_ALL))
			msg = _("Build the current file with Make and the default target");
	else if (last_toolbutton_action == GBO_TO_POINTER(GEANY_GBO_CUSTOM))
			msg = _("Build the current file with Make and the specified target");
	else if (last_toolbutton_action == GBO_TO_POINTER(GEANY_GBO_MAKE_OBJECT))
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


static void on_build_next_error(GtkWidget *menuitem, gpointer user_data)
{
	if (ui_tree_view_find_next(GTK_TREE_VIEW(msgwindow.tree_compiler),
		msgwin_goto_compiler_file_line))
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
	}
	else
		ui_set_statusbar(FALSE, _("No more build errors."));
}


static void on_build_previous_error(GtkWidget *menuitem, gpointer user_data)
{
	if (ui_tree_view_find_previous(GTK_TREE_VIEW(msgwindow.tree_compiler),
		msgwin_goto_compiler_file_line))
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER);
	}
	else
		ui_set_statusbar(FALSE, _("No more build errors."));
}


void build_toolbutton_build_clicked(GtkAction *action, gpointer unused)
{
	if (last_toolbutton_action == GBO_TO_POINTER(GEANY_GBO_BUILD))
	{
		on_build_menu_item(NULL, GBO_TO_POINTER(GEANY_GBO_BUILD));
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
typedef struct RowWidgets
{
	GtkWidget *entries[GEANY_BC_CMDENTRIES_COUNT];
	GeanyBuildSource src;
	GeanyBuildSource dst;
	GeanyBuildCommand *cmdsrc;
	guint grp;
	guint cmd;
	gboolean cleared;
	gboolean used_dst;
} RowWidgets;

static GdkColor *insensitive_color;

static void set_row_color(RowWidgets *r, GdkColor *color )
{
	enum GeanyBuildCmdEntries i;

	for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
		gtk_widget_modify_text(r->entries[i], GTK_STATE_NORMAL, color);
}


static void set_build_command_entry_text(GtkWidget *wid, const gchar *text)
{
	if (GTK_IS_BUTTON(wid))
		gtk_button_set_label(GTK_BUTTON(wid), text);
	else
		gtk_entry_set_text(GTK_ENTRY(wid), text);
}


static void on_clear_dialog_row(GtkWidget *unused, gpointer user_data)
{
	RowWidgets *r = user_data;
	guint src;
	enum GeanyBuildCmdEntries i;
	GeanyBuildCommand *bc = get_next_build_cmd(NULL, r->grp, r->cmd, r->dst, &src);

	if (bc != NULL)
	{
		r->cmdsrc = bc;
		r->src = src;
		for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
		{
			set_build_command_entry_text(r->entries[i],
				id_to_str(bc,i) != NULL ? id_to_str(bc,i) : "");
		}
	}
	else
	{
		r->cmdsrc = NULL;
		for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
		{
			set_build_command_entry_text(r->entries[i], "");
		}
	}
	r->used_dst = FALSE;
	set_row_color(r, insensitive_color);
	r->cleared = TRUE;
}


static void on_clear_dialog_regex_row(GtkEntry *regex, gpointer unused)
{
	gtk_entry_set_text(regex,"");
}


static void on_label_button_clicked(GtkWidget *wid, gpointer user_data)
{
	RowWidgets *r = user_data;
	GtkWidget *top_level = gtk_widget_get_toplevel(wid);
	const gchar *old = gtk_button_get_label(GTK_BUTTON(wid));
	gchar *str;

	if (GTK_WIDGET_TOPLEVEL(top_level) && GTK_IS_WINDOW(top_level))
		str = dialogs_show_input(_("Set menu item label"), GTK_WINDOW(top_level), NULL, old);
	else
		str = dialogs_show_input(_("Set menu item label"), NULL, NULL, old);

	if (!str)
		return;

	gtk_button_set_label(GTK_BUTTON(wid), str);
	g_free(str);
	r->used_dst = TRUE;
	set_row_color(r, NULL);
}


static void on_entry_focus(GtkWidget *wid, GdkEventFocus *unused, gpointer user_data)
{
	RowWidgets *r = user_data;

	r->used_dst = TRUE;
	set_row_color(r, NULL);
}


/* Column headings, array NULL-terminated */
static const gchar *colheads[] =
{
	"#",
	N_("Label"),
	N_("Command"),
	N_("Working directory"),
	N_("Reset"),
	NULL
};

/* column names */
#define DC_ITEM 0
#define DC_ENTRIES 1
#define DC_CLEAR 4
#define DC_N_COL 5

static const guint entry_x_padding = 3;
static const guint entry_y_padding = 0;


static RowWidgets *build_add_dialog_row(GeanyDocument *doc, GtkTable *table, guint row,
				GeanyBuildSource dst, guint grp, guint cmd, gboolean dir)
{
	GtkWidget *label, *clear, *clearicon;
	RowWidgets *roww;
	GeanyBuildCommand *bc;
	guint src;
	enum GeanyBuildCmdEntries i;
	guint column = 0;
	gchar *text;

	text = g_strdup_printf("%d.", cmd + 1);
	label = gtk_label_new(text);
	g_free(text);
	insensitive_color = &(gtk_widget_get_style(label)->text[GTK_STATE_INSENSITIVE]);
	gtk_table_attach(table, label, column, column + 1, row, row + 1, GTK_FILL,
		GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	roww = g_new0(RowWidgets, 1);
	roww->src = GEANY_BCS_COUNT;
	roww->grp = grp;
	roww->cmd = cmd;
	roww->dst = dst;
	for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
	{
		gint xflags = (i == GEANY_BC_COMMAND) ? GTK_FILL | GTK_EXPAND : GTK_FILL;

		column += 1;
		if (i == GEANY_BC_LABEL)
		{
			GtkWidget *wid = roww->entries[i] = gtk_button_new();
			gtk_button_set_use_underline(GTK_BUTTON(wid), TRUE);
			gtk_widget_set_tooltip_text(wid, _("Click to set menu item label"));
			g_signal_connect(wid, "clicked", G_CALLBACK(on_label_button_clicked), roww);
		}
		else
		{
			roww->entries[i] = gtk_entry_new();
			g_signal_connect(roww->entries[i], "focus-in-event", G_CALLBACK(on_entry_focus), roww);
		}
		gtk_table_attach(table, roww->entries[i], column, column + 1, row, row + 1, xflags,
			GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	}
	column++;
	clearicon = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
	clear = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(clear), clearicon);
	g_signal_connect(clear, "clicked", G_CALLBACK(on_clear_dialog_row), roww);
	gtk_table_attach(table, clear, column, column + 1, row, row + 1, GTK_FILL,
		GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	roww->cmdsrc = bc = get_build_cmd(doc, grp, cmd, &src);
	if (bc != NULL)
		roww->src = src;

	for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
	{
		const gchar *str = "";

		if (bc != NULL )
		{
			if ((str = id_to_str(bc, i)) == NULL)
				str = "";
			else if (dst == src)
				roww->used_dst = TRUE;
		}
		set_build_command_entry_text(roww->entries[i], str);
	}
	if (bc != NULL && (dst > src))
		set_row_color(roww, insensitive_color);
	if (bc != NULL && (src > dst || (grp == GEANY_GBG_FT && (doc == NULL || doc->file_type == NULL))))
	{
		for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
			gtk_widget_set_sensitive(roww->entries[i], FALSE);
		gtk_widget_set_sensitive(clear, FALSE);
	}
	return roww;
}


typedef struct BuildTableFields
{
	RowWidgets 	**rows;
	GtkWidget	 *fileregex;
	GtkWidget	 *nonfileregex;
	gchar		**fileregexstring;
	gchar		**nonfileregexstring;
} BuildTableFields;


GtkWidget *build_commands_table(GeanyDocument *doc, GeanyBuildSource dst, BuildTableData *table_data,
								GeanyFiletype *ft)
{
	GtkWidget *label, *sep, *clearicon, *clear;
	BuildTableFields *fields;
	GtkTable *table;
	const gchar **ch;
	gchar *txt;
	guint col, row, cmdindex;
	guint cmd;
	guint src;
	gboolean sensitivity;
	guint sep_padding = entry_y_padding + 3;

	table = GTK_TABLE(gtk_table_new(build_items_count + 12, 5, FALSE));
	fields = g_new0(BuildTableFields, 1);
	fields->rows = g_new0(RowWidgets*, build_items_count);
	for (ch = colheads, col = 0; *ch != NULL; ch++, col++)
	{
		label = gtk_label_new(_(*ch));
		gtk_table_attach(table, label, col, col + 1, 0, 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	}
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, DC_N_COL, 1, 2, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, sep_padding);
	if (ft != NULL && ft->id != GEANY_FILETYPES_NONE)
		txt = g_strdup_printf(_("%s commands"), ft->name);
	else
		txt = g_strdup_printf(_("%s commands"), _("No filetype"));

	label = ui_label_new_bold(txt);
	g_free(txt);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, DC_N_COL, 2, 3, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, entry_y_padding);
	for (row = 3, cmdindex = 0, cmd = 0; cmd < build_groups_count[GEANY_GBG_FT]; ++row, ++cmdindex, ++cmd)
		fields->rows[cmdindex] = build_add_dialog_row(doc, table, row, dst, GEANY_GBG_FT, cmd, FALSE);
	label = gtk_label_new(_("Error regular expression:"));
	gtk_table_attach(table, label, 0, DC_ENTRIES + 1, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, entry_y_padding);
	fields->fileregex = gtk_entry_new();
	fields->fileregexstring = build_get_regex(GEANY_GBG_FT, NULL, &src);
	sensitivity = (ft == NULL) ? FALSE : TRUE;
	if (fields->fileregexstring != NULL && *(fields->fileregexstring) != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(fields->fileregex), *(fields->fileregexstring));
		if (src > dst)
			sensitivity = FALSE;
	}
	gtk_table_attach(table, fields->fileregex, DC_ENTRIES + 1, DC_CLEAR, row, row + 1, GTK_FILL,
		GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	clearicon = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
	clear = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(clear), clearicon);
	g_signal_connect_swapped(clear, "clicked",
		G_CALLBACK(on_clear_dialog_regex_row), (fields->fileregex));
	gtk_table_attach(table, clear, DC_CLEAR, DC_CLEAR + 1, row, row + 1, GTK_FILL,
		GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	gtk_widget_set_sensitive(fields->fileregex, sensitivity);
	gtk_widget_set_sensitive(clear, sensitivity);
	++row;
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, DC_N_COL, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, sep_padding);
	++row;
	label = ui_label_new_bold(_("Independent commands"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, DC_N_COL, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, entry_y_padding);
	for (++row, cmd = 0; cmd < build_groups_count[GEANY_GBG_NON_FT]; ++row, ++cmdindex, ++cmd)
		fields->rows[cmdindex] = build_add_dialog_row(
			doc, table, row, dst, GEANY_GBG_NON_FT, cmd, TRUE);
	label = gtk_label_new(_("Error regular expression:"));
	gtk_table_attach(table, label, 0, DC_ENTRIES + 1, row, row + 1, GTK_FILL,
		GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	fields->nonfileregex = gtk_entry_new();
	fields->nonfileregexstring = build_get_regex(GEANY_GBG_NON_FT, NULL, &src);
	sensitivity = TRUE;
	if (fields->nonfileregexstring != NULL && *(fields->nonfileregexstring) != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(fields->nonfileregex), *(fields->nonfileregexstring));
		sensitivity = src > dst ? FALSE : TRUE;
	}
	gtk_table_attach(table, fields->nonfileregex, DC_ENTRIES + 1, DC_CLEAR, row, row + 1, GTK_FILL,
		GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	clearicon = gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU);
	clear = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(clear), clearicon);
	g_signal_connect_swapped(clear, "clicked",
		G_CALLBACK(on_clear_dialog_regex_row), (fields->nonfileregex));
	gtk_table_attach(table, clear, DC_CLEAR, DC_CLEAR + 1, row, row + 1, GTK_FILL,
		GTK_FILL | GTK_EXPAND, entry_x_padding, entry_y_padding);
	gtk_widget_set_sensitive(fields->nonfileregex, sensitivity);
	gtk_widget_set_sensitive(clear, sensitivity);
	++row;
	label = gtk_label_new(NULL);
	ui_label_set_markup(GTK_LABEL(label), "<i>%s</i>",
		_("Note: Item 2 opens a dialog and appends the response to the command."));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, DC_N_COL, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, entry_y_padding);
	++row;
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, DC_N_COL, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, sep_padding);
	++row;
	label = ui_label_new_bold(_("Execute commands"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, DC_N_COL, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, entry_y_padding);
	for (++row, cmd = 0; cmd < build_groups_count[GEANY_GBG_EXEC]; ++row, ++cmdindex, ++cmd)
		fields->rows[cmdindex] = build_add_dialog_row(doc, table, row, dst, GEANY_GBG_EXEC, cmd, TRUE);
	sep = gtk_hseparator_new();
	gtk_table_attach(table, sep, 0, DC_N_COL, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, sep_padding);
	++row;
	label = gtk_label_new(NULL);
	ui_label_set_markup(GTK_LABEL(label), "<i>%s</i>",
		_("%d, %e, %f, %p are substituted in command and directory fields, see manual for details."));
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_table_attach(table, label, 0, DC_N_COL, row, row + 1, GTK_FILL, GTK_FILL | GTK_EXPAND,
		entry_x_padding, entry_y_padding);
	/*printf("%d extra rows in dialog\n", row-build_items_count);*/
	++row;
	*table_data = fields;
	return GTK_WIDGET(table);
}


void build_free_fields(BuildTableData table_data)
{
	guint cmdindex;

	for (cmdindex = 0; cmdindex < build_items_count; ++cmdindex)
		g_free(table_data->rows[cmdindex]);
	g_free(table_data->rows);
	g_free(table_data);
}


/* string compare where null pointers match null or 0 length strings */
#if 0
static gint stcmp(const gchar *a, const gchar *b)
{
	if (a == NULL && b == NULL)
		return 0;
	if (a == NULL && b != NULL)
		return strlen(b);
	if (a != NULL && b == NULL)
		return strlen(a);
	return strcmp(a, b);
}
#endif


static const gchar *get_build_command_entry_text(GtkWidget *wid)
{
	if (GTK_IS_BUTTON(wid))
		return gtk_button_get_label(GTK_BUTTON(wid));
	else
		return gtk_entry_get_text(GTK_ENTRY(wid));
}


static gboolean read_row(BuildDestination *dst, BuildTableData table_data, guint drow, guint grp, guint cmd)
{
	gchar *entries[GEANY_BC_CMDENTRIES_COUNT];
	gboolean changed = FALSE;
	enum GeanyBuildCmdEntries i;

	for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
	{
		entries[i] = g_strdup(get_build_command_entry_text(table_data->rows[drow]->entries[i]));
	}
	if (table_data->rows[drow]->cleared)
	{
		if (dst->dst[grp] != NULL)
		{
			if (*(dst->dst[grp]) == NULL)
				*(dst->dst[grp]) = g_new0(GeanyBuildCommand, build_groups_count[grp]);
			(*(dst->dst[grp]))[cmd].exists = FALSE;
			(*(dst->dst[grp]))[cmd].changed = TRUE;
			changed = TRUE;
		}
	}
	if (table_data->rows[drow]->used_dst == TRUE)
	{
		if (dst->dst[grp] != NULL)
		{
			if (*(dst->dst[grp]) == NULL)
				*(dst->dst[grp]) = g_new0(GeanyBuildCommand, build_groups_count[grp]);
			for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
				set_command(&(*(dst->dst[grp]))[cmd], i, entries[i]);
			(*(dst->dst[grp]))[cmd].exists = TRUE;
			(*(dst->dst[grp]))[cmd].changed = TRUE;
			changed = TRUE;
		}
	}
	else
	{
		for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
			g_free(entries[i]);
	}
	return changed;
}


static gboolean read_regex(GtkWidget *regexentry, gchar **src, gchar **dst)
{
	gboolean changed = FALSE;
	const gchar *reg = gtk_entry_get_text(GTK_ENTRY(regexentry));

	if (((src == NULL			/* originally there was no regex */
		|| *src == NULL)		/* or it was NULL*/
		&& NZV(reg))			/* and something was typed */
		|| (src != NULL			/* originally there was a regex*/
		&& (*src == NULL 		/* and either it was NULL */
		|| strcmp(*src, reg) != 0)))	/* or it has been changed */
	{
		if (dst != NULL)
		{
			SETPTR(*dst, g_strdup(reg));
			changed = TRUE;
		}
	}
	return changed;
}


static gboolean build_read_commands(BuildDestination *dst, BuildTableData table_data, gint response)
{
	guint cmdindex, cmd;
	gboolean changed = FALSE;

	if (response == GTK_RESPONSE_ACCEPT)
	{
		for (cmdindex = 0, cmd = 0; cmd < build_groups_count[GEANY_GBG_FT]; ++cmdindex, ++cmd)
			changed |= read_row(dst, table_data, cmdindex, GEANY_GBG_FT, cmd);
		for (cmd = 0; cmd < build_groups_count[GEANY_GBG_NON_FT]; ++cmdindex, ++cmd)
			changed |= read_row(dst, table_data, cmdindex, GEANY_GBG_NON_FT, cmd);
		for (cmd = 0; cmd < build_groups_count[GEANY_GBG_EXEC]; ++cmdindex, ++cmd)
			changed |= read_row(dst, table_data, cmdindex, GEANY_GBG_EXEC, cmd);
		changed |= read_regex(table_data->fileregex, table_data->fileregexstring, dst->fileregexstr);
		changed |= read_regex(table_data->nonfileregex, table_data->nonfileregexstring, dst->nonfileregexstr);
	}
	return changed;
}


void build_read_project(GeanyFiletype *ft, BuildTableData build_properties)
{
	BuildDestination menu_dst;

	if (ft != NULL)
	{
		menu_dst.dst[GEANY_GBG_FT] = &(ft->projfilecmds);
		menu_dst.fileregexstr = &(ft->projerror_regex_string);
	}
	else
	{
		menu_dst.dst[GEANY_GBG_FT] = NULL;
		menu_dst.fileregexstr = NULL;
	}
	menu_dst.dst[GEANY_GBG_NON_FT] = &non_ft_proj;
	menu_dst.dst[GEANY_GBG_EXEC] = &exec_proj;
	menu_dst.nonfileregexstr = &regex_proj;

	build_read_commands(&menu_dst, build_properties, GTK_RESPONSE_ACCEPT);
}


static void show_build_commands_dialog(void)
{
	GtkWidget *dialog, *table, *vbox;
	GeanyDocument *doc = document_get_current();
	GeanyFiletype *ft = NULL;
	const gchar *title = _("Set Build Commands");
	gint response;
	BuildTableData table_data;
	BuildDestination prefdsts;

	if (doc != NULL)
		ft = doc->file_type;
	dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(main_widgets.window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	table = build_commands_table(doc, GEANY_BCS_PREF, &table_data, ft);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
	gtk_widget_show_all(dialog);
	/* run modally to prevent user changing idx filetype */
	response = gtk_dialog_run(GTK_DIALOG(dialog));

	prefdsts.dst[GEANY_GBG_NON_FT] = &non_ft_pref;
	if (ft != NULL)
	{
		prefdsts.dst[GEANY_GBG_FT] = &(ft->homefilecmds);
		prefdsts.fileregexstr = &(ft->homeerror_regex_string);
		prefdsts.dst[GEANY_GBG_EXEC] = &(ft->homeexeccmds);
	}
	else
	{
		prefdsts.dst[GEANY_GBG_FT] = NULL;
		prefdsts.fileregexstr = NULL;
		prefdsts.dst[GEANY_GBG_EXEC] = NULL;
	}
	prefdsts.nonfileregexstr = &regex_pref;
	if (build_read_commands(&prefdsts, table_data, response) && ft != NULL)
		filetypes_save_commands(ft);
	build_free_fields(table_data);

	build_menu_update(doc);
	gtk_widget_destroy(dialog);
}


/* Creates the relevant build menu if necessary. */
BuildMenuItems *build_get_menu_items(gint filetype_idx)
{
	BuildMenuItems *items;

	items = &menu_items;
	if (items->menu == NULL)
		create_build_menu(items);
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
 *       xx = LB for label, CM for command and WD for working dir */
static const gchar *groups[GEANY_GBG_COUNT] = { "FT", "NF", "EX" };
static const gchar *fixedkey="xx_xx_xx";

#define set_key_grp(key, grp) (key[prefixlen + 0] = grp[0], key[prefixlen + 1] = grp[1])
#define set_key_cmd(key, cmd) (key[prefixlen + 3] = cmd[0], key[prefixlen + 4] = cmd[1])
#define set_key_fld(key, fld) (key[prefixlen + 6] = fld[0], key[prefixlen + 7] = fld[1])

static void build_load_menu_grp(GKeyFile *config, GeanyBuildCommand **dst, gint grp,
								gchar *prefix, gboolean loc)
{
	guint cmd;
	gsize prefixlen; /* NOTE prefixlen used in macros above */
	GeanyBuildCommand *dstcmd;
	gchar *key;
	static gchar cmdbuf[3] = "  ";

	if (*dst == NULL)
		*dst = g_new0(GeanyBuildCommand, build_groups_count[grp]);
	dstcmd = *dst;
	prefixlen = prefix == NULL ? 0 : strlen(prefix);
	key = g_strconcat(prefix == NULL ? "" : prefix, fixedkey, NULL);
	for (cmd = 0; cmd < build_groups_count[grp]; ++cmd)
	{
		gchar *label;
		if (cmd >= 100)
			return; /* ensure no buffer overflow */
		sprintf(cmdbuf, "%02d", cmd);
		set_key_grp(key, groups[grp]);
		set_key_cmd(key, cmdbuf);
		set_key_fld(key, "LB");
		if (loc)
			label = g_key_file_get_locale_string(config, build_grp_name, key, NULL, NULL);
		else
			label = g_key_file_get_string(config, build_grp_name, key, NULL);
		if (label != NULL)
		{
			dstcmd[cmd].exists = TRUE;
			SETPTR(dstcmd[cmd].label, label);
			set_key_fld(key,"CM");
			SETPTR(dstcmd[cmd].command,
					g_key_file_get_string(config, build_grp_name, key, NULL));
			set_key_fld(key,"WD");
			SETPTR(dstcmd[cmd].working_dir,
					g_key_file_get_string(config, build_grp_name, key, NULL));
		}
		else dstcmd[cmd].exists = FALSE;
	}
	g_free(key);
}


/* for the specified source load new format build menu items or try to make some sense of
 * old format setings, not done perfectly but better than ignoring them */
void build_load_menu(GKeyFile *config, GeanyBuildSource src, gpointer p)
{
	GeanyFiletype *ft;
	GeanyProject *pj;
	gchar **ftlist;
	gchar *value, *basedir, *makebasedir;
	gboolean bvalue = FALSE;

	if (g_key_file_has_group(config, build_grp_name))
	{
		switch (src)
		{
			case GEANY_BCS_FT:
				ft = (GeanyFiletype*)p;
				if (ft == NULL)
					return;
				build_load_menu_grp(config, &(ft->filecmds), GEANY_GBG_FT, NULL, TRUE);
				build_load_menu_grp(config, &(ft->ftdefcmds), GEANY_GBG_NON_FT, NULL, TRUE);
				build_load_menu_grp(config, &(ft->execcmds), GEANY_GBG_EXEC, NULL, TRUE);
				SETPTR(ft->error_regex_string,
						g_key_file_get_string(config, build_grp_name, "error_regex", NULL));
				break;
			case GEANY_BCS_HOME_FT:
				ft = (GeanyFiletype*)p;
				if (ft == NULL)
					return;
				build_load_menu_grp(config, &(ft->homefilecmds), GEANY_GBG_FT, NULL, FALSE);
				build_load_menu_grp(config, &(ft->homeexeccmds), GEANY_GBG_EXEC, NULL, FALSE);
				SETPTR(ft->homeerror_regex_string,
						g_key_file_get_string(config, build_grp_name, "error_regex", NULL));
				break;
			case GEANY_BCS_PREF:
				build_load_menu_grp(config, &non_ft_pref, GEANY_GBG_NON_FT, NULL, FALSE);
				build_load_menu_grp(config, &exec_pref, GEANY_GBG_EXEC, NULL, FALSE);
				SETPTR(regex_pref, g_key_file_get_string(config, build_grp_name, "error_regex", NULL));
				break;
			case GEANY_BCS_PROJ:
				build_load_menu_grp(config, &non_ft_proj, GEANY_GBG_NON_FT, NULL, FALSE);
				build_load_menu_grp(config, &exec_proj, GEANY_GBG_EXEC, NULL, FALSE);
				SETPTR(regex_proj, g_key_file_get_string(config, build_grp_name, "error_regex", NULL));
				pj = (GeanyProject*)p;
				if (p == NULL)
					return;
				ftlist = g_key_file_get_string_list(config, build_grp_name, "filetypes", NULL, NULL);
				if (ftlist != NULL)
				{
					gchar **ftname;
					if (pj->build_filetypes_list == NULL)
						pj->build_filetypes_list = g_ptr_array_new();
					g_ptr_array_set_size(pj->build_filetypes_list, 0);
					for (ftname = ftlist; *ftname != NULL; ++ftname)
					{
						ft = filetypes_lookup_by_name(*ftname);
						if (ft != NULL)
						{
							gchar *regkey = g_strdup_printf("%serror_regex", *ftname);
							g_ptr_array_add(pj->build_filetypes_list, ft);
							SETPTR(ft->projerror_regex_string,
									g_key_file_get_string(config, build_grp_name, regkey, NULL));
							g_free(regkey);
							build_load_menu_grp(config, &(ft->projfilecmds), GEANY_GBG_FT, *ftname, FALSE);
							build_load_menu_grp(config, &(ft->projexeccmds), GEANY_GBG_EXEC, *ftname, FALSE);
						}
					}
					g_free(ftlist);
				}
				break;
			default: /* defaults don't load from config, see build_init */
				break;
		}
	}

	/* load old [build_settings] values if there is no value defined by [build-menu] */

	/* set GeanyBuildCommand if it doesn't already exist and there is a command */
/* TODO: rewrite as function */
#define ASSIGNIF(type, id, string, value) \
	if (NZV(value) && ! type[GBO_TO_CMD(id)].exists) { \
		type[GBO_TO_CMD(id)].exists = TRUE; \
		SETPTR(type[GBO_TO_CMD(id)].label, g_strdup(string)); \
		SETPTR(type[GBO_TO_CMD(id)].command, (value)); \
		SETPTR(type[GBO_TO_CMD(id)].working_dir, NULL); \
		type[GBO_TO_CMD(id)].old = TRUE; \
	} else \
		g_free(value);

	switch (src)
	{
		case GEANY_BCS_FT:
			ft = (GeanyFiletype*)p;
			value = g_key_file_get_string(config, "build_settings", "compiler", NULL);
			if (value != NULL)
			{
				if (ft->filecmds == NULL)
					ft->filecmds = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_FT]);
				ASSIGNIF(ft->filecmds, GEANY_GBO_COMPILE, _("_Compile"), value);
			}
			value = g_key_file_get_string(config, "build_settings", "linker", NULL);
			if (value != NULL)
			{
				if (ft->filecmds == NULL)
					ft->filecmds = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_FT]);
				ASSIGNIF(ft->filecmds, GEANY_GBO_BUILD, _("_Build"), value);
			}
			value = g_key_file_get_string(config, "build_settings", "run_cmd", NULL);
			if (value != NULL)
			{
				if (ft->execcmds == NULL)
					ft->execcmds = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_EXEC]);
				ASSIGNIF(ft->execcmds, GEANY_GBO_EXEC, _("_Execute"), value);
			}
			if (ft->error_regex_string == NULL)
				ft->error_regex_string = g_key_file_get_string(config, "build_settings", "error_regex", NULL);
			break;
		case GEANY_BCS_PROJ:
			if (non_ft_pref == NULL)
				non_ft_pref = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_NON_FT]);
			basedir = project_get_base_path();
			if (basedir == NULL)
				basedir = g_strdup("%d");
			bvalue = g_key_file_get_boolean(config, "project", "make_in_base_path", NULL);
			if (bvalue)
				makebasedir = g_strdup(basedir);
			else
				makebasedir = g_strdup("%d");
			if (non_ft_pref[GBO_TO_CMD(GEANY_GBO_MAKE_ALL)].old)
				SETPTR(non_ft_pref[GBO_TO_CMD(GEANY_GBO_MAKE_ALL)].working_dir, g_strdup(makebasedir));
			if (non_ft_pref[GBO_TO_CMD(GEANY_GBO_CUSTOM)].old)
				SETPTR(non_ft_pref[GBO_TO_CMD(GEANY_GBO_CUSTOM)].working_dir, g_strdup(makebasedir));
			if (non_ft_pref[GBO_TO_CMD(GEANY_GBO_MAKE_OBJECT)].old)
				SETPTR(non_ft_pref[GBO_TO_CMD(GEANY_GBO_MAKE_OBJECT)].working_dir, g_strdup("%d"));
			value = g_key_file_get_string(config, "project", "run_cmd", NULL);
			if (NZV(value))
			{
				if (exec_proj == NULL)
					exec_proj = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_EXEC]);
				if (! exec_proj[GBO_TO_CMD(GEANY_GBO_EXEC)].exists)
				{
					exec_proj[GBO_TO_CMD(GEANY_GBO_EXEC)].exists = TRUE;
					SETPTR(exec_proj[GBO_TO_CMD(GEANY_GBO_EXEC)].label, g_strdup(_("_Execute")));
					SETPTR(exec_proj[GBO_TO_CMD(GEANY_GBO_EXEC)].command, value);
					SETPTR(exec_proj[GBO_TO_CMD(GEANY_GBO_EXEC)].working_dir, g_strdup(basedir));
					exec_proj[GBO_TO_CMD(GEANY_GBO_EXEC)].old = TRUE;
				}
			}
			g_free(makebasedir);
			g_free(basedir);
			break;
		case GEANY_BCS_PREF:
			value = g_key_file_get_string(config, "tools", "make_cmd", NULL);
			if (value != NULL)
			{
				if (non_ft_pref == NULL)
					non_ft_pref = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_NON_FT]);
				ASSIGNIF(non_ft_pref, GEANY_GBO_CUSTOM, _("Make Custom _Target"),
						g_strdup_printf("%s ", value));
				ASSIGNIF(non_ft_pref, GEANY_GBO_MAKE_OBJECT, _("Make _Object"),
						g_strdup_printf("%s %%e.o",value));
				ASSIGNIF(non_ft_pref, GEANY_GBO_MAKE_ALL, _("_Make"), value);
			}
			break;
		default:
			break;
	}
}


static guint build_save_menu_grp(GKeyFile *config, GeanyBuildCommand *src, gint grp, gchar *prefix)
{
	guint cmd;
	gsize prefixlen; /* NOTE prefixlen used in macros above */
	gchar *key;
	guint count = 0;
	enum GeanyBuildCmdEntries i;

	if (src == NULL)
		return 0;
	prefixlen = prefix == NULL ? 0 : strlen(prefix);
	key = g_strconcat(prefix == NULL ? "" : prefix, fixedkey, NULL);
	for (cmd = 0; cmd < build_groups_count[grp]; ++cmd)
	{
		if (src[cmd].exists) ++count;
		if (src[cmd].changed)
		{
			static gchar cmdbuf[4] = "   ";
			if (cmd >= 100)
				return count; /* ensure no buffer overflow */
			sprintf(cmdbuf, "%02d", cmd);
			set_key_grp(key, groups[grp]);
			set_key_cmd(key, cmdbuf);
			if (src[cmd].exists)
			{
				for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
				{
					set_key_fld(key, config_keys[i]);
					g_key_file_set_string(config, build_grp_name, key, id_to_str(&src[cmd], i));
				}
			}
			else
			{
				for (i = 0; i < GEANY_BC_CMDENTRIES_COUNT; i++)
				{
					set_key_fld(key, config_keys[i]);
					g_key_file_remove_key(config, build_grp_name, key, NULL);
				}
			}
		}
	}
	g_free(key);
	return count;
}


typedef struct ForEachData
{
	GKeyFile *config;
	GPtrArray *ft_names;
} ForEachData;


static void foreach_project_filetype(gpointer data, gpointer user_data)
{
	GeanyFiletype *ft = data;
	ForEachData *d = user_data;
	guint i = 0;
	gchar *regkey = g_strdup_printf("%serror_regex", ft->name);

	i += build_save_menu_grp(d->config, ft->projfilecmds, GEANY_GBG_FT, ft->name);
	i += build_save_menu_grp(d->config, ft->projexeccmds, GEANY_GBG_EXEC, ft->name);
	if (NZV(ft->projerror_regex_string))
	{
		g_key_file_set_string(d->config, build_grp_name, regkey, ft->projerror_regex_string);
		i++;
	}
	else
		g_key_file_remove_key(d->config, build_grp_name, regkey, NULL);
	g_free(regkey);
	if (i > 0)
		g_ptr_array_add(d->ft_names, ft->name);
}


/* TODO: untyped ptr is too ugly (also for build_load_menu) */
void build_save_menu(GKeyFile *config, gpointer ptr, GeanyBuildSource src)
{
	GeanyFiletype *ft;
	GeanyProject *pj;
	ForEachData data;

	switch (src)
	{
		case GEANY_BCS_HOME_FT:
			ft = (GeanyFiletype*)ptr;
			if (ft == NULL)
				return;
			build_save_menu_grp(config, ft->homefilecmds, GEANY_GBG_FT, NULL);
			build_save_menu_grp(config, ft->homeexeccmds, GEANY_GBG_EXEC, NULL);
			if (NZV(ft->homeerror_regex_string))
				g_key_file_set_string(config, build_grp_name, "error_regex", ft->homeerror_regex_string);
			else
				g_key_file_remove_key(config, build_grp_name, "error_regex", NULL);
			break;
		case GEANY_BCS_PREF:
			build_save_menu_grp(config, non_ft_pref, GEANY_GBG_NON_FT, NULL);
			build_save_menu_grp(config, exec_pref, GEANY_GBG_EXEC, NULL);
			if (NZV(regex_pref))
				g_key_file_set_string(config, build_grp_name, "error_regex", regex_pref);
			else
				g_key_file_remove_key(config, build_grp_name, "error_regex", NULL);
			break;
		case GEANY_BCS_PROJ:
			pj = (GeanyProject*)ptr;
			build_save_menu_grp(config, non_ft_proj, GEANY_GBG_NON_FT, NULL);
			build_save_menu_grp(config, exec_proj, GEANY_GBG_EXEC, NULL);
			if (NZV(regex_proj))
				g_key_file_set_string(config, build_grp_name, "error_regex", regex_proj);
			else
				g_key_file_remove_key(config, build_grp_name, "error_regex", NULL);
			if (pj->build_filetypes_list != NULL)
			{
				data.config = config;
				data.ft_names = g_ptr_array_new();
				g_ptr_array_foreach(pj->build_filetypes_list, foreach_project_filetype, (gpointer)(&data));
				if (data.ft_names->pdata != NULL)
					g_key_file_set_string_list(config, build_grp_name, "filetypes",
								(const gchar**)(data.ft_names->pdata), data.ft_names->len);
				else
					g_key_file_remove_key(config, build_grp_name, "filetypes", NULL);
				g_ptr_array_free(data.ft_names, TRUE);
			}
			break;
		default: /* defaults and GEANY_BCS_FT can't save */
			break;
	}
}


/* FIXME: count is int only because calling code doesn't handle checking its value itself */
void build_set_group_count(GeanyBuildGroup grp, gint count)
{
	guint i, sum;

	g_return_if_fail(count >= 0);

	if ((guint) count > build_groups_count[grp])
		build_groups_count[grp] = (guint) count;
	for (i = 0, sum = 0; i < GEANY_GBG_COUNT; ++i)
		sum += build_groups_count[i];
	build_items_count = sum;
}


/** Get the count of commands for the group
 *
 * Get the number of commands in the group specified by @a grp.
 *
 * @param grp the group of the specified menu item.
 *
 * @return a count of the number of commands in the group
 *
 **/

guint build_get_group_count(const GeanyBuildGroup grp)
{
	g_return_val_if_fail(grp < GEANY_GBG_COUNT, 0);
	return build_groups_count[grp];
}


static void on_project_close(void)
{
	/* remove project regexen */
	SETPTR(regex_proj, NULL);
}


static struct
{
	const gchar *label;
	const gchar *command;
	const gchar *working_dir;
	GeanyBuildCommand **ptr;
	gint index;
} default_cmds[] = {
	{ N_("_Make"), "make", NULL, &non_ft_def, GBO_TO_CMD(GEANY_GBO_MAKE_ALL)},
	{ N_("Make Custom _Target"), "make ", NULL, &non_ft_def, GBO_TO_CMD(GEANY_GBO_CUSTOM)},
	{ N_("Make _Object"), "make %e.o", NULL, &non_ft_def, GBO_TO_CMD(GEANY_GBO_MAKE_OBJECT)},
	{ N_("_Execute"), "./%e", NULL, &exec_def, GBO_TO_CMD(GEANY_GBO_EXEC)},
	{ NULL, NULL, NULL, NULL, 0 }
};


void build_init(void)
{
	GtkWidget *item;
	GtkWidget *toolmenu;
	gint cmdindex;

	g_signal_connect(geany_object, "project-close", on_project_close, NULL);

	ft_def = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_FT]);
	non_ft_def = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_NON_FT]);
	exec_def = g_new0(GeanyBuildCommand, build_groups_count[GEANY_GBG_EXEC]);
	run_info = g_new0(RunInfo, build_groups_count[GEANY_GBG_EXEC]);

	for (cmdindex = 0; default_cmds[cmdindex].command != NULL; ++cmdindex)
	{
		GeanyBuildCommand *cmd = &((*(default_cmds[cmdindex].ptr))[ default_cmds[cmdindex].index ]);
		cmd->exists = TRUE;
		cmd->label = g_strdup(_(default_cmds[cmdindex].label));
		cmd->command = g_strdup(default_cmds[cmdindex].command);
		cmd->working_dir = g_strdup(default_cmds[cmdindex].working_dir);
	}

	/* create the toolbar Build item sub menu */
	toolmenu = gtk_menu_new();
	g_object_ref(toolmenu);

	/* build the code */
	item = ui_image_menu_item_new(GEANY_STOCK_BUILD, _("_Build"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(toolmenu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_build_activate),
		GBO_TO_POINTER(GEANY_GBO_BUILD));
	widgets.toolitem_build = item;

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(toolmenu), item);

	/* build the code with make all */
	item = gtk_image_menu_item_new_with_mnemonic(_("_Make All"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(toolmenu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_make_activate),
		GBO_TO_POINTER(GEANY_GBO_MAKE_ALL));
	widgets.toolitem_make_all = item;

	/* build the code with make custom */
	item = gtk_image_menu_item_new_with_mnemonic(_("Make Custom _Target"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(toolmenu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_make_activate),
		GBO_TO_POINTER(GEANY_GBO_CUSTOM));
	widgets.toolitem_make_custom = item;

	/* build the code with make object */
	item = gtk_image_menu_item_new_with_mnemonic(_("Make _Object"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(toolmenu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_toolbutton_make_activate),
		GBO_TO_POINTER(GEANY_GBO_MAKE_OBJECT));
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

	/* get toolbar action pointers */
	widgets.build_action = toolbar_get_action_by_name("Build");
	widgets.compile_action = toolbar_get_action_by_name("Compile");
	widgets.run_action = toolbar_get_action_by_name("Run");
	widgets.toolmenu = toolmenu;
	/* set the submenu to the toolbar item */
	geany_menu_button_action_set_menu(GEANY_MENU_BUTTON_ACTION(widgets.build_action), toolmenu);
}
