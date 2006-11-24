/*
 *      ui_utils.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
 *      Copyright 2006 Nick Treleaven <nick.treleaven@btinternet.com>
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


#include "geany.h"

#include <string.h>

#include "ui_utils.h"
#include "sciwrappers.h"
#include "document.h"
#include "support.h"
#include "msgwindow.h"
#include "utils.h"
#include "callbacks.h"
#include "encodings.h"
#include "images.c"
#include "treeviews.h"
#include "symbols.h"


static gchar *menu_item_get_text(GtkMenuItem *menu_item);

static void update_recent_menu();
static void recent_file_loaded(const gchar *utf8_filename);
static void
recent_file_activate_cb                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


/* allow_override is TRUE if text can be ignored when another message has been set
 * that didn't use allow_override and has not timed out. */
void ui_set_statusbar(const gchar *text, gboolean allow_override)
{
	static glong last_time = 0;
	GTimeVal timeval;
	const gint GEANY_STATUS_TIMEOUT = 1;

	g_get_current_time(&timeval);

	if (! allow_override)
	{
		gtk_statusbar_pop(GTK_STATUSBAR(app->statusbar), 1);
		gtk_statusbar_push(GTK_STATUSBAR(app->statusbar), 1, text);
		last_time = timeval.tv_sec;
	}
	else
	if (timeval.tv_sec > last_time + GEANY_STATUS_TIMEOUT)
	{
		gtk_statusbar_pop(GTK_STATUSBAR(app->statusbar), 1);
		gtk_statusbar_push(GTK_STATUSBAR(app->statusbar), 1, text);
	}
}


/* updates the status bar */
void ui_update_statusbar(gint idx, gint pos)
{
	gchar *text;
	const gchar *cur_tag;
	guint line, col;

	if (idx == -1) idx = document_get_cur_idx();

	if (idx >= 0 && doc_list[idx].is_valid)
	{
		utils_get_current_function(idx, &cur_tag);

		if (pos == -1) pos = sci_get_current_position(doc_list[idx].sci);
		line = sci_get_line_from_position(doc_list[idx].sci, pos);

	   // Add temporary fix for sci infinite loop in Document::GetColumn(int)
	   // when current pos is beyond document end (can occur when removing
	   // blocks of selected lines especially esp. brace sections near end of file).
		if (pos <= sci_get_length(doc_list[idx].sci))
			col = sci_get_col_from_position(doc_list[idx].sci, pos);
		else
			col = 0;

		text = g_strdup_printf(_("%c  line: % 4d column: % 3d  selection: % 4d   %s      mode: %s%s      cur. function: %s      encoding: %s %s     filetype: %s"),
			(doc_list[idx].changed) ? 42 : 32,
			(line + 1), (col + 1),
			sci_get_selected_text_length(doc_list[idx].sci) - 1,
			doc_list[idx].do_overwrite ? _("OVR") : _("INS"),
			document_get_eol_mode(idx),
			(doc_list[idx].readonly) ? ", read only" : "",
			cur_tag,
			(doc_list[idx].encoding) ? doc_list[idx].encoding : _("unknown"),
			(utils_is_unicode_charset(doc_list[idx].encoding)) ? ((doc_list[idx].has_bom) ? _("(with BOM)") : _("(without BOM)")) : "",
			(doc_list[idx].file_type) ? doc_list[idx].file_type->title : _("unknown"));
		ui_set_statusbar(text, TRUE); //can be overridden by status messages
		g_free(text);
	}
	else
	{
		ui_set_statusbar("", TRUE); //can be overridden by status messages
	}
}


/* This sets the window title according to the current filename. */
void ui_set_window_title(gint index)
{
	gchar *title;

	if (index >= 0)
	{
		title = g_strdup_printf ("%s: %s %s",
				PACKAGE,
				(doc_list[index].file_name != NULL) ? g_filename_to_utf8(doc_list[index].file_name, -1, NULL, NULL, NULL) : _("untitled"),
				doc_list[index].changed ? _("(Unsaved)") : "");
		gtk_window_set_title(GTK_WINDOW(app->window), title);
		g_free(title);
	}
	else
		gtk_window_set_title(GTK_WINDOW(app->window), PACKAGE);
}


void ui_set_editor_font(const gchar *font_name)
{
	guint i;
	gint size;
	gchar *fname;
	PangoFontDescription *font_desc;

	g_return_if_fail(font_name != NULL);
	// do nothing if font has not changed
	if (app->editor_font != NULL)
		if (strcmp(font_name, app->editor_font) == 0) return;

	g_free(app->editor_font);
	app->editor_font = g_strdup(font_name);

	font_desc = pango_font_description_from_string(app->editor_font);

	fname = g_strdup_printf("!%s", pango_font_description_get_family(font_desc));
	size = pango_font_description_get_size(font_desc) / PANGO_SCALE;

	/* We copy the current style, and update the font in all open tabs. */
	for(i = 0; i < doc_array->len; i++)
	{
		if (doc_list[i].sci)
		{
			document_set_font(i, fname, size);
		}
	}
	pango_font_description_free(font_desc);

	msgwin_status_add(_("Font updated (%s)."), app->editor_font);
	g_free(fname);
}


void ui_set_fullscreen()
{
	if (app->fullscreen)
	{
		gtk_window_fullscreen(GTK_WINDOW(app->window));
	}
	else
	{
		gtk_window_unfullscreen(GTK_WINDOW(app->window));
	}
}


// update = rescan the tags for document[idx].filename
void ui_update_tag_list(gint idx, gboolean update)
{
	GList *tmp;
	const GList *tags;

	if (gtk_bin_get_child(GTK_BIN(app->tagbar)))
		gtk_container_remove(GTK_CONTAINER(app->tagbar), gtk_bin_get_child(GTK_BIN(app->tagbar)));

	if (app->default_tag_tree == NULL)
	{
		GtkTreeIter iter;
		GtkTreeStore *store = gtk_tree_store_new(1, G_TYPE_STRING);
		app->default_tag_tree = gtk_tree_view_new();
		treeviews_prepare_taglist(app->default_tag_tree, store);
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, 0, _("No tags found"), -1);
		gtk_widget_show(app->default_tag_tree);
		g_object_ref((gpointer)app->default_tag_tree);	// to hold it after removing
	}

	// make all inactive, because there is no more tab left, or something strange occured
	if (idx == -1 || doc_list[idx].file_type == NULL || ! doc_list[idx].file_type->has_tags)
	{
		gtk_widget_set_sensitive(app->tagbar, FALSE);
		gtk_container_add(GTK_CONTAINER(app->tagbar), app->default_tag_tree);
		return;
	}

	if (update)
	{	// updating the tag list in the left tag window
		if (doc_list[idx].tag_tree == NULL)
		{
			doc_list[idx].tag_store = gtk_tree_store_new(1, G_TYPE_STRING);
			doc_list[idx].tag_tree = gtk_tree_view_new();
			treeviews_prepare_taglist(doc_list[idx].tag_tree, doc_list[idx].tag_store);
			gtk_widget_show(doc_list[idx].tag_tree);
			g_object_ref((gpointer)doc_list[idx].tag_tree);	// to hold it after removing
		}

		tags = symbols_get_tag_list(idx, tm_tag_max_t);
		if (doc_list[idx].tm_file != NULL && tags != NULL)
		{
			GtkTreeIter iter;
			GtkTreeModel *model;

			doc_list[idx].has_tags = TRUE;
			gtk_tree_store_clear(doc_list[idx].tag_store);
			// unref the store to speed up the filling(from TreeView Tutorial)
			model = gtk_tree_view_get_model(GTK_TREE_VIEW(doc_list[idx].tag_tree));
			g_object_ref(model); // Make sure the model stays with us after the tree view unrefs it
			gtk_tree_view_set_model(GTK_TREE_VIEW(doc_list[idx].tag_tree), NULL); // Detach model from view

			treeviews_init_tag_list(idx);
			for (tmp = (GList*)tags; tmp; tmp = g_list_next(tmp))
			{
				switch (((GeanySymbol*)tmp->data)->type)
				{
					case tm_tag_prototype_t:
					case tm_tag_method_t:
					case tm_tag_function_t:
					{
						if (tv.tag_function.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_function));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_macro_t:
					case tm_tag_macro_with_arg_t:
					{
						if (tv.tag_macro.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_macro));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_class_t:
					{
						if (tv.tag_class.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_class));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_member_t:
					case tm_tag_field_t:
					{
						if (tv.tag_member.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_member));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_typedef_t:
					case tm_tag_enum_t:
					case tm_tag_union_t:
					case tm_tag_struct_t:
					case tm_tag_interface_t:
					{
						if (tv.tag_struct.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_struct));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_variable_t:
					{
						if (tv.tag_variable.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_variable));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					case tm_tag_namespace_t:
					case tm_tag_package_t:
					{
						if (tv.tag_namespace.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_namespace));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
						break;
					}
					default:
					{
						if (tv.tag_other.stamp == -1) break;
						gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv.tag_other));
						gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, ((GeanySymbol*)tmp->data)->str, -1);
					}
				}
			}
			gtk_tree_view_set_model(GTK_TREE_VIEW(doc_list[idx].tag_tree), model); // Re-attach model to view
			g_object_unref(model);
			gtk_tree_view_expand_all(GTK_TREE_VIEW(doc_list[idx].tag_tree));

			gtk_widget_set_sensitive(app->tagbar, TRUE);
			gtk_container_add(GTK_CONTAINER(app->tagbar), doc_list[idx].tag_tree);
			/// TODO why I have to do this here?
			g_object_ref((gpointer)doc_list[idx].tag_tree);
		}
		else
		{	// tags == NULL
			gtk_widget_set_sensitive(app->tagbar, FALSE);
			gtk_container_add(GTK_CONTAINER(app->tagbar), app->default_tag_tree);
		}
	}
	else
	{	// update == FALSE
		if (doc_list[idx].has_tags)
		{
			gtk_widget_set_sensitive(app->tagbar, TRUE);
			gtk_container_add(GTK_CONTAINER(app->tagbar), doc_list[idx].tag_tree);
		}
		else
		{
			gtk_widget_set_sensitive(app->tagbar, FALSE);
			gtk_container_add(GTK_CONTAINER(app->tagbar), app->default_tag_tree);
		}
	}
}


void ui_update_popup_reundo_items(gint index)
{
	gboolean enable_undo;
	gboolean enable_redo;

	if (index == -1)
	{
		enable_undo = FALSE;
		enable_redo = FALSE;
	}
	else
	{
		enable_undo = document_can_undo(index);
		enable_redo = document_can_redo(index);
	}

	// index 0 is the popup menu, 1 is the menubar, 2 is the toolbar
	gtk_widget_set_sensitive(app->undo_items[0], enable_undo);
	gtk_widget_set_sensitive(app->undo_items[1], enable_undo);
	gtk_widget_set_sensitive(app->undo_items[2], enable_undo);

	gtk_widget_set_sensitive(app->redo_items[0], enable_redo);
	gtk_widget_set_sensitive(app->redo_items[1], enable_redo);
	gtk_widget_set_sensitive(app->redo_items[2], enable_redo);
}


void ui_update_popup_copy_items(gint index)
{
	gboolean enable;
	guint i;

	if (index == -1) enable = FALSE;
	else enable = sci_can_copy(doc_list[index].sci);

	for(i = 0; i < (sizeof(app->popup_items)/sizeof(GtkWidget*)); i++)
		gtk_widget_set_sensitive(app->popup_items[i], enable);
}


void ui_update_popup_goto_items(gboolean enable)
{
	gtk_widget_set_sensitive(app->popup_goto_items[0], enable);
	gtk_widget_set_sensitive(app->popup_goto_items[1], enable);
	gtk_widget_set_sensitive(app->popup_goto_items[2], enable);
}


void ui_update_menu_copy_items(gint idx)
{
	gboolean enable = FALSE;
	guint i;
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	if (IS_SCINTILLA(focusw))
		enable = (idx == -1) ? FALSE : sci_can_copy(doc_list[idx].sci);
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

	for(i = 0; i < (sizeof(app->menu_copy_items)/sizeof(GtkWidget*)); i++)
		gtk_widget_set_sensitive(app->menu_copy_items[i], enable);
}


void ui_update_insert_include_item(gint idx, gint item)
{
	gboolean enable = FALSE;

	if (idx == -1 || doc_list[idx].file_type == NULL) enable = FALSE;
	else if (doc_list[idx].file_type->id == GEANY_FILETYPES_C ||
			 doc_list[idx].file_type->id == GEANY_FILETYPES_CPP)
	{
		enable = TRUE;
	}
	gtk_widget_set_sensitive(app->menu_insert_include_item[item], enable);
}


void ui_update_fold_items()
{
	gtk_widget_set_sensitive(lookup_widget(app->window, "menu_fold_all1"), app->pref_editor_folding);
	gtk_widget_set_sensitive(lookup_widget(app->window, "menu_unfold_all1"), app->pref_editor_folding);
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
		g_signal_connect((gpointer) tmp_menu, "activate", G_CALLBACK(on_insert_include_activate),
																	(gpointer) includes[i]);
		g_signal_connect((gpointer) tmp_popup, "activate", G_CALLBACK(on_insert_include_activate),
																	 (gpointer) includes[i]);
		i++;
	}
	gtk_widget_show_all(edit_menu_item);
	gtk_widget_show_all(popup_menu_item);
	gtk_container_add(GTK_CONTAINER(me), edit_menu_item);
	gtk_container_add(GTK_CONTAINER(mp), popup_menu_item);
}


void ui_create_insert_menu_items()
{
	GtkMenu *menu_edit = GTK_MENU(lookup_widget(app->window, "insert_include2_menu"));
	GtkMenu *menu_popup = GTK_MENU(lookup_widget(app->popup_menu, "insert_include1_menu"));
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
	g_signal_connect((gpointer) blank, "activate", G_CALLBACK(on_insert_include_activate),
																	(gpointer) "blank");
	blank = gtk_separator_menu_item_new ();
	gtk_container_add(GTK_CONTAINER(menu_edit), blank);
	gtk_widget_show(blank);

	blank = gtk_menu_item_new_with_label("#include \"...\"");
	gtk_container_add(GTK_CONTAINER(menu_popup), blank);
	gtk_widget_show(blank);
	g_signal_connect((gpointer) blank, "activate", G_CALLBACK(on_insert_include_activate),
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

	item = gtk_menu_item_new_with_label(label);
	gtk_container_add(GTK_CONTAINER(me), item);
	gtk_widget_show(item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_insert_date_activate), label);

	item = gtk_menu_item_new_with_label(label);
	gtk_container_add(GTK_CONTAINER(mp), item);
	gtk_widget_show(item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_insert_date_activate), label);
}


void ui_create_insert_date_menu_items()
{
	GtkMenu *menu_edit = GTK_MENU(lookup_widget(app->window, "insert_date1_menu"));
	GtkMenu *menu_popup = GTK_MENU(lookup_widget(app->popup_menu, "insert_date2_menu"));
	GtkWidget *item;

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

	item = gtk_menu_item_new_with_label(_("Use custom date format"));
	gtk_container_add(GTK_CONTAINER(menu_edit), item);
	gtk_widget_show(item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_insert_date_activate),
													_("Use custom date format"));
	g_object_set_data_full(G_OBJECT(app->window), "insert_date_custom1", gtk_widget_ref(item),
													(GDestroyNotify)gtk_widget_unref);

	item = gtk_menu_item_new_with_label(_("Use custom date format"));
	gtk_container_add(GTK_CONTAINER(menu_popup), item);
	gtk_widget_show(item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_insert_date_activate),
													_("Use custom date format"));
	g_object_set_data_full(G_OBJECT(app->popup_menu), "insert_date_custom2", gtk_widget_ref(item),
													(GDestroyNotify)gtk_widget_unref);

	insert_date_items(menu_edit, menu_popup, _("Set custom date format"));
}


void ui_save_buttons_toggle(gboolean enable)
{
	guint i;
	gboolean dirty_tabs = FALSE;

	gtk_widget_set_sensitive(app->save_buttons[0], enable);
	gtk_widget_set_sensitive(app->save_buttons[1], enable);

	// save all menu item and tool button
	for (i = 0; i < (guint) gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)); i++)
	{
		// count the amount of files where changes were made and if there are some,
		// we need the save all button / item
		if (! dirty_tabs && doc_list[i].is_valid && doc_list[i].changed)
			dirty_tabs = TRUE;
	}

	gtk_widget_set_sensitive(app->save_buttons[2], (dirty_tabs > 0) ? TRUE : FALSE);
	gtk_widget_set_sensitive(app->save_buttons[3], (dirty_tabs > 0) ? TRUE : FALSE);
}


void ui_close_buttons_toggle()
{
	guint i;
	gboolean enable = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) ? TRUE : FALSE;

	for(i = 0; i < (sizeof(app->sensitive_buttons)/sizeof(GtkWidget*)); i++)
			gtk_widget_set_sensitive(app->sensitive_buttons[i], enable);
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


void ui_treeviews_show_hide(G_GNUC_UNUSED gboolean force)
{
	GtkWidget *widget;

/*	geany_debug("\nSidebar: %s\nSymbol: %s\nFiles: %s", ui_btoa(app->sidebar_visible),
					ui_btoa(app->sidebar_symbol_visible), ui_btoa(app->sidebar_openfiles_visible));
*/

	if (! app->sidebar_openfiles_visible && ! app->sidebar_symbol_visible)
	{
		app->sidebar_visible = FALSE;
	}

	widget = lookup_widget(app->window, "menu_show_sidebar1");
	if (app->sidebar_visible != gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	{
		app->ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), app->sidebar_visible);
		app->ignore_callback = FALSE;
	}

	ui_widget_show_hide(app->treeview_notebook, app->sidebar_visible);

	ui_widget_show_hide(gtk_notebook_get_nth_page(
					GTK_NOTEBOOK(app->treeview_notebook), 0), app->sidebar_symbol_visible);
	ui_widget_show_hide(gtk_notebook_get_nth_page(
					GTK_NOTEBOOK(app->treeview_notebook), 1), app->sidebar_openfiles_visible);
}


void ui_document_show_hide(gint idx)
{
	gchar *widget_name;

	if (idx == -1 || ! doc_list[idx].is_valid) return;
	app->ignore_callback = TRUE;

	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_line_breaking1")),
			doc_list[idx].line_breaking);
	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_use_auto_indention1")),
			doc_list[idx].use_auto_indention);
	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "set_file_readonly1")),
			doc_list[idx].readonly);
	gtk_check_menu_item_set_active(
			GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_write_unicode_bom1")),
			doc_list[idx].has_bom);

	switch (sci_get_eol_mode(doc_list[idx].sci))
	{
		case SC_EOL_CR: widget_name = "cr"; break;
		case SC_EOL_LF: widget_name = "lf"; break;
		default: widget_name = "crlf"; break;
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, widget_name)),
																					TRUE);

	gtk_widget_set_sensitive(lookup_widget(app->window, "menu_write_unicode_bom1"),
			utils_is_unicode_charset(doc_list[idx].encoding));

	encodings_select_radio_item(doc_list[idx].encoding);
	filetypes_select_radio_item(doc_list[idx].file_type);

	app->ignore_callback = FALSE;

}


void ui_update_toolbar_icons(GtkIconSize size)
{
	GtkWidget *button_image = NULL;
	GtkWidget *widget = NULL;
	GtkWidget *oldwidget = NULL;

	// destroy old widget
	widget = lookup_widget(app->window, "toolbutton22");
	oldwidget = gtk_tool_button_get_icon_widget(GTK_TOOL_BUTTON(widget));
	if (oldwidget && GTK_IS_WIDGET(oldwidget)) gtk_widget_destroy(oldwidget);
	// create new widget
	button_image = ui_new_image_from_inline(GEANY_IMAGE_SAVE_ALL, FALSE);
	gtk_widget_show(button_image);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(widget), button_image);

	gtk_toolbar_set_icon_size(GTK_TOOLBAR(app->toolbar), size);
}


void ui_update_toolbar_items()
{
	// show toolbar
	GtkWidget *widget = lookup_widget(app->window, "menu_show_toolbar1");
	if (app->toolbar_visible && ! gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	{
		app->toolbar_visible = ! app->toolbar_visible;	// will be changed by the toggled callback
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), TRUE);
	}
	else if (! app->toolbar_visible && gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
	{
		app->toolbar_visible = ! app->toolbar_visible;	// will be changed by the toggled callback
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), FALSE);
	}

	// fileops
	ui_widget_show_hide(lookup_widget(app->window, "menutoolbutton1"), app->pref_toolbar_show_fileops);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton9"), app->pref_toolbar_show_fileops);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton10"), app->pref_toolbar_show_fileops);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton22"), app->pref_toolbar_show_fileops);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton23"), app->pref_toolbar_show_fileops);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton15"), app->pref_toolbar_show_fileops);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem7"), app->pref_toolbar_show_fileops);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem2"), app->pref_toolbar_show_fileops);
	// search
	ui_widget_show_hide(lookup_widget(app->window, "entry1"), app->pref_toolbar_show_search);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton18"), app->pref_toolbar_show_search);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem5"), app->pref_toolbar_show_search);
	// goto line
	ui_widget_show_hide(lookup_widget(app->window, "entry_goto_line"), app->pref_toolbar_show_goto);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton25"), app->pref_toolbar_show_goto);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem8"), app->pref_toolbar_show_goto);
	// compile
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton13"), app->pref_toolbar_show_compile);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton26"), app->pref_toolbar_show_compile);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem6"), app->pref_toolbar_show_compile);
	// colour
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton24"), app->pref_toolbar_show_colour);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem3"), app->pref_toolbar_show_colour);
	// zoom
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton20"), app->pref_toolbar_show_zoom);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton21"), app->pref_toolbar_show_zoom);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem4"), app->pref_toolbar_show_zoom);
	// undo
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton_undo"), app->pref_toolbar_show_undo);
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton_redo"), app->pref_toolbar_show_undo);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem9"), app->pref_toolbar_show_undo);
	// quit
	ui_widget_show_hide(lookup_widget(app->window, "toolbutton19"), app->pref_toolbar_show_quit);
	ui_widget_show_hide(lookup_widget(app->window, "separatortoolitem8"), app->pref_toolbar_show_quit);
}


GdkPixbuf *ui_new_pixbuf_from_inline(gint img, gboolean small_img)
{
	switch(img)
	{
		case GEANY_IMAGE_SMALL_CROSS: return gdk_pixbuf_new_from_inline(-1, close_small_inline, FALSE, NULL); break;
		case GEANY_IMAGE_LOGO: return gdk_pixbuf_new_from_inline(-1, aladin_inline, FALSE, NULL); break;
		case GEANY_IMAGE_SAVE_ALL:
		{
			if ((app->toolbar_icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR) || small_img)
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
			if ((app->toolbar_icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR) || small_img)
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

	//return gtk_image_new_from_pixbuf(pixbuf);
}


GtkWidget *ui_new_image_from_inline(gint img, gboolean small_img)
{
	return gtk_image_new_from_pixbuf(ui_new_pixbuf_from_inline(img, small_img));
}


void ui_create_recent_menu()
{
	GtkWidget *recent_menu = lookup_widget(app->window, "recent_files1_menu");
	GtkWidget *tmp;
	guint i;
	gchar *filename;

	if (g_queue_get_length(app->recent_queue) == 0)
	{
		gtk_widget_set_sensitive(lookup_widget(app->window, "recent_files1"), FALSE);
		return;
	}

	for (i = 0; i < MIN(app->mru_length, g_queue_get_length(app->recent_queue));
		i++)
	{
		filename = g_queue_peek_nth(app->recent_queue, i);
		tmp = gtk_menu_item_new_with_label(filename);
		gtk_widget_show(tmp);
		gtk_menu_shell_append(GTK_MENU_SHELL(recent_menu), tmp);
		g_signal_connect((gpointer) tmp, "activate",
					G_CALLBACK(recent_file_activate_cb), NULL);
	}
}


static void
recent_file_activate_cb                (GtkMenuItem     *menuitem,
                                        G_GNUC_UNUSED gpointer         user_data)
{
	gchar *utf8_filename = menu_item_get_text(menuitem);
	gchar *locale_filename = utils_get_locale_from_utf8(utf8_filename);

	if (document_open_file(-1, locale_filename, 0, FALSE, NULL, NULL) > -1)
		recent_file_loaded(utf8_filename);

	g_free(locale_filename);
	g_free(utf8_filename);
}


void ui_add_recent_file(const gchar *utf8_filename)
{
	if (g_queue_find_custom(app->recent_queue, utf8_filename, (GCompareFunc) strcmp) == NULL)
	{
		g_queue_push_head(app->recent_queue, g_strdup(utf8_filename));
		if (g_queue_get_length(app->recent_queue) > app->mru_length)
		{
			g_free(g_queue_pop_tail(app->recent_queue));
		}
		update_recent_menu();
	}
	else recent_file_loaded(utf8_filename);	// filename already in recent list
}


// Returns: newly allocated string with the UTF-8 menu text.
static gchar *menu_item_get_text(GtkMenuItem *menu_item)
{
	const gchar *text = NULL;

	if (GTK_BIN (menu_item)->child)
	{
		GtkWidget *child = GTK_BIN (menu_item)->child;

		if (GTK_IS_LABEL (child))
			text = gtk_label_get_text(GTK_LABEL(child));
	}
	// GTK owns text so it's much safer to return a copy of it in case the memory is reallocated
	return g_strdup(text);
}


static void recent_file_loaded(const gchar *utf8_filename)
{
	GList *item, *children;
	void *data;
	GtkWidget *recent_menu, *tmp;

	// first reorder the queue
	item = g_queue_find_custom(app->recent_queue, utf8_filename, (GCompareFunc) strcmp);
	g_return_if_fail(item != NULL);

	data = item->data;
	g_queue_remove(app->recent_queue, data);
	g_queue_push_head(app->recent_queue, data);

	// now reorder the recent files menu
	recent_menu = lookup_widget(app->window, "recent_files1_menu");
	children = gtk_container_get_children(GTK_CONTAINER(recent_menu));

	// remove the old menuitem for the filename
	for (item = children; item != NULL; item = g_list_next(item))
	{
		gchar *menu_text;

		data = item->data;
		if (! GTK_IS_MENU_ITEM(data)) continue;
		menu_text = menu_item_get_text(GTK_MENU_ITEM(data));

		if (g_str_equal(menu_text, utf8_filename))
		{
			gtk_widget_destroy(GTK_WIDGET(data));
			g_free(menu_text);
			break;
		}
		g_free(menu_text);
	}
	// now prepend a new menuitem for the filename
	tmp = gtk_menu_item_new_with_label(utf8_filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(recent_menu), tmp);
	g_signal_connect((gpointer) tmp, "activate",
				G_CALLBACK(recent_file_activate_cb), NULL);
}


static void update_recent_menu()
{
	GtkWidget *recent_menu = lookup_widget(app->window, "recent_files1_menu");
	GtkWidget *recent_files_item = lookup_widget(app->window, "recent_files1");
	GtkWidget *tmp;
	gchar *filename;
	GList *children;

	if (g_queue_get_length(app->recent_queue) == 0)
	{
		gtk_widget_set_sensitive(recent_files_item, FALSE);
		return;
	}
	else if (! GTK_WIDGET_SENSITIVE(recent_files_item))
	{
		gtk_widget_set_sensitive(recent_files_item, TRUE);
	}

	// clean the MRU list before adding an item
	children = gtk_container_get_children(GTK_CONTAINER(recent_menu));
	if (g_list_length(children) > app->mru_length - 1)
	{
		GList *item = g_list_nth(children, app->mru_length - 1);
		while (item != NULL)
		{
			if (GTK_IS_MENU_ITEM(item->data)) gtk_widget_destroy(GTK_WIDGET(item->data));
			item = g_list_next(item);
		}
	}

	filename = g_queue_peek_head(app->recent_queue);
	tmp = gtk_menu_item_new_with_label(filename);
	gtk_widget_show(tmp);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(recent_menu), tmp);
	g_signal_connect((gpointer) tmp, "activate",
				G_CALLBACK(recent_file_activate_cb), NULL);
}


void ui_show_markers_margin()
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		sci_set_symbol_margin(doc_list[idx].sci, app->show_markers_margin);
	}
}


void ui_show_linenumber_margin()
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		sci_set_line_numbers(doc_list[idx].sci, app->show_linenumber_margin, 0);
	}
}


/* Creates a GNOME HIG style frame (with no border and indented child alignment).
 * Returns the frame widget, setting the alignment container for packing child widgets */
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

/* common convenience function for getting a fixed border for dialogs
 * that doesn't increase the button box border */
GtkWidget *ui_dialog_vbox_new(GtkDialog *dialog)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 12);	// need child vbox to set a separate border.

	gtk_container_set_border_width(GTK_CONTAINER(vbox), BUTTON_BOX_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	return vbox;
}


/* Create a GtkButton with custom text and a stock image, aligned like
 * gtk_button_new_from_stock */
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
	if (equal) return;	// don't prepend duplicate

	gtk_combo_box_prepend_text(combo, text);
	
	// limit history
	path = gtk_tree_path_new_from_indices(history_len, -1);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}
	gtk_tree_path_free(path);
}

