/*
 *      geanypage.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/*
 * Simple container class used for pages in editor notebooks
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "geanypage.h"

#include "utils.h"

#include "gtkcompat.h"
#include <glib-object.h>

enum {
	PROP_0,

	PROP_LABEL,
	PROP_ICON,
	NUM_PROPERTIES
};

enum {
	SIG_TRY_CLOSE,

	NUM_SIGNALS
};

static GParamSpec *props[NUM_PROPERTIES];
static guint signals[NUM_SIGNALS] = { 0 };

G_DEFINE_TYPE(GeanyPage, geany_page, GTK_TYPE_BOX)

static void geany_cclosure_marshal_BOOL__VOID(GClosure *closure, GValue *return_value,
								guint n_param_values, const GValue *param_values,
								gpointer invocation_hint G_GNUC_UNUSED, gpointer marshal_data)
{
	gboolean (*callback)(gpointer *, gpointer *);
	GCClosure *cc = (GCClosure*) closure;
	gpointer data1, data2;
	gboolean v_return;

	g_return_if_fail(return_value != NULL);
	g_return_if_fail(n_param_values == 1);

	callback = marshal_data ? marshal_data : cc->callback;

	if (G_CCLOSURE_SWAP_DATA(closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer(param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer(param_values + 0);
		data2 = closure->data;
	}

	v_return = callback(data1, data2);

	g_value_set_boolean(return_value, v_return);
}

static void geany_page_init (GeanyPage * self)
{
}


static void geany_page_finalize (GObject* obj)
{
	GeanyPage * self = GEANY_PAGE(obj);
	g_free(self->label);
	if (self->icon)
		g_object_unref(self->icon);
	g_object_unref(self->tab_widget);
	G_OBJECT_CLASS (geany_page_parent_class)->finalize (obj);
}


static void geany_page_get_property(GObject *object, guint       property_id,
                                    GValue  *value,  GParamSpec *pspec)
{
	GeanyPage *self = GEANY_PAGE(object);

	switch (property_id)
	{
		case PROP_LABEL:
			g_value_set_string(value, self->label);
			break;
		case PROP_ICON:
			g_value_set_object(value, self->icon);
			break;
		default:
			g_assert_not_reached();
			break;
	}
}


static void geany_page_set_property(GObject      *object, guint       property_id,
                                    const GValue *value,  GParamSpec *pspec)
{
	GeanyPage *self = GEANY_PAGE(object);

	switch (property_id)
	{
		case PROP_LABEL:
			SETPTR(self->label, g_value_dup_string(value));
			break;
		case PROP_ICON:
			if (self->icon)
				g_object_unref(self->icon);
			self->icon = g_value_dup_object(value);
			break;
		default:
			g_assert_not_reached();
			break;
	}
}


static void geany_page_class_init (GeanyPageClass * klass)
{
	GObjectClass *oklass = G_OBJECT_CLASS (klass);
	oklass->finalize     = geany_page_finalize;
	oklass->get_property = geany_page_get_property;
	oklass->set_property = geany_page_set_property;

	props[PROP_LABEL] =
	g_param_spec_string("label",
	                    "Label of page",
	                    "The label used for the tab and tab click menu",
	                    "unset" /* default value */,
	                    G_PARAM_READWRITE);

	props[PROP_ICON] =
	g_param_spec_object("icon",
	                    "Icon of page",
	                    "The icon used for the tab click menu",
	                    G_TYPE_ICON /* GType */,
	                    G_PARAM_READWRITE);

	g_object_class_install_properties(oklass, NUM_PROPERTIES, props);

	signals[SIG_TRY_CLOSE] = g_signal_new (g_intern_static_string("try-close"),
	                    G_OBJECT_CLASS_TYPE (oklass),
	                    G_SIGNAL_RUN_LAST,
	                    0, NULL, NULL,
	                    geany_cclosure_marshal_BOOL__VOID,
	                    G_TYPE_BOOLEAN, 0);
}


static void update_tab_widget_name(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
	GtkWidget *w = GTK_WIDGET(user_data);

	gtk_widget_set_name(w, gtk_widget_get_name(GTK_WIDGET(obj)));
}


/* Constructs a new GeanyPage instance with the given label */
GtkWidget* geany_page_new_with_label(const gchar *label_text)
{
	GtkWidget *label, *ebox;
	GeanyPage *self = g_object_new(GEANY_TYPE_PAGE,
	                              "label", label_text,
	                              "orientation", GTK_ORIENTATION_VERTICAL, NULL);

	label = gtk_label_new(label_text);
	/* we need to use an event box for the tooltip, labels don't get the necessary events */
	ebox = gtk_event_box_new();
	gtk_widget_set_has_window(ebox, FALSE);
	gtk_container_add(GTK_CONTAINER(ebox), label);
	g_signal_connect_object(ebox, "notify::name", G_CALLBACK(update_tab_widget_name), label, 0);

	self->tab_widget = g_object_ref(ebox);

	return (GtkWidget *) self;
}


gboolean geany_page_try_close(GeanyPage *self)
{
	gboolean ret = TRUE;

	/* The handler should destroy the tab (usually by calling gtk_notebook_remove_page). Keep a ref
	 * to avoid actually freeing it during the signal handling (gtk_widget_destroy() will detach
	 * the widget, but not finalize if it's ref'd) */
	g_object_ref(self);
	g_signal_emit(self, signals[SIG_TRY_CLOSE], NULL, &ret);
	g_object_unref(self);

	return ret;
}
