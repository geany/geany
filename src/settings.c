/*
 * settings.c - this file is part of Geany, a fast and lightweight IDE
 *
 * Copyright 2016 Matthew Brush <matt@geany.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "settings.h"
#include "app.h"
#include "geany.h"
#include "sidebar.h"
#include "ui_utils.h"

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#define SETTINGS_SCHEMA_ID "org.geany.Settings"
#define SETTINGS_PATH "/"
#define SETTINGS_ROOT_GROUP "general"
#define SETTINGS_KEYFILE "settings.ini"

GSettings *geany_settings = NULL;


static void on_sidebar_pos_left_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	sidebar_set_position_left(g_settings_get_boolean(settings, key));
}


#define ui_lookup_main_widget(name) ui_lookup_widget(main_widgets.window, name)
#define ui_lookup_pref_widget(name) ui_lookup_widget(ui_widgets.prefs_dialog, name)


static void settings_bind(void)
{
	g_settings_bind(geany_settings, "sidebar-page", ui_lookup_main_widget("notebook3"), "page", G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind(geany_settings, "sidebar-visible", ui_lookup_main_widget("notebook3"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(geany_settings, "sidebar-visible", ui_lookup_main_widget("menu_show_sidebar1"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(geany_settings, "sidebar-documents-visible", ui_lookup_main_widget("scrolledwindow7"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(geany_settings, "sidebar-symbols-visible", ui_lookup_main_widget("scrolledwindow2"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(geany_settings, "msgwin-visible", ui_lookup_main_widget("notebook_info"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(geany_settings, "msgwin-visible", ui_lookup_main_widget("menu_show_messages_window1"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(geany_settings, "statusbar-visible", ui_lookup_main_widget("statusbar"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(geany_settings, "fullscreen", ui_lookup_main_widget("menu_fullscreen1"), "active", G_SETTINGS_BIND_DEFAULT);
}


static void settings_bind_prefs(GSettings *settings)
{
	g_settings_bind(settings, "sidebar-visible", ui_lookup_pref_widget("check_sidebar_visible"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-documents-visible", ui_lookup_pref_widget("check_list_symbol"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-symbols-visible", ui_lookup_pref_widget("check_list_openfiles"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-pos-left", ui_lookup_pref_widget("radio_sidebar_left"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "statusbar-visible", ui_lookup_pref_widget("check_statusbar_visible"), "active", G_SETTINGS_BIND_DEFAULT);

	g_signal_connect(geany_settings, "changed::sidebar-pos-left", G_CALLBACK(on_sidebar_pos_left_changed), NULL);
}


static GSettings *settings_create(void)
{
	gchar *config_fn;
	GSettingsBackend *backend;
	GSettingsSchemaSource *source;
	GSettingsSchema *schema;
	GSettings *settings;
	GError *error = NULL;

	source = g_settings_schema_source_new_from_directory(
		GEANY_SETTINGS_SCHEMA_DIR, NULL, TRUE, &error);
	if (source == NULL)
	{
		g_error("failed to add schema source '" GEANY_SETTINGS_SCHEMA_DIR
			"': %s", error->message);
		g_error_free(error);
		return NULL;
	}

	schema = g_settings_schema_source_lookup(source, SETTINGS_SCHEMA_ID, FALSE);
	g_settings_schema_source_unref(source);
	if (schema == NULL)
	{
		g_error("unable to locate schema in '" GEANY_SETTINGS_SCHEMA_DIR "'");
		return NULL;
	}

	config_fn = g_build_filename(app->configdir, SETTINGS_KEYFILE, NULL);
	backend = g_keyfile_settings_backend_new(config_fn, SETTINGS_PATH, SETTINGS_ROOT_GROUP);
	settings = g_settings_new_full(schema, backend, SETTINGS_PATH);

	g_object_unref(backend);
	g_free(config_fn);
	g_settings_schema_unref(schema);

	g_assert(G_IS_SETTINGS(settings));
	return settings;
}


GSettings *settings_create_for_prefs(void)
{
	GSettings *settings = settings_create();
	settings_bind_prefs(settings);
	return settings;
}


void settings_init(void)
{
	geany_settings = settings_create();
	settings_bind();
}


void settings_finalize(void)
{
	g_object_unref(geany_settings);
}
