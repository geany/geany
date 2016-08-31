/*
 *      stash.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

/** Opaque type for a group of settings. */
typedef struct StashGroup StashGroup;

/** Can be @c GtkWidget* or @c gchar* depending on whether the @a owner argument is used for
 * stash_group_display() and stash_group_update(). */
typedef gconstpointer StashWidgetID;

GType stash_group_get_type(void);

StashGroup *stash_group_new(const gchar *name);

void stash_group_add_boolean(StashGroup *group, gboolean *setting,
		const gchar *key_name, gboolean default_value);

void stash_group_add_integer(StashGroup *group, gint *setting,
		const gchar *key_name, gint default_value);

void stash_group_add_string(StashGroup *group, gchar **setting,
		const gchar *key_name, const gchar *default_value);

void stash_group_add_string_vector(StashGroup *group, gchar ***setting,
		const gchar *key_name, const gchar **default_value);

void stash_group_load_from_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_save_to_key_file(StashGroup *group, GKeyFile *keyfile);

void stash_group_free(StashGroup *group);

gboolean stash_group_load_from_file(StashGroup *group, const gchar *filename);

gint stash_group_save_to_file(StashGroup *group, const gchar *filename,
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

void stash_group_set_various(StashGroup *group, gboolean write_once);

void stash_group_set_use_defaults(StashGroup *group, gboolean use_defaults);

/* *** GTK-related functions *** */

void stash_tree_setup(GPtrArray *group_array, GtkTreeView *tree);

void stash_tree_display(GtkTreeView *tree);

void stash_tree_update(GtkTreeView *tree);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_STASH_H */
