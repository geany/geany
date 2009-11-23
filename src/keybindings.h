/*
 *      keybindings.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

/**
 * @file keybindings.h
 * Configurable keyboard shortcuts.
 **/


#ifndef GEANY_KEYBINDINGS_H
#define GEANY_KEYBINDINGS_H 1


/** Function pointer type used for keybinding callbacks. */
typedef void (*GeanyKeyCallback) (guint key_id);

/** Represents a single keybinding action.
 * Use keybindings_set_item() to set. */
typedef struct GeanyKeyBinding
{
	guint key;				/**< Key value in lower-case, such as @c GDK_a or 0 */
	GdkModifierType mods;	/**< Modifier keys, such as @c GDK_CONTROL_MASK or 0 */
	gchar *name;			/**< Key name for the configuration file, such as @c "menu_new" */
	/** Label used in the preferences dialog keybindings tab.
	 * May contain underscores - these won't be displayed. */
	gchar *label;
	/** Function called when the key combination is pressed, or @c NULL to use the group callback
	 * (preferred). @see plugin_set_key_group(). */
	GeanyKeyCallback callback;
	GtkWidget *menu_item;	/**< Optional widget to set an accelerator for, or @c NULL */
}
GeanyKeyBinding;


/** Function pointer type used for keybinding group callbacks.
 * You should return @c TRUE to indicate handling the callback. (Occasionally, if the keybinding
 * cannot apply in the current situation, it is useful to return @c FALSE to allow a later keybinding
 * with the same key combination to handle it). */
typedef gboolean (*GeanyKeyGroupCallback) (guint key_id);

/** A collection of keybindings grouped together. */
typedef struct GeanyKeyGroup GeanyKeyGroup;

/* Plugins should not set these fields. */
#ifdef GEANY_PRIVATE
struct GeanyKeyGroup
{
	const gchar *name;		/* Group name used in the configuration file, such as @c "html_chars" */
	const gchar *label;		/* Group label used in the preferences dialog keybindings tab */
	gsize count;			/* number of keybindings the group holds */
	GeanyKeyBinding *keys;	/* array of GeanyKeyBinding structs */
	gboolean plugin;		/* used by plugin */
	GeanyKeyGroupCallback callback;	/* use this or individual keybinding callbacks */
};
#endif


extern GPtrArray *keybinding_groups;	/* array of GeanyKeyGroup pointers */

extern const gchar keybindings_keyfile_group_name[];


/* Note: we don't need to increment the plugin ABI when appending keybindings or keygroups,
 * just make sure to only insert keybindings/groups immediately before the _COUNT item, so
 * the existing enum values stay the same.
 * The _COUNT item should not be used by plugins, as it may well change. */

/** Keybinding group IDs */
enum
{
	GEANY_KEY_GROUP_FILE,
	GEANY_KEY_GROUP_PROJECT,
	GEANY_KEY_GROUP_EDITOR,
	GEANY_KEY_GROUP_CLIPBOARD,
	GEANY_KEY_GROUP_SELECT,
	GEANY_KEY_GROUP_FORMAT,
	GEANY_KEY_GROUP_INSERT,
	GEANY_KEY_GROUP_SETTINGS,
	GEANY_KEY_GROUP_SEARCH,
	GEANY_KEY_GROUP_GOTO,
	GEANY_KEY_GROUP_VIEW,
	GEANY_KEY_GROUP_FOCUS,
	GEANY_KEY_GROUP_NOTEBOOK,
	GEANY_KEY_GROUP_DOCUMENT,
	GEANY_KEY_GROUP_BUILD,
	GEANY_KEY_GROUP_TOOLS,
	GEANY_KEY_GROUP_HELP,
	GEANY_KEY_GROUP_COUNT
};

/** File group keybinding command IDs */
enum
{
	GEANY_KEYS_FILE_NEW,
	GEANY_KEYS_FILE_OPEN,
	GEANY_KEYS_FILE_OPENSELECTED,
	GEANY_KEYS_FILE_SAVE,
	GEANY_KEYS_FILE_SAVEAS,
	GEANY_KEYS_FILE_SAVEALL,
	GEANY_KEYS_FILE_PRINT,
	GEANY_KEYS_FILE_CLOSE,
	GEANY_KEYS_FILE_CLOSEALL,
	GEANY_KEYS_FILE_RELOAD,
	GEANY_KEYS_FILE_COUNT
};

/** Project group keybinding command IDs */
enum
{
	GEANY_KEYS_PROJECT_PROPERTIES,
	GEANY_KEYS_PROJECT_COUNT
};

/** Editor group keybinding command IDs */
enum
{
	GEANY_KEYS_EDITOR_UNDO,
	GEANY_KEYS_EDITOR_REDO,
	GEANY_KEYS_EDITOR_DELETELINE,
	GEANY_KEYS_EDITOR_DUPLICATELINE,
	GEANY_KEYS_EDITOR_TRANSPOSELINE,
	GEANY_KEYS_EDITOR_SCROLLTOLINE,
	GEANY_KEYS_EDITOR_SCROLLLINEUP,
	GEANY_KEYS_EDITOR_SCROLLLINEDOWN,
	GEANY_KEYS_EDITOR_COMPLETESNIPPET,
	GEANY_KEYS_EDITOR_SUPPRESSSNIPPETCOMPLETION,
	GEANY_KEYS_EDITOR_SNIPPETNEXTCURSOR,
	GEANY_KEYS_EDITOR_CONTEXTACTION,
	GEANY_KEYS_EDITOR_AUTOCOMPLETE,
	GEANY_KEYS_EDITOR_CALLTIP,
	GEANY_KEYS_EDITOR_MACROLIST,
	GEANY_KEYS_EDITOR_DELETELINETOEND,
	GEANY_KEYS_EDITOR_WORDPARTCOMPLETION,
	GEANY_KEYS_EDITOR_MOVELINEUP,
	GEANY_KEYS_EDITOR_MOVELINEDOWN,
	GEANY_KEYS_EDITOR_COUNT
};

/** Clipboard group keybinding command IDs */
enum
{
	GEANY_KEYS_CLIPBOARD_CUT,
	GEANY_KEYS_CLIPBOARD_COPY,
	GEANY_KEYS_CLIPBOARD_PASTE,
	GEANY_KEYS_CLIPBOARD_CUTLINE,
	GEANY_KEYS_CLIPBOARD_COPYLINE,
	GEANY_KEYS_CLIPBOARD_COUNT
};

/** Select group keybinding command IDs */
enum
{

	GEANY_KEYS_SELECT_ALL,
	GEANY_KEYS_SELECT_WORD,
	GEANY_KEYS_SELECT_LINE,
	GEANY_KEYS_SELECT_PARAGRAPH,
	GEANY_KEYS_SELECT_WORDPARTLEFT,
	GEANY_KEYS_SELECT_WORDPARTRIGHT,
	GEANY_KEYS_SELECT_COUNT
};

/** Format group keybinding command IDs */
enum
{
	GEANY_KEYS_FORMAT_TOGGLECASE,
	GEANY_KEYS_FORMAT_COMMENTLINETOGGLE,
	GEANY_KEYS_FORMAT_COMMENTLINE,
	GEANY_KEYS_FORMAT_UNCOMMENTLINE,
	GEANY_KEYS_FORMAT_INCREASEINDENT,
	GEANY_KEYS_FORMAT_DECREASEINDENT,
	GEANY_KEYS_FORMAT_INCREASEINDENTBYSPACE,
	GEANY_KEYS_FORMAT_DECREASEINDENTBYSPACE,
	GEANY_KEYS_FORMAT_AUTOINDENT,
	GEANY_KEYS_FORMAT_SENDTOCMD1,
	GEANY_KEYS_FORMAT_SENDTOCMD2,
	GEANY_KEYS_FORMAT_SENDTOCMD3,
	GEANY_KEYS_FORMAT_SENDTOVTE,
	GEANY_KEYS_FORMAT_REFLOWPARAGRAPH,
	GEANY_KEYS_FORMAT_COUNT
};

/** Insert group keybinding command IDs */
enum
{
	GEANY_KEYS_INSERT_DATE,
	GEANY_KEYS_INSERT_ALTWHITESPACE,
	GEANY_KEYS_INSERT_COUNT
};

/** Settings group keybinding command IDs */
enum
{
	GEANY_KEYS_SETTINGS_PREFERENCES,
	GEANY_KEYS_SETTINGS_PLUGINPREFERENCES,
	GEANY_KEYS_SETTINGS_COUNT
};

/** Search group keybinding command IDs */
enum
{
	GEANY_KEYS_SEARCH_FIND,
	GEANY_KEYS_SEARCH_FINDNEXT,
	GEANY_KEYS_SEARCH_FINDPREVIOUS,
	GEANY_KEYS_SEARCH_FINDINFILES,
	GEANY_KEYS_SEARCH_REPLACE,
	GEANY_KEYS_SEARCH_FINDNEXTSEL,
	GEANY_KEYS_SEARCH_FINDPREVSEL,
	GEANY_KEYS_SEARCH_NEXTMESSAGE,
	GEANY_KEYS_SEARCH_PREVIOUSMESSAGE,
	GEANY_KEYS_SEARCH_FINDUSAGE,
	GEANY_KEYS_SEARCH_FINDDOCUMENTUSAGE,
	GEANY_KEYS_SEARCH_MARKALL,
	GEANY_KEYS_SEARCH_COUNT
};

/** Go To group keybinding command IDs */
enum
{
	GEANY_KEYS_GOTO_FORWARD,
	GEANY_KEYS_GOTO_BACK,
	GEANY_KEYS_GOTO_LINE,
	GEANY_KEYS_GOTO_LINESTART,
	GEANY_KEYS_GOTO_LINEEND,
	GEANY_KEYS_GOTO_MATCHINGBRACE,
	GEANY_KEYS_GOTO_TOGGLEMARKER,
	GEANY_KEYS_GOTO_NEXTMARKER,
	GEANY_KEYS_GOTO_PREVIOUSMARKER,
	GEANY_KEYS_GOTO_PREVWORDPART,
	GEANY_KEYS_GOTO_NEXTWORDPART,
	GEANY_KEYS_GOTO_TAGDEFINITION,
	GEANY_KEYS_GOTO_TAGDECLARATION,
	GEANY_KEYS_GOTO_LINEENDVISUAL,
	GEANY_KEYS_GOTO_COUNT
};

/** View group keybinding command IDs */
enum
{
	GEANY_KEYS_VIEW_TOGGLEALL,
	GEANY_KEYS_VIEW_FULLSCREEN,
	GEANY_KEYS_VIEW_MESSAGEWINDOW,
	GEANY_KEYS_VIEW_SIDEBAR,
	GEANY_KEYS_VIEW_ZOOMIN,
	GEANY_KEYS_VIEW_ZOOMOUT,
	GEANY_KEYS_VIEW_COUNT
};

/** Focus group keybinding command IDs */
enum
{
	GEANY_KEYS_FOCUS_EDITOR,
	GEANY_KEYS_FOCUS_SCRIBBLE,
	GEANY_KEYS_FOCUS_VTE,
	GEANY_KEYS_FOCUS_SEARCHBAR,
	GEANY_KEYS_FOCUS_SIDEBAR,
	GEANY_KEYS_FOCUS_COMPILER,
	GEANY_KEYS_FOCUS_MESSAGES,
	GEANY_KEYS_FOCUS_COUNT
};

/** Notebook Tab group keybinding command IDs */
enum
{
	GEANY_KEYS_NOTEBOOK_SWITCHTABLEFT,
	GEANY_KEYS_NOTEBOOK_SWITCHTABRIGHT,
	GEANY_KEYS_NOTEBOOK_SWITCHTABLASTUSED,
	GEANY_KEYS_NOTEBOOK_MOVETABLEFT,
	GEANY_KEYS_NOTEBOOK_MOVETABRIGHT,
	GEANY_KEYS_NOTEBOOK_MOVETABFIRST,
	GEANY_KEYS_NOTEBOOK_MOVETABLAST,
	GEANY_KEYS_NOTEBOOK_COUNT
};

/** Document group keybinding command IDs */
enum
{
	GEANY_KEYS_DOCUMENT_REPLACETABS,
	GEANY_KEYS_DOCUMENT_REPLACESPACES,
	GEANY_KEYS_DOCUMENT_TOGGLEFOLD,
	GEANY_KEYS_DOCUMENT_FOLDALL,
	GEANY_KEYS_DOCUMENT_UNFOLDALL,
	GEANY_KEYS_DOCUMENT_RELOADTAGLIST,
	GEANY_KEYS_DOCUMENT_LINEWRAP,
	GEANY_KEYS_DOCUMENT_LINEBREAK,
	GEANY_KEYS_DOCUMENT_COUNT
};

/** Build group keybinding command IDs */
enum
{
	GEANY_KEYS_BUILD_COMPILE,
	GEANY_KEYS_BUILD_LINK,
	GEANY_KEYS_BUILD_MAKE,
	GEANY_KEYS_BUILD_MAKEOWNTARGET,
	GEANY_KEYS_BUILD_MAKEOBJECT,
	GEANY_KEYS_BUILD_NEXTERROR,
	GEANY_KEYS_BUILD_PREVIOUSERROR,
	GEANY_KEYS_BUILD_RUN,
	GEANY_KEYS_BUILD_OPTIONS,
	GEANY_KEYS_BUILD_COUNT
};

/** Tools group keybinding command IDs */
enum
{
	GEANY_KEYS_TOOLS_OPENCOLORCHOOSER,
	GEANY_KEYS_TOOLS_COUNT
};

/** Help group keybinding command IDs */
enum
{
	GEANY_KEYS_HELP_HELP,
	GEANY_KEYS_HELP_COUNT
};


void keybindings_init(void);

void keybindings_load_keyfile(void);

void keybindings_free(void);

GeanyKeyGroup *keybindings_set_group(GeanyKeyGroup *group, const gchar *section_name,
		const gchar *label, gsize count, GeanyKeyGroupCallback callback) G_GNUC_WARN_UNUSED_RESULT;

void keybindings_free_group(GeanyKeyGroup *group);

GeanyKeyBinding *keybindings_set_item(GeanyKeyGroup *group, gsize key_id,
		GeanyKeyCallback callback, guint key, GdkModifierType mod,
		gchar *name, gchar *label, GtkWidget *menu_item);

GeanyKeyBinding *keybindings_get_item(GeanyKeyGroup *group, gsize key_id);

gchar *keybindings_get_label(GeanyKeyBinding *kb);

void keybindings_update_combo(GeanyKeyBinding *kb, guint key, GdkModifierType mods);

void keybindings_send_command(guint group_id, guint key_id);

GeanyKeyBinding *keybindings_lookup_item(guint group_id, guint key_id);

/* just write the content of the keys array to the config file */
void keybindings_write_to_file(void);

void keybindings_show_shortcuts(void);

const GeanyKeyBinding *keybindings_check_event(GdkEventKey *ev, gint *group_id, gint *binding_id);

#endif

