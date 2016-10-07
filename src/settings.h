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

extern GSettings *geany_settings;

void settings_init(void);
void settings_finalize(void);
GSettings *settings_create_for_prefs(void);

G_END_DECLS

#endif // GEANY_SETTINGS_H
