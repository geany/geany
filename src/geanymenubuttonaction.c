/*
 *      geanymenubuttonaction.c - this file is part of Geany, a fast and lightweight IDE
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

/* GtkAction subclass to provide a GtkMenuToolButton in a toolbar.
 * This class is missing the action_create_menu_item() function and so can't be
 * used for creating menu items. */


#include "geany.h"
#include "support.h"
#include "utils.h"
#include "geanymenubuttonaction.h"


typedef struct _GeanyMenubuttonActionPrivate		GeanyMenubuttonActionPrivate;

#define GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(obj)	(GEANY_MENU_BUTTON_ACTION(obj)->priv)


struct _GeanyMenubuttonActionPrivate
{
	GtkWidget	*menu;

	gchar *tooltip_arrow;
};

enum
{
	PROP_0,
	PROP_TOOLTIP_ARROW
};

enum
{
	BUTTON_CLICKED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


G_DEFINE_TYPE(GeanyMenubuttonAction, geany_menu_button_action, GTK_TYPE_ACTION)


static void geany_menu_button_action_finalize(GObject *object)
{
	GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(object);

	g_object_unref(priv->menu);
	g_free(priv->tooltip_arrow);

	(* G_OBJECT_CLASS(geany_menu_button_action_parent_class)->finalize)(object);
}


static void delegate_button_activated(GtkAction *action)
{
	g_signal_emit(action, signals[BUTTON_CLICKED], 0);
}


static void geany_menu_button_action_set_property(GObject *object, guint prop_id,
												  const GValue *value, GParamSpec *pspec)
{
	switch (prop_id)
	{
	case PROP_TOOLTIP_ARROW:
	{
		GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(object);
		g_free(priv->tooltip_arrow);
		priv->tooltip_arrow = g_value_dup_string(value);
		break;
	}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}


static GtkWidget *geany_menu_button_action_create_tool_item(GtkAction *action)
{
	GtkWidget *toolitem;
	GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(action);

	toolitem = g_object_new(GTK_TYPE_MENU_TOOL_BUTTON, NULL);
	gtk_menu_tool_button_set_arrow_tooltip_text(GTK_MENU_TOOL_BUTTON(toolitem), priv->tooltip_arrow);

	return toolitem;
}


static void geany_menu_button_action_class_init(GeanyMenubuttonActionClass *klass)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS(klass);
	GObjectClass *g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = geany_menu_button_action_finalize;
	g_object_class->set_property = geany_menu_button_action_set_property;

	action_class->activate = delegate_button_activated;
	action_class->create_tool_item = geany_menu_button_action_create_tool_item;
	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;

	g_type_class_add_private(klass, sizeof(GeanyMenubuttonActionPrivate));

	g_object_class_install_property(g_object_class,
									PROP_TOOLTIP_ARROW,
									g_param_spec_string(
									"tooltip-arrow",
									"Arrow tooltip",
									"A special tooltip for the arrow button",
									"",
									G_PARAM_WRITABLE));

	signals[BUTTON_CLICKED] = g_signal_new("button-clicked",
										G_TYPE_FROM_CLASS(klass),
										(GSignalFlags) 0,
										0,
										0,
										NULL,
										g_cclosure_marshal_VOID__VOID,
										G_TYPE_NONE, 0);
}


static void geany_menu_button_action_init(GeanyMenubuttonAction *action)
{
	GeanyMenubuttonActionPrivate *priv;

	action->priv = G_TYPE_INSTANCE_GET_PRIVATE(action,
		GEANY_MENU_BUTTON_ACTION_TYPE, GeanyMenubuttonActionPrivate);

	priv = action->priv;
	priv->tooltip_arrow = NULL;
	priv->menu = NULL;
}


GtkAction *geany_menu_button_action_new(const gchar *name,
										const gchar *label,
									    const gchar *tooltip,
									    const gchar *tooltip_arrow,
										const gchar *stock_id)
{
	GtkAction *action = g_object_new(GEANY_MENU_BUTTON_ACTION_TYPE,
		"name", name,
		"label", label,
		"tooltip", tooltip,
		"tooltip-arrow", tooltip_arrow,
		"stock-id", stock_id,
		NULL);

	return action;
}


GtkWidget *geany_menu_button_action_get_menu(GeanyMenubuttonAction *action)
{
	GeanyMenubuttonActionPrivate *priv;

	g_return_val_if_fail(action != NULL, NULL);

	priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(action);

	return priv->menu;
}


static void menu_items_changed_cb(GtkContainer *container, GtkWidget *widget, GeanyMenubuttonAction *action)
{
	GeanyMenubuttonActionPrivate *priv;
	gboolean enable;
	GSList *l;

	g_return_if_fail(action != NULL);

	priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(action);
	if (priv->menu != NULL)
	{
		GList *children = gtk_container_get_children(GTK_CONTAINER(priv->menu));

		enable = (g_list_length(children) > 0);
		g_list_free(children);
	}
	else
		enable = FALSE;

	foreach_slist(l, gtk_action_get_proxies(GTK_ACTION(action)))
	{
		/* On Windows a GtkImageMenuItem proxy is created for whatever reason. So we filter
		 * by type and act only on GtkMenuToolButton proxies. */
		/* TODO find why the GtkImageMenuItem proxy is created */
		if (! GTK_IS_MENU_TOOL_BUTTON(l->data))
			continue;

		if (enable)
		{
			if (gtk_menu_tool_button_get_menu(GTK_MENU_TOOL_BUTTON(l->data)) == NULL)
				gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(l->data), priv->menu);
		}
		else
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(l->data), NULL);
	}
}


void geany_menu_button_action_set_menu(GeanyMenubuttonAction *action, GtkWidget *menu)
{
	GeanyMenubuttonActionPrivate *priv;

	g_return_if_fail(action != NULL);

	priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(action);

	if (priv->menu != NULL && GTK_IS_WIDGET(priv->menu))
		g_signal_handlers_disconnect_by_func(priv->menu, menu_items_changed_cb, action);
	if (menu != NULL)
	{
		g_signal_connect(menu, "add", G_CALLBACK(menu_items_changed_cb), action);
		g_signal_connect(menu, "remove", G_CALLBACK(menu_items_changed_cb), action);
	}

	priv->menu = menu;

	menu_items_changed_cb(GTK_CONTAINER(menu), NULL, action);
}
