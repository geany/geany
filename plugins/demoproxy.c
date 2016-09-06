/*
 *      demoproxy.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2015 Thomas Martitz <kugel(at)rockbox(dot)org>
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

/**
 * Demo proxy - example of a basic proxy plugin for Geany. Sub-plugins add menu items to the
 * Tools menu and have a help dialog.
 *
 * Note: This is compiled but not installed by default. On Unix, you can install it by compiling
 * Geany and then copying (or symlinking) to the plugins/demoproxy.so and
 * plugins/demoproxytest.px files to ~/.config/geany/plugins
 * - it will be loaded at next startup.
 */

/* plugin API, always comes first */
#include "geanyplugin.h"

typedef struct {
	GKeyFile       *file;
	gchar          *help_text;
	GSList         *menu_items;
}
PluginContext;


static gboolean proxy_init(GeanyPlugin *plugin, gpointer pdata)
{
	PluginContext *data;
	gint i = 0;
	gchar *text;

	data = (PluginContext *) pdata;

	/* Normally, you would instruct the VM/interpreter to call into the actual plugin. The
	 * plugin would be identified by pdata. Because there is no interpreter for
	 * .ini files we do it inline, as this is just a demo */
	data->help_text = g_key_file_get_locale_string(data->file, "Help", "text", NULL, NULL);
	while (TRUE)
	{
		GtkWidget *item;
		gchar *key = g_strdup_printf("item%d", i++);
		text = g_key_file_get_locale_string(data->file, "Init", key, NULL, NULL);
		g_free(key);

		if (!text)
			break;

		item = gtk_menu_item_new_with_label(text);
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(plugin->geany_data->main_widgets->tools_menu), item);
		gtk_widget_set_sensitive(item, FALSE);
		data->menu_items = g_slist_prepend(data->menu_items, (gpointer) item);
		g_free(text);
	}

	return TRUE;
}


static void proxy_help(GeanyPlugin *plugin, gpointer pdata)
{
	PluginContext *data;
	GtkWidget *dialog;

	data = (PluginContext *) pdata;

	dialog = gtk_message_dialog_new(
		GTK_WINDOW(plugin->geany_data->main_widgets->window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_INFO,
		GTK_BUTTONS_OK,
		"%s", data->help_text);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		_("(From the %s plugin)"), plugin->info->name);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


static void proxy_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	PluginContext *data = (PluginContext *) pdata;

	g_slist_free_full(data->menu_items, (GDestroyNotify) gtk_widget_destroy);
	g_free(data->help_text);
}


static gint demoproxy_probe(GeanyPlugin *proxy, const gchar *filename, gpointer pdata)
{
	/* We know the extension is right (Geany checks that). For demo purposes we perform an
	 * additional check. This is not necessary when the extension is unique enough. */
	gboolean match = FALSE;
	gchar linebuf[128];
	FILE *f = fopen(filename, "r");
	if (f != NULL)
	{
		if (fgets(linebuf, sizeof(linebuf), f) != NULL)
			match = utils_str_equal(linebuf, "#!!PROXY_MAGIC!!\n");
		fclose(f);
	}
	return match ? GEANY_PROXY_MATCH : GEANY_PROXY_IGNORE;
}


static gpointer demoproxy_load(GeanyPlugin *proxy, GeanyPlugin *plugin,
                               const gchar *filename, gpointer pdata)
{
	GKeyFile *file;
	gboolean result;

	file = g_key_file_new();
	result = g_key_file_load_from_file(file, filename, 0, NULL);

	if (result)
	{
		PluginContext *data = g_new0(PluginContext, 1);
		data->file = file;

		plugin->info->name = g_key_file_get_locale_string(data->file, "Info", "name", NULL, NULL);
		plugin->info->description = g_key_file_get_locale_string(data->file, "Info", "description", NULL, NULL);
		plugin->info->version = g_key_file_get_locale_string(data->file, "Info", "version", NULL, NULL);
		plugin->info->author = g_key_file_get_locale_string(data->file, "Info", "author", NULL, NULL);

		plugin->funcs->init = proxy_init;
		plugin->funcs->help = proxy_help;
		plugin->funcs->cleanup = proxy_cleanup;

		/* Cannot pass g_free as free_func be Geany calls it before unloading, and since 
		 * demoproxy_unload() accesses the data this would be catastrophic */
		GEANY_PLUGIN_REGISTER_FULL(plugin, 225, data, NULL);
		return data;
	}

	g_key_file_free(file);
	return NULL;
}


static void demoproxy_unload(GeanyPlugin *proxy, GeanyPlugin *plugin, gpointer load_data, gpointer pdata)
{
	PluginContext *data = load_data;

	g_free((gchar *)plugin->info->name);
	g_free((gchar *)plugin->info->description);
	g_free((gchar *)plugin->info->version);
	g_free((gchar *)plugin->info->author);

	g_key_file_free(data->file);
	g_free(data);
}


/* Called by Geany to initialize the plugin. */
static gboolean demoproxy_init(GeanyPlugin *plugin, gpointer pdata)
{
	const gchar *extensions[] = { "ini", "px", NULL };

	plugin->proxy_funcs->probe  = demoproxy_probe;
	plugin->proxy_funcs->load   = demoproxy_load;
	plugin->proxy_funcs->unload = demoproxy_unload;

	return geany_plugin_register_proxy(plugin, extensions);
}


/* Called by Geany before unloading the plugin. */
static void demoproxy_cleanup(GeanyPlugin *plugin, gpointer data)
{
}


G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	plugin->info->name = _("Demo Proxy");
	plugin->info->description = _("Example Proxy.");
	plugin->info->version = "0.1";
	plugin->info->author = _("The Geany developer team");

	plugin->funcs->init = demoproxy_init;
	plugin->funcs->cleanup = demoproxy_cleanup;

	GEANY_PLUGIN_REGISTER(plugin, 225);
}
