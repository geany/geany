/*
 *      geanymenubuttonaction.h - this file is part of Geany, a fast and lightweight IDE
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef GEANY_MENU_BUTTON_ACTION_H
#define GEANY_MENU_BUTTON_ACTION_H

G_BEGIN_DECLS

#define GEANY_MENU_BUTTON_ACTION_TYPE				(geany_menu_button_action_get_type())
#define GEANY_MENU_BUTTON_ACTION(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			GEANY_MENU_BUTTON_ACTION_TYPE, GeanyMenubuttonAction))
#define GEANY_MENU_BUTTON_ACTION_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			GEANY_MENU_BUTTON_ACTION_TYPE, GeanyMenubuttonActionClass))
#define IS_GEANY_MENU_BUTTON_ACTION(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			GEANY_MENU_BUTTON_ACTION_TYPE))
#define IS_GEANY_MENU_BUTTON_ACTION_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			GEANY_MENU_BUTTON_ACTION_TYPE))

typedef struct _GeanyMenubuttonAction				GeanyMenubuttonAction;
typedef struct _GeanyMenubuttonActionClass			GeanyMenubuttonActionClass;

struct _GeanyMenubuttonActionPrivate;

struct _GeanyMenubuttonAction
{
	GtkAction parent;
	struct _GeanyMenubuttonActionPrivate *priv;
};

struct _GeanyMenubuttonActionClass
{
	GtkActionClass parent_class;
};

GType		geany_menu_button_action_get_type	(void);
GtkAction*	geany_menu_button_action_new		(const gchar	*name,
												 const gchar	*label,
												 const gchar	*tooltip,
												 const gchar	*tooltip_arrow,
												 const gchar	*stock_id);
GtkWidget*	geany_menu_button_action_get_menu	(GeanyMenubuttonAction *action);

void		geany_menu_button_action_set_menu	(GeanyMenubuttonAction *action, GtkWidget *menu);

G_END_DECLS

#endif /* GEANY_MENU_BUTTON_ACTION_H */
