/*
 *      geany.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* Main header - should be included first in all source files.
 * externs and function prototypes are implemented in main.c. */

#ifndef GEANY_H
#define GEANY_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "tm_tagmanager.h"

#ifndef PLAT_GTK
#   define PLAT_GTK 1	// needed when including ScintillaWidget.h
#endif


// for detailed description look in the documentation, things are not
// listed in the documentation should not be changed ;-)
#define GEANY_FILEDEFS_SUBDIR			"filedefs"
#define GEANY_TEMPLATES_SUBDIR			"templates"
#define GEANY_CODENAME					"Vensell"
#define GEANY_HOMEPAGE					"http://geany.uvena.de/"
#define GEANY_PROJECT_EXT				"geany"
#define GEANY_USE_WIN32_DIALOG			0
#define GEANY_CHECK_FILE_DELAY			30
#define GEANY_WORDCHARS					"_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define GEANY_TOGGLE_MARK				"~ "
#define GEANY_MAX_WORD_LENGTH			192
#define GEANY_MAX_AUTOCOMPLETE_WORDS	30
#define GEANY_MAX_SYMBOLLIST_HEIGHT		10
#define GEANY_MIN_SYMBOLLIST_CHARS		4
#define GEANY_STRING_UNTITLED			_("untitled")
#define GEANY_MSGWIN_HEIGHT				208
#define GEANY_WINDOW_MINIMAL_WIDTH		550
#define GEANY_WINDOW_MINIMAL_HEIGHT		350
#define GEANY_WINDOW_DEFAULT_WIDTH		900
#define GEANY_WINDOW_DEFAULT_HEIGHT		600
// some default settings which are used at the very first start of Geany to fill configuration file
#define GEANY_DEFAULT_TOOLS_MAKE		"make"
#ifdef G_OS_WIN32
#define GEANY_DEFAULT_TOOLS_TERMINAL	"cmd.exe"
#else
#define GEANY_DEFAULT_TOOLS_TERMINAL	"xterm"
#endif
#define GEANY_DEFAULT_TOOLS_BROWSER		"firefox"
#define GEANY_DEFAULT_TOOLS_PRINTCMD	"lpr"
#define GEANY_DEFAULT_TOOLS_GREP		"grep"
#define GEANY_DEFAULT_MRU_LENGTH		10
#define GEANY_DEFAULT_FONT_SYMBOL_LIST	"Sans 9"
#define GEANY_DEFAULT_FONT_MSG_WINDOW	"Sans 9"
#define GEANY_DEFAULT_FONT_EDITOR		"Monospace 10"



// useful forward declarations
typedef struct filetype filetype;
typedef struct _GeanyProject GeanyProject;


/* Commonly used items.
 * Remember to increment abi_version in plugindata.h when changing items. */
typedef struct GeanyApp
{
	gboolean			debug_mode;
	gchar				*configdir;
	gchar				*datadir;
	gchar				*docdir;
	gchar			   **active_plugins; // list of plugin filenames to load at startup
	const TMWorkspace	*tm_workspace;
	GeanyProject		*project;	// currently active project or NULL if none is open
	gboolean			ignore_callback;	// should not be used in new code (use clicked instead of toggled signal)

	/* Important widgets */
	GtkWidget			*window;
	GtkWidget			*toolbar;
	GtkWidget			*treeview_notebook;
	GtkWidget			*notebook;
	GtkWidget			*statusbar;	// use ui_set_statusbar() to set
	GtkWidget			*popup_menu;
}
GeanyApp;

extern GeanyApp *app;


enum
{
	GEANY_IMAGE_SMALL_CROSS,
	GEANY_IMAGE_LOGO,
	GEANY_IMAGE_COMPILE,
	GEANY_IMAGE_SAVE_ALL,
	GEANY_IMAGE_NEW_ARROW
};

enum
{
	UP,
	DOWN,
	LEFT,
	RIGHT
};

enum
{
	KILOBYTE = 1024,
	MEGABYTE = (KILOBYTE*1024),
	GIGABYTE = (MEGABYTE*1024)
};


/* Useful for some variable argument list functions, e.g. in utils.h */
#if ! GLIB_CHECK_VERSION(2, 8, 0)
#define G_GNUC_NULL_TERMINATED
#endif


// prototype is here so that all files can use it.
void geany_debug(gchar const *format, ...) G_GNUC_PRINTF (1, 2);

#endif
