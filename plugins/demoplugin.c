/*
 *      demoplugin.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */

/* Demo plugin. */

#include "geany.h"
#include "support.h"
#include "plugindata.h"


PluginFields	*plugin_fields;
GeanyData		*geany_data;


/* Check that Geany supports plugin API version 2 or later, and check
 * for binary compatibility. */
VERSION_CHECK(7)

/* All plugins must set name, description and version */
PLUGIN_INFO(_("Demo"), _("Example plugin."), "0.1")


/* Callback when the menu item is clicked */
static void
item_activate(GtkMenuItem *menuitem, gpointer gdata)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(geany_data->app->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		_("Hello World!"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), info()->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/* Called by Geany to initialize the plugin */
void init(GeanyData *data)
{
	GtkWidget *demo_item;

	// Add an item to the Tools menu
	demo_item = gtk_menu_item_new_with_mnemonic(_("_Demo Plugin"));
	gtk_widget_show(demo_item);
	gtk_container_add(GTK_CONTAINER(geany_data->tools_menu), demo_item);
	g_signal_connect(G_OBJECT(demo_item), "activate", G_CALLBACK(item_activate), NULL);

	// keep a pointer to the menu item, so we can remove it when the plugin is unloaded
	plugin_fields->menu_item = demo_item;
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done */
void cleanup()
{
	// remove the menu item added in init()
	gtk_widget_destroy(plugin_fields->menu_item);
}
