/*
 *      stash.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 The Geany contributors
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

#ifndef GEANY_STASH_H
#define GEANY_STASH_H 1

#include "gtkcompat.h"

G_BEGIN_DECLS


/** Can be @c GtkWidget* or @c gchar* depending on whether the @a owner argument is used for
 * stash_group_display() and stash_group_update(). */
typedef gconstpointer StashWidgetID;

/**
 *  Structure to hold a preference key and its properties.
 */
struct StashPref
{
	GType setting_type;			/**< Setting type. eg, G_TYPE_INT */
	const gchar *key_name;		/**< Key name. */
	gchar *comment;				/**< Comment associated with the key */
	gpointer setting;			/**< Address of a variable */
	gboolean session;			/**< Whether this StashPref holds session data */
	gpointer setting_backup;	/**< backup of previous setting when override is in effect */
	gboolean over_ride;			/**< whether a runtime override is in effect */
	gpointer default_value;		/**< Default value. eg, (gpointer)1 */
	GType widget_type;			/**< Widget type, eg, GTK_TYPE_TOGGLE_BUTTON */
	StashWidgetID widget_id;	/**< Widget ID. (GtkWidget*) or (gchar*) */
	/** extra fields depending on widget_type */
	union
	{
		struct EnumWidget *radio_buttons;	/**< Radio buttons. */
		const gchar *property_name;			/**< Property name. */
	} extra;
};

typedef struct StashPref StashPref;


/** Struct for a group of settings. */
struct StashGroup
{
	guint refcount;				/* ref count for GBoxed implementation */
	const gchar *name;			/**< group name to use in the keyfile */
	GPtrArray *entries;			/**< array of (StashPref*) */
	gboolean various;			/**< mark group for display/edit in stash treeview */
	const gchar *prefix;		/**< text to display for Various UI instead of name */
	gboolean use_defaults;		/**< use default values if there's no keyfile entry */
};

typedef struct StashGroup StashGroup;


GPtrArray *get_stash_groups();

GType stash_group_get_type(void);

StashGroup *stash_group_new(const gchar *name);

StashGroup *stash_group_get_group(const gchar *group_name, const gchar *key_name);

StashPref *stash_group_get_pref_by_name(StashGroup *group, const gchar *key_name);

void stash_group_add_comment(StashGroup *group,
		const gchar *key_name, const gchar *comment);

void stash_group_add_double(StashGroup *group, gdouble *setting,
		const gchar *key_name, gdouble default_value);

void stash_group_add_boolean(StashGroup *group, gboolean *setting,
		const gchar *key_name, gboolean default_value);

void stash_group_add_integer(StashGroup *group, gint *setting,
		const gchar *key_name, gint default_value);

void stash_group_add_string(StashGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value);

void stash_group_add_string_vector(StashGroup *group, gchar ***setting,
		const gchar *key_name, const gchar **default_value);

void stash_group_pref_set_override(StashPref *pref, gpointer new_setting);

gpointer stash_group_pref_unset_override(StashPref *pref);

void stash_group_load_from_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_load_config_from_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_load_session_from_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_save_to_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_save_config_to_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_save_session_to_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_free(StashGroup *group);

void stash_group_release(StashGroup *group);

gboolean stash_group_load_from_file(StashGroup *group, const gchar *filename);

gboolean stash_group_load_config_from_file(StashGroup *group, const gchar *filename);

gboolean stash_group_load_session_from_file(StashGroup *group, const gchar *filename);

gint stash_group_save_to_file(StashGroup *group, const gchar *filename,
		GKeyFileFlags flags);

gint stash_group_save_config_to_file(StashGroup *group, const gchar *filename,
		GKeyFileFlags flags);

gint stash_group_save_session_to_file(StashGroup *group, const gchar *filename,
		GKeyFileFlags flags);

/* *** GTK-related functions *** */

void stash_group_add_toggle_button(StashGroup *group, gboolean *setting,
		const gchar *key_name, gboolean default_value, StashWidgetID widget_id);

void stash_group_add_radio_buttons(StashGroup *group, gint *setting,
		const gchar *key_name, gint default_value,
		StashWidgetID widget_id, gint enum_id, ...) G_GNUC_NULL_TERMINATED;

void stash_group_add_spin_button_integer(StashGroup *group, gint *setting,
		const gchar *key_name, gint default_value, StashWidgetID widget_id);

void stash_group_add_combo_box(StashGroup *group, gint *setting,
		const gchar *key_name, gint default_value, StashWidgetID widget_id);

void stash_group_add_combo_box_entry(StashGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value, StashWidgetID widget_id);

void stash_group_add_entry(StashGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value, StashWidgetID widget_id);

void stash_group_add_widget_property(StashGroup *group, gpointer setting,
		const gchar *key_name, gpointer default_value, StashWidgetID widget_id,
		const gchar *property_name, GType type);

void stash_group_display(StashGroup *group, GtkWidget *owner);

void stash_group_update(StashGroup *group, GtkWidget *owner);

void stash_group_free_settings(StashGroup *group);


#ifdef GEANY_PRIVATE

void stash_group_set_various(StashGroup *group, gboolean various,
	const gchar *prefix);

void stash_group_set_use_defaults(StashGroup *group, gboolean use_defaults);

/* *** GTK-related functions *** */

void stash_tree_setup(GPtrArray *group_array, GtkTreeView *tree);

void stash_tree_display(GtkTreeView *tree);

void stash_tree_update(GtkTreeView *tree);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_STASH_H */
