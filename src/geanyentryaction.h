/*
 *      geanyentryaction.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either vergeany 2 of the License, or
 *      (at your option) any later vergeany.
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


#ifndef __GEANY_ENTRY_ACTION_H__
#define __GEANY_ENTRY_ACTION_H__

G_BEGIN_DECLS

#define GEANY_ENTRY_ACTION_TYPE					(geany_entry_action_get_type())
#define GEANY_ENTRY_ACTION(obj)					(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			GEANY_ENTRY_ACTION_TYPE, GeanyEntryAction))
#define GEANY_ENTRY_ACTION_CLASS(klass)			(G_TYPE_CHECK_CLASS_CAST((klass),\
			GEANY_ENTRY_ACTION_TYPE, GeanyEntryActionClass))
#define IS_GEANY_ENTRY_ACTION(obj)				(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			GEANY_ENTRY_ACTION_TYPE))
#define IS_GEANY_ENTRY_ACTION_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass),\
			GEANY_ENTRY_ACTION_TYPE))

typedef struct _GeanyEntryAction				GeanyEntryAction;
typedef struct _GeanyEntryActionClass			GeanyEntryActionClass;

struct _GeanyEntryActionPrivate;

struct _GeanyEntryAction
{
	GtkAction parent;
	struct _GeanyEntryActionPrivate *priv;
};

struct _GeanyEntryActionClass
{
	GtkActionClass parent_class;
};

GType		geany_entry_action_get_type	(void);
GtkAction*	geany_entry_action_new		(const gchar	*name,
										 const gchar	*label,
										 const gchar	*tooltip,
										 gboolean numeric);
GtkWidget*	geany_entry_action_get_menu	(GeanyEntryAction *action);

G_END_DECLS

#endif /* __GEANY_ENTRY_ACTION_H__ */
