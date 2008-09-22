/*
 *      ui_utils.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *
 * $Id$
 */

/** @file ui_utils.h
 * User Interface general utility functions.
 */

#include "geany.h"

#include <string.h>

#include "ui_utils.h"
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
#include "treeviews.h"
#include "win32.h"
#include "project.h"
#include "editor.h"
#include "plugins.h"
#include "symbols.h"


GeanyInterfacePrefs	interface_prefs;
GeanyToolbarPrefs	toolbar_prefs;
GeanyMainWidgets	main_widgets;

UIPrefs			ui_prefs;
UIWidgets		ui_widgets;

static struct
{
	/* pointers to widgets only sensitive when there is at least one document */
	GPtrArray	*document_buttons;
	GtkWidget	*menu_insert_include_items[2];
	GtkWidget	*popup_goto_items[4];
	GtkWidget	*popup_copy_items[3];
	GtkWidget	*menu_copy_items[3];
	GtkWidget	*redo_items[3];
	GtkWidget	*undo_items[3];
	GtkWidget	*save_buttons[4];
}
widgets;


static void update_recent_menu(void);
static void recent_file_loaded(const gchar *utf8_filename);
static void
recent_file_activate_cb                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


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


/** Display text on the statusbar.
 * @param log Whether the message should be recorded in the Status window.
 * @param format A @c printf -style string. */
void ui_set_statusbar(gboolean log, const gchar *format, ...)
{
	gchar string[512];
	va_list args;

	va_start(args, format);
	g_vsnprintf(string, 512, format, args);
	va_end(args);

	if (! prefs.suppress_status_messages)
		set_statusbar(string, FALSE);

	if (log || prefs.suppress_status_messages)
		msgwin_status_add("%s", string);
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
		const gchar sp[] = "      ";
		guint line, col;
		const gchar *cur_tag;
		gchar *filetype_name;

		/* workaround to make the name of filetype GEANY_FILETYPES_NONE translatable */
		if (doc->file_type == NULL || doc->file_type->id == GEANY_FILETYPES_NONE)
			filetype_name = _("None");
		else
			filetype_name = doc->file_type->name;

		if (stats_str == NULL)
			stats_str = g_string_sized_new(120);

		if (pos == -1) pos = sci_get_current_position(doc->editor->sci);
		line = sci_get_line_from_position(doc->editor->sci, pos);

		/* Add temporary fix for sci infinite loop in Document::GetColumn(int)
		 * when current pos is beyond document end (can occur when removing
		 * blocks of selected lines especially esp. brace sections near end of file). */
		if (pos <= sci_get_length(doc->editor->sci))
			col = sci_get_col_from_position(doc->editor->sci, pos);
		else
			col = 0;

		/* Status bar statistics: col = column, sel = selection. */
		g_string_printf(stats_str, _("line: %d\t col: %d\t sel: %d\t "),
			(line + 1), col,
			sci_get_selected_text_length(doc->editor->sci) - 1);

		g_string_append(stats_str,
			/* RO = read-only */
			(doc->readonly) ? _("RO ") :
				/* OVR = overwrite/overtype, INS = insert */
				(sci_get_overtype(doc->editor->sci) ? _("OVR") : _("INS")));
		g_string_append(stats_str, sp);

		switch (editor_get_indent_prefs(doc->editor)->type)
		{
			case GEANY_INDENT_TYPE_TABS:
				g_string_append(stats_str, _("TAB"));
				break;
			case GEANY_INDENT_TYPE_SPACES:
				g_string_append(stats_str, _("SP"));	/* SP = space */
				break;
			case GEANY_INDENT_TYPE_BOTH:
				g_string_append(stats_str, _("T/S"));	/* T/S = tabs and spaces */
				break;
		}
		g_string_append(stats_str, sp);
		g_string_append_printf(stats_str, _("mode: %s"),
			editor_get_eol_char_name(doc));
		g_string_append(stats_str, sp);
		g_string_append_printf(stats_str, _("encoding: %s %s"),
			(doc->encoding) ? doc->encoding : _("unknown"),
			(encodings_is_unicode_charset(doc->encoding)) ?
				/* BOM = byte order mark */
				((doc->has_bom) ? _("(with BOM)") : "") : "");
		g_string_append(stats_str, sp);
		g_string_append_printf(stats_str, _("filetype: %s"), filetype_name);
		g_string_append(stats_str, sp);
		if (doc->changed)
		{
			g_string_append(stats_str, _("MOD"));	/* MOD = modified */
			g_string_append(stats_str, sp);
		}

		symbols_get_current_function(doc, &cur_tag);
		g_string_append_printf(stats_str, _("scope: %s"),
			cur_tag);

#ifdef GEANY_DEBUG
		g_string_append(stats_str, sp);
		g_string_append_printf(stats_str, "pos: %d", pos);
		g_string_append(stats_str, sp);
		g_string_append_printf(stats_str, "style: %d", sci_get_style_at(doc->editor->sci, pos));
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
			gchar *base_name = g_path_get_basename(DOC_FILENAME(doc));
			gchar *dirname = g_path_get_dirname(DOC_FILENAME(doc));

			g_string_append(str, base_name);
			g_string_append(str, " - ");
			g_string_append(str, dirname ? dirname : "");
			g_free(base_name);
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
	gtk_window_set_title(GTK_WINDOW(main_widgets.window), str->str);
	g_string_free(str, TRUE);
}


void ui_set_editor_font(const gchar *font_name)
{
	guint i;

	g_return_if_fail(font_name != NULL);

	/* do nothing if font has not changed */
	if (interface_prefs.editor_font != NULL)
		if (strcmp(font_name, interface_prefs.editor_font) == 0) return;

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
	gtk_widget_set_sensitive(widgets.undo_items[0], enable_undo);
	gtk_widget_set_sensitive(widgets.undo_items[1], enable_undo);
	gtk_widget_set_sensitive(widgets.undo_items[2], enable_undo);

	gtk_widget_set_sensitive(widgets.redo_items[0], enable_redo);
	gtk_widget_set_sensitive(widgets.redo_items[1], enable_redo);
	gtk_widget_set_sensitive(widgets.redo_items[2], enable_redo);
}


void ui_update_popup_copy_items(GeanyDocument *doc)
{
	gboolean enable;
	guint i;

	if (doc == NULL)
		enable = FALSE;
	else
		enable = sci_has_selection(doc->editor->sci);

	for (i = 0; i < G_N_ELEMENTS(widgets.popup_copy_items); i++)
		gtk_widget_set_sensitive(widgets.popup_copy_items[i], enable);
}


void ui_update_popup_goto_items(gboolean enable)
{
	gtk_widget_set_sensitive(widgets.popup_goto_items[0], enable);
	gtk_widget_set_sensitive(widgets.popup_goto_items[1], enable);
	gtk_widget_set_sensitive(widgets.popup_goto_items[2], enable);
	gtk_widget_set_sensitive(widgets.popup_goto_items[3], enable);
}


void ui_update_menu_copy_items(GeanyDocument *doc)
{
	gboolean enable = FALSE;
	guint i;
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

	for (i = 0; i < G_N_ELEMENTS(widgets.menu_copy_items); i++)
		gtk_widget_set_sensitive(widgets.menu_copy_items[i], enable);
}


void ui_update_insert_include_item(GeanyDocument *doc, gint item)
{
	gboolean enable = FALSE;

	if (doc == NULL || doc->file_type == NULL)
		enable = FALSE;
	else if (doc->file_type->id == GEANY_FILETYPES_C ||
			 doc->file_type->id == GEANY_FILETYPES_CPP)
	{
		enable = TRUE;
	}
	gtk_widget_set_sensitive(widgets.menu_insert_include_items[item], enable);
}


void ui_update_fold_items(void)
{
	ui_widget_show_hide(lookup_widget(main_widgets.window, "menu_fold_all1"), editor_prefs.folding);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "menu_unfold_all1"), editor_prefs.folding);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separator22"), editor_prefs.folding);
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
	GtkMenu *menu_edit = GTK_MENU(lookup_widget(main_widgets.window, "insert_include2_menu"));
	GtkMenu *menu_popup = GTK_MENU(lookup_widget(main_widgets.editor_menu, "insert_include1_menu"));
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
	GtkMenu *menu_edit = GTK_MENU(lookup_widget(main_widgets.window, "insert_date1_menu"));
	GtkMenu *menu_popup = GTK_MENU(lookup_widget(main_widgets.editor_menu, "insert_date2_menu"));
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
	g_object_set_data_full(G_OBJECT(main_widgets.window),
		"insert_date_custom1", gtk_widget_ref(item), (GDestroyNotify)gtk_widget_unref);

	item = gtk_menu_item_new_with_mnemonic(str);
	gtk_container_add(GTK_CONTAINER(menu_popup), item);
	gtk_widget_show(item);
	g_signal_connect(item, "activate", G_CALLBACK(on_insert_date_activate), str);
	g_object_set_data_full(G_OBJECT(main_widgets.editor_menu),
		"insert_date_custom2", gtk_widget_ref(item), (GDestroyNotify)gtk_widget_unref);

	insert_date_items(menu_edit, menu_popup, _("_Set Custom Date Format"));
}


void ui_save_buttons_toggle(gboolean enable)
{
	guint i;
	gboolean dirty_tabs = FALSE;

	gtk_widget_set_sensitive(widgets.save_buttons[0], enable);
	gtk_widget_set_sensitive(widgets.save_buttons[1], enable);

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

	gtk_widget_set_sensitive(widgets.save_buttons[2], dirty_tabs);
	gtk_widget_set_sensitive(widgets.save_buttons[3], dirty_tabs);
}


#define add_doc_widget(widget_name) \
	g_ptr_array_add(widgets.document_buttons, lookup_widget(main_widgets.window, widget_name))

static void init_document_widgets(void)
{
	widgets.document_buttons = g_ptr_array_new();

	/* Cache the document-sensitive widgets so we don't have to keep looking them up
	 * when using ui_document_buttons_update(). */
	add_doc_widget("menu_close1");
	add_doc_widget("close_other_documents1");
	add_doc_widget("toolbutton_close");
	add_doc_widget("menu_change_font1");
	add_doc_widget("toolbutton_search_entry");
	add_doc_widget("toolbutton_search");
	add_doc_widget("toolbutton_zoomin");
	add_doc_widget("toolbutton_zoomout");
	add_doc_widget("menu_close_all1");
	add_doc_widget("menu_save_all1");
	add_doc_widget("toolbutton_saveall");
	add_doc_widget("toolbutton_compile"); /* compile_button */
	add_doc_widget("menu_save_as1");
	add_doc_widget("toolbutton_reload");
	add_doc_widget("menu_count_words1");
	add_doc_widget("menu_build1");
	add_doc_widget("add_comments1");
	add_doc_widget("menu_paste1");
	add_doc_widget("menu_undo2");
	add_doc_widget("preferences2");
	add_doc_widget("menu_reload1");
	add_doc_widget("menu_document1");
	add_doc_widget("menu_markers_margin1");
	add_doc_widget("menu_linenumber_margin1");
	add_doc_widget("menu_choose_color1");
	add_doc_widget("menu_zoom_in1");
	add_doc_widget("menu_zoom_out1");
	add_doc_widget("normal_size1");
	add_doc_widget("toolbutton_colour");
	add_doc_widget("toolbutton_goto");
	add_doc_widget("toolbutton_goto_entry");
	add_doc_widget("treeview6");
	add_doc_widget("print1");
	add_doc_widget("menu_reload_as1");
	add_doc_widget("menu_select_all1");
	add_doc_widget("insert_date1");
	add_doc_widget("menu_format1");
	add_doc_widget("menu_open_selected_file1");
	add_doc_widget("page_setup1");
	add_doc_widget("find1");
	add_doc_widget("find_next1");
	add_doc_widget("find_previous1");
	add_doc_widget("replace1");
	add_doc_widget("find_nextsel1");
	add_doc_widget("find_prevsel1");
	add_doc_widget("go_to_line1");
}


void ui_document_buttons_update(void)
{
	guint i;
	gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) ? TRUE : FALSE;

	for (i = 0; i < widgets.document_buttons->len; i++)
	{
		GtkWidget *widget = g_ptr_array_index(widgets.document_buttons, i);
		gtk_widget_set_sensitive(widget, enable);
	}

#ifdef HAVE_PLUGINS
	plugins_update_document_sensitive(enable);
#endif
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

	widget = lookup_widget(main_widgets.window, "menu_show_sidebar1");
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
	gchar *widget_name;
	GtkWidget *item;
	const GeanyIndentPrefs *iprefs;

	if (doc == NULL)
		doc = document_get_current();

	if (doc == NULL)
		return;

	ignore_callback = TRUE;

	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(lookup_widget(main_widgets.window, "menu_line_wrapping1")),
			doc->editor->line_wrapping);

	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(lookup_widget(main_widgets.window, "line_breaking1")),
			doc->editor->line_breaking);

	iprefs = editor_get_indent_prefs(doc->editor);

	item = lookup_widget(main_widgets.window, "menu_use_auto_indentation1");
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
	item = lookup_widget(main_widgets.window, widget_name);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);

	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(lookup_widget(main_widgets.window, "set_file_readonly1")),
			doc->readonly);

	item = lookup_widget(main_widgets.window, "menu_write_unicode_bom1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), doc->has_bom);
	gtk_widget_set_sensitive(item, encodings_is_unicode_charset(doc->encoding));

	switch (sci_get_eol_mode(doc->editor->sci))
	{
		case SC_EOL_CR: widget_name = "cr"; break;
		case SC_EOL_LF: widget_name = "lf"; break;
		default: widget_name = "crlf"; break;
	}
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(lookup_widget(main_widgets.window, widget_name)), TRUE);

	encodings_select_radio_item(doc->encoding);
	filetypes_select_radio_item(doc->file_type);

	ignore_callback = FALSE;
}


void ui_update_toolbar_icons(GtkIconSize size)
{
	GtkWidget *button_image = NULL;
	GtkWidget *widget = NULL;
	GtkWidget *oldwidget = NULL;

	/* destroy old widget */
	widget = lookup_widget(main_widgets.window, "toolbutton_saveall");
	oldwidget = gtk_tool_button_get_icon_widget(GTK_TOOL_BUTTON(widget));
	if (oldwidget && GTK_IS_WIDGET(oldwidget)) gtk_widget_destroy(oldwidget);
	/* create new widget */
	button_image = ui_new_image_from_inline(GEANY_IMAGE_SAVE_ALL, FALSE);
	gtk_widget_show(button_image);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(widget), button_image);

	gtk_toolbar_set_icon_size(GTK_TOOLBAR(main_widgets.toolbar), size);
}


void ui_update_toolbar_items(void)
{
	/* show toolbar */
	GtkWidget *widget = lookup_widget(main_widgets.window, "menu_show_toolbar1");
	if (toolbar_prefs.visible && ! gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	{	/* will be changed by the toggled callback */
		toolbar_prefs.visible = ! toolbar_prefs.visible;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), TRUE);
	}
	else if (! toolbar_prefs.visible && gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	{	/* will be changed by the toggled callback */
		toolbar_prefs.visible = ! toolbar_prefs.visible;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), FALSE);
	}

	/* fileops */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_new"), toolbar_prefs.show_fileops);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_open"), toolbar_prefs.show_fileops);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_save"), toolbar_prefs.show_fileops);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_saveall"), toolbar_prefs.show_fileops);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_reload"), toolbar_prefs.show_fileops);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_close"), toolbar_prefs.show_fileops);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem7"), toolbar_prefs.show_fileops);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem2"), toolbar_prefs.show_fileops);
	/* search */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_search_entry"), toolbar_prefs.show_search);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_search"), toolbar_prefs.show_search);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem5"), toolbar_prefs.show_search);
	/* goto line */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_goto_entry"), toolbar_prefs.show_goto);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_goto"), toolbar_prefs.show_goto);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem8"), toolbar_prefs.show_goto);
	/* compile */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_compile"), toolbar_prefs.show_compile);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_run"), toolbar_prefs.show_compile);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem6"), toolbar_prefs.show_compile);
	/* colour */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_colour"), toolbar_prefs.show_colour);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem3"), toolbar_prefs.show_colour);
	/* zoom */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_zoomin"), toolbar_prefs.show_zoom);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_zoomout"), toolbar_prefs.show_zoom);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem4"), toolbar_prefs.show_zoom);
	/* indent */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_indent_dec"), toolbar_prefs.show_indent);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_indent_inc"), toolbar_prefs.show_indent);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem11"), toolbar_prefs.show_indent);
	/* undo */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_undo"), toolbar_prefs.show_undo);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_redo"), toolbar_prefs.show_undo);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem9"), toolbar_prefs.show_undo);
	/* navigation */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_back"), toolbar_prefs.show_navigation);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_forward"), toolbar_prefs.show_navigation);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem10"), toolbar_prefs.show_navigation);
	/* quit */
	ui_widget_show_hide(lookup_widget(main_widgets.window, "toolbutton_quit"), toolbar_prefs.show_quit);
	ui_widget_show_hide(lookup_widget(main_widgets.window, "separatortoolitem8"), toolbar_prefs.show_quit);
}


/* Note: remember to unref the pixbuf once an image or window has added a reference. */
GdkPixbuf *ui_new_pixbuf_from_inline(gint img, gboolean small_img)
{
	switch(img)
	{
		case GEANY_IMAGE_SMALL_CROSS: return gdk_pixbuf_new_from_inline(-1, close_small_inline, FALSE, NULL); break;
		case GEANY_IMAGE_LOGO: return gdk_pixbuf_new_from_inline(-1, aladin_inline, FALSE, NULL); break;
		case GEANY_IMAGE_SAVE_ALL:
		{
			if ((toolbar_prefs.icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR) || small_img)
			{
				return gdk_pixbuf_scale_simple(gdk_pixbuf_new_from_inline(-1, save_all_inline, FALSE, NULL),
                                             16, 16, GDK_INTERP_HYPER);
			}
			else
			{
				return gdk_pixbuf_new_from_inline(-1, save_all_inline, FALSE, NULL);
			}
			break;
		}
		case GEANY_IMAGE_NEW_ARROW:
		{
			if ((toolbar_prefs.icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR) || small_img)
			{
				return gdk_pixbuf_scale_simple(gdk_pixbuf_new_from_inline(-1, newfile_inline, FALSE, NULL),
                                             16, 16, GDK_INTERP_HYPER);
			}
			else
			{
				return gdk_pixbuf_new_from_inline(-1, newfile_inline, FALSE, NULL);
			}
			break;
		}
		default: return NULL;
	}

	/*return gtk_image_new_from_pixbuf(pixbuf);*/
}


GtkWidget *ui_new_image_from_inline(gint img, gboolean small_img)
{
	GtkWidget *wid;
	GdkPixbuf *pb;

	pb = ui_new_pixbuf_from_inline(img, small_img);
	wid = gtk_image_new_from_pixbuf(pb);
	g_object_unref(pb);	/* the image doesn't adopt our reference, so remove our ref. */
	return wid;
}


void ui_create_recent_menu(void)
{
	GtkWidget *tmp;
	guint i;
	gchar *filename;

	if (g_queue_get_length(ui_prefs.recent_queue) > 0)
	{
		gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(
				lookup_widget(main_widgets.window, "toolbutton_open")), ui_widgets.recent_files_toolbar);
	}

	for (i = 0; i < MIN(file_prefs.mru_length, g_queue_get_length(ui_prefs.recent_queue)); i++)
	{
		filename = g_queue_peek_nth(ui_prefs.recent_queue, i);
		/* create menu item for the recent files menu in the menu bar */
		tmp = gtk_menu_item_new_with_label(filename);
		gtk_widget_show(tmp);
		gtk_menu_shell_append(GTK_MENU_SHELL(ui_widgets.recent_files_menubar), tmp);
		g_signal_connect(tmp, "activate", G_CALLBACK(recent_file_activate_cb), NULL);
		/* create menu item for the recent files menu in the toolbar bar */
		tmp = gtk_menu_item_new_with_label(filename);
		gtk_widget_show(tmp);
		gtk_menu_shell_append(GTK_MENU_SHELL(ui_widgets.recent_files_toolbar), tmp);
		g_signal_connect(tmp, "activate", G_CALLBACK(recent_file_activate_cb), NULL);
	}
}


static void
recent_file_activate_cb                (GtkMenuItem     *menuitem,
                                        G_GNUC_UNUSED gpointer         user_data)
{
	gchar *utf8_filename = ui_menu_item_get_text(menuitem);
	gchar *locale_filename = utils_get_locale_from_utf8(utf8_filename);

	if (document_open_file(locale_filename, FALSE, NULL, NULL) != NULL)
		recent_file_loaded(utf8_filename);

	g_free(locale_filename);
	g_free(utf8_filename);
}


void ui_add_recent_file(const gchar *utf8_filename)
{
	if (g_queue_find_custom(ui_prefs.recent_queue, utf8_filename, (GCompareFunc) strcmp) == NULL)
	{
#if GTK_CHECK_VERSION(2, 10, 0)
		GtkRecentManager *manager = gtk_recent_manager_get_default();
		gchar *uri = g_filename_to_uri(utf8_filename, NULL, NULL);
		if (uri != NULL)
		{
			gtk_recent_manager_add_item(manager, uri);
			g_free(uri);
		}
#endif
		g_queue_push_head(ui_prefs.recent_queue, g_strdup(utf8_filename));
		if (g_queue_get_length(ui_prefs.recent_queue) > file_prefs.mru_length)
		{
			g_free(g_queue_pop_tail(ui_prefs.recent_queue));
		}
		update_recent_menu();
	}
	else recent_file_loaded(utf8_filename);	/* filename already in recent list */
}


/* Returns: newly allocated string with the UTF-8 menu text. */
gchar *ui_menu_item_get_text(GtkMenuItem *menu_item)
{
	const gchar *text = NULL;

	if (GTK_BIN(menu_item)->child)
	{
		GtkWidget *child = GTK_BIN(menu_item)->child;

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


static void recent_file_loaded(const gchar *utf8_filename)
{
	GList *item, *children;
	void *data;
	GtkWidget *tmp;

	/* first reorder the queue */
	item = g_queue_find_custom(ui_prefs.recent_queue, utf8_filename, (GCompareFunc) strcmp);
	g_return_if_fail(item != NULL);

	data = item->data;
	g_queue_remove(ui_prefs.recent_queue, data);
	g_queue_push_head(ui_prefs.recent_queue, data);

	/* remove the old menuitem for the filename */
	children = gtk_container_get_children(GTK_CONTAINER(ui_widgets.recent_files_menubar));
	item = g_list_find_custom(children, utf8_filename, (GCompareFunc) find_recent_file_item);
	if (item != NULL) gtk_widget_destroy(GTK_WIDGET(item->data));

	children = gtk_container_get_children(GTK_CONTAINER(ui_widgets.recent_files_toolbar));
	item = g_list_find_custom(children, utf8_filename, (GCompareFunc) find_recent_file_item);
	if (item != NULL) gtk_widget_destroy(GTK_WIDGET(item->data));

	/* now prepend a new menuitem for the filename,
	 * first for the recent files menu in the menu bar */
	tmp = gtk_menu_item_new_with_label(utf8_filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(ui_widgets.recent_files_menubar), tmp);
	g_signal_connect(tmp, "activate", G_CALLBACK(recent_file_activate_cb), NULL);
	/* then for the recent files menu in the tool bar */
	tmp = gtk_menu_item_new_with_label(utf8_filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(ui_widgets.recent_files_toolbar), tmp);
	g_signal_connect(tmp, "activate", G_CALLBACK(recent_file_activate_cb), NULL);
}


static void update_recent_menu(void)
{
	GtkWidget *tmp;
	static GtkMenuToolButton *menu = NULL;
	gchar *filename;
	GList *children, *item;

	if (menu == NULL)
		menu = GTK_MENU_TOOL_BUTTON(lookup_widget(main_widgets.window, "toolbutton_open"));

	if (gtk_menu_tool_button_get_menu(menu) == NULL)
	{
		gtk_menu_tool_button_set_menu(menu, ui_widgets.recent_files_toolbar);
	}

	/* clean the MRU list before adding an item (menubar) */
	children = gtk_container_get_children(GTK_CONTAINER(ui_widgets.recent_files_menubar));
	if (g_list_length(children) > file_prefs.mru_length - 1)
	{
		item = g_list_nth(children, file_prefs.mru_length - 1);
		while (item != NULL)
		{
			if (GTK_IS_MENU_ITEM(item->data)) gtk_widget_destroy(GTK_WIDGET(item->data));
			item = g_list_next(item);
		}
	}

	/* clean the MRU list before adding an item (toolbar) */
	children = gtk_container_get_children(GTK_CONTAINER(ui_widgets.recent_files_toolbar));
	if (g_list_length(children) > file_prefs.mru_length - 1)
	{
		item = g_list_nth(children, file_prefs.mru_length - 1);
		while (item != NULL)
		{
			if (GTK_IS_MENU_ITEM(item->data)) gtk_widget_destroy(GTK_WIDGET(item->data));
			item = g_list_next(item);
		}
	}

	filename = g_queue_peek_head(ui_prefs.recent_queue);
	/* create item for the menu bar menu */
	tmp = gtk_menu_item_new_with_label(filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(ui_widgets.recent_files_menubar), tmp);
	g_signal_connect(tmp, "activate", G_CALLBACK(recent_file_activate_cb), NULL);
	/* create item for the tool bar menu */
	tmp = gtk_menu_item_new_with_label(filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(ui_widgets.recent_files_toolbar), tmp);
	g_signal_connect(tmp, "activate", G_CALLBACK(recent_file_activate_cb), NULL);
}


void ui_show_markers_margin(void)
{
	gint i, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	GeanyDocument *doc;

	for(i = 0; i < max; i++)
	{
		doc = document_get_from_page(i);
		sci_set_symbol_margin(doc->editor->sci, editor_prefs.show_markers_margin);
	}
}


void ui_show_linenumber_margin(void)
{
	gint i, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	GeanyDocument *doc;

	for(i = 0; i < max; i++)
	{
		doc = document_get_from_page(i);
		sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin, 0);
	}
}


/** Creates a GNOME HIG-style frame (with no border and indented child alignment).
 * @param label_text The label text.
 * @param alignment An address to store the alignment widget pointer.
 * @return The frame widget, setting the alignment container for packing child widgets. */
GtkWidget *ui_frame_new_with_alignment(const gchar *label_text, GtkWidget **alignment)
{
	GtkWidget *label, *align;
	GtkWidget *frame = gtk_frame_new (NULL);
	gchar *label_markup;

	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

	align = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_container_add (GTK_CONTAINER (frame), align);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);

	label_markup = g_strconcat("<b>", label_text, "</b>", NULL);
	label = gtk_label_new (label_markup);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	g_free(label_markup);

	*alignment = align;
	return frame;
}


const gint BUTTON_BOX_BORDER = 5;

/** Convenience function for getting a fixed border for dialogs that doesn't
 * increase the button box border.
 * @param dialog The parent container for the @c GtkVBox.
 * @return The packed @c GtkVBox. */
GtkWidget *ui_dialog_vbox_new(GtkDialog *dialog)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 12);	/* need child vbox to set a separate border. */

	gtk_container_set_border_width(GTK_CONTAINER(vbox), BUTTON_BOX_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	return vbox;
}


/** Create a @c GtkButton with custom text and a stock image, aligned like
 * @c gtk_button_new_from_stock().
 * @param stock_id A @c GTK_STOCK_NAME string.
 * @param text Button label text, can include mnemonics.
 * @return The new @c GtkButton.
 */
GtkWidget *ui_button_new_with_image(const gchar *stock_id, const gchar *text)
{
	GtkWidget *image, *label, *align, *hbox, *button;

	hbox = gtk_hbox_new(FALSE, 2);
	image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new_with_mnemonic(text);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	button = gtk_button_new();
	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(align), hbox);
	gtk_container_add(GTK_CONTAINER(button), align);
	gtk_widget_show_all(align);
	return button;
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

	/* set_spacing is deprecated but there seems to be no alternative,
	* GTK 2.6 defaults to no spacing, unlike dialog button box */
	gtk_button_box_set_spacing(copy, 10);
	gtk_button_box_set_layout(copy, gtk_button_box_get_layout(master));

	/* now we need to put the widest widget from each button box in a size group,
	* but we don't know the width before they are drawn, and for different label
	* translations the widest widget can vary, so we just add all widgets. */
	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_container_foreach(GTK_CONTAINER(master), add_to_size_group, size_group);
	gtk_container_foreach(GTK_CONTAINER(copy), add_to_size_group, size_group);
	g_object_unref(size_group);
}


/* Prepends the active text to the drop down list, unless the first element in
 * the list is identical, ensuring there are <= history_len elements. */
void ui_combo_box_add_to_history(GtkComboBox *combo, const gchar *text)
{
	const gint history_len = 30;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *combo_text;
	gboolean equal = FALSE;
	GtkTreePath *path;

	model = gtk_combo_box_get_model(combo);
	if (gtk_tree_model_get_iter_first(model, &iter))
	{
		gtk_tree_model_get(model, &iter, 0, &combo_text, -1);
		equal = utils_str_equal(combo_text, text);
		g_free(combo_text);
	}
	if (equal) return;	/* don't prepend duplicate */

	gtk_combo_box_prepend_text(combo, text);

	/* limit history */
	path = gtk_tree_path_new_from_indices(history_len, -1);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}
	gtk_tree_path_free(path);
}


/* Changes the color of the notebook tab text and open files items according to
 * document status. */
void ui_update_tab_status(GeanyDocument *doc)
{
	GdkColor *color = document_get_status_color(doc);
	Document *fdoc = DOCUMENT(doc);

	/* NULL color will reset to default */
	gtk_widget_modify_fg(fdoc->tab_label, GTK_STATE_NORMAL, color);
	gtk_widget_modify_fg(fdoc->tab_label, GTK_STATE_ACTIVE, color);
	gtk_widget_modify_fg(fdoc->tabmenu_label, GTK_STATE_NORMAL, color);
	gtk_widget_modify_fg(fdoc->tabmenu_label, GTK_STATE_ACTIVE, color);

	treeviews_openfiles_update(doc);
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
		if (cb())
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


void ui_widget_modify_font_from_string(GtkWidget *wid, const gchar *str)
{
	PangoFontDescription *pfd;

	pfd = pango_font_description_from_string(str);
	gtk_widget_modify_font(wid, pfd);
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
	g_object_set_data(G_OBJECT(open_btn), "action", (gpointer) action);
	g_object_set_data_full(G_OBJECT(open_btn), "entry",
		gtk_widget_ref(path_entry), (GDestroyNotify) gtk_widget_unref);
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
	g_free(locale_path);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		gchar *dir_locale;

		dir_locale = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog));
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
	GtkFileChooserAction action =
		(GtkFileChooserAction) g_object_get_data(G_OBJECT(path_box), "action");
	GtkEntry *entry =
		(GtkEntry *) g_object_get_data(G_OBJECT(path_box), "entry");
	const gchar *title = g_object_get_data(G_OBJECT(path_box), "title");
	gchar *utf8_path;

	/* TODO: extend for other actions */
	g_return_if_fail(action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

	if (title == NULL)
		title = (action == GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER) ?
			_("Select Folder") : _("Select File");

#ifdef G_OS_WIN32
	utf8_path = win32_show_project_folder_dialog(ui_widgets.prefs_dialog, title,
						gtk_entry_get_text(GTK_ENTRY(entry)));
#else
	utf8_path = run_file_chooser(title, action, gtk_entry_get_text(GTK_ENTRY(entry)));
#endif

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


/** Pack all @c GtkWidgets passed after the row argument into a table, using
 * one widget per cell. The first widget is not expanded as the table grows,
 * as this is usually a label.
 * @param table
 * @param row The row number of the table.
 */
void ui_table_add_row(GtkTable *table, gint row, ...)
{
	va_list args;
	gint i;
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


void ui_init(void)
{
	ui_widgets.statusbar = lookup_widget(main_widgets.window, "statusbar");
	ui_widgets.print_page_setup = lookup_widget(main_widgets.window, "page_setup1");

	widgets.popup_goto_items[0] = lookup_widget(main_widgets.editor_menu, "goto_tag_definition1");
	widgets.popup_goto_items[1] = lookup_widget(main_widgets.editor_menu, "goto_tag_declaration1");
	widgets.popup_goto_items[2] = lookup_widget(main_widgets.editor_menu, "find_usage1");
	widgets.popup_goto_items[3] = lookup_widget(main_widgets.editor_menu, "find_document_usage1");
	widgets.popup_copy_items[0] = lookup_widget(main_widgets.editor_menu, "cut1");
	widgets.popup_copy_items[1] = lookup_widget(main_widgets.editor_menu, "copy1");
	widgets.popup_copy_items[2] = lookup_widget(main_widgets.editor_menu, "delete1");
	widgets.menu_copy_items[0] = lookup_widget(main_widgets.window, "menu_cut1");
	widgets.menu_copy_items[1] = lookup_widget(main_widgets.window, "menu_copy1");
	widgets.menu_copy_items[2] = lookup_widget(main_widgets.window, "menu_delete1");
	widgets.menu_insert_include_items[0] = lookup_widget(main_widgets.editor_menu, "insert_include1");
	widgets.menu_insert_include_items[1] = lookup_widget(main_widgets.window, "insert_include2");
	widgets.save_buttons[0] = lookup_widget(main_widgets.window, "menu_save1");
	widgets.save_buttons[1] = lookup_widget(main_widgets.window, "toolbutton_save");
	widgets.save_buttons[2] = lookup_widget(main_widgets.window, "menu_save_all1");
	widgets.save_buttons[3] = lookup_widget(main_widgets.window, "toolbutton_saveall");
	widgets.redo_items[0] = lookup_widget(main_widgets.editor_menu, "redo1");
	widgets.redo_items[1] = lookup_widget(main_widgets.window, "menu_redo2");
	widgets.redo_items[2] = lookup_widget(main_widgets.window, "toolbutton_redo");
	widgets.undo_items[0] = lookup_widget(main_widgets.editor_menu, "undo1");
	widgets.undo_items[1] = lookup_widget(main_widgets.window, "menu_undo2");
	widgets.undo_items[2] = lookup_widget(main_widgets.window, "toolbutton_undo");

	init_document_widgets();
}


/** Returns the position for adding new toolbar items. The returned position can be used
 *  to add new toolbar items with @c gtk_toolbar_insert(). The toolbar object can be accessed
 *  with @a geany->main_widgets->toolbar.
 *  The position is always the last one before the Quit button (if it is shown).
 *
 *  @return The position for new toolbar items or @c -1 if an error occurred.
 */
gint ui_get_toolbar_insert_position(void)
{
	GtkWidget *quit = lookup_widget(main_widgets.window, "toolbutton_quit");
	gint pos = gtk_toolbar_get_item_index(GTK_TOOLBAR(main_widgets.toolbar), GTK_TOOL_ITEM(quit));

	if (pos > 0)
		pos--; /* use one position before the real position of the quit button to place new
				* items between the last separator and the quit button */

	return pos;
}

