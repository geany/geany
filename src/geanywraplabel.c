/*
 *      geanywraplabel.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * A GtkLabel subclass that can wrap to any width, unlike GtkLabel which has a fixed wrap point.
 * (inspired by libview's WrapLabel, http://view.sourceforge.net)
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "geanywraplabel.h"


struct _GeanyWrapLabelClass
{
	GtkLabelClass parent_class;
};

typedef struct
{
	gint wrap_width;
	gint wrap_height;
} GeanyWrapLabelPrivate;

struct _GeanyWrapLabel
{
	GtkLabel parent;
	GeanyWrapLabelPrivate *priv;
};


#if GTK_CHECK_VERSION(3, 0, 0)
static gboolean geany_wrap_label_draw(GtkWidget *widget, cairo_t *cr);
static void geany_wrap_label_get_preferred_width (GtkWidget *widget,
		gint *minimal_width, gint *natural_width);
static void geany_wrap_label_get_preferred_height (GtkWidget *widget,
		gint *minimal_height, gint *natural_height);
static void geany_wrap_label_get_preferred_width_for_height (GtkWidget *widget,
		gint height, gint *minimal_width, gint *natural_width);
static void geany_wrap_label_get_preferred_height_for_width (GtkWidget *widget,
		gint width, gint *minimal_height, gint *natural_height);
static GtkSizeRequestMode geany_wrap_label_get_request_mode(GtkWidget *widget);
#else
static gboolean geany_wrap_label_expose		(GtkWidget *widget, GdkEventExpose *event);
static void geany_wrap_label_size_request	(GtkWidget *widget, GtkRequisition *req);
#endif
static void geany_wrap_label_size_allocate	(GtkWidget *widget, GtkAllocation *alloc);
static void geany_wrap_label_set_wrap_width	(GtkWidget *widget, gint width);
static void geany_wrap_label_label_notify	(GObject *object, GParamSpec *pspec, gpointer data);

G_DEFINE_TYPE(GeanyWrapLabel, geany_wrap_label, GTK_TYPE_LABEL)


static void geany_wrap_label_class_init(GeanyWrapLabelClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	widget_class->size_allocate = geany_wrap_label_size_allocate;
#if GTK_CHECK_VERSION(3, 0, 0)
	widget_class->draw = geany_wrap_label_draw;
	widget_class->get_preferred_width = geany_wrap_label_get_preferred_width;
	widget_class->get_preferred_width_for_height = geany_wrap_label_get_preferred_width_for_height;
	widget_class->get_preferred_height = geany_wrap_label_get_preferred_height;
	widget_class->get_preferred_height_for_width = geany_wrap_label_get_preferred_height_for_width;
	widget_class->get_request_mode = geany_wrap_label_get_request_mode;
#else
	widget_class->size_request = geany_wrap_label_size_request;
	widget_class->expose_event = geany_wrap_label_expose;
#endif

	g_type_class_add_private(klass, sizeof (GeanyWrapLabelPrivate));
}


static void geany_wrap_label_init(GeanyWrapLabel *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
		GEANY_WRAP_LABEL_TYPE, GeanyWrapLabelPrivate);

	self->priv->wrap_width = 0;
	self->priv->wrap_height = 0;

	g_signal_connect(self, "notify::label", G_CALLBACK(geany_wrap_label_label_notify), NULL);
	gtk_misc_set_alignment(GTK_MISC(self), 0.0, 0.0);
}


/* Sets the point at which the text should wrap. */
static void geany_wrap_label_set_wrap_width(GtkWidget *widget, gint width)
{
	GeanyWrapLabel *self = GEANY_WRAP_LABEL(widget);
	PangoLayout *layout;

	if (width <= 0)
		return;

	layout = gtk_label_get_layout(GTK_LABEL(widget));

	/*
	* We may need to reset the wrap width, so do this regardless of whether
	* or not we've changed the width.
	*/
	pango_layout_set_width(layout, width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_get_pixel_size(layout, NULL, &self->priv->wrap_height);

	if (self->priv->wrap_width != width)
	{
		self->priv->wrap_width = width;
		gtk_widget_queue_resize(widget);
	}
}


/* updates the wrap width when the label text changes */
static void geany_wrap_label_label_notify(GObject *object, GParamSpec *pspec, gpointer data)
{
	GeanyWrapLabel *self = GEANY_WRAP_LABEL(object);

	geany_wrap_label_set_wrap_width(GTK_WIDGET(object), self->priv->wrap_width);
}


#if GTK_CHECK_VERSION(3, 0, 0)
/* makes sure the layout is setup for rendering and chains to parent renderer */
static gboolean geany_wrap_label_draw(GtkWidget *widget, cairo_t *cr)
{
	GeanyWrapLabel *self = GEANY_WRAP_LABEL(widget);
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(widget));

	pango_layout_set_width(layout, self->priv->wrap_width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);

	return (* GTK_WIDGET_CLASS(geany_wrap_label_parent_class)->draw)(widget, cr);
}


static void geany_wrap_label_get_preferred_width (GtkWidget *widget,
		gint *minimal_width, gint *natural_width)
{
	*minimal_width = *natural_width = 0;
}


static void geany_wrap_label_get_preferred_width_for_height (GtkWidget *widget,
		gint height, gint *minimal_width, gint *natural_width)
{
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(widget));;

	pango_layout_set_height(layout, height * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_get_pixel_size(layout, natural_width, NULL);

	*minimal_width = 0;
}


static void geany_wrap_label_get_preferred_height (GtkWidget *widget,
		gint *minimal_height, gint *natural_height)
{
	*minimal_height = *natural_height = GEANY_WRAP_LABEL(widget)->priv->wrap_height;
}


static void geany_wrap_label_get_preferred_height_for_width (GtkWidget *widget,
		gint width, gint *minimal_height, gint *natural_height)
{
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(widget));

	pango_layout_set_width(layout, width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_get_pixel_size(layout, NULL, natural_height);

	*minimal_height = *natural_height;
}


static GtkSizeRequestMode geany_wrap_label_get_request_mode(GtkWidget *widget)
{
	return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

#else /* GTK3 */

/* makes sure the layout is setup for rendering and chains to parent renderer */
static gboolean geany_wrap_label_expose(GtkWidget *widget, GdkEventExpose *event)
{
	GeanyWrapLabel *self = GEANY_WRAP_LABEL(widget);
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(widget));

	pango_layout_set_width(layout, self->priv->wrap_width * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);

	return (* GTK_WIDGET_CLASS(geany_wrap_label_parent_class)->expose_event)(widget, event);
}


/* Forces the height to be the size necessary for the Pango layout, while allowing the
 * width to be flexible. */
static void geany_wrap_label_size_request(GtkWidget *widget, GtkRequisition *req)
{
	req->width = 0;
	req->height = GEANY_WRAP_LABEL(widget)->priv->wrap_height;
}
#endif /* GTK3 */


/* Sets the wrap width to the width allocated to us. */
static void geany_wrap_label_size_allocate(GtkWidget *widget, GtkAllocation *alloc)
{
	(* GTK_WIDGET_CLASS(geany_wrap_label_parent_class)->size_allocate)(widget, alloc);

	geany_wrap_label_set_wrap_width(widget, alloc->width);

#if GTK_CHECK_VERSION(3, 0, 0)
{
	/* ask the parent to recompute our size, because it seems GTK3 size
	 * caching is too aggressive */
	GtkWidget *parent = gtk_widget_get_parent(widget);
	if (GTK_IS_CONTAINER(parent))
		gtk_container_check_resize(GTK_CONTAINER(parent));
}
#endif
}


GtkWidget *geany_wrap_label_new(const gchar *text)
{
	return g_object_new(GEANY_WRAP_LABEL_TYPE, "label", text, NULL);
}
