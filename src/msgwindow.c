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


#include <time.h>

#include "geany.h"

#include "support.h"
#include "callbacks.h"
#include "msgwindow.h"
#include "utils.h"
#include "search.h"
#include "document.h"

#include <string.h>
#include <stdlib.h>


static GdkColor dark = {0, 58832, 58832, 58832};
static GdkColor white = {0, 65535, 65535, 65535};

static void msgwin_parse_grep_line(const gchar *string, gchar **filename, gint *line);


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

	msgwindow.store_msg = gtk_list_store_new(4, G_TYPE_INT, G_TYPE_INT, GDK_TYPE_COLOR, G_TYPE_STRING);
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
void msgwin_msg_add(gint line, gint idx, gchar *string)
{
	GtkTreeIter iter;
	static gint state = 0;

	if (! app->msgwindow_visible) return;

	gtk_list_store_append(msgwindow.store_msg, &iter);
	gtk_list_store_set(msgwindow.store_msg, &iter, 0, line, 1, idx, 2, ((state++ % 2) == 0) ? &white : &dark, 3, string, -1);
}


// adds a status message
void msgwin_status_add(gchar const *format, ...)
{
	GtkTreeIter iter;
	static gint state = 0;
	static gchar string[512];
	gchar *statusmsg, *time_str;
	va_list args;

	//if (! app->msgwindow_visible) return;

	va_start(args, format);
	g_vsnprintf(string, 511, format, args);
	va_end(args);

	// display status message in status bar
	utils_set_statusbar(string, FALSE);

	gtk_list_store_append(msgwindow.store_status, &iter);
	//gtk_list_store_insert(msgwindow.store_status, &iter, 0);
	//gtk_list_store_set(msgwindow.store_status, &iter, 0, (state > 0) ? &white : &dark, 1, string, -1);

	// add a timestamp to status messages
	time_str = utils_get_current_time_string();
	if (time_str == NULL)
		statusmsg = g_strdup(string);
	else
		statusmsg = g_strconcat(time_str, ": ", string, NULL);
	g_free(time_str);

	gtk_list_store_set(msgwindow.store_status, &iter, 0,
		((state++ % 2) == 0) ? &white : &dark, 1, statusmsg, -1);
	g_free(statusmsg);

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


gboolean msgwin_goto_compiler_file_line()
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gchar *string;
	gboolean ret = FALSE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_compiler));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 1, &string, -1);
		if (string != NULL)
		{
			gint line;
			gint idx;
			gchar *filename;
			utils_parse_compiler_error_line(string, &filename, &line);
			if (filename != NULL && line > -1)
			{
				// use document_open_file to find an already open file, or open it in place
				idx = document_open_file(-1, filename, 0, FALSE, NULL, NULL);
				// document_set_indicator will check valid idx
				document_set_indicator(idx, line - 1);
				// utils_goto_file_line will check valid filename.
				ret = utils_goto_file_line(filename, FALSE, line);
			}
			g_free(filename);
		}
		g_free(string);
	}
	return ret;
}


gboolean msgwin_goto_messages_file_line()
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gboolean ret = FALSE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_msg));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gint idx, line;
		gchar *string;

		gtk_tree_model_get(model, &iter, 0, &line, 1, &idx, 3, &string, -1);
		if (line >= 0 && idx >= 0)
			utils_goto_line(idx, line); //checks valid idx
		else if (line < 0 && string != NULL)
		{
			gchar *filename;
			msgwin_parse_grep_line(string, &filename, &line);
			if (filename != NULL && line > -1)
			{
				// use document_open_file to find an already open file, or open it in place
				idx = document_open_file(-1, filename, 0, FALSE, NULL, NULL);
				// utils_goto_file_line will check valid filename.
				ret = utils_goto_file_line(filename, FALSE, line);
			}
			g_free(filename);
		}
		g_free(string);
	}
	return ret;
}


// Taken from utils_parse_compiler_error_line, could refactor both (keep get_cur_idx).
/* Try to parse the file and line number for string and when something useful is
 * found, store the line number in *line and the relevant file with the error in
 * *filename.
 * *line will be -1 if no error was found in string.
 * *filename must be freed unless NULL. */
static void msgwin_parse_grep_line(const gchar *string, gchar **filename, gint *line)
{
	gchar *end = NULL;
	gchar *path;
	gchar **fields;
	gchar *pattern;				// pattern to split the error message into some fields
	guint field_min_len;		// used to detect errors after parsing
	guint field_idx_line;		// idx of the field where the line is
	guint field_idx_file;		// idx of the field where the filename is
	guint skip_dot_slash = 0;	// number of characters to skip at the beginning of the filename
	gint cur_idx;

	*filename = NULL;
	*line = -1;

	if (string == NULL) return;

	// conflict:3:conflicting types for `foo'
	pattern = ":";
	field_min_len = 3;
	field_idx_line = 1;
	field_idx_file = 0;

	fields = g_strsplit_set(string, pattern, field_min_len);

	// parse the line
	if (g_strv_length(fields) < field_min_len)
	{
		g_strfreev(fields);
		return;
	}

	*line = strtol(fields[field_idx_line], &end, 10);

	// if the line could not be read, line is 0 and an error occurred, so we leave
	if (fields[field_idx_line] == end)
	{
		g_strfreev(fields);
		return;
	}

	// skip some characters at the beginning of the filename, at the moment only "./"
	// can be extended if other "trash" is known
	if (strncmp(fields[field_idx_file], "./", 2) == 0) skip_dot_slash = 2;

	// get the basename of the built file to get the path to look for other files
	cur_idx = document_get_cur_idx();
	if (cur_idx >= 0 && doc_list[cur_idx].is_valid)
	{
		path = g_path_get_dirname(doc_list[cur_idx].file_name);
		*filename = g_strconcat(path, G_DIR_SEPARATOR_S,
			fields[field_idx_file] + skip_dot_slash, NULL);
		g_free(path);
	}

	g_strfreev(fields);
}


