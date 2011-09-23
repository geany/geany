/*
 *      interface.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"


static gchar*		interface_file = NULL;
static GtkBuilder*	builder = NULL;
static GHashTable*	objects_table = NULL;


/* Used to find out the name of the GtkBuilder object since some objects
 * will be buildable and use it's name for that and those that aren't
 * will have "gtk-builder-name" data field set on the object. */
static const gchar *interface_guess_object_name(GObject *obj)
{
	const gchar *name;

	g_return_val_if_fail(G_IS_OBJECT(obj), NULL);

	name = g_object_get_data(obj, "gtk-builder-name");
	if (! name && GTK_IS_BUILDABLE(obj))
		name = gtk_buildable_get_name(GTK_BUILDABLE(obj));

	if (! name)
		return NULL;

	return name;
}


/* Returns a widget from a name.
 *
 * Call it with the name of the GObject you want returned.  This is
 * similar to @a ui_lookup_widget except that it supports arbitrary
 * GObjects.
 *
 * @note The GObject must either be in the GtkBuilder/Glade file or
 * have been added with the function @a interface_add_object.
 *
 * @param widget Widget with the @a widget_name property set.
 * @param widget_name Name to lookup.
 * @return The widget found.
 *
 * @see ui_hookup_widget().
 * @see interface_add_object().
 * @since 0.21
 */
GObject *interface_get_object(const gchar *name)
{
	gpointer *found;

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(objects_table != NULL, NULL);

	found = g_hash_table_lookup(objects_table, (gconstpointer) name);

	if (found == NULL)
		return NULL;

	return G_OBJECT(found);
}


/* Sets a name to lookup GObject @a obj.
 *
 * This is similar to @a ui_hookup_widget() except that it supports
 * arbitrary GObjects.
 *
 * @param obj GObject.
 * @param name Name.
 *
 * @see ui_hookup_widget().
 * @since 0.21
 **/
void interface_add_object(GObject *obj, const gchar *name)
{
	g_return_if_fail(G_IS_OBJECT(obj));
	g_return_if_fail(objects_table != NULL);

	g_hash_table_insert(objects_table, g_strdup(name), g_object_ref(obj));
}


void interface_init(void)
{
	const gchar *name;
	GError *error;
	GSList *iter, *all_objects;
	GtkCellRenderer *renderer;

	g_return_if_fail(builder == NULL);

	interface_file = g_build_filename(GEANY_DATADIR, "geany", "geany.glade", NULL);

	if (! (builder = gtk_builder_new()))
	{
		g_error("Failed to initialize the user-interface");
		return;
	}

	error = NULL;
	if (! gtk_builder_add_from_file(builder, interface_file, &error))
	{
		g_error("Failed to load the user-interface file: %s", error->message);
		g_error_free(error);
		return;
	}

	gtk_builder_connect_signals(builder, NULL);

	/* TODO: set translation domain on GtkBuilder? */;

	objects_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

	all_objects = gtk_builder_get_objects(builder);
	for (iter = all_objects; iter != NULL; iter = g_slist_next(iter))
	{
		name = interface_guess_object_name(iter->data);
		if (! name)
		{
			g_warning("Unable to get name from GtkBuilder object");
			continue;
		}

		interface_add_object(iter->data, name);

		/* FIXME: Hack to get Glade 3/GtkBuilder combo boxes working */
		if (GTK_IS_COMBO_BOX(iter->data))
		{
			renderer = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(iter->data), renderer, TRUE);
			gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(iter->data),
				renderer, "text", 0, NULL);
		}
	}
	g_slist_free(all_objects);
}


void interface_finalize(void)
{
	g_free(interface_file);
	if (objects_table != NULL)
		g_hash_table_destroy(objects_table);
	if (builder != NULL)
		g_object_unref(builder);
}


/* Compatibility functions with Glade 2 generated code.
 *
 * TODO: remove these
 */
GtkWidget *create_window1(void)
{
	return GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
}


GtkWidget *create_toolbar_popup_menu1(void)
{
	return GTK_WIDGET(gtk_builder_get_object(builder, "toolbar_popup_menu1"));
}


GtkWidget *create_edit_menu1(void)
{
	return GTK_WIDGET(gtk_builder_get_object(builder, "edit_menu1"));
}


GtkWidget *create_prefs_dialog(void)
{
	return GTK_WIDGET(gtk_builder_get_object(builder, "prefs_dialog"));
}


GtkWidget *create_project_dialog(void)
{
	return GTK_WIDGET(gtk_builder_get_object(builder, "project_dialog"));
}
