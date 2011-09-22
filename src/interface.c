/*
 * Compatibility wrapper for old Glade2 code generation file.
 *
 * DO NOT ADD NEW CODE HERE!
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#define GLADE_HOOKUP_OBJECT(component, widget, name) \
	g_object_set_data_full(G_OBJECT(component), name, \
		g_object_ref(G_OBJECT(widget)), (GDestroyNotify) g_object_unref)

#define GLADE_HOOKUP_OBJECT_NO_REF(component, widget, name) \
	g_object_set_data(G_OBJECT(component), name, G_OBJECT(widget))


static gchar *ui_file = NULL;
static GtkBuilder *builder = NULL;
static GSList *objects = NULL;

static void init_builder(void);


GObject *interface_get_object(const gchar *name)
{
	GSList *iter;

	init_builder();

	g_return_val_if_fail(name != NULL, NULL);
	g_return_val_if_fail(objects != NULL, NULL);

	for (iter = objects; iter != NULL; iter = g_slist_next(iter))
	{
		if (G_IS_OBJECT(iter->data))
		{
			if (g_strcmp0(g_object_get_data(iter->data, "name"), name) == 0)
				return G_OBJECT(iter->data);
			else if (g_strcmp0(g_object_get_data(iter->data, "gtk-builder-name"), name) == 0)
				return G_OBJECT(iter->data);
			else if (GTK_IS_BUILDABLE(iter->data) &&
				g_strcmp0(gtk_buildable_get_name(GTK_BUILDABLE(iter->data)), name) == 0)
			{
				return G_OBJECT(iter->data);
			}
			else if (GTK_IS_WIDGET(iter->data) &&
				g_strcmp0(gtk_widget_get_name(GTK_WIDGET(iter->data)), name) == 0)
			{
				return G_OBJECT(iter->data);
			}
		}
	}

	g_debug("Can't locate: %s", name);

	return NULL;
}


void interface_set_object(GObject *obj, const gchar *name)
{
	init_builder();
	g_return_if_fail(G_IS_OBJECT(obj));

	if (! name)
		name = g_object_get_data(obj, "name");
	if (! name)
		name = g_object_get_data(obj, "gtk-builder-name");
	if (! name && GTK_IS_BUILDABLE(obj))
		name = gtk_buildable_get_name(GTK_BUILDABLE(obj));
	else if (! name && GTK_IS_WIDGET(obj))
		name = gtk_widget_get_name(GTK_WIDGET(obj));

	g_return_if_fail(name);

	if (g_strcmp0("entry_shell", name) == 0)
		g_debug("I know i added the entry_shell object.");

	g_object_set_data_full(obj, "name", g_strdup(name), g_free);

	objects = g_slist_append(objects, g_object_ref(obj));
}


static void init_builder(void)
{
	GError *error;
	GSList *iter, *all_objects;

	if (builder) /* only run once */
		return;

	ui_file = g_build_filename(GEANY_DATADIR, "geany", "geany.glade", NULL);

	if (! (builder = gtk_builder_new()))
	{
		g_error("failed to initialize the user-interface");
		return;
	}

	error = NULL;
	if (! gtk_builder_add_from_file(builder, ui_file, &error))
	{
		g_error("failed to load the user-interface file: %s",
			error->message);
		g_error_free(error);
		return;
	}

	/* TODO: make sure all callbacks have G_MODULE_EXPORT */
	gtk_builder_connect_signals(builder, NULL);

	/* TODO: set translation domain? */;

	all_objects = gtk_builder_get_objects(builder);
	for (iter = all_objects; iter != NULL; iter = g_slist_next(iter))
	{
		interface_set_object(iter->data, NULL);
		//g_debug("ADDED OBJECT: %s", g_object_get_data(iter->data, "name"));
		if (GTK_IS_COMBO_BOX(iter->data))
		{
			GtkCellRenderer *r = gtk_cell_renderer_text_new();
			gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(iter->data), r, TRUE);
			gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(iter->data),
				r, "text", 0, NULL);
		}
	}
}


/* TODO: Hookup to atexit() or make public and call at program exit? */
/*       Ensure everything is freed. */
static void finalize_builder(void)
{
	GSList *iter;
	g_return_if_fail(GTK_IS_BUILDER(builder));
	for (iter = objects; iter != NULL; iter = g_slist_next(iter))
		g_object_unref(iter->data);
	g_slist_free(objects);
	g_free(ui_file);
	g_object_unref(builder);
}


GtkWidget *create_window1(void)
{
	init_builder();
	return GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
}


GtkWidget *create_toolbar_popup_menu1(void)
{
	init_builder();
	return GTK_WIDGET(gtk_builder_get_object(builder, "toolbar_popup_menu1"));
}


GtkWidget *create_edit_menu1(void)
{
	init_builder();
	return GTK_WIDGET(gtk_builder_get_object(builder, "edit_menu1"));
}


GtkWidget *create_prefs_dialog(void)
{
	init_builder();
	return GTK_WIDGET(gtk_builder_get_object(builder, "prefs_dialog"));
}


GtkWidget *create_project_dialog(void)
{
	init_builder();
	return GTK_WIDGET(gtk_builder_get_object(builder, "project_dialog"));
}
