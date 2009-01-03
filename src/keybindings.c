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
#include "toolbar.h"
#include "geanywraplabel.h"


GPtrArray *keybinding_groups;	/* array of GeanyKeyGroup pointers */

/* keyfile group name for non-plugin KB groups */
const gchar keybindings_keyfile_group_name[] = "Bindings";

static gboolean ignore_keybinding = FALSE;

static GtkAccelGroup *kb_accel_group = NULL;
static const gboolean swap_alt_tab_order = FALSE;

const gsize MAX_MRU_DOCS = 20;
static GQueue *mru_docs = NULL;

static gboolean switch_dialog_cancelled = TRUE;
static GtkWidget *switch_dialog = NULL;
static GtkWidget *switch_dialog_label = NULL;


/* central keypress event handler, almost all keypress events go to this function */
static gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
static gboolean on_key_release_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

static gboolean check_current_word(void);
static gboolean read_current_word(void);

static void cb_func_file_action(guint key_id);
static void cb_func_project_action(guint key_id);
static void cb_func_editor_action(guint key_id);
static void cb_func_select_action(guint key_id);
static void cb_func_format_action(guint key_id);
static void cb_func_insert_action(guint key_id);
static void cb_func_search_action(guint key_id);
static void cb_func_goto_action(guint key_id);
static void cb_func_switch_action(guint key_id);
static void cb_func_clipboard(guint key_id);
static void cb_func_build_action(guint key_id);
static void cb_func_document_action(guint key_id);
static void cb_func_view_action(guint key_id);

/* note: new keybindings should normally use per group callbacks */
static void cb_func_menu_help(guint key_id);
static void cb_func_menu_preferences(guint key_id);

static void cb_func_menu_fullscreen(guint key_id);
static void cb_func_menu_messagewindow(guint key_id);

static void cb_func_menu_opencolorchooser(guint key_id);

static void cb_func_switch_tableft(guint key_id);
static void cb_func_switch_tabright(guint key_id);
static void cb_func_switch_tablastused(guint key_id);
static void cb_func_move_tab(guint key_id);

static void add_popup_menu_accels(void);
static void apply_kb_accel(GeanyKeyGroup *group, GeanyKeyBinding *kb, gpointer user_data);


/* This is used to set default keybindings on startup but at this point we don't want to
 * assign the keybinding to the menu_item (apply_kb_accel) otherwise it can't be overridden
 * by user keybindings anymore */
/** Simple convenience function to fill a GeanyKeyBinding struct item.
 * @param group
 * @param key_id
 * @param callback
 * @param key
 * @param mod
 * @param name
 * @param label
 * @param menu_item */
void keybindings_set_item(GeanyKeyGroup *group, gsize key_id,
		GeanyKeyCallback callback, guint key, GdkModifierType mod,
		gchar *name, gchar *label, GtkWidget *menu_item)
{
	GeanyKeyBinding *kb;

	g_assert(key_id < group->count);

	kb = &group->keys[key_id];

	kb->name = name;
	kb->label = label;
	kb->key = key;
	kb->mods = mod;
	kb->callback = callback;
	kb->menu_item = menu_item;
}


static GeanyKeyGroup *add_kb_group(GeanyKeyGroup *group,
		const gchar *name, const gchar *label, gsize count, GeanyKeyBinding *keys)
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
	ui_lookup_widget(main_widgets.window, G_STRINGIFY(widget_name))

/* Expansion for group_id = FILE:
 * static GeanyKeyBinding FILE_keys[GEANY_KEYS_FILE_COUNT]; */
#define DECLARE_KEYS(group_id) \
	static GeanyKeyBinding group_id ## _keys[GEANY_KEYS_ ## group_id ## _COUNT]

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
	static GeanyKeyGroup groups[GEANY_KEY_GROUP_COUNT];
	GeanyKeyGroup *group;
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
		0, 0, "edit_insertwhitespace", _("Insert alternative white space"), NULL);

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
	keybindings_set_item(group, GEANY_KEYS_SEARCH_PREVIOUSMESSAGE, cb_func_search_action,
		0, 0, "menu_previousmessage", _("Previous Message"), LW(previous_message1));
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDUSAGE, cb_func_search_action,
		0, 0, "popup_findusage", _("Find Usage"), NULL);
	keybindings_set_item(group, GEANY_KEYS_SEARCH_FINDDOCUMENTUSAGE, cb_func_search_action,
		0, 0, "popup_finddocumentusage", _("Find Document Usage"), NULL);

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
	keybindings_set_item(group, GEANY_KEYS_GOTO_LINESTART, cb_func_goto_action,
		GDK_Home, 0, "edit_gotolinestart", _("Go to Start of Line"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_LINEEND, cb_func_goto_action,
		GDK_End, 0, "edit_gotolineend", _("Go to End of Line"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_PREVWORDSTART, cb_func_goto_action,
		GDK_slash, GDK_CONTROL_MASK, "edit_prevwordstart", _("Go to Previous Word Part"), NULL);
	keybindings_set_item(group, GEANY_KEYS_GOTO_NEXTWORDSTART, cb_func_goto_action,
		GDK_backslash, GDK_CONTROL_MASK, "edit_nextwordstart", _("Go to Next Word Part"), NULL);

	group = ADD_KB_GROUP(VIEW, _("View"));

	keybindings_set_item(group, GEANY_KEYS_VIEW_TOGGLEALL, cb_func_view_action,
		0, 0, "menu_toggleall", _("Toggle All Additional Widgets"),
		LW(menu_toggle_all_additional_widgets1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_FULLSCREEN, cb_func_menu_fullscreen,
		GDK_F11, 0, "menu_fullscreen", _("Fullscreen"), LW(menu_fullscreen1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_MESSAGEWINDOW, cb_func_menu_messagewindow,
		0, 0, "menu_messagewindow", _("Toggle Messages Window"),
		LW(menu_show_messages_window1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_SIDEBAR, cb_func_view_action,
		0, 0, "toggle_sidebar", _("Toggle Sidebar"), LW(menu_show_sidebar1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_ZOOMIN, cb_func_view_action,
		GDK_plus, GDK_CONTROL_MASK, "menu_zoomin", _("Zoom In"), LW(menu_zoom_in1));
	keybindings_set_item(group, GEANY_KEYS_VIEW_ZOOMOUT, cb_func_view_action,
		GDK_minus, GDK_CONTROL_MASK, "menu_zoomout", _("Zoom Out"), LW(menu_zoom_out1));

	group = ADD_KB_GROUP(FOCUS, _("Focus"));

	keybindings_set_item(group, GEANY_KEYS_FOCUS_EDITOR, cb_func_switch_action,
		GDK_F2, 0, "switch_editor", _("Switch to Editor"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_SCRIBBLE, cb_func_switch_action,
		GDK_F6, 0, "switch_scribble", _("Switch to Scribble"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_VTE, cb_func_switch_action,
		GDK_F4, 0, "switch_vte", _("Switch to VTE"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_SEARCHBAR, cb_func_switch_action,
		GDK_F7, 0, "switch_search_bar", _("Switch to Search Bar"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_SIDEBAR, cb_func_switch_action,
		0, 0, "switch_sidebar", _("Switch to Sidebar"), NULL);
	keybindings_set_item(group, GEANY_KEYS_FOCUS_COMPILER, cb_func_switch_action,
		0, 0, "switch_compiler", _("Switch to Compiler"), NULL);

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

	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_LINEWRAP, cb_func_document_action,
		0, 0, "menu_linewrap", _("Toggle Line wrapping"), LW(menu_line_wrapping1));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_LINEBREAK, cb_func_document_action,
		0, 0, "menu_linebreak", _("Toggle Line breaking"), LW(line_breaking1));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_REPLACETABS, cb_func_document_action,
		0, 0, "menu_replacetabs", _("Replace tabs by space"), LW(menu_replace_tabs));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_REPLACESPACES, cb_func_document_action,
		0, 0, "menu_replacespaces", _("Replace spaces by tabs"), LW(menu_replace_spaces));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_TOGGLEFOLD, cb_func_document_action,
		0, 0, "menu_togglefold", _("Toggle current fold"), NULL);
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_FOLDALL, cb_func_document_action,
		0, 0, "menu_foldall", _("Fold all"), LW(menu_fold_all1));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_UNFOLDALL, cb_func_document_action,
		0, 0, "menu_unfoldall", _("Unfold all"), LW(menu_unfold_all1));
	keybindings_set_item(group, GEANY_KEYS_DOCUMENT_RELOADTAGLIST, cb_func_document_action,
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
	keybindings_set_item(group, GEANY_KEYS_BUILD_PREVIOUSERROR, cb_func_build_action,
		0, 0, "build_previouserror", _("Previous error"), NULL);
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


/* before the tab changes, add the current document to the MRU list */
static void on_notebook_switch_page()
{
	GeanyDocument *old = document_get_current();

	/* when closing current doc, old is NULL */
	if (old)
	{
		g_queue_push_head(mru_docs, old);

		if (g_queue_get_length(mru_docs) > MAX_MRU_DOCS)
			g_queue_pop_tail(mru_docs);
	}
}


/* really this should be just after a document was closed, not idle */
static gboolean on_idle_close(gpointer data)
{
	GeanyDocument *current;

	current = document_get_current();

	while (current && g_queue_peek_head(mru_docs) == current)
		g_queue_pop_head(mru_docs);

	return FALSE;
}


static void on_document_close(GObject *obj, GeanyDocument *doc)
{
	g_queue_remove_all(mru_docs, doc);
	g_idle_add(on_idle_close, NULL);
}


void keybindings_init(void)
{
	mru_docs = g_queue_new();
	g_signal_connect(main_widgets.notebook, "switch-page",
		G_CALLBACK(on_notebook_switch_page), NULL);
	g_signal_connect(geany_object, "document-close",
		G_CALLBACK(on_document_close), NULL);

	keybinding_groups = g_ptr_array_sized_new(GEANY_KEY_GROUP_COUNT);

	kb_accel_group = gtk_accel_group_new();

	init_default_kb();

	gtk_window_add_accel_group(GTK_WINDOW(main_widgets.window), kb_accel_group);

	g_signal_connect(main_widgets.window, "key-press-event", G_CALLBACK(on_key_press_event), NULL);
	/* in case the switch dialog misses an event while drawing the dialog */
	g_signal_connect(main_widgets.window, "key-release-event", G_CALLBACK(on_key_release_event), NULL);
}


static void apply_kb_accel(GeanyKeyGroup *group, GeanyKeyBinding *kb, gpointer user_data)
{
	if (kb->key != 0 && kb->menu_item)
	{
		gtk_widget_add_accelerator(kb->menu_item, "activate", kb_accel_group,
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
	}
}


typedef void (*KBItemCallback) (GeanyKeyGroup *group, GeanyKeyBinding *kb, gpointer user_data);

static void keybindings_foreach(KBItemCallback cb, gpointer user_data)
{
	gsize g, i;

	for (g = 0; g < keybinding_groups->len; g++)
	{
		GeanyKeyGroup *group = g_ptr_array_index(keybinding_groups, g);

		for (i = 0; i < group->count; i++)
		{
			GeanyKeyBinding *kb = &group->keys[i];

			cb(group, kb, user_data);
		}
	}
}


static void load_kb(GeanyKeyGroup *group, GeanyKeyBinding *kb, gpointer user_data)
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

	/* set menu accels now, after user keybindings have been read and processed
	 * if we would set it before, user keybindings could not override menu item's default
	 * keybindings */
	keybindings_foreach(apply_kb_accel, NULL);
}


static void add_menu_accel(GeanyKeyGroup *group, guint kb_id,
	GtkAccelGroup *accel_group, GtkWidget *menuitem)
{
	GeanyKeyBinding *kb = &group->keys[kb_id];

	if (kb->key != 0)
		gtk_widget_add_accelerator(menuitem, "activate", accel_group,
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
}


#define GEANY_ADD_POPUP_ACCEL(kb_id, wid) \
	add_menu_accel(group, kb_id, accel_group, ui_lookup_widget(main_widgets.editor_menu, G_STRINGIFY(wid)))

/* set the menu item accelerator shortcuts (just for visibility, they are handled anyway) */
static void add_popup_menu_accels(void)
{
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GeanyKeyGroup *group;

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
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_SEARCH_FINDDOCUMENTUSAGE, find_document_usage1);

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

	gtk_window_add_accel_group(GTK_WINDOW(main_widgets.window), accel_group);
}


static void set_keyfile_kb(GeanyKeyGroup *group, GeanyKeyBinding *kb, gpointer user_data)
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
	g_queue_free(mru_docs);
}


static void fill_shortcut_labels_treeview(GtkWidget *tree)
{
	gsize g, i;
	gchar *shortcut;
	GeanyKeyBinding *kb;
	GeanyKeyGroup *group;
	GtkListStore *store;
	GtkTreeIter iter;

	store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, PANGO_TYPE_WEIGHT);

	for (g = 0; g < keybinding_groups->len; g++)
	{
		group = g_ptr_array_index(keybinding_groups, g);

		if (g > 0)
		{
			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, -1);
		}

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, group->label, 2, PANGO_WEIGHT_BOLD, -1);

		for (i = 0; i < group->count; i++)
		{
			kb = &group->keys[i];
			shortcut = gtk_accelerator_get_label(kb->key, kb->mods);

			gtk_list_store_append(store, &iter);
			gtk_list_store_set(store, &iter, 0, kb->label, 1, shortcut, 2, PANGO_WEIGHT_NORMAL, -1);

			g_free(shortcut);
		}
	}

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_object_unref(store);
}


static GtkWidget *create_dialog(void)
{
	GtkWidget *dialog, *tree, *label, *swin, *vbox;
	GtkCellRenderer *text_renderer;
	GtkTreeViewColumn *column;
	gint height;

	dialog = gtk_dialog_new_with_buttons(_("Keyboard Shortcuts"), GTK_WINDOW(main_widgets.window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_STOCK_EDIT, GTK_RESPONSE_APPLY,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_widget_set_name(dialog, "GeanyDialog");

	height = GEANY_WINDOW_MINIMAL_HEIGHT;
	gtk_window_set_default_size(GTK_WINDOW(dialog), -1, height);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

	label = gtk_label_new(_("The following keyboard shortcuts are configurable:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	tree = gtk_tree_view_new();
	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	text_renderer = gtk_cell_renderer_text_new();
    /* we can't use "weight-set", see http://bugzilla.gnome.org/show_bug.cgi?id=355214 */
	column = gtk_tree_view_column_new_with_attributes(
		NULL, text_renderer, "text", 0, "weight", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	text_renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(NULL, text_renderer, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	fill_shortcut_labels_treeview(tree);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_ETCHED_IN);
	gtk_container_add(GTK_CONTAINER(swin), tree);

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);
	return dialog;
}


/* non-modal keyboard shortcuts dialog, so user can edit whilst seeing the shortcuts */
static GtkWidget *key_dialog = NULL;

static void on_dialog_response(GtkWidget *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_APPLY)
	{
		GtkWidget *wid;

		prefs_show_dialog();
		/* select the KB page */
		wid = ui_lookup_widget(ui_widgets.prefs_dialog, "frame22");
		if (wid != NULL)
		{
			GtkNotebook *nb = GTK_NOTEBOOK(ui_lookup_widget(ui_widgets.prefs_dialog, "notebook2"));

			if (nb != NULL)
				gtk_notebook_set_current_page(nb, gtk_notebook_page_num(nb, wid));
		}
	}
	gtk_widget_destroy(dialog);
	key_dialog = NULL;
}


void keybindings_show_shortcuts(void)
{
	if (key_dialog)
		gtk_widget_destroy(key_dialog);	/* in case the key_dialog is still visible */

	key_dialog = create_dialog();
	g_signal_connect(key_dialog, "response", G_CALLBACK(on_dialog_response), NULL);
	gtk_widget_show_all(key_dialog);
}


static gboolean check_fixed_kb(guint keyval, guint state)
{
	/* check alt-0 to alt-9 for setting current notebook page */
	if (state & GDK_MOD1_MASK && keyval >= GDK_0 && keyval <= GDK_9)
	{
		gint page = keyval - GDK_0 - 1;
		gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));

		/* alt-0 is for the rightmost tab */
		if (keyval == GDK_0)
			page = npages - 1;
		/* invert the order if tabs are added on the other side */
		if (swap_alt_tab_order && ! file_prefs.tab_order_ltr)
			page = (npages - 1) - page;

		gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), page);
		return TRUE;
	}
	if (keyval == GDK_Page_Up || keyval == GDK_Page_Down)
	{
		/* switch to first or last document */
		if (state == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
		{
			if (keyval == GDK_Page_Up)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), 0);
			if (keyval == GDK_Page_Down)
				gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook),
					gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) - 1);
			return TRUE;
		}
	}
	return FALSE;
}


/* We have a special case for GEANY_KEYS_EDIT_COMPLETESNIPPET, because we need to
 * return FALSE if no completion occurs, so the tab or space is handled normally. */
static gboolean check_snippet_completion(guint keyval, guint state)
{
	GeanyKeyBinding *kb = keybindings_lookup_item(GEANY_KEY_GROUP_EDITOR,
		GEANY_KEYS_EDITOR_COMPLETESNIPPET);

	if (kb->key == keyval && kb->mods == state)
	{
		GeanyDocument *doc = document_get_current();
		GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

		/* keybinding only valid when scintilla widget has focus */
		if (doc != NULL && focusw == GTK_WIDGET(doc->editor->sci))
		{
			ScintillaObject *sci = doc->editor->sci;
			gint pos = sci_get_current_position(sci);

			if (editor_prefs.complete_snippets)
				return editor_complete_snippet(doc->editor, pos);
		}
	}
	return FALSE;
}


#ifdef HAVE_VTE
static gboolean on_menu_expose_event(GtkWidget *widget, GdkEventExpose *event,
		gpointer user_data)
{
	if (!GTK_WIDGET_SENSITIVE(widget))
		gtk_widget_set_sensitive(GTK_WIDGET(widget), TRUE);
	return FALSE;
}


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
	if (gtk_window_get_focus(GTK_WINDOW(main_widgets.window)) != vc->vte)
		return FALSE;
	/* prevent menubar flickering: */
	if (state == GDK_SHIFT_MASK && (keyval >= GDK_a && keyval <= GDK_z))
		return FALSE;
	if (state == 0 && (keyval < GDK_F1 || keyval > GDK_F35))	/* e.g. backspace */
		return FALSE;

	/* make focus commands override any bash commands */
	for (i = 0; i < GEANY_KEYS_FOCUS_COUNT; i++)
	{
		GeanyKeyBinding *kb = keybindings_lookup_item(GEANY_KEY_GROUP_FOCUS, i);

		if (state == kb->mods && keyval == kb->key)
			return FALSE;
	}

	/* Temporarily disable the menus to prevent conflicting menu accelerators
	 * from overriding the VTE bash shortcuts.
	 * Note: maybe there's a better way of doing this ;-) */
	widget = ui_lookup_widget(main_widgets.window, "menubar1");
	gtk_widget_set_sensitive(widget, FALSE);
	{
		/* make the menubar sensitive before it is redrawn */
		static gboolean connected = FALSE;
		if (!connected)
			g_signal_connect(widget, "expose-event", G_CALLBACK(on_menu_expose_event), NULL);
	}

	widget = main_widgets.editor_menu;
	gtk_widget_set_sensitive(widget, FALSE);
	g_idle_add(set_sensitive, widget);
	return TRUE;
}
#endif


static void check_disk_status(void)
{
	GeanyDocument *doc = document_get_current();

	if (doc != NULL)
	{
		document_check_disk_status(doc, FALSE);
	}
}


/* central keypress event handler, almost all keypress events go to this function */
static gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
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

	ignore_keybinding = FALSE;
	for (g = 0; g < keybinding_groups->len; g++)
	{
		GeanyKeyGroup *group = g_ptr_array_index(keybinding_groups, g);

		for (i = 0; i < group->count; i++)
		{
			GeanyKeyBinding *kb = &group->keys[i];

			if (keyval == kb->key && state == kb->mods)
			{
				if (kb->callback == NULL)
					return FALSE;	/* ignore the keybinding */

				/* call the corresponding callback function for this shortcut */
				kb->callback(i);
				return !ignore_keybinding;
			}
		}
	}
	/* fixed keybindings can be overridden by user bindings, so check them last */
	if (check_fixed_kb(keyval, state))
		return TRUE;
	return FALSE;
}


static gboolean is_modifier_key(guint keyval)
{
	switch (keyval)
	{
		case GDK_Shift_L:
		case GDK_Shift_R:
		case GDK_Control_L:
		case GDK_Control_R:
		case GDK_Meta_L:
		case GDK_Meta_R:
		case GDK_Alt_L:
		case GDK_Alt_R:
		case GDK_Super_L:
		case GDK_Super_R:
		case GDK_Hyper_L:
		case GDK_Hyper_R:
			return TRUE;
		default:
			return FALSE;
	}
}


GeanyKeyBinding *keybindings_lookup_item(guint group_id, guint key_id)
{
	GeanyKeyGroup *group;

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
	GeanyKeyBinding *kb;

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
			on_toolbutton_reload_clicked(NULL, NULL);
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
		case GEANY_KEYS_SEARCH_PREVIOUSMESSAGE:
			on_previous_message1_activate(NULL, NULL); break;
		case GEANY_KEYS_SEARCH_FINDUSAGE:
			read_current_word();
			on_find_usage1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_SEARCH_FINDDOCUMENTUSAGE:
			read_current_word();
			on_find_document_usage1_activate(NULL, NULL);
			break;
	}
}

static void cb_func_menu_opencolorchooser(G_GNUC_UNUSED guint key_id)
{
	on_show_color_chooser1_activate(NULL, NULL);
}


static void cb_func_view_action(guint key_id)
{
	switch (key_id)
	{
		case GEANY_KEYS_VIEW_TOGGLEALL:
			on_menu_toggle_all_additional_widgets1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_VIEW_SIDEBAR:
			on_menu_show_sidebar1_toggled(NULL, NULL);
			break;
		case GEANY_KEYS_VIEW_ZOOMIN:
			on_zoom_in1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_VIEW_ZOOMOUT:
			on_zoom_out1_activate(NULL, NULL);
			break;
		default:
			break;
	}
}


static void cb_func_menu_fullscreen(G_GNUC_UNUSED guint key_id)
{
	GtkCheckMenuItem *c = GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_fullscreen1"));

	gtk_check_menu_item_set_active(c, ! gtk_check_menu_item_get_active(c));
}

static void cb_func_menu_messagewindow(G_GNUC_UNUSED guint key_id)
{
	GtkCheckMenuItem *c = GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_show_messages_window1"));

	gtk_check_menu_item_set_active(c, ! gtk_check_menu_item_get_active(c));
}


static void cb_func_build_action(guint key_id)
{
	GtkWidget *item;
	GeanyFiletype *ft;
	BuildMenuItems *menu_items;

	GeanyDocument *doc = document_get_current();
	if (doc == NULL)
		return;

	ft = doc->file_type;
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
		case GEANY_KEYS_BUILD_PREVIOUSERROR:
			item = menu_items->item_previous_error;
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


static gboolean read_current_word(void)
{
	gint pos;
	GeanyDocument *doc = document_get_current();

	if (doc == NULL)
		return FALSE;

	pos = sci_get_current_position(doc->editor->sci);

	editor_find_current_word(doc->editor, pos,
		editor_info.current_word, GEANY_MAX_WORD_LENGTH, NULL);

	return (*editor_info.current_word != 0);
}


static gboolean check_current_word(void)
{
	if (!read_current_word())
	{
		utils_beep();
		return FALSE;
	}
	return TRUE;
}


static void focus_sidebar(void)
{
	if (ui_prefs.sidebar_visible)
	{
		gint page_num = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.sidebar_notebook));
		GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_widgets.sidebar_notebook), page_num);

		/* gtk_widget_grab_focus() won't work because of the scrolled window containers */
		gtk_widget_child_focus(page, GTK_DIR_TAB_FORWARD);
	}
}


static void cb_func_switch_action(guint key_id)
{
	switch (key_id)
	{
		case GEANY_KEYS_FOCUS_EDITOR:
		{
			GeanyDocument *doc = document_get_current();
			if (doc != NULL)
				gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
			break;
		}
		case GEANY_KEYS_FOCUS_SCRIBBLE:
			msgwin_switch_tab(MSG_SCRATCH, TRUE);
			break;
		case GEANY_KEYS_FOCUS_SEARCHBAR:
			if (toolbar_prefs.visible)
			{
				GtkWidget *search_entry = toolbar_get_widget_child_by_name("SearchEntry");
				if (search_entry != NULL)
					gtk_widget_grab_focus(search_entry);
			}
			break;
		case GEANY_KEYS_FOCUS_SIDEBAR:
			focus_sidebar();
			break;
		case GEANY_KEYS_FOCUS_VTE:
			msgwin_switch_tab(MSG_VTE, TRUE);
			break;
		case GEANY_KEYS_FOCUS_COMPILER:
			msgwin_switch_tab(MSG_COMPILER, TRUE);
			break;
	}
}


static void switch_document(gint direction)
{
	gint page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.notebook));

	if (direction == LEFT)
	{
		if (cur_page > 0)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), cur_page - 1);
		else
			gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), page_count - 1);
	}
	else if (direction == RIGHT)
	{
		if (cur_page < page_count - 1)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), cur_page + 1);
		else
			gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), 0);
	}
}


static void cb_func_switch_tableft(G_GNUC_UNUSED guint key_id)
{
	switch_document(LEFT);
}

static void cb_func_switch_tabright(G_GNUC_UNUSED guint key_id)
{
	switch_document(RIGHT);
}


static gboolean on_key_release_event(GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
{
	/* user may have rebound keybinding to a different modifier than Ctrl, so check all */
	if (!switch_dialog_cancelled && is_modifier_key(ev->keyval))
	{
		switch_dialog_cancelled = TRUE;

		if (switch_dialog && GTK_WIDGET_VISIBLE(switch_dialog))
			gtk_widget_hide(switch_dialog);
	}
	return FALSE;
}


static GtkWidget *ui_minimal_dialog_new(GtkWindow *parent, const gchar *title)
{
	GtkWidget *dialog;

	dialog = gtk_window_new(GTK_WINDOW_POPUP);

	if (parent)
	{
		gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	}
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_set_name(dialog, "GeanyDialog");
	return dialog;
}


static GtkWidget *create_switch_dialog(void)
{
	GtkWidget *dialog, *widget, *vbox;

	dialog = ui_minimal_dialog_new(GTK_WINDOW(main_widgets.window), _("Switch to Document"));
	gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 150, -1);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);

	widget = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(vbox), widget);

	widget = geany_wrap_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(widget), GTK_JUSTIFY_CENTER);
	gtk_container_add(GTK_CONTAINER(vbox), widget);
	switch_dialog_label = widget;

	g_signal_connect(dialog, "key-release-event", G_CALLBACK(on_key_release_event), NULL);
	return dialog;
}


static gboolean on_switch_timeout(G_GNUC_UNUSED gpointer data)
{
	if (switch_dialog_cancelled)
		return FALSE;

	if (!switch_dialog)
		switch_dialog = create_switch_dialog();

	geany_wrap_label_set_text(GTK_LABEL(switch_dialog_label),
		DOC_FILENAME(document_get_current()));
	gtk_widget_show_all(switch_dialog);
	return FALSE;
}


static void cb_func_switch_tablastused(G_GNUC_UNUSED guint key_id)
{
	/* TODO: MRU switching order */
	GeanyDocument *last_doc = g_queue_peek_head(mru_docs);

	if (!DOC_VALID(last_doc))
		return;

	gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook),
		document_get_notebook_page(last_doc));

	/* if there's a modifier key, we can switch back in MRU order each time unless
	 * the key is released */
	if (!switch_dialog_cancelled)
	{
		on_switch_timeout(NULL);	/* update filename label */
	}
	else
	if (keybindings_lookup_item(GEANY_KEY_GROUP_NOTEBOOK,
		GEANY_KEYS_NOTEBOOK_SWITCHTABLASTUSED)->mods)
	{
		switch_dialog_cancelled = FALSE;

		/* delay showing dialog to give user time to let go of any modifier keys */
		g_timeout_add(600, on_switch_timeout, NULL);
	}
}


/* move document left/right/first/last */
static void cb_func_move_tab(guint key_id)
{
	GtkWidget *sci;
	GtkNotebook *nb = GTK_NOTEBOOK(main_widgets.notebook);
	gint cur_page = gtk_notebook_get_current_page(nb);
	GeanyDocument *doc = document_get_current();

	if (doc == NULL)
		return;

	sci = GTK_WIDGET(doc->editor->sci);

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


static void goto_matching_brace(GeanyDocument *doc)
{
	gint pos, new_pos;

	if (doc == NULL)
		return;

	pos = sci_get_current_position(doc->editor->sci);
	if (! utils_isbrace(sci_get_char_at(doc->editor->sci, pos), TRUE))
		pos--; /* set pos to the brace */

	new_pos = sci_find_matching_brace(doc->editor->sci, pos);
	if (new_pos != -1)
	{	/* set the cursor at the brace */
		sci_set_current_position(doc->editor->sci, new_pos, FALSE);
		editor_display_current_line(doc->editor, 0.5F);
	}
}


static void cb_func_clipboard(guint key_id)
{
	GeanyDocument *doc = document_get_current();

	if (doc == NULL)
		return;

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
			sci_send_command(doc->editor->sci, SCI_LINECOPY);
			break;
		case GEANY_KEYS_CLIPBOARD_CUTLINE:
			sci_send_command(doc->editor->sci, SCI_LINECUT);
			break;
	}
}


/* Common function for goto keybindings, useful even when sci doesn't have focus. */
static void cb_func_goto_action(guint key_id)
{
	gint cur_line;
	GeanyDocument *doc = document_get_current();

	if (doc == NULL)
		return;

	cur_line = sci_get_current_line(doc->editor->sci);

	switch (key_id)
	{
		case GEANY_KEYS_GOTO_BACK:
			navqueue_go_back();
			return;
		case GEANY_KEYS_GOTO_FORWARD:
			navqueue_go_forward();
			return;
		case GEANY_KEYS_GOTO_LINE:
			on_go_to_line_activate(NULL, NULL);
			return;
		case GEANY_KEYS_GOTO_MATCHINGBRACE:
			goto_matching_brace(doc);
			return;
		case GEANY_KEYS_GOTO_TOGGLEMARKER:
		{
			gboolean set = sci_is_marker_set_at_line(doc->editor->sci, cur_line, 1);

			sci_set_marker_at_line(doc->editor->sci, cur_line, ! set, 1);
			return;
		}
		case GEANY_KEYS_GOTO_NEXTMARKER:
		{
			gint mline = sci_marker_next(doc->editor->sci, cur_line + 1, 1 << 1, TRUE);

			if (mline != -1)
			{
				sci_set_current_line(doc->editor->sci, mline);
				editor_display_current_line(doc->editor, 0.5F);
			}
			return;
		}
		case GEANY_KEYS_GOTO_PREVIOUSMARKER:
		{
			gint mline = sci_marker_previous(doc->editor->sci, cur_line - 1, 1 << 1, TRUE);

			if (mline != -1)
			{
				sci_set_current_line(doc->editor->sci, mline);
				editor_display_current_line(doc->editor, 0.5F);
			}
			return;
		}
		case GEANY_KEYS_GOTO_TAGDEFINITION:
			if (check_current_word())
				symbols_goto_tag(editor_info.current_word, TRUE);
			return;
		case GEANY_KEYS_GOTO_TAGDECLARATION:
			if (check_current_word())
				symbols_goto_tag(editor_info.current_word, FALSE);
			return;
	}
	/* only check editor-sensitive keybindings when editor has focus */
	if (gtk_window_get_focus(GTK_WINDOW(main_widgets.window)) != GTK_WIDGET(doc->editor->sci))
	{
		ignore_keybinding = TRUE;
		return;
	}
	switch (key_id)
	{
		case GEANY_KEYS_GOTO_LINESTART:
			sci_send_command(doc->editor->sci, editor_prefs.smart_home_key ? SCI_VCHOME : SCI_HOME);
			break;
		case GEANY_KEYS_GOTO_LINEEND:
			sci_send_command(doc->editor->sci, SCI_LINEEND);
			break;
		case GEANY_KEYS_GOTO_PREVWORDSTART:
			sci_send_command(doc->editor->sci, SCI_WORDPARTLEFT);
			break;
		case GEANY_KEYS_GOTO_NEXTWORDSTART:
			sci_send_command(doc->editor->sci, SCI_WORDPARTRIGHT);
			break;
	}
}


static void duplicate_lines(GeanyEditor *editor)
{
	if (sci_get_lines_selected(editor->sci) > 1)
	{	/* ignore extra_line because of selecting lines from the line number column */
		editor_select_lines(editor, FALSE);
		sci_selection_duplicate(editor->sci);
	}
	else if (sci_has_selection(editor->sci))
		sci_selection_duplicate(editor->sci);
	else
		sci_line_duplicate(editor->sci);
}


static void delete_lines(GeanyEditor *editor)
{
	editor_select_lines(editor, TRUE); /* include last line (like cut lines, copy lines do) */
	sci_clear(editor->sci);	/* (SCI_LINEDELETE only does 1 line) */
}


/* common function for editor keybindings, only valid when scintilla has focus. */
static void cb_func_editor_action(guint key_id)
{
	GeanyDocument *doc = document_get_current();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	/* edit keybindings only valid when scintilla widget has focus */
	if (doc == NULL || focusw != GTK_WIDGET(doc->editor->sci))
		return;

	switch (key_id)
	{
		case GEANY_KEYS_EDITOR_UNDO:
			on_undo1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDITOR_REDO:
			on_redo1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_EDITOR_SCROLLTOLINE:
			editor_scroll_to_line(doc->editor, -1, 0.5F);
			break;
		case GEANY_KEYS_EDITOR_SCROLLLINEUP:
			sci_send_command(doc->editor->sci, SCI_LINESCROLLUP);
			break;
		case GEANY_KEYS_EDITOR_SCROLLLINEDOWN:
			sci_send_command(doc->editor->sci, SCI_LINESCROLLDOWN);
			break;
		case GEANY_KEYS_EDITOR_DUPLICATELINE:
			duplicate_lines(doc->editor);
			break;
		case GEANY_KEYS_EDITOR_DELETELINE:
			delete_lines(doc->editor);
			break;
		case GEANY_KEYS_EDITOR_TRANSPOSELINE:
			sci_send_command(doc->editor->sci, SCI_LINETRANSPOSE);
			break;
		case GEANY_KEYS_EDITOR_AUTOCOMPLETE:
			editor_start_auto_complete(doc->editor, sci_get_current_position(doc->editor->sci), TRUE);
			break;
		case GEANY_KEYS_EDITOR_CALLTIP:
			editor_show_calltip(doc->editor, -1);
			break;
		case GEANY_KEYS_EDITOR_MACROLIST:
			editor_show_macro_list(doc->editor);
			break;
		case GEANY_KEYS_EDITOR_CONTEXTACTION:
			if (check_current_word())
				on_context_action1_activate(GTK_MENU_ITEM(ui_lookup_widget(main_widgets.editor_menu,
					"context_action1")), NULL);
			break;
		case GEANY_KEYS_EDITOR_SUPPRESSSNIPPETCOMPLETION:
		{
			GeanyKeyBinding *kb = keybindings_lookup_item(GEANY_KEY_GROUP_EDITOR,
				GEANY_KEYS_EDITOR_COMPLETESNIPPET);

			switch (kb->key)
			{
				case GDK_space:
					sci_add_text(doc->editor->sci, " ");
					break;
				case GDK_Tab:
					sci_send_command(doc->editor->sci, SCI_TAB);
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
	GeanyDocument *doc = document_get_current();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	/* keybindings only valid when scintilla widget has focus */
	if (doc == NULL || focusw != GTK_WIDGET(doc->editor->sci))
		return;

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
			editor_indentation_by_one_space(doc->editor, -1, FALSE);
			break;
		case GEANY_KEYS_FORMAT_DECREASEINDENTBYSPACE:
			editor_indentation_by_one_space(doc->editor, -1, TRUE);
			break;
		case GEANY_KEYS_FORMAT_AUTOINDENT:
			editor_smart_line_indentation(doc->editor, -1);
			break;
		case GEANY_KEYS_FORMAT_TOGGLECASE:
			on_toggle_case1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_FORMAT_SENDTOCMD1:
			if (ui_prefs.custom_commands && g_strv_length(ui_prefs.custom_commands) > 0)
				tools_execute_custom_command(doc, ui_prefs.custom_commands[0]);
			break;
		case GEANY_KEYS_FORMAT_SENDTOCMD2:
			if (ui_prefs.custom_commands && g_strv_length(ui_prefs.custom_commands) > 1)
				tools_execute_custom_command(doc, ui_prefs.custom_commands[1]);
			break;
		case GEANY_KEYS_FORMAT_SENDTOCMD3:
			if (ui_prefs.custom_commands && g_strv_length(ui_prefs.custom_commands) > 2)
				tools_execute_custom_command(doc, ui_prefs.custom_commands[2]);
			break;
	}
}


/* common function for select keybindings, only valid when scintilla has focus. */
static void cb_func_select_action(guint key_id)
{
	GeanyDocument *doc = document_get_current();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));
	static GtkWidget *scribble_widget = NULL;

	/* special case for Select All in the scribble widget */
	if (scribble_widget == NULL) /* lookup the scribble widget only once */
		scribble_widget = ui_lookup_widget(main_widgets.window, "textview_scribble");
	if (key_id == GEANY_KEYS_SELECT_ALL && focusw == scribble_widget)
	{
		g_signal_emit_by_name(scribble_widget, "select-all", TRUE);
		return;
	}

	/* keybindings only valid when scintilla widget has focus */
	if (doc == NULL || focusw != GTK_WIDGET(doc->editor->sci))
		return;

	switch (key_id)
	{
		case GEANY_KEYS_SELECT_ALL:
			on_menu_select_all1_activate(NULL, NULL);
			break;
		case GEANY_KEYS_SELECT_WORD:
			editor_select_word(doc->editor);
			break;
		case GEANY_KEYS_SELECT_LINE:
			editor_select_lines(doc->editor, FALSE);
			break;
		case GEANY_KEYS_SELECT_PARAGRAPH:
			editor_select_paragraph(doc->editor);
			break;
	}
}


static void cb_func_document_action(guint key_id)
{
	GeanyDocument *doc = document_get_current();
	if (doc == NULL)
		return;

	switch (key_id)
	{
		case GEANY_KEYS_DOCUMENT_REPLACETABS:
			on_replace_tabs_activate(NULL, NULL);
			break;
		case GEANY_KEYS_DOCUMENT_REPLACESPACES:
			on_replace_spaces_activate(NULL, NULL);
			break;
		case GEANY_KEYS_DOCUMENT_LINEBREAK:
			on_line_breaking1_activate(NULL, NULL);
			ui_document_show_hide(doc);
			break;
		case GEANY_KEYS_DOCUMENT_LINEWRAP:
			on_line_wrapping1_toggled(NULL, NULL);
			ui_document_show_hide(doc);
			break;
		case GEANY_KEYS_DOCUMENT_RELOADTAGLIST:
			document_update_tag_list(doc, TRUE);
			break;
		case GEANY_KEYS_DOCUMENT_FOLDALL:
			editor_fold_all(doc->editor);
			break;
		case GEANY_KEYS_DOCUMENT_UNFOLDALL:
			editor_unfold_all(doc->editor);
			break;
		case GEANY_KEYS_DOCUMENT_TOGGLEFOLD:
			if (editor_prefs.folding)
			{
				gint line = sci_get_current_line(doc->editor->sci);
				sci_toggle_fold(doc->editor->sci, line);
				break;
			}
	}
}


/* common function for insert keybindings, only valid when scintilla has focus. */
static void cb_func_insert_action(guint key_id)
{
	GeanyDocument *doc = document_get_current();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	/* keybindings only valid when scintilla widget has focus */
	if (doc == NULL || focusw != GTK_WIDGET(doc->editor->sci)) return;

	switch (key_id)
	{
		case GEANY_KEYS_INSERT_ALTWHITESPACE:
			editor_insert_alternative_whitespace(doc->editor);
			break;
		case GEANY_KEYS_INSERT_DATE:
			gtk_menu_item_activate(GTK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "insert_date_custom1")));
			break;
	}
}

