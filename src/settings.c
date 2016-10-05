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

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#define SETTINGS_SCHEMA_ID "org.geany.Settings"
#define SETTINGS_PATH "/"
#define SETTINGS_ROOT_GROUP "general"
#define SETTINGS_KEYFILE "settings.ini"

GSettings *geany_settings = NULL;

void settings_init(void)
{
	gchar *config_fn;
	GSettingsBackend *backend;
	GSettingsSchemaSource *source;
	GSettingsSchema *schema;
	GError *error = NULL;

	source = g_settings_schema_source_new_from_directory(
		GEANY_SETTINGS_SCHEMA_DIR, NULL, TRUE, &error);
	if (source == NULL)
	{
		g_error("failed to add schema source '" GEANY_SETTINGS_SCHEMA_DIR
			"': %s", error->message);
		g_error_free(error);
		return;
	}

	schema = g_settings_schema_source_lookup(source, SETTINGS_SCHEMA_ID, FALSE);
	g_settings_schema_source_unref(source);
	if (schema == NULL)
	{
		g_error("unable to locate schema in '" GEANY_SETTINGS_SCHEMA_DIR "'");
		return;
	}

	config_fn = g_build_filename(app->configdir, SETTINGS_KEYFILE, NULL);
	backend = g_keyfile_settings_backend_new(config_fn, SETTINGS_PATH, SETTINGS_ROOT_GROUP);
	geany_settings = g_settings_new_full(schema, backend, SETTINGS_PATH);

	g_object_unref(backend);
	g_free(config_fn);
	g_settings_schema_unref(schema);

	g_assert(G_IS_SETTINGS(geany_settings));

	geany_debug("Settings initialized");
}

void settings_finalize(void)
{
	g_object_unref(geany_settings);
}
