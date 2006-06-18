/*
 *      geany.h - this file is part of Geany, a fast and lightweight IDE
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#ifndef PLAT_GTK
#   define PLAT_GTK 1
#endif

#ifndef GEANY_H
#define GEANY_H

#include "Scintilla.h"
#include "SciLexer.h"
#include "ScintillaWidget.h"

#include "tm_tagmanager.h"

#include "filetypes.h"

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)


#ifdef G_OS_WIN32
# include <windows.h>
# include <commdlg.h>
# define GEANY_WIN32
# define GEANY_DATA_DIR PACKAGE_DATA_DIR
#else
# define GEANY_DATA_DIR PACKAGE_DATA_DIR "/" PACKAGE
#endif


// for detailed description look in the documentation, things are not
// listed in the documentation should not be changed ;-)
#define GEANY_HOME_DIR					g_get_home_dir()
#define GEANY_FILEDEFS_SUBDIR			"filedefs"
#define GEANY_FIFO_NAME					"geany_fifo.0"
#define GEANY_CODENAME					"Kadir"
#define GEANY_HOMEPAGE					"http://geany.uvena.de/"
#define GEANY_MAX_OPEN_FILES			25
#define GEANY_SESSION_FILES				25
#define GEANY_MAX_TAGS_COUNT			1000
#define GEANY_CHECK_FILE_DELAY			30
#define GEANY_WORDCHARS					"_#&abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define GEANY_MAX_WORD_LENGTH			192
#define GEANY_MAX_AUTOCOMPLETE_WORDS	30
#define GEANY_STRING_UNTITLED			_("untitled")
#define GEANY_MSGWIN_HEIGHT				208
#define GEANY_WINDOW_MINIMAL_WIDTH		620
#define GEANY_WINDOW_MINIMAL_HEIGHT		440
#define GEANY_WINDOW_DEFAULT_WIDTH		900
#define GEANY_WINDOW_DEFAULT_HEIGHT		600

#ifndef DOCDIR
# define DOCDIR PACKAGE_DATA_DIR"/doc/"PACKAGE"/html/"
#endif


/* structure for representing an open tab with all its related stuff. */
typedef struct document
{
	gboolean		 is_valid;
	gboolean		 has_tags;
	// the filename is encoded in UTF-8, but every GLibC function expect the locale representation
	gchar 			*file_name;
	gchar 			*encoding;
	filetype		*file_type;
	TMWorkObject	*tm_file;
	ScintillaObject	*sci;
	GtkWidget		*tab_label;
	GtkWidget		*tabmenu_label;
	GtkWidget		*tag_tree;
	GtkTreeStore	*tag_store;
	GtkTreeIter		 iter;
	gboolean		 readonly;
	gboolean		 changed;
	gboolean		 do_overwrite;
	gboolean		 line_breaking;
	gboolean		 use_auto_indention;
	time_t			 last_check;	// to remember the last disk check
	time_t			 mtime;
} document;



/* array of document elements to hold all information of the notebook tabs */
document doc_list[GEANY_MAX_OPEN_FILES];


/* store some pointers and variables for frequently used widgets  */
typedef struct MyApp
{
	gint			 	 toolbar_icon_style;
	// 0:x, 1:y, 2:width, 3:height
	gint				 geometry[4];
	gboolean			 debug_mode;
	// represents the state at startup while opening session files
	gboolean			 opening_session_files;
	// represents the state when Geany is quitting completely
	gboolean			 quitting;
	gboolean			 ignore_global_tags;
	gboolean			 toolbar_visible;
	gboolean			 treeview_symbol_visible;
	gboolean			 treeview_openfiles_visible;
	gboolean			 msgwindow_visible;
	gboolean			 fullscreen;
	gboolean			 beep_on_errors;
	gboolean			 switch_msgwin_pages;
	gboolean			 tab_order_ltr;
	gboolean			 show_markers_margin;
	gboolean			 show_linenumber_margin;
	gboolean			 brace_match_ltgt;
	gboolean			 main_window_realized;
	// I know, it is a bit confusing, but this line breaking is globally,
	// to change the default value at startup, I think
	gboolean			 pref_editor_line_breaking;
	gboolean			 pref_editor_use_auto_indention;
	gboolean			 pref_editor_use_indicators;
	gboolean			 pref_editor_show_white_space;
	gboolean			 pref_editor_show_indent_guide;
	gboolean			 pref_editor_show_line_endings;
	gboolean			 pref_editor_auto_close_xml_tags;
	gboolean			 pref_editor_auto_complete_constructs;
	gboolean			 pref_editor_folding;
	gint				 pref_editor_tab_width;
	gboolean			 pref_editor_new_line;
	gboolean			 pref_editor_trail_space;
	gboolean			 pref_main_load_session;
	gboolean			 pref_main_save_winpos;
	gboolean			 pref_main_confirm_exit;
	gboolean			 pref_main_show_search;
	gboolean			 pref_main_show_goto;
	gint				 cur_idx;	// currently only used by the build system (build_spawn_cmd)
	gint				 mru_length;
	gint				 long_line_column;
#ifdef HAVE_FIFO
	gboolean			 ignore_fifo;
	GIOChannel			*fifo_ioc;
#endif
#ifdef HAVE_VTE
	gboolean			 load_vte;
	gboolean			 have_vte;
	gchar				*lib_vte;
#endif
	gchar				*long_line_color;
	gchar				*pref_template_developer;
	gchar				*pref_template_company;
	gchar				*pref_template_mail;
	gchar				*pref_template_initial;
	gchar				*pref_template_version;
	gchar				*editor_font;
	gchar				*tagbar_font;
	gchar				*msgwin_font;
	gchar				*configdir;
	gchar				*search_text;
	gchar				*terminal_settings;
	gchar				 build_make_custopt[256];
	gchar				*build_browser_cmd;
	gchar				*build_make_cmd;
	gchar				*build_term_cmd;
	gchar			   **recent_files;
	GtkIconSize			 toolbar_icon_size;
	GtkWidget			*toolbar;
	GtkWidget			*run_button;
	GtkWidget			*compile_button;
	GtkWidget			*compile_button_image;
	GtkWidget			*tagbar;
	GtkWidget			*treeview_notebook;
	GtkWidget			*notebook;
	GtkWidget			*statusbar;
	GtkWidget			*window;
	GtkWidget			*popup_menu;
	GtkWidget			*toolbar_menu;
	GtkWidget			*new_file_menu;
	GtkWidget			*menu_insert_include_item[2];
	GtkWidget			*popup_goto_items[3];
	GtkWidget			*popup_items[4];
	GtkWidget			*menu_copy_items[4];
	GtkWidget			*redo_items[2];
	GtkWidget			*undo_items[2];
	GtkWidget			*save_buttons[2];
	GtkWidget			*sensitive_buttons[31];
	GtkWidget			*open_colorsel;
	GtkWidget			*open_fontsel;
	GtkWidget			*open_filesel;
	GtkWidget			*save_filesel;
	GtkWidget			*prefs_dialog;
	GtkWidget			*find_dialog;
	GtkWidget			*replace_dialog;
	GtkWidget			*default_tag_tree;
	const TMWorkspace	*tm_workspace;
	GQueue				*recent_queue;
} MyApp;

MyApp *app;

/// TODO remove me
gboolean alternative_scrolling;


// small struct to track tag name and type together
typedef struct GeanySymbol
{
	gchar	*str;
	gint	 type;
	gint	 line;
} GeanySymbol;


enum
{
	GEANY_IMAGE_SMALL_CROSS,
	GEANY_IMAGE_LOGO,
	GEANY_IMAGE_COMPILE,
	GEANY_IMAGE_SAVE_ALL,
	GEANY_IMAGE_NEW_ARROW
};

enum {
	GEANY_RESPONSE_REPLACE = 1,
	GEANY_RESPONSE_REPLACE_ALL = 2,
	GEANY_RESPONSE_REPLACE_SEL = 3
};


enum
{
	UP,
	DOWN,
	LEFT,
	RIGHT
};

enum {
	KILOBYTE = 1024,
	MEGABYTE = (KILOBYTE*1024),
	GIGABYTE = (MEGABYTE*1024)
};

// prototype from tagmanager/parse.h, used in document.c, ugly but it works
extern langType getNamedLanguage(const char *const name);

// implementation in main.c
void geany_debug(gchar const *format, ...);


#endif
