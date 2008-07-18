/*
 *      geanywraplabel.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *  $Id$
 */

/*
 * A GtkLabel subclass that can wrap to any width, unlike GtkLabel which has a fixed wrap point.
 * (inspired by libview's WrapLabel, http://view.sourceforge.net)
 */


#include <gtk/gtklabel.h>
#include "geanywraplabel.h"


/* Local data */
static GObjectClass *parent_class = NULL;



#define GEANY_WRAP_LABEL_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
	GEANY_WRAP_LABEL_TYPE, GeanyWrapLabelPrivate))


struct _GeanyWrapLabelClass
{
	GtkLabelClass parent_class;
};

struct _GeanyWrapLabel
{
	GtkLabel parent;
};

typedef struct
{
	gsize wrap_width;
} GeanyWrapLabelPrivate;


static void geany_wrap_label_class_init		(GeanyWrapLabelClass *klass);
static void geany_wrap_label_init			(GeanyWrapLabel *self);
static void geany_wrap_label_size_request	(GtkWidget *widget, GtkRequisition *req);
static void geany_wrap_label_size_allocate	(GtkWidget *widget, GtkAllocation *alloc);
static void geany_wrap_label_set_wrap_width	(GtkWidget *widget, gsize width);



GType geany_wrap_label_get_type()
{
	static GType type = G_TYPE_INVALID;

	if (G_UNLIKELY(type == G_TYPE_INVALID))
	{
		GTypeInfo gwl_info = {
			sizeof (GeanyWrapLabelClass),
			NULL, NULL,
			(GClassInitFunc) geany_wrap_label_class_init,
			NULL,
			NULL,
			sizeof (GeanyWrapLabel),
			3,
			(GInstanceInitFunc) geany_wrap_label_init,
			NULL
		};

		type = g_type_register_static(GTK_TYPE_LABEL, "GeanyWrapLabel", &gwl_info, 0);
	}

	return type;
}


static void geany_wrap_label_class_init(GeanyWrapLabelClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	widget_class->size_request = geany_wrap_label_size_request;
	widget_class->size_allocate = geany_wrap_label_size_allocate;

	g_type_class_add_private(klass, sizeof (GeanyWrapLabelPrivate));
}


static void geany_wrap_label_init(GeanyWrapLabel *self)
{
	GeanyWrapLabelPrivate *priv = GEANY_WRAP_LABEL_GET_PRIVATE(self);

	priv->wrap_width = 0;
}


/* Sets the point at which the text should wrap. */
static void geany_wrap_label_set_wrap_width(GtkWidget *widget, gsize width)
{
	GeanyWrapLabelPrivate *priv;

	if (width == 0)
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
	(* GTK_WIDGET_CLASS(parent_class)->size_allocate)(widget, alloc);

	geany_wrap_label_set_wrap_width(widget, alloc->width);
}


void geany_wrap_label_set_text(GtkLabel *label, const gchar *text)
{
	GeanyWrapLabelPrivate *priv = GEANY_WRAP_LABEL_GET_PRIVATE(label);

	gtk_label_set_text(label, text);
	geany_wrap_label_set_wrap_width(GTK_WIDGET(label), priv->wrap_width);
}


GtkWidget *geany_wrap_label_new(const gchar *text)
{
	GtkWidget *l = g_object_new(GEANY_WRAP_LABEL_TYPE, NULL);

	if (text != NULL && text[0] != '\0')
		gtk_label_set_text(GTK_LABEL(l), text);

	pango_layout_set_wrap(gtk_label_get_layout(GTK_LABEL(l)), PANGO_WRAP_WORD_CHAR);
	gtk_misc_set_alignment(GTK_MISC(l), 0.0, 0.0);

	return l;
}
