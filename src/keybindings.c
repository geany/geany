/*
 *      keybindings.c - this file is part of Geany, a fast and lightweight IDE
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

/*
 * Configurable keyboard shortcuts.
 */

#include <gdk/gdkkeysyms.h>

#include "geany.h"
#include "keybindings.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "document.h"
#include "callbacks.h"
#include "prefs.h"
#include "msgwindow.h"
#include "sci_cb.h"
#include "sciwrappers.h"
#include "build.h"
#include "tools.h"
// include vte.h on non-Win32 systems, else define fake vte_init
#ifdef HAVE_VTE
# include "vte.h"
#endif


const gboolean swap_alt_tab_order = FALSE;


/* simple convenience function to allocate and fill the struct */
static binding *fill(KBCallback func, guint key, GdkModifierType mod, const gchar *name,
		const gchar *label);

static void cb_func_menu_new(guint key_id);
static void cb_func_menu_open(guint key_id);
static void cb_func_menu_open_selected(guint key_id);
static void cb_func_menu_save(guint key_id);
static void cb_func_menu_saveall(guint key_id);
static void cb_func_menu_saveas(guint key_id);
static void cb_func_menu_print(guint key_id);
static void cb_func_menu_close(guint key_id);
static void cb_func_menu_closeall(guint key_id);
static void cb_func_menu_reloadfile(guint key_id);
static void cb_func_menu_undo(guint key_id);
static void cb_func_menu_redo(guint key_id);
static void cb_func_menu_selectall(guint key_id);
static void cb_func_menu_preferences(guint key_id);
static void cb_func_menu_insert_date(guint key_id);
static void cb_func_menu_findnext(guint key_id);
static void cb_func_menu_findprevious(guint key_id);
static void cb_func_menu_findnextsel(guint key_id);
static void cb_func_menu_findprevsel(guint key_id);
static void cb_func_menu_replace(guint key_id);
static void cb_func_menu_findinfiles(guint key_id);
static void cb_func_menu_nextmessage(guint key_id);
static void cb_func_menu_gotoline(guint key_id);
static void cb_func_menu_opencolorchooser(guint key_id);
static void cb_func_menu_fullscreen(guint key_id);
static void cb_func_menu_messagewindow(guint key_id);
static void cb_func_menu_zoomin(guint key_id);
static void cb_func_menu_zoomout(guint key_id);
static void cb_func_menu_replacetabs(guint key_id);
static void cb_func_menu_foldall(guint key_id);
static void cb_func_menu_unfoldall(guint key_id);
static void cb_func_menu_insert_specialchars(guint key_id);
static void cb_func_build_action(guint key_id);
static void cb_func_reloadtaglist(guint key_id);
static void cb_func_switch_editor(guint key_id);
static void cb_func_switch_scribble(guint key_id);
static void cb_func_switch_vte(guint key_id);
static void cb_func_switch_tableft(guint key_id);
static void cb_func_switch_tabright(guint key_id);
static void cb_func_switch_tablastused(guint key_id);
static void cb_func_toggle_sidebar(guint key_id);

// common function for editing keybindings, only valid when scintilla has focus.
static void cb_func_edit(guint key_id);

// common function for keybindings using current word
static void cb_func_current_word(guint key_id);

static void keybindings_add_accels();


void keybindings_init(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "keybindings.conf", NULL);
	gchar *val;
	guint i;
	guint key;
	GdkModifierType mods;
	GKeyFile *config = g_key_file_new();

	// init all fields of keys with default values
	keys[GEANY_KEYS_MENU_NEW] = fill(cb_func_menu_new,
		GDK_n, GDK_CONTROL_MASK, "menu_new", _("New"));
	keys[GEANY_KEYS_MENU_OPEN] = fill(cb_func_menu_open,
		GDK_o, GDK_CONTROL_MASK, "menu_open", _("Open"));
	keys[GEANY_KEYS_MENU_OPEN_SELECTED] = fill(cb_func_menu_open_selected,
		GDK_o, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "menu_open_selected", _("Open selected file"));
	keys[GEANY_KEYS_MENU_SAVE] = fill(cb_func_menu_save,
		GDK_s, GDK_CONTROL_MASK, "menu_save", _("Save"));
	keys[GEANY_KEYS_MENU_SAVEAS] = fill(cb_func_menu_saveas,
		0, 0, "menu_saveas", _("Save as"));
	keys[GEANY_KEYS_MENU_SAVEALL] = fill(cb_func_menu_saveall,
		GDK_S, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "menu_saveall", _("Save all"));
	keys[GEANY_KEYS_MENU_PRINT] = fill(cb_func_menu_print,
		GDK_p, GDK_CONTROL_MASK, "menu_print", _("Print"));
	keys[GEANY_KEYS_MENU_CLOSE] = fill(cb_func_menu_close,
		GDK_w, GDK_CONTROL_MASK, "menu_close", _("Close"));
	keys[GEANY_KEYS_MENU_CLOSEALL] = fill(cb_func_menu_closeall,
		GDK_w, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "menu_closeall", _("Close all"));
	keys[GEANY_KEYS_MENU_RELOADFILE] = fill(cb_func_menu_reloadfile,
		GDK_r, GDK_CONTROL_MASK, "menu_reloadfile", _("Reload file"));
	keys[GEANY_KEYS_MENU_UNDO] = fill(cb_func_menu_undo,
		GDK_z, GDK_CONTROL_MASK, "menu_undo", _("Undo"));
	keys[GEANY_KEYS_MENU_REDO] = fill(cb_func_menu_redo,
		GDK_y, GDK_CONTROL_MASK, "menu_redo", _("Redo"));
	keys[GEANY_KEYS_MENU_SELECTALL] = fill(cb_func_menu_selectall,
		GDK_a, GDK_CONTROL_MASK, "menu_selectall", _("Select All"));
	keys[GEANY_KEYS_MENU_INSERTDATE] = fill(cb_func_menu_insert_date,
		GDK_d, GDK_SHIFT_MASK | GDK_MOD1_MASK, "menu_insert_date", _("Insert date"));
	keys[GEANY_KEYS_MENU_PREFERENCES] = fill(cb_func_menu_preferences,
		0, 0, "menu_preferences", _("Preferences"));
	keys[GEANY_KEYS_MENU_FINDNEXT] = fill(cb_func_menu_findnext,
		GDK_F3, 0, "menu_findnext", _("Find Next"));
	keys[GEANY_KEYS_MENU_FINDPREVIOUS] = fill(cb_func_menu_findprevious,
		GDK_F3, GDK_SHIFT_MASK, "menu_findprevious", _("Find Previous"));
	keys[GEANY_KEYS_MENU_FINDNEXTSEL] = fill(cb_func_menu_findnextsel,
		0, 0, "menu_findnextsel", _("Find Next Selection"));
	keys[GEANY_KEYS_MENU_FINDPREVSEL] = fill(cb_func_menu_findprevsel,
		0, 0, "menu_findprevsel", _("Find Previous Selection"));
	keys[GEANY_KEYS_MENU_REPLACE] = fill(cb_func_menu_replace,
		GDK_e, GDK_CONTROL_MASK, "menu_replace", _("Replace"));
	keys[GEANY_KEYS_MENU_FINDINFILES] = fill(cb_func_menu_findinfiles, GDK_f,
		GDK_CONTROL_MASK | GDK_SHIFT_MASK, "menu_findinfiles", _("Find in files"));
	keys[GEANY_KEYS_MENU_NEXTMESSAGE] = fill(cb_func_menu_nextmessage,
		0, 0, "menu_nextmessage", _("Next Message"));
	keys[GEANY_KEYS_MENU_GOTOLINE] = fill(cb_func_menu_gotoline,
		GDK_l, GDK_CONTROL_MASK, "menu_gotoline", _("Go to line"));
	keys[GEANY_KEYS_MENU_OPENCOLORCHOOSER] = fill(cb_func_menu_opencolorchooser,
		0, 0, "menu_opencolorchooser", _("Show Colour Chooser"));
	keys[GEANY_KEYS_MENU_FULLSCREEN] = fill(cb_func_menu_fullscreen,
		GDK_F11, 0, "menu_fullscreen", _("Fullscreen"));
	keys[GEANY_KEYS_MENU_MESSAGEWINDOW] = fill(cb_func_menu_messagewindow,
		0, 0, "menu_messagewindow", _("Toggle Messages Window"));
	keys[GEANY_KEYS_MENU_SIDEBAR] = fill(cb_func_toggle_sidebar,
		0, 0, "toggle_sidebar", _("Toggle Sidebar"));
	keys[GEANY_KEYS_MENU_ZOOMIN] = fill(cb_func_menu_zoomin,
		GDK_plus, GDK_CONTROL_MASK, "menu_zoomin", _("Zoom In"));
	keys[GEANY_KEYS_MENU_ZOOMOUT] = fill(cb_func_menu_zoomout,
		GDK_minus, GDK_CONTROL_MASK, "menu_zoomout", _("Zoom Out"));
	keys[GEANY_KEYS_MENU_REPLACETABS] = fill(cb_func_menu_replacetabs,
		0, 0, "menu_replacetabs", _("Replace tabs by space"));
	keys[GEANY_KEYS_MENU_FOLDALL] = fill(cb_func_menu_foldall,
		0, 0, "menu_foldall", _("Fold all"));
	keys[GEANY_KEYS_MENU_UNFOLDALL] = fill(cb_func_menu_unfoldall,
		0, 0, "menu_unfoldall", _("Unfold all"));
	keys[GEANY_KEYS_MENU_INSERTSPECIALCHARS] = fill(cb_func_menu_insert_specialchars,
		0, 0, "menu_insert_specialchars", _("Insert Special HTML Characters"));
	keys[GEANY_KEYS_BUILD_COMPILE] = fill(cb_func_build_action,
		GDK_F8, 0, "build_compile", _("Compile"));
	keys[GEANY_KEYS_BUILD_LINK] = fill(cb_func_build_action,
		GDK_F9, 0, "build_link", _("Build"));
	keys[GEANY_KEYS_BUILD_MAKE] = fill(cb_func_build_action,
		GDK_F9, GDK_SHIFT_MASK, "build_make", _("Make all"));
	keys[GEANY_KEYS_BUILD_MAKEOWNTARGET] = fill(cb_func_build_action,
		GDK_F9, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "build_makeowntarget",
		_("Make custom target"));
	keys[GEANY_KEYS_BUILD_MAKEOBJECT] = fill(cb_func_build_action,
		0, 0, "build_makeobject", _("Make object"));
	keys[GEANY_KEYS_BUILD_NEXTERROR] = fill(cb_func_build_action,
		0, 0, "build_nexterror", _("Next error"));
	keys[GEANY_KEYS_BUILD_RUN] = fill(cb_func_build_action,
		GDK_F5, 0, "build_run", _("Run"));
	keys[GEANY_KEYS_BUILD_RUN2] = fill(cb_func_build_action,
		0, 0, "build_run2", _("Run (alternative command)"));
	keys[GEANY_KEYS_BUILD_OPTIONS] = fill(cb_func_build_action,
		0, 0, "build_options", _("Build options"));
	keys[GEANY_KEYS_RELOADTAGLIST] = fill(cb_func_reloadtaglist,
		GDK_r, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "reloadtaglist", _("Reload symbol list"));
	keys[GEANY_KEYS_SWITCH_EDITOR] = fill(cb_func_switch_editor,
		GDK_F2, 0, "switch_editor", _("Switch to Editor"));
	keys[GEANY_KEYS_SWITCH_SCRIBBLE] = fill(cb_func_switch_scribble,
		GDK_F6, 0, "switch_scribble", _("Switch to Scribble"));
	keys[GEANY_KEYS_SWITCH_VTE] = fill(cb_func_switch_vte,
		GDK_F4, 0, "switch_vte", _("Switch to VTE"));
	keys[GEANY_KEYS_SWITCH_TABLEFT] = fill(cb_func_switch_tableft,
		GDK_Page_Up, GDK_CONTROL_MASK, "switch_tableft", _("Switch to left document"));
	keys[GEANY_KEYS_SWITCH_TABRIGHT] = fill(cb_func_switch_tabright,
		GDK_Page_Down, GDK_CONTROL_MASK, "switch_tabright", _("Switch to right document"));
	keys[GEANY_KEYS_SWITCH_TABLASTUSED] = fill(cb_func_switch_tablastused,
		GDK_Tab, GDK_CONTROL_MASK, "switch_tablastused", _("Switch to last used document"));

	keys[GEANY_KEYS_EDIT_DUPLICATELINE] = fill(cb_func_edit,
		GDK_g, GDK_CONTROL_MASK, "edit_duplicateline", _("Duplicate line or selection"));
	keys[GEANY_KEYS_EDIT_TOLOWERCASE] = fill(cb_func_edit,
		GDK_u, GDK_CONTROL_MASK, "edit_tolowercase", _("Convert Selection to lower-case"));
	keys[GEANY_KEYS_EDIT_TOUPPERCASE] = fill(cb_func_edit,
		GDK_u, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "edit_touppercase", _("Convert Selection to upper-case"));
	keys[GEANY_KEYS_EDIT_COMMENTLINETOGGLE] = fill(cb_func_edit,
		GDK_b, GDK_CONTROL_MASK, "edit_commentlinetoggle", _("Toggle line commentation"));
	keys[GEANY_KEYS_EDIT_COMMENTLINE] = fill(cb_func_edit,
		GDK_d, GDK_CONTROL_MASK, "edit_commentline", _("Comment line(s)"));
	keys[GEANY_KEYS_EDIT_UNCOMMENTLINE] = fill(cb_func_edit,
		GDK_d, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "edit_uncommentline", _("Uncomment line(s)"));
	keys[GEANY_KEYS_EDIT_INCREASEINDENT] = fill(cb_func_edit,
		GDK_i, GDK_CONTROL_MASK, "edit_increaseindent", _("Increase indent"));
	keys[GEANY_KEYS_EDIT_DECREASEINDENT] = fill(cb_func_edit,
		GDK_i, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "edit_decreaseindent", _("Decrease indent"));
	keys[GEANY_KEYS_EDIT_SENDTOCMD1] = fill(cb_func_edit,
		GDK_1, GDK_CONTROL_MASK, "edit_sendtocmd1", _("Send Selection to custom command 1"));
	keys[GEANY_KEYS_EDIT_SENDTOCMD2] = fill(cb_func_edit,
		GDK_2, GDK_CONTROL_MASK, "edit_sendtocmd2", _("Send Selection to custom command 2"));
	keys[GEANY_KEYS_EDIT_SENDTOCMD3] = fill(cb_func_edit,
		GDK_3, GDK_CONTROL_MASK, "edit_sendtocmd3", _("Send Selection to custom command 3"));
	keys[GEANY_KEYS_EDIT_GOTOMATCHINGBRACE] = fill(cb_func_edit,
		GDK_less, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "edit_gotomatchingbrace",
		_("Goto matching brace"));
	keys[GEANY_KEYS_EDIT_TOGGLEMARKER] = fill(cb_func_edit,
		GDK_m, GDK_CONTROL_MASK, "edit_togglemarker",
		_("Toggle marker"));
	keys[GEANY_KEYS_EDIT_GOTONEXTMARKER] = fill(cb_func_edit,
		GDK_period, GDK_CONTROL_MASK, "edit_gotonextmarker",
		_("Goto next marker"));
	keys[GEANY_KEYS_EDIT_GOTOPREVIOUSMARKER] = fill(cb_func_edit,
		GDK_comma, GDK_CONTROL_MASK, "edit_gotopreviousmarker",
		_("Goto previous marker"));
	keys[GEANY_KEYS_EDIT_AUTOCOMPLETE] = fill(cb_func_edit,
		GDK_space, GDK_CONTROL_MASK, "edit_autocomplete", _("Complete word"));
#ifdef G_OS_WIN32
	// on windows alt-space is taken by the window manager
	keys[GEANY_KEYS_EDIT_CALLTIP] = fill(cb_func_edit,
		GDK_space, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "edit_calltip", _("Show calltip"));
#else
	keys[GEANY_KEYS_EDIT_CALLTIP] = fill(cb_func_edit,
		GDK_space, GDK_MOD1_MASK, "edit_calltip", _("Show calltip"));
#endif
	keys[GEANY_KEYS_EDIT_MACROLIST] = fill(cb_func_edit,
		GDK_Return, GDK_CONTROL_MASK, "edit_macrolist", _("Show macro list"));
	keys[GEANY_KEYS_EDIT_SUPPRESSCOMPLETION] = fill(cb_func_edit,
		GDK_space, GDK_SHIFT_MASK, "edit_suppresscompletion", _("Suppress auto completion"));

	keys[GEANY_KEYS_EDIT_SELECTWORD] = fill(cb_func_edit,
		0, 0, "edit_selectword", _("Select current word"));

	keys[GEANY_KEYS_POPUP_FINDUSAGE] = fill(cb_func_current_word,
		0, 0, "popup_findusage", _("Find Usage"));
	keys[GEANY_KEYS_POPUP_GOTOTAGDEFINITION] = fill(cb_func_current_word,
		0, 0, "popup_gototagdefinition", _("Go to tag definition"));
	keys[GEANY_KEYS_POPUP_GOTOTAGDECLARATION] = fill(cb_func_current_word,
		0, 0, "popup_gototagdeclaration", _("Go to tag declaration"));

	// now load user defined keys
	if (g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		for (i = 0; i < GEANY_MAX_KEYS; i++)
		{
			val = g_key_file_get_string(config, "Bindings", keys[i]->name, NULL);
			if (val != NULL)
			{
				gtk_accelerator_parse(val, &key, &mods);
				keys[i]->key = key;
				keys[i]->mods = mods;
			}
			g_free(val);
		}
	}

	g_free(configfile);
	g_key_file_free(config);

	keybindings_add_accels();
}


#define GEANY_ADD_ACCEL(gkey, wid) \
	if (keys[(gkey)]->key != 0) \
		gtk_widget_add_accelerator( \
			lookup_widget(app->window, G_STRINGIFY(wid)), \
			"activate", accel_group, keys[(gkey)]->key, keys[(gkey)]->mods, \
			GTK_ACCEL_VISIBLE)

#define GEANY_ADD_POPUP_ACCEL(gkey, wid) \
	if (keys[(gkey)]->key != 0) \
		gtk_widget_add_accelerator( \
			lookup_widget(app->popup_menu, G_STRINGIFY(wid)), \
			"activate", accel_group, keys[(gkey)]->key, keys[(gkey)]->mods, \
			GTK_ACCEL_VISIBLE)

static void keybindings_add_accels()
{
	GtkAccelGroup *accel_group = gtk_accel_group_new();

	// apply the settings
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_OPEN_SELECTED, menu_open_selected_file1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_SAVEALL, menu_save_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_SAVEAS, menu_save_as1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_PRINT, print1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_CLOSE, menu_close1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_CLOSEALL, menu_close_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_RELOADFILE, menu_reload1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_UNDO, menu_undo2);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_REDO, menu_redo2);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_SELECTALL, menu_select_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_INSERTDATE, insert_date_custom1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_PREFERENCES, preferences1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDNEXT, find_next1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDPREVIOUS, find_previous1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDNEXTSEL, find_nextsel1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDPREVSEL, find_prevsel1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_REPLACE, replace1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDINFILES, find_in_files1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_NEXTMESSAGE, next_message1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_GOTOLINE, go_to_line1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FULLSCREEN, menu_fullscreen1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_MESSAGEWINDOW, menu_show_messages_window1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_OPENCOLORCHOOSER, menu_choose_color1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_ZOOMIN, menu_zoom_in1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_ZOOMOUT, menu_zoom_out1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_REPLACETABS, menu_replace_tabs);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FOLDALL, menu_fold_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_UNFOLDALL, menu_unfold_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_INSERTSPECIALCHARS, menu_insert_special_chars1);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_TOLOWERCASE, menu_to_lower_case2);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_TOUPPERCASE, menu_to_upper_case2);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_COMMENTLINE, menu_comment_line1);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_UNCOMMENTLINE, menu_uncomment_line1);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_COMMENTLINETOGGLE, menu_toggle_line_commentation1);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_DUPLICATELINE, menu_duplicate_line1);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_INCREASEINDENT, menu_increase_indent1);
	GEANY_ADD_ACCEL(GEANY_KEYS_EDIT_DECREASEINDENT, menu_decrease_indent1);

	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_UNDO, undo1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_REDO, redo1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_SELECTALL, menu_select_all2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_INSERTDATE, insert_date_custom2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_OPEN_SELECTED, menu_open_selected_file2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_POPUP_FINDUSAGE, find_usage1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_POPUP_GOTOTAGDEFINITION, goto_tag_definition1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_POPUP_GOTOTAGDECLARATION, goto_tag_declaration1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_GOTOLINE, go_to_line);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_TOLOWERCASE, to_lower_case1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_TOUPPERCASE, to_upper_case1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_COMMENTLINE, menu_comment_line2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_UNCOMMENTLINE, menu_uncomment_line2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_COMMENTLINETOGGLE, menu_toggle_line_commentation2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_DUPLICATELINE, menu_duplicate_line2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_INCREASEINDENT, menu_increase_indent2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDIT_DECREASEINDENT, menu_decrease_indent2);

	// the build menu items are set if the build menus are created

	gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);
}


/* just write the content of the keys array to the config file */
void keybindings_write_to_file(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "keybindings.conf", NULL);
	gchar *val, *data;
	guint i;
	GKeyFile *config = g_key_file_new();

	// add comment if the file is newly created
	if (! g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		g_key_file_set_comment(config, NULL, NULL, "Keybindings for Geany\nThe format looks like \"<Control>a\" or \"<Shift><Alt>F1\".\nBut you can also change the keys in Geany's preferences dialog.", NULL);
	}

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		val = gtk_accelerator_name(keys[i]->key, keys[i]->mods);
		g_key_file_set_string(config, "Bindings", keys[i]->name, val);
		g_free(val);
	}

	// write the file
	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(configfile, data);

	g_free(data);
	g_free(configfile);
	g_key_file_free(config);
}


void keybindings_free(void)
{
	guint i;

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		g_free(keys[i]);
	}
}


void keybindings_show_shortcuts()
{
	GtkWidget *dialog, *hbox, *label1, *label2, *label3, *swin, *vbox;
	GString *text_names = g_string_sized_new(600);
	GString *text_keys = g_string_sized_new(600);
	gchar *shortcut;
	guint i;
	gint height, response;

	dialog = gtk_dialog_new_with_buttons(_("Keyboard shortcuts"), GTK_WINDOW(app->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_EDIT, GTK_RESPONSE_APPLY,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	height = GEANY_WINDOW_MINIMAL_HEIGHT;
	gtk_window_set_default_size(GTK_WINDOW(dialog), height * 0.8, height);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

	label3 = gtk_label_new(_("The following keyboard shortcuts are defined:"));
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 6);

	label1 = gtk_label_new(NULL);

	label2 = gtk_label_new(NULL);

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		shortcut = gtk_accelerator_get_label(keys[i]->key, keys[i]->mods);
		g_string_append(text_names, keys[i]->label);
		g_string_append(text_names, "\n");
		g_string_append(text_keys, shortcut);
		g_string_append(text_keys, "\n");
		g_free(shortcut);
	}

	gtk_label_set_text(GTK_LABEL(label1), text_names->str);
	gtk_label_set_text(GTK_LABEL(label2), text_keys->str);

	gtk_container_add(GTK_CONTAINER(hbox), label1);
	gtk_container_add(GTK_CONTAINER(hbox), label2);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), hbox);

	gtk_box_pack_start(GTK_BOX(vbox), label3, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

	gtk_widget_show_all(dialog);
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_APPLY)
	{
		GtkWidget *wid;

		prefs_show_dialog();
		// select the KB page
		wid = lookup_widget(app->prefs_dialog, "frame22");
		if (wid != NULL)
		{
			GtkNotebook *nb = GTK_NOTEBOOK(lookup_widget(app->prefs_dialog, "notebook2"));

			if (nb != NULL)
				gtk_notebook_set_current_page(nb, gtk_notebook_page_num(nb, wid));
		}
	}

	gtk_widget_destroy(dialog);

	g_string_free(text_names, TRUE);
	g_string_free(text_keys, TRUE);
}


static gboolean check_fixed_kb(GdkEventKey *event)
{
	// check alt-0 to alt-9 for setting current notebook page
	if (event->state & GDK_MOD1_MASK && event->keyval >= GDK_0 && event->keyval <= GDK_9)
	{
		gint page = event->keyval - GDK_0 - 1;
		gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

		// alt-0 is for the rightmost tab
		if (event->keyval == GDK_0)
			page = npages - 1;
		// invert the order if tabs are added on the other side
		if (swap_alt_tab_order && ! app->tab_order_ltr)
			page = (npages - 1) - page;

		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);
		return TRUE;
	}
	return FALSE;
}


/* central keypress event handler, almost all keypress events go to this function */
gboolean keybindings_got_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	guint i, k;

	if (check_fixed_kb(event)) return TRUE;

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		// ugly hack to get around that CTRL+Shift+r results in 'R' not 'r'
		k = keys[i]->key;
		if (event->state & GDK_SHIFT_MASK)
		{
			// skip entries which don't include SHIFT
			if (! (keys[i]->mods & GDK_SHIFT_MASK)) continue;
			// raise the keyval
			if (keys[i]->key >= GDK_a && keys[i]->key <= GDK_z) k = keys[i]->key - 32;
		}

		// ignore numlock key, not necessary but nice
		if (event->state & GDK_MOD2_MASK) event->state -= GDK_MOD2_MASK;

		if (event->keyval == k && event->state == keys[i]->mods)
		{
			// call the corresponding callback function for this shortcut
			if (keys[i]->cb_func != NULL) keys[i]->cb_func(i);
			return TRUE;
		}
	}
	return FALSE;
}


/* simple convenience function to allocate and fill the struct */
static binding *fill(KBCallback func, guint key, GdkModifierType mod, const gchar *name,
		const gchar *label)
{
	binding *result;

	result = g_new0(binding, 1);
	result->name = name;
	result->label = label;
	result->key = key;
	result->mods = mod;
	result->cb_func = func;

	return result;
}


/* these are the callback functions, each shortcut has its own function, this is only for clear code */
static void cb_func_menu_new(G_GNUC_UNUSED guint key_id)
{
	document_new_file(NULL, NULL);
}

static void cb_func_menu_open(G_GNUC_UNUSED guint key_id)
{
	on_open1_activate(NULL, NULL);
}

static void cb_func_menu_open_selected(G_GNUC_UNUSED guint key_id)
{
	on_menu_open_selected_file1_activate(NULL, NULL);
}

static void cb_func_menu_save(G_GNUC_UNUSED guint key_id)
{
	on_save1_activate(NULL, NULL);
}

static void cb_func_menu_saveall(G_GNUC_UNUSED guint key_id)
{
	on_save_all1_activate(NULL, NULL);
}

static void cb_func_menu_saveas(G_GNUC_UNUSED guint key_id)
{
	on_save_as1_activate(NULL, NULL);
}

static void cb_func_menu_close(G_GNUC_UNUSED guint key_id)
{
	on_close1_activate(NULL, NULL);
}

static void cb_func_menu_closeall(G_GNUC_UNUSED guint key_id)
{
	on_close_all1_activate(NULL, NULL);
}

static void cb_func_menu_reloadfile(G_GNUC_UNUSED guint key_id)
{
	on_toolbutton23_clicked(NULL, NULL);
}

static void cb_func_menu_undo(G_GNUC_UNUSED guint key_id)
{
	on_undo1_activate(NULL, NULL);
}

static void cb_func_menu_redo(G_GNUC_UNUSED guint key_id)
{
	on_redo1_activate(NULL, NULL);
}

static void cb_func_menu_selectall(G_GNUC_UNUSED guint key_id)
{
	on_menu_select_all1_activate(NULL, NULL);
}

static void cb_func_menu_preferences(G_GNUC_UNUSED guint key_id)
{
	on_preferences1_activate(NULL, NULL);
}

static void cb_func_menu_findnext(G_GNUC_UNUSED guint key_id)
{
	on_find_next1_activate(NULL, NULL);
}

static void cb_func_menu_findprevious(G_GNUC_UNUSED guint key_id)
{
	on_find_previous1_activate(NULL, NULL);
}

static void cb_func_menu_findprevsel(G_GNUC_UNUSED guint key_id)
{
	on_find_prevsel1_activate(NULL, NULL);
}

static void cb_func_menu_findnextsel(G_GNUC_UNUSED guint key_id)
{
	on_find_nextsel1_activate(NULL, NULL);
}


static void cb_func_menu_replace(G_GNUC_UNUSED guint key_id)
{
	on_replace1_activate(NULL, NULL);
}

static void cb_func_menu_findinfiles(G_GNUC_UNUSED guint key_id)
{
	on_find_in_files1_activate(NULL, NULL);
}

static void cb_func_menu_nextmessage(guint key_id)
{
	on_next_message1_activate(NULL, NULL);
}

static void cb_func_menu_gotoline(G_GNUC_UNUSED guint key_id)
{
	on_go_to_line1_activate(NULL, NULL);
}

static void cb_func_menu_opencolorchooser(G_GNUC_UNUSED guint key_id)
{
	on_show_color_chooser1_activate(NULL, NULL);
}

static void cb_func_menu_fullscreen(G_GNUC_UNUSED guint key_id)
{
	GtkCheckMenuItem *c = GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_fullscreen1"));

	gtk_check_menu_item_set_active(c, ! gtk_check_menu_item_get_active(c));
}

static void cb_func_menu_messagewindow(G_GNUC_UNUSED guint key_id)
{
	GtkCheckMenuItem *c = GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_messages_window1"));

	gtk_check_menu_item_set_active(c, ! gtk_check_menu_item_get_active(c));
}

static void cb_func_menu_zoomin(G_GNUC_UNUSED guint key_id)
{
	on_zoom_in1_activate(NULL, NULL);
}

static void cb_func_menu_zoomout(G_GNUC_UNUSED guint key_id)
{
	on_zoom_out1_activate(NULL, NULL);
}

static void cb_func_menu_foldall(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_fold_all(idx);
}

static void cb_func_menu_unfoldall(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_unfold_all(idx);
}

static void cb_func_build_action(guint key_id)
{
	gint idx = document_get_cur_idx();
	GtkWidget *item;
	filetype *ft;
	BuildMenuItems *menu_items;

	if (! DOC_IDX_VALID(idx)) return;

	ft = doc_list[idx].file_type;
	if (! ft) return;
	menu_items = build_get_menu_items(ft->id);

	switch (key_id)
	{
		case GEANY_KEYS_BUILD_COMPILE:
		item = menu_items->item_compile;
		break;

		case GEANY_KEYS_BUILD_LINK:
		item = menu_items->item_link;
		break;

		case GEANY_KEYS_BUILD_MAKE:
		item = menu_items->item_make_all;
		break;

		case GEANY_KEYS_BUILD_MAKEOWNTARGET:
		item = menu_items->item_make_custom;
		break;

		case GEANY_KEYS_BUILD_MAKEOBJECT:
		item = menu_items->item_make_object;
		break;

		case GEANY_KEYS_BUILD_NEXTERROR:
		item = menu_items->item_next_error;
		break;

		case GEANY_KEYS_BUILD_RUN:
		item = menu_items->item_exec;
		break;

		case GEANY_KEYS_BUILD_RUN2:
		item = menu_items->item_exec2;
		break;

		case GEANY_KEYS_BUILD_OPTIONS:
		item = menu_items->item_set_args;
		break;

		default:
		item = NULL;
	}
	if (item && GTK_WIDGET_IS_SENSITIVE(item))
		gtk_menu_item_activate(GTK_MENU_ITEM(item));
}

static void cb_func_reloadtaglist(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_update_tag_list(idx, TRUE);
}


// common function for keybindings using current word
static void cb_func_current_word(guint key_id)
{
	gint idx = document_get_cur_idx();
	gint pos;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	pos = sci_get_current_position(doc_list[idx].sci);

	sci_cb_find_current_word(doc_list[idx].sci, pos,
		editor_info.current_word, GEANY_MAX_WORD_LENGTH, NULL);

	if (*editor_info.current_word == 0)
		utils_beep();
	else
		switch (key_id)
		{
			case GEANY_KEYS_POPUP_FINDUSAGE:
			on_find_usage1_activate(NULL, NULL);
			break;
			case GEANY_KEYS_POPUP_GOTOTAGDEFINITION:
			on_goto_tag_activate(GTK_MENU_ITEM(lookup_widget(app->popup_menu,
				"goto_tag_definition1")), NULL);
			break;
			case GEANY_KEYS_POPUP_GOTOTAGDECLARATION:
			on_goto_tag_activate(GTK_MENU_ITEM(lookup_widget(app->popup_menu,
				"goto_tag_declaration1")), NULL);
			break;
		}
}


static void cb_func_switch_editor(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
}

static void cb_func_switch_scribble(G_GNUC_UNUSED guint key_id)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_SCRATCH);
	msgwin_show_hide(TRUE);
	gtk_widget_grab_focus(lookup_widget(app->window, "textview_scribble"));
}

static void cb_func_switch_vte(G_GNUC_UNUSED guint key_id)
{
#ifdef HAVE_VTE
	msgwin_show_hide(TRUE);
	/* the msgwin must be visible before we switch to the VTE page so that
	 * the font settings are applied on realization */
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_VTE);
	gtk_widget_grab_focus(vc->vte);
#endif
}

static void cb_func_switch_tableft(G_GNUC_UNUSED guint key_id)
{
	utils_switch_document(LEFT);
}

static void cb_func_switch_tabright(G_GNUC_UNUSED guint key_id)
{
	utils_switch_document(RIGHT);
}

static void cb_func_switch_tablastused(G_GNUC_UNUSED guint key_id)
{
	gint last_doc_idx = callbacks_data.last_doc_idx;

	if (DOC_IDX_VALID(last_doc_idx))
		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook),
			document_get_notebook_page(last_doc_idx));
}

static void cb_func_toggle_sidebar(G_GNUC_UNUSED guint key_id)
{
	static gint active_page = -1;

	if (app->sidebar_visible)
	{
		// to remember the active page because GTK (e.g. 2.8.18) doesn't do it and shows always
		// the last page (for unknown reason, with GTK 2.6.4 it works)
		active_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->treeview_notebook));
	}

	app->sidebar_visible = ! app->sidebar_visible;

	if ((! app->sidebar_openfiles_visible && ! app->sidebar_symbol_visible))
	{
		app->sidebar_openfiles_visible = TRUE;
		app->sidebar_symbol_visible = TRUE;
	}

	ui_treeviews_show_hide(TRUE);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(app->treeview_notebook), active_page);
}


static void goto_matching_brace(gint idx)
{
	gint pos, new_pos;

	if (! DOC_IDX_VALID(idx)) return;

	pos = sci_get_current_position(doc_list[idx].sci);
	if (! utils_isbrace(sci_get_char_at(doc_list[idx].sci, pos)))
		pos--; // set pos to the brace

	new_pos = sci_find_bracematch(doc_list[idx].sci, pos);
	if (new_pos != -1)
	{
		sci_goto_pos(doc_list[idx].sci, new_pos, TRUE); // set the cursor at the brace
		doc_list[idx].scroll_percent = 0.5F;
	}
}


// common function for editing keybindings, only valid when scintilla has focus.
static void cb_func_edit(guint key_id)
{
	gint idx = document_get_cur_idx();
	gint cur_line;
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	// edit keybindings only valid when scintilla widget has focus
	if (! DOC_IDX_VALID(idx) || focusw != GTK_WIDGET(doc_list[idx].sci)) return;

	cur_line = sci_get_current_line(doc_list[idx].sci, -1);

	switch (key_id)
	{
		case GEANY_KEYS_EDIT_DUPLICATELINE:
			on_menu_duplicate_line1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_COMMENTLINETOGGLE:
			on_menu_toggle_line_commentation1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_COMMENTLINE:
			on_menu_comment_line1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_UNCOMMENTLINE:
			on_menu_uncomment_line1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_AUTOCOMPLETE:
			sci_cb_start_auto_complete(idx, sci_get_current_position(doc_list[idx].sci), TRUE);
			break;
		case GEANY_KEYS_EDIT_CALLTIP:
			sci_cb_show_calltip(idx, -1);
			break;
		case GEANY_KEYS_EDIT_MACROLIST:
			sci_cb_show_macro_list(doc_list[idx].sci);
			break;
		case GEANY_KEYS_EDIT_SUPPRESSCOMPLETION:
			sci_add_text(doc_list[idx].sci, " ");
			break;
		case GEANY_KEYS_EDIT_SELECTWORD:
			sci_cb_select_word(doc_list[idx].sci);
			break;
		case GEANY_KEYS_EDIT_INCREASEINDENT:
			on_menu_increase_indent1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_DECREASEINDENT:
			on_menu_decrease_indent1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_GOTOMATCHINGBRACE:
			goto_matching_brace(idx);
			break;
		case GEANY_KEYS_EDIT_TOGGLEMARKER:
		{
			gboolean set = sci_is_marker_set_at_line(doc_list[idx].sci, cur_line, 1);

			sci_set_marker_at_line(doc_list[idx].sci, cur_line, ! set, 1);
			break;
		}
		case GEANY_KEYS_EDIT_GOTONEXTMARKER:
		{
			gint mline = sci_marker_next(doc_list[idx].sci, cur_line + 1, 1 << 1, TRUE);

			if (mline != -1)
			{
				sci_goto_line(doc_list[idx].sci, mline, TRUE);
				doc_list[idx].scroll_percent = 0.5F;
			}
			break;
		}
		case GEANY_KEYS_EDIT_GOTOPREVIOUSMARKER:
		{
			gint mline = sci_marker_previous(doc_list[idx].sci, cur_line - 1, 1 << 1, TRUE);

			if (mline != -1)
			{
				sci_goto_line(doc_list[idx].sci, mline, TRUE);
				doc_list[idx].scroll_percent = 0.5F;
			}
			break;
		}
		case GEANY_KEYS_EDIT_TOLOWERCASE:
			on_to_lower_case1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_TOUPPERCASE:
			on_to_upper_case1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDIT_SENDTOCMD1:
			if (app->custom_commands && g_strv_length(app->custom_commands) > 0)
				tools_execute_custom_command(idx, app->custom_commands[0]);
			break;
		case GEANY_KEYS_EDIT_SENDTOCMD2:
			if (app->custom_commands && g_strv_length(app->custom_commands) > 1)
				tools_execute_custom_command(idx, app->custom_commands[1]);
			break;
		case GEANY_KEYS_EDIT_SENDTOCMD3:
			if (app->custom_commands && g_strv_length(app->custom_commands) > 2)
				tools_execute_custom_command(idx, app->custom_commands[2]);
			break;
	}
}


static void cb_func_menu_replacetabs(G_GNUC_UNUSED guint key_id)
{
	on_replace_tabs_activate(NULL, NULL);
}

static void cb_func_menu_print(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_print(idx);
}

static void cb_func_menu_insert_date(G_GNUC_UNUSED guint key_id)
{
	gtk_menu_item_activate(GTK_MENU_ITEM(lookup_widget(app->popup_menu, "insert_date_custom2")));
}

static void cb_func_menu_insert_specialchars(G_GNUC_UNUSED guint key_id)
{
	on_menu_insert_special_chars1_activate(NULL, NULL);
}

