/*
 *      geanywraplabel.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 */

/*
 * A GtkLabel subclass that can wrap to any width, unlike GtkLabel which has a fixed wrap point.
 * (inspired by libview's WrapLabel, http://view.sourceforge.net)
 */


#include "geany.h"
#include "utils.h"
#include "geanywraplabel.h"



#define GEANY_WRAP_LABEL_GET_PRIVATE(obj)	(GEANY_WRAP_LABEL(obj)->priv)


struct _GeanyWrapLabelClass
{
	GtkLabelClass parent_class;
};

typedef struct
{
	gint wrap_width;
} GeanyWrapLabelPrivate;

struct _GeanyWrapLabel
{
	GtkLabel parent;
	GeanyWrapLabelPrivate *priv;
};


static void geany_wrap_label_size_request	(GtkWidget *widget, GtkRequisition *req);
static void geany_wrap_label_size_allocate	(GtkWidget *widget, GtkAllocation *alloc);
static void geany_wrap_label_set_wrap_width	(GtkWidget *widget, gint width);
static void geany_wrap_label_label_notify	(GObject *object, GParamSpec *pspec, gpointer data);

G_DEFINE_TYPE(GeanyWrapLabel, geany_wrap_label, GTK_TYPE_LABEL)


static void geany_wrap_label_class_init(GeanyWrapLabelClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	widget_class->size_request = geany_wrap_label_size_request;
	widget_class->size_allocate = geany_wrap_label_size_allocate;

	g_type_class_add_private(klass, sizeof (GeanyWrapLabelPrivate));
}


static void geany_wrap_label_init(GeanyWrapLabel *self)
{
	GeanyWrapLabelPrivate *priv;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
		GEANY_WRAP_LABEL_TYPE, GeanyWrapLabelPrivate);

	priv = self->priv;
	priv->wrap_width = 0;

	g_signal_connect(self, "notify::label", G_CALLBACK(geany_wrap_label_label_notify), NULL);
	pango_layout_set_wrap(gtk_label_get_layout(GTK_LABEL(self)), PANGO_WRAP_WORD_CHAR);
	gtk_misc_set_alignment(GTK_MISC(self), 0.0, 0.0);
}


/* Sets the point at which the text should wrap. */
static void geany_wrap_label_set_wrap_width(GtkWidget *widget, gint width)
{
	GeanyWrapLabelPrivate *priv;

	if (width <= 0)
		return;

	/*
	* We may need to reset the wrap width, so do this regardless of whether
	* or not we've changed the width.
	*/
	pango_layout_set_width(gtk_label_get_layout(GTK_LABEL(widget)), width * PANGO_SCALE);

	priv = GEANY_WRAP_LABEL_GET_PRIVATE(widget);
	if (priv->wrap_width != width)
	{
		priv->wrap_width = width;
		gtk_widget_queue_resize(widget);
	}
}


/* updates the wrap width when the label text changes */
static void geany_wrap_label_label_notify(GObject *object, GParamSpec *pspec, gpointer data)
{
	GeanyWrapLabelPrivate *priv = GEANY_WRAP_LABEL_GET_PRIVATE(object);

	geany_wrap_label_set_wrap_width(GTK_WIDGET(object), priv->wrap_width);
}


/* Forces the height to be the size necessary for the Pango layout, while allowing the
 * width to be flexible. */
static void geany_wrap_label_size_request(GtkWidget *widget, GtkRequisition *req)
{
	gint height;

	pango_layout_get_pixel_size(gtk_label_get_layout(GTK_LABEL(widget)), NULL, &height);

	req->width  = 0;
	req->height = height;
}


/* Sets the wrap width to the width allocated to us. */
static void geany_wrap_label_size_allocate(GtkWidget *widget, GtkAllocation *alloc)
{
	(* GTK_WIDGET_CLASS(geany_wrap_label_parent_class)->size_allocate)(widget, alloc);

	geany_wrap_label_set_wrap_width(widget, alloc->width);
}


GtkWidget *geany_wrap_label_new(const gchar *text)
{
	return g_object_new(GEANY_WRAP_LABEL_TYPE, "label", text, NULL);
}
