/*
 *      geanymenubuttonaction.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* GtkAction subclass to provide a GtkMenuToolButton in a toolbar.
 * This class is missing the action_create_menu_item() function and so can't be
 * used for creating menu items. */


#include "geany.h"
#include "support.h"
#include "geanymenubuttonaction.h"


typedef struct _GeanyMenubuttonActionPrivate		GeanyMenubuttonActionPrivate;

#define GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
			GEANY_MENU_BUTTON_ACTION_TYPE, GeanyMenubuttonActionPrivate))


struct _GeanyMenubuttonActionPrivate
{
	GtkWidget	*button;
	GtkWidget	*menu;
	gboolean	 menu_added;
};

enum
{
	BUTTON_CLICKED,

	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


static void geany_menu_button_action_class_init			(GeanyMenubuttonActionClass *klass);
static void geany_menu_button_action_init      			(GeanyMenubuttonAction *action);

static GtkActionClass *parent_class = NULL;

GType geany_menu_button_action_get_type(void)
{
	static GType self_type = 0;
	if (! self_type)
	{
		static const GTypeInfo self_info =
		{
			sizeof(GeanyMenubuttonActionClass),
			NULL, NULL,
			(GClassInitFunc)geany_menu_button_action_class_init,
			NULL, NULL,
			sizeof(GeanyMenubuttonAction),
			0,
			(GInstanceInitFunc)geany_menu_button_action_init,
			NULL
		};

		self_type = g_type_register_static(GTK_TYPE_ACTION, "GeanyMenubuttonAction", &self_info, 0);
	}

	return self_type;
}


static void geany_menu_button_action_finalize(GObject *object)
{
	GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(object);

	g_object_unref(priv->menu);

	if (G_OBJECT_CLASS(parent_class)->finalize)
		(* G_OBJECT_CLASS(parent_class)->finalize)(object);
}


static void menu_filled_cb(GtkContainer *container, GtkWidget *widget, gpointer data)
{
	GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(data);

	if (! priv->menu_added)
	{
		gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(priv->button), priv->menu);
		priv->menu_added = TRUE;
	}
}


static void geany_menu_button_action_connect_proxy(GtkAction *action, GtkWidget *widget)
{
	GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(action);
	/* add the menu to the menu button once it got items ("add" from GtkContainer) */
	g_signal_connect(priv->menu, "add", G_CALLBACK(menu_filled_cb), action);

    GTK_ACTION_CLASS(parent_class)->connect_proxy(action, widget);
}


static void delegate_button_activated(GtkAction *action)
{
	g_signal_emit(action, signals[BUTTON_CLICKED], 0);
}


static GtkWidget *geany_menu_button_action_create_tool_item(GtkAction *action)
{
	GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(action);

	priv->menu = gtk_menu_new();
	g_object_ref(priv->menu);
	gtk_widget_show(priv->menu);

	priv->button = g_object_new(GTK_TYPE_MENU_TOOL_BUTTON, NULL);

	return priv->button;
}


static void geany_menu_button_action_class_init(GeanyMenubuttonActionClass *klass)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS(klass);
	GObjectClass *g_object_class = G_OBJECT_CLASS(klass);

	g_object_class->finalize = geany_menu_button_action_finalize;

	action_class->activate = delegate_button_activated;
	action_class->connect_proxy = geany_menu_button_action_connect_proxy;
	action_class->create_tool_item = geany_menu_button_action_create_tool_item;
	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;

	parent_class = (GtkActionClass*)g_type_class_peek(GTK_TYPE_ACTION);
	g_type_class_add_private((gpointer)klass, sizeof(GeanyMenubuttonActionPrivate));

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
	GeanyMenubuttonActionPrivate *priv = GEANY_MENU_BUTTON_ACTION_GET_PRIVATE(action);

	priv->menu = NULL;
	priv->button = NULL;
	priv->menu_added = FALSE;
}


GtkAction *geany_menu_button_action_new(const gchar *name,
										const gchar *label,
									    const gchar *tooltip,
										const gchar *stock_id)
{
	GtkAction *action = g_object_new(GEANY_MENU_BUTTON_ACTION_TYPE,
		"name", name,
		"label", label,
		"tooltip", tooltip,
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
