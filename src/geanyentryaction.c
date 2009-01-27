/*
 *      geanyentryaction.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* GtkAction subclass to provide a GtkEntry in a toolbar.
 * This class is missing the action_create_menu_item() function and so can't be
 * used for creating menu items. */


#include "geany.h"
#include "support.h"
#include "ui_utils.h"
#include "geanyentryaction.h"
#include <ctype.h>


typedef struct _GeanyEntryActionPrivate		GeanyEntryActionPrivate;

#define GEANY_ENTRY_ACTION_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
			GEANY_ENTRY_ACTION_TYPE, GeanyEntryActionPrivate))


struct _GeanyEntryActionPrivate
{
	GtkWidget	*entry;
	gboolean	 numeric;
};

enum
{
	ENTRY_ACTIVATE,
	ENTRY_CHANGED,

	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


static void geany_entry_action_class_init			(GeanyEntryActionClass *klass);
static void geany_entry_action_init      			(GeanyEntryAction *action);

static GtkActionClass *parent_class = NULL;

GType geany_entry_action_get_type(void)
{
	static GType self_type = 0;
	if (! self_type)
	{
		static const GTypeInfo self_info =
		{
			sizeof(GeanyEntryActionClass),
			NULL, NULL,
			(GClassInitFunc)geany_entry_action_class_init,
			NULL, NULL,
			sizeof(GeanyEntryAction),
			0,
			(GInstanceInitFunc)geany_entry_action_init,
			NULL
		};

		self_type = g_type_register_static(GTK_TYPE_ACTION, "GeanyEntryAction", &self_info, 0);
	}

	return self_type;
}


static GtkWidget *geany_entry_action_create_tool_item(GtkAction *action)
{
	GtkWidget *toolitem;
	GeanyEntryActionPrivate *priv = GEANY_ENTRY_ACTION_GET_PRIVATE(action);

	priv->entry = gtk_entry_new();
	if (priv->numeric)
		gtk_entry_set_width_chars(GTK_ENTRY(priv->entry), 9);

	ui_entry_add_clear_icon(priv->entry);

	gtk_widget_show(priv->entry);

	toolitem = g_object_new(GTK_TYPE_TOOL_ITEM, NULL);
	gtk_container_add(GTK_CONTAINER(toolitem), priv->entry);

	return toolitem;
}


static void delegate_entry_activate_cb(GtkEntry *entry, GeanyEntryAction *action)
{
	GeanyEntryActionPrivate *priv = GEANY_ENTRY_ACTION_GET_PRIVATE(action);
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(priv->entry));

	g_signal_emit(action, signals[ENTRY_ACTIVATE], 0, text);
}


static void delegate_entry_changed_cb(GtkEditable *editable, GeanyEntryAction *action)
{
	GeanyEntryActionPrivate *priv = GEANY_ENTRY_ACTION_GET_PRIVATE(action);
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(priv->entry));

	g_signal_emit(action, signals[ENTRY_CHANGED], 0, text);
}


static void entry_insert_text_cb(GtkEditable *editable, gchar *new_text, gint new_text_len,
								 gint *position, GeanyEntryAction *action)
{
	/* don't insert any text when it is not a digit */
	if (! isdigit(*new_text))
		g_signal_stop_emission_by_name(editable, "insert-text");
}


static void geany_entry_action_connect_proxy(GtkAction *action, GtkWidget *widget)
{
	GeanyEntryActionPrivate *priv = GEANY_ENTRY_ACTION_GET_PRIVATE(action);

	if (priv->numeric)
		g_signal_connect(priv->entry, "insert-text", G_CALLBACK(entry_insert_text_cb), action);
	g_signal_connect(priv->entry, "changed", G_CALLBACK(delegate_entry_changed_cb), action);
	g_signal_connect(priv->entry, "activate", G_CALLBACK(delegate_entry_activate_cb), action);

    GTK_ACTION_CLASS(parent_class)->connect_proxy(action, widget);
}


static void geany_entry_action_class_init(GeanyEntryActionClass *klass)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS(klass);

	action_class->connect_proxy = geany_entry_action_connect_proxy;
	action_class->create_tool_item = geany_entry_action_create_tool_item;
	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;

	parent_class = (GtkActionClass*)g_type_class_peek(GTK_TYPE_ACTION);
	g_type_class_add_private((gpointer)klass, sizeof(GeanyEntryActionPrivate));

	signals[ENTRY_CHANGED] = g_signal_new("entry-changed",
									G_TYPE_FROM_CLASS(klass),
									(GSignalFlags) 0,
									0,
									0,
									NULL,
									g_cclosure_marshal_VOID__STRING,
									G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[ENTRY_ACTIVATE] = g_signal_new("entry-activate",
									G_TYPE_FROM_CLASS(klass),
									(GSignalFlags) 0,
									0,
									0,
									NULL,
									g_cclosure_marshal_VOID__STRING,
									G_TYPE_NONE, 1, G_TYPE_STRING);
}


static void geany_entry_action_init(GeanyEntryAction *action)
{
	GeanyEntryActionPrivate *priv = GEANY_ENTRY_ACTION_GET_PRIVATE(action);

	priv->entry = NULL;
	priv->numeric = FALSE;
}


GtkAction *geany_entry_action_new(const gchar *name, const gchar *label,
								  const gchar *tooltip, gboolean numeric)
{
	GtkAction *action = g_object_new(GEANY_ENTRY_ACTION_TYPE,
		"name", name,
		"label", label,
		"tooltip", tooltip,
		NULL);
	GeanyEntryActionPrivate *priv = GEANY_ENTRY_ACTION_GET_PRIVATE(action);

	priv->numeric = numeric;

	return action;
}

