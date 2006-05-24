/*
 *      keybindings.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_KEYBINDINGS_H
#define GEANY_KEYBINDINGS_H 1

// holds all user-definable key bindings
typedef struct binding
{
	guint key;
	GdkModifierType mods;
	const gchar *name;
	// function pointer to a callback function, just to keep the code in keypress event
	// callback function clear
	void (*cb_func) (void);
} binding;


enum
{
	GEANY_KEYS_MENU_NEW = 0,
	GEANY_KEYS_MENU_OPEN,
	GEANY_KEYS_MENU_SAVE,
	GEANY_KEYS_MENU_SAVEALL,
	GEANY_KEYS_MENU_CLOSEALL,
	GEANY_KEYS_MENU_RELOADFILE,
	GEANY_KEYS_MENU_UNDO,
	GEANY_KEYS_MENU_REDO,
	GEANY_KEYS_MENU_PREFERENCES,
	GEANY_KEYS_MENU_FIND_NEXT,
	GEANY_KEYS_MENU_FINDPREVIOUS,
	GEANY_KEYS_MENU_REPLACE,
	GEANY_KEYS_MENU_GOTOLINE,
	GEANY_KEYS_MENU_OPENCOLORCHOOSER,
	GEANY_KEYS_MENU_FULLSCREEN,
	GEANY_KEYS_MENU_MESSAGEWINDOW,
	GEANY_KEYS_MENU_ZOOMIN,
	GEANY_KEYS_MENU_ZOOMOUT,
	GEANY_KEYS_MENU_FOLDALL,
	GEANY_KEYS_MENU_UNFOLDALL,
	GEANY_KEYS_BUILD_COMPILE,
	GEANY_KEYS_BUILD_LINK,
	GEANY_KEYS_BUILD_MAKE,
	GEANY_KEYS_BUILD_MAKEOWNTARGET,
	GEANY_KEYS_BUILD_RUN,
	GEANY_KEYS_BUILD_RUN2,
	GEANY_KEYS_BUILD_OPTIONS,
	GEANY_KEYS_RELOADTAGLIST,
	GEANY_KEYS_SWITCH_EDITOR,
	GEANY_KEYS_SWITCH_SCRIBBLE,
	GEANY_KEYS_SWITCH_VTE,
	GEANY_KEYS_SWITCH_TABLEFT,
	GEANY_KEYS_SWITCH_TABRIGHT,
	GEANY_KEYS_TOOGLE_SIDEBAR,
	GEANY_KEYS_EDIT_DUPLICATELINE,
	GEANY_KEYS_EDIT_COMMENTLINE,
	GEANY_KEYS_EDIT_AUTOCOMPLETE,
	GEANY_KEYS_EDIT_CALLTIP,
	GEANY_KEYS_EDIT_MACROLIST,
	GEANY_KEYS_EDIT_SUPPRESSCOMPLETION,
	GEANY_MAX_KEYS
};

binding	*keys[GEANY_MAX_KEYS];



void keybindings_init(void);

void keybindings_free(void);

/* just write the content of the keys array to the config file */
void keybindings_write_to_file(void);

/* central keypress event handler, almost all keypress events go to this function */
gboolean keybindings_got_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

#endif

