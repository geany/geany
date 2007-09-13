/*
 *      keybindings.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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


#ifndef GEANY_KEYBINDINGS_H
#define GEANY_KEYBINDINGS_H 1

typedef void (*KBCallback) (guint key_id);

// holds all user-definable key bindings
typedef struct binding
{
	guint key;
	GdkModifierType mods;
	// at the moment only needed as keys for the configuration file because indices or translatable
	// strings as keys are not very useful
	const gchar *name;
	const gchar *label;
	// function pointer to a callback function, just to keep the code in keypress event
	// callback function clear
	KBCallback cb_func;
	// string to use as a section name in the preferences dialog in keybinding treeview as well as
	// in the keybinding help dialog, set only for the first binding in the section
	gchar *section;
} binding;


typedef enum
{
	GEANY_KEYS_MENU_NEW = 0,
	GEANY_KEYS_MENU_OPEN,
	GEANY_KEYS_MENU_OPENSELECTED,
	GEANY_KEYS_MENU_SAVE,
	GEANY_KEYS_MENU_SAVEAS,
	GEANY_KEYS_MENU_SAVEALL,
	GEANY_KEYS_MENU_PRINT,
	GEANY_KEYS_MENU_CLOSE,
	GEANY_KEYS_MENU_CLOSEALL,
	GEANY_KEYS_MENU_RELOADFILE,
	GEANY_KEYS_MENU_PROJECTPROPERTIES,

	GEANY_KEYS_MENU_UNDO,
	GEANY_KEYS_MENU_REDO,
	GEANY_KEYS_MENU_SELECTALL,
	GEANY_KEYS_MENU_INSERTDATE,
	GEANY_KEYS_MENU_PREFERENCES,

	GEANY_KEYS_MENU_FIND,
	GEANY_KEYS_MENU_FINDNEXT,
	GEANY_KEYS_MENU_FINDPREVIOUS,
	GEANY_KEYS_MENU_FINDINFILES,
	GEANY_KEYS_MENU_REPLACE,
	GEANY_KEYS_MENU_FINDNEXTSEL,
	GEANY_KEYS_MENU_FINDPREVSEL,
	GEANY_KEYS_MENU_NEXTMESSAGE,
	GEANY_KEYS_MENU_GOTOLINE,

	GEANY_KEYS_MENU_TOGGLEALL,
	GEANY_KEYS_MENU_FULLSCREEN,
	GEANY_KEYS_MENU_MESSAGEWINDOW,
	GEANY_KEYS_MENU_SIDEBAR,
	GEANY_KEYS_MENU_ZOOMIN,
	GEANY_KEYS_MENU_ZOOMOUT,

	GEANY_KEYS_MENU_REPLACETABS,
	GEANY_KEYS_MENU_FOLDALL,
	GEANY_KEYS_MENU_UNFOLDALL,
	GEANY_KEYS_RELOADTAGLIST,

	GEANY_KEYS_BUILD_COMPILE,
	GEANY_KEYS_BUILD_LINK,
	GEANY_KEYS_BUILD_MAKE,
	GEANY_KEYS_BUILD_MAKEOWNTARGET,
	GEANY_KEYS_BUILD_MAKEOBJECT,
	GEANY_KEYS_BUILD_NEXTERROR,
	GEANY_KEYS_BUILD_RUN,
	GEANY_KEYS_BUILD_RUN2,
	GEANY_KEYS_BUILD_OPTIONS,

	GEANY_KEYS_MENU_OPENCOLORCHOOSER,
	GEANY_KEYS_MENU_INSERTSPECIALCHARS,

	GEANY_KEYS_MENU_HELP,

	GEANY_KEYS_SWITCH_EDITOR,
	GEANY_KEYS_SWITCH_SCRIBBLE,
	GEANY_KEYS_SWITCH_VTE,
	GEANY_KEYS_SWITCH_SEARCH_BAR,
	GEANY_KEYS_SWITCH_TABLEFT,
	GEANY_KEYS_SWITCH_TABRIGHT,
	GEANY_KEYS_SWITCH_TABLASTUSED,
	GEANY_KEYS_MOVE_TABLEFT,
	GEANY_KEYS_MOVE_TABRIGHT,
	GEANY_KEYS_NAV_FORWARD,
	GEANY_KEYS_NAV_BACK,

	GEANY_KEYS_EDIT_TOGGLECASE,
	GEANY_KEYS_EDIT_DUPLICATELINE,
	GEANY_KEYS_EDIT_DELETELINE,
	GEANY_KEYS_EDIT_COPYLINE,
	GEANY_KEYS_EDIT_CUTLINE,
	GEANY_KEYS_EDIT_TRANSPOSELINE,
	GEANY_KEYS_EDIT_COMMENTLINETOGGLE,
	GEANY_KEYS_EDIT_COMMENTLINE,
	GEANY_KEYS_EDIT_UNCOMMENTLINE,
	GEANY_KEYS_EDIT_INCREASEINDENT,
	GEANY_KEYS_EDIT_DECREASEINDENT,
	GEANY_KEYS_EDIT_INCREASEINDENTBYSPACE,
	GEANY_KEYS_EDIT_DECREASEINDENTBYSPACE,
	GEANY_KEYS_EDIT_AUTOINDENT,
	GEANY_KEYS_EDIT_SENDTOCMD1,
	GEANY_KEYS_EDIT_SENDTOCMD2,
	GEANY_KEYS_EDIT_SENDTOCMD3,
	GEANY_KEYS_EDIT_GOTOMATCHINGBRACE,
	GEANY_KEYS_EDIT_TOGGLEMARKER,
	GEANY_KEYS_EDIT_GOTONEXTMARKER,
	GEANY_KEYS_EDIT_GOTOPREVIOUSMARKER,
	GEANY_KEYS_EDIT_SELECTWORD,
	GEANY_KEYS_EDIT_SELECTLINE,
	GEANY_KEYS_EDIT_SELECTPARAGRAPH,
	GEANY_KEYS_EDIT_SCROLLTOLINE,
	GEANY_KEYS_EDIT_SCROLLLINEUP,
	GEANY_KEYS_EDIT_SCROLLLINEDOWN,
	GEANY_KEYS_EDIT_INSERTALTWHITESPACE,

	GEANY_KEYS_POPUP_FINDUSAGE,
	GEANY_KEYS_POPUP_GOTOTAGDEFINITION,
	GEANY_KEYS_POPUP_GOTOTAGDECLARATION,
	GEANY_KEYS_POPUP_CONTEXTACTION,

	GEANY_KEYS_EDIT_AUTOCOMPLETE,
	GEANY_KEYS_EDIT_CALLTIP,
	GEANY_KEYS_EDIT_MACROLIST,
	GEANY_KEYS_EDIT_COMPLETECONSTRUCT,
	GEANY_KEYS_EDIT_SUPPRESSCOMPLETION,
	GEANY_MAX_KEYS
}
GeanyKeyCommand;

binding	*keys[GEANY_MAX_KEYS];


void keybindings_init(void);

void keybindings_free(void);

void keybindings_cmd(GeanyKeyCommand cmd_id);

/* just write the content of the keys array to the config file */
void keybindings_write_to_file(void);

void keybindings_show_shortcuts();

/* central keypress event handler, almost all keypress events go to this function */
gboolean keybindings_got_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

#endif

