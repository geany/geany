/*
 *      ui_utils.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2011-2012 Matthew Brush <mbrush(at)codebrainz(dot)ca>
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

/** @file ui_utils.h
 * User Interface general utility functions.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "ui_utils.h"

#include "app.h"
#include "callbacks.h"
#include "dialogs.h"
#include "documentprivate.h"
#include "encodingsprivate.h"
#include "filetypes.h"
#include "geanymenubuttonaction.h"
#include "keyfile.h"
#include "main.h"
#include "msgwindow.h"
#include "prefs.h"
#include "project.h"
#include "sciwrappers.h"
#include "sidebar.h"
#include "stash.h"
#include "support.h"
#include "symbols.h"
#include "toolbar.h"
#include "utils.h"
#include "win32.h"
#include "osx.h"

#include "gtkcompat.h"

#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <gdk/gdkkeysyms.h>


#define DEFAULT_STATUSBAR_TEMPLATE N_(\
	"line: %l / %L\t "   \
	"col: %c\t "         \
	"sel: %s\t "         \
	"%w      %t      %m" \
	"mode: %M      "     \
	"encoding: %e      " \
	"filetype: %f      " \
	"scope: %S")

GeanyInterfacePrefs	interface_prefs;
GeanyMainWidgets	main_widgets;

UIPrefs			ui_prefs;
UIWidgets		ui_widgets;

static GtkBuilder *builder = NULL;
static GtkWidget* window1 = NULL;
static GtkWidget* toolbar_popup_menu1 = NULL;
static GtkWidget* edit_menu1 = NULL;
static GtkWidget* prefs_dialog = NULL;
static GtkWidget* project_dialog = NULL;

static struct
{
	/* pointers to widgets only sensitive when there is at least one document, the pointers can
	 * also be GtkAction objects, so check each pointer before using it */
	GPtrArray	*document_buttons;
	GtkWidget	*menu_insert_include_items[2];
	GtkWidget	*popup_goto_items[4];
	GtkWidget	*popup_copy_items[3];
	GtkWidget	*menu_copy_items[3];
	GtkWidget	*redo_items[3];
	GtkWidget	*undo_items[3];
	GtkWidget	*save_buttons[4];
	GtkWidget	*config_files_menu;
}
widgets;

enum
{
	RECENT_FILE_FILE,
	RECENT_FILE_PROJECT
};

typedef struct
{
	gint type;
	GQueue *recent_queue;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	void (*activate_cb)(GtkMenuItem *, gpointer);
} GeanyRecentFiles;


static void update_recent_menu(GeanyRecentFiles *grf);
static void recent_file_loaded(const gchar *utf8_filename, GeanyRecentFiles *grf);
static void recent_file_activate_cb(GtkMenuItem *menuitem, gpointer user_data);
static void recent_project_activate_cb(GtkMenuItem *menuitem, gpointer user_data);
static GtkWidget *progress_bar_create(void);
static void ui_menu_sort_by_label(GtkMenu *menu);


/* simple wrapper for gtk_widget_set_sensitive() to allow widget being NULL */
void ui_widget_set_sensitive(GtkWidget *widget, gboolean set)
{
	if (widget != NULL)
		gtk_widget_set_sensitive(widget, set);
}


/* allow_override is TRUE if text can be ignored when another message has been set
 * that didn't use allow_override and has not timed out. */
static void set_statusbar(const gchar *text, gboolean allow_override)
{
	static guint id = 0;
	static glong last_time = 0;
	GTimeVal timeval;
	const gint GEANY_STATUS_TIMEOUT = 1;

	if (! interface_prefs.statusbar_visible)
		return; /* just do nothing if statusbar is not visible */

	if (id == 0)
		id = gtk_statusbar_get_context_id(GTK_STATUSBAR(ui_widgets.statusbar), "geany-main");

	g_get_current_time(&timeval);

	if (! allow_override)
	{
		gtk_statusbar_pop(GTK_STATUSBAR(ui_widgets.statusbar), id);
		gtk_statusbar_push(GTK_STATUSBAR(ui_widgets.statusbar), id, text);
		last_time = timeval.tv_sec;
	}
	else
	if (timeval.tv_sec > last_time + GEANY_STATUS_TIMEOUT)
	{
		gtk_statusbar_pop(GTK_STATUSBAR(ui_widgets.statusbar), id);
		gtk_statusbar_push(GTK_STATUSBAR(ui_widgets.statusbar), id, text);
	}
}


/** Displays text on the statusbar.
 * @param log Whether the message should be recorded in the Status window.
 * @param format A @c printf -style string. */
GEANY_API_SYMBOL
void ui_set_statusbar(gboolean log, const gchar *format, ...)
{
	gchar *string;
	va_list args;

	va_start(args, format);
	string = g_strdup_vprintf(format, args);
	va_end(args);

	if (! prefs.suppress_status_messages)
		set_statusbar(string, FALSE);

	if (log || prefs.suppress_status_messages)
		msgwin_status_add("%s", string);

	g_free(string);
}


/* note: some comments below are for translators */
static gchar *create_statusbar_statistics(GeanyDocument *doc,
	guint line, guint vcol, guint pos)
{
	const gchar *cur_tag;
	const gchar *fmt;
	const gchar *expos;	/* % expansion position */
	const gchar sp[] = "      ";
	GString *stats_str;
	ScintillaObject *sci = doc->editor->sci;

	if (!EMPTY(ui_prefs.statusbar_template))
		fmt = ui_prefs.statusbar_template;
	else
		fmt = _(DEFAULT_STATUSBAR_TEMPLATE);

	stats_str = g_string_sized_new(120);

	while ((expos = strchr(fmt, '%')) != NULL)
	{
		/* append leading text before % char */
		g_string_append_len(stats_str, fmt, expos - fmt);

		switch (*++expos)
		{
			case 'l':
				g_string_append_printf(stats_str, "%d", line + 1);
				break;
			case 'L':
				g_string_append_printf(stats_str, "%d",
					sci_get_line_count(doc->editor->sci));
				break;
			case 'c':
				g_string_append_printf(stats_str, "%d", vcol);
				break;
			case 'C':
				g_string_append_printf(stats_str, "%d", vcol + 1);
				break;
			case 'p':
				g_string_append_printf(stats_str, "%u", pos);
				break;
			case 's':
			{
				gint len = sci_get_selected_text_length(sci) - 1;
				/* check if whole lines are selected */
				if (!len || sci_get_col_from_position(sci,
						sci_get_selection_start(sci)) != 0 ||
					sci_get_col_from_position(sci,
						sci_get_selection_end(sci)) != 0)
					g_string_append_printf(stats_str, "%d", len);
				else /* L = lines */
					g_string_append_printf(stats_str, _("%dL"),
						sci_get_lines_selected(doc->editor->sci) - 1);
				break;
			}
			case 'n' :
				g_string_append_printf(stats_str, "%d",
					sci_get_selected_text_length(doc->editor->sci) - 1);
				break;
			case 'w':
				/* RO = read-only */
				g_string_append(stats_str, (doc->readonly) ? _("RO ") :
					/* OVR = overwrite/overtype, INS = insert */
					(sci_get_overtype(doc->editor->sci) ? _("OVR") : _("INS")));
				break;
			case 'r':
				if (doc->readonly)
				{
					g_string_append(stats_str, _("RO "));	/* RO = read-only */
					g_string_append(stats_str, sp + 1);
				}
				break;
			case 't':
			{
				switch (editor_get_indent_prefs(doc->editor)->type)
				{
					case GEANY_INDENT_TYPE_TABS:
						g_string_append(stats_str, _("TAB"));
						break;
					case GEANY_INDENT_TYPE_SPACES:	/* SP = space */
						g_string_append(stats_str, _("SP"));
						break;
					case GEANY_INDENT_TYPE_BOTH:	/* T/S = tabs and spaces */
						g_string_append(stats_str, _("T/S"));
						break;
				}
				break;
			}
			case 'm':
				if (doc->changed)
				{
					g_string_append(stats_str, _("MOD"));	/* MOD = modified */
					g_string_append(stats_str, sp);
				}
				break;
			case 'M':
				g_string_append(stats_str, utils_get_eol_short_name(sci_get_eol_mode(doc->editor->sci)));
				break;
			case 'e':
				g_string_append(stats_str,
					doc->encoding ? doc->encoding : _("unknown"));
				if (encodings_is_unicode_charset(doc->encoding) && (doc->has_bom))
				{
					g_string_append_c(stats_str, ' ');
					g_string_append(stats_str, _("(with BOM)"));	/* BOM = byte order mark */
				}
				break;
			case 'f':
				g_string_append(stats_str, filetypes_get_display_name(doc->file_type));
				break;
			case 'S':
				symbols_get_current_scope(doc, &cur_tag);
				g_string_append(stats_str, cur_tag);
				break;
			case 'Y':
				g_string_append_c(stats_str, ' ');
				g_string_append_printf(stats_str, "%d",
					sci_get_style_at(doc->editor->sci, pos));
				break;
			default:
				g_string_append_len(stats_str, expos, 1);
		}

		/* skip past %c chars */
		if (*expos)
			fmt = expos + 1;
		else
			break;
	}
	/* add any remaining text */
	g_string_append(stats_str, fmt);

	return g_string_free(stats_str, FALSE);
}


/* updates the status bar document statistics */
void ui_update_statusbar(GeanyDocument *doc, gint pos)
{
	g_return_if_fail(doc == NULL || doc->is_valid);

	if (! interface_prefs.statusbar_visible)
		return; /* just do nothing if statusbar is not visible */

	if (doc == NULL)
		doc = document_get_current();

	if (doc != NULL)
	{
		guint line, vcol;
		gchar *stats_str;

		if (pos == -1)
			pos = sci_get_current_position(doc->editor->sci);
		line = sci_get_line_from_position(doc->editor->sci, pos);

		/* Add temporary fix for sci infinite loop in Document::GetColumn(int)
		 * when current pos is beyond document end (can occur when removing
		 * blocks of selected lines especially esp. brace sections near end of file). */
		if (pos <= sci_get_length(doc->editor->sci))
			vcol = sci_get_col_from_position(doc->editor->sci, pos);
		else
			vcol = 0;
		vcol += sci_get_cursor_virtual_space(doc->editor->sci);

		stats_str = create_statusbar_statistics(doc, line, vcol, pos);

		/* can be overridden by status messages */
		set_statusbar(stats_str, TRUE);
		g_free(stats_str);
	}
	else	/* no documents */
	{
		set_statusbar("", TRUE);	/* can be overridden by status messages */
	}
}


/* This sets the window title according to the current filename. */
void ui_set_window_title(GeanyDocument *doc)
{
	GString *str;
	GeanyProject *project = app->project;

	g_return_if_fail(doc == NULL || doc->is_valid);

	if (doc == NULL)
		doc = document_get_current();

	str = g_string_new(NULL);

	if (doc != NULL)
	{
		g_string_append(str, doc->changed ? "*" : "");

		if (doc->file_name == NULL)
			g_string_append(str, DOC_FILENAME(doc));
		else
		{
			gchar *short_name = document_get_basename_for_display(doc, 30);
			gchar *dirname = g_path_get_dirname(DOC_FILENAME(doc));

			g_string_append(str, short_name);
			g_string_append(str, " - ");
			g_string_append(str, dirname ? dirname : "");
			g_free(short_name);
			g_free(dirname);
		}
		g_string_append(str, " - ");
	}
	if (project)
	{
		g_string_append_c(str, '[');
		g_string_append(str, project->name);
		g_string_append(str, "] - ");
	}
	g_string_append(str, "Geany");
	if (cl_options.new_instance)
	{
		g_string_append(str, _(" (new instance)"));
	}
	gtk_window_set_title(GTK_WINDOW(main_widgets.window), str->str);
	g_string_free(str, TRUE);
}


void ui_set_editor_font(const gchar *font_name)
{
	guint i;

	g_return_if_fail(font_name != NULL);

	/* do nothing if font has not changed */
	if (interface_prefs.editor_font != NULL)
		if (strcmp(font_name, interface_prefs.editor_font) == 0)
			return;

	g_free(interface_prefs.editor_font);
	interface_prefs.editor_font = g_strdup(font_name);

	/* We copy the current style, and update the font in all open tabs. */
	for (i = 0; i < documents_array->len; i++)
	{
		if (documents[i]->editor)
		{
			editor_set_font(documents[i]->editor, interface_prefs.editor_font);
		}
	}

	ui_set_statusbar(TRUE, _("Font updated (%s)."), interface_prefs.editor_font);
}


void ui_set_fullscreen(void)
{
	if (ui_prefs.fullscreen)
	{
		gtk_window_fullscreen(GTK_WINDOW(main_widgets.window));
	}
	else
	{
		gtk_window_unfullscreen(GTK_WINDOW(main_widgets.window));
	}
}


void ui_update_popup_reundo_items(GeanyDocument *doc)
{
	gboolean enable_undo;
	gboolean enable_redo;
	guint i, len;

	g_return_if_fail(doc == NULL || doc->is_valid);

	if (doc == NULL)
	{
		enable_undo = FALSE;
		enable_redo = FALSE;
	}
	else
	{
		enable_undo = document_can_undo(doc);
		enable_redo = document_can_redo(doc);
	}

	/* index 0 is the popup menu, 1 is the menubar, 2 is the toolbar */
	len = G_N_ELEMENTS(widgets.undo_items);
	for (i = 0; i < len; i++)
	{
		ui_widget_set_sensitive(widgets.undo_items[i], enable_undo);
	}
	len = G_N_ELEMENTS(widgets.redo_items);
	for (i = 0; i < len; i++)
	{
		ui_widget_set_sensitive(widgets.redo_items[i], enable_redo);
	}
}


void ui_update_popup_copy_items(GeanyDocument *doc)
{
	gboolean enable;
	guint i, len;

	g_return_if_fail(doc == NULL || doc->is_valid);

	if (doc == NULL)
		enable = FALSE;
	else
		enable = sci_has_selection(doc->editor->sci);

	len = G_N_ELEMENTS(widgets.popup_copy_items);
	for (i = 0; i < len; i++)
		ui_widget_set_sensitive(widgets.popup_copy_items[i], enable);
}


void ui_update_popup_goto_items(gboolean enable)
{
	guint i, len;
	len = G_N_ELEMENTS(widgets.popup_goto_items);
	for (i = 0; i < len; i++)
		ui_widget_set_sensitive(widgets.popup_goto_items[i], enable);
}


void ui_update_menu_copy_items(GeanyDocument *doc)
{
	gboolean enable = FALSE;
	guint i, len;
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	g_return_if_fail(doc == NULL || doc->is_valid);

	if (IS_SCINTILLA(focusw))
		enable = (doc == NULL) ? FALSE : sci_has_selection(doc->editor->sci);
	else
	if (GTK_IS_EDITABLE(focusw))
		enable = gtk_editable_get_selection_bounds(GTK_EDITABLE(focusw), NULL, NULL);
	else
	if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		enable = gtk_text_buffer_get_selection_bounds(buffer, NULL, NULL);
	}

	len = G_N_ELEMENTS(widgets.menu_copy_items);
	for (i = 0; i < len; i++)
		ui_widget_set_sensitive(widgets.menu_copy_items[i], enable);
}


void ui_update_insert_include_item(GeanyDocument *doc, gint item)
{
	gboolean enable = FALSE;

	g_return_if_fail(doc == NULL || doc->is_valid);

	if (doc == NULL || doc->file_type == NULL)
		enable = FALSE;
	else if (doc->file_type->id == GEANY_FILETYPES_C ||  doc->file_type->id == GEANY_FILETYPES_CPP)
		enable = TRUE;

	ui_widget_set_sensitive(widgets.menu_insert_include_items[item], enable);
}


void ui_update_fold_items(void)
{
	ui_widget_show_hide(ui_lookup_widget(main_widgets.window, "menu_fold_all1"), editor_prefs.folding);
	ui_widget_show_hide(ui_lookup_widget(main_widgets.window, "menu_unfold_all1"), editor_prefs.folding);
	ui_widget_show_hide(ui_lookup_widget(main_widgets.window, "separator22"), editor_prefs.folding);
}


/* @include include name or NULL for empty with cursor ready for typing it */
static void insert_include(GeanyDocument *doc, gint pos, const gchar *include)
{
	gint pos_after = -1;
	gchar *text;

	g_return_if_fail(doc != NULL);
	g_return_if_fail(pos == -1 || pos >= 0);

	if (pos == -1)
		pos = sci_get_current_position(doc->editor->sci);

	if (! include)
	{
		text = g_strdup("#include \"\"\n");
		pos_after = pos + 10;
	}
	else
	{
		text = g_strconcat("#include <", include, ">\n", NULL);
	}

	sci_start_undo_action(doc->editor->sci);
	sci_insert_text(doc->editor->sci, pos, text);
	sci_end_undo_action(doc->editor->sci);
	g_free(text);
	if (pos_after >= 0)
		sci_goto_pos(doc->editor->sci, pos_after, FALSE);
}


static void on_popup_insert_include_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_include(document_get_current(), editor_info.click_pos, user_data);
}


static void on_menu_insert_include_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_include(document_get_current(), -1, user_data);
}


static void insert_include_items(GtkMenu *me, GtkMenu *mp, gchar **includes, gchar *label)
{
	guint i = 0;
	GtkWidget *edit_menu, *edit_menu_item;
	GtkWidget *popup_menu, *popup_menu_item;

	edit_menu = gtk_menu_new();
	popup_menu = gtk_menu_new();
	edit_menu_item = gtk_menu_item_new_with_label(label);
	popup_menu_item = gtk_menu_item_new_with_label(label);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_menu_item), edit_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(popup_menu_item), popup_menu);

	while (includes[i] != NULL)
	{
		GtkWidget *tmp_menu = gtk_menu_item_new_with_label(includes[i]);
		GtkWidget *tmp_popup = gtk_menu_item_new_with_label(includes[i]);

		gtk_container_add(GTK_CONTAINER(edit_menu), tmp_menu);
		gtk_container_add(GTK_CONTAINER(popup_menu), tmp_popup);
		g_signal_connect(tmp_menu, "activate",
					G_CALLBACK(on_menu_insert_include_activate), (gpointer) includes[i]);
		g_signal_connect(tmp_popup, "activate",
					G_CALLBACK(on_popup_insert_include_activate), (gpointer) includes[i]);
		i++;
	}
	gtk_widget_show_all(edit_menu_item);
	gtk_widget_show_all(popup_menu_item);
	gtk_container_add(GTK_CONTAINER(me), edit_menu_item);
	gtk_container_add(GTK_CONTAINER(mp), popup_menu_item);
}


void ui_create_insert_menu_items(void)
{
	GtkMenu *menu_edit = GTK_MENU(ui_lookup_widget(main_widgets.window, "insert_include2_menu"));
	GtkMenu *menu_popup = GTK_MENU(ui_lookup_widget(main_widgets.editor_menu, "insert_include1_menu"));
	GtkWidget *blank;
	const gchar *c_includes_stdlib[] = {
		"assert.h", "ctype.h", "errno.h", "float.h", "limits.h", "locale.h", "math.h", "setjmp.h",
		"signal.h", "stdarg.h", "stddef.h", "stdio.h", "stdlib.h", "string.h", "time.h", NULL
	};
	const gchar *c_includes_c99[] = {
		"complex.h", "fenv.h", "inttypes.h", "iso646.h", "stdbool.h", "stdint.h",
		"tgmath.h", "wchar.h", "wctype.h", NULL
	};
	const gchar *c_includes_cpp[] = {
		"cstdio", "cstring", "cctype", "cmath", "ctime", "cstdlib", "cstdarg", NULL
	};
	const gchar *c_includes_cppstdlib[] = {
		"iostream", "fstream", "iomanip", "sstream", "exception", "stdexcept",
		"memory", "locale", NULL
	};
	const gchar *c_includes_stl[] = {
		"bitset", "deque", "list", "map", "set", "queue", "stack", "vector", "algorithm",
		"iterator", "functional", "string", "complex", "valarray", NULL
	};

	blank = gtk_menu_item_new_with_label("#include \"...\"");
	gtk_container_add(GTK_CONTAINER(menu_edit), blank);
	gtk_widget_show(blank);
	g_signal_connect(blank, "activate", G_CALLBACK(on_menu_insert_include_activate), NULL);
	blank = gtk_separator_menu_item_new ();
	gtk_container_add(GTK_CONTAINER(menu_edit), blank);
	gtk_widget_show(blank);

	blank = gtk_menu_item_new_with_label("#include \"...\"");
	gtk_container_add(GTK_CONTAINER(menu_popup), blank);
	gtk_widget_show(blank);
	g_signal_connect(blank, "activate", G_CALLBACK(on_popup_insert_include_activate), NULL);
	blank = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_popup), blank);
	gtk_widget_show(blank);

	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_stdlib, _("C Standard Library"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_c99, _("ISO C99"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_cpp, _("C++ (C Standard Library)"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_cppstdlib, _("C++ Standard Library"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_stl, _("C++ STL"));
}


static void insert_date(GeanyDocument *doc, gint pos, const gchar *date_style)
{
	const gchar *format = NULL;
	gchar *time_str;

	g_return_if_fail(doc != NULL);
	g_return_if_fail(pos == -1 || pos >= 0);

	if (pos == -1)
		pos = sci_get_current_position(doc->editor->sci);

	/* set default value */
	if (utils_str_equal("", ui_prefs.custom_date_format))
	{
		g_free(ui_prefs.custom_date_format);
		ui_prefs.custom_date_format = g_strdup("%d.%m.%Y");
	}

	if (utils_str_equal(_("dd.mm.yyyy"), date_style))
		format = "%d.%m.%Y";
	else if (utils_str_equal(_("mm.dd.yyyy"), date_style))
		format = "%m.%d.%Y";
	else if (utils_str_equal(_("yyyy/mm/dd"), date_style))
		format = "%Y/%m/%d";
	else if (utils_str_equal(_("dd.mm.yyyy hh:mm:ss"), date_style))
		format = "%d.%m.%Y %H:%M:%S";
	else if (utils_str_equal(_("mm.dd.yyyy hh:mm:ss"), date_style))
		format = "%m.%d.%Y %H:%M:%S";
	else if (utils_str_equal(_("yyyy/mm/dd hh:mm:ss"), date_style))
		format = "%Y/%m/%d %H:%M:%S";
	else if (utils_str_equal(_("_Use Custom Date Format"), date_style))
		format = ui_prefs.custom_date_format;
	else
	{
		gchar *str = dialogs_show_input(_("Custom Date Format"), GTK_WINDOW(main_widgets.window),
				_("Enter here a custom date and time format. "
				"You can use any conversion specifiers which can be used with the ANSI C strftime function."),
				ui_prefs.custom_date_format);
		if (str)
			SETPTR(ui_prefs.custom_date_format, str);
		return;
	}

	time_str = utils_get_date_time(format, NULL);
	if (time_str != NULL)
	{
		sci_start_undo_action(doc->editor->sci);
		sci_insert_text(doc->editor->sci, pos, time_str);
		sci_goto_pos(doc->editor->sci, pos + strlen(time_str), FALSE);
		sci_end_undo_action(doc->editor->sci);
		g_free(time_str);
	}
	else
	{
		utils_beep();
		ui_set_statusbar(TRUE,
				_("Date format string could not be converted (possibly too long)."));
	}
}


static void on_popup_insert_date_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_date(document_get_current(), editor_info.click_pos, user_data);
}


static void on_menu_insert_date_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_date(document_get_current(), -1, user_data);
}


static void insert_date_items(GtkMenu *me, GtkMenu *mp, gchar *label)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_mnemonic(label);
	gtk_container_add(GTK_CONTAINER(me), item);
	gtk_widget_show(item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_insert_date_activate), label);

	item = gtk_menu_item_new_with_mnemonic(label);
	gtk_container_add(GTK_CONTAINER(mp), item);
	gtk_widget_show(item);
	g_signal_connect(item, "activate", G_CALLBACK(on_popup_insert_date_activate), label);
}


void ui_create_insert_date_menu_items(void)
{
	GtkMenu *menu_edit = GTK_MENU(ui_lookup_widget(main_widgets.window, "insert_date1_menu"));
	GtkMenu *menu_popup = GTK_MENU(ui_lookup_widget(main_widgets.editor_menu, "insert_date2_menu"));
	GtkWidget *item;
	gchar *str;

	insert_date_items(menu_edit, menu_popup, _("dd.mm.yyyy"));
	insert_date_items(menu_edit, menu_popup, _("mm.dd.yyyy"));
	insert_date_items(menu_edit, menu_popup, _("yyyy/mm/dd"));

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_edit), item);
	gtk_widget_show(item);
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_popup), item);
	gtk_widget_show(item);

	insert_date_items(menu_edit, menu_popup, _("dd.mm.yyyy hh:mm:ss"));
	insert_date_items(menu_edit, menu_popup, _("mm.dd.yyyy hh:mm:ss"));
	insert_date_items(menu_edit, menu_popup, _("yyyy/mm/dd hh:mm:ss"));

	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_edit), item);
	gtk_widget_show(item);
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_popup), item);
	gtk_widget_show(item);

	str = _("_Use Custom Date Format");
	item = gtk_menu_item_new_with_mnemonic(str);
	gtk_container_add(GTK_CONTAINER(menu_edit), item);
	gtk_widget_show(item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_insert_date_activate), str);
	ui_hookup_widget(main_widgets.window, item, "insert_date_custom1");

	item = gtk_menu_item_new_with_mnemonic(str);
	gtk_container_add(GTK_CONTAINER(menu_popup), item);
	gtk_widget_show(item);
	g_signal_connect(item, "activate", G_CALLBACK(on_popup_insert_date_activate), str);
	ui_hookup_widget(main_widgets.editor_menu, item, "insert_date_custom2");

	insert_date_items(menu_edit, menu_popup, _("_Set Custom Date Format"));
}


void ui_save_buttons_toggle(gboolean enable)
{
	guint i;
	gboolean dirty_tabs = FALSE;

	if (ui_prefs.allow_always_save)
		enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) > 0;

	ui_widget_set_sensitive(widgets.save_buttons[0], enable);
	ui_widget_set_sensitive(widgets.save_buttons[1], enable);

	/* save all menu item and tool button */
	for (i = 0; i < documents_array->len; i++)
	{
		/* check whether there are files where changes were made and if there are some,
		 * we need the save all button / item */
		if (documents[i]->is_valid && documents[i]->changed)
		{
			dirty_tabs = TRUE;
			break;
		}
	}

	ui_widget_set_sensitive(widgets.save_buttons[2], dirty_tabs);
	ui_widget_set_sensitive(widgets.save_buttons[3], dirty_tabs);
}


#define add_doc_widget(widget_name) \
	g_ptr_array_add(widgets.document_buttons, ui_lookup_widget(main_widgets.window, widget_name))

#define add_doc_toolitem(widget_name) \
	g_ptr_array_add(widgets.document_buttons, toolbar_get_action_by_name(widget_name))

static void init_document_widgets(void)
{
	widgets.document_buttons = g_ptr_array_new();

	/* Cache the document-sensitive widgets so we don't have to keep looking them up
	 * when using ui_document_buttons_update(). */
	add_doc_widget("menu_close1");
	add_doc_widget("close_other_documents1");
	add_doc_widget("menu_change_font1");
	add_doc_widget("menu_close_all1");
	add_doc_widget("menu_save1");
	add_doc_widget("menu_save_all1");
	add_doc_widget("menu_save_as1");
	add_doc_widget("menu_count_words1");
	add_doc_widget("menu_build1");
	add_doc_widget("add_comments1");
	add_doc_widget("menu_paste1");
	add_doc_widget("menu_undo2");
	add_doc_widget("properties1");
	add_doc_widget("menu_reload1");
	add_doc_widget("menu_document1");
	add_doc_widget("menu_choose_color1");
	add_doc_widget("menu_color_schemes");
	add_doc_widget("menu_markers_margin1");
	add_doc_widget("menu_linenumber_margin1");
	add_doc_widget("menu_show_white_space1");
	add_doc_widget("menu_show_line_endings1");
	add_doc_widget("menu_show_indentation_guides1");
	add_doc_widget("menu_zoom_in1");
	add_doc_widget("menu_zoom_out1");
	add_doc_widget("normal_size1");
	add_doc_widget("treeview6");
	add_doc_widget("print1");
	add_doc_widget("menu_reload_as1");
	add_doc_widget("menu_select_all1");
	add_doc_widget("insert_date1");
	add_doc_widget("insert_alternative_white_space1");
	add_doc_widget("menu_format1");
	add_doc_widget("commands2");
	add_doc_widget("menu_open_selected_file1");
	add_doc_widget("page_setup1");
	add_doc_widget("find1");
	add_doc_widget("find_next1");
	add_doc_widget("find_previous1");
	add_doc_widget("go_to_next_marker1");
	add_doc_widget("go_to_previous_marker1");
	add_doc_widget("replace1");
	add_doc_widget("find_nextsel1");
	add_doc_widget("find_prevsel1");
	add_doc_widget("find_usage1");
	add_doc_widget("find_document_usage1");
	add_doc_widget("mark_all1");
	add_doc_widget("go_to_line1");
	add_doc_widget("goto_tag_definition1");
	add_doc_widget("goto_tag_declaration1");
	add_doc_widget("reset_indentation1");
	add_doc_toolitem("Close");
	add_doc_toolitem("CloseAll");
	add_doc_toolitem("Search");
	add_doc_toolitem("SearchEntry");
	add_doc_toolitem("ZoomIn");
	add_doc_toolitem("ZoomOut");
	add_doc_toolitem("Indent");
	add_doc_toolitem("UnIndent");
	add_doc_toolitem("Cut");
	add_doc_toolitem("Copy");
	add_doc_toolitem("Paste");
	add_doc_toolitem("Delete");
	add_doc_toolitem("Save");
	add_doc_toolitem("SaveAs");
	add_doc_toolitem("SaveAll");
	add_doc_toolitem("Compile");
	add_doc_toolitem("Run");
	add_doc_toolitem("Reload");
	add_doc_toolitem("Color");
	add_doc_toolitem("Goto");
	add_doc_toolitem("GotoEntry");
	add_doc_toolitem("Replace");
	add_doc_toolitem("Print");
}


void ui_document_buttons_update(void)
{
	guint i;
	gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) > 0;

	for (i = 0; i < widgets.document_buttons->len; i++)
	{
		GtkWidget *widget = g_ptr_array_index(widgets.document_buttons, i);
		if (GTK_IS_ACTION(widget))
			gtk_action_set_sensitive(GTK_ACTION(widget), enable);
		else
			ui_widget_set_sensitive(widget, enable);
	}
}


static void on_doc_sensitive_widget_destroy(GtkWidget *widget, G_GNUC_UNUSED gpointer user_data)
{
	g_ptr_array_remove_fast(widgets.document_buttons, widget);
}


/** Adds a widget to the list of widgets that should be set sensitive/insensitive
 * when some documents are present/no documents are open.
 * It will be removed when the widget is destroyed.
 * @param widget The widget to add.
 *
 * @since 0.15
 **/
GEANY_API_SYMBOL
void ui_add_document_sensitive(GtkWidget *widget)
{
	gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) > 0;

	ui_widget_set_sensitive(widget, enable);

	g_ptr_array_add(widgets.document_buttons, widget);
	g_signal_connect(widget, "destroy", G_CALLBACK(on_doc_sensitive_widget_destroy), NULL);
}


void ui_widget_show_hide(GtkWidget *widget, gboolean show)
{
	if (show)
	{
		gtk_widget_show(widget);
	}
	else
	{
		gtk_widget_hide(widget);
	}
}


void ui_sidebar_show_hide(void)
{
	GtkWidget *widget;

	/* check that there are no other notebook pages before hiding the sidebar completely
	 * other pages could be e.g. the file browser plugin */
	if (! interface_prefs.sidebar_openfiles_visible && ! interface_prefs.sidebar_symbol_visible &&
		gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.sidebar_notebook)) <= 2)
	{
		ui_prefs.sidebar_visible = FALSE;
	}

	widget = ui_lookup_widget(main_widgets.window, "menu_show_sidebar1");
	if (ui_prefs.sidebar_visible != gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	{
		ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), ui_prefs.sidebar_visible);
		ignore_callback = FALSE;
	}

	ui_widget_show_hide(main_widgets.sidebar_notebook, ui_prefs.sidebar_visible);

	ui_widget_show_hide(gtk_notebook_get_nth_page(
		GTK_NOTEBOOK(main_widgets.sidebar_notebook), 0), interface_prefs.sidebar_symbol_visible);
	ui_widget_show_hide(gtk_notebook_get_nth_page(
		GTK_NOTEBOOK(main_widgets.sidebar_notebook), 1), interface_prefs.sidebar_openfiles_visible);
}


void ui_document_show_hide(GeanyDocument *doc)
{
	const gchar *widget_name;
	GtkWidget *item;
	const GeanyIndentPrefs *iprefs;

	g_return_if_fail(doc == NULL || doc->is_valid);

	if (doc == NULL)
		doc = document_get_current();

	if (doc == NULL)
		return;

	ignore_callback = TRUE;

	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_line_wrapping1")),
			doc->editor->line_wrapping);

	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "line_breaking1")),
			doc->editor->line_breaking);

	iprefs = editor_get_indent_prefs(doc->editor);

	item = ui_lookup_widget(main_widgets.window, "menu_use_auto_indentation1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), doc->editor->auto_indent);

	switch (iprefs->type)
	{
		case GEANY_INDENT_TYPE_SPACES:
			widget_name = "spaces1"; break;
		case GEANY_INDENT_TYPE_TABS:
			widget_name = "tabs1"; break;
		case GEANY_INDENT_TYPE_BOTH:
		default:
			widget_name = "tabs_and_spaces1"; break;
	}
	item = ui_lookup_widget(main_widgets.window, widget_name);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);

	if (iprefs->width >= 1 && iprefs->width <= 8)
	{
		gchar *name;

		name = g_strdup_printf("indent_width_%d", iprefs->width);
		item = ui_lookup_widget(main_widgets.window, name);
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
		g_free(name);
	}

	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "set_file_readonly1")),
			doc->readonly);

	item = ui_lookup_widget(main_widgets.window, "menu_write_unicode_bom1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), doc->has_bom);
	ui_widget_set_sensitive(item, encodings_is_unicode_charset(doc->encoding));

	switch (sci_get_eol_mode(doc->editor->sci))
	{
		case SC_EOL_CR: widget_name = "cr"; break;
		case SC_EOL_LF: widget_name = "lf"; break;
		default: widget_name = "crlf"; break;
	}
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, widget_name)), TRUE);

	encodings_select_radio_item(doc->encoding);
	filetypes_select_radio_item(doc->file_type);

	ignore_callback = FALSE;
}


void ui_set_search_entry_background(GtkWidget *widget, gboolean success)
{
	gtk_widget_set_name(widget, success ? NULL : "geany-search-entry-no-match");
}


static void recent_create_menu(GeanyRecentFiles *grf)
{
	guint i, len;

	len = MIN(file_prefs.mru_length, g_queue_get_length(grf->recent_queue));
	for (i = 0; i < len; i++)
	{
		/* create menu item for the recent files menu in the menu bar */
		const gchar *filename = g_queue_peek_nth(grf->recent_queue, i);
		GtkWidget *tmp = gtk_menu_item_new_with_label(filename);

		gtk_widget_show(tmp);
		gtk_container_add(GTK_CONTAINER(grf->menubar), tmp);
		g_signal_connect(tmp, "activate", G_CALLBACK(grf->activate_cb), NULL);
		/* create menu item for the recent files menu in the toolbar */
		if (grf->toolbar != NULL)
		{
			tmp = gtk_menu_item_new_with_label(filename);
			gtk_widget_show(tmp);
			gtk_container_add(GTK_CONTAINER(grf->toolbar), tmp);
			g_signal_connect(tmp, "activate", G_CALLBACK(grf->activate_cb), NULL);
		}
	}
}


static GeanyRecentFiles *recent_get_recent_files(void)
{
	static GeanyRecentFiles grf = { RECENT_FILE_FILE, NULL, NULL, NULL, NULL };

	if (G_UNLIKELY(grf.recent_queue == NULL))
	{
		grf.recent_queue = ui_prefs.recent_queue;
		grf.menubar = ui_widgets.recent_files_menu_menubar;
		grf.toolbar = geany_menu_button_action_get_menu(GEANY_MENU_BUTTON_ACTION(
						toolbar_get_action_by_name("Open")));
		grf.activate_cb = recent_file_activate_cb;
	}
	return &grf;
}


static GeanyRecentFiles *recent_get_recent_projects(void)
{
	static GeanyRecentFiles grf = { RECENT_FILE_PROJECT, NULL, NULL, NULL, NULL };

	if (G_UNLIKELY(grf.recent_queue == NULL))
	{
		grf.recent_queue = ui_prefs.recent_projects_queue;
		grf.menubar = ui_widgets.recent_projects_menu_menubar;
		grf.toolbar = NULL;
		grf.activate_cb = recent_project_activate_cb;
	}
	return &grf;
}


void ui_create_recent_menus(void)
{
	recent_create_menu(recent_get_recent_files());
	recent_create_menu(recent_get_recent_projects());
}


static void recent_file_activate_cb(GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *utf8_filename = ui_menu_item_get_text(menuitem);
	gchar *locale_filename = utils_get_locale_from_utf8(utf8_filename);

	if (document_open_file(locale_filename, FALSE, NULL, NULL) != NULL)
		recent_file_loaded(utf8_filename, recent_get_recent_files());

	g_free(locale_filename);
	g_free(utf8_filename);
}


static void recent_project_activate_cb(GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *utf8_filename = ui_menu_item_get_text(menuitem);
	gchar *locale_filename = utils_get_locale_from_utf8(utf8_filename);

	if (project_ask_close() && project_load_file_with_session(locale_filename))
		recent_file_loaded(utf8_filename, recent_get_recent_projects());

	g_free(locale_filename);
	g_free(utf8_filename);
}


static void add_recent_file(const gchar *utf8_filename, GeanyRecentFiles *grf,
		const GtkRecentData *rdata)
{
	if (g_queue_find_custom(grf->recent_queue, utf8_filename, (GCompareFunc) strcmp) == NULL)
	{

		if (grf->type == RECENT_FILE_FILE && rdata)
		{
			GtkRecentManager *manager = gtk_recent_manager_get_default();
			gchar *uri = g_filename_to_uri(utf8_filename, NULL, NULL);
			if (uri != NULL)
			{
				gtk_recent_manager_add_full(manager, uri, rdata);
				g_free(uri);
			}
		}

		g_queue_push_head(grf->recent_queue, g_strdup(utf8_filename));
		if (g_queue_get_length(grf->recent_queue) > file_prefs.mru_length)
		{
			g_free(g_queue_pop_tail(grf->recent_queue));
		}
		update_recent_menu(grf);
	}
	/* filename already in recent list */
	else
		recent_file_loaded(utf8_filename, grf);
}


void ui_add_recent_document(GeanyDocument *doc)
{
	/* what are the groups for actually? */
	static const gchar *groups[2] = {
		"geany",
		NULL
	};
	GtkRecentData rdata;

	/* Prepare the data for gtk_recent_manager_add_full() */
	rdata.display_name = NULL;
	rdata.description = NULL;
	rdata.mime_type = doc->file_type->mime_type;
	/* if we ain't got no mime-type, fallback to plain text */
	if (! rdata.mime_type)
		rdata.mime_type = (gchar *) "text/plain";
	rdata.app_name = (gchar *) "geany";
	rdata.app_exec = (gchar *) "geany %u";
	rdata.groups = (gchar **) groups;
	rdata.is_private = FALSE;

	add_recent_file(doc->file_name, recent_get_recent_files(), &rdata);
}


void ui_add_recent_project_file(const gchar *utf8_filename)
{
	add_recent_file(utf8_filename, recent_get_recent_projects(), NULL);
}


/* Returns: newly allocated string with the UTF-8 menu text. */
gchar *ui_menu_item_get_text(GtkMenuItem *menu_item)
{
	const gchar *text = NULL;

	if (gtk_bin_get_child(GTK_BIN(menu_item)))
	{
		GtkWidget *child = gtk_bin_get_child(GTK_BIN(menu_item));

		if (GTK_IS_LABEL(child))
			text = gtk_label_get_text(GTK_LABEL(child));
	}
	/* GTK owns text so it's much safer to return a copy of it in case the memory is reallocated */
	return g_strdup(text);
}


static gint find_recent_file_item(gconstpointer list_data, gconstpointer user_data)
{
	gchar *menu_text = ui_menu_item_get_text(GTK_MENU_ITEM(list_data));
	gint result;

	if (utils_str_equal(menu_text, user_data))
		result = 0;
	else
		result = 1;

	g_free(menu_text);
	return result;
}


/* update the project menu item's sensitivity */
void ui_update_recent_project_menu(void)
{
	GeanyRecentFiles *grf = recent_get_recent_projects();
	GList *children, *item;

	/* only need to update the menubar menu, the project doesn't have a toolbar item */
	children = gtk_container_get_children(GTK_CONTAINER(grf->menubar));
	for (item = children; item; item = item->next)
	{
		gboolean sensitive = TRUE;

		if (app->project)
		{
			const gchar *filename = gtk_menu_item_get_label(item->data);
			sensitive = g_strcmp0(app->project->file_name, filename) != 0;
		}
		gtk_widget_set_sensitive(item->data, sensitive);
	}
	g_list_free(children);
}


/* Use instead of gtk_menu_reorder_child() to update the menubar properly on OS X */
static void menu_reorder_child(GtkMenu *menu, GtkWidget *child, gint position)
{
	gtk_menu_reorder_child(menu, child, position);
#ifdef MAC_INTEGRATION
	/* On OS X GtkMenuBar is kept in sync with the native OS X menubar using
	 * signals. Unfortunately gtk_menu_reorder_child() doesn't emit anything
	 * so we have to update the OS X menubar manually. */
	gtkosx_application_sync_menubar(gtkosx_application_get());
#endif
}


static void add_recent_file_menu_item(const gchar *utf8_filename, GeanyRecentFiles *grf, GtkWidget *menu)
{
	GtkWidget *child = gtk_menu_item_new_with_label(utf8_filename);

	gtk_widget_show(child);
	if (menu != grf->toolbar)
		gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), child);
	else
	{
		/* this is a bit ugly, but we need to use gtk_container_add(). Using
		 * gtk_menu_shell_prepend() doesn't emit GtkContainer's "add" signal
		 * which we need in GeanyMenubuttonAction */
		gtk_container_add(GTK_CONTAINER(menu), child);
		menu_reorder_child(GTK_MENU(menu), child, 0);
	}
	g_signal_connect(child, "activate", G_CALLBACK(grf->activate_cb), NULL);
}


static void recent_file_loaded(const gchar *utf8_filename, GeanyRecentFiles *grf)
{
	GList *item;
	GtkWidget *parents[] = { grf->menubar, grf->toolbar };
	guint i;

	/* first reorder the queue */
	item = g_queue_find_custom(grf->recent_queue, utf8_filename, (GCompareFunc) strcmp);
	g_return_if_fail(item != NULL);

	g_queue_unlink(grf->recent_queue, item);
	g_queue_push_head_link(grf->recent_queue, item);

	for (i = 0; i < G_N_ELEMENTS(parents); i++)
	{
		GList *children;

		if (! parents[i])
			continue;

		children = gtk_container_get_children(GTK_CONTAINER(parents[i]));
		item = g_list_find_custom(children, utf8_filename, (GCompareFunc) find_recent_file_item);
		/* either reorder or prepend a new one */
		if (item)
			menu_reorder_child(GTK_MENU(parents[i]), item->data, 0);
		else
			add_recent_file_menu_item(utf8_filename, grf, parents[i]);
		g_list_free(children);
	}

	if (grf->type == RECENT_FILE_PROJECT)
		ui_update_recent_project_menu();
}


static void update_recent_menu(GeanyRecentFiles *grf)
{
	gchar *filename;
	GtkWidget *parents[] = { grf->menubar, grf->toolbar };
	guint i;

	filename = g_queue_peek_head(grf->recent_queue);

	for (i = 0; i < G_N_ELEMENTS(parents); i++)
	{
		GList *children;

		if (! parents[i])
			continue;

		/* clean the MRU list before adding an item */
		children = gtk_container_get_children(GTK_CONTAINER(parents[i]));
		if (g_list_length(children) > file_prefs.mru_length - 1)
		{
			GList *item = g_list_nth(children, file_prefs.mru_length - 1);
			while (item != NULL)
			{
				if (GTK_IS_MENU_ITEM(item->data))
					gtk_widget_destroy(GTK_WIDGET(item->data));
				item = g_list_next(item);
			}
		}
		g_list_free(children);

		/* create the new item */
		add_recent_file_menu_item(filename, grf, parents[i]);
	}

	if (grf->type == RECENT_FILE_PROJECT)
		ui_update_recent_project_menu();
}


void ui_toggle_editor_features(GeanyUIEditorFeatures feature)
{
	guint i;

	foreach_document (i)
	{
		GeanyDocument *doc = documents[i];

		switch (feature)
		{
			case GEANY_EDITOR_SHOW_MARKERS_MARGIN:
				sci_set_symbol_margin(doc->editor->sci, editor_prefs.show_markers_margin);
				break;
			case GEANY_EDITOR_SHOW_LINE_NUMBERS:
				sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin);
				break;
			case GEANY_EDITOR_SHOW_WHITE_SPACE:
				sci_set_visible_white_spaces(doc->editor->sci, editor_prefs.show_white_space);
				break;
			case GEANY_EDITOR_SHOW_LINE_ENDINGS:
				sci_set_visible_eols(doc->editor->sci, editor_prefs.show_line_endings);
				break;
			case GEANY_EDITOR_SHOW_INDENTATION_GUIDES:
				editor_set_indentation_guides(doc->editor);
				break;
		}
	}
}


void ui_update_view_editor_menu_items(void)
{
	ignore_callback = TRUE;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_markers_margin1")), editor_prefs.show_markers_margin);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_linenumber_margin1")), editor_prefs.show_linenumber_margin);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_show_white_space1")), editor_prefs.show_white_space);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_show_line_endings1")), editor_prefs.show_line_endings);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_show_indentation_guides1")), editor_prefs.show_indent_guide);
	ignore_callback = FALSE;
}


/** Creates a GNOME HIG-style frame (with no border and indented child alignment).
 * @param label_text The label text.
 * @param alignment An address to store the alignment widget pointer.
 *
 * @return @transfer{floating} The frame widget, setting the alignment container for
 * packing child widgets.
 **/
GEANY_API_SYMBOL
GtkWidget *ui_frame_new_with_alignment(const gchar *label_text, GtkWidget **alignment)
{
	GtkWidget *label, *align;
	GtkWidget *frame = gtk_frame_new(NULL);

	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	align = gtk_alignment_new(0.5, 0.5, 1, 1);
	gtk_container_add(GTK_CONTAINER(frame), align);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);

	label = ui_label_new_bold(label_text);
	gtk_frame_set_label_widget(GTK_FRAME(frame), label);

	*alignment = align;
	return frame;
}


/** Makes a fixed border for dialogs without increasing the button box border.
 * @param dialog The parent container for the @c GtkVBox.
 *
 * @return @transfer{none} The packed @c GtkVBox. */
GEANY_API_SYMBOL
GtkWidget *ui_dialog_vbox_new(GtkDialog *dialog)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 12);	/* need child vbox to set a separate border. */

	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox, TRUE, TRUE, 0);
	return vbox;
}


/* Reorders a dialog's buttons
 * @param dialog A dialog
 * @param response First response ID to reorder
 * @param ... more response IDs, terminated by -1
 *
 * Like gtk_dialog_set_alternative_button_order(), but reorders the default
 * buttons layout, not the alternative one.  This is useful if you e.g. added a
 * button to a dialog which already had some and need yours not to be on the
 * end.
 */
/* Heavily based on gtk_dialog_set_alternative_button_order().
 * This relies on the action area to be a GtkBox, but although not documented
 * the API expose it to be a GtkHButtonBox though GtkBuilder, so it should be
 * fine */
void ui_dialog_set_primary_button_order(GtkDialog *dialog, gint response, ...)
{
	va_list ap;
	GtkWidget *action_area = gtk_dialog_get_action_area(dialog);
	gint position;

	va_start(ap, response);
	for (position = 0; response != -1; position++)
	{
		GtkWidget *child = gtk_dialog_get_widget_for_response(dialog, response);
		if (child)
			gtk_box_reorder_child(GTK_BOX(action_area), child, position);
		else
			g_warning("%s: no child button with response id %d.", G_STRFUNC, response);

		response = va_arg(ap, gint);
	}
	va_end(ap);
}


/** Creates a @c GtkButton with custom text and a stock image similar to
 * @c gtk_button_new_from_stock().
 * @param stock_id A @c GTK_STOCK_NAME string.
 * @param text Button label text, can include mnemonics.
 *
 * @return @transfer{floating} The new @c GtkButton.
 */
GEANY_API_SYMBOL
GtkWidget *ui_button_new_with_image(const gchar *stock_id, const gchar *text)
{
	GtkWidget *image, *button;

	button = gtk_button_new_with_mnemonic(text);
	gtk_widget_show(button);
	image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(button), image);
	/* note: image is shown by gtk */
	return button;
}


/** Creates a @c GtkImageMenuItem with a stock image and a custom label.
 * @param stock_id Stock image ID, e.g. @c GTK_STOCK_OPEN.
 * @param label Menu item label, can include mnemonics.
 * @return @transfer{floating} The new @c GtkImageMenuItem.
 *
 *  @since 0.16
 */
GEANY_API_SYMBOL
GtkWidget *
ui_image_menu_item_new(const gchar *stock_id, const gchar *label)
{
	GtkWidget *item = gtk_image_menu_item_new_with_mnemonic(label);
	GtkWidget *image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_MENU);

	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(image);
	return item;
}


static void entry_clear_icon_release_cb(GtkEntry *entry, gint icon_pos,
										GdkEvent *event, gpointer data)
{
	if (event->button.button == 1 && icon_pos == 1)
	{
		gtk_entry_set_text(entry, "");
		gtk_widget_grab_focus(GTK_WIDGET(entry));
	}
}


/** Adds a small clear icon to the right end of the passed @a entry.
 *  A callback to clear the contents of the GtkEntry is automatically added.
 *
 * @param entry The GtkEntry object to which the icon should be attached.
 *
 *  @since 0.16
 */
GEANY_API_SYMBOL
void ui_entry_add_clear_icon(GtkEntry *entry)
{
	g_object_set(entry, "secondary-icon-stock", GTK_STOCK_CLEAR,
		"secondary-icon-activatable", TRUE, NULL);
	g_signal_connect(entry, "icon-release", G_CALLBACK(entry_clear_icon_release_cb), NULL);
}


/* Adds a :activate-backwards signal emitted by default when <Shift>Return is pressed */
void ui_entry_add_activate_backward_signal(GtkEntry *entry)
{
	static gboolean installed = FALSE;

	g_return_if_fail(GTK_IS_ENTRY(entry));

	if (G_UNLIKELY(! installed))
	{
		GtkBindingSet *binding_set;

		installed = TRUE;

		/* try to handle the unexpected case where GTK would already have installed the signal */
		if (g_signal_lookup("activate-backward", G_TYPE_FROM_INSTANCE(entry)))
		{
			g_warning("Signal GtkEntry:activate-backward is unexpectedly already installed");
			return;
		}

		g_signal_new("activate-backward", G_TYPE_FROM_INSTANCE(entry),
			G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, 0, NULL, NULL,
			g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
		binding_set = gtk_binding_set_by_class(GTK_ENTRY_GET_CLASS(entry));
		gtk_binding_entry_add_signal(binding_set, GDK_Return, GDK_SHIFT_MASK, "activate-backward", 0);
	}
}


static void add_to_size_group(GtkWidget *widget, gpointer size_group)
{
	g_return_if_fail(GTK_IS_SIZE_GROUP(size_group));
	gtk_size_group_add_widget(GTK_SIZE_GROUP(size_group), widget);
}


/* Copies the spacing and layout of the master GtkHButtonBox and synchronises
 * the width of each button box's children.
 * Should be called after all child widgets have been packed. */
void ui_hbutton_box_copy_layout(GtkButtonBox *master, GtkButtonBox *copy)
{
	GtkSizeGroup *size_group;

	gtk_box_set_spacing(GTK_BOX(copy), 10);
	gtk_button_box_set_layout(copy, gtk_button_box_get_layout(master));

	/* now we need to put the widest widget from each button box in a size group,
	* but we don't know the width before they are drawn, and for different label
	* translations the widest widget can vary, so we just add all widgets. */
	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_container_foreach(GTK_CONTAINER(master), add_to_size_group, size_group);
	gtk_container_foreach(GTK_CONTAINER(copy), add_to_size_group, size_group);
	g_object_unref(size_group);
}


static gboolean tree_model_find_text(GtkTreeModel *model,
		GtkTreeIter *iter, gint column, const gchar *text)
{
	gchar *combo_text;
	gboolean found = FALSE;

	if (gtk_tree_model_get_iter_first(model, iter))
	{
		do
		{
			gtk_tree_model_get(model, iter, 0, &combo_text, -1);
			found = utils_str_equal(combo_text, text);
			g_free(combo_text);

			if (found)
				return TRUE;
		}
		while (gtk_tree_model_iter_next(model, iter));
	}
	return FALSE;
}


/** Prepends @a text to the drop down list, removing a duplicate element in
 * the list if found. Also ensures there are <= @a history_len elements.
 * @param combo_entry .
 * @param text @nullable Text to add, or @c NULL for current entry text.
 * @param history_len Max number of items, or @c 0 for default. */
GEANY_API_SYMBOL
void ui_combo_box_add_to_history(GtkComboBoxText *combo_entry,
		const gchar *text, gint history_len)
{
	GtkComboBox *combo = GTK_COMBO_BOX(combo_entry);
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;

	if (history_len <= 0)
		history_len = 10;
	if (!text)
		text = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))));

	model = gtk_combo_box_get_model(combo);

	if (tree_model_find_text(model, &iter, 0, text))
	{
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}
	gtk_combo_box_text_prepend_text(combo_entry, text);

	/* limit history */
	path = gtk_tree_path_new_from_indices(history_len, -1);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}
	gtk_tree_path_free(path);
}


/* Same as gtk_combo_box_text_prepend_text(), except that text is only prepended if it not already
 * exists in the combo's model. */
void ui_combo_box_prepend_text_once(GtkComboBoxText *combo, const gchar *text)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
	if (tree_model_find_text(model, &iter, 0, text))
		return;	/* don't prepend duplicate */

	gtk_combo_box_text_prepend_text(combo, text);
}


/* Changes the color of the notebook tab text and open files items according to
 * document status. */
void ui_update_tab_status(GeanyDocument *doc)
{
	gtk_widget_set_name(doc->priv->tab_label, document_get_status_widget_class(doc));

	sidebar_openfiles_update(doc);
}


static gboolean tree_model_iter_get_next(GtkTreeModel *model, GtkTreeIter *iter,
		gboolean down)
{
	GtkTreePath *path;
	gboolean result;

	if (down)
		return gtk_tree_model_iter_next(model, iter);

	path = gtk_tree_model_get_path(model, iter);
	result = gtk_tree_path_prev(path) && gtk_tree_model_get_iter(model, iter, path);
	gtk_tree_path_free(path);
	return result;
}


/* note: the while loop might be more efficient when searching upwards if it
 * used tree paths instead of tree iters, but in practice it probably doesn't matter much. */
static gboolean tree_view_find(GtkTreeView *treeview, TVMatchCallback cb, gboolean down)
{
	GtkTreeSelection *treesel;
	GtkTreeIter iter;
	GtkTreeModel *model;

	treesel = gtk_tree_view_get_selection(treeview);
	if (gtk_tree_selection_get_selected(treesel, &model, &iter))
	{
		/* get the next selected item */
		if (! tree_model_iter_get_next(model, &iter, down))
			return FALSE;	/* no more items */
	}
	else	/* no selection */
	{
		if (! gtk_tree_model_get_iter_first(model, &iter))
			return TRUE;	/* no items */
	}
	while (TRUE)
	{
		gtk_tree_selection_select_iter(treesel, &iter);
		if (cb(FALSE))
			break;	/* found next message */

		if (! tree_model_iter_get_next(model, &iter, down))
			return FALSE;	/* no more items */
	}
	/* scroll item in view */
	if (ui_prefs.msgwindow_visible)
	{
		GtkTreePath *path = gtk_tree_model_get_path(
			gtk_tree_view_get_model(treeview), &iter);

		gtk_tree_view_scroll_to_cell(treeview, path, NULL, TRUE, 0.5, 0.5);
		gtk_tree_path_free(path);
	}
	return TRUE;
}


/* Returns FALSE if the treeview has items but no matching next item. */
gboolean ui_tree_view_find_next(GtkTreeView *treeview, TVMatchCallback cb)
{
	return tree_view_find(treeview, cb, TRUE);
}


/* Returns FALSE if the treeview has items but no matching next item. */
gboolean ui_tree_view_find_previous(GtkTreeView *treeview, TVMatchCallback cb)
{
	return tree_view_find(treeview, cb, FALSE);
}


/* Shamelessly stolen from GTK */
static gboolean ui_tree_view_query_tooltip_cb(GtkWidget *widget, gint x, gint y,
		gboolean keyboard_tip, GtkTooltip *tooltip, gpointer data)
{
	GValue value = { 0 };
	GValue transformed = { 0 };
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	GtkTreeView *tree_view = GTK_TREE_VIEW(widget);
	gint column = GPOINTER_TO_INT(data);
	gboolean tootlip_set = FALSE;

	if (! gtk_tree_view_get_tooltip_context(tree_view, &x, &y, keyboard_tip, &model, &path, &iter))
		return FALSE;

	gtk_tree_model_get_value(model, &iter, column, &value);

	g_value_init(&transformed, G_TYPE_STRING);
	if (g_value_transform(&value, &transformed) && g_value_get_string(&transformed))
	{
		gtk_tooltip_set_text(tooltip, g_value_get_string(&transformed));
		gtk_tree_view_set_tooltip_row(tree_view, tooltip, path);
		tootlip_set = TRUE;
	}

	g_value_unset(&transformed);
	g_value_unset(&value);
	gtk_tree_path_free(path);

	return tootlip_set;
}


/** Adds text tooltips to a tree view.
 *
 * This is similar to gtk_tree_view_set_tooltip_column() but considers the column contents to be
 * text, not markup -- it uses gtk_tooltip_set_text() rather than gtk_tooltip_set_markup() to set
 * the tooltip's value.
 *
 * @warning Unlike gtk_tree_view_set_tooltip_column() you currently cannot change or remove the
 * tooltip column after it has been added.  Trying to do so will probably give funky results.
 *
 * @param tree_view The tree view
 * @param column The column to get the tooltip from
 *
 * @since 1.25 (API 223)
 */
/* Note: @p column is int and not uint both to match gtk_tree_view_set_tooltip_column() signature
 * and to allow future support of -1 to unset if ever wanted */
GEANY_API_SYMBOL
void ui_tree_view_set_tooltip_text_column(GtkTreeView *tree_view, gint column)
{
	g_return_if_fail(column >= 0);
	g_return_if_fail(GTK_IS_TREE_VIEW(tree_view));

	g_signal_connect(tree_view, "query-tooltip",
			G_CALLBACK(ui_tree_view_query_tooltip_cb), GINT_TO_POINTER(column));
	gtk_widget_set_has_tooltip(GTK_WIDGET(tree_view), TRUE);
}


/**
 * Modifies the font of a widget using gtk_widget_modify_font().
 *
 * @param widget The widget.
 * @param str The font name as expected by pango_font_description_from_string().
 */
GEANY_API_SYMBOL
void ui_widget_modify_font_from_string(GtkWidget *widget, const gchar *str)
{
	PangoFontDescription *pfd;

	pfd = pango_font_description_from_string(str);
	gtk_widget_modify_font(widget, pfd);
	pango_font_description_free(pfd);
}


/** Creates a @c GtkHBox with @a entry packed into it and an open button which runs a
 * file chooser, replacing entry text (if successful) with the path returned from the
 * @c GtkFileChooser.
 * @note @a entry can be the child of an unparented widget, such as @c GtkComboBoxEntry.
 * @param title @nullable The file chooser dialog title, or @c NULL.
 * @param action The mode of the file chooser.
 * @param entry Can be an unpacked @c GtkEntry, or the child of an unpacked widget,
 * such as @c GtkComboBoxEntry.
 *
 * @return @transfer{floating} The @c GtkHBox.
 */
/* @see ui_setup_open_button_callback(). */
GEANY_API_SYMBOL
GtkWidget *ui_path_box_new(const gchar *title, GtkFileChooserAction action, GtkEntry *entry)
{
	GtkWidget *vbox, *dirbtn, *openimg, *hbox, *path_entry, *parent, *next_parent;

	hbox = gtk_hbox_new(FALSE, 6);
	path_entry = GTK_WIDGET(entry);

	/* prevent path_entry being vertically stretched to the height of dirbtn */
	vbox = gtk_vbox_new(FALSE, 0);

	parent = path_entry;
	while ((next_parent = gtk_widget_get_parent(parent)) != NULL)
		parent = next_parent;

	gtk_box_pack_start(GTK_BOX(vbox), parent, TRUE, FALSE, 0);

	dirbtn = gtk_button_new();
	openimg = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(dirbtn), openimg);
	ui_setup_open_button_callback(dirbtn, title, action, entry);

	gtk_box_pack_end(GTK_BOX(hbox), dirbtn, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	return hbox;
}


static void ui_path_box_open_clicked(GtkButton *button, gpointer user_data);


/* Setup a GtkButton to run a GtkFileChooser, setting entry text if successful.
 * title can be NULL.
 * action is the file chooser mode to use. */
void ui_setup_open_button_callback(GtkWidget *open_btn, const gchar *title,
		GtkFileChooserAction action, GtkEntry *entry)
{
	GtkWidget *path_entry = GTK_WIDGET(entry);

	if (title)
		g_object_set_data_full(G_OBJECT(open_btn), "title", g_strdup(title),
				(GDestroyNotify) g_free);
	g_object_set_data(G_OBJECT(open_btn), "action", GINT_TO_POINTER(action));
	g_signal_connect(open_btn, "clicked", G_CALLBACK(ui_path_box_open_clicked), path_entry);
}


#ifndef G_OS_WIN32
static gchar *run_file_chooser(const gchar *title, GtkFileChooserAction action,
		const gchar *utf8_path)
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new(title,
		GTK_WINDOW(main_widgets.window), action,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	gchar *locale_path;
	gchar *ret_path = NULL;

	gtk_widget_set_name(dialog, "GeanyDialog");
	locale_path = utils_get_locale_from_utf8(utf8_path);
	if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
	{
		if (g_path_is_absolute(locale_path) && g_file_test(locale_path, G_FILE_TEST_IS_DIR))
			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), locale_path);
	}
	else if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
		if (g_path_is_absolute(locale_path))
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), locale_path);
	}
	g_free(locale_path);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		gchar *dir_locale;

		dir_locale = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		ret_path = utils_get_utf8_from_locale(dir_locale);
		g_free(dir_locale);
	}
	gtk_widget_destroy(dialog);
	return ret_path;
}
#endif


static void ui_path_box_open_clicked(GtkButton *button, gpointer user_data)
{
	GtkFileChooserAction action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "action"));
	GtkEntry *entry = user_data;
	const gchar *title = g_object_get_data(G_OBJECT(button), "title");
	gchar *utf8_path = NULL;

	/* TODO: extend for other actions */
	g_return_if_fail(action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER ||
					 action == GTK_FILE_CHOOSER_ACTION_OPEN);

	if (title == NULL)
		title = (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ?
			_("Select Folder") : _("Select File");

	if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
	{
#ifdef G_OS_WIN32
		utf8_path = win32_show_file_dialog(GTK_WINDOW(ui_widgets.prefs_dialog), title,
						gtk_entry_get_text(GTK_ENTRY(entry)));
#else
		utf8_path = run_file_chooser(title, action, gtk_entry_get_text(GTK_ENTRY(entry)));
#endif
	}
	else if (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER)
	{
		gchar *path = g_path_get_dirname(gtk_entry_get_text(GTK_ENTRY(entry)));
#ifdef G_OS_WIN32
		utf8_path = win32_show_folder_dialog(ui_widgets.prefs_dialog, title,
						gtk_entry_get_text(GTK_ENTRY(entry)));
#else
		utf8_path = run_file_chooser(title, action, path);
#endif
		g_free(path);
	}

	if (utf8_path != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), utf8_path);
		g_free(utf8_path);
	}
}


void ui_statusbar_showhide(gboolean state)
{
	/* handle statusbar visibility */
	if (state)
	{
		gtk_widget_show(ui_widgets.statusbar);
		ui_update_statusbar(NULL, -1);
	}
	else
		gtk_widget_hide(ui_widgets.statusbar);
}


/** Packs all @c GtkWidgets passed after the row argument into a table, using
 * one widget per cell. The first widget is not expanded as the table grows,
 * as this is usually a label.
 * @param table
 * @param row The row number of the table.
 */
GEANY_API_SYMBOL
void ui_table_add_row(GtkTable *table, gint row, ...)
{
	va_list args;
	guint i;
	GtkWidget *widget;

	va_start(args, row);
	for (i = 0; (widget = va_arg(args, GtkWidget*), widget != NULL); i++)
	{
		gint options = (i == 0) ? GTK_FILL : GTK_EXPAND | GTK_FILL;

		gtk_table_attach(GTK_TABLE(table), widget, i, i + 1, row, row + 1,
			options, 0, 0, 0);
	}
	va_end(args);
}


static void on_config_file_clicked(GtkWidget *widget, gpointer user_data)
{
	const gchar *file_name = user_data;
	GeanyFiletype *ft = NULL;

	if (strstr(file_name, G_DIR_SEPARATOR_S "filetypes."))
		ft = filetypes[GEANY_FILETYPES_CONF];

	if (g_file_test(file_name, G_FILE_TEST_EXISTS))
		document_open_file(file_name, FALSE, ft, NULL);
	else
	{
		gchar *utf8_filename = utils_get_utf8_from_locale(file_name);
		gchar *base_name = g_path_get_basename(file_name);
		gchar *global_file = g_build_filename(app->datadir, base_name, NULL);
		gchar *global_content = NULL;

		/* if the requested file doesn't exist in the user's config dir, try loading the file
		 * from the global data directory and use its contents for the newly created file */
		if (g_file_test(global_file, G_FILE_TEST_EXISTS))
			g_file_get_contents(global_file, &global_content, NULL, NULL);

		document_new_file(utf8_filename, ft, global_content);

		utils_free_pointers(4, utf8_filename, base_name, global_file, global_content, NULL);
	}
}


static void free_on_closure_notify(gpointer data, GClosure *closure)
{
	g_free(data);
}


/* @note You should connect to the "document-save" signal yourself to detect
 * if the user has just saved the config file, reloading it. */
void ui_add_config_file_menu_item(const gchar *real_path, const gchar *label, GtkContainer *parent)
{
	GtkWidget *item;

	if (!parent)
		parent = GTK_CONTAINER(widgets.config_files_menu);

	if (!label)
	{
		gchar *base_name;

		base_name = g_path_get_basename(real_path);
		item = gtk_menu_item_new_with_label(base_name);
		g_free(base_name);
	}
	else
		item = gtk_menu_item_new_with_mnemonic(label);

	gtk_widget_show(item);
	gtk_container_add(parent, item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_config_file_clicked),
			g_strdup(real_path), free_on_closure_notify, 0);
}


static gboolean sort_menu(gpointer data)
{
	ui_menu_sort_by_label(GTK_MENU(data));
	return FALSE;
}


static void create_config_files_menu(void)
{
	GtkWidget *menu, *item;

	widgets.config_files_menu = menu = gtk_menu_new();

	item = ui_lookup_widget(main_widgets.window, "configuration_files1");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);

	item = gtk_menu_item_new_with_mnemonic(_("_Filetype Configuration"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	ui_widgets.config_files_filetype_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), ui_widgets.config_files_filetype_menu);
	gtk_widget_show(item);

	/* sort menu after all items added */
	g_idle_add(sort_menu, widgets.config_files_menu);
}


/* adds factory icons with a named icon source using the stock items id */
static void add_stock_icons(const GtkStockItem *items, gsize count)
{
	GtkIconFactory *factory = gtk_icon_factory_new();
	GtkIconSource *source = gtk_icon_source_new();
	gsize i;

	for (i = 0; i < count; i++)
	{
		GtkIconSet *set = gtk_icon_set_new();

		gtk_icon_source_set_icon_name(source, items[i].stock_id);
		gtk_icon_set_add_source(set, source);
		gtk_icon_factory_add(factory, items[i].stock_id, set);
		gtk_icon_set_unref(set);
	}
	gtk_icon_source_free(source);
	gtk_icon_factory_add_default(factory);
	g_object_unref(factory);
}


void ui_init_stock_items(void)
{
	GtkStockItem items[] =
	{
		{ GEANY_STOCK_SAVE_ALL, N_("Save All"), 0, 0, GETTEXT_PACKAGE },
		{ GEANY_STOCK_CLOSE_ALL, N_("Close All"), 0, 0, GETTEXT_PACKAGE },
		{ GEANY_STOCK_BUILD, N_("Build"), 0, 0, GETTEXT_PACKAGE }
	};

	gtk_stock_add(items, G_N_ELEMENTS(items));
	add_stock_icons(items, G_N_ELEMENTS(items));
}


void ui_init_toolbar_widgets(void)
{
	widgets.save_buttons[1] = toolbar_get_widget_by_name("Save");
	widgets.save_buttons[3] = toolbar_get_widget_by_name("SaveAll");
	widgets.redo_items[2] = toolbar_get_widget_by_name("Redo");
	widgets.undo_items[2] = toolbar_get_widget_by_name("Undo");
}


void ui_swap_sidebar_pos(void)
{
	GtkWidget *pane = ui_lookup_widget(main_widgets.window, "hpaned1");
	GtkWidget *left = gtk_paned_get_child1(GTK_PANED(pane));
	GtkWidget *right = gtk_paned_get_child2(GTK_PANED(pane));

	g_object_ref(left);
	g_object_ref(right);
	gtk_container_remove (GTK_CONTAINER (pane), left);
	gtk_container_remove (GTK_CONTAINER (pane), right);
	/* only scintilla notebook should expand */
	gtk_paned_pack1(GTK_PANED(pane), right, right == main_widgets.notebook, TRUE);
	gtk_paned_pack2(GTK_PANED(pane), left, left == main_widgets.notebook, TRUE);
	g_object_unref(left);
	g_object_unref(right);

	gtk_paned_set_position(GTK_PANED(pane), gtk_widget_get_allocated_width(pane)
		- gtk_paned_get_position(GTK_PANED(pane)));
}


static void init_recent_files(void)
{
	GtkWidget *toolbar_recent_files_menu;

	/* add recent files to the File menu */
	ui_widgets.recent_files_menuitem = ui_lookup_widget(main_widgets.window, "recent_files1");
	ui_widgets.recent_files_menu_menubar = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(ui_widgets.recent_files_menuitem),
							ui_widgets.recent_files_menu_menubar);

	/* add recent files to the toolbar Open button */
	toolbar_recent_files_menu = gtk_menu_new();
	g_object_ref(toolbar_recent_files_menu);
	geany_menu_button_action_set_menu(GEANY_MENU_BUTTON_ACTION(
		toolbar_get_action_by_name("Open")), toolbar_recent_files_menu);
}


static void ui_menu_move(GtkWidget *menu, GtkWidget *old, GtkWidget *new)
{
	g_object_ref(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(old), NULL);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(new), menu);
	g_object_unref(menu);
}


typedef struct GeanySharedMenu
{
	const gchar *menu;
	const gchar *menubar_item;
	const gchar *popup_item;
}
GeanySharedMenu;

#define foreach_menu(item, array) \
	for (item = array; item->menu; item++)

static void on_editor_menu_show(GtkWidget *widget, GeanySharedMenu *items)
{
	GeanySharedMenu *item;

	foreach_menu(item, items)
	{
		GtkWidget *popup = ui_lookup_widget(main_widgets.editor_menu, item->popup_item);
		GtkWidget *bar = ui_lookup_widget(main_widgets.window, item->menubar_item);
		GtkWidget *menu = ui_lookup_widget(main_widgets.window, item->menu);

		ui_menu_move(menu, bar, popup);
	}
}


static void on_editor_menu_hide(GtkWidget *widget, GeanySharedMenu *items)
{
	GeanySharedMenu *item;

	foreach_menu(item, items)
	{
		GtkWidget *popup = ui_lookup_widget(main_widgets.editor_menu, item->popup_item);
		GtkWidget *bar = ui_lookup_widget(main_widgets.window, item->menubar_item);
		GtkWidget *menu = ui_lookup_widget(main_widgets.window, item->menu);

		ui_menu_move(menu, popup, bar);
	}
}


/* Currently ui_init() is called before keyfile.c stash group code is initialized,
 * so this is called after that's done. */
void ui_init_prefs(void)
{
	StashGroup *group = stash_group_new(PACKAGE);

	/* various prefs */
	configuration_add_various_pref_group(group);

	stash_group_add_boolean(group, &interface_prefs.show_symbol_list_expanders,
		"show_symbol_list_expanders", TRUE);
	stash_group_add_boolean(group, &interface_prefs.compiler_tab_autoscroll,
		"compiler_tab_autoscroll", TRUE);
	stash_group_add_boolean(group, &ui_prefs.allow_always_save,
		"allow_always_save", FALSE);
	stash_group_add_string(group, &ui_prefs.statusbar_template,
		"statusbar_template", _(DEFAULT_STATUSBAR_TEMPLATE));
	stash_group_add_boolean(group, &ui_prefs.new_document_after_close,
		"new_document_after_close", FALSE);
	stash_group_add_boolean(group, &interface_prefs.msgwin_status_visible,
		"msgwin_status_visible", TRUE);
	stash_group_add_boolean(group, &interface_prefs.msgwin_compiler_visible,
		"msgwin_compiler_visible", TRUE);
	stash_group_add_boolean(group, &interface_prefs.msgwin_messages_visible,
		"msgwin_messages_visible", TRUE);
	stash_group_add_boolean(group, &interface_prefs.msgwin_scribble_visible,
		"msgwin_scribble_visible", TRUE);
}


/* Used to find out the name of the GtkBuilder retrieved object since
 * some objects will be GTK_IS_BUILDABLE() and use the GtkBuildable
 * 'name' property for that and those that don't implement GtkBuildable
 * will have a "gtk-builder-name" stored in the GObject's data list. */
static const gchar *ui_guess_object_name(GObject *obj)
{
	const gchar *name = NULL;

	g_return_val_if_fail(G_IS_OBJECT(obj), NULL);

	if (GTK_IS_BUILDABLE(obj))
		name = gtk_buildable_get_name(GTK_BUILDABLE(obj));
	if (! name)
		name = g_object_get_data(obj, "gtk-builder-name");
	if (! name)
		return NULL;

	return name;
}


/* Compatibility functions */
GtkWidget *create_edit_menu1(void)
{
	return edit_menu1;
}


GtkWidget *create_prefs_dialog(void)
{
	return prefs_dialog;
}


GtkWidget *create_project_dialog(void)
{
	return project_dialog;
}


GtkWidget *create_toolbar_popup_menu1(void)
{
	return toolbar_popup_menu1;
}


GtkWidget *create_window1(void)
{
	return window1;
}


static GtkWidget *ui_get_top_parent(GtkWidget *widget)
{
	GtkWidget *parent;

	g_return_val_if_fail(GTK_IS_WIDGET(widget), NULL);

	for (;;)
	{
		if (GTK_IS_MENU(widget))
			parent = gtk_menu_get_attach_widget(GTK_MENU(widget));
		else
			parent = gtk_widget_get_parent(widget);
		if (parent == NULL)
			parent = (GtkWidget*) g_object_get_data(G_OBJECT(widget), "GladeParentKey");
		if (parent == NULL)
			break;
		widget = parent;
	}

	return widget;
}


void ui_init_builder(void)
{
	gchar *interface_file;
	const gchar *name;
	GError *error;
	GSList *iter, *all_objects;
	GtkWidget *widget, *toplevel;

	/* prevent function from being called twice */
	if (GTK_IS_BUILDER(builder))
		return;

	builder = gtk_builder_new();

	gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);

	error = NULL;
	interface_file = g_build_filename(app->datadir, "geany.glade", NULL);
	if (! gtk_builder_add_from_file(builder, interface_file, &error))
	{
		/* Show the user this message so they know WTF happened */
		dialogs_show_msgbox_with_secondary(GTK_MESSAGE_ERROR,
			_("Geany cannot start!"), error->message);
		/* Aborts */
		g_error("Cannot create user-interface: %s", error->message);
		g_error_free(error);
		g_free(interface_file);
		g_object_unref(builder);
		return;
	}
	g_free(interface_file);

	callbacks_connect(builder);

	edit_menu1 = GTK_WIDGET(gtk_builder_get_object(builder, "edit_menu1"));
	prefs_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "prefs_dialog"));
	project_dialog = GTK_WIDGET(gtk_builder_get_object(builder, "project_dialog"));
	toolbar_popup_menu1 = GTK_WIDGET(gtk_builder_get_object(builder, "toolbar_popup_menu1"));
	window1 = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));

	g_object_set_data(G_OBJECT(edit_menu1), "edit_menu1", edit_menu1);
	g_object_set_data(G_OBJECT(prefs_dialog), "prefs_dialog", prefs_dialog);
	g_object_set_data(G_OBJECT(project_dialog), "project_dialog", project_dialog);
	g_object_set_data(G_OBJECT(toolbar_popup_menu1), "toolbar_popup_menu1", toolbar_popup_menu1);
	g_object_set_data(G_OBJECT(window1), "window1", window1);

	all_objects = gtk_builder_get_objects(builder);
	for (iter = all_objects; iter != NULL; iter = g_slist_next(iter))
	{
		if (! GTK_IS_WIDGET(iter->data))
			continue;

		widget = GTK_WIDGET(iter->data);

		name = ui_guess_object_name(G_OBJECT(widget));
		if (! name)
		{
			g_warning("Unable to get name from GtkBuilder object");
			continue;
		}

		toplevel = ui_get_top_parent(widget);
		if (toplevel)
			ui_hookup_widget(toplevel, widget, name);
	}
	g_slist_free(all_objects);
}


static void init_custom_style(void)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	const struct {
		guint version;
		const gchar *file;
	} css_files[] = {
		/*
		 * Keep these from newest to oldest,
		 * and make sure 0 remains last
		 */
		{ 20, "geany-3.20.css" },
		{ 0, "geany-3.0.css" },
	};
	guint gtk_version = gtk_get_minor_version();
	gsize i = 0;
	gchar *css_file;
	GtkCssProvider *css = gtk_css_provider_new();
	GError *error = NULL;

	/* gtk_version will never be smaller than 0 */
	while (css_files[i].version > gtk_version)
		++i;

	css_file = g_build_filename(app->datadir, css_files[i].file, NULL);
	if (! gtk_css_provider_load_from_path(css, css_file, &error))
	{
		g_warning("Failed to load custom CSS: %s", error->message);
		g_error_free(error);
	}
	else
	{
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
			GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	g_object_unref(css);
	g_free(css_file);
#else
	/* see setup_gtk2_styles() in main.c */
#endif
}


void ui_init(void)
{
	init_custom_style();

	init_recent_files();

	ui_widgets.statusbar = ui_lookup_widget(main_widgets.window, "statusbar");
	ui_widgets.print_page_setup = ui_lookup_widget(main_widgets.window, "page_setup1");

	main_widgets.progressbar = progress_bar_create();

	/* current word sensitive items */
	widgets.popup_goto_items[0] = ui_lookup_widget(main_widgets.editor_menu, "goto_tag_definition2");
	widgets.popup_goto_items[1] = ui_lookup_widget(main_widgets.editor_menu, "context_action1");
	widgets.popup_goto_items[2] = ui_lookup_widget(main_widgets.editor_menu, "find_usage2");
	widgets.popup_goto_items[3] = ui_lookup_widget(main_widgets.editor_menu, "find_document_usage2");

	widgets.popup_copy_items[0] = ui_lookup_widget(main_widgets.editor_menu, "cut1");
	widgets.popup_copy_items[1] = ui_lookup_widget(main_widgets.editor_menu, "copy1");
	widgets.popup_copy_items[2] = ui_lookup_widget(main_widgets.editor_menu, "delete1");
	widgets.menu_copy_items[0] = ui_lookup_widget(main_widgets.window, "menu_cut1");
	widgets.menu_copy_items[1] = ui_lookup_widget(main_widgets.window, "menu_copy1");
	widgets.menu_copy_items[2] = ui_lookup_widget(main_widgets.window, "menu_delete1");
	widgets.menu_insert_include_items[0] = ui_lookup_widget(main_widgets.editor_menu, "insert_include1");
	widgets.menu_insert_include_items[1] = ui_lookup_widget(main_widgets.window, "insert_include2");
	widgets.save_buttons[0] = ui_lookup_widget(main_widgets.window, "menu_save1");
	widgets.save_buttons[2] = ui_lookup_widget(main_widgets.window, "menu_save_all1");
	widgets.redo_items[0] = ui_lookup_widget(main_widgets.editor_menu, "redo1");
	widgets.redo_items[1] = ui_lookup_widget(main_widgets.window, "menu_redo2");
	widgets.undo_items[0] = ui_lookup_widget(main_widgets.editor_menu, "undo1");
	widgets.undo_items[1] = ui_lookup_widget(main_widgets.window, "menu_undo2");

	/* reparent context submenus as needed */
	{
		GeanySharedMenu arr[] = {
			{"commands2_menu", "commands2", "commands1"},
			{"menu_format1_menu", "menu_format1", "menu_format2"},
			{"more1_menu", "more1", "search2"},
			{NULL, NULL, NULL}
		};
		static GeanySharedMenu items[G_N_ELEMENTS(arr)];

		memcpy(items, arr, sizeof(arr));
		g_signal_connect(main_widgets.editor_menu, "show", G_CALLBACK(on_editor_menu_show), items);
		g_signal_connect(main_widgets.editor_menu, "hide", G_CALLBACK(on_editor_menu_hide), items);
	}

	ui_init_toolbar_widgets();
	init_document_widgets();
	create_config_files_menu();
}


void ui_finalize_builder(void)
{
	if (GTK_IS_BUILDER(builder))
		g_object_unref(builder);

	/* cleanup refs lingering even after GtkBuilder is destroyed */
	if (GTK_IS_WIDGET(edit_menu1))
		gtk_widget_destroy(edit_menu1);
	if (GTK_IS_WIDGET(prefs_dialog))
		gtk_widget_destroy(prefs_dialog);
	if (GTK_IS_WIDGET(project_dialog))
		gtk_widget_destroy(project_dialog);
	if (GTK_IS_WIDGET(toolbar_popup_menu1))
		gtk_widget_destroy(toolbar_popup_menu1);
	if (GTK_IS_WIDGET(window1))
		gtk_widget_destroy(window1);
}


static void auto_separator_update(GeanyAutoSeparator *autosep)
{
	g_return_if_fail(autosep->item_count >= 0);

	if (autosep->widget)
	{
		if (autosep->item_count > 0)
			ui_widget_show_hide(autosep->widget, autosep->show_count > 0);
		else
			gtk_widget_destroy(autosep->widget);
	}
}


static void on_auto_separator_item_show_hide(GtkWidget *widget, gpointer user_data)
{
	GeanyAutoSeparator *autosep = user_data;

	if (gtk_widget_get_visible(widget))
		autosep->show_count++;
	else
		autosep->show_count--;
	auto_separator_update(autosep);
}


static void on_auto_separator_item_destroy(GtkWidget *widget, gpointer user_data)
{
	GeanyAutoSeparator *autosep = user_data;

	autosep->item_count--;
	autosep->item_count = MAX(autosep->item_count, 0);
	/* gtk_widget_get_visible() won't work now the widget is being destroyed,
	 * so assume widget was visible */
	autosep->show_count--;
	autosep->show_count = MAX(autosep->item_count, 0);
	auto_separator_update(autosep);
}


/* Show the separator widget if @a item or another is visible. */
/* Note: This would be neater taking a widget argument, setting a "visible-count"
 * property, and using reference counting to keep the widget alive whilst its visible group
 * is alive. */
void ui_auto_separator_add_ref(GeanyAutoSeparator *autosep, GtkWidget *item)
{
	/* set widget ptr NULL when widget destroyed */
	if (autosep->item_count == 0)
		g_signal_connect(autosep->widget, "destroy",
			G_CALLBACK(gtk_widget_destroyed), &autosep->widget);

	if (gtk_widget_get_visible(item))
		autosep->show_count++;

	autosep->item_count++;
	auto_separator_update(autosep);

	g_signal_connect(item, "show", G_CALLBACK(on_auto_separator_item_show_hide), autosep);
	g_signal_connect(item, "hide", G_CALLBACK(on_auto_separator_item_show_hide), autosep);
	g_signal_connect(item, "destroy", G_CALLBACK(on_auto_separator_item_destroy), autosep);
}


/**
 * Sets @a text as the contents of the tooltip for @a widget.
 *
 * @param widget The widget the tooltip should be set for.
 * @param text The text for the tooltip.
 *
 * @since 0.16
 * @deprecated 0.21 use gtk_widget_set_tooltip_text() instead
 */
GEANY_API_SYMBOL
void ui_widget_set_tooltip_text(GtkWidget *widget, const gchar *text)
{
	gtk_widget_set_tooltip_text(widget, text);
}


/** Returns a widget from a name in a component, usually created by Glade.
 * Call it with the toplevel widget in the component (i.e. a window/dialog),
 * or alternatively any widget in the component, and the name of the widget
 * you want returned.
 * @param widget Widget with the @a widget_name property set.
 * @param widget_name Name to lookup.
 *
 * @return @transfer{none} The widget found.
 * @see ui_hookup_widget().
 *
 *  @since 0.16
 */
GEANY_API_SYMBOL
GtkWidget *ui_lookup_widget(GtkWidget *widget, const gchar *widget_name)
{
	GtkWidget *parent, *found_widget;

	g_return_val_if_fail(widget != NULL, NULL);
	g_return_val_if_fail(widget_name != NULL, NULL);

	for (;;)
	{
		if (GTK_IS_MENU(widget))
			parent = gtk_menu_get_attach_widget(GTK_MENU(widget));
		else
			parent = gtk_widget_get_parent(widget);
		if (parent == NULL)
			parent = (GtkWidget*) g_object_get_data(G_OBJECT(widget), "GladeParentKey");
		if (parent == NULL)
			break;
		widget = parent;
	}

	found_widget = (GtkWidget*) g_object_get_data(G_OBJECT(widget), widget_name);
	if (G_UNLIKELY(found_widget == NULL))
		g_warning("Widget not found: %s", widget_name);
	return found_widget;
}


/* wraps gtk_builder_get_object()
 * unlike ui_lookup_widget(), it does only support getting object created from the main
 * UI file, but it can fetch any object, not only widgets */
gpointer ui_builder_get_object (const gchar *name)
{
	return gtk_builder_get_object (builder, name);
}


/* Progress Bar */
static guint progress_bar_timer_id = 0;


static GtkWidget *progress_bar_create(void)
{
	GtkWidget *bar = gtk_progress_bar_new();

	/* Set the progressbar's height to 1 to fit it in the statusbar */
	gtk_widget_set_size_request(bar, -1, 1);
	gtk_box_pack_start (GTK_BOX(ui_widgets.statusbar), bar, FALSE, FALSE, 3);

	return bar;
}


static gboolean progress_bar_pulse(gpointer data)
{
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(main_widgets.progressbar));

	return TRUE;
}


/**
 * Starts a constantly pulsing progressbar in the right corner of the statusbar
 * (if the statusbar is visible). This is a convenience function which adds a timer to
 * pulse the progressbar constantly until ui_progress_bar_stop() is called.
 * You can use this function when you have time consuming asynchronous operation and want to
 * display some activity in the GUI and when you don't know about detailed progress steps.
 * The progressbar widget is hidden by default when it is not active. This function and
 * ui_progress_bar_stop() will show and hide it automatically for you.
 *
 * You can also access the progressbar widget directly using @c geany->main_widgets->progressbar
 * and use the GtkProgressBar API to set discrete fractions to display better progress information.
 * In this case, you need to show and hide the widget yourself. You can find some example code
 * in @c src/printing.c.
 *
 * @param text @nullable The text to be shown as the progress bar label or @c NULL to leave it empty.
 *
 *  @since 0.16
 **/
GEANY_API_SYMBOL
void ui_progress_bar_start(const gchar *text)
{
	g_return_if_fail(progress_bar_timer_id == 0);

	if (! interface_prefs.statusbar_visible)
		return;

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(main_widgets.progressbar), text);

	progress_bar_timer_id = g_timeout_add(200, progress_bar_pulse, NULL);

	gtk_widget_show(GTK_WIDGET(main_widgets.progressbar));
}


/** Stops a running progress bar and hides the widget again.
 *
 *  @since 0.16
 **/
GEANY_API_SYMBOL
void ui_progress_bar_stop(void)
{
	gtk_widget_hide(GTK_WIDGET(main_widgets.progressbar));

	if (progress_bar_timer_id != 0)
	{
		g_source_remove(progress_bar_timer_id);
		progress_bar_timer_id = 0;
	}
}


static gint compare_menu_item_labels(gconstpointer a, gconstpointer b)
{
	GtkMenuItem *item_a = GTK_MENU_ITEM(a);
	GtkMenuItem *item_b = GTK_MENU_ITEM(b);
	gchar *sa, *sb;
	gint result;

	/* put entries with submenus at the end of the menu */
	if (gtk_menu_item_get_submenu(item_a) && !gtk_menu_item_get_submenu(item_b))
		return 1;
	else if (!gtk_menu_item_get_submenu(item_a) && gtk_menu_item_get_submenu(item_b))
		return -1;

	sa = ui_menu_item_get_text(item_a);
	sb = ui_menu_item_get_text(item_b);
	result = utils_str_casecmp(sa, sb);
	g_free(sa);
	g_free(sb);
	return result;
}


/* Currently @a menu should contain only GtkMenuItems with labels. */
static void ui_menu_sort_by_label(GtkMenu *menu)
{
	GList *list = gtk_container_get_children(GTK_CONTAINER(menu));
	GList *node;
	gint pos;

	list = g_list_sort(list, compare_menu_item_labels);
	pos = 0;
	foreach_list(node, list)
	{
		menu_reorder_child(menu, node->data, pos);
		pos++;
	}
	g_list_free(list);
}


void ui_label_set_markup(GtkLabel *label, const gchar *format, ...)
{
	va_list a;
	gchar *text;

	va_start(a, format);
	text = g_strdup_vprintf(format, a);
	va_end(a);

	gtk_label_set_text(label, text);
	gtk_label_set_use_markup(label, TRUE);
	g_free(text);
}


GtkWidget *ui_label_new_bold(const gchar *text)
{
	GtkWidget *label;
	gchar *label_text;

	label_text = g_markup_escape_text(text, -1);
	label = gtk_label_new(NULL);
	ui_label_set_markup(GTK_LABEL(label), "<b>%s</b>", label_text);
	g_free(label_text);
	return label;
}


/** @girskip
 * Adds a list of document items to @a menu.
 * @param menu Menu.
 * @param active @nullable Which document to highlight, or @c NULL.
 * @param callback is used for each menu item's @c "activate" signal and will be
 * passed the corresponding document pointer as @c user_data.
 * @warning You should check @c doc->is_valid in the callback.
 * @since 0.19
 **/
GEANY_API_SYMBOL
void ui_menu_add_document_items(GtkMenu *menu, GeanyDocument *active, GCallback callback)
{
	ui_menu_add_document_items_sorted(menu, active, callback, NULL);
}


/** @girskip
 * Adds a list of document items to @a menu.
 *
 * @a compare_func might be NULL to not sort the documents in the menu. In this case,
 * the order of the document tabs is used.
 *
 * See document_compare_by_display_name() for an example sort function.
 *
 * @param menu Menu.
 * @param active @nullable Which document to highlight, or @c NULL.
 * @param callback is used for each menu item's @c "activate" signal and will be passed
 * the corresponding document pointer as @c user_data.
 * @param compare_func is used to sort the list. Might be @c NULL to not sort the list.
 * @warning You should check @c doc->is_valid in the callback.
 * @since 0.21
 **/
GEANY_API_SYMBOL
void ui_menu_add_document_items_sorted(GtkMenu *menu, GeanyDocument *active,
	GCallback callback, GCompareFunc compare_func)
{
	GtkWidget *menu_item, *menu_item_label, *image;
	GeanyDocument *doc;
	guint i, len;
	gchar *base_name, *label;
	GPtrArray *sorted_documents;

	len = (guint) gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));

	sorted_documents = g_ptr_array_sized_new(len);
	/* copy the documents_array into the new one */
	foreach_document(i)
	{
		g_ptr_array_add(sorted_documents, documents[i]);
	}
	if (compare_func == NULL)
		compare_func = document_compare_by_tab_order;

	/* and now sort it */
	g_ptr_array_sort(sorted_documents, compare_func);

	for (i = 0; i < sorted_documents->len; i++)
	{
		doc = g_ptr_array_index(sorted_documents, i);

		base_name = g_path_get_basename(DOC_FILENAME(doc));
		menu_item = gtk_image_menu_item_new_with_label(base_name);
		image = gtk_image_new_from_gicon(doc->file_type->icon, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);

		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		g_signal_connect(menu_item, "activate", callback, doc);

		menu_item_label = gtk_bin_get_child(GTK_BIN(menu_item));
		gtk_widget_set_name(menu_item_label, document_get_status_widget_class(doc));

		if (doc == active)
		{
			label = g_markup_escape_text(base_name, -1);
			ui_label_set_markup(GTK_LABEL(menu_item_label), "<b>%s</b>", label);
			g_free(label);
		}

		g_free(base_name);
	}
	g_ptr_array_free(sorted_documents, TRUE);
}


/** Checks whether the passed @a keyval is the Enter or Return key.
 * There are three different Enter/Return key values
 * (@c GDK_Return, @c GDK_ISO_Enter, @c GDK_KP_Enter).
 * This is just a convenience function.
 * @param keyval A keyval.
 * @return @c TRUE if @a keyval is the one of the Enter/Return key values, otherwise @c FALSE.
 * @since 0.19 */
GEANY_API_SYMBOL
gboolean ui_is_keyval_enter_or_return(guint keyval)
{
	return (keyval == GDK_Return || keyval == GDK_ISO_Enter|| keyval == GDK_KP_Enter);
}


/** Reads an integer from the GTK default settings registry
 * (see http://library.gnome.org/devel/gtk/stable/GtkSettings.html).
 * @param property_name The property to read.
 * @param default_value The default value in case the value could not be read.
 * @return The value for the property if it exists, otherwise the @a default_value.
 * @since 0.19 */
GEANY_API_SYMBOL
gint ui_get_gtk_settings_integer(const gchar *property_name, gint default_value)
{
	if (g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(
		gtk_settings_get_default())), property_name))
	{
		gint value;
		g_object_get(G_OBJECT(gtk_settings_get_default()), property_name, &value, NULL);
		return value;
	}
	else
		return default_value;
}


void ui_editable_insert_text_callback(GtkEditable *editable, gchar *new_text,
									  gint new_text_len, gint *position, gpointer data)
{
	gboolean first = position != NULL && *position == 0;
	gint i;

	if (new_text_len == -1)
		new_text_len = (gint) strlen(new_text);

	for (i = 0; i < new_text_len; i++, new_text++)
	{
		if ((!first || !strchr("+-", *new_text)) && !isdigit(*new_text))
		{
			g_signal_stop_emission_by_name(editable, "insert-text");
			break;
		}
		first = FALSE;
	}
}


/* gets the icon that applies to a particular MIME type */
GIcon *ui_get_mime_icon(const gchar *mime_type)
{
	GIcon *icon = NULL;
	gchar *ctype;

	ctype = g_content_type_from_mime_type(mime_type);
	if (ctype)
	{
		GdkScreen *screen = gdk_screen_get_default();

		icon = g_content_type_get_icon(ctype);
		if (screen && icon)
		{
			GtkIconInfo *icon_info;

			icon_info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_for_screen(screen), icon, 16, 0);
			if (!icon_info)
			{
				g_object_unref(icon);
				icon = NULL;
			}
			else
				gtk_icon_info_free(icon_info);
		}

		g_free(ctype);
	}

	/* fallback if icon lookup failed, like it might happen on Windows (?) */
	if (! icon)
	{
		const gchar *icon_name = "text-x-generic";

		if (strstr(mime_type, "directory"))
			icon_name = "folder";

		icon = g_themed_icon_new(icon_name);
	}
	return icon;
}


void ui_focus_current_document(void)
{
	GeanyDocument *doc = document_get_current();

	if (doc != NULL)
		document_grab_focus(doc);
}


/** Finds the label text associated with stock_id
 * @param stock_id stock_id to lookup e.g. @c GTK_STOCK_OPEN.
 * @return The label text for stock
 * @since Geany 1.22 */
GEANY_API_SYMBOL
const gchar *ui_lookup_stock_label(const gchar *stock_id)
{
	GtkStockItem item;

	if (gtk_stock_lookup(stock_id, &item))
		return item.label;

	g_warning("No stock id '%s'!", stock_id);
	return NULL;
}


/* finds the next iter at any level
 * @param iter in/out, the current iter, will be changed to the next one
 * @param down whether to try the child iter
 * @return TRUE if there @p iter was set, or FALSE if there is no next iter */
gboolean ui_tree_model_iter_any_next(GtkTreeModel *model, GtkTreeIter *iter, gboolean down)
{
	GtkTreeIter guess;
	GtkTreeIter copy = *iter;

	/* go down if the item has children */
	if (down && gtk_tree_model_iter_children(model, &guess, iter))
		*iter = guess;
	/* or to the next item at the same level */
	else if (gtk_tree_model_iter_next(model, &copy))
		*iter = copy;
	/* or to the next item at a parent level */
	else if (gtk_tree_model_iter_parent(model, &guess, iter))
	{
		copy = guess;
		while (TRUE)
		{
			if (gtk_tree_model_iter_next(model, &copy))
			{
				*iter = copy;
				return TRUE;
			}
			else if (gtk_tree_model_iter_parent(model, &copy, &guess))
				guess = copy;
			else
				return FALSE;
		}
	}
	else
		return FALSE;

	return TRUE;
}


GtkWidget *ui_create_encodings_combo_box(gboolean has_detect, gint default_enc)
{
	GtkCellRenderer *renderer;
	GtkTreeIter iter;
	GtkWidget *combo = gtk_combo_box_new();
	GtkTreeStore *store = encodings_encoding_store_new(has_detect);

	if (default_enc < 0 || default_enc >= GEANY_ENCODINGS_MAX)
		default_enc = has_detect ? GEANY_ENCODINGS_MAX : GEANY_ENCODING_NONE;

	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
	if (encodings_encoding_store_get_iter(store, &iter, default_enc))
		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo), &iter);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer,
			encodings_encoding_store_cell_data_func, NULL, NULL);

	return combo;
}


gint ui_encodings_combo_box_get_active_encoding(GtkComboBox *combo)
{
	GtkTreeIter iter;
	gint enc = GEANY_ENCODING_NONE;

	/* there should always be an active iter anyway, but we check just in case */
	if (gtk_combo_box_get_active_iter(combo, &iter))
	{
		GtkTreeModel *model = gtk_combo_box_get_model(combo);
		enc = encodings_encoding_store_get_encoding(GTK_TREE_STORE(model), &iter);
	}

	return enc;
}


gboolean ui_encodings_combo_box_set_active_encoding(GtkComboBox *combo, gint enc)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_combo_box_get_model(combo);

	if (encodings_encoding_store_get_iter(GTK_TREE_STORE(model), &iter, enc))
	{
		gtk_combo_box_set_active_iter(combo, &iter);
		return TRUE;
	}
	return FALSE;
}
