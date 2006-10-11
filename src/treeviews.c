/*
 *      treeviews.c
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


/* the following two functions are document-related, but I think they fit better here than in document.c*/
void treeviews_prepare_taglist(GtkWidget *tree, GtkTreeStore *store)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Symbols"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	gtk_widget_modify_font(tree, pango_font_description_from_string(app->tagbar_font));
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_signal_connect(G_OBJECT(tree), "button-press-event",
						G_CALLBACK(on_tree_view_button_press_event), GINT_TO_POINTER(7));

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), FALSE);

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(on_taglist_tree_selection_changed), NULL);
}


void treeviews_init_tag_list(gint idx)
{
	// init all GtkTreeIters with -1 to make them invalid to avoid crashes when switching between
	// filetypes(e.g. config file to Python crashes Geany without this)
	tv.tag_function.stamp = -1;
	tv.tag_class.stamp = -1;
	tv.tag_member.stamp = -1;
	tv.tag_macro.stamp = -1;
	tv.tag_variable.stamp = -1;
	tv.tag_namespace.stamp = -1;
	tv.tag_struct.stamp = -1;
	tv.tag_other.stamp = -1;

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_DIFF:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_function), 0, _("Files"), -1);
			break;
		}
		case GEANY_FILETYPES_DOCBOOK:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_function), 0, _("Chapter"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_class), 0, _("Section"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_member), 0, _("Sect1"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_macro), 0, _("Sect2"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_variable), 0, _("Sect3"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_struct), 0, _("Appendix"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_other), 0, _("Other"), -1);
			//gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_namespace), NULL);
			//gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_namespace), 0, _("Other"), -1);
			break;
		}
		case GEANY_FILETYPES_LATEX:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_function), 0, _("Command"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_class), 0, _("Environment"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_member), 0, _("Section"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_macro), 0, _("Subsection"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_variable), 0, _("Subsubsection"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_struct), 0, _("Label"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_namespace), 0, _("Begin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_other), 0, _("Other"), -1);
			break;
		}
		case GEANY_FILETYPES_PERL:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_function), 0, _("Function"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_class), 0, _("Package"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_member), 0, _("My"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_macro), 0, _("Local"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_variable), 0, _("Our"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_struct), 0, _("Label"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_namespace), 0, _("Begin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_other), 0, _("Other"), -1);
*/
			break;
		}
		case GEANY_FILETYPES_RUBY:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_function), 0, _("Methods"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_class), 0, _("Classes"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_member), 0, _("Singletons"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_macro), 0, _("Mixins"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_variable), 0, _("Variables"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_struct), 0, _("Members"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_namespace), 0, _("Begin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_other), 0, _("Other"), -1);
*/
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_function), 0, _("Functions"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_class), 0, _("Classes"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_member), 0, _("Methods"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_macro), 0, _("Mixin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_variable), 0, _("Variables"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_struct), 0, _("Members"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_namespace), 0, _("Begin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_other), 0, _("Other"), -1);
*/
			break;
		}
		default:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_function), 0, _("Functions"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_class), 0, _("Classes"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_member), 0, _("Members"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_macro), 0, _("Macros"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_variable), 0, _("Variables"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_namespace), 0, _("Namespaces"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_struct), 0, _("Structs / Typedefs"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv.tag_other), 0, _("Other"), -1);
		}
	}
}


/* does some preparing things to the open files list widget */
void treeviews_prepare_openfiles(void)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	tv.tree_openfiles = lookup_widget(app->window, "treeview6");

	// store the short filename to show, and the index as reference
	tv.store_openfiles = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(tv.tree_openfiles), GTK_TREE_MODEL(tv.store_openfiles));

	// set policy settings for the scolledwindow around the treeview again, because glade
	// doesn't keep the settings
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(lookup_widget(app->window, "scrolledwindow7")),
			GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Open files"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tv.tree_openfiles), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv.tree_openfiles), FALSE);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tv.tree_openfiles), FALSE);

	gtk_widget_modify_font(tv.tree_openfiles, pango_font_description_from_string(app->tagbar_font));
	g_signal_connect(G_OBJECT(tv.tree_openfiles), "button-press-event",
						G_CALLBACK(on_tree_view_button_press_event), GINT_TO_POINTER(6));

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(on_openfiles_tree_selection_changed), NULL);
}


GtkTreeIter treeviews_openfiles_add(gint idx, const gchar *string)
{
	GtkTreeIter iter;

	gtk_list_store_append(tv.store_openfiles, &iter);
	gtk_list_store_set(tv.store_openfiles, &iter, 0, string, 1, idx, -1);

	return iter;
}


// I think this wrapper function is useful
void treeviews_openfiles_remove(GtkTreeIter iter)
{
	gtk_list_store_remove(tv.store_openfiles, &iter);
}


void treeviews_openfiles_update(GtkTreeIter iter, const gchar *string)
{
	gtk_list_store_set(tv.store_openfiles, &iter, 0, string, -1);
}


void treeviews_openfiles_update_all(void)
{
	guint i;
	gint idx;
	gchar *shortname;

	gtk_list_store_clear(tv.store_openfiles);
	for (i = 0; i < (guint) gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)); i++)
	{
		idx = document_get_n_idx(i);
		if (! doc_list[idx].is_valid) continue;

		if (doc_list[idx].file_name == NULL)
			shortname = g_strdup(GEANY_STRING_UNTITLED);
		else
			shortname = g_path_get_basename(doc_list[idx].file_name);

		doc_list[idx].iter = treeviews_openfiles_add(idx, shortname);
		g_free(shortname);
	}

}


void treeviews_create_taglist_popup_menu(void)
{
	GtkWidget *item;

	tv.popup_taglist = gtk_menu_new();

	item = gtk_image_menu_item_new_with_label(_("Hide"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_taglist_tree_popup_clicked), GINT_TO_POINTER(0));

	item = gtk_image_menu_item_new_with_label(_("Hide sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_taglist_tree_popup_clicked), GINT_TO_POINTER(1));
}


void treeviews_create_openfiles_popup_menu(void)
{
	GtkWidget *item;

	tv.popup_openfiles = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock("gtk-close", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(0));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);

	item = gtk_image_menu_item_new_from_stock("gtk-save", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(1));

	item = gtk_image_menu_item_new_with_label(_("Reload"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(2));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);

	item = gtk_image_menu_item_new_with_label(_("Hide"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(3));

	item = gtk_image_menu_item_new_with_label(_("Hide sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect((gpointer) item, "activate",
				G_CALLBACK(on_openfiles_tree_popup_clicked), GINT_TO_POINTER(4));
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

