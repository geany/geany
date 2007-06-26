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


static PluginData *my_data;

static struct
{
	GtkWidget *menu_item;
}
local_data;


/* This performs runtime checks that try to ensure:
 * 1. Geany ABI data types are compatible with this plugin.
 * 2. Geany sources provide the required API for this plugin. */
VERSION_CHECK(1)


static void
item_activate(GtkMenuItem *menuitem, gpointer gdata)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(my_data->app->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		_("Hello World!"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), my_data->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


void init(PluginData *data)
{
	my_data = data;
	my_data->name = g_strdup("Demo");

	local_data.menu_item = gtk_menu_item_new_with_mnemonic(_("_Demo Plugin"));
	gtk_widget_show(local_data.menu_item);
	gtk_container_add(GTK_CONTAINER(my_data->tools_menu), local_data.menu_item);
	g_signal_connect(G_OBJECT(local_data.menu_item), "activate", G_CALLBACK(item_activate), NULL);
}


void cleanup()
{
	gtk_widget_destroy(local_data.menu_item);

	g_free(my_data->name);
}
