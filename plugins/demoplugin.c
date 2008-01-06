/*
 *      demoplugin.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/**
 * Demo plugin - example of a basic plugin for Geany. Adds a menu item to the
 * Tools menu.
 *
 * Note: This is not installed by default, but (on *nix) you can build it as follows:
 * cd plugins
 * make demoplugin.so
 *
 * Then copy or symlink the plugins/demoplugin.so file to ~/.geany/plugins
 * - it will be loaded at next startup.
 */


#include "geany.h"		// for the GeanyApp data type
#include "support.h"	// for the _() translation macro (see also po/POTFILES.in)

#include "plugindata.h"		// this defines the plugin API
#include "pluginmacros.h"	// some useful macros to avoid typing geany_data so often


/* These items are set by Geany before init() is called. */
PluginFields	*plugin_fields;
GeanyData		*geany_data;


/* Check that Geany supports plugin API version 7 or later, and check
 * for binary compatibility. */
VERSION_CHECK(7)

/* All plugins must set name, description, version and author. */
PLUGIN_INFO(_("Demo"), _("Example plugin."), VERSION, _("The Geany developer team"))


// text to be shown in the plugin dialog
static gchar *welcome_text = NULL;


/* Callback when the menu item is clicked. */
static void
item_activate(GtkMenuItem *menuitem, gpointer gdata)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(app->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		welcome_text);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), info()->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/* Called by Geany to initialize the plugin.
 * Note: data is the same as geany_data. */
void init(GeanyData *data)
{
	GtkWidget *demo_item;

	// Add an item to the Tools menu
	demo_item = gtk_menu_item_new_with_mnemonic(_("_Demo Plugin"));
	gtk_widget_show(demo_item);
	gtk_container_add(GTK_CONTAINER(geany_data->tools_menu), demo_item);
	g_signal_connect(G_OBJECT(demo_item), "activate", G_CALLBACK(item_activate), NULL);

	welcome_text = g_strdup(_("Hello World!"));

	// keep a pointer to the menu item, so we can remove it when the plugin is unloaded
	plugin_fields->menu_item = demo_item;
}


/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * init() was called.
 * You can omit this function if the plugin doesn't need to be configured.
 * Note: parent is the parent window which can be used as the transient window for the created
 *       dialog. */
void configure(GtkWidget *parent)
{
	GtkWidget *dialog, *label, *entry, *vbox;

	// example configuration dialog
	dialog = gtk_dialog_new_with_buttons(_("Demo"),
		GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui->dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	// add a label and a text entry t the dialog
	label = gtk_label_new("Welcome text to show:");
	gtk_widget_show(label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	entry = gtk_entry_new();
	gtk_widget_show(entry);
	if (welcome_text != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), welcome_text);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), entry);
	gtk_widget_show(vbox);

	// run the dialog and check for the response code
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		g_free(welcome_text);
		welcome_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		// maybe the plugin should write here the settings into a file
		// (e.g. using GLib's GKeyFile API)
		// all plugin specific files should be created in:
		// app->configdir G_DIR_SEPARATOR_S plugins G_DIR_SEPARATOR_S pluginname G_DIR_SEPARATOR_S
		// e.g. this could be: ~/.geany/plugins/Demo/, please use app->configdir
	}
	gtk_widget_destroy(dialog);
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done.
 * Be sure to leave Geany as it was before init(). */
void cleanup()
{
	// remove the menu item added in init()
	gtk_widget_destroy(plugin_fields->menu_item);
	// release other allocated strings and objects
	g_free(welcome_text);
}
