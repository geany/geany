/*
 *      msgwindow.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#include "geany.h"

#include "support.h"
#include "callbacks.h"
#include "msgwindow.h"

static GdkColor dark = {0, 58832, 58832, 58832};
static GdkColor white = {0, 65535, 65535, 65535};


/* does some preparing things to the status message list widget */
void msgwin_prepare_status_tree_view(void)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	msgwindow.store_status = gtk_list_store_new(2, GDK_TYPE_COLOR, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(msgwindow.tree_status), GTK_TREE_MODEL(msgwindow.store_status));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Status messages"), renderer, "background-gdk", 0, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(msgwindow.tree_status), column);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(msgwindow.tree_status), FALSE);

	gtk_widget_modify_font(msgwindow.tree_status, pango_font_description_from_string(app->msgwin_font));
	g_signal_connect(G_OBJECT(msgwindow.tree_status), "button-press-event",
				G_CALLBACK(on_tree_view_button_press_event), GINT_TO_POINTER(3));

}


/* does some preparing things to the message list widget
 * (currently used for showing results of 'Find usage') */
void msgwin_prepare_msg_tree_view(void)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	msgwindow.store_msg = gtk_list_store_new(4, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_COLOR, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(msgwindow.tree_msg), GTK_TREE_MODEL(msgwindow.store_msg));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer, "background-gdk", 2, "text", 3, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(msgwindow.tree_msg), column);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(msgwindow.tree_msg), FALSE);

	gtk_widget_modify_font(msgwindow.tree_msg, pango_font_description_from_string(app->msgwin_font));
	g_signal_connect(G_OBJECT(msgwindow.tree_msg), "button-press-event",
					G_CALLBACK(on_tree_view_button_press_event), GINT_TO_POINTER(4));

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_msg));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	//g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(on_msg_tree_selection_changed), NULL);
}


/* does some preparing things to the compiler list widget */
void msgwin_prepare_compiler_tree_view(void)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	msgwindow.store_compiler = gtk_list_store_new(2, GDK_TYPE_COLOR, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(msgwindow.tree_compiler), GTK_TREE_MODEL(msgwindow.store_compiler));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(NULL, renderer, "foreground-gdk", 0, "text", 1, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(msgwindow.tree_compiler), column);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(msgwindow.tree_compiler), FALSE);

	gtk_widget_modify_font(msgwindow.tree_compiler, pango_font_description_from_string(app->msgwin_font));
	g_signal_connect(G_OBJECT(msgwindow.tree_compiler), "button-press-event",
					G_CALLBACK(on_tree_view_button_press_event), GINT_TO_POINTER(5));

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_compiler));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	//g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(on_msg_tree_selection_changed), NULL);
}


// adds string to the compiler textview
void msgwin_compiler_add(gint msg_color, gboolean scroll, gchar const *format, ...)
{
	GtkTreeIter iter;
	GdkColor *color;
	GtkTreePath *path;
	static GdkColor red = {0, 65535, 0, 0};
	static GdkColor blue = {0, 0, 0, 65535};
	static GdkColor black = {0, 0, 0, 0};
	static gchar string[512];
	va_list args;

	if (! app->msgwindow_visible) return;

	va_start(args, format);
	g_vsnprintf(string, 511, format, args);
	va_end(args);

	switch (msg_color)
	{
		case COLOR_RED: color = &red; break;
		case COLOR_BLUE: color = &blue; break;
		default: color = &black;
	}

	gtk_list_store_append(msgwindow.store_compiler, &iter);
	gtk_list_store_set(msgwindow.store_compiler, &iter, 0, color, 1, string, -1);

	path = gtk_tree_model_get_path(gtk_tree_view_get_model(GTK_TREE_VIEW(msgwindow.tree_compiler)), &iter);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(msgwindow.tree_compiler), path, NULL, TRUE, 0.5, 0.5);

	if (scroll)
	{
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(msgwindow.tree_compiler), path, NULL, FALSE);
	}
	gtk_tree_path_free(path);
}


// adds string to the msg treeview
void msgwin_msg_add(gint line, gchar *file, gchar *string)
{
	GtkTreeIter iter;
	static gint state = 0;

	if (! app->msgwindow_visible) return;

	gtk_list_store_append(msgwindow.store_msg, &iter);
	gtk_list_store_set(msgwindow.store_msg, &iter, 0, line, 1, file, 2, ((state++ % 2) == 0) ? &white : &dark, 3, string, -1);
}


// adds a status message
void msgwin_status_add(gchar const *format, ...)
{
	GtkTreeIter iter;
	static gint state = 0;
	static gchar string[512];
	va_list args;

	//if (! app->msgwindow_visible) return;

	va_start(args, format);
	g_vsnprintf(string, 511, format, args);
	va_end(args);

	gtk_list_store_append(msgwindow.store_status, &iter);
	//gtk_list_store_insert(msgwindow.store_status, &iter, 0);
	//gtk_list_store_set(msgwindow.store_status, &iter, 0, (state > 0) ? &white : &dark, 1, string, -1);
	gtk_list_store_set(msgwindow.store_status, &iter, 0, ((state++ % 2) == 0) ? &white : &dark, 1, string, -1);

	if (app->main_window_realized)
	{
		GtkTreePath *path = gtk_tree_model_get_path(gtk_tree_view_get_model(GTK_TREE_VIEW(msgwindow.tree_status)), &iter);

		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(msgwindow.tree_status), path, NULL, FALSE, 0.0, 0.0);
		if (app->switch_msgwin_pages) gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_STATUS);
		gtk_tree_path_free(path);
	}
}


GtkWidget *msgwin_create_message_popup_menu(gint type)
{
	GtkWidget *message_popup_menu, *clear;

	message_popup_menu = gtk_menu_new();

	clear = gtk_image_menu_item_new_from_stock("gtk-clear", NULL);
	gtk_widget_show(clear);
	gtk_container_add(GTK_CONTAINER(message_popup_menu), clear);

	if (type == 3)
		g_signal_connect((gpointer)clear, "activate", G_CALLBACK(on_message_treeview_clear_activate), msgwindow.store_status);
	else if (type == 4)
		g_signal_connect((gpointer)clear, "activate", G_CALLBACK(on_message_treeview_clear_activate), msgwindow.store_msg);
	else if (type == 5)
	{
		GtkWidget *copy = gtk_image_menu_item_new_from_stock("gtk-copy", NULL);
		gtk_widget_show(copy);
		gtk_container_add(GTK_CONTAINER(message_popup_menu), copy);

		g_signal_connect((gpointer)copy, "activate", G_CALLBACK(on_compiler_treeview_copy_activate), NULL);
		g_signal_connect((gpointer)clear, "activate", G_CALLBACK(on_message_treeview_clear_activate), msgwindow.store_compiler);
	}

	return message_popup_menu;
}

