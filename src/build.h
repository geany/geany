/*
 *      build.h - this file is part of Geany, a fast and lightweight IDE
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

/** @file build.h Interface to the Build menu functionality. */

#ifndef GEANY_BUILD_H
#define GEANY_BUILD_H 1

#include "document.h"
#include "filetypes.h"

#include "gtkcompat.h"

G_BEGIN_DECLS

/** Groups of Build menu items. */
typedef enum
{
	GEANY_GBG_FT,		/**< filetype items */
	GEANY_GBG_NON_FT,	/**< non filetype items.*/
	GEANY_GBG_EXEC,		/**< execute items */
	GEANY_GBG_COUNT		/**< count of groups. */
} GeanyBuildGroup;

/** Build menu item sources in increasing priority */
typedef enum
{
	GEANY_BCS_DEF,		/**< Default values. */
	GEANY_BCS_FT,		/**< System filetype values. */
	GEANY_BCS_HOME_FT,	/**< Filetypes in ~/.config/geany/filedefs */
	GEANY_BCS_PREF,		/**< Preferences file ~/.config/geany/geany.conf */
	GEANY_BCS_PROJ_FT,	/**< Project file filetype command */
	GEANY_BCS_PROJ,		/**< Project file if open. */
	GEANY_BCS_COUNT		/**< Count of sources. */
} GeanyBuildSource;

/** The entries of a command for a menu item */
typedef enum GeanyBuildCmdEntries
{
	GEANY_BC_LABEL,				/**< The menu item label, _ marks mnemonic */
	GEANY_BC_COMMAND,			/**< The command to run. */
	GEANY_BC_WORKING_DIR,		/**< The directory to run in */
	GEANY_BC_CMDENTRIES_COUNT	/**< Count of entries */
} GeanyBuildCmdEntries;

void build_activate_menu_item(const GeanyBuildGroup grp, const guint cmd);

const gchar *build_get_current_menu_item(const GeanyBuildGroup grp, const guint cmd,
                                         const GeanyBuildCmdEntries field);

void build_remove_menu_item(const GeanyBuildSource src, const GeanyBuildGroup grp, const gint cmd);

void build_set_menu_item(const GeanyBuildSource src, const GeanyBuildGroup grp,
                         const guint cmd, const GeanyBuildCmdEntries field, const gchar *value);

guint build_get_group_count(const GeanyBuildGroup grp);


#ifdef GEANY_PRIVATE

/* include the fixed widgets in an array indexed by groups */
#define GBG_FIXED GEANY_GBG_COUNT

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

typedef struct GeanyBuildInfo
{
	GeanyBuildGroup	 grp;
	guint			 cmd;
	GPid			 pid;	/* process id of the spawned process */
	gchar			*dir;
	guint			 file_type_id;
	gchar			*custom_target;
	gint			 message_count;
} GeanyBuildInfo;

extern GeanyBuildInfo build_info;

/* * The command for a menu item. */
typedef struct GeanyBuildCommand
{
	/* * Pointers to g_string values of the command entries.
	 * Must be freed if the pointer is changed. */
	gchar 		*label;						/* *< Menu item label */
	gchar		*command;					/* *< Command to run */
	gchar		*working_dir;				/* *< working directory */
	gboolean	 exists;					/* *< If the entries have valid values. */
	gboolean	 changed;					/* *< Save on exit if @c changed, remove if not @c exist. */
	gboolean	 old;						/* *< Converted from old format. */
} GeanyBuildCommand;

typedef struct BuildMenuItems
{
	GtkWidget		*menu;
	GtkWidget		**menu_item[GEANY_GBG_COUNT + 1];  /* +1 for fixed items */
} BuildMenuItems;

/* a structure defining the destinations for a set of groups of commands & regex */
typedef struct BuildDestination
{
	GeanyBuildCommand	**dst[GEANY_GBG_COUNT];
	gchar				**fileregexstr;
	gchar				**nonfileregexstr;
} BuildDestination;

/* opaque pointers returned from build functions and passed back to them */
typedef struct BuildTableFields *BuildTableData;


void build_init(void);

void build_finalize(void);

/* menu configuration dialog functions */
GtkWidget *build_commands_table(GeanyDocument *doc, GeanyBuildSource dst, BuildTableData *data, GeanyFiletype *ft);

void build_read_project(GeanyFiletype *ft, BuildTableData build_properties);

void build_free_fields(BuildTableData data);

/* build response decode assistance function */
gboolean build_parse_make_dir(const gchar *string, gchar **prefix);

/* build menu functions */

void build_menu_update(GeanyDocument *doc);

void build_toolbutton_build_clicked(GtkAction *action, gpointer user_data);

GeanyBuildCommand *build_get_menu_item(const GeanyBuildSource src, const GeanyBuildGroup grp, const guint cmd);

BuildMenuItems *build_get_menu_items(gint filetype_idx);

/* load and store menu configuration */
void build_load_menu(GKeyFile *config, GeanyBuildSource dst, gpointer ptr);

void build_save_menu(GKeyFile *config, gpointer ptr, GeanyBuildSource src);

void build_set_group_count(GeanyBuildGroup grp, gint count);

gchar **build_get_regex(GeanyBuildGroup grp, GeanyFiletype *ft, guint *from);

gboolean build_keybinding(guint key_id);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_BUILD_H */
