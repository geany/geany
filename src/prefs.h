/*
 *      prefs.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 */

#ifndef GEANY_PREFS_H
#define GEANY_PREFS_H 1

/** General Preferences dialog settings. */
typedef struct GeanyPrefs
{
	gboolean		load_session;
	gboolean		load_plugins;
	gboolean		save_winpos;
	gboolean		confirm_exit;
	gboolean		beep_on_errors;		/* use utils_beep() instead */
	gboolean		suppress_status_messages;
	gboolean		switch_to_status;
	gboolean		auto_focus;
	gchar			*default_open_path;	/**< Default path to look for files when no other path is appropriate. */
	gchar			*custom_plugin_path;
}
GeanyPrefs;

extern GeanyPrefs prefs;


typedef struct GeanyToolPrefs
{
	gchar			*browser_cmd;
	gchar			*term_cmd;
	gchar			*grep_cmd;
	gchar			*context_action_cmd;
}
GeanyToolPrefs;

extern GeanyToolPrefs tool_prefs;


void prefs_show_dialog(void);

#endif
