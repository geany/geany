/*
 * settings.h - this file is part of Geany, a fast and lightweight IDE
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

#ifndef GEANY_SETTINGS_H
#define GEANY_SETTINGS_H 1

#include <gio/gio.h>

G_BEGIN_DECLS

#ifdef GEANY_PRIVATE

extern GSettings *geany_settings;

void settings_init(void);
void settings_finalize(void);
GSettings *settings_create_for_prefs(void);

#endif

gboolean settings_get_bool(const gchar *key);
gint settings_get_int(const gchar *key);
gchar *settings_get_string(const gchar *key);
gint settings_get_enum(const gchar *key);

void settings_set_bool(const gchar *key, gboolean value);
void settings_set_int(const gchar *key, gint value);
void settings_set_string(const gchar *key, const gchar *value);
void settings_set_enum(const gchar *key, gint value);

G_END_DECLS

#endif // GEANY_SETTINGS_H
