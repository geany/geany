/*
 *      geanyobject.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef __GEANYOBJECT_H__
#define __GEANYOBJECT_H__

#include <glib-object.h>

G_BEGIN_DECLS

extern GObject *geany_object;

typedef enum
{
	GCB_DOCUMENT_NEW,
	GCB_DOCUMENT_OPEN,
	GCB_DOCUMENT_SAVE,
	GCB_MAX
} GeanyCallbackId;


#define GEANY_OBJECT_TYPE				(geany_object_get_type())
#define GEANY_OBJECT(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
		GEANY_OBJECT_TYPE, GeanyObject))
#define GEANY_OBJECT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
		GEANY_OBJECT_TYPE, GeanyObjectClass))
#define IS_GEANY_OBJECT(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
		GEANY_OBJECT_TYPE))
#define IS_GEANY_OBJECT_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass),\
		GEANY_OBJECT_TYPE))

typedef struct _GeanyObject				GeanyObject;
typedef struct _GeanyObjectClass			GeanyObjectClass;

struct _GeanyObject
{
	GObject parent;
	/* add your public declarations here */
};

struct SCNotification;

struct _GeanyObjectClass
{
	GObjectClass parent_class;

	void (*document_new)(gint idx);
	void (*document_open)(gint idx);
	void (*document_save)(gint idx);
};

GType		geany_object_get_type	(void);
GObject*	geany_object_new			(void);

G_END_DECLS

#endif /* __GEANYOBJECT_H__ */
