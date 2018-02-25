/*
 *      prefs.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

#ifndef GEANY_PREFS_H
#define GEANY_PREFS_H 1

#include <glib.h>

G_BEGIN_DECLS

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
	gboolean		save_wingeom;
}
GeanyPrefs;

/** Tools preferences */
typedef struct GeanyToolPrefs
{
	gchar			*browser_cmd;			/**< web browser command */
	gchar			*term_cmd;				/**< terminal emulator command */
	gchar			*grep_cmd;				/**< grep command */
	gchar			*context_action_cmd;	/**< context action command */
}
GeanyToolPrefs;


#ifdef GEANY_PRIVATE

extern GeanyPrefs prefs;

extern GeanyToolPrefs tool_prefs;

void prefs_show_dialog(void);

void prefs_kb_search_name(const gchar *search);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_PREFS_H */
