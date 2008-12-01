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


typedef enum SettingAction
{
	SETTING_READ,
	SETTING_WRITE
}
SettingAction;



static void handle_bool_setting(GeanyPrefGroup *group, GeanyPrefEntry *se,
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


static void handle_int_setting(GeanyPrefGroup *group, GeanyPrefEntry *se,
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

	foreach_c_array(entry, group->entries, group->entries_len)
	{
		if (group->write_once && action == SETTING_WRITE &&
			g_key_file_has_key(keyfile, group->name, entry->key_name, NULL))
			continue; /* don't overwrite write_once prefs */

		switch (entry->type)
		{
			case G_TYPE_BOOLEAN:
				handle_bool_setting(group, entry, keyfile, action); break;
			case G_TYPE_INT:
				handle_int_setting(group, entry, keyfile, action); break;
			case G_TYPE_STRING:
				handle_string_setting(group, entry, keyfile, action); break;
			default:
				g_warning("Unhandled type for %s::%s!", group->name, entry->key_name);
		}
	}
}


void stash_load_group(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(group, keyfile, SETTING_READ);
}


void stash_save_group(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(group, keyfile, SETTING_WRITE);
}


