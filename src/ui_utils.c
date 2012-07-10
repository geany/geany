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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** @file ui_utils.h
 * User Interface general utility functions.
 */

#include "geany.h"

#include "support.h"

#include <string.h>
#include <ctype.h>
#include <gdk/gdkkeysyms.h>

#include "ui_utils.h"
#include "dialogs.h"
#include "prefs.h"
#include "sciwrappers.h"
#include "document.h"
#include "documentprivate.h"
#include "filetypes.h"
#include "support.h"
#include "msgwindow.h"
#include "utils.h"
#include "callbacks.h"
#include "encodings.h"
#include "images.c"
#include "sidebar.h"
#include "win32.h"
#include "project.h"
#include "editor.h"
#include "plugins.h"
#include "symbols.h"
#include "toolbar.h"
#include "geanymenubuttonaction.h"
#include "main.h"
#include "stash.h"
#include "keyfile.h"


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
	static glong last_time = 0;
	GTimeVal timeval;
	const gint GEANY_STATUS_TIMEOUT = 1;

	if (! interface_prefs.statusbar_visible)
		return; /* just do nothing if statusbar is not visible */

	g_get_current_time(&timeval);

	if (! allow_override)
	{
		gtk_statusbar_pop(GTK_STATUSBAR(ui_widgets.statusbar), 1);
		gtk_statusbar_push(GTK_STATUSBAR(ui_widgets.statusbar), 1, text);
		last_time = timeval.tv_sec;
	}
	else
	if (timeval.tv_sec > last_time + GEANY_STATUS_TIMEOUT)
	{
		gtk_statusbar_pop(GTK_STATUSBAR(ui_widgets.statusbar), 1);
		gtk_statusbar_push(GTK_STATUSBAR(ui_widgets.statusbar), 1, text);
	}
}


/** Displays text on the statusbar.
 * @param log Whether the message should be recorded in the Status window.
 * @param format A @c printf -style string. */
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


static gchar *statusbar_template = NULL;

/* note: some comments below are for translators */
static void add_statusbar_statistics(GString *stats_str,
		GeanyDocument *doc, guint line, guint col)
{
	const gchar *cur_tag;
	const gchar *fmt;
	const gchar *expos;	/* % expansion position */
	const gchar sp[] = "      ";
	ScintillaObject *sci = doc->editor->sci;

	fmt = NZV(statusbar_template) ? statusbar_template :
		/* Status bar statistics: col = column, sel = selection. */
		_("line: %l / %L\t col: %c\t sel: %s\t %w      %t      %m"
		"mode: %M      encoding: %e      filetype: %f      scope: %S");

	g_string_assign(stats_str, "");
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
				g_string_append_printf(stats_str, "%d", col);
				break;
			case 'C':
				g_string_append_printf(stats_str, "%d", col + 1);
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
				g_string_append(stats_str, editor_get_eol_char_name(doc->editor));
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
				symbols_get_current_function(doc, &cur_tag);
				g_string_append(stats_str, cur_tag);
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
}


/* updates the status bar document statistics */
void ui_update_statusbar(GeanyDocument *doc, gint pos)
{
	if (! interface_prefs.statusbar_visible)
		return; /* just do nothing if statusbar is not visible */

	if (doc == NULL)
		doc = document_get_current();

	if (doc != NULL)
	{
		static GString *stats_str = NULL;
		guint line, col;

		if (G_UNLIKELY(stats_str == NULL))
			stats_str = g_string_sized_new(120);

		if (pos == -1)
			pos = sci_get_current_position(doc->editor->sci);
		line = sci_get_line_from_position(doc->editor->sci, pos);

		/* Add temporary fix for sci infinite loop in Document::GetColumn(int)
		 * when current pos is beyond document end (can occur when removing
		 * blocks of selected lines especially esp. brace sections near end of file). */
		if (pos <= sci_get_length(doc->editor->sci))
			col = sci_get_col_from_position(doc->editor->sci, pos);
		else
			col = 0;

		add_statusbar_statistics(stats_str, doc, line, col);

#ifdef GEANY_DEBUG
	{
		const gchar sp[] = "      ";
		g_string_append(stats_str, sp);
		g_string_append_printf(stats_str, _("pos: %d"), pos);
		g_string_append(stats_str, sp);
		g_string_append_printf(stats_str, _("style: %d"), sci_get_style_at(doc->editor->sci, pos));
	}
#endif
		/* can be overridden by status messages */
		set_statusbar(stats_str->str, TRUE);
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


static void insert_include_items(GtkMenu *me, GtkMenu *mp, gchar **includes, gchar *label)
{
	guint i = 0;
	GtkWidget *tmp_menu;
	GtkWidget *tmp_popup;
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
		tmp_menu = gtk_menu_item_new_with_label(includes[i]);
		tmp_popup = gtk_menu_item_new_with_label(includes[i]);
		gtk_container_add(GTK_CONTAINER(edit_menu), tmp_menu);
		gtk_container_add(GTK_CONTAINER(popup_menu), tmp_popup);
		g_signal_connect(tmp_menu, "activate",
					G_CALLBACK(on_menu_insert_include_activate), (gpointer) includes[i]);
		g_signal_connect(tmp_popup, "activate",
					G_CALLBACK(on_insert_include_activate), (gpointer) includes[i]);
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
		"bitset", "dequev", "list", "map", "set", "queue", "stack", "vector", "algorithm",
		"iterator", "functional", "string", "complex", "valarray", NULL
	};

	blank = gtk_menu_item_new_with_label("#include \"...\"");
	gtk_container_add(GTK_CONTAINER(menu_edit), blank);
	gtk_widget_show(blank);
	g_signal_connect(blank, "activate", G_CALLBACK(on_menu_insert_include_activate),
																	(gpointer) "blank");
	blank = gtk_separator_menu_item_new ();
	gtk_container_add(GTK_CONTAINER(menu_edit), blank);
	gtk_widget_show(blank);

	blank = gtk_menu_item_new_with_label("#include \"...\"");
	gtk_container_add(GTK_CONTAINER(menu_popup), blank);
	gtk_widget_show(blank);
	g_signal_connect(blank, "activate", G_CALLBACK(on_insert_include_activate),
																	(gpointer) "blank");
	blank = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_popup), blank);
	gtk_widget_show(blank);

	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_stdlib, _("C Standard Library"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_c99, _("ISO C99"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_cpp, _("C++ (C Standard Library)"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_cppstdlib, _("C++ Standard Library"));
	insert_include_items(menu_edit, menu_popup, (gchar**) c_includes_stl, _("C++ STL"));
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
	g_signal_connect(item, "activate", G_CALLBACK(on_insert_date_activate), label);
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
	g_signal_connect(item, "activate", G_CALLBACK(on_insert_date_activate), str);
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
	add_doc_widget("preferences2");
	add_doc_widget("menu_reload1");
	add_doc_widget("menu_document1");
	add_doc_widget("menu_choose_color1");
	add_doc_widget("menu_zoom_in1");
	add_doc_widget("menu_zoom_out1");
	add_doc_widget("menu_view_editor1");
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
	static const GdkColor red   = {0, 0xffff, 0x6666, 0x6666};
	static const GdkColor white = {0, 0xffff, 0xffff, 0xffff};
	static gboolean old_value = TRUE;

	g_return_if_fail(widget != NULL);

	/* update only if really needed */
	if (old_value != success)
	{
		gtk_widget_modify_base(widget, GTK_STATE_NORMAL, success ? NULL : &red);
		gtk_widget_modify_text(widget, GTK_STATE_NORMAL, success ? NULL : &white);

		old_value = success;
	}
}


static gboolean have_tango_icon_theme(void)
{
	static gboolean result = FALSE;
	static gboolean checked = FALSE;

	if (! checked)
	{
		gchar *theme_name;

		g_object_get(G_OBJECT(gtk_settings_get_default()), "gtk-icon-theme-name", &theme_name, NULL);
		SETPTR(theme_name, g_utf8_strdown(theme_name, -1));

		result = (strstr(theme_name, "tango") != NULL);
		checked = TRUE;

		g_free(theme_name);
	}

	return result;
}


/* Note: remember to unref the pixbuf once an image or window has added a reference. */
GdkPixbuf *ui_new_pixbuf_from_inline(gint img)
{
	switch (img)
	{
		case GEANY_IMAGE_LOGO:
			return gdk_pixbuf_new_from_inline(-1, aladin_inline, FALSE, NULL);
			break;
		case GEANY_IMAGE_SAVE_ALL:
		{
			/* check whether the icon theme looks like a Gnome icon theme, if so use the
			 * old Gnome based Save All icon, otherwise assume a Tango-like icon theme */
			if (have_tango_icon_theme())
				return gdk_pixbuf_new_from_inline(-1, save_all_tango_inline, FALSE, NULL);
			else
				return gdk_pixbuf_new_from_inline(-1, save_all_gnome_inline, FALSE, NULL);
			break;
		}
		case GEANY_IMAGE_CLOSE_ALL:
		{
			return gdk_pixbuf_new_from_inline(-1, close_all_inline, FALSE, NULL);
			break;
		}
		case GEANY_IMAGE_BUILD:
		{
			return gdk_pixbuf_new_from_inline(-1, build_inline, FALSE, NULL);
			break;
		}
		default:
			return NULL;
	}
}


static GdkPixbuf *ui_new_pixbuf_from_stock(const gchar *stock_id)
{
	if (utils_str_equal(stock_id, GEANY_STOCK_CLOSE_ALL))
		return ui_new_pixbuf_from_inline(GEANY_IMAGE_CLOSE_ALL);
	else if (utils_str_equal(stock_id, GEANY_STOCK_BUILD))
		return ui_new_pixbuf_from_inline(GEANY_IMAGE_BUILD);
	else if (utils_str_equal(stock_id, GEANY_STOCK_SAVE_ALL))
		return ui_new_pixbuf_from_inline(GEANY_IMAGE_SAVE_ALL);

	return NULL;
}


GtkWidget *ui_new_image_from_inline(gint img)
{
	GtkWidget *wid;
	GdkPixbuf *pb;

	pb = ui_new_pixbuf_from_inline(img);
	wid = gtk_image_new_from_pixbuf(pb);
	g_object_unref(pb);	/* the image doesn't adopt our reference, so remove our ref. */
	return wid;
}


static void recent_create_menu(GeanyRecentFiles *grf)
{
	GtkWidget *tmp;
	guint i, len;
	gchar *filename;

	len = MIN(file_prefs.mru_length, g_queue_get_length(grf->recent_queue));
	for (i = 0; i < len; i++)
	{
		filename = g_queue_peek_nth(grf->recent_queue, i);
		/* create menu item for the recent files menu in the menu bar */
		tmp = gtk_menu_item_new_with_label(filename);
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


static void recent_file_loaded(const gchar *utf8_filename, GeanyRecentFiles *grf)
{
	GList *item, *children;
	void *data;
	GtkWidget *tmp;

	/* first reorder the queue */
	item = g_queue_find_custom(grf->recent_queue, utf8_filename, (GCompareFunc) strcmp);
	g_return_if_fail(item != NULL);

	data = item->data;
	g_queue_remove(grf->recent_queue, data);
	g_queue_push_head(grf->recent_queue, data);

	/* remove the old menuitem for the filename */
	children = gtk_container_get_children(GTK_CONTAINER(grf->menubar));
	item = g_list_find_custom(children, utf8_filename, (GCompareFunc) find_recent_file_item);
	if (item != NULL)
		gtk_widget_destroy(GTK_WIDGET(item->data));
	g_list_free(children);

	if (grf->toolbar != NULL)
	{
		children = gtk_container_get_children(GTK_CONTAINER(grf->toolbar));
		item = g_list_find_custom(children, utf8_filename, (GCompareFunc) find_recent_file_item);
		if (item != NULL)
			gtk_widget_destroy(GTK_WIDGET(item->data));
		g_list_free(children);
	}
	/* now prepend a new menuitem for the filename,
	 * first for the recent files menu in the menu bar */
	tmp = gtk_menu_item_new_with_label(utf8_filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(grf->menubar), tmp);
	g_signal_connect(tmp, "activate", G_CALLBACK(grf->activate_cb), NULL);
	/* then for the recent files menu in the tool bar */
	if (grf->toolbar != NULL)
	{
		tmp = gtk_menu_item_new_with_label(utf8_filename);
		gtk_widget_show(tmp);
		gtk_container_add(GTK_CONTAINER(grf->toolbar), tmp);
		/* this is a bit ugly, but we need to use gtk_container_add(). Using
		 * gtk_menu_shell_prepend() doesn't emit GtkContainer's "add" signal which we need in
		 * GeanyMenubuttonAction */
		gtk_menu_reorder_child(GTK_MENU(grf->toolbar), tmp, 0);
		g_signal_connect(tmp, "activate", G_CALLBACK(grf->activate_cb), NULL);
	}
}


static void update_recent_menu(GeanyRecentFiles *grf)
{
	GtkWidget *tmp;
	gchar *filename;
	GList *children, *item;

	filename = g_queue_peek_head(grf->recent_queue);

	/* clean the MRU list before adding an item (menubar) */
	children = gtk_container_get_children(GTK_CONTAINER(grf->menubar));
	if (g_list_length(children) > file_prefs.mru_length - 1)
	{
		item = g_list_nth(children, file_prefs.mru_length - 1);
		while (item != NULL)
		{
			if (GTK_IS_MENU_ITEM(item->data))
				gtk_widget_destroy(GTK_WIDGET(item->data));
			item = g_list_next(item);
		}
	}
	g_list_free(children);

	/* create item for the menu bar menu */
	tmp = gtk_menu_item_new_with_label(filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(grf->menubar), tmp);
	g_signal_connect(tmp, "activate", G_CALLBACK(grf->activate_cb), NULL);

	/* clean the MRU list before adding an item (toolbar) */
	if (grf->toolbar != NULL)
	{
		children = gtk_container_get_children(GTK_CONTAINER(grf->toolbar));
		if (g_list_length(children) > file_prefs.mru_length - 1)
		{
			item = g_list_nth(children, file_prefs.mru_length - 1);
			while (item != NULL)
			{
				if (GTK_IS_MENU_ITEM(item->data))
					gtk_widget_destroy(GTK_WIDGET(item->data));
				item = g_list_next(item);
			}
		}
		g_list_free(children);

		/* create item for the tool bar menu */
		tmp = gtk_menu_item_new_with_label(filename);
		gtk_widget_show(tmp);
		gtk_container_add(GTK_CONTAINER(grf->toolbar), tmp);
		gtk_menu_reorder_child(GTK_MENU(grf->toolbar), tmp, 0);
		g_signal_connect(tmp, "activate", G_CALLBACK(grf->activate_cb), NULL);
	}
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
				sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin, 0);
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
 * @return The frame widget, setting the alignment container for packing child widgets. */
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
 * @return The packed @c GtkVBox. */
GtkWidget *ui_dialog_vbox_new(GtkDialog *dialog)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 12);	/* need child vbox to set a separate border. */

	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	return vbox;
}


/** Creates a @c GtkButton with custom text and a stock image similar to
 * @c gtk_button_new_from_stock().
 * @param stock_id A @c GTK_STOCK_NAME string.
 * @param text Button label text, can include mnemonics.
 * @return The new @c GtkButton.
 */
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
 * @return The new @c GtkImageMenuItem.
 *
 *  @since 0.16
 */
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
 * @param text Text to add, or @c NULL for current entry text.
 * @param history_len Max number of items, or @c 0 for default. */
void ui_combo_box_add_to_history(GtkComboBoxEntry *combo_entry,
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
	gtk_combo_box_prepend_text(combo, text);

	/* limit history */
	path = gtk_tree_path_new_from_indices(history_len, -1);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}
	gtk_tree_path_free(path);
}


/* Same as gtk_combo_box_prepend_text(), except that text is only prepended if it not already
 * exists in the combo's model. */
void ui_combo_box_prepend_text_once(GtkComboBox *combo, const gchar *text)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model(combo);
	if (tree_model_find_text(model, &iter, 0, text))
		return;	/* don't prepend duplicate */

	gtk_combo_box_prepend_text(combo, text);
}


/* Changes the color of the notebook tab text and open files items according to
 * document status. */
void ui_update_tab_status(GeanyDocument *doc)
{
	const GdkColor *color = document_get_status_color(doc);

	/* NULL color will reset to default */
	gtk_widget_modify_fg(doc->priv->tab_label, GTK_STATE_NORMAL, color);
	gtk_widget_modify_fg(doc->priv->tab_label, GTK_STATE_ACTIVE, color);

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


/**
 * Modifies the font of a widget using gtk_widget_modify_font().
 *
 * @param widget The widget.
 * @param str The font name as expected by pango_font_description_from_string().
 */
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
 * @param title The file chooser dialog title, or @c NULL.
 * @param action The mode of the file chooser.
 * @param entry Can be an unpacked @c GtkEntry, or the child of an unpacked widget,
 * such as @c GtkComboBoxEntry.
 * @return The @c GtkHBox.
 */
/* @see ui_setup_open_button_callback(). */
GtkWidget *ui_path_box_new(const gchar *title, GtkFileChooserAction action, GtkEntry *entry)
{
	GtkWidget *vbox, *dirbtn, *openimg, *hbox, *path_entry;

	hbox = gtk_hbox_new(FALSE, 6);
	path_entry = GTK_WIDGET(entry);

	/* prevent path_entry being vertically stretched to the height of dirbtn */
	vbox = gtk_vbox_new(FALSE, 0);
	if (gtk_widget_get_parent(path_entry))	/* entry->parent may be a GtkComboBoxEntry */
	{
		GtkWidget *parent = gtk_widget_get_parent(path_entry);

		gtk_box_pack_start(GTK_BOX(vbox), parent, TRUE, FALSE, 0);
	}
	else
		gtk_box_pack_start(GTK_BOX(vbox), path_entry, TRUE, FALSE, 0);

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
	ui_hookup_widget(open_btn, path_entry, "entry");
	g_signal_connect(open_btn, "clicked", G_CALLBACK(ui_path_box_open_clicked), open_btn);
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
	GtkWidget *path_box = GTK_WIDGET(user_data);
	GtkFileChooserAction action = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(path_box), "action"));
	GtkEntry *entry = g_object_get_data(G_OBJECT(path_box), "entry");
	const gchar *title = g_object_get_data(G_OBJECT(path_box), "title");
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

	/* sort menu after all items added */
	g_idle_add(sort_menu, widgets.config_files_menu);
}


void ui_init_stock_items(void)
{
	GtkIconSet *icon_set;
	GtkIconFactory *factory = gtk_icon_factory_new();
	GdkPixbuf *pb;
	guint i, len;
	GtkStockItem items[] =
	{
		{ GEANY_STOCK_SAVE_ALL, N_("Save All"), 0, 0, GETTEXT_PACKAGE },
		{ GEANY_STOCK_CLOSE_ALL, N_("Close All"), 0, 0, GETTEXT_PACKAGE },
		{ GEANY_STOCK_BUILD, N_("Build"), 0, 0, GETTEXT_PACKAGE }
	};

	len = G_N_ELEMENTS(items);
	for (i = 0; i < len; i++)
	{
		pb = ui_new_pixbuf_from_stock(items[i].stock_id);
		icon_set = gtk_icon_set_new_from_pixbuf(pb);

		gtk_icon_factory_add(factory, items[i].stock_id, icon_set);

		gtk_icon_set_unref(icon_set);
		g_object_unref(pb);
	}
	gtk_stock_add((GtkStockItem *) items, len);
	gtk_icon_factory_add_default(factory);
	g_object_unref(factory);
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

	gtk_paned_set_position(GTK_PANED(pane), pane->allocation.width
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
	stash_group_add_string(group, &statusbar_template,
		"statusbar_template", "");
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

	gtk_builder_connect_signals(builder, NULL);

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


void ui_init(void)
{
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


void ui_finalize(void)
{
	g_free(statusbar_template);
}


static void auto_separator_update(GeanyAutoSeparator *autosep)
{
	g_return_if_fail(autosep->ref_count >= 0);

	if (autosep->widget)
		ui_widget_show_hide(autosep->widget, autosep->ref_count > 0);
}


static void on_auto_separator_item_show_hide(GtkWidget *widget, gpointer user_data)
{
	GeanyAutoSeparator *autosep = user_data;

	if (GTK_WIDGET_VISIBLE(widget))
		autosep->ref_count++;
	else
		autosep->ref_count--;
	auto_separator_update(autosep);
}


static void on_auto_separator_item_destroy(GtkWidget *widget, gpointer user_data)
{
	GeanyAutoSeparator *autosep = user_data;

	/* GTK_WIDGET_VISIBLE won't work now the widget is being destroyed,
	 * so assume widget was visible */
	autosep->ref_count--;
	autosep->ref_count = MAX(autosep->ref_count, 0);
	auto_separator_update(autosep);
}


/* Show the separator widget if @a item or another is visible. */
/* Note: This would be neater taking a widget argument, setting a "visible-count"
 * property, and using reference counting to keep the widget alive whilst its visible group
 * is alive. */
void ui_auto_separator_add_ref(GeanyAutoSeparator *autosep, GtkWidget *item)
{
	/* set widget ptr NULL when widget destroyed */
	if (autosep->ref_count == 0)
		g_signal_connect(autosep->widget, "destroy",
			G_CALLBACK(gtk_widget_destroyed), &autosep->widget);

	if (GTK_WIDGET_VISIBLE(item))
	{
		autosep->ref_count++;
		auto_separator_update(autosep);
	}
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
 * @return The widget found.
 * @see ui_hookup_widget().
 *
 *  @since 0.16
 */
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
			parent = widget->parent;
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
 * @param text The text to be shown as the progress bar label or NULL to leave it empty.
 *
 *  @since 0.16
 **/
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

	sa = ui_menu_item_get_text(item_a);
	sb = ui_menu_item_get_text(item_b);
	result = utils_str_casecmp(sa, sb);
	g_free(sa);
	g_free(sb);
	return result;
}


/* Currently @a menu should contain only GtkMenuItems with labels. */
void ui_menu_sort_by_label(GtkMenu *menu)
{
	GList *list = gtk_container_get_children(GTK_CONTAINER(menu));
	GList *node;
	gint pos;

	list = g_list_sort(list, compare_menu_item_labels);
	pos = 0;
	foreach_list(node, list)
	{
		gtk_menu_reorder_child(menu, node->data, pos);
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


/** Adds a list of document items to @a menu.
 * @param menu Menu.
 * @param active Which document to highlight, or @c NULL.
 * @param callback is used for each menu item's @c "activate" signal and will be passed
 * the corresponding document pointer as @c user_data.
 * @warning You should check @c doc->is_valid in the callback.
 * @since 0.19 */
void ui_menu_add_document_items(GtkMenu *menu, GeanyDocument *active, GCallback callback)
{
	ui_menu_add_document_items_sorted(menu, active, callback, NULL);
}


/** Adds a list of document items to @a menu.
 *
 * @a compare_func might be NULL to not sort the documents in the menu. In this case,
 * the order of the document tabs is used.
 *
 * See document_sort_by_display_name() for an example sort function.
 *
 * @param menu Menu.
 * @param active Which document to highlight, or @c NULL.
 * @param callback is used for each menu item's @c "activate" signal and will be passed
 * the corresponding document pointer as @c user_data.
 * @param compare_func is used to sort the list. Might be @c NULL to not sort the list.
 * @warning You should check @c doc->is_valid in the callback.
 * @since 0.21 */
void ui_menu_add_document_items_sorted(GtkMenu *menu, GeanyDocument *active,
	GCallback callback, GCompareFunc compare_func)
{
	GtkWidget *menu_item, *menu_item_label, *image;
	const GdkColor *color;
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
		image = gtk_image_new_from_pixbuf(doc->file_type->icon);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);

		gtk_widget_show(menu_item);
		gtk_container_add(GTK_CONTAINER(menu), menu_item);
		g_signal_connect(menu_item, "activate", callback, doc);

		color = document_get_status_color(doc);
		menu_item_label = gtk_bin_get_child(GTK_BIN(menu_item));
		gtk_widget_modify_fg(menu_item_label, GTK_STATE_NORMAL, color);
		gtk_widget_modify_fg(menu_item_label, GTK_STATE_ACTIVE, color);

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
GdkPixbuf *ui_get_mime_icon(const gchar *mime_type, GtkIconSize size)
{
	GdkPixbuf *icon = NULL;
	gchar *ctype;
	GIcon *gicon;
	GtkIconInfo *info;
	GtkIconTheme *theme;
	gint real_size;

	if (!gtk_icon_size_lookup(size, &real_size, NULL))
	{
		g_return_val_if_reached(NULL);
	}
	ctype = g_content_type_from_mime_type(mime_type);
	if (ctype != NULL)
	{
		gicon = g_content_type_get_icon(ctype);
		theme = gtk_icon_theme_get_default();
		info = gtk_icon_theme_lookup_by_gicon(theme, gicon, real_size, 0);
		g_object_unref(gicon);
		g_free(ctype);

		if (info != NULL)
		{
			icon = gtk_icon_info_load_icon(info, NULL);
			gtk_icon_info_free(info);
		}
	}

	/* fallback for builds with GIO < 2.18 or if icon lookup failed, like it might happen on Windows */
	if (icon == NULL)
	{
		const gchar *stock_id = GTK_STOCK_FILE;
		GtkIconSet *icon_set;

		if (strstr(mime_type, "directory"))
			stock_id = GTK_STOCK_DIRECTORY;

		icon_set = gtk_icon_factory_lookup_default(stock_id);
		if (icon_set)
		{
			icon = gtk_icon_set_render_icon(icon_set, gtk_widget_get_default_style(),
				gtk_widget_get_default_direction(),
				GTK_STATE_NORMAL, size, NULL, NULL);
		}
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
const gchar *ui_lookup_stock_label(const gchar *stock_id)
{
	GtkStockItem item;

	if (gtk_stock_lookup(stock_id, &item))
		return item.label;

	g_warning("No stock id '%s'!", stock_id);
	return NULL;
}
