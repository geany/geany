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


#include "geany.h"		/* for the GeanyApp data type */
#include "support.h"	/* for the _() translation macro (see also po/POTFILES.in) */
#include "ui_utils.h"

#include "plugindata.h"		/* this defines the plugin API */
#include "pluginmacros.h"	/* some useful macros to save typing */


/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


/* Check that the running Geany supports the plugin API used below, and check
 * for binary compatibility. */
PLUGIN_VERSION_CHECK(100)

/* All plugins must set name, description, version and author. */
PLUGIN_SET_INFO(_("Demo"), _("Example plugin."), VERSION, _("The Geany developer team"))


static GtkWidget *main_menu_item = NULL;
/* text to be shown in the plugin dialog */
static gchar *welcome_text = NULL;


/* Callback when the menu item is clicked. */
static void
item_activate(GtkMenuItem *menuitem, gpointer gdata)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(geany->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		"%s", welcome_text);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), geany_plugin->info->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


/* Called by Geany to initialize the plugin.
 * Note: data is the same as geany_data. */
void plugin_init(GeanyData *data)
{
	GtkWidget *demo_item;

	/* Add an item to the Tools menu */
	demo_item = gtk_menu_item_new_with_mnemonic(_("_Demo Plugin"));
	gtk_widget_show(demo_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), demo_item);
	g_signal_connect(demo_item, "activate", G_CALLBACK(item_activate), NULL);

	/* make the menu item sensitive only when documents are open */
	p_ui->add_document_sensitive(demo_item);
	/* keep a pointer to the menu item, so we can remove it when the plugin is unloaded */
	main_menu_item = demo_item;

	welcome_text = g_strdup(_("Hello World!"));
}


/* Callback connected in plugin_configure(). */
static void
on_configure_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	/* catch OK or Apply clicked */
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		/* We only have one pref here, but for more you would use a struct for user_data */
		GtkWidget *entry = GTK_WIDGET(user_data);

		g_free(welcome_text);
		welcome_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		/* maybe the plugin should write here the settings into a file
		 * (e.g. using GLib's GKeyFile API)
		 * all plugin specific files should be created in:
		 * geany->app->configdir G_DIR_SEPARATOR_S plugins G_DIR_SEPARATOR_S pluginname G_DIR_SEPARATOR_S
		 * e.g. this could be: ~/.geany/plugins/Demo/, please use geany->app->configdir */
	}
}


/* Called by Geany to show the plugin's configure dialog. This function is always called after
 * plugin_init() was called.
 * You can omit this function if the plugin doesn't need to be configured.
 * Note: parent is the parent window which can be used as the transient window for the created
 *       dialog. */
GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *label, *entry, *vbox;

	/* example configuration dialog */
	vbox = gtk_vbox_new(FALSE, 6);

	/* add a label and a text entry to the dialog */
	label = gtk_label_new(_("Welcome text to show:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	entry = gtk_entry_new();
	if (welcome_text != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), welcome_text);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), entry);

	gtk_widget_show_all(vbox);

	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), entry);
	return vbox;
}


/* Called by Geany before unloading the plugin.
 * Here any UI changes should be removed, memory freed and any other finalization done.
 * Be sure to leave Geany as it was before plugin_init(). */
void plugin_cleanup(void)
{
	/* remove the menu item added in plugin_init() */
	gtk_widget_destroy(main_menu_item);
	/* release other allocated strings and objects */
	g_free(welcome_text);
}
