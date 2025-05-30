/*
 *      geanyentryaction.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 The Geany contributors
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

/* GtkAction subclass to provide a GtkEntry in a toolbar.
 * This class is missing the action_create_menu_item() function and so can't be
 * used for creating menu items. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "geanyentryaction.h"

#include "ui_utils.h"

#include <ctype.h>


typedef struct _GeanyEntryActionPrivate		GeanyEntryActionPrivate;

#define GEANY_ENTRY_ACTION_GET_PRIVATE(obj)	(GEANY_ENTRY_ACTION(obj)->priv)


struct _GeanyEntryActionPrivate
{
	gboolean	 numeric;
};

enum
{
	ENTRY_ACTIVATE,
	ENTRY_ACTIVATE_BACKWARD,
	ENTRY_CHANGED,

	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


G_DEFINE_TYPE(GeanyEntryAction, geany_entry_action, GTK_TYPE_ACTION)


static void delegate_entry_activate_cb(GtkEntry *entry, GeanyEntryAction *action)
{
	const gchar *text = gtk_entry_get_text(entry);

	g_signal_emit(action, signals[ENTRY_ACTIVATE], 0, text);
}


static void delegate_entry_activate_backward_cb(GtkEntry *entry, GeanyEntryAction *action)
{
	const gchar *text = gtk_entry_get_text(entry);

	g_signal_emit(action, signals[ENTRY_ACTIVATE_BACKWARD], 0, text);
}


static void delegate_entry_changed_cb(GtkEditable *editable, GeanyEntryAction *action)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(editable));

	g_signal_emit(action, signals[ENTRY_CHANGED], 0, text);
}


static GtkWidget *geany_entry_action_create_tool_item(GtkAction *action)
{
	GtkWidget *toolitem;
	GtkWidget *entry;
	GeanyEntryActionPrivate *priv = GEANY_ENTRY_ACTION_GET_PRIVATE(action);

	entry = gtk_entry_new();
	if (priv->numeric)
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 9);

	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	ui_entry_add_activate_backward_signal(GTK_ENTRY(entry));

	if (priv->numeric)
		g_signal_connect(entry, "insert-text",
			G_CALLBACK(ui_editable_insert_text_callback), NULL);
	g_signal_connect(entry, "changed", G_CALLBACK(delegate_entry_changed_cb), action);
	g_signal_connect(entry, "activate", G_CALLBACK(delegate_entry_activate_cb), action);
	g_signal_connect(entry, "activate-backward",
		G_CALLBACK(delegate_entry_activate_backward_cb), action);

	gtk_widget_show(entry);

	toolitem = g_object_new(GTK_TYPE_TOOL_ITEM, NULL);
	gtk_container_add(GTK_CONTAINER(toolitem), entry);

	return toolitem;
}


static void geany_entry_action_class_init(GeanyEntryActionClass *klass)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS(klass);

	action_class->create_tool_item = geany_entry_action_create_tool_item;
	action_class->toolbar_item_type = GTK_TYPE_MENU_TOOL_BUTTON;

	g_type_class_add_private(klass, sizeof(GeanyEntryActionPrivate));

	signals[ENTRY_CHANGED] = g_signal_new("entry-changed",
									G_TYPE_FROM_CLASS(klass),
									G_SIGNAL_RUN_LAST,
									0,
									NULL,
									NULL,
									g_cclosure_marshal_VOID__STRING,
									G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[ENTRY_ACTIVATE] = g_signal_new("entry-activate",
									G_TYPE_FROM_CLASS(klass),
									G_SIGNAL_RUN_LAST,
									0,
									NULL,
									NULL,
									g_cclosure_marshal_VOID__STRING,
									G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[ENTRY_ACTIVATE_BACKWARD] = g_signal_new("entry-activate-backward",
									G_TYPE_FROM_CLASS(klass),
									G_SIGNAL_RUN_LAST,
									0,
									NULL,
									NULL,
									g_cclosure_marshal_VOID__STRING,
									G_TYPE_NONE, 1, G_TYPE_STRING);
}


static void geany_entry_action_init(GeanyEntryAction *action)
{
	GeanyEntryActionPrivate *priv;

	action->priv = G_TYPE_INSTANCE_GET_PRIVATE(action,
		GEANY_ENTRY_ACTION_TYPE, GeanyEntryActionPrivate);

	priv = action->priv;
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
