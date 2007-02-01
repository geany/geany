/*
 *      treeviews.c
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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

#include <string.h>

#include "geany.h"
#include "support.h"
#include "callbacks.h"
#include "treeviews.h"
#include "document.h"
#include "utils.h"
#include "ui_utils.h"


enum
{
	TREEVIEW_SYMBOL = 0,
	TREEVIEW_OPENFILES
};

enum
{
	OPENFILES_ACTION_REMOVE = 0,
	OPENFILES_ACTION_SAVE,
	OPENFILES_ACTION_RELOAD,
	OPENFILES_ACTION_HIDE,
	OPENFILES_ACTION_HIDE_ALL,
	SYMBOL_ACTION_HIDE,
	SYMBOL_ACTION_HIDE_ALL
};


/* callback prototypes */
static void on_taglist_tree_popup_clicked(GtkMenuItem *menuitem, gpointer user_data);
static void on_openfiles_tree_selection_changed(GtkTreeSelection *selection, gpointer data);
static void on_openfiles_tree_popup_clicked(GtkMenuItem *menuitem, gpointer user_data);
static void on_taglist_tree_selection_changed(GtkTreeSelection *selection, gpointer data);
static gboolean on_treeviews_button_press_event(GtkWidget *widget, GdkEventButton *event,
																			gpointer user_data);



/* the following two functions are document-related, but I think they fit better here than in document.c */
void treeviews_prepare_taglist(GtkWidget *tree, GtkTreeStore *store)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	PangoFontDescription *pfd;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Symbols"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	pfd = pango_font_description_from_string(app->tagbar_font);
	gtk_widget_modify_font(tree, pfd);
	pango_font_description_free(pfd);

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_signal_connect(G_OBJECT(tree), "button-press-event",
					G_CALLBACK(on_treeviews_button_press_event), GINT_TO_POINTER(TREEVIEW_SYMBOL));

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), FALSE);

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed",
				G_CALLBACK(on_taglist_tree_selection_changed), NULL);
}


/* does some preparing things to the open files list widget */
void treeviews_prepare_openfiles()
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	PangoFontDescription *pfd;
	GtkTreeSortable    *sortable;

	tv.tree_openfiles = lookup_widget(app->window, "treeview6");

	// store the short filename to show, and the index as reference
	tv.store_openfiles = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_INT, GDK_TYPE_COLOR);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tv.tree_openfiles), GTK_TREE_MODEL(tv.store_openfiles));

	// set policy settings for the scolledwindow around the treeview again, because glade
	// doesn't keep the settings
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(lookup_widget(app->window, "scrolledwindow7")),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Open files"), renderer,
															"text", 0, "foreground-gdk", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tv.tree_openfiles), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv.tree_openfiles), FALSE);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tv.tree_openfiles), FALSE);

	// sort opened filenames in the store_openfiles treeview
	sortable = GTK_TREE_SORTABLE(GTK_TREE_MODEL(tv.store_openfiles));
	gtk_tree_sortable_set_sort_column_id(sortable, 0, GTK_SORT_ASCENDING);

	pfd = pango_font_description_from_string(app->tagbar_font);
	gtk_widget_modify_font(tv.tree_openfiles, pfd);
	pango_font_description_free(pfd);

	g_signal_connect(G_OBJECT(tv.tree_openfiles), "button-press-event",
						G_CALLBACK(on_treeviews_button_press_event), GINT_TO_POINTER(TREEVIEW_OPENFILES));

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(on_openfiles_tree_selection_changed), NULL);
}


/* Also sets doc_list[idx].iter.
 * This is called recursively in treeviews_openfiles_update_all(). */
void treeviews_openfiles_add(gint idx)
{
	GtkTreeIter *iter = &doc_list[idx].iter;

	gtk_list_store_append(tv.store_openfiles, iter);
	treeviews_openfiles_update(idx);
}


void treeviews_openfiles_update(gint idx)
{
	gchar *basename;
	GdkColor *color = document_get_status(idx);

	basename = g_path_get_basename(DOC_FILENAME(idx));
	gtk_list_store_set(tv.store_openfiles, &doc_list[idx].iter,
		0, basename, 1, idx, 2, color, -1);
	g_free(basename);
}


void treeviews_openfiles_update_all()
{
	guint i;
	gint idx;

	gtk_list_store_clear(tv.store_openfiles);
	for (i = 0; i < (guint) gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)); i++)
	{
		idx = document_get_n_idx(i);
		if (! doc_list[idx].is_valid) continue;

		treeviews_openfiles_add(idx);
	}
}


void treeviews_remove_document(gint idx)
{
	GtkTreeIter *iter = &doc_list[idx].iter;

	gtk_list_store_remove(tv.store_openfiles, iter);

	if (GTK_IS_WIDGET(doc_list[idx].tag_tree))
	{
		gtk_widget_destroy(doc_list[idx].tag_tree);
		if (GTK_IS_TREE_VIEW(doc_list[idx].tag_tree))
		{
			// Because it was ref'd in ui_update_tag_list, it needs unref'ing
			g_object_unref((gpointer)doc_list[idx].tag_tree);
		}
		doc_list[idx].tag_tree = NULL;
	}
}


void treeviews_create_taglist_popup_menu()
{
	GtkWidget *item;

	tv.popup_taglist = gtk_menu_new();

	item = gtk_image_menu_item_new_with_label(_("Hide"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_taglist_tree_popup_clicked), GINT_TO_POINTER(SYMBOL_ACTION_HIDE));

	item = gtk_image_menu_item_new_with_label(_("Hide sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_taglist_tree_popup_clicked), GINT_TO_POINTER(SYMBOL_ACTION_HIDE_ALL));
}


void treeviews_create_openfiles_popup_menu()
{
	GtkWidget *item;

	tv.popup_openfiles = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock("gtk-close", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(OPENFILES_ACTION_REMOVE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);

	item = gtk_image_menu_item_new_from_stock("gtk-save", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(OPENFILES_ACTION_SAVE));

	item = gtk_image_menu_item_new_with_label(_("Reload"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(OPENFILES_ACTION_RELOAD));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);

	item = gtk_image_menu_item_new_with_label(_("Hide"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_openfiles_tree_popup_clicked),
												GINT_TO_POINTER(OPENFILES_ACTION_HIDE));

	item = gtk_image_menu_item_new_with_label(_("Hide sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_openfiles_tree_popup_clicked),
													GINT_TO_POINTER(OPENFILES_ACTION_HIDE_ALL));
}


/* compares the given data (GINT_TO_PONTER(idx)) with the idx from the selected row of openfiles
 * treeview, in case of a match the row is selected and TRUE is returned
 * (called indirectly from gtk_tree_model_foreach()) */
gboolean treeviews_find_node(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	gint idx = -1;

	gtk_tree_model_get(GTK_TREE_MODEL(tv.store_openfiles), iter, 1, &idx, -1);

	if (idx == GPOINTER_TO_INT(data))
	{
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(tv.tree_openfiles), path, NULL, FALSE);
		return TRUE;
	}
	else return FALSE;
}


/* callbacks */

static void on_openfiles_tree_popup_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	GtkTreeModel *model;
	gint idx = -1;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 1, &idx, -1);
		if (idx >= 0)
		{
			switch (GPOINTER_TO_INT(user_data))
			{
				case OPENFILES_ACTION_REMOVE:
				{
					document_remove(gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook), GTK_WIDGET(doc_list[idx].sci)));
					break;
				}
				case OPENFILES_ACTION_SAVE:
				{
					if (doc_list[idx].changed) document_save_file(idx, FALSE);
					break;
				}
				case OPENFILES_ACTION_RELOAD:
				{
					on_toolbutton23_clicked(NULL, NULL);
					break;
				}
				case OPENFILES_ACTION_HIDE:
				{
					app->sidebar_openfiles_visible = FALSE;
					ui_treeviews_show_hide(FALSE);
					break;
				}
				case OPENFILES_ACTION_HIDE_ALL:
				{
					app->sidebar_visible = FALSE;
					ui_treeviews_show_hide(TRUE);
					break;
				}
			}
		}
	}
}


static void on_openfiles_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint idx = 0;

	// use switch_notebook_page to ignore changing the notebook page because it is already done
	if (gtk_tree_selection_get_selected(selection, &model, &iter) && ! app->ignore_callback)
	{
		gtk_tree_model_get(model, &iter, 1, &idx, -1);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook),
					gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook),
					(GtkWidget*) doc_list[idx].sci));
	}
}


static void on_taglist_tree_popup_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data))
	{
		case SYMBOL_ACTION_HIDE:
		{
			app->sidebar_symbol_visible = FALSE;
			ui_treeviews_show_hide(FALSE);
			break;
		}
		case SYMBOL_ACTION_HIDE_ALL:
		{
			app->sidebar_visible = FALSE;
			ui_treeviews_show_hide(TRUE);
			break;
		}
	}
}


static void on_taglist_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *string;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 0, &string, -1);
		if (string && (strlen(string) > 0))
		{
			gint idx = document_get_cur_idx();
			utils_goto_line(idx, utils_get_local_tag(idx, string));
			g_free(string);
		}
	}
}


static gboolean on_treeviews_button_press_event(GtkWidget *widget, GdkEventButton *event,
																			gpointer user_data)
{
	if (event->button == 1)
	{
		if (GPOINTER_TO_INT(user_data) == TREEVIEW_SYMBOL)
		{	// allow reclicking of taglist treeview item
			GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
			on_taglist_tree_selection_changed(select, NULL);
		}
	}

	if (event->button == 3)
	{	// popupmenu to hide or clear the active treeview
		if (GPOINTER_TO_INT(user_data) == TREEVIEW_OPENFILES)
			gtk_menu_popup(GTK_MENU(tv.popup_openfiles), NULL, NULL, NULL, NULL,
																event->button, event->time);
		else if (GPOINTER_TO_INT(user_data) == TREEVIEW_SYMBOL)
			gtk_menu_popup(GTK_MENU(tv.popup_taglist), NULL, NULL, NULL, NULL,
																event->button, event->time);
	}
	return FALSE;
}
