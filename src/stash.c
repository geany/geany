/*
 *      stash.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

/*
 * @file stash.h
 * Lightweight library for reading/writing @c GKeyFile settings and synchronizing widgets with
 * C variables.
 *
 * Note: Stash should only depend on GLib and GTK, but currently has some minor
 * dependencies on Geany's utils.c.
 *
 * @section Terms
 * 'Setting' is used only for data stored on disk or in memory.
 * 'Pref' can also include visual widget information.
 *
 * @section Memory Usage
 * Stash will not duplicate strings if they are normally static arrays, such as
 * keyfile group names and key names, string default values, widget_id names, property names.
 *
 * @section String Settings
 * String settings and other dynamically allocated settings must be initialized to NULL as they
 * will be freed before reassigning.
 *
 * @section Widget Support
 * Widgets very commonly used in configuration dialogs will be supported with their own function.
 * Widgets less commonly used such as GtkExpander or widget settings that aren't commonly needed
 * to be persistent won't be directly supported, to keep the library lightweight. However, you can
 * use stash_group_add_widget_property() to also save these settings for any read/write widget
 * property.
 *
 * @section Example
 * @include stash-example.c
 */
/* TODO: Type naming should be changed to be independent of Geany.
 * TODO: Rename to GStash as a libstash already exists.
 */

/* Implementation Note
 * We use a GArray to hold prefs. It would be more efficient for user code to declare
 * a static array of GeanyPrefEntry structs, but we don't do this because:
 *
 * * It would be more ugly (lots of casts and NULLs).
 * * Less type checking.
 * * The API would have to break when adding/changing fields.
 *
 * Usually the prefs code isn't what user code will spend most of its time doing, so this
 * should be efficient enough. But, if desired we could add a stash_group_set_size() function
 * to reduce reallocation.
 *
 * TODO: Maybe using GSlice chunks with an extra 'next' pointer would be more (memory) efficient.
 */


#include <gtk/gtk.h>

#include "stash.h"
#include "utils.h"		/* only for foreach_*, utils_get_setting_*(). Stash should not depend on Geany. */


struct GeanyPrefEntry
{
	GType setting_type;			/* e.g. G_TYPE_INT */
	gpointer setting;
	const gchar *key_name;
	gpointer default_value;
	GType widget_type;			/* e.g. GTK_TYPE_TOGGLE_BUTTON */
	GeanyWidgetID widget_id;	/* (GtkWidget*) or (gchar*) */
	gpointer fields;			/* extra fields */
};

typedef struct GeanyPrefEntry GeanyPrefEntry;

struct GeanyPrefGroup
{
	const gchar *name;			/* group name to use in the keyfile */
	GArray *entries;			/* array of GeanyPrefEntry */
	gboolean write_once;		/* only write settings if they don't already exist */
	gboolean use_defaults;		/* use default values if there's no keyfile entry */
};

typedef struct EnumWidget
{
	GeanyWidgetID widget_id;
	gint enum_id;
}
EnumWidget;


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
			g_key_file_set_string(config, group->name, se->key_name,
				*setting ? *setting : "");
			break;
	}
}


static void handle_strv_setting(GeanyPrefGroup *group, GeanyPrefEntry *se,
		GKeyFile *config, SettingAction action)
{
	gchararray **setting = se->setting;

	switch (action)
	{
		case SETTING_READ:
			g_strfreev(*setting);
			*setting = g_key_file_get_string_list(config, group->name, se->key_name,
				NULL, NULL);
			if (*setting == NULL)
				*setting = g_strdupv(se->default_value);
			break;

		case SETTING_WRITE:
		{
			/* don't try to save a NULL string vector */
			gchar *dummy[] = { "", NULL };
			gchar **strv = *setting ? *setting : dummy;

			g_key_file_set_string_list(config, group->name, se->key_name,
				(const gchar**)strv, g_strv_length(strv));
			break;
		}
	}
}


static void keyfile_action(SettingAction action, GeanyPrefGroup *group, GKeyFile *keyfile)
{
	GeanyPrefEntry *entry;

	foreach_array(GeanyPrefEntry, entry, group->entries)
	{
		/* don't overwrite write_once prefs */
		if (group->write_once && action == SETTING_WRITE &&
			g_key_file_has_key(keyfile, group->name, entry->key_name, NULL))
			continue;
		/* don't override settings with default values */
		if (!group->use_defaults && action == SETTING_READ &&
			!g_key_file_has_key(keyfile, group->name, entry->key_name, NULL))
			continue;

		switch (entry->setting_type)
		{
			case G_TYPE_BOOLEAN:
				handle_boolean_setting(group, entry, keyfile, action); break;
			case G_TYPE_INT:
				handle_integer_setting(group, entry, keyfile, action); break;
			case G_TYPE_STRING:
				handle_string_setting(group, entry, keyfile, action); break;
			default:
				/* G_TYPE_STRV is not a constant */
				if (entry->setting_type == G_TYPE_STRV)
					handle_strv_setting(group, entry, keyfile, action);
				else
					g_warning("Unhandled type for %s::%s in %s()!", group->name, entry->key_name,
						G_STRFUNC);
		}
	}
}


/** Reads all key values (usually from a configuration file) into the group settings.
 * @param group .
 * @param keyfile . */
void stash_group_load_from_key_file(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(SETTING_READ, group, keyfile);
}


/** Writes group settings into key values for a configuration file.
 * @param group .
 * @param keyfile . */
void stash_group_save_to_key_file(GeanyPrefGroup *group, GKeyFile *keyfile)
{
	keyfile_action(SETTING_WRITE, group, keyfile);
}


/** Creates a new group.
 * @param name Name used for @c GKeyFile group.
 * @return Group. */
GeanyPrefGroup *stash_group_new(const gchar *name)
{
	GeanyPrefGroup *group = g_new0(GeanyPrefGroup, 1);

	group->name = name;
	group->entries = g_array_new(FALSE, FALSE, sizeof(GeanyPrefEntry));
	group->use_defaults = TRUE;
	return group;
}


/** Frees a group.
 * @param group . */
void stash_group_free(GeanyPrefGroup *group)
{
	GeanyPrefEntry *entry;

	foreach_array(GeanyPrefEntry, entry, group->entries)
	{
		if (entry->widget_type == GTK_TYPE_RADIO_BUTTON)
			g_free(entry->fields);
		else if (entry->widget_type == G_TYPE_PARAM)
			continue;
		else
			g_assert(entry->fields == NULL);	/* to prevent memory leaks, must handle fields above */
	}
	g_array_free(group->entries, TRUE);
	g_free(group);
}


/* Useful so the user can edit the keyfile manually while the program is running,
 * and the setting won't be overridden.
 * @c FALSE by default. */
void stash_group_set_write_once(GeanyPrefGroup *group, gboolean write_once)
{
	group->write_once = write_once;
}


/* When @c FALSE, Stash doesn't change the setting if there is no keyfile entry, so it
 * remains whatever it was initialized/set to by user code.
 * @c TRUE by default. */
void stash_group_set_use_defaults(GeanyPrefGroup *group, gboolean use_defaults)
{
	group->use_defaults = use_defaults;
}


static GeanyPrefEntry *
add_pref(GeanyPrefGroup *group, GType type, gpointer setting,
		const gchar *key_name, gpointer default_value)
{
	GeanyPrefEntry entry = {type, setting, key_name, default_value, G_TYPE_NONE, NULL, NULL};
	GArray *array = group->entries;

	g_array_append_val(array, entry);

	return &g_array_index(array, GeanyPrefEntry, array->len - 1);
}


/** Adds boolean setting.
 * @param group .
 * @param setting Address of setting variable.
 * @param key_name Name for key in a @c GKeyFile.
 * @param default_value Value to use if the key doesn't exist when loading. */
void stash_group_add_boolean(GeanyPrefGroup *group, gboolean *setting,
		const gchar *key_name, gboolean default_value)
{
	add_pref(group, G_TYPE_BOOLEAN, setting, key_name, GINT_TO_POINTER(default_value));
}


/** Adds integer setting.
 * @param group .
 * @param setting Address of setting variable.
 * @param key_name Name for key in a @c GKeyFile.
 * @param default_value Value to use if the key doesn't exist when loading. */
void stash_group_add_integer(GeanyPrefGroup *group, gint *setting,
		const gchar *key_name, gint default_value)
{
	add_pref(group, G_TYPE_INT, setting, key_name, GINT_TO_POINTER(default_value));
}


/** Adds string setting.
 * The contents of @a setting will be freed before being replaced, so make sure it is
 * allocated, or initialized to @c NULL.
 * @param group .
 * @param setting Address of setting variable.
 * @param key_name Name for key in a @c GKeyFile.
 * @param default_value Value to use if the key doesn't exist when loading. Not duplicated. */
void stash_group_add_string(GeanyPrefGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value)
{
	add_pref(group, G_TYPE_STRING, setting, key_name, (gpointer)default_value);
}


/** Adds string vector setting (array of strings).
 * The contents of @a setting will be freed before being replaced, so make sure it is
 * allocated, or initialized to @c NULL.
 * @param group .
 * @param setting Address of setting variable.
 * @param key_name Name for key in a @c GKeyFile.
 * @param default_value Value to use if the key doesn't exist when loading. Not duplicated. */
void stash_group_add_string_vector(GeanyPrefGroup *group, gchar ***setting,
		const gchar *key_name, const gchar **default_value)
{
	add_pref(group, G_TYPE_STRV, setting, key_name, (gpointer)default_value);
}


/* *** GTK-related functions *** */

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


static void handle_spin_button(GtkWidget *widget, GeanyPrefEntry *entry,
		PrefAction action)
{
	gint *setting = entry->setting;

	g_assert(entry->setting_type == G_TYPE_INT);	/* only int spin prefs */

	switch (action)
	{
		case PREF_DISPLAY:
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), *setting);
			break;
		case PREF_UPDATE:
			/* if the widget is focussed, the value might not be updated */
			gtk_spin_button_update(GTK_SPIN_BUTTON(widget));
			*setting = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
			break;
	}
}


static void handle_combo_box(GtkWidget *widget, GeanyPrefEntry *entry,
		PrefAction action)
{
	gint *setting = entry->setting;

	switch (action)
	{
		case PREF_DISPLAY:
			gtk_combo_box_set_active(GTK_COMBO_BOX(widget), *setting);
			break;
		case PREF_UPDATE:
			*setting = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
			break;
	}
}


static void handle_entry(GtkWidget *widget, GeanyPrefEntry *entry,
		PrefAction action)
{
	gchararray *setting = entry->setting;

	switch (action)
	{
		case PREF_DISPLAY:
			gtk_entry_set_text(GTK_ENTRY(widget), *setting);
			break;
		case PREF_UPDATE:
			g_free(*setting);
			*setting = g_strdup(gtk_entry_get_text(GTK_ENTRY(widget)));
			break;
	}
}


static void handle_combo_box_entry(GtkWidget *widget, GeanyPrefEntry *entry,
		PrefAction action)
{
	widget = gtk_bin_get_child(GTK_BIN(widget));
	handle_entry(widget, entry, action);
}


/* taken from Glade 2.x generated support.c */
static GtkWidget*
lookup_widget(GtkWidget *widget, const gchar *widget_name)
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


static GtkWidget *
get_widget(GtkWidget *owner, GeanyWidgetID widget_id)
{
	GtkWidget *widget = widget_id;

	if (owner)
	{
		const gchar *widget_name = widget_id;

		widget = lookup_widget(owner, widget_name);
	}
	if (!GTK_IS_WIDGET(widget))
	{
		g_warning("Unknown widget in %s()!", G_STRFUNC);
		return NULL;
	}
	return widget;
}


static void handle_radio_button(GtkWidget *widget, gint enum_id, gboolean *setting,
		PrefAction action)
{
	switch (action)
	{
		case PREF_DISPLAY:
			if (*setting == enum_id)
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
			break;
		case PREF_UPDATE:
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
				*setting = enum_id;
			break;
	}
}


static void handle_radio_buttons(GtkWidget *owner, EnumWidget *fields,
		gboolean *setting,
		PrefAction action)
{
	EnumWidget *field = fields;
	gsize count = 0;
	GtkWidget *widget = NULL;

	while (1)
	{
		widget = get_widget(owner, field->widget_id);

		if (!widget)
			continue;

		count++;
		handle_radio_button(widget, field->enum_id, setting, action);
		field++;
		if (!field->widget_id)
			break;
	}
	if (g_slist_length(gtk_radio_button_get_group(GTK_RADIO_BUTTON(widget))) != count)
		g_warning("Missing/invalid radio button widget IDs found!");
}


static void handle_widget_property(GtkWidget *widget, GeanyPrefEntry *entry,
		PrefAction action)
{
	GObject *object = G_OBJECT(widget);
	const gchar *name = entry->fields;

	switch (action)
	{
		case PREF_DISPLAY:
			g_object_set(object, name, entry->setting, NULL);
			break;
		case PREF_UPDATE:
			if (entry->setting_type == G_TYPE_STRING)
				g_free(entry->setting);
			/* TODO: Which other types need freeing here? */

			g_object_get(object, name, entry->setting, NULL);
			break;
	}
}


static void pref_action(PrefAction action, GeanyPrefGroup *group, GtkWidget *owner)
{
	GeanyPrefEntry *entry;

	foreach_array(GeanyPrefEntry, entry, group->entries)
	{
		GtkWidget *widget;

		/* ignore settings with no widgets */
		if (entry->widget_type == G_TYPE_NONE)
			continue;

		/* radio buttons have several widgets */
		if (entry->widget_type == GTK_TYPE_RADIO_BUTTON)
		{
			handle_radio_buttons(owner, entry->fields, entry->setting, action);
			continue;
		}

		widget = get_widget(owner, entry->widget_id);
		if (!widget)
		{
			g_warning("Unknown widget for %s::%s in %s()!", group->name, entry->key_name,
				G_STRFUNC);
			continue;
		}

		/* note: can't use switch for GTK_TYPE macros */
		if (entry->widget_type == GTK_TYPE_TOGGLE_BUTTON)
			handle_toggle_button(widget, entry->setting, action);
		else if (entry->widget_type == GTK_TYPE_SPIN_BUTTON)
			handle_spin_button(widget, entry, action);
		else if (entry->widget_type == GTK_TYPE_COMBO_BOX)
			handle_combo_box(widget, entry, action);
		else if (entry->widget_type == GTK_TYPE_COMBO_BOX_ENTRY)
			handle_combo_box_entry(widget, entry, action);
		else if (entry->widget_type == GTK_TYPE_ENTRY)
			handle_entry(widget, entry, action);
		else if (entry->widget_type == G_TYPE_PARAM)
			handle_widget_property(widget, entry, action);
		else
			g_warning("Unhandled type for %s::%s in %s()!", group->name, entry->key_name,
				G_STRFUNC);
	}
}


/* @param owner If non-NULL, used to lookup widgets by name. */
void stash_group_display(GeanyPrefGroup *group, GtkWidget *owner)
{
	pref_action(PREF_DISPLAY, group, owner);
}


void stash_group_update(GeanyPrefGroup *group, GtkWidget *owner)
{
	pref_action(PREF_UPDATE, group, owner);
}


static GeanyPrefEntry *
add_widget_pref(GeanyPrefGroup *group, GType setting_type, gpointer setting,
		const gchar *key_name, gpointer default_value,
		GType widget_type, GeanyWidgetID widget_id)
{
	GeanyPrefEntry *entry =
		add_pref(group, setting_type, setting, key_name, default_value);

	entry->widget_type = widget_type;
	entry->widget_id = widget_id;
	return entry;
}


/* Used for GtkCheckButton or GtkToggleButton widgets.
 * @see stash_group_add_radio_buttons(). */
void stash_group_add_toggle_button(GeanyPrefGroup *group, gboolean *setting,
		const gchar *key_name, gboolean default_value, GeanyWidgetID widget_id)
{
	add_widget_pref(group, G_TYPE_BOOLEAN, setting, key_name, GINT_TO_POINTER(default_value),
		GTK_TYPE_TOGGLE_BUTTON, widget_id);
}


/* @param ... pairs of widget_id, enum_id.
 * Example (using widget lookup strings, but widget pointers can also work):
 * @code
 * enum {FOO, BAR};
 * stash_group_add_radio_buttons(group, &which_one_setting, "which_one", BAR,
 * 	"radio_foo", FOO, "radio_bar", BAR, NULL);
 * @endcode */
void stash_group_add_radio_buttons(GeanyPrefGroup *group, gint *setting,
		const gchar *key_name, gint default_value,
		GeanyWidgetID widget_id, gint enum_id, ...)
{
	GeanyPrefEntry *entry =
		add_widget_pref(group, G_TYPE_INT, setting, key_name, GINT_TO_POINTER(default_value),
			GTK_TYPE_RADIO_BUTTON, NULL);
	va_list args;
	gsize count = 1;
	EnumWidget *item, *array;

	/* count pairs of args */
	va_start(args, enum_id);
	while (1)
	{
		gint dummy;

		if (!va_arg(args, gpointer))
			break;
		dummy = va_arg(args, gint);
		count++;
	}
	va_end(args);

	array = g_new0(EnumWidget, count + 1);
	entry->fields = array;

	va_start(args, enum_id);
	foreach_c_array(item, array, count)
	{
		if (item == array)
		{
			/* first element */
			item->widget_id = widget_id;
			item->enum_id = enum_id;
		}
		else
		{
			item->widget_id = va_arg(args, gpointer);
			item->enum_id = va_arg(args, gint);
		}
	}
	va_end(args);
}


void stash_group_add_spin_button_integer(GeanyPrefGroup *group, gint *setting,
		const gchar *key_name, gint default_value, GeanyWidgetID widget_id)
{
	add_widget_pref(group, G_TYPE_INT, setting, key_name, GINT_TO_POINTER(default_value),
		GTK_TYPE_SPIN_BUTTON, widget_id);
}


/* @see stash_group_add_combo_box_entry(). */
void stash_group_add_combo_box(GeanyPrefGroup *group, gint *setting,
		const gchar *key_name, gint default_value, GeanyWidgetID widget_id)
{
	add_widget_pref(group, G_TYPE_INT, setting, key_name, GINT_TO_POINTER(default_value),
		GTK_TYPE_COMBO_BOX, widget_id);
}


/* We could maybe also have something like stash_group_add_combo_box_entry_with_menu()
 * for the history list - or should that be stored as a separate setting? */
void stash_group_add_combo_box_entry(GeanyPrefGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value, GeanyWidgetID widget_id)
{
	add_widget_pref(group, G_TYPE_STRING, setting, key_name, (gpointer)default_value,
		GTK_TYPE_COMBO_BOX_ENTRY, widget_id);
}


void stash_group_add_entry(GeanyPrefGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value, GeanyWidgetID widget_id)
{
	add_widget_pref(group, G_TYPE_STRING, setting, key_name, (gpointer)default_value,
		GTK_TYPE_ENTRY, widget_id);
}


static GType object_get_property_type(GObject *object, const gchar *property_name)
{
	GObjectClass *klass = G_OBJECT_GET_CLASS(object);
	GParamSpec *ps;

	ps = g_object_class_find_property(klass, property_name);
	return ps->value_type;
}


/* Add a widget's read/write property to the stash group. The property will be set when calling
 * stash_group_display(), and read when calling stash_group_update().
 * @param setting address of e.g. an integer if using an integer property.
 * @param default_value should be cast into a pointer e.g. with @c GINT_TO_POINTER().
 * @param type can be 0 if passing a GtkWidget as the @a widget_id argument to look it up from the
 * GObject data.
 * @warning Currently only string GValue properties will be freed before setting; patch for
 * other types - see handle_widget_property(). */
void stash_group_add_widget_property(GeanyPrefGroup *group, gpointer setting,
		const gchar *key_name, gpointer default_value, GeanyWidgetID widget_id,
		const gchar *property_name, GType type)
{
	if (!type)
		type = object_get_property_type(G_OBJECT(widget_id), property_name);

	add_widget_pref(group, type, setting, key_name, default_value,
		G_TYPE_PARAM, widget_id)->fields = (gchar*)property_name;
}


