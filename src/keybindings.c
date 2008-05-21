/*
 *      keybindings.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
#include "filetypes.h"
#include "callbacks.h"
#include "prefs.h"
#include "msgwindow.h"
#include "editor.h"
#include "sciwrappers.h"
#include "build.h"
#include "tools.h"
#include "navqueue.h"
#include "symbols.h"
#include "vte.h"


GPtrArray *keybinding_groups;	/* array of KeyBindingGroup pointers */

/* keyfile group name for non-plugin KB groups */
const gchar keybindings_keyfile_group_name[] = "Bindings";

static GtkAccelGroup *kb_accel_group = NULL;
static const gboolean swap_alt_tab_order = FALSE;


static gboolean check_current_word(void);

static void cb_func_file_action(guint key_id);
static void cb_func_project_action(guint key_id);
static void cb_func_editor_action(guint key_id);
static void cb_func_select_action(guint key_id);
static void cb_func_format_action(guint key_id);
static void cb_func_insert_action(guint key_id);
static void cb_func_search_action(guint key_id);
static void cb_func_goto_action(guint key_id);
static void cb_func_clipboard(guint key_id);
static void cb_func_build_action(guint key_id);

/* TODO: refactor individual callbacks per group */
static void cb_func_menu_help(guint key_id);
static void cb_func_menu_preferences(guint key_id);

static void cb_func_menu_toggle_all(guint key_id);
static void cb_func_menu_fullscreen(guint key_id);
static void cb_func_menu_messagewindow(guint key_id);
static void cb_func_menu_zoomin(guint key_id);
static void cb_func_menu_zoomout(guint key_id);

static void cb_func_menu_replacetabs(guint key_id);
static void cb_func_menu_foldall(guint key_id);
static void cb_func_menu_unfoldall(guint key_id);
static void cb_func_reloadtaglist(guint key_id);

static void cb_func_menu_opencolorchooser(guint key_id);

static void cb_func_switch_editor(guint key_id);
static void cb_func_switch_scribble(guint key_id);
static void cb_func_switch_vte(guint key_id);
static void cb_func_switch_search_bar(guint key_id);
static void cb_func_switch_sidebar(guint key_id);
static void cb_func_switch_tableft(guint key_id);
static void cb_func_switch_tabright(guint key_id);
static void cb_func_switch_tablastused(guint key_id);
static void cb_func_move_tab(guint key_id);
static void cb_func_toggle_sidebar(guint key_id);

static void add_popup_menu_accels(void);
static void apply_kb_accel(KeyBinding *kb);


/** Simple convenience function to fill a KeyBinding struct item */
void keybindings_set_item(KeyBindingGroup *group, gsize key_id,
		KeyCallback callback, guint key, GdkModifierType mod,
		gchar *name, gchar *label, GtkWidget *menu_item)
{
	KeyBinding *kb;

	g_assert(key_id < group->count);

	kb = &group->keys[key_id];

	kb->name = name;
	kb->label = label;
	kb->key = key;
	kb->mods = mod;
	kb->callback = callback;
	kb->menu_item = menu_item;

	apply_kb_accel(kb);
}


static KeyBindingGroup *add_kb_group(KeyBindingGroup *group,
		const gchar *name, const gchar *label, gsize count, KeyBinding *keys)
{
	g_ptr_array_add(keybinding_groups, group);

	group->name = name;
	group->label = label;
	group->count = count;
	group->keys = keys;
	return group;
}


/* Lookup a widget in the main window */
#define LW(widget_name) \
	lookup_widget(app->window, G_STRINGIFY(widget_name))

/* Expansion for group_id = FILE:
 * static KeyBinding FILE_keys[GEANY_KEYS_FILE_COUNT]; */
#define DECLARE_KEYS(group_id) \
	static KeyBinding group_id ## _keys[GEANY_KEYS_ ## group_id ## _COUNT]

/* Expansion for group_id = FILE:
 * add_kb_group(&groups[GEANY_KEY_GROUP_FILE], NULL, _("File menu"),
 * 	GEANY_KEYS_FILE_COUNT, FILE_keys); */
#define ADD_KB_GROUP(group_id, label) \
	add_kb_group(&groups[GEANY_KEY_GROUP_ ## group_id], keybindings_keyfile_group_name, label, \
		GEANY_KEYS_ ## group_id ## _COUNT, group_id ## _keys)

/* Init all fields of keys with default values.
 * The menu_item field is always the main menu item, popup menu accelerators are
 * set in add_popup_menu_accels(). */
static void init_default_kb(void)
{
	static KeyBindingGroup groups[GEANY_KEY_GROUP_COUNT];
	KeyBindingGroup *group;
	DECLARE_KEYS(FILE);
	DECLARE_KEYS(PROJECT);
	DECLARE_KEYS(EDITOR);
	DECLARE_KEYS(CLIPBOARD);
	DECLARE_KEYS(SELECT);
	DECLARE_KEYS(FORMAT);
	DECLARE_KEYS(INSERT);
	DECLARE_KEYS(SETTINGS);
	DECLARE_KEYS(SEARCH);
	DECLARE_KEYS(GOTO);
	DECLARE_KEYS(VIEW);
	DECLARE_KEYS(FOCUS);
	DECLARE_KEYS(NOTEBOOK);
	DECLARE_KEYS(DOCUMENT);
	DECLARE_KEYS(BUILD);
	DECLARE_KEYS(TOOLS);
	DECLARE_KEYS(HELP);

	group = ADD_KB_GROUP(FILE, _("File"));

	keybindings_set_item(group, GEANY_KEYS_FILE_NEW, cb_func_file_action,
		GDK_n, GDK_CONTROL_MASK, "menu_new", _("New"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FILE_OPEN, cb_func_file_action,
		GDK_o, GDK_CONTROL_MASK, "menu_open", _("Open"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FILE_OPENSELECTED, cb_func_file_action,
		GDK_o, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "menu_open_selected",
		_("Open selected file"), LW(menu_open_selected_file1));
	keybindings_set_item(group, GEANY_KEYS_FILE_SAVE, cb_func_file_action,
		GDK_s, GDK_CONTROL_MASK, "menu_save", _("Save"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FILE_SAVEAS, cb_func_file_action,
		0, 0, "menu_saveas", _("Save as"), LW(menu_save_as1));
	keybindings_set_item(group, GEANY_KEYS_FILE_SAVEALL, cb_func_file_action,
		GDK_S, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "menu_saveall", _("Save all"),
		LW(menu_save_all1));
	keybindings_set_item(group, GEANY_KEYS_FILE_PRINT, cb_func_file_action,
		GDK_p, GDK_CONTROL_MASK, "menu_print", _("Print"), LW(print1));
	keybindings_set_item(group, GEANY_KEYS_FILE_CLOSE, cb_func_file_action,
		GDK_w, GDK_CONTROL_MASK, "menu_close", _("Close"), LW(menu_close1));
	keybindings_set_item(group, GEANY_KEYS_FILE_CLOSEALL, cb_func_file_action,
		GDK_w, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "menu_closeall", _("Close all"),
		LW(menu_close_all1));
	keybindings_set_item(group, GEANY_KEYS_FILE_RELOAD, cb_func_file_action,
		GDK_r, GDK_CONTROL_MASK, "menu_reloadfile", _("Reload file"), LW(menu_reload1));

	group = ADD_KB_GROUP(PROJECT, _("Project"));

	keybindings_set_item(group, GEANY_KEYS_PROJECT_PROPERTIES, cb_func_project_action,
		0, 0, "project_properties", _("Project properties"), LW(project_properties1));

	group = ADD_KB_GROUP(EDITOR, _("Editor"));

	keybindings_set_item(group, GEANY_KEYS_EDITOR_UNDO, cb_func_editor_action,
		GDK_z, GDK_CONTROL_MASK, "menu_undo", _("Undo"), LW(menu_undo2));
	keybindings_set_item(group, GEANY_KEYS_EDITOR_REDO, cb_func_editor_action,
		GDK_y, GDK_CONTROL_MASK, "menu_redo", _("Redo"), LW(menu_redo2));
	keybindings_set_item(group, GEANY_KEYS_EDITOR_DUPLICATELINE, cb_func_editor_action,
		GDK_d, GDK_CONTROL_MASK, "edit_duplicateline", _("Duplicate line or selection"),
		LW(menu_duplicate_line1));
	keybindings_set_item(group, GEANY_KEYS_EDITOR_DELETELINE, cb_func_editor_action,
		GDK_k, GDK_CONTROL_MASK, "edit_deleteline", _("Delete current line(s)"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_TRANSPOSELINE, cb_func_editor_action,
		GDK_t, GDK_CONTROL_MASK, "edit_transposeline", _("Transpose current line"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_SCROLLTOLINE, cb_func_editor_action,
		GDK_l, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "edit_scrolltoline", _("Scroll to current line"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_SCROLLLINEUP, cb_func_editor_action,
		GDK_Up, GDK_MOD1_MASK, "edit_scrolllineup", _("Scroll up the view by one line"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_SCROLLLINEDOWN, cb_func_editor_action,
		GDK_Down, GDK_MOD1_MASK, "edit_scrolllinedown", _("Scroll down the view by one line"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_COMPLETESNIPPET, NULL,	/* handled specially in check_snippet_completion() */
		GDK_Tab, 0, "edit_completesnippet", _("Complete snippet"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_SUPPRESSSNIPPETCOMPLETION, cb_func_editor_action,
		0, 0, "edit_suppresssnippetcompletion", _("Suppress snippet completion"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_CONTEXTACTION, cb_func_editor_action,
		0, 0, "popup_contextaction", _("Context Action"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_AUTOCOMPLETE, cb_func_editor_action,
		GDK_space, GDK_CONTROL_MASK, "edit_autocomplete", _("Complete word"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_CALLTIP, cb_func_editor_action,
		GDK_space, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "edit_calltip", _("Show calltip"), NULL);
	keybindings_set_item(group, GEANY_KEYS_EDITOR_MACROLIST, cb_func_editor_action,
		GDK_Return, GDK_CONTROL_MASK, "edit_macrolist", _("Show macro list"), NULL);

	group = ADD_KB_GROUP(CLIPBOARD, _("Clipboard"));

	keybindings_set_item(group, GEANY_KEYS_CLIPBOARD_CUT, cb_func_clipboard,
		GDK_x, GDK_CONTROL_MASK, "menu_cut", _("Cut"), NULL);
	keybindings_set_item(group, GEANY_KEYS_CLIPBOARD_COPY, cb_func_clipboard,
		GDK_c, GDK_CONTROL_MASK, "menu_copy", _("Copy"), NULL);
	keybindings_set_item(group, GEANY_KEYS_CLIPBOARD_PASTE, cb_func_clipboard,
		GDK_v, GDK_CONTROL_MASK, "menu_paste", _("Paste"), NULL);
	keybindings_set_item(group, GEANY_KEYS_CLIPBOARD_COPYLINE, cb_func_clipboard,
		GDK_c, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "edit_copyline", _("Copy current line(s)"), NULL);
	keybindings_set_item(group, GEANY_KEYS_CLIPBOARD_CUTLINE, cb_func_clipboard,
		GDK_x, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "edit_cutline", _("Cut current line(s)"), NULL);

	group = ADD_KB_GROUP(SELECT, _("Select"));

	keybindings_set_item(group, GEANY_KEYS_SELECT_ALL, cb_func_select_action,
		GDK_a, GDK_CONTROL_MASK, "menu_selectall", _("Select All"), LW(menu_select_all1));
	keybindings_set_item(group, GEANY_KEYS_SELECT_WORD, cb_func_select_action,
		GDK_w, GDK_SHIFT_MASK | GDK_MOD1_MASK, "edit_selectword", _("Select current word"), NULL);
	keybindings_set_item(group, GEANY_KEYS_SELECT_LINE, cb_func_select_action,
		GDK_l, GDK_SHIFT_MASK | GDK_MOD1_MASK, "edit_selectline", _("Select current line(s)"), NULL);
	keybindings_set_item(group, GEANY_KEYS_SELECT_PARAGRAPH, cb_func_select_action,
		GDK_p, GDK_SHIFT_MASK | GDK_MOD1_MASK, "edit_selectparagraph", _("Select current paragraph"), NULL);

	group = ADD_KB_GROUP(FORMAT, _("Format"));

	keybindings_set_item(group, GEANY_KEYS_FORMAT_TOGGLECASE, cb_func_format_action,
		GDK_u, GDK_CONTROL_MASK | GDK_MOD1_MASK, "edit_togglecase",
		_("Toggle Case of Selection"), LW(menu_toggle_case2));
	keybindings_set_item(group, GEANY_KEYS_FORMAT_COMMENTLINETOGGLE, cb_func_format_action,
		GDK_e, GDK_CONTROL_MASK, "edit_commentlinetoggle", _("Toggle line commentation"),
		LW(menu_toggle_line_commentation1));
	keybindings_set_item(group, GEANY_KEYS_FORMAT_COMMENTLINE, cb_func_format_action,
		0, 0, "edit_commentline", _("Comment line(s)"), LW(menu_comment_line1));
	keybindings_set_item(group, GEANY_KEYS_FORMAT_UNCOMMENTLINE, cb_func_format_action,
		0, 0, "edit_uncommentline", _("Uncomment line(s)"), LW(menu_uncomment_line1));
	keybindings_set_item(group, GEANY_KEYS_FORMAT_INCREASEINDENT, cb_func_format_action,
		GDK_i, GDK_CONTROL_MASK, "edit_increaseindent", _("Increase indent"),
		LW(menu_increase_indent1));
	keybindings_set_item(group, GEANY_KEYS_FORMAT_DECREASEINDENT, cb_func_format_action,
		GDK_u, GDK_CONTROL_MASK, "edit_decreaseindent", _("Decrease indent"),
		LW(menu_decrease_indent1));
	keybindings_set_item(group, GEANY_KEYS_FORMAT_INCREASEINDENTBYSPACE, cb_func_format_action,
		0, 0, "edit_increaseindentbyspace", _("Increase indent by one space"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FORMAT_DECREASEINDENTBYSPACE, cb_func_format_action,
		0, 0, "edit_decreaseindentbyspace", _("Decrease indent by one space"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FORMAT_AUTOINDENT, cb_func_format_action,
		0, 0, "edit_autoindent", _("Smart line indent"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FORMAT_SENDTOCMD1, cb_func_format_action,
		GDK_1, GDK_CONTROL_MASK, "edit_sendtocmd1", _("Send to Custom Command 1"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FORMAT_SENDTOCMD2, cb_func_format_action,
		GDK_2, GDK_CONTROL_MASK, "edit_sendtocmd2", _("Send to Custom Command 2"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FORMAT_SENDTOCMD3, cb_func_format_action,
		GDK_3, GDK_CONTROL_MASK, "edit_sendtocmd3", _("Send to Custom Command 3"), NULL);

	group = ADD_KB_GROUP(INSERT, _("Insert"));

	keybindings_set_item(group, GEANY_KEYS_INSERT_DATE, cb_func_insert_action,
		GDK_d, GDK_SHIFT_MASK | GDK_MOD1_MASK, "menu_insert_date", _("Insert date"),
		LW(insert_date_custom1));
	keybindings_set_item(group, GEANY_KEYS_INSERT_ALTWHITESPACE, cb_func_insert_action,
		0, 0, "edit_insertwhitespace", _("Insert alternative whitespace"), NULL);

	group = ADD_KB_GROUP(SETTINGS, _("Settings"));

	keybindings_set_item(group, GEANY_KEYS_SETTINGS_PREFERENCES, cb_func_menu_preferences,
		GDK_p, GDK_CONTROL_MASK | GDK_MOD1_MASK, "menu_preferences", _("Preferences"),
		LW(preferences1));

	group = ADD_KB_GROUP(SEARCH, _("Search"));

	keybindings_set_item(group, GEANY_KEYS_SEARCH_FIND, cb_func_search_action,
		GDK_f, GDK_CONTROL_MASK, "menu_find", _("Find"), LW(find1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDNEXT, cb_func_search_action,
		GDK_g, GDK_CONTROL_MASK, "menu_findnext", _("Find Next"), LW(find_next1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDPREVIOUS, cb_func_search_action,
		GDK_g, GDK_CONTROL_MASK | GDK_SHIFT_MASK, "menu_findprevious", _("Find Previous"),
		LW(find_previous1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDNEXTSEL, cb_func_search_action,
		0, 0, "menu_findnextsel", _("Find Next Selection"), LW(find_nextsel1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDPREVSEL, cb_func_search_action,
		0, 0, "menu_findprevsel", _("Find Previous Selection"), LW(find_prevsel1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_REPLACE, cb_func_search_action,
		GDK_h, GDK_CONTROL_MASK, "menu_replace", _("Replace"), LW(replace1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDINFILES, cb_func_search_action, GDK_f,
		GDK_CONTROL_MASK | GDK_SHIFT_MASK, "menu_findinfiles", _("Find in Files"),
		LW(find_in_files1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_NEXTMESSAGE, cb_func_search_action,
		0, 0, "menu_nextmessage", _("Next Message"), LW(next_message1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDUSAGE, cb_func_search_action,
		0, 0, "popup_findusage", _("Find Usage"), NULL);

	group = ADD_KB_GROUP(GOTO, _("Go to"));

	keybindings_set_item(group, GEANY_KEYS_GOTO_BACK, cb_func_goto_action,
		0, 0, "nav_back", _("Navigate back a location"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_FORWARD, cb_func_goto_action,
		0, 0, "nav_forward", _("Navigate forward a location"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_LINE, cb_func_goto_action,
		GDK_l, GDK_CONTROL_MASK, "menu_gotoline", _("Go to Line"), LW(go_to_line1));
	keybindings_set_item(group, GEANY_KEYS_GOTO_MATCHINGBRACE, cb_func_goto_action,
		GDK_b, GDK_CONTROL_MASK, "edit_gotomatchingbrace",
		_("Go to matching brace"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_TOGGLEMARKER, cb_func_goto_action,
		GDK_m, GDK_CONTROL_MASK, "edit_togglemarker",
		_("Toggle marker"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_NEXTMARKER, cb_func_goto_action,
		GDK_period, GDK_CONTROL_MASK, "edit_gotonextmarker",
		_("Go to next marker"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_PREVIOUSMARKER, cb_func_goto_action,
		GDK_comma, GDK_CONTROL_MASK, "edit_gotopreviousmarker",
		_("Go to previous marker"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_TAGDEFINITION, cb_func_goto_action,
		0, 0, "popup_gototagdefinition", _("Go to Tag Definition"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_TAGDECLARATION, cb_func_goto_action,
		0, 0, "popup_gototagdeclaration", _("Go to Tag Declaration"), NULL);

	group = ADD_KB_GROUP(VIEW, _("View"));

	keybindings_set_item(group, GEANY_KEYS_VIEW_TOGGLEALL, cb_func_menu_toggle_all,
		0, 0, "menu_toggleall", _("Toggle All Additional Widgets"),
		LW(menu_toggle_all_additional_widgets1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_FULLSCREEN, cb_func_menu_fullscreen,
		GDK_F11, 0, "menu_fullscreen", _("Fullscreen"), LW(menu_fullscreen1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_MESSAGEWINDOW, cb_func_menu_messagewindow,
		0, 0, "menu_messagewindow", _("Toggle Messages Window"),
		LW(menu_show_messages_window1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_SIDEBAR, cb_func_toggle_sidebar,
		0, 0, "toggle_sidebar", _("Toggle Sidebar"), LW(menu_show_sidebar1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_ZOOMIN, cb_func_menu_zoomin,
		GDK_plus, GDK_CONTROL_MASK, "menu_zoomin", _("Zoom In"), LW(menu_zoom_in1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_ZOOMOUT, cb_func_menu_zoomout,
		GDK_minus, GDK_CONTROL_MASK, "menu_zoomout", _("Zoom Out"), LW(menu_zoom_out1));

	group = ADD_KB_GROUP(FOCUS, _("Focus"));

	keybindings_set_item(group, GEANY_KEYS_FOCUS_EDITOR, cb_func_switch_editor,
		GDK_F2, 0, "switch_editor", _("Switch to Editor"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_SCRIBBLE, cb_func_switch_scribble,
		GDK_F6, 0, "switch_scribble", _("Switch to Scribble"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_VTE, cb_func_switch_vte,
		GDK_F4, 0, "switch_vte", _("Switch to VTE"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_SEARCHBAR, cb_func_switch_search_bar,
		GDK_F7, 0, "switch_search_bar", _("Switch to Search Bar"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_SIDEBAR, cb_func_switch_sidebar,
		0, 0, "switch_sidebar", _("Switch to Sidebar"), NULL);

	group = ADD_KB_GROUP(NOTEBOOK, _("Notebook tab"));

	keybindings_set_item(group, GEANY_KEYS_NOTEBOOK_SWITCHTABLEFT, cb_func_switch_tableft,
		GDK_Page_Up, GDK_CONTROL_MASK, "switch_tableft", _("Switch to left document"), NULL);
	keybindings_set_item(group, GEANY_KEYS_NOTEBOOK_SWITCHTABRIGHT, cb_func_switch_tabright,
		GDK_Page_Down, GDK_CONTROL_MASK, "switch_tabright", _("Switch to right document"), NULL);
	keybindings_set_item(group, GEANY_KEYS_NOTEBOOK_SWITCHTABLASTUSED, cb_func_switch_tablastused,
		GDK_Tab, GDK_CONTROL_MASK, "switch_tablastused", _("Switch to last used document"), NULL);
	keybindings_set_item(group, GEANY_KEYS_NOTEBOOK_MOVETABLEFT, cb_func_move_tab,
		GDK_Page_Up, GDK_MOD1_MASK, "move_tableft", _("Move document left"), NULL);
	keybindings_set_item(group, GEANY_KEYS_NOTEBOOK_MOVETABRIGHT, cb_func_move_tab,
		GDK_Page_Down, GDK_MOD1_MASK, "move_tabright", _("Move document right"), NULL);
	keybindings_set_item(group, GEANY_KEYS_NOTEBOOK_MOVETABFIRST, cb_func_move_tab,
		0, 0, "move_tabfirst", _("Move document first"), NULL);
	keybindings_set_item(group, GEANY_KEYS_NOTEBOOK_MOVETABLAST, cb_func_move_tab,
		0, 0, "move_tablast", _("Move document last"), NULL);

	group = ADD_KB_GROUP(DOCUMENT, _("Document"));

	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_REPLACETABS, cb_func_menu_replacetabs,
		0, 0, "menu_replacetabs", _("Replace tabs by space"), LW(menu_replace_tabs));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_FOLDALL, cb_func_menu_foldall,
		0, 0, "menu_foldall", _("Fold all"), LW(menu_fold_all1));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_UNFOLDALL, cb_func_menu_unfoldall,
		0, 0, "menu_unfoldall", _("Unfold all"), LW(menu_unfold_all1));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_RELOADTAGLIST, cb_func_reloadtaglist,
		GDK_r, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "reloadtaglist", _("Reload symbol list"), NULL);

	group = ADD_KB_GROUP(BUILD, _("Build"));

	keybindings_set_item(group, GEANY_KEYS_BUILD_COMPILE, cb_func_build_action,
		GDK_F8, 0, "build_compile", _("Compile"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_LINK, cb_func_build_action,
		GDK_F9, 0, "build_link", _("Build"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_MAKE, cb_func_build_action,
		GDK_F9, GDK_SHIFT_MASK, "build_make", _("Make all"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_MAKEOWNTARGET, cb_func_build_action,
		GDK_F9, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "build_makeowntarget",
		_("Make custom target"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_MAKEOBJECT, cb_func_build_action,
		0, 0, "build_makeobject", _("Make object"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_NEXTERROR, cb_func_build_action,
		0, 0, "build_nexterror", _("Next error"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_RUN, cb_func_build_action,
		GDK_F5, 0, "build_run", _("Run"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_RUN2, cb_func_build_action,
		0, 0, "build_run2", _("Run (alternative command)"), NULL);
	keybindings_set_item(group, GEANY_KEYS_BUILD_OPTIONS, cb_func_build_action,
		0, 0, "build_options", _("Build options"), NULL);

	group = ADD_KB_GROUP(TOOLS, _("Tools"));

	keybindings_set_item(group, GEANY_KEYS_TOOLS_OPENCOLORCHOOSER, cb_func_menu_opencolorchooser,
		0, 0, "menu_opencolorchooser", _("Show Color Chooser"), LW(menu_choose_color1));

	group = ADD_KB_GROUP(HELP, _("Help"));

	keybindings_set_item(group, GEANY_KEYS_HELP_HELP, cb_func_menu_help,
		GDK_F1, 0, "menu_help", _("Help"), LW(help1));
}


void keybindings_init(void)
{
	keybinding_groups = g_ptr_array_sized_new(GEANY_KEY_GROUP_COUNT);

	kb_accel_group = gtk_accel_group_new();

	init_default_kb();

	gtk_window_add_accel_group(GTK_WINDOW(app->window), kb_accel_group);
}


typedef void (*KBItemCallback) (KeyBindingGroup *group, KeyBinding *kb, gpointer user_data);

static void keybindings_foreach(KBItemCallback cb, gpointer user_data)
{
	gsize g, i;

	for (g = 0; g < keybinding_groups->len; g++)
	{
		KeyBindingGroup *group = g_ptr_array_index(keybinding_groups, g);

		for (i = 0; i < group->count; i++)
		{
			KeyBinding *kb = &group->keys[i];

			cb(group, kb, user_data);
		}
	}
}


static void apply_kb_accel(KeyBinding *kb)
{
	if (kb->key != 0 && kb->menu_item)
		gtk_widget_add_accelerator(kb->menu_item, "activate", kb_accel_group,
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
}


static void load_kb(KeyBindingGroup *group, KeyBinding *kb, gpointer user_data)
{
	GKeyFile *config = user_data;
	gchar *val;
	guint key;
	GdkModifierType mods;

	val = g_key_file_get_string(config, group->name, kb->name, NULL);
	if (val != NULL)
	{
		gtk_accelerator_parse(val, &key, &mods);
		kb->key = key;
		kb->mods = mods;

		apply_kb_accel(kb);
	}
	g_free(val);
}


static void load_user_kb(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "keybindings.conf", NULL);
	GKeyFile *config = g_key_file_new();

	/* now load user defined keys */
	if (g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		keybindings_foreach(load_kb, config);
	}
	g_free(configfile);
	g_key_file_free(config);
}


void keybindings_load_keyfile(void)
{
	load_user_kb();
	add_popup_menu_accels();
}


static void add_menu_accel(KeyBindingGroup *group, guint kb_id,
	GtkAccelGroup *accel_group, GtkWidget *menuitem)
{
	KeyBinding *kb = &group->keys[kb_id];

	if (kb->key != 0)
		gtk_widget_add_accelerator(menuitem, "activate", accel_group,
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
}


#define GEANY_ADD_POPUP_ACCEL(kb_id, wid) \
	add_menu_accel(group, kb_id, accel_group, lookup_widget(app->popup_menu, G_STRINGIFY(wid)))

/* set the menu item accelerator shortcuts (just for visibility, they are handled anyway) */
static void add_popup_menu_accels(void)
{
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	KeyBindingGroup *group;

	group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_EDITOR);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDITOR_UNDO, undo1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDITOR_REDO, redo1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDITOR_DUPLICATELINE, menu_duplicate_line2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_EDITOR_CONTEXTACTION, context_action1);

	group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_SELECT);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_SELECT_ALL, menu_select_all2);

	group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_INSERT);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_INSERT_DATE, insert_date_custom2);

	group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_FILE);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_FILE_OPENSELECTED, menu_open_selected_file2);

	group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_SEARCH);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_SEARCH_FINDUSAGE, find_usage1);

	group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_GOTO);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_GOTO_LINE, go_to_line);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_GOTO_TAGDEFINITION, goto_tag_definition1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_GOTO_TAGDECLARATION, goto_tag_declaration1);

	group = g_ptr_array_index(keybinding_groups, GEANY_KEY_GROUP_FORMAT);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_FORMAT_TOGGLECASE, toggle_case1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_FORMAT_COMMENTLINE, menu_comment_line2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_FORMAT_UNCOMMENTLINE, menu_uncomment_line2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_FORMAT_COMMENTLINETOGGLE, menu_toggle_line_commentation2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_FORMAT_INCREASEINDENT, menu_increase_indent2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_FORMAT_DECREASEINDENT, menu_decrease_indent2);

	/* the build menu items are set if the build menus are created */

	gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);
}


static void set_keyfile_kb(KeyBindingGroup *group, KeyBinding *kb, gpointer user_data)
{
	GKeyFile *config = user_data;
	gchar *val;

	val = gtk_accelerator_name(kb->key, kb->mods);
	g_key_file_set_string(config, group->name, kb->name, val);
	g_free(val);
}


/* just write the content of the keys array to the config file */
void keybindings_write_to_file(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "keybindings.conf", NULL);
	gchar *data;
	GKeyFile *config = g_key_file_new();

 	/* add comment if the file is newly created */
	if (! g_key_file_load_from_file(config, configfile, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		g_key_file_set_comment(config, NULL, NULL, "Keybindings for Geany\nThe format looks like \"<Control>a\" or \"<Shift><Alt>F1\".\nBut you can also change the keys in Geany's preferences dialog.", NULL);
	}

	keybindings_foreach(set_keyfile_kb, config);

	/* write the file */
	data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(configfile, data);

	g_free(data);
	g_free(configfile);
	g_key_file_free(config);
}


void keybindings_free(void)
{
	g_ptr_array_free(keybinding_groups, TRUE);
}


static void get_shortcut_labels_text(GString **text_names_str, GString **text_keys_str)
{
	gsize g, i;
	GString *text_names = g_string_sized_new(600);
	GString *text_keys = g_string_sized_new(600);

	*text_names_str = text_names;
	*text_keys_str = text_keys;

	for (g = 0; g < keybinding_groups->len; g++)
	{
		KeyBindingGroup *group = g_ptr_array_index(keybinding_groups, g);

		if (g == 0)
		{
			g_string_append_printf(text_names, "<b>%s</b>\n", group->label);
			g_string_append(text_keys, "\n");
		}
		else
		{
			g_string_append_printf(text_names, "\n<b>%s</b>\n", group->label);
			g_string_append(text_keys, "\n\n");
		}

		for (i = 0; i < group->count; i++)
		{
			KeyBinding *kb = &group->keys[i];
			gchar *shortcut;

			shortcut = gtk_accelerator_get_label(kb->key, kb->mods);
			g_string_append(text_names, kb->label);
			g_string_append(text_names, "\n");
			g_string_append(text_keys, shortcut);
			g_string_append(text_keys, "\n");
			g_free(shortcut);
		}
	}
}


void keybindings_show_shortcuts(void)
{
	GtkWidget *dialog, *hbox, *label1, *label2, *label3, *swin, *vbox;
	GString *text_names;
	GString *text_keys;
	gint height, response;

	dialog = gtk_dialog_new_with_buttons(_("Keyboard Shortcuts"), GTK_WINDOW(app->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_EDIT, GTK_RESPONSE_APPLY,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_widget_set_name(dialog, "GeanyDialog");

	height = GEANY_WINDOW_MINIMAL_HEIGHT;
	gtk_window_set_default_size(GTK_WINDOW(dialog), -1, height);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

	label3 = gtk_label_new(_("The following keyboard shortcuts are configurable:"));
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 6);

	label1 = gtk_label_new(NULL);

	label2 = gtk_label_new(NULL);

	get_shortcut_labels_text(&text_names, &text_keys);

	gtk_label_set_markup(GTK_LABEL(label1), text_names->str);
	gtk_label_set_text(GTK_LABEL(label2), text_keys->str);

	g_string_free(text_names, TRUE);
	g_string_free(text_keys, TRUE);

	gtk_container_add(GTK_CONTAINER(hbox), label1);
	gtk_container_add(GTK_CONTAINER(hbox), label2);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER,
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
		/* select the KB page */
		wid = lookup_widget(ui_widgets.prefs_dialog, "frame22");
		if (wid != NULL)
		{
			GtkNotebook *nb = GTK_NOTEBOOK(lookup_widget(ui_widgets.prefs_dialog, "notebook2"));

			if (nb != NULL)
				gtk_notebook_set_current_page(nb, gtk_notebook_page_num(nb, wid));
		}
	}

	gtk_widget_destroy(dialog);
}


static gboolean check_fixed_kb(guint keyval, guint state)
{
	/* check alt-0 to alt-9 for setting current notebook page */
	if (state & GDK_MOD1_MASK && keyval >= GDK_0 && keyval <= GDK_9)
	{
		gint page = keyval - GDK_0 - 1;
		gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

		/* alt-0 is for the rightmost tab */
		if (keyval == GDK_0)
			page = npages - 1;
		/* invert the order if tabs are added on the other side */
		if (swap_alt_tab_order && ! file_prefs.tab_order_ltr)
			page = (npages - 1) - page;

		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), page);
		return TRUE;
	}
	if (keyval == GDK_Page_Up || keyval == GDK_Page_Down)
	{
		/* switch to first or last document */
		if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
		{
			if (keyval == GDK_Page_Up)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), 0);
			if (keyval == GDK_Page_Down)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook),
					gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) - 1);
			return TRUE;
		}
	}
	return FALSE;
}


/* We have a special case for GEANY_KEYS_EDIT_COMPLETESNIPPET, because we need to
 * return FALSE if no completion occurs, so the tab or space is handled normally. */
static gboolean check_snippet_completion(guint keyval, guint state)
{
	KeyBinding *kb = keybindings_lookup_item(GEANY_KEY_GROUP_EDITOR,
		GEANY_KEYS_EDITOR_COMPLETESNIPPET);

	if (kb->key == keyval && kb->mods == state)
	{
		gint idx = document_get_cur_idx();
		GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

		/* keybinding only valid when scintilla widget has focus */
		if (DOC_IDX_VALID(idx) && focusw == GTK_WIDGET(doc_list[idx].sci))
		{
			ScintillaObject *sci = doc_list[idx].sci;
			gint pos = sci_get_current_position(sci);

			if (editor_prefs.complete_snippets)
				return editor_complete_snippet(idx, pos);
		}
	}
	return FALSE;
}


#ifdef HAVE_VTE
static gboolean set_sensitive(gpointer widget)
{
	gtk_widget_set_sensitive(GTK_WIDGET(widget), TRUE);
	return FALSE;
}


static gboolean check_vte(GdkModifierType state, guint keyval)
{
	guint i;
	GtkWidget *widget;

	if (! vc->enable_bash_keys)
		return FALSE;
	if (gtk_window_get_focus(GTK_WINDOW(app->window)) != vc->vte)
		return FALSE;
	/* prevent menubar flickering: */
	if (state == GDK_SHIFT_MASK && (keyval >= GDK_a && keyval <= GDK_z))
		return FALSE;
	if (state == 0 && (keyval < GDK_F1 || keyval > GDK_F35))	/* e.g. backspace */
		return FALSE;

	/* make focus commands override any bash commands */
	for (i = 0; i < GEANY_KEYS_FOCUS_COUNT; i++)
	{
		KeyBinding *kb = keybindings_lookup_item(GEANY_KEY_GROUP_FOCUS, i);

		if (state == kb->mods && keyval == kb->key)
			return FALSE;
	}

	/* Temporarily disable the menus to prevent conflicting menu accelerators
	 * from overriding the VTE bash shortcuts.
	 * Ideally we would just somehow disable the menubar without redrawing it,
	 * but maybe that's not possible. */
	widget = lookup_widget(app->window, "menubar1");
	gtk_widget_set_sensitive(widget, FALSE);
	g_idle_add(&set_sensitive, (gpointer) widget);
	widget = app->popup_menu;
	gtk_widget_set_sensitive(widget, FALSE);
	g_idle_add(&set_sensitive, (gpointer) widget);
	return TRUE;
}
#endif


static void check_disk_status(void)
{
	gint idx = document_get_cur_idx();

	if (DOC_IDX_VALID(idx))
	{
		utils_check_disk_status(idx, FALSE);
	}
}


/* central keypress event handler, almost all keypress events go to this function */
gboolean keybindings_got_event(GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
{
	guint state, keyval;
	gsize g, i;

	if (ev->keyval == 0)
		return FALSE;

	check_disk_status();

	keyval = ev->keyval;
    state = ev->state & GEANY_KEYS_MODIFIER_MASK;

	/* hack to get around that CTRL+Shift+r results in GDK_R not GDK_r */
	if ((ev->state & GDK_SHIFT_MASK) || (ev->state & GDK_LOCK_MASK))
		if (keyval >= GDK_A && keyval <= GDK_Z)
			keyval += GDK_a - GDK_A;

	/*geany_debug("%d (%d) %d (%d)", keyval, ev->keyval, state, ev->state);*/

	/* special cases */
#ifdef HAVE_VTE
	if (vte_info.have_vte && check_vte(state, keyval))
		return FALSE;
#endif
	if (check_snippet_completion(keyval, state))
		return TRUE;

	for (g = 0; g < keybinding_groups->len; g++)
	{
		KeyBindingGroup *group = g_ptr_array_index(keybinding_groups, g);

		for (i = 0; i < group->count; i++)
		{
			KeyBinding *kb = &group->keys[i];

			if (keyval == kb->key && state == kb->mods)
			{
				if (kb->callback == NULL)
					return FALSE;	/* ignore the keybinding */

				/* call the corresponding callback function for this shortcut */
				kb->callback(i);
				return TRUE;
			}
		}
	}
	/* fixed keybindings can be overridden by user bindings, so check them last */
	if (check_fixed_kb(keyval, state))
		return TRUE;
	return FALSE;
}


KeyBinding *keybindings_lookup_item(guint group_id, guint key_id)
{
	KeyBindingGroup *group;

	g_return_val_if_fail(group_id < keybinding_groups->len, NULL);

	group = g_ptr_array_index(keybinding_groups, group_id);

	g_return_val_if_fail(group, NULL);
	g_return_val_if_fail(key_id < group->count, NULL);

	return &group->keys[key_id];
}


/** Mimic a (built-in only) keybinding action.
 * 	Example: @code keybindings_send_command(GEANY_KEY_GROUP_FILE, GEANY_KEYS_FILE_OPEN); @endcode
 * 	@param group_id The index for the key group that contains the @a key_id keybinding.
 * 	@param key_id The keybinding command index. */
void keybindings_send_command(guint group_id, guint key_id)
{
	KeyBinding *kb;

	g_return_if_fail(group_id < GEANY_KEY_GROUP_COUNT);	/* can't use this for plugin groups */

	kb = keybindings_lookup_item(group_id, key_id);
	if (kb)
		kb->callback(key_id);
}


/* These are the callback functions, either each group or each shortcut has it's
 * own function. */


static void cb_func_file_action(guint key_id)
{
	switch (key_id)
	{
		case GEANY_KEYS_FILE_NEW:
			document_new_file(NULL, NULL, NULL);
			break;
		case GEANY_KEYS_FILE_OPEN:
			on_open1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_OPENSELECTED:
			on_menu_open_selected_file1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_SAVE:
			on_save1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_SAVEAS:
			on_save_as1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_SAVEALL:
			on_save_all1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_CLOSE:
			on_close1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_CLOSEALL:
			on_close_all1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_RELOAD:
			on_toolbutton23_clicked(NULL, NULL);
			break;
		case GEANY_KEYS_FILE_PRINT:
			on_print1_activate(NULL, NULL);
			break;
	}
}


static void cb_func_project_action(guint key_id)
{
	switch (key_id)
	{
		case GEANY_KEYS_PROJECT_PROPERTIES:
			if (app->project)
				on_project_properties1_activate(NULL, NULL);
			break;
	}
}


static void cb_func_menu_preferences(G_GNUC_UNUSED guint key_id)
{
	on_preferences1_activate(NULL, NULL);
}

static void cb_func_menu_help(G_GNUC_UNUSED guint key_id)
{
	on_help1_activate(NULL, NULL);
}

static void cb_func_search_action(guint key_id)
{
	switch (key_id)
	{
		case GEANY_KEYS_SEARCH_FIND:
			on_find1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_FINDNEXT:
			on_find_next1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_FINDPREVIOUS:
			on_find_previous1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_FINDPREVSEL:
			on_find_prevsel1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_FINDNEXTSEL:
			on_find_nextsel1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_REPLACE:
			on_replace1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_FINDINFILES:
			on_find_in_files1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_NEXTMESSAGE:
			on_next_message1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_FINDUSAGE:
			if (check_current_word())
				on_find_usage1_activate(NULL, NULL);
			break;
	}
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
	editor_fold_all(idx);
}

static void cb_func_menu_unfoldall(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	editor_unfold_all(idx);
}

static void cb_func_build_action(guint key_id)
{
	gint idx = document_get_cur_idx();
	GtkWidget *item;
	GeanyFiletype *ft;
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
	/* Note: For Build menu items it's OK (at the moment) to assume they are in the correct
	 * sensitive state, but some other menus don't update the sensitive status until
	 * they are redrawn. */
	if (item && GTK_WIDGET_IS_SENSITIVE(item))
		gtk_menu_item_activate(GTK_MENU_ITEM(item));
}

static void cb_func_reloadtaglist(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_update_tag_list(idx, TRUE);
}


static gboolean check_current_word(void)
{
	gint idx = document_get_cur_idx();
	gint pos;

	if (! DOC_IDX_VALID(idx))
		return FALSE;

	pos = sci_get_current_position(doc_list[idx].sci);

	editor_find_current_word(doc_list[idx].sci, pos,
		editor_info.current_word, GEANY_MAX_WORD_LENGTH, NULL);

	if (*editor_info.current_word == 0)
	{
		utils_beep();
		return FALSE;
	}
	return TRUE;
}


static void cb_func_switch_editor(G_GNUC_UNUSED guint key_id)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
}

static void cb_func_switch_scribble(G_GNUC_UNUSED guint key_id)
{
	msgwin_switch_tab(MSG_SCRATCH, TRUE);
}

static void cb_func_switch_search_bar(G_GNUC_UNUSED guint key_id)
{
	if (toolbar_prefs.visible && toolbar_prefs.show_search)
		gtk_widget_grab_focus(lookup_widget(app->window, "entry1"));
}

static void cb_func_switch_sidebar(G_GNUC_UNUSED guint key_id)
{
	if (ui_prefs.sidebar_visible)
	{
		gint page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->treeview_notebook));
		GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->treeview_notebook), page_num);

		/* gtk_widget_grab_focus() won't work because of the scrolled window containers */
		gtk_widget_child_focus(page, GTK_DIR_TAB_FORWARD);
	}
}

static void cb_func_switch_vte(G_GNUC_UNUSED guint key_id)
{
	msgwin_switch_tab(MSG_VTE, TRUE);
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

/* move document left/right/first/last */
static void cb_func_move_tab(guint key_id)
{
	gint idx = document_get_cur_idx();
	GtkWidget *sci = GTK_WIDGET(doc_list[idx].sci);
	GtkNotebook *nb = GTK_NOTEBOOK(app->notebook);
	gint cur_page = gtk_notebook_get_current_page(nb);

	if (! DOC_IDX_VALID(idx))
		return;

	switch (key_id)
	{
		case GEANY_KEYS_NOTEBOOK_MOVETABLEFT:
			gtk_notebook_reorder_child(nb, sci, cur_page - 1);	/* notebook wraps around by default */
			break;
		case GEANY_KEYS_NOTEBOOK_MOVETABRIGHT:
		{
			gint npage = cur_page + 1;

			if (npage == gtk_notebook_get_n_pages(nb))
				npage = 0;	/* wraparound */
			gtk_notebook_reorder_child(nb, sci, npage);
			break;
		}
		case GEANY_KEYS_NOTEBOOK_MOVETABFIRST:
			gtk_notebook_reorder_child(nb, sci, (file_prefs.tab_order_ltr) ? 0 : -1);
			break;
		case GEANY_KEYS_NOTEBOOK_MOVETABLAST:
			gtk_notebook_reorder_child(nb, sci, (file_prefs.tab_order_ltr) ? -1 : 0);
			break;
	}
	return;
}

static void cb_func_toggle_sidebar(G_GNUC_UNUSED guint key_id)
{
	on_menu_show_sidebar1_toggled(NULL, NULL);
}


static void cb_func_menu_toggle_all(G_GNUC_UNUSED guint key_id)
{
	on_menu_toggle_all_additional_widgets1_activate(NULL, NULL);
}


static void goto_matching_brace(gint idx)
{
	gint pos, new_pos;

	if (! DOC_IDX_VALID(idx)) return;

	pos = sci_get_current_position(doc_list[idx].sci);
	if (! utils_isbrace(sci_get_char_at(doc_list[idx].sci, pos), TRUE))
		pos--; /* set pos to the brace */

	new_pos = sci_find_bracematch(doc_list[idx].sci, pos);
	if (new_pos != -1)
	{	/* set the cursor at the brace */
		sci_set_current_position(doc_list[idx].sci, new_pos, FALSE);
		editor_display_current_line(idx, 0.5F);
	}
}


static void cb_func_clipboard(guint key_id)
{
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx)) return;

	switch (key_id)
	{
		case GEANY_KEYS_CLIPBOARD_CUT:
			on_cut1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_CLIPBOARD_COPY:
			on_copy1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_CLIPBOARD_PASTE:
			on_paste1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_CLIPBOARD_COPYLINE:
			sci_cmd(doc_list[idx].sci, SCI_LINECOPY);
			break;
		case GEANY_KEYS_CLIPBOARD_CUTLINE:
			sci_cmd(doc_list[idx].sci, SCI_LINECUT);
			break;
	}
}


/* Common function for goto keybindings, useful even when sci doesn't have focus. */
static void cb_func_goto_action(guint key_id)
{
	gint cur_line;
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx)) return;

	cur_line = sci_get_current_line(doc_list[idx].sci);

	switch (key_id)
	{
		case GEANY_KEYS_GOTO_BACK:
			navqueue_go_back();
			break;
		case GEANY_KEYS_GOTO_FORWARD:
			navqueue_go_forward();
			break;
		case GEANY_KEYS_GOTO_LINE:
			on_go_to_line1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_GOTO_MATCHINGBRACE:
			goto_matching_brace(idx);
			break;
		case GEANY_KEYS_GOTO_TOGGLEMARKER:
		{
			gboolean set = sci_is_marker_set_at_line(doc_list[idx].sci, cur_line, 1);

			sci_set_marker_at_line(doc_list[idx].sci, cur_line, ! set, 1);
			break;
		}
		case GEANY_KEYS_GOTO_NEXTMARKER:
		{
			gint mline = sci_marker_next(doc_list[idx].sci, cur_line + 1, 1 << 1, TRUE);

			if (mline != -1)
			{
				sci_set_current_line(doc_list[idx].sci, mline);
				editor_display_current_line(idx, 0.5F);
			}
			break;
		}
		case GEANY_KEYS_GOTO_PREVIOUSMARKER:
		{
			gint mline = sci_marker_previous(doc_list[idx].sci, cur_line - 1, 1 << 1, TRUE);

			if (mline != -1)
			{
				sci_set_current_line(doc_list[idx].sci, mline);
				editor_display_current_line(idx, 0.5F);
			}
			break;
		}
		case GEANY_KEYS_GOTO_TAGDEFINITION:
			if (check_current_word())
				symbols_goto_tag(editor_info.current_word, TRUE);
			break;
		case GEANY_KEYS_GOTO_TAGDECLARATION:
			if (check_current_word())
				symbols_goto_tag(editor_info.current_word, FALSE);
			break;
	}
}


static void duplicate_lines(ScintillaObject *sci)
{
	if (sci_get_lines_selected(sci) > 1)
	{	/* ignore extra_line because of selecting lines from the line number column */
		editor_select_lines(sci, FALSE);
		sci_selection_duplicate(sci);
	}
	else if (sci_can_copy(sci))
		sci_selection_duplicate(sci);
	else
		sci_line_duplicate(sci);
}


static void delete_lines(ScintillaObject *sci)
{
	editor_select_lines(sci, TRUE); /* include last line (like cut lines, copy lines do) */
	sci_clear(sci);	/* (SCI_LINEDELETE only does 1 line) */
}


/* common function for editor keybindings, only valid when scintilla has focus. */
static void cb_func_editor_action(guint key_id)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	/* edit keybindings only valid when scintilla widget has focus */
	if (! DOC_IDX_VALID(idx) || focusw != GTK_WIDGET(doc_list[idx].sci)) return;

	switch (key_id)
	{
		case GEANY_KEYS_EDITOR_UNDO:
			on_undo1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDITOR_REDO:
			on_redo1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDITOR_SCROLLTOLINE:
			editor_scroll_to_line(doc_list[idx].sci, -1, 0.5F);
			break;
		case GEANY_KEYS_EDITOR_SCROLLLINEUP:
			sci_cmd(doc_list[idx].sci, SCI_LINESCROLLUP);
			break;
		case GEANY_KEYS_EDITOR_SCROLLLINEDOWN:
			sci_cmd(doc_list[idx].sci, SCI_LINESCROLLDOWN);
			break;
		case GEANY_KEYS_EDITOR_DUPLICATELINE:
			duplicate_lines(doc_list[idx].sci);
			break;
		case GEANY_KEYS_EDITOR_DELETELINE:
			delete_lines(doc_list[idx].sci);
			break;
		case GEANY_KEYS_EDITOR_TRANSPOSELINE:
			sci_cmd(doc_list[idx].sci, SCI_LINETRANSPOSE);
			break;
		case GEANY_KEYS_EDITOR_AUTOCOMPLETE:
			editor_start_auto_complete(idx, sci_get_current_position(doc_list[idx].sci), TRUE);
			break;
		case GEANY_KEYS_EDITOR_CALLTIP:
			editor_show_calltip(idx, -1);
			break;
		case GEANY_KEYS_EDITOR_MACROLIST:
			editor_show_macro_list(doc_list[idx].sci);
			break;
		case GEANY_KEYS_EDITOR_CONTEXTACTION:
			if (check_current_word())
				on_context_action1_activate(GTK_MENU_ITEM(lookup_widget(app->popup_menu,
					"context_action1")), NULL);
			break;
		case GEANY_KEYS_EDITOR_SUPPRESSSNIPPETCOMPLETION:
		{
			KeyBinding *kb = keybindings_lookup_item(GEANY_KEY_GROUP_EDITOR,
				GEANY_KEYS_EDITOR_COMPLETESNIPPET);

			switch (kb->key)
			{
				case GDK_space:
					sci_add_text(doc_list[idx].sci, " ");
					break;
				case GDK_Tab:
					sci_cmd(doc_list[idx].sci, SCI_TAB);
					break;
				default:
					break;
			}
			break;
		}
	}
}


/* common function for format keybindings, only valid when scintilla has focus. */
static void cb_func_format_action(guint key_id)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	/* keybindings only valid when scintilla widget has focus */
	if (! DOC_IDX_VALID(idx) || focusw != GTK_WIDGET(doc_list[idx].sci)) return;

	switch (key_id)
	{
		case GEANY_KEYS_FORMAT_COMMENTLINETOGGLE:
			on_menu_toggle_line_commentation1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FORMAT_COMMENTLINE:
			on_menu_comment_line1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FORMAT_UNCOMMENTLINE:
			on_menu_uncomment_line1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FORMAT_INCREASEINDENT:
			on_menu_increase_indent1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FORMAT_DECREASEINDENT:
			on_menu_decrease_indent1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FORMAT_INCREASEINDENTBYSPACE:
			editor_indentation_by_one_space(idx, -1, FALSE);
			break;
		case GEANY_KEYS_FORMAT_DECREASEINDENTBYSPACE:
			editor_indentation_by_one_space(idx, -1, TRUE);
			break;
		case GEANY_KEYS_FORMAT_AUTOINDENT:
			editor_smart_line_indentation(idx, -1);
			break;
		case GEANY_KEYS_FORMAT_TOGGLECASE:
			on_toggle_case1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FORMAT_SENDTOCMD1:
			if (ui_prefs.custom_commands && g_strv_length(ui_prefs.custom_commands) > 0)
				tools_execute_custom_command(idx, ui_prefs.custom_commands[0]);
			break;
		case GEANY_KEYS_FORMAT_SENDTOCMD2:
			if (ui_prefs.custom_commands && g_strv_length(ui_prefs.custom_commands) > 1)
				tools_execute_custom_command(idx, ui_prefs.custom_commands[1]);
			break;
		case GEANY_KEYS_FORMAT_SENDTOCMD3:
			if (ui_prefs.custom_commands && g_strv_length(ui_prefs.custom_commands) > 2)
				tools_execute_custom_command(idx, ui_prefs.custom_commands[2]);
			break;
	}
}


/* common function for select keybindings, only valid when scintilla has focus. */
static void cb_func_select_action(guint key_id)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));
	static GtkWidget *scribble_widget = NULL;

	/* special case for Select All in the scribble widget */
	if (scribble_widget == NULL) /* lookup the scribble widget only once */
		scribble_widget = lookup_widget(app->window, "textview_scribble");
	if (key_id == GEANY_KEYS_SELECT_ALL && focusw == scribble_widget)
	{
		g_signal_emit_by_name(scribble_widget, "select-all", TRUE);
		return;
	}

	/* keybindings only valid when scintilla widget has focus */
	if (! DOC_IDX_VALID(idx) || focusw != GTK_WIDGET(doc_list[idx].sci)) return;

	switch (key_id)
	{
		case GEANY_KEYS_SELECT_ALL:
			on_menu_select_all1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_SELECT_WORD:
			editor_select_word(doc_list[idx].sci);
			break;
		case GEANY_KEYS_SELECT_LINE:
			editor_select_lines(doc_list[idx].sci, FALSE);
			break;
		case GEANY_KEYS_SELECT_PARAGRAPH:
			editor_select_paragraph(doc_list[idx].sci);
			break;
	}
}


static void cb_func_menu_replacetabs(G_GNUC_UNUSED guint key_id)
{
	on_replace_tabs_activate(NULL, NULL);
}


/* common function for insert keybindings, only valid when scintilla has focus. */
static void cb_func_insert_action(guint key_id)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	/* keybindings only valid when scintilla widget has focus */
	if (! DOC_IDX_VALID(idx) || focusw != GTK_WIDGET(doc_list[idx].sci)) return;

	switch (key_id)
	{
		case GEANY_KEYS_INSERT_ALTWHITESPACE:
			editor_insert_alternative_whitespace(idx);
			break;
		case GEANY_KEYS_INSERT_DATE:
			gtk_menu_item_activate(GTK_MENU_ITEM(lookup_widget(app->window, "insert_date_custom1")));
			break;
	}
}

