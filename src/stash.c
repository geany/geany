/*
 *      stash.c - this file is part of Geany, a fast and lightweight IDE
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

/* Mini-library for reading/writing GKeyFile settings and synchronizing widgets with
 * C variables. */


#include <gtk/gtk.h>

#include "stash.h"
#include "utils.h"

#define foreach_array(type, item, array) \
	foreach_c_array(item, ((type*)(gpointer)array->data), array->len)


struct GeanyPrefEntry
{
	GType type;					/* e.g. G_TYPE_INT */
	gpointer setting;
	const gchar *key_name;
	gpointer default_value;
	const gchar *widget_name;
};

struct GeanyPrefGroup
{
	const gchar *name;			/* group name to use in the keyfile */
	GArray *entries;			/* array of GeanyPrefEntry */
	gboolean write_once;		/* only write settings if they don't already exist */
};


typedef enum SettingAction
{
	SETTING_READ,
	SETTING_WRITE
}
SettingAction;



static void handle_boolean_setting(GeanyPrefGroup *group, GeanyPrefEntry *se,
		GKeyFile *config, SettingAction action)
{
	gboolean *setting = se->setting;

	switch (action)
	{
		case SETTING_READ:
			*setting = utils_get_setting_boolean(config, group->name, se->key_name,
				GPOINTER_TO_INT(se->default_value));
			break;
		case SETTING_WRITE:
			g_key_file_set_boolean(config, group->name, se->key_name, *setting);
			break;
	}
}


static void handle_integer_setting(GeanyPrefGroup *group, GeanyPrefEntry *se,
		GKeyFile *config, SettingAction action)
{
	gboolean *setting = se->setting;

	switch (action)
	{
		case SETTING_READ:
			*setting = utils_get_setting_integer(config, group->name, se->key_name,
				GPOINTER_TO_INT(se->default_value));
			break;
		case SETTING_WRITE:
			g_key_file_set_integer(config, group->name, se->key_name, *setting);
			break;
	}
}


static void handle_string_setting(GeanyPrefGroup *group, GeanyPrefEntry *se,
		GKeyFile *config, SettingAction action)
{
	gchararray *setting = se->setting;

	switch (action)
	{
		case SETTING_READ:
			g_free(*setting);
			*setting = utils_get_setting_string(config, group->name, se->key_name,
				se->default_value);
			break;
		case SETTING_WRITE:
			g_key_file_set_string(config, group->name, se->key_name, *setting);
			break;
	}
}


static void keyfile_action(GeanyPrefGroup *group, GKeyFile *keyfile, SettingAction action)
{
	GeanyPrefEntry *entry;

	foreach_array(GeanyPrefEntry, entry, group->entries)
	{
		if (group->write_once && action == SETTING_WRITE &&
			g_key_file_has_key(keyfile, group->name, entry->key_name, NULL))
			continue; /* don't overwrite write_once prefs */

		switch (entry->type)
		{
			case G_TYPE_BOOLEAN:
				handle_boolean_setting(group, entry, keyfile, action); break;
			case G_TYPE_INT:
				handle_integer_setting(group, entry, keyfile, action); break;
			case G_TYPE_STRING:
				handle_string_setting(group, entry, keyfile, action); break;
			default:
				g_warning("Unhandled type for %s::%s!", group->name, entry->key_name);
		}
	}
}


void stash_group_load(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(group, keyfile, SETTING_READ);
}


void stash_group_save(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(group, keyfile, SETTING_WRITE);
}


GeanyPrefGroup *stash_group_new(const gchar *name)
{
	GeanyPrefGroup *group = g_new0(GeanyPrefGroup, 1);

	group->name = name;
	group->entries = g_array_new(FALSE, FALSE, sizeof(GeanyPrefEntry));
	return group;
}


void stash_group_free(GeanyPrefGroup *group)
{
	g_array_free(group->entries, TRUE);
	g_free(group);
}


void stash_group_set_write_once(GeanyPrefGroup *group, gboolean write_once)
{
	group->write_once = write_once;
}


static void add_pref(GeanyPrefGroup *group, GType type, gpointer setting,
		const gchar *key_name, gpointer default_value)
{
	GeanyPrefEntry entry = {type, setting, key_name, default_value, NULL};

	g_array_append_val(group->entries, entry);
}


void stash_group_add_boolean(GeanyPrefGroup *group, gboolean *setting,
		const gchar *key_name, gboolean default_value)
{
	add_pref(group, G_TYPE_BOOLEAN, setting, key_name, GINT_TO_POINTER(default_value));
}


void stash_group_add_integer(GeanyPrefGroup *group, gint *setting,
		const gchar *key_name, gint default_value)
{
	add_pref(group, G_TYPE_INT, setting, key_name, GINT_TO_POINTER(default_value));
}


void stash_group_add_string(GeanyPrefGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value)
{
	add_pref(group, G_TYPE_STRING, setting, key_name, g_strdup(default_value));
}


