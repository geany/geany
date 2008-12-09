/*
 *      stash.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#ifndef GEANY_STASH_H
#define GEANY_STASH_H

typedef struct GeanyPrefEntry GeanyPrefEntry;

typedef struct GeanyPrefGroup GeanyPrefGroup;


GeanyPrefGroup *stash_group_new(const gchar *name);

void stash_group_set_write_once(GeanyPrefGroup *group, gboolean write_once);

void stash_group_add_boolean(GeanyPrefGroup *group, gboolean *setting,
                const gchar *key_name, gboolean default_value);

void stash_group_add_integer(GeanyPrefGroup *group, gint *setting,
                const gchar *key_name, gint default_value);

void stash_group_add_string(GeanyPrefGroup *group, gchar **setting,
                const gchar *key_name, const gchar *default_value);

void stash_group_load(GeanyPrefGroup *group, GKeyFile *keyfile);

void stash_group_save(GeanyPrefGroup *group, GKeyFile *keyfile);

void stash_group_free(GeanyPrefGroup *group);

#endif
