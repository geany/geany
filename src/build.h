/*
 *      build.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#ifndef GEANY_BUILD_H
#define GEANY_BUILD_H 1

#define GEANY_BUILD_ERR_HIGHLIGHT_MAX 100

/* Geany Known Build Commands, currently only these can have keybindings
 * Order is important (see GBO_TO_GBG, GBO_TO_CMD below) */
typedef enum
{
	GBO_COMPILE,
	GBO_BUILD,
	GBO_MAKE_ALL,
	GBO_MAKE_CUSTOM,
	GBO_MAKE_OBJECT,
	GBO_EXEC,
	GBO_COUNT	/* count of how many */
} GeanyBuildType;

typedef enum	/* build command groups, order as above */
{
	GBG_FT,		/* filetype */
	GBG_NON_FT,	/* non filetype */
	GBG_EXEC,	/* execute */
	GBG_COUNT	/* count of how many */
} GeanyBuildGroup;

/* include the fixed widgets in an array indexed by groups */
#define GBG_FIXED GBG_COUNT

/* convert GBO_xxx to GBG_xxx and command number
 * Note they are macros so they can be used in static initialisers */
#define GBO_TO_GBG(gbo) ((gbo)>GBO_EXEC?GBG_COUNT:((gbo)>=GBO_EXEC?GBG_EXEC:((gbo)>=GBO_MAKE_ALL?GBG_NON_FT:GBG_FT)))
#define GBO_TO_CMD(gbo) ((gbo)>=GBO_COUNT?(gbo)-GBO_COUNT:((gbo)>=GBO_EXEC?(gbo)-GBO_EXEC:((gbo)>=GBO_MAKE_ALL?(gbo)-GBO_MAKE_ALL:(gbo))))

enum GeanyBuildFixedMenuItems
{
	GBF_NEXT_ERROR,
	GBF_PREV_ERROR,
	GBF_COMMANDS,
	GBF_SEP_1,
	GBF_SEP_2,
	GBF_SEP_3,
	GBF_SEP_4,
	GBF_COUNT
};

typedef enum	/* build command sources, in increasing priority */
{
	BCS_DEF,	/* default */
	BCS_FT,		/* filetype */
	BCS_HOME_FT,/* filetypes in home */
	BCS_PREF,	/* preferences */
	BCS_PROJ,	/* project */
	BCS_COUNT	/* count of how many */
} GeanyBuildSource;

typedef struct GeanyBuildInfo
{
	GeanyBuildGroup	 grp;
	gint			 cmd;
	GPid			 pid;	/* process id of the spawned process */
	gchar			*dir;
	guint			 file_type_id;
	gchar			*custom_target;
	gint			 message_count;
} GeanyBuildInfo;

extern GeanyBuildInfo build_info;

typedef enum  GeanyBuildCmdEntries
{
    BC_LABEL,
    BC_COMMAND,
    BC_CMDENTRIES_COUNT,
} GeanyBuildCmdEntries;

typedef struct GeanyBuildCommand
{
	gchar *entries[BC_CMDENTRIES_COUNT];
	gboolean	 exists;
	gboolean	 run_in_base_dir;
	gboolean	 changed;
} GeanyBuildCommand;

extern GeanyBuildCommand *non_ft_proj, *exec_proj; /* project command array pointers */
extern gchar *regex_proj; /* project non-fileregex string */

typedef struct BuildMenuItems
{
	GtkWidget		*menu;
	GtkWidget		**menu_item[GBG_COUNT+1];  /* +1 for fixed items */
} BuildMenuItems;

/* a structure defining the destinations for a set of groups of commands & regex */
typedef struct BuildDestination
{
	GeanyBuildCommand	**dst[GBG_COUNT];
	gchar				**fileregexstr;
	gchar				**nonfileregexstr;
} BuildDestination;

/* opaque pointers returned from build functions and passed back to them */
typedef struct TableFields *TableData;

void build_init(void);

void build_finalize(void);

/* menu configuration dialog functions */
GtkWidget *build_commands_table( GeanyDocument *doc, GeanyBuildSource dst, TableData *data, GeanyFiletype *ft );

gboolean read_build_commands( BuildDestination *dst, TableData data, gint response );

void free_build_data( TableData data );

/* build response decode assistance function */
gboolean build_parse_make_dir(const gchar *string, gchar **prefix);

/* build menu functions */
void build_menu_update(GeanyDocument *doc);

BuildMenuItems *build_get_menu_items(gint filetype_idx);

void build_toolbutton_build_clicked(GtkAction *action, gpointer user_data);

void remove_command( GeanyBuildSource src, GeanyBuildGroup grp, gint cmd );

/* load and store menu configuration */
void load_build_menu( GKeyFile *config, GeanyBuildSource dst, gpointer ptr );

void save_build_menu( GKeyFile *config, gpointer ptr, GeanyBuildSource src );

void set_build_grp_count( GeanyBuildGroup grp, guint count );

gchar **get_build_regex(GeanyBuildGroup grp, GeanyFiletype *ft, gint *from);

#endif
