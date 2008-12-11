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

/* Memory Usage
 * Stash will not duplicate strings if they are normally static arrays, such as
 * keyfile group names and key names. */


#include <gtk/gtk.h>

#include "stash.h"
#include "utils.h"		/* utils_get_setting_*() */

#define foreach_array(type, item, array) \
	foreach_c_array(item, ((type*)(gpointer)array->data), array->len)


struct GeanyPrefEntry
{
	GType type;					/* e.g. G_TYPE_INT */
	gpointer setting;
	const gchar *key_name;
	gpointer default_value;
	GType widget_type;			/* e.g. GTK_TYPE_TOGGLE_BUTTON */
	gpointer widget_id;			/* can be GtkWidget or gchararray */
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

typedef enum PrefAction
{
	PREF_DISPLAY,
	PREF_UPDATE
}
PrefAction;


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


static void keyfile_action(SettingAction action, GeanyPrefGroup *group, GKeyFile *keyfile)
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
				g_warning("Unhandled type for %s::%s in %s!", group->name, entry->key_name,
					G_GNUC_FUNCTION);
		}
	}
}


void stash_group_load(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(SETTING_READ, group, keyfile);
}


void stash_group_save(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(SETTING_WRITE, group, keyfile);
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


static GeanyPrefEntry *
add_pref(GeanyPrefGroup *group, GType type, gpointer setting,
		const gchar *key_name, gpointer default_value)
{
	GeanyPrefEntry entry = {type, setting, key_name, default_value, G_TYPE_NONE, NULL};
	GArray *array = group->entries;

	g_array_append_val(array, entry);

	return &g_array_index(array, GeanyPrefEntry, array->len - 1);
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


/* @param default_value Not duplicated. */
void stash_group_add_string(GeanyPrefGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value)
{
	add_pref(group, G_TYPE_STRING, setting, key_name, (gpointer)default_value);
}


static void handle_toggle_button(GtkWidget *widget, gboolean *setting,
		PrefAction action)
{
	switch (action)
	{
		case PREF_DISPLAY:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), *setting);
			break;
		case PREF_UPDATE:
			*setting = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
			break;
	}
}


/* taken from Glade 2.x generated support.c */
static GtkWidget*
lookup_widget                          (GtkWidget       *widget,
                                        const gchar     *widget_name)
{
	GtkWidget *parent, *found_widget;

	for (;;)
		{
			if (GTK_IS_MENU (widget))
				parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
			else
				parent = widget->parent;
			if (!parent)
				parent = (GtkWidget*) g_object_get_data (G_OBJECT (widget), "GladeParentKey");
			if (parent == NULL)
				break;
			widget = parent;
		}

	found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget), widget_name);
	if (!found_widget)
		g_warning ("Widget not found: %s", widget_name);
	return found_widget;
}


static void pref_action(PrefAction action, GeanyPrefGroup *group, GtkWidget *owner)
{
	GeanyPrefEntry *entry;

	foreach_array(GeanyPrefEntry, entry, group->entries)
	{
		GtkWidget *widget = entry->widget_id;
		const gchar *widget_name = entry->widget_id;

		if (entry->widget_type == G_TYPE_NONE)
			continue;

		if (owner)
		{
			widget = lookup_widget(owner, widget_name);
		}
		if (!GTK_IS_WIDGET(widget))
		{
			g_warning("Unknown widget for %s::%s in %s!", group->name, entry->key_name,
				G_GNUC_FUNCTION);
			continue;
		}

		if (entry->widget_type == GTK_TYPE_TOGGLE_BUTTON)
			handle_toggle_button(widget, entry->setting, action);
		else
			g_warning("Unhandled type for %s::%s in %s!", group->name, entry->key_name,
				G_GNUC_FUNCTION);
	}
}


/** @param owner If non-NULL, used to lookup widgets by name. */
void stash_group_display(GeanyPrefGroup *group, GtkWidget *owner)
{
	pref_action(PREF_DISPLAY, group, owner);
}


void stash_group_update(GeanyPrefGroup *group, GtkWidget *owner)
{
	pref_action(PREF_UPDATE, group, owner);
}


void stash_group_add_toggle_button(GeanyPrefGroup *group, gboolean *setting,
		const gchar *key_name, gboolean default_value, gpointer widget_id)
{
	GeanyPrefEntry *entry =
		add_pref(group, G_TYPE_BOOLEAN, setting, key_name, GINT_TO_POINTER(default_value));

	entry->widget_type = GTK_TYPE_TOGGLE_BUTTON;
	entry->widget_id = widget_id;
}


