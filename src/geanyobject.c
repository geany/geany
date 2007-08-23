/*
 *      geanyobject.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* GObject used for connecting and emitting signals when certain events happen,
 * e.g. opening a document.
 * Mainly used for plugins - geany_object is created in plugins_init(). */

#include "geany.h"
#include "geanyobject.h"

GObject	*geany_object;

static guint geany_object_signals[GCB_MAX] = { 0 };


typedef struct _GeanyObjectPrivate GeanyObjectPrivate;

#define GEANY_OBJECT_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
		GEANY_OBJECT_TYPE, GeanyObjectPrivate))

struct _GeanyObjectPrivate
{
	/* add your private declarations here */
	gchar dummy; // to avoid warnings (g_type_class_add_private: assertion `private_size > 0' failed)
};

static void geany_object_class_init			(GeanyObjectClass *klass);
static void geany_object_init				(GeanyObject *self);
static void geany_object_finalize			(GObject *object);

/* Local data */
static GObjectClass *parent_class = NULL;


GType geany_object_get_type(void)
{
	static GType self_type = 0;
	if (! self_type)
	{
		static const GTypeInfo self_info =
		{
			sizeof(GeanyObjectClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)geany_object_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(GeanyObject),
			0,
			(GInstanceInitFunc)geany_object_init,
			NULL
		};

		self_type = g_type_register_static(G_TYPE_OBJECT, "GeanyObject", &self_info, 0);	}

	return self_type;
}


static void create_signals(GObjectClass *g_object_class)
{
	geany_object_signals[GCB_DOCUMENT_NEW] = g_signal_new (
		"document-new",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_new),
		NULL, NULL,
		gtk_marshal_NONE__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);
	geany_object_signals[GCB_DOCUMENT_OPEN] = g_signal_new (
		"document-open",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_open),
		NULL, NULL,
		gtk_marshal_NONE__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);
	geany_object_signals[GCB_DOCUMENT_SAVE] = g_signal_new (
		"document-save",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_save),
		NULL, NULL,
		gtk_marshal_NONE__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);
	geany_object_signals[GCB_DOCUMENT_ACTIVATE] = g_signal_new (
		"document-activate",
		G_OBJECT_CLASS_TYPE (g_object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (GeanyObjectClass, document_activate),
		NULL, NULL,
		gtk_marshal_NONE__INT,
		G_TYPE_NONE, 1,
		G_TYPE_INT);
}


static void geany_object_class_init(GeanyObjectClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = geany_object_finalize;

	parent_class = (GObjectClass*)g_type_class_peek(G_TYPE_OBJECT);
	g_type_class_add_private((gpointer)klass, sizeof(GeanyObjectPrivate));

	create_signals(g_object_class);
}


static void geany_object_init(GeanyObject *self)
{

}


GObject* geany_object_new(void)
{
	return (GObject*)g_object_new(GEANY_OBJECT_TYPE, NULL);
}


void geany_object_finalize(GObject *object)
{
	GeanyObject *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_GEANY_OBJECT(object));

	self = GEANY_OBJECT(object);

	if (G_OBJECT_CLASS(parent_class)->finalize)
		(* G_OBJECT_CLASS(parent_class)->finalize)(object);
}

