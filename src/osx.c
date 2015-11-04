/*
 *      osx.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2015 Jiri Techet <techet(at)gmail(dot)com>
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

#ifdef MAC_INTEGRATION

#include "osx.h"

#include "utils.h"
#include "ui_utils.h"
#include "main.h"


static gboolean app_block_termination_cb(GtkosxApplication *app, gpointer data)
{
	return !main_quit();
}


/* For some reason osx doesn't like when the NSApplicationOpenFile handler blocks for 
 * a long time which may be caused by the project_ask_close() below. Finish the
 * NSApplicationOpenFile handler immediately and perform the potentially blocking
 * code on idle in this function. */
static gboolean open_project_idle(gchar *locale_path)
{
	gchar *utf8_path;

	utf8_path = utils_get_utf8_from_locale(locale_path);
	if (app->project == NULL || 
		(g_strcmp0(utf8_path, app->project->file_name) != 0 && project_ask_close()))
		project_load_file_with_session(locale_path);
	g_free(utf8_path);
	g_free(locale_path);
	return FALSE;
}


static gboolean app_open_file_cb(GtkosxApplication *osx_app, gchar *path, gpointer user_data)
{
	gchar opened = FALSE;
	gchar *locale_path;

	locale_path = utils_get_locale_from_utf8(path);

	if (!g_path_is_absolute(locale_path))
	{
		gchar *cwd = g_get_current_dir();
		SETPTR(locale_path, g_build_filename(cwd, locale_path, NULL));
		g_free(cwd);
	}

	if (g_str_has_suffix(path, ".geany"))
	{
		g_idle_add((GSourceFunc)open_project_idle, locale_path);
		opened = TRUE;
	}
	else
	{
		opened = document_open_file(locale_path, FALSE, NULL, NULL) != NULL;
		g_free(locale_path);
	}

	return opened;
}


static void on_new_window(GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	utils_start_new_geany_instance(NULL);
}


void osx_ui_init(void)
{
	GtkWidget *item, *menu;
	GtkosxApplication *osx_app = gtkosx_application_get();

	item = ui_lookup_widget(main_widgets.window, "menubar1");
	gtk_widget_hide(item);
	gtkosx_application_set_menu_bar(osx_app, GTK_MENU_SHELL(item));

	item = ui_lookup_widget(main_widgets.window, "menu_quit1");
	gtk_widget_hide(item);

	item = ui_lookup_widget(main_widgets.window, "menu_info1");
	gtkosx_application_insert_app_menu_item(osx_app, item, 0);

	item = ui_lookup_widget(main_widgets.window, "menu_help1");
	gtkosx_application_set_help_menu(osx_app, GTK_MENU_ITEM(item));

	gtkosx_application_set_use_quartz_accelerators(osx_app, FALSE);

	g_signal_connect(osx_app, "NSApplicationBlockTermination",
					G_CALLBACK(app_block_termination_cb), NULL);
	g_signal_connect(osx_app, "NSApplicationOpenFile",
					G_CALLBACK(app_open_file_cb), NULL);

	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_label("New Window");
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_new_window), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtkosx_application_set_dock_menu(osx_app, GTK_MENU_SHELL(menu));
}

#endif /* MAC_INTEGRATION */
