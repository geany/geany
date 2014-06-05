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

static GParamSpec *props[NUM_PROPERTIES];

G_DEFINE_TYPE(GeanyPage, geany_page, GTK_TYPE_BOX)


static void geany_page_init (GeanyPage * self)
{
}


static void geany_page_finalize (GObject* obj)
{
	GeanyPage * self = GEANY_PAGE(obj);
	g_free(self->label);
	g_object_unref(self->icon);
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
}


/* Constructs a new GeanyPage instance with the given label */
GtkWidget* geany_page_new_with_label(const gchar *label)
{
	return g_object_new(GEANY_TYPE_PAGE, "label", label, NULL);
}
