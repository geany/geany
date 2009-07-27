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

/** @file build.h Interface to the Build menu functionality. */

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

/** Groups of Build menu items. */
typedef enum
{
	GBG_FT,		/**< filetype items */
	GBG_NON_FT,	/**< non filetype items.*/
	GBG_EXEC,	/**< execute items */
	GBG_COUNT	/**< count of groups. */
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

/** Build menu item sources in increasing priority */
typedef enum
{
	BCS_DEF,	/**< Default values. */
	BCS_FT,		/**< System filetype values. */
	BCS_HOME_FT,/**< Filetypes in ~/.config/geany/filedefs */
	BCS_PREF,	/**< Preferences file ~/.config/geany/geany.conf */
	BCS_PROJ,	/**< Project file if open. */
	BCS_COUNT	/**< Count of sources. */
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

/** The entries of a command for a menu item */
typedef enum  GeanyBuildCmdEntries
{
    BC_LABEL,				/**< The menu item label, _ marks mnemonic */
    BC_COMMAND,				/**< The command to run. */
    BC_WORKING_DIR,			/**< The directory to run in */
    BC_CMDENTRIES_COUNT,	/**< Count of entries */
} GeanyBuildCmdEntries;

/** The command for a menu item. */
typedef struct GeanyBuildCommand
{
 	/** Pointers to g_string values values of the command entries.
	 * Must be freed if the pointer is changed. */
	gchar *entries[BC_CMDENTRIES_COUNT];
	gboolean	 exists;					/**< If the entries have valid values. */
	gboolean	 changed;					/**< Save on exit if @c changed, remove if not @c exist. */
	gboolean	 old;						/**< Converted from old format. */
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

void free_build_fields( TableData data );

void set_build_non_ft_wd_to_proj(TableData table_data);

/* build response decode assistance function */
gboolean build_parse_make_dir(const gchar *string, gchar **prefix);

/* build menu functions */

/** Update the build menu to reflect changes in configuration or status.
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

void build_menu_update(GeanyDocument *doc);


void build_toolbutton_build_clicked(GtkAction *action, gpointer user_data);

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
 * @see build_menu_update
 **/

void build_remove_menu_item(GeanyBuildSource src, GeanyBuildGroup grp, gint cmd);

/** Get the @a GeanyBuildCommand structure for the specified Build menu item.
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

GeanyBuildCommand *build_get_menu_item(GeanyBuildSource src, GeanyBuildGroup grp, gint cmd);

/** Get the @a GeanyBuildCommand structure for the menu item.
 *
 * Get the current highest priority command specified by @a grp and @a cmd.  This is the one
 * that the menu item will use if activated.
 *
 * @param grp the group of the specified menu item.
 * @param cmd the index of the command within the group.
 * @param src pointer to @a gint to return which source provided the command. Ignored if @a NULL.
 *        Values are one of @a GeanyBuildSource but returns a signed type not the enum.
 *
 * @return a pointer to the @a GeanyBuildCommand structure or @a NULL if it doesn't exist.
 *         This is a pointer to an internal structure and must not be freed.
 *
 * @see build_menu_update
 **/

GeanyBuildCommand *build_get_current_menu_item(GeanyBuildGroup grp, gint cmd, gint *src);

BuildMenuItems *build_get_menu_items(gint filetype_idx);

/* load and store menu configuration */
void load_build_menu( GKeyFile *config, GeanyBuildSource dst, gpointer ptr);

void save_build_menu( GKeyFile *config, gpointer ptr, GeanyBuildSource src);

void set_build_grp_count(GeanyBuildGroup grp, gint count);

gint get_build_group_count(GeanyBuildGroup grp);

gchar **get_build_regex(GeanyBuildGroup grp, GeanyFiletype *ft, gint *from);

#endif
