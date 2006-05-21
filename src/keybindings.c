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
#include "msgwindow.h"
#include "sci_cb.h"
#include "sciwrappers.h"
// include vte.h on non-Win32 systems, else define fake vte_init
#ifdef HAVE_VTE
# include "vte.h"
#endif



/* simple convenience function to allocate and fill the struct */
static binding *fill(void (*func) (void), guint key, GdkModifierType mod, const gchar *name);

static void cb_func_menu_new(void);
static void cb_func_menu_open(void);
static void cb_func_menu_save(void);
static void cb_func_menu_saveall(void);
static void cb_func_menu_closeall(void);
static void cb_func_menu_reloadfile(void);
static void cb_func_menu_undo(void);
static void cb_func_menu_redo(void);
static void cb_func_menu_findnext(void);
static void cb_func_menu_replace(void);
static void cb_func_menu_preferences(void);
static void cb_func_menu_opencolorchooser(void);
static void cb_func_menu_fullscreen(void);
static void cb_func_menu_messagewindow(void);
static void cb_func_menu_zoomin(void);
static void cb_func_menu_zoomout(void);
static void cb_func_menu_foldall(void);
static void cb_func_menu_unfoldall(void);
static void cb_func_build_compile(void);
static void cb_func_build_link(void);
static void cb_func_build_make(void);
static void cb_func_build_makeowntarget(void);
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


void keybindings_init(void)
{
	gchar *configfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "keybindings.conf", NULL);
	gchar *val;
	guint i;
	guint key;
	GdkModifierType mods;
	GKeyFile *config = g_key_file_new();
	GtkAccelGroup *accel_group = gtk_accel_group_new();

	// init all fields of keys with default values
	keys[GEANY_KEYS_MENU_NEW] = fill(cb_func_menu_new, GDK_n, GDK_CONTROL_MASK, "menu_new");
	keys[GEANY_KEYS_MENU_OPEN] = fill(cb_func_menu_open, GDK_o, GDK_CONTROL_MASK, "menu_open");
	keys[GEANY_KEYS_MENU_SAVE] = fill(cb_func_menu_save, GDK_s, GDK_CONTROL_MASK, "menu_save");
	keys[GEANY_KEYS_MENU_SAVEALL] = fill(cb_func_menu_saveall, GDK_S, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "menu_saveall");
	keys[GEANY_KEYS_MENU_CLOSEALL] = fill(cb_func_menu_closeall, GDK_d, GDK_MOD1_MASK, "menu_closeall");
	keys[GEANY_KEYS_MENU_RELOADFILE] = fill(cb_func_menu_reloadfile, GDK_r, GDK_CONTROL_MASK, "menu_reloadfile");
	keys[GEANY_KEYS_MENU_UNDO] = fill(cb_func_menu_undo, GDK_z, GDK_CONTROL_MASK, "menu_undo");
	keys[GEANY_KEYS_MENU_REDO] = fill(cb_func_menu_redo, GDK_y, GDK_CONTROL_MASK, "menu_redo");
	keys[GEANY_KEYS_MENU_FIND_NEXT] = fill(cb_func_menu_findnext, GDK_F3, 0, "menu_findnext");
	keys[GEANY_KEYS_MENU_REPLACE] = fill(cb_func_menu_replace, GDK_F3, GDK_CONTROL_MASK, "menu_replace");
	keys[GEANY_KEYS_MENU_PREFERENCES] = fill(cb_func_menu_preferences, GDK_p, GDK_CONTROL_MASK, "menu_preferences");
	keys[GEANY_KEYS_MENU_OPENCOLORCHOOSER] = fill(cb_func_menu_opencolorchooser, 0, 0, "menu_opencolorchooser");
	keys[GEANY_KEYS_MENU_FULLSCREEN] = fill(cb_func_menu_fullscreen, GDK_F11, 0, "menu_fullscreen");
	keys[GEANY_KEYS_MENU_MESSAGEWINDOW] = fill(cb_func_menu_messagewindow, 0, 0, "menu_messagewindow");
	keys[GEANY_KEYS_MENU_ZOOMIN] = fill(cb_func_menu_zoomin, GDK_plus, GDK_CONTROL_MASK, "menu_zoomin");
	keys[GEANY_KEYS_MENU_ZOOMOUT] = fill(cb_func_menu_zoomout, GDK_minus, GDK_CONTROL_MASK, "menu_zoomout");
	keys[GEANY_KEYS_MENU_FOLDALL] = fill(cb_func_menu_foldall, 0, 0, "menu_foldall");
	keys[GEANY_KEYS_MENU_UNFOLDALL] = fill(cb_func_menu_unfoldall, 0, 0, "menu_unfoldall");
	keys[GEANY_KEYS_BUILD_COMPILE] = fill(cb_func_build_compile, GDK_F8, 0, "build_compile");
	keys[GEANY_KEYS_BUILD_LINK] = fill(cb_func_build_link, GDK_F9, 0, "build_link");
	keys[GEANY_KEYS_BUILD_MAKE] = fill(cb_func_build_make, GDK_F9, GDK_SHIFT_MASK, "build_make");
	keys[GEANY_KEYS_BUILD_MAKEOWNTARGET] = fill(cb_func_build_makeowntarget, GDK_F9, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "build_makeowntarget");
	keys[GEANY_KEYS_BUILD_RUN] = fill(cb_func_build_run, GDK_F5, 0, "build_run");
	keys[GEANY_KEYS_BUILD_RUN2] = fill(cb_func_build_run2, 0, 0, "build_run2");
	keys[GEANY_KEYS_BUILD_OPTIONS] = fill(cb_func_build_options, 0, 0, "build_options");
	keys[GEANY_KEYS_RELOADTAGLIST] = fill(cb_func_reloadtaglist, GDK_r, GDK_SHIFT_MASK | GDK_CONTROL_MASK, "reloadtaglist");
	keys[GEANY_KEYS_SWITCH_EDITOR] = fill(cb_func_switch_editor, GDK_F2, 0, "switch_editor");
	keys[GEANY_KEYS_SWITCH_SCRIBBLE] = fill(cb_func_switch_scribble, GDK_F6, 0, "switch_scribble");
	keys[GEANY_KEYS_SWITCH_VTE] = fill(cb_func_switch_vte, GDK_F4, 0, "switch_vte");
	keys[GEANY_KEYS_SWITCH_TABLEFT] = fill(cb_func_switch_tableft, 0, 0, "switch_tableft");
	keys[GEANY_KEYS_SWITCH_TABRIGHT] = fill(cb_func_switch_tabright, 0, 0, "switch_tabright");
	keys[GEANY_KEYS_TOOGLE_SIDEBAR] = fill(cb_func_toggle_sidebar, 0, 0, "toggle_sidebar");
	keys[GEANY_KEYS_EDIT_DUPLICATELINE] = fill(cb_func_edit_duplicateline, GDK_g, GDK_CONTROL_MASK, "edit_duplicateline");
	keys[GEANY_KEYS_EDIT_COMMENTLINE] = fill(cb_func_edit_commentline, GDK_d, GDK_CONTROL_MASK, "edit_commentline");
	keys[GEANY_KEYS_EDIT_AUTOCOMPLETE] = fill(cb_func_edit_autocomplete, GDK_space, GDK_CONTROL_MASK, "edit_autocomplete");
	keys[GEANY_KEYS_EDIT_CALLTIP] = fill(cb_func_edit_calltip, GDK_space, GDK_MOD1_MASK, "edit_calltip");
	keys[GEANY_KEYS_EDIT_MACROLIST] = fill(cb_func_edit_macrolist, GDK_Return, GDK_CONTROL_MASK, "edit_macrolist");
	keys[GEANY_KEYS_EDIT_SUPPRESSCOMPLETION] = fill(cb_func_edit_suppresscompletion, GDK_space, GDK_SHIFT_MASK, "edit_suppresscompletion");

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


	// apply the settings
	if (keys[GEANY_KEYS_MENU_SAVEALL]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_save_all1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_SAVEALL]->key, keys[GEANY_KEYS_MENU_SAVEALL]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_CLOSEALL]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_close_all1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_CLOSEALL]->key, keys[GEANY_KEYS_MENU_CLOSEALL]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_RELOADFILE]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "revert1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_RELOADFILE]->key, keys[GEANY_KEYS_MENU_RELOADFILE]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_PREFERENCES]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "preferences1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_PREFERENCES]->key, keys[GEANY_KEYS_MENU_PREFERENCES]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_FIND_NEXT]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "find_next1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_FIND_NEXT]->key, keys[GEANY_KEYS_MENU_FIND_NEXT]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_FULLSCREEN]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_fullscreen1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_FULLSCREEN]->key, keys[GEANY_KEYS_MENU_FULLSCREEN]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_MESSAGEWINDOW]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_show_messages_window1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_MESSAGEWINDOW]->key, keys[GEANY_KEYS_MENU_MESSAGEWINDOW]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_OPENCOLORCHOOSER]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_choose_color1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_OPENCOLORCHOOSER]->key, keys[GEANY_KEYS_MENU_OPENCOLORCHOOSER]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_ZOOMIN]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_zoom_in1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_ZOOMIN]->key, keys[GEANY_KEYS_MENU_ZOOMIN]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_ZOOMOUT]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_zoom_out1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_ZOOMOUT]->key, keys[GEANY_KEYS_MENU_ZOOMOUT]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_FOLDALL]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_fold_all1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_FOLDALL]->key, keys[GEANY_KEYS_MENU_FOLDALL]->mods, GTK_ACCEL_VISIBLE);

	if (keys[GEANY_KEYS_MENU_UNFOLDALL]->key != 0)
		gtk_widget_add_accelerator(lookup_widget(app->window, "menu_unfold_all1"), "activate", accel_group,
		keys[GEANY_KEYS_MENU_UNFOLDALL]->key, keys[GEANY_KEYS_MENU_UNFOLDALL]->mods, GTK_ACCEL_VISIBLE);

	// the build menu items are set if the build menus are created

	gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);

	g_free(configfile);
	g_key_file_free(config);
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
static binding *fill(void (*func) (void), guint key, GdkModifierType mod, const gchar *name)
{
	binding *result;

	result = g_new0(binding, 1);
	result->name = name;
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

static void cb_func_menu_findnext(void)
{
	on_find_next1_activate(NULL, NULL);
}

static void cb_func_menu_replace(void)
{
	dialogs_show_replace();
}

static void cb_func_menu_preferences(void)
{
	dialogs_show_prefs_dialog();
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
	document_fold_all(idx);
}

static void cb_func_menu_unfoldall(void)
{
	gint idx = document_get_cur_idx();
	document_unfold_all(idx);
}

static void cb_func_build_compile(void)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].file_type->menu_items->can_compile && doc_list[idx].file_name != NULL)
		on_build_compile_activate(NULL, NULL);
}

static void cb_func_build_link(void)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].file_type->menu_items->can_link && doc_list[idx].file_name != NULL)
		on_build_build_activate(NULL, NULL);
}

static void cb_func_build_make(void)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].file_name != NULL)
		on_build_make_activate(NULL, GINT_TO_POINTER(0));
}

static void cb_func_build_makeowntarget(void)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].file_name != NULL)
		on_build_make_activate(NULL, GINT_TO_POINTER(1));
}

static void cb_func_build_run(void)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].file_type->menu_items->can_exec && doc_list[idx].file_name != NULL)
		on_build_execute_activate(NULL, GINT_TO_POINTER(0));
}

static void cb_func_build_run2(void)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].file_type->menu_items->can_exec && doc_list[idx].file_name != NULL)
		on_build_execute_activate(NULL, GINT_TO_POINTER(1));
}

static void cb_func_build_options(void)
{
	gint idx = document_get_cur_idx();
	if ((doc_list[idx].file_type->menu_items->can_compile ||
		doc_list[idx].file_type->menu_items->can_link ||
		doc_list[idx].file_type->menu_items->can_exec) &&
		doc_list[idx].file_name != NULL)
		on_build_arguments_activate(NULL, NULL);
}

static void cb_func_reloadtaglist(void)
{
	gint idx = document_get_cur_idx();
	document_update_tag_list(idx, TRUE);
}

static void cb_func_switch_editor(void)
{
	gint idx = document_get_cur_idx();
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
	app->treeview_symbol_visible = ! app->treeview_symbol_visible;
	app->treeview_openfiles_visible = ! app->treeview_openfiles_visible;

	utils_treeviews_showhide();;
}

static void cb_func_edit_duplicateline(void)
{
	gint idx = document_get_cur_idx();
	sci_line_duplicate(doc_list[idx].sci);
}

static void cb_func_edit_commentline(void)
{
	gint idx = document_get_cur_idx();
	sci_cb_do_comment(idx);
}

static void cb_func_edit_autocomplete(void)
{
	gint idx = document_get_cur_idx();
	sci_cb_start_auto_complete(doc_list[idx].sci, sci_get_current_position(doc_list[idx].sci));
}

static void cb_func_edit_calltip(void)
{
	gint idx = document_get_cur_idx();
	sci_cb_show_calltip(doc_list[idx].sci, -1);
}

static void cb_func_edit_macrolist(void)
{
	gint idx = document_get_cur_idx();
	sci_cb_show_macro_list(doc_list[idx].sci);
}

static void cb_func_edit_suppresscompletion(void)
{
	gint idx = document_get_cur_idx();
	sci_add_text(doc_list[idx].sci, " ");
}

