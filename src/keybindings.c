/*
 *      keybindings.c - this file is part of Geany, a fast and lightweight IDE
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


#include <gdk/gdkkeysyms.h>

#include "geany.h"
#include "keybindings.h"
#include "support.h"
#include "utils.h"
#include "document.h"
#include "callbacks.h"
#include "dialogs.h"
#include "prefs.h"
#include "msgwindow.h"
#include "sci_cb.h"
#include "sciwrappers.h"
// include vte.h on non-Win32 systems, else define fake vte_init
#ifdef HAVE_VTE
# include "vte.h"
#endif



/* simple convenience function to allocate and fill the struct */
static binding *fill(void (*func) (void), guint key, GdkModifierType mod, const gchar *name, const gchar *label);

static void cb_func_menu_new(void);
static void cb_func_menu_open(void);
static void cb_func_menu_save(void);
static void cb_func_menu_saveall(void);
static void cb_func_menu_print(void);
static void cb_func_menu_close(void);
static void cb_func_menu_closeall(void);
static void cb_func_menu_reloadfile(void);
static void cb_func_menu_undo(void);
static void cb_func_menu_redo(void);
static void cb_func_menu_selectall(void);
static void cb_func_menu_preferences(void);
static void cb_func_menu_findnext(void);
static void cb_func_menu_findprevious(void);
static void cb_func_menu_replace(void);
static void cb_func_menu_findinfiles(void);
static void cb_func_menu_gotoline(void);
static void cb_func_menu_opencolorchooser(void);
static void cb_func_menu_fullscreen(void);
static void cb_func_menu_messagewindow(void);
static void cb_func_menu_zoomin(void);
static void cb_func_menu_zoomout(void);
static void cb_func_menu_replacetabs(void);
static void cb_func_menu_foldall(void);
static void cb_func_menu_unfoldall(void);
static void cb_func_build_compile(void);
static void cb_func_build_link(void);
static void cb_func_build_make(void);
static void cb_func_build_makeowntarget(void);
static void cb_func_build_makeobject(void);
static void cb_func_build_run(void);
static void cb_func_build_run2(void);
static void cb_func_build_options(void);
static void cb_func_reloadtaglist(void);
static void cb_func_switch_editor(void);
static void cb_func_switch_scribble(void);
static void cb_func_switch_vte(void);
static void cb_func_switch_tableft(void);
static void cb_func_switch_tabright(void);
static void cb_func_toggle_sidebar(void);
static void cb_func_edit_duplicateline(void);
static void cb_func_edit_commentline(void);
static void cb_func_edit_autocomplete(void);
static void cb_func_edit_calltip(void);
static void cb_func_edit_macrolist(void);
static void cb_func_edit_suppresscompletion(void);
static void cb_func_popup_findusage(void);
static void cb_func_popup_gototagdefinition(void);
static void cb_func_popup_gototagdeclaration(void);

static void keybindings_call_popup_item(int menuitemkey);
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
	keys[GEANY_KEYS_MENU_SAVE] = fill(cb_func_menu_save,
		GDK_s, GDK_CONTROL_MASK, "menu_save", _("Save"));
	keys[GEANY_KEYS_MENU_SAVEALL] = fill(cb_func_menu_saveall,
		GDK_S, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "menu_saveall", _("Save all"));
	keys[GEANY_KEYS_MENU_PRINT] = fill(cb_func_menu_print,
		GDK_p, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "menu_print", _("Print"));
	keys[GEANY_KEYS_MENU_CLOSE] = fill(cb_func_menu_close,
		GDK_w, GDK_CONTROL_MASK, "menu_close", _("Close"));
	keys[GEANY_KEYS_MENU_CLOSEALL] = fill(cb_func_menu_closeall,
		GDK_d, GDK_MOD1_MASK, "menu_closeall", _("Close all"));
	keys[GEANY_KEYS_MENU_RELOADFILE] = fill(cb_func_menu_reloadfile,
		GDK_r, GDK_CONTROL_MASK, "menu_reloadfile", _("Reload file"));
	keys[GEANY_KEYS_MENU_UNDO] = fill(cb_func_menu_undo,
		GDK_z, GDK_CONTROL_MASK, "menu_undo", _("Undo"));
	keys[GEANY_KEYS_MENU_REDO] = fill(cb_func_menu_redo,
		GDK_y, GDK_CONTROL_MASK, "menu_redo", _("Redo"));
	keys[GEANY_KEYS_MENU_SELECTALL] = fill(cb_func_menu_selectall,
		GDK_a, GDK_CONTROL_MASK, "menu_selectall", _("Select All"));
	keys[GEANY_KEYS_MENU_PREFERENCES] = fill(cb_func_menu_preferences,
		GDK_p, GDK_CONTROL_MASK, "menu_preferences", _("Preferences"));
	keys[GEANY_KEYS_MENU_FINDNEXT] = fill(cb_func_menu_findnext,
		GDK_F3, 0, "menu_findnext", _("Find Next"));
	keys[GEANY_KEYS_MENU_FINDPREVIOUS] = fill(cb_func_menu_findprevious,
		GDK_F3, GDK_SHIFT_MASK, "menu_findprevious", _("Find Previous"));
	keys[GEANY_KEYS_MENU_REPLACE] = fill(cb_func_menu_replace,
		GDK_e, GDK_CONTROL_MASK, "menu_replace", _("Replace"));
	keys[GEANY_KEYS_MENU_FINDINFILES] = fill(cb_func_menu_findinfiles, GDK_f,
		GDK_CONTROL_MASK | GDK_SHIFT_MASK, "menu_findinfiles", _("Find in files"));
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
	keys[GEANY_KEYS_BUILD_COMPILE] = fill(cb_func_build_compile,
		GDK_F8, 0, "build_compile", _("Compile"));
	keys[GEANY_KEYS_BUILD_LINK] = fill(cb_func_build_link,
		GDK_F9, 0, "build_link", _("Build"));
	keys[GEANY_KEYS_BUILD_MAKE] = fill(cb_func_build_make,
		GDK_F9, GDK_SHIFT_MASK, "build_make", _("Make all"));
	keys[GEANY_KEYS_BUILD_MAKEOWNTARGET] = fill(cb_func_build_makeowntarget,
		GDK_F9, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "build_makeowntarget",
		_("Make custom target"));
	keys[GEANY_KEYS_BUILD_MAKEOBJECT] = fill(cb_func_build_makeobject,
		0, 0, "build_makeobject", _("Make object"));
	keys[GEANY_KEYS_BUILD_RUN] = fill(cb_func_build_run,
		GDK_F5, 0, "build_run", _("Run"));
	keys[GEANY_KEYS_BUILD_RUN2] = fill(cb_func_build_run2,
		0, 0, "build_run2", _("Run (alternative command)"));
	keys[GEANY_KEYS_BUILD_OPTIONS] = fill(cb_func_build_options,
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
	keys[GEANY_KEYS_EDIT_DUPLICATELINE] = fill(cb_func_edit_duplicateline,
		GDK_g, GDK_CONTROL_MASK, "edit_duplicateline", _("Duplicate line or selection"));
	keys[GEANY_KEYS_EDIT_COMMENTLINE] = fill(cb_func_edit_commentline,
		GDK_d, GDK_CONTROL_MASK, "edit_commentline", _("Comment line"));
	keys[GEANY_KEYS_EDIT_AUTOCOMPLETE] = fill(cb_func_edit_autocomplete,
		GDK_space, GDK_CONTROL_MASK, "edit_autocomplete", _("Complete word"));
	keys[GEANY_KEYS_EDIT_CALLTIP] = fill(cb_func_edit_calltip,
		GDK_space, GDK_MOD1_MASK, "edit_calltip", _("Show calltip"));
	keys[GEANY_KEYS_EDIT_MACROLIST] = fill(cb_func_edit_macrolist,
		GDK_Return, GDK_CONTROL_MASK, "edit_macrolist", _("Show macro list"));
	keys[GEANY_KEYS_EDIT_SUPPRESSCOMPLETION] = fill(cb_func_edit_suppresscompletion,
		GDK_space, GDK_SHIFT_MASK, "edit_suppresscompletion", _("Suppress auto completion"));
	keys[GEANY_KEYS_POPUP_FINDUSAGE] = fill(cb_func_popup_findusage,
		0, 0, "popup_findusage", _("Find Usage"));
	keys[GEANY_KEYS_POPUP_GOTOTAGDEFINITION] = fill(cb_func_popup_gototagdefinition,
		0, 0, "popup_gototagdefinition", _("Go to tag definition"));
	keys[GEANY_KEYS_POPUP_GOTOTAGDECLARATION] = fill(cb_func_popup_gototagdeclaration,
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
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_SAVEALL, menu_save_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_PRINT, print1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_CLOSE, menu_close1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_CLOSEALL, menu_close_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_RELOADFILE, revert1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_UNDO, menu_undo2);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_REDO, menu_redo2);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_SELECTALL, menu_select_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_PREFERENCES, preferences1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDNEXT, find_next1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDPREVIOUS, find_previous1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_REPLACE, replace1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FINDINFILES, find_in_files1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_GOTOLINE, go_to_line1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FULLSCREEN, menu_fullscreen1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_MESSAGEWINDOW, menu_show_messages_window1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_OPENCOLORCHOOSER, menu_choose_color1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_ZOOMIN, menu_zoom_in1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_ZOOMOUT, menu_zoom_out1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_REPLACETABS, menu_replace_tabs);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_FOLDALL, menu_fold_all1);
	GEANY_ADD_ACCEL(GEANY_KEYS_MENU_UNFOLDALL, menu_unfold_all1);

	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_UNDO, undo1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_REDO, redo1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_SELECTALL, menu_select_all2);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_ZOOMIN, zoom_in1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_ZOOMOUT, zoom_out1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_POPUP_FINDUSAGE, find_usage1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_POPUP_GOTOTAGDEFINITION, goto_tag_definition1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_POPUP_GOTOTAGDECLARATION, goto_tag_declaration1);
	GEANY_ADD_POPUP_ACCEL(GEANY_KEYS_MENU_GOTOLINE, go_to_line);

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


/* central keypress event handler, almost all keypress events go to this function */
gboolean keybindings_got_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	guint i, k;

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
			if (keys[i]->cb_func != NULL) keys[i]->cb_func();
			return TRUE;
		}
	}
	return FALSE;
}


/* simple convenience function to allocate and fill the struct */
static binding *fill(void (*func) (void), guint key, GdkModifierType mod, const gchar *name, const gchar *label)
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
static void cb_func_menu_new(void)
{
	document_new_file(NULL);
}

static void cb_func_menu_open(void)
{
	dialogs_show_open_file();
}

static void cb_func_menu_save(void)
{
	on_save1_activate(NULL, NULL);
}

static void cb_func_menu_saveall(void)
{
	on_save_all1_activate(NULL, NULL);
}

static void cb_func_menu_close(void)
{
	on_close1_activate(NULL, NULL);
}

static void cb_func_menu_closeall(void)
{
	on_close_all1_activate(NULL, NULL);
}

static void cb_func_menu_reloadfile(void)
{
	on_toolbutton23_clicked(NULL, NULL);
}

static void cb_func_menu_undo(void)
{
	on_undo1_activate(NULL, NULL);
}

static void cb_func_menu_redo(void)
{
	on_redo1_activate(NULL, NULL);
}

static void cb_func_menu_selectall(void)
{
	on_menu_select_all1_activate(NULL, NULL);
}

static void cb_func_menu_preferences(void)
{
	dialogs_show_prefs_dialog();
}

static void cb_func_menu_findnext(void)
{
	on_find_next1_activate(NULL, NULL);
}

static void cb_func_menu_findprevious(void)
{
	on_find_previous1_activate(NULL, NULL);
}

static void cb_func_menu_replace(void)
{
	dialogs_show_replace();
}

static void cb_func_menu_findinfiles(void)
{
	dialogs_show_find_in_files();
}

static void cb_func_menu_gotoline(void)
{
	on_go_to_line1_activate(NULL, NULL);
}

static void cb_func_menu_opencolorchooser(void)
{
	dialogs_show_color();
}

static void cb_func_menu_fullscreen(void)
{
	GtkCheckMenuItem *c = GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_fullscreen1"));

	gtk_check_menu_item_set_active(c, ! gtk_check_menu_item_get_active(c));
}

static void cb_func_menu_messagewindow(void)
{
	GtkCheckMenuItem *c = GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_messages_window1"));

	gtk_check_menu_item_set_active(c, ! gtk_check_menu_item_get_active(c));
}

static void cb_func_menu_zoomin(void)
{
	on_zoom_in1_activate(NULL, NULL);
}

static void cb_func_menu_zoomout(void)
{
	on_zoom_out1_activate(NULL, NULL);
}

static void cb_func_menu_foldall(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_fold_all(idx);
}

static void cb_func_menu_unfoldall(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_unfold_all(idx);
}

static void cb_func_build_compile(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (doc_list[idx].file_type->menu_items->can_compile && doc_list[idx].file_name != NULL)
		on_build_compile_activate(NULL, NULL);
}

static void cb_func_build_link(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (doc_list[idx].file_type->menu_items->can_link && doc_list[idx].file_name != NULL)
		on_build_build_activate(NULL, NULL);
}

static void cb_func_build_make(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (doc_list[idx].file_name != NULL)
		on_build_make_activate(NULL, GINT_TO_POINTER(0));
}

static void cb_func_build_makeowntarget(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (doc_list[idx].file_name != NULL)
		on_build_make_activate(NULL, GINT_TO_POINTER(1));
}

static void cb_func_build_makeobject(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (doc_list[idx].file_name != NULL)
		on_build_make_activate(NULL, GINT_TO_POINTER(2));
}

static void cb_func_build_run(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (doc_list[idx].file_type->menu_items->can_exec && doc_list[idx].file_name != NULL)
		on_build_execute_activate(NULL, GINT_TO_POINTER(0));
}

static void cb_func_build_run2(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (doc_list[idx].file_type->menu_items->can_exec && doc_list[idx].file_name != NULL)
		on_build_execute_activate(NULL, GINT_TO_POINTER(1));
}

static void cb_func_build_options(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if ((doc_list[idx].file_type->menu_items->can_compile ||
		doc_list[idx].file_type->menu_items->can_link ||
		doc_list[idx].file_type->menu_items->can_exec) &&
		doc_list[idx].file_name != NULL)
		on_build_arguments_activate(NULL, NULL);
}

static void cb_func_reloadtaglist(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_update_tag_list(idx, TRUE);
}


static void cb_func_popup_findusage(void)
{
	keybindings_call_popup_item(GEANY_KEYS_POPUP_FINDUSAGE);
}


static void cb_func_popup_gototagdefinition(void)
{
	keybindings_call_popup_item(GEANY_KEYS_POPUP_GOTOTAGDEFINITION);
}


static void cb_func_popup_gototagdeclaration(void)
{
	keybindings_call_popup_item(GEANY_KEYS_POPUP_GOTOTAGDECLARATION);
}


static void keybindings_call_popup_item(int menuitemkey)
{
	gint idx = document_get_cur_idx();
	gint pos;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	pos = sci_get_current_position(doc_list[idx].sci);

	utils_find_current_word(doc_list[idx].sci, pos,
		current_word, GEANY_MAX_WORD_LENGTH);

	if (*current_word == 0)
		utils_beep();
	else
		switch (menuitemkey)
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


static void cb_func_switch_editor(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
}

static void cb_func_switch_scribble(void)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_SCRATCH);
	gtk_widget_grab_focus(lookup_widget(app->window, "textview_scribble"));
}

static void cb_func_switch_vte(void)
{
#ifdef HAVE_VTE
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_VTE);
	gtk_widget_grab_focus(vc->vte);
#endif
}

static void cb_func_switch_tableft(void)
{
	utils_switch_document(LEFT);
}

static void cb_func_switch_tabright(void)
{
	utils_switch_document(RIGHT);
}

static void cb_func_toggle_sidebar(void)
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

	utils_treeviews_showhide();
	gtk_notebook_set_current_page(GTK_NOTEBOOK(app->treeview_notebook), active_page);
}

static void cb_func_edit_duplicateline(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (sci_can_copy(doc_list[idx].sci))
		sci_selection_duplicate(doc_list[idx].sci);
	else
		sci_line_duplicate(doc_list[idx].sci);
}

static void cb_func_edit_commentline(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_cb_do_comment(idx);
}

static void cb_func_edit_autocomplete(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_cb_start_auto_complete(doc_list[idx].sci, sci_get_current_position(doc_list[idx].sci), idx);
}

static void cb_func_edit_calltip(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_cb_show_calltip(doc_list[idx].sci, -1, idx);
}

static void cb_func_edit_macrolist(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_cb_show_macro_list(doc_list[idx].sci);
}

static void cb_func_edit_suppresscompletion(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_add_text(doc_list[idx].sci, " ");
}

static void cb_func_menu_replacetabs(void)
{
	on_replace_tabs_activate(NULL, NULL);
}

static void cb_func_menu_print(void)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	document_print(idx);
}
