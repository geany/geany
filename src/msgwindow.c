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
#include "ui_utils.h"
#include "utils.h"
#include "document.h"
#include "build.h"

#include <string.h>
#include <stdlib.h>



// used for parse_file_line
typedef struct
{
	const gchar *string;	// line data
	const gchar *dir;		// working directory when string was generated
	const gchar *pattern;	// pattern to split the error message into some fields
	guint min_fields;		// used to detect errors after parsing
	guint line_idx;			// idx of the field where the line is
	gint file_idx;			// idx of the field where the filename is or -1
} ParseData;

static const GdkColor dark = {0, 58832, 58832, 58832};
static const GdkColor white = {0, 65535, 65535, 65535};

MessageWindow msgwindow;


static void msgwin_parse_grep_line(const gchar *string, gchar **filename, gint *line);
static gboolean on_msgwin_button_press_event(GtkWidget *widget, GdkEventButton *event,
																			gpointer user_data);


void msgwin_init()
{
	msgwindow.notebook = lookup_widget(app->window, "notebook_info");
	msgwindow.tree_status = lookup_widget(app->window, "treeview3");
	msgwindow.tree_msg = lookup_widget(app->window, "treeview4");
	msgwindow.tree_compiler = lookup_widget(app->window, "treeview5");
	msgwindow.find_in_files_dir = NULL;

	gtk_widget_set_sensitive(lookup_widget(app->window, "next_message1"), FALSE);
}


void msgwin_finalize()
{
	g_free(msgwindow.find_in_files_dir);
}


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
				G_CALLBACK(on_msgwin_button_press_event), GINT_TO_POINTER(MSG_STATUS));

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
	// use button-release-event so the selection has changed (connect_after button-press-event doesn't work)
	g_signal_connect(G_OBJECT(msgwindow.tree_msg), "button-release-event",
					G_CALLBACK(on_msgwin_button_press_event), GINT_TO_POINTER(MSG_MESSAGE));

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
	// use button-release-event so the selection has changed (connect_after button-press-event doesn't work)
	g_signal_connect(G_OBJECT(msgwindow.tree_compiler), "button-release-event",
					G_CALLBACK(on_msgwin_button_press_event), GINT_TO_POINTER(MSG_COMPILER));

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_compiler));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	//g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(on_msg_tree_selection_changed), NULL);
}


static const GdkColor color_error = {0, 65535, 0, 0};

// adds string to the compiler textview
void msgwin_compiler_add(gint msg_color, const gchar *format, ...)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	const GdkColor *color;
	const GdkColor dark_red = {0, 65535 / 2, 0, 0};
	const GdkColor blue = {0, 0, 0, 0xD000};	// not too bright ;-)
	const GdkColor black = {0, 0, 0, 0};
	gchar string[512];
	va_list args;

	va_start(args, format);
	g_vsnprintf(string, 512, format, args);
	va_end(args);

	switch (msg_color)
	{
		case COLOR_RED: color = &color_error; break;
		case COLOR_DARK_RED: color = &dark_red; break;
		case COLOR_BLUE: color = &blue; break;
		default: color = &black;
	}

	gtk_list_store_append(msgwindow.store_compiler, &iter);
	gtk_list_store_set(msgwindow.store_compiler, &iter, 0, color, 1, string, -1);

	if (app->msgwindow_visible)
	{
		path = gtk_tree_model_get_path(
			gtk_tree_view_get_model(GTK_TREE_VIEW(msgwindow.tree_compiler)), &iter);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(msgwindow.tree_compiler), path, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);
	}

	// calling build_menu_update for every build message would be overkill
	gtk_widget_set_sensitive(build_get_menu_items(-1)->item_next_error, TRUE);
}


void msgwin_show()
{
	app->msgwindow_visible = TRUE;
	app->ignore_callback = TRUE;
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_messages_window1")), TRUE);
	app->ignore_callback = FALSE;
	gtk_widget_show(lookup_widget(app->window, "scrolledwindow1"));
}


// adds string to the msg treeview
void msgwin_msg_add(gint line, gint idx, const gchar *string)
{
	GtkTreeIter iter;
	static gint state = 0;

	if (! app->msgwindow_visible) msgwin_show();

	gtk_list_store_append(msgwindow.store_msg, &iter);
	gtk_list_store_set(msgwindow.store_msg, &iter, 0, line, 1, idx, 2,
		((state++ % 2) == 0) ? &white : &dark, 3, string, -1);

	gtk_widget_set_sensitive(lookup_widget(app->window, "next_message1"), TRUE);
}


// logs a status message (use ui_set_statusbar() to just display text on the statusbar)
void msgwin_status_add(const gchar *format, ...)
{
	GtkTreeIter iter;
	static gint state = 0;
	gchar string[512];
	gchar *statusmsg, *time_str;
	va_list args;

	va_start(args, format);
	g_vsnprintf(string, 512, format, args);
	va_end(args);

	// display status message in status bar
	ui_set_statusbar("%s", string);

	// add a timestamp to status messages
	time_str = utils_get_current_time_string();
	if (time_str == NULL)
		statusmsg = g_strdup(string);
	else
		statusmsg = g_strconcat(time_str, ": ", string, NULL);
	g_free(time_str);

	// add message to Status window
	gtk_list_store_append(msgwindow.store_status, &iter);
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


static void
on_message_treeview_clear_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkListStore *store;

	switch (GPOINTER_TO_INT(user_data))
	{
		case MSG_MESSAGE:
		gtk_widget_set_sensitive(lookup_widget(app->window, "next_message1"), FALSE);
		store = msgwindow.store_msg;
		break;

		case MSG_COMPILER:
		gtk_widget_set_sensitive(build_get_menu_items(-1)->item_next_error, FALSE);
		store = msgwindow.store_compiler;
		break;

		default: // MSG_STATUS
		store = msgwindow.store_status;
	}
	gtk_list_store_clear(store);
}


static void
on_compiler_treeview_copy_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *tv = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint str_idx = 1;

	switch (GPOINTER_TO_INT(user_data))
	{
		case MSG_STATUS:
		tv = msgwindow.tree_status;
		break;

		case MSG_COMPILER:
		tv = msgwindow.tree_compiler;
		break;

		case MSG_MESSAGE:
		tv = msgwindow.tree_msg;
		str_idx = 3;
		break;
	}
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gchar *string;

		gtk_tree_model_get(model, &iter, str_idx, &string, -1);
		if (string && *string)
		{
			gtk_clipboard_set_text(gtk_clipboard_get(gdk_atom_intern("CLIPBOARD", FALSE)),
				string, -1);
		}
		g_free(string);
	}
}


GtkWidget *msgwin_create_message_popup_menu(gint type)
{
	GtkWidget *message_popup_menu, *clear, *copy;

	message_popup_menu = gtk_menu_new();

	clear = gtk_image_menu_item_new_from_stock("gtk-clear", NULL);
	gtk_widget_show(clear);
	gtk_container_add(GTK_CONTAINER(message_popup_menu), clear);

	copy = gtk_image_menu_item_new_from_stock("gtk-copy", NULL);
	gtk_widget_show(copy);
	gtk_container_add(GTK_CONTAINER(message_popup_menu), copy);

	g_signal_connect((gpointer)copy, "activate",
		G_CALLBACK(on_compiler_treeview_copy_activate), GINT_TO_POINTER(type));
	g_signal_connect((gpointer)clear, "activate",
		G_CALLBACK(on_message_treeview_clear_activate), GINT_TO_POINTER(type));

	return message_popup_menu;
}


gboolean msgwin_goto_compiler_file_line()
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	gchar *string;
	gboolean ret = FALSE;
	GdkColor *color;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_compiler));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		// if the item is not coloured red, it's not an error line
		gtk_tree_model_get(model, &iter, 0, &color, -1);
		if (! gdk_color_equal(color, &color_error))
		{
			gdk_color_free(color);
			return FALSE;
		}
		gdk_color_free(color);

		gtk_tree_model_get(model, &iter, 1, &string, -1);
		if (string != NULL)
		{
			gint line;
			gint idx;
			gchar *filename;
			msgwin_parse_compiler_error_line(string, &filename, &line);
			if (filename != NULL && line > -1)
			{
				gchar *utf8_filename = utils_get_utf8_from_locale(filename);
				idx = document_find_by_filename(utf8_filename, FALSE);
				g_free(utf8_filename);

				if (idx < 0)	// file not already open
					idx = document_open_file(-1, filename, 0, FALSE, NULL, NULL);

				if (idx >= 0 && doc_list[idx].is_valid)
				{
					if (! doc_list[idx].changed)	// if modified, line may be wrong
						document_set_indicator(idx, line - 1);
					ret = utils_goto_line(idx, line);
				}
			}
			g_free(filename);
		}
		g_free(string);
	}
	return ret;
}


/* try to parse the file and line number where the error occured described in line
 * and when something useful is found, it stores the line number in *line and the
 * relevant file with the error in *filename.
 * *line will be -1 if no error was found in string.
 * *filename must be freed unless it is NULL. */
static void parse_file_line(ParseData *data, gchar **filename, gint *line)
{
	gchar *end = NULL;
	gchar **fields;
	guint skip_dot_slash = 0;	// number of characters to skip at the beginning of the filename

	*filename = NULL;
	*line = -1;

	g_return_if_fail(data->dir != NULL && data->string != NULL);

	fields = g_strsplit_set(data->string, data->pattern, data->min_fields);

	// parse the line
	if (g_strv_length(fields) < data->min_fields)
	{
		g_strfreev(fields);
		return;
	}

	*line = strtol(fields[data->line_idx], &end, 10);

	// if the line could not be read, line is 0 and an error occurred, so we leave
	if (fields[data->line_idx] == end)
	{
		g_strfreev(fields);
		return;
	}

	// let's stop here if there is no filename in the error message
	if (data->file_idx == -1)
	{
		// we have no filename in the error message, so take the current one and hope it's correct
		document *doc = document_get_current();
		if (doc != NULL)
			*filename = g_strdup(doc->file_name);
		g_strfreev(fields);
		return;
	}

	// skip some characters at the beginning of the filename, at the moment only "./"
	// can be extended if other "trash" is known
	if (strncmp(fields[data->file_idx], "./", 2) == 0) skip_dot_slash = 2;

	// get the build directory to get the path to look for other files
	if (! utils_is_absolute_path(fields[data->file_idx]))
		*filename = g_strconcat(data->dir, G_DIR_SEPARATOR_S,
			fields[data->file_idx] + skip_dot_slash, NULL);
	else
		*filename = g_strdup(fields[data->file_idx]);

	g_strfreev(fields);
}


/* try to parse the file and line number where the error occured described in string
 * and when something useful is found, it stores the line number in *line and the
 * relevant file with the error in *filename.
 * *line will be -1 if no error was found in string.
 * *filename must be freed unless it is NULL. */
void msgwin_parse_compiler_error_line(const gchar *string, gchar **filename, gint *line)
{
	ParseData data = {string, build_info.dir, NULL, 0, 0, 0};

	*filename = NULL;
	*line = -1;

	g_return_if_fail(build_info.dir != NULL);
	if (string == NULL) return;

	switch (build_info.file_type_id)
	{
		case GEANY_FILETYPES_ALL:
		{
			return;
		}
		case GEANY_FILETYPES_PHP:
		{
			// Parse error: parse error, unexpected T_CASE in brace_bug.php on line 3
			// Parse error: syntax error, unexpected T_LNUMBER, expecting T_FUNCTION in bob.php on line 16
			gchar *tmp = strstr(string, " in ");

			if(tmp != NULL)
			{
				data.string = tmp;
				data.pattern = " ";
				data.min_fields = 6;
				data.line_idx = 5;
				data.file_idx = 2;
			}
			else
			{
				data.pattern = " ";
				data.min_fields = 11;
				data.line_idx = 10;
				data.file_idx = 7;
			}
			break;
		}
		case GEANY_FILETYPES_PERL:
		{
			// syntax error at test.pl line 7, near "{
			data.pattern = " ";
			data.min_fields = 6;
			data.line_idx = 5;
			data.file_idx = 3;
			break;
		}
		// the error output of python and tcl equals
		case GEANY_FILETYPES_TCL:
		case GEANY_FILETYPES_PYTHON:
		{
			// File "HyperArch.py", line 37, in ?
			// (file "clrdial.tcl" line 12)
			data.pattern = " \"";
			data.min_fields = 6;
			data.line_idx = 5;
			data.file_idx = 2;
			break;
		}
		case GEANY_FILETYPES_PASCAL:
		{
			// bandit.pas(149,3) Fatal: Syntax error, ";" expected but "ELSE" found
			data.pattern = "(";
			data.min_fields = 2;
			data.line_idx = 1;
			data.file_idx = 0;
			break;
		}
		case GEANY_FILETYPES_D:
		{
			// GNU D compiler front-end, gdc
			// gantry.d:18: variable gantry.main.c reference to auto class must be auto
			// warning - gantry.d:20: statement is not reachable
			// Digital Mars dmd compiler
			// warning - pi.d(118): implicit conversion of expression (digit) of type int ...
			// gantry.d(18): variable gantry.main.c reference to auto class must be auto
			if (strncmp(string, "warning - ", 10) == 0)
			{
				data.pattern = " (:";
				data.min_fields = 4;
				data.line_idx = 3;
				data.file_idx = 2;
			}
			else
			{
				data.pattern = "(:";
				data.min_fields = 2;
				data.line_idx = 1;
				data.file_idx = 0;
			}
			break;
		}
		case GEANY_FILETYPES_FERITE:
		{
			// Error: Parse Error: on line 5 in "/tmp/hello.fe"
			// Error: Compile Error: on line 24, in /test/class.fe
			if (strncmp(string, "Error: Compile Error", 20) == 0)
			{
				data.pattern = " ";
				data.min_fields = 8;
				data.line_idx = 5;
				data.file_idx = 7;
			}
			else
			{
				data.pattern = " \"";
				data.min_fields = 10;
				data.line_idx = 5;
				data.file_idx = 8;
			}
			break;
		}
		case GEANY_FILETYPES_HTML:
		{
			// line 78 column 7 - Warning: <table> missing '>' for end of tag
			data.pattern = " ";
			data.min_fields = 4;
			data.line_idx = 1;
			data.file_idx = -1;
			break;
		}
		/* All GNU gcc-like error messages */
		case GEANY_FILETYPES_C:
		case GEANY_FILETYPES_CPP:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_JAVA:
			// only gcc is supported, I don't know any other C(++) compilers and their error messages
			// empty.h:4: Warnung: type defaults to `int' in declaration of `foo'
			// empty.c:21: error: conflicting types for `foo'
			// Only parse file and line, so that linker errors will also work (with -g)
		case GEANY_FILETYPES_FORTRAN:
		case GEANY_FILETYPES_LATEX:
			// ./kommtechnik_2b.tex:18: Emergency stop.
		case GEANY_FILETYPES_MAKE:	// Assume makefile is building with gcc
		default:	// The default is a GNU gcc type error
		{
			// don't accidently find libtool versions x:y:x and think it is a file name
			if (strstr(string, "libtool --mode=link") == NULL)
			{
				data.pattern = ":";
				data.min_fields = 3;
				data.line_idx = 1;
				data.file_idx = 0;
				break;
			}
		}
	}

	if (data.pattern != NULL)
		parse_file_line(&data, filename, line);
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
		{
			ret = utils_goto_line(idx, line);	// checks valid idx
		}
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


/* Try to parse the file and line number for string and when something useful is
 * found, store the line number in *line and the relevant file with the error in
 * *filename.
 * *line will be -1 if no error was found in string.
 * *filename must be freed unless NULL. */
static void msgwin_parse_grep_line(const gchar *string, gchar **filename, gint *line)
{
	ParseData data;

	*filename = NULL;
	*line = -1;

	g_return_if_fail(msgwindow.find_in_files_dir != NULL);
	if (string == NULL) return;

	// conflict:3:conflicting types for `foo'
	data.string = string;
	data.dir = msgwindow.find_in_files_dir;
	data.pattern = ":";
	data.min_fields = 3;
	data.line_idx = 1;
	data.file_idx = 0;

	parse_file_line(&data, filename, line);
}


static gboolean on_msgwin_button_press_event(GtkWidget *widget, GdkEventButton *event,
																			gpointer user_data)
{
	// user_data might be NULL, GPOINTER_TO_INT returns 0 if called with NULL

	if (event->button == 1)
	{
		switch (GPOINTER_TO_INT(user_data))
		{
			case MSG_COMPILER:
			{	// single click in the compiler treeview
				msgwin_goto_compiler_file_line();
				break;
			}
			case MSG_MESSAGE:
			{	// single click in the message treeview (results of 'Find usage')
				msgwin_goto_messages_file_line();
				break;
			}
		}
	}

	if (event->button == 3)
	{	// popupmenu to hide or clear the active treeview
		switch (GPOINTER_TO_INT(user_data))
		{
			case MSG_STATUS:
			{
				gtk_menu_popup(GTK_MENU(msgwindow.popup_status_menu), NULL, NULL, NULL, NULL,
																	event->button, event->time);
				break;
			}
			case MSG_MESSAGE:
			{
				gtk_menu_popup(GTK_MENU(msgwindow.popup_msg_menu), NULL, NULL, NULL, NULL,
																	event->button, event->time);
				break;
			}
			case MSG_COMPILER:
			{
				gtk_menu_popup(GTK_MENU(msgwindow.popup_compiler_menu), NULL, NULL, NULL, NULL,
																	event->button, event->time);
				break;
			}
		}
	}
	return FALSE;
}


