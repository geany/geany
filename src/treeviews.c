/*
 *      treeviews.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

/*
 * Sidebar related code for the Symbol list and Open files GtkTreeViews.
 */

#include <string.h>

#include "geany.h"
#include "support.h"
#include "callbacks.h"
#include "treeviews.h"
#include "document.h"
#include "editor.h"
#include "documentprivate.h"
#include "filetypes.h"
#include "utils.h"
#include "ui_utils.h"
#include "symbols.h"
#include "navqueue.h"

#include <gdk/gdkkeysyms.h>

SidebarTreeviews tv;

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
	SYMBOL_ACTION_SORT_BY_NAME,
	SYMBOL_ACTION_SORT_BY_APPEARANCE,
	SYMBOL_ACTION_HIDE,
	SYMBOL_ACTION_HIDE_ALL
};

typedef struct
{
	GtkWidget *documents_fullpath;
	GtkWidget *documents_show_symbols;
	GtkWidget *documents_show_documents;
	GtkWidget *symbols_show_symbols;
	GtkWidget *symbols_show_documents;
} menu_items;
static menu_items mi;

static GtkListStore	*store_openfiles;
static GtkWidget *tag_window;	/* scrolled window that holds the symbol list GtkTreeView */

/* callback prototypes */
static void on_taglist_tree_popup_clicked(GtkMenuItem *menuitem, gpointer user_data);
static void on_openfiles_tree_selection_changed(GtkTreeSelection *selection, gpointer data);
static void on_openfiles_document_action(GtkMenuItem *menuitem, gpointer user_data);
static void on_openfiles_hide_item_clicked(GtkMenuItem *menuitem, gpointer user_data);
static gboolean on_taglist_tree_selection_changed(GtkTreeSelection *selection);
static gboolean on_treeviews_button_press_event(GtkWidget *widget, GdkEventButton *event,
																			gpointer user_data);
static gboolean on_treeviews_key_press_event(GtkWidget *widget, GdkEventKey *event,
																			gpointer user_data);
static void on_list_document_activate(GtkCheckMenuItem *item, gpointer user_data);
static void on_list_symbol_activate(GtkCheckMenuItem *item, gpointer user_data);


/* the prepare_* functions are document-related, but I think they fit better here than in document.c */
static void prepare_taglist(GtkWidget *tree, GtkTreeStore *store)
{
	GtkCellRenderer *text_renderer, *icon_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	PangoFontDescription *pfd;

	text_renderer = gtk_cell_renderer_text_new();
	icon_renderer = gtk_cell_renderer_pixbuf_new();
    column = gtk_tree_view_column_new();

    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
  	gtk_tree_view_column_set_attributes(column, icon_renderer, "pixbuf", SYMBOLS_COLUMN_ICON, NULL);
  	g_object_set(icon_renderer, "xalign", 0.0, NULL);

  	gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
  	gtk_tree_view_column_set_attributes(column, text_renderer, "text", SYMBOLS_COLUMN_NAME, NULL);
  	g_object_set(text_renderer, "yalign", 0.5, NULL);
  	gtk_tree_view_column_set_title(column, _("Symbols"));

	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	pfd = pango_font_description_from_string(interface_prefs.tagbar_font);
	gtk_widget_modify_font(tree, pfd);
	pango_font_description_free(pfd);

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), GTK_TREE_MODEL(store));
	g_object_unref(store);

	g_signal_connect(tree, "button-press-event",
					G_CALLBACK(on_treeviews_button_press_event), GINT_TO_POINTER(TREEVIEW_SYMBOL));
	g_signal_connect(tree, "key-press-event",
					G_CALLBACK(on_treeviews_key_press_event), GINT_TO_POINTER(TREEVIEW_SYMBOL));

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), FALSE);

#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(tree), interface_prefs.show_symbol_list_expanders);
	if (! interface_prefs.show_symbol_list_expanders)
		gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(tree), 10);
#endif

	/* selection handling */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	/* callback for changed selection not necessary, will be handled by button-press-event */
}


static gboolean
on_default_tag_tree_button_press_event(GtkWidget *widget, GdkEventButton *event,
		gpointer user_data)
{
	if (event->button == 3)
	{
		on_treeviews_button_press_event(widget, event, GINT_TO_POINTER(TREEVIEW_SYMBOL));
	}
	return FALSE;
}


/* update = rescan the tags for doc->filename */
void treeviews_update_tag_list(GeanyDocument *doc, gboolean update)
{
	if (gtk_bin_get_child(GTK_BIN(tag_window)))
		gtk_container_remove(GTK_CONTAINER(tag_window), gtk_bin_get_child(GTK_BIN(tag_window)));

	if (tv.default_tag_tree == NULL)
	{
		GtkScrolledWindow *scrolled_window = GTK_SCROLLED_WINDOW(tag_window);
		GtkWidget *label;

		/* default_tag_tree is a GtkViewPort with a GtkLabel inside it */
		tv.default_tag_tree = gtk_viewport_new(
			gtk_scrolled_window_get_hadjustment(scrolled_window),
			gtk_scrolled_window_get_vadjustment(scrolled_window));
		label = gtk_label_new(_("No tags found"));
		gtk_misc_set_alignment(GTK_MISC(label), 0.1, 0.01);
		gtk_container_add(GTK_CONTAINER(tv.default_tag_tree), label);
		gtk_widget_show_all(tv.default_tag_tree);
		g_signal_connect(tv.default_tag_tree, "button-press-event",
			G_CALLBACK(on_default_tag_tree_button_press_event), NULL);
		g_object_ref((gpointer)tv.default_tag_tree);	/* to hold it after removing */
	}

	/* show default empty tag tree if there are no tags */
	if (doc == NULL || doc->file_type == NULL ||
		! filetype_has_tags(doc->file_type))
	{
		gtk_container_add(GTK_CONTAINER(tag_window), tv.default_tag_tree);
		return;
	}

	if (update)
	{	/* updating the tag list in the left tag window */
		if (doc->priv->tag_tree == NULL)
		{
			doc->priv->tag_store = gtk_tree_store_new(
				SYMBOLS_N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
			doc->priv->tag_tree = gtk_tree_view_new();
			prepare_taglist(doc->priv->tag_tree, doc->priv->tag_store);
			gtk_widget_show(doc->priv->tag_tree);
			g_object_ref((gpointer)doc->priv->tag_tree);	/* to hold it after removing */
		}

		doc->has_tags = symbols_recreate_tag_list(doc, SYMBOLS_SORT_USE_PREVIOUS);
	}

	if (doc->has_tags)
	{
		gtk_container_add(GTK_CONTAINER(tag_window), doc->priv->tag_tree);
	}
	else
	{
		gtk_container_add(GTK_CONTAINER(tag_window), tv.default_tag_tree);
	}
}


#if GTK_CHECK_VERSION(2, 12, 0)
gboolean on_treeviews_tooltip_queried(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode,
									  GtkTooltip *tooltip, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_view_get_tooltip_context(
			GTK_TREE_VIEW(widget), &x, &y, keyboard_mode, &model, NULL, &iter))
	{
		gchar *file_name = NULL;

		gtk_tree_model_get(GTK_TREE_MODEL(store_openfiles), &iter, 3, &file_name, -1);
		if (file_name != NULL)
		{
			gtk_tooltip_set_text(tooltip, file_name);
			g_free(file_name);
			return TRUE;
		}
	}

	return FALSE;
}
#endif


/* does some preparing things to the open files list widget */
static void prepare_openfiles(void)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	PangoFontDescription *pfd;
	GtkTreeSortable *sortable;

	tv.tree_openfiles = lookup_widget(main_widgets.window, "treeview6");

	/* store the short filename to show, and the index as reference,
	 * the colour (black/red/green) and the full name for the tooltip */
#if GTK_CHECK_VERSION(2, 12, 0)
	store_openfiles = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_COLOR, G_TYPE_STRING);
#else
	store_openfiles = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_COLOR);
#endif
	gtk_tree_view_set_model(GTK_TREE_VIEW(tv.tree_openfiles), GTK_TREE_MODEL(store_openfiles));
	g_object_unref(store_openfiles);

	/* set policy settings for the scolledwindow around the treeview again, because glade
	 * doesn't keep the settings */
	gtk_scrolled_window_set_policy(
			GTK_SCROLLED_WINDOW(lookup_widget(main_widgets.window, "scrolledwindow7")),
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Documents"), renderer,
															"text", 0, "foreground-gdk", 2, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tv.tree_openfiles), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tv.tree_openfiles), FALSE);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tv.tree_openfiles), FALSE);

	/* sort opened filenames in the store_openfiles treeview */
	sortable = GTK_TREE_SORTABLE(GTK_TREE_MODEL(store_openfiles));
	gtk_tree_sortable_set_sort_column_id(sortable, 0, GTK_SORT_ASCENDING);

	pfd = pango_font_description_from_string(interface_prefs.tagbar_font);
	gtk_widget_modify_font(tv.tree_openfiles, pfd);
	pango_font_description_free(pfd);

#if GTK_CHECK_VERSION(2, 12, 0)
	/* GTK 2.12 tooltips */
	gtk_widget_set_has_tooltip(tv.tree_openfiles, TRUE);
	g_signal_connect(tv.tree_openfiles, "query-tooltip",
						G_CALLBACK(on_treeviews_tooltip_queried), NULL);
#endif

	g_signal_connect(tv.tree_openfiles, "button-press-event",
			G_CALLBACK(on_treeviews_button_press_event), GINT_TO_POINTER(TREEVIEW_OPENFILES));

	/* selection handling */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(select, "changed", G_CALLBACK(on_openfiles_tree_selection_changed), NULL);
}


/* Also sets documents[idx]->iter.
 * This is called recursively in treeviews_openfiles_update_all(). */
void treeviews_openfiles_add(GeanyDocument *doc)
{
	GtkTreeIter *iter = &doc->priv->iter;

	gtk_list_store_append(store_openfiles, iter);
	treeviews_openfiles_update(doc);
}


void treeviews_openfiles_update(GeanyDocument *doc)
{
	gchar *basename;
	GdkColor *color = document_get_status_color(doc);

	if (interface_prefs.sidebar_openfiles_fullpath)
		basename = DOC_FILENAME(doc);
	else
		basename = g_path_get_basename(DOC_FILENAME(doc));
	gtk_list_store_set(store_openfiles, &doc->priv->iter,
#if GTK_CHECK_VERSION(2, 12, 0)
		0, basename, 1, doc, 2, color, 3, DOC_FILENAME(doc), -1);
#else
		0, basename, 1, doc, 2, color, -1);
#endif
	if (! interface_prefs.sidebar_openfiles_fullpath)
		g_free(basename);
}


void treeviews_openfiles_update_all()
{
	guint i;
	GeanyDocument *doc;

	gtk_list_store_clear(store_openfiles);
	for (i = 0; i < (guint) gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)); i++)
	{
		doc = document_get_from_page(i);
		if (doc == NULL)
			continue;

		treeviews_openfiles_add(doc);
	}
}


void treeviews_remove_document(GeanyDocument *doc)
{
	GtkTreeIter *iter = &doc->priv->iter;

	gtk_list_store_remove(store_openfiles, iter);

	if (GTK_IS_WIDGET(doc->priv->tag_tree))
	{
		gtk_widget_destroy(doc->priv->tag_tree);
		if (GTK_IS_TREE_VIEW(doc->priv->tag_tree))
		{
			/* Because it was ref'd in treeviews_update_tag_list, it needs unref'ing */
			g_object_unref((gpointer)doc->priv->tag_tree);
		}
		doc->priv->tag_tree = NULL;
	}
}


static void create_taglist_popup_menu(void)
{
	GtkWidget *item;

	tv.popup_taglist = gtk_menu_new();

	item = gtk_menu_item_new_with_mnemonic(_("Sort by _Name"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_taglist_tree_popup_clicked),
			GINT_TO_POINTER(SYMBOL_ACTION_SORT_BY_NAME));

	item = gtk_menu_item_new_with_mnemonic(_("Sort by _Appearance"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_taglist_tree_popup_clicked),
			GINT_TO_POINTER(SYMBOL_ACTION_SORT_BY_APPEARANCE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);

	mi.symbols_show_symbols = gtk_check_menu_item_new_with_mnemonic(_("Show S_ymbol List"));
	gtk_widget_show(mi.symbols_show_symbols);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), mi.symbols_show_symbols);
	g_signal_connect(mi.symbols_show_symbols, "activate",
			G_CALLBACK(on_list_symbol_activate), NULL);

	mi.symbols_show_documents = gtk_check_menu_item_new_with_mnemonic(_("Show _Document List"));
	gtk_widget_show(mi.symbols_show_documents);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), mi.symbols_show_documents);
	g_signal_connect(mi.symbols_show_documents, "activate",
			G_CALLBACK(on_list_document_activate), NULL);

	item = gtk_image_menu_item_new_with_mnemonic(_("H_ide Sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_taglist), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_taglist_tree_popup_clicked),
			GINT_TO_POINTER(SYMBOL_ACTION_HIDE_ALL));
}


static void on_openfiles_fullpath_activate(GtkCheckMenuItem *item, gpointer user_data)
{
	interface_prefs.sidebar_openfiles_fullpath = gtk_check_menu_item_get_active(item);
	treeviews_openfiles_update_all();
}


static void on_list_document_activate(GtkCheckMenuItem *item, gpointer user_data)
{
	interface_prefs.sidebar_openfiles_visible = gtk_check_menu_item_get_active(item);
	ui_sidebar_show_hide();
}


static void on_list_symbol_activate(GtkCheckMenuItem *item, gpointer user_data)
{
	interface_prefs.sidebar_symbol_visible = gtk_check_menu_item_get_active(item);
	ui_sidebar_show_hide();
}


static void create_openfiles_popup_menu(void)
{
	GtkWidget *item;

	tv.popup_openfiles = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock("gtk-close", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_REMOVE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);

	item = gtk_image_menu_item_new_from_stock("gtk-save", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_SAVE));

	item = gtk_image_menu_item_new_with_mnemonic(_("_Reload"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-revert-to-saved", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect(item, "activate",
			G_CALLBACK(on_openfiles_document_action), GINT_TO_POINTER(OPENFILES_ACTION_RELOAD));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);

	mi.documents_fullpath = gtk_check_menu_item_new_with_mnemonic(_("Show _Full Path Name"));
	gtk_widget_show(mi.documents_fullpath);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), mi.documents_fullpath);
	g_signal_connect(mi.documents_fullpath, "activate",
			G_CALLBACK(on_openfiles_fullpath_activate), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);

	mi.documents_show_symbols = gtk_check_menu_item_new_with_mnemonic(_("Show S_ymbol List"));
	gtk_widget_show(mi.documents_show_symbols);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), mi.documents_show_symbols);
	g_signal_connect(mi.documents_show_symbols, "activate",
			G_CALLBACK(on_list_symbol_activate), NULL);

	mi.documents_show_documents = gtk_check_menu_item_new_with_mnemonic(_("Show _Document List"));
	gtk_widget_show(mi.documents_show_documents);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), mi.documents_show_documents);
	g_signal_connect(mi.documents_show_documents, "activate",
			G_CALLBACK(on_list_document_activate), NULL);

	item = gtk_image_menu_item_new_with_mnemonic(_("H_ide Sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(tv.popup_openfiles), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_openfiles_hide_item_clicked), NULL);
}


/* compares the given data with the doc pointer from the selected row of openfiles
 * treeview, in case of a match the row is selected and TRUE is returned
 * (called indirectly from gtk_tree_model_foreach()) */
static gboolean tree_model_find_node(GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	GeanyDocument *doc;

	gtk_tree_model_get(GTK_TREE_MODEL(store_openfiles), iter, 1, &doc, -1);

	if (doc == data)
	{
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(tv.tree_openfiles), path, NULL, FALSE);
		return TRUE;
	}
	else return FALSE;
}


void treeviews_select_openfiles_item(GeanyDocument *doc)
{
	gtk_tree_model_foreach(GTK_TREE_MODEL(store_openfiles), tree_model_find_node, doc);
}


/* callbacks */

static void on_openfiles_document_action(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	GtkTreeModel *model;
	GeanyDocument *doc;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 1, &doc, -1);
		if (DOC_VALID(doc))
		{
			switch (GPOINTER_TO_INT(user_data))
			{
				case OPENFILES_ACTION_REMOVE:
				{
					document_close(doc);
					break;
				}
				case OPENFILES_ACTION_SAVE:
				{
					document_save_file(doc, FALSE);
					break;
				}
				case OPENFILES_ACTION_RELOAD:
				{
					on_toolbutton_reload_clicked(NULL, NULL);
					break;
				}
			}
		}
	}
}


static void on_openfiles_hide_item_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
	ui_prefs.sidebar_visible = FALSE;
	ui_sidebar_show_hide();
}


static gboolean change_focus(gpointer data)
{
	GeanyDocument *doc = data;

	/* idx might not be valid e.g. if user closed a tab whilst Geany is opening files */
	if (DOC_VALID(doc))
	{
		GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));
		GtkWidget *sci = GTK_WIDGET(doc->editor->sci);

		if (focusw == tv.tree_openfiles)
			gtk_widget_grab_focus(sci);
	}
	return FALSE;
}


static void on_openfiles_tree_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GeanyDocument *doc = NULL;

	/* use switch_notebook_page to ignore changing the notebook page because it is already done */
	if (gtk_tree_selection_get_selected(selection, &model, &iter) && ! ignore_callback)
	{
		gtk_tree_model_get(model, &iter, 1, &doc, -1);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook),
					gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook),
					(GtkWidget*) doc->editor->sci));
		g_idle_add((GSourceFunc) change_focus, doc);
	}
}


static void on_taglist_tree_popup_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data))
	{
		case SYMBOL_ACTION_SORT_BY_NAME:
		{
			GeanyDocument *doc = document_get_current();
			if (doc != NULL)
				doc->has_tags = symbols_recreate_tag_list(doc, SYMBOLS_SORT_BY_NAME);
			break;
		}
		case SYMBOL_ACTION_SORT_BY_APPEARANCE:
		{
			GeanyDocument *doc = document_get_current();
			if (doc != NULL)
				doc->has_tags = symbols_recreate_tag_list(doc, SYMBOLS_SORT_BY_APPEARANCE);
			break;
		}
		case SYMBOL_ACTION_HIDE:
		{
			interface_prefs.sidebar_symbol_visible = FALSE;
			ui_sidebar_show_hide();
			break;
		}
		case SYMBOL_ACTION_HIDE_ALL:
		{
			ui_prefs.sidebar_visible = FALSE;
			ui_sidebar_show_hide();
			break;
		}
	}
}


static gboolean on_taglist_tree_selection_changed(GtkTreeSelection *selection)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint line = 0;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		const TMTag *tag;

		gtk_tree_model_get(model, &iter, SYMBOLS_COLUMN_TAG, &tag, -1);
		if (!tag)
			return FALSE;

		line = tag->atts.entry.line;
		if (line > 0)
		{
			GeanyDocument *doc = document_get_current();

			navqueue_goto_line(doc, doc, line);
		}
	}
	return FALSE;
}


static gboolean on_treeviews_key_press_event(GtkWidget *widget, GdkEventKey *event,
											 gpointer user_data)
{
	if (event->keyval == GDK_Return ||
		event->keyval == GDK_ISO_Enter ||
		event->keyval == GDK_KP_Enter ||
		event->keyval == GDK_space)
	{
		GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		/* delay the query of selection state because this callback is executed before GTK
		 * changes the selection (g_signal_connect_after would be better but it doesn't work) */
		g_idle_add((GSourceFunc) on_taglist_tree_selection_changed, select);
	}
	return FALSE;
}


static gboolean on_treeviews_button_press_event(GtkWidget *widget, GdkEventButton *event,
												gpointer user_data)
{
	if (event->type == GDK_2BUTTON_PRESS && GPOINTER_TO_INT(user_data) == TREEVIEW_SYMBOL)
	{	/* double click on parent node(section) expands/collapses it */
		GtkTreeModel *model;
		GtkTreeSelection *selection;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			if (gtk_tree_model_iter_has_child(model, &iter))
			{
				GtkTreePath *path = gtk_tree_model_get_path(model, &iter);

				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
				else
					gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);

				gtk_tree_path_free(path);
				return TRUE;
			}
		}
	}
	else if (event->button == 1 && GPOINTER_TO_INT(user_data) == TREEVIEW_SYMBOL)
	{	/* allow reclicking of taglist treeview item */
		GtkTreeSelection *select = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		/* delay the query of selection state because this callback is executed before GTK
		 * changes the selection (g_signal_connect_after would be better but it doesn't work) */
		g_idle_add((GSourceFunc) on_taglist_tree_selection_changed, select);
	}
	else if (event->button == 3)
	{	/* popupmenu to hide or clear the active treeview */
		if (GPOINTER_TO_INT(user_data) == TREEVIEW_OPENFILES)
		{
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi.documents_show_documents),
				interface_prefs.sidebar_openfiles_visible);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi.documents_show_symbols),
				interface_prefs.sidebar_symbol_visible);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi.documents_fullpath),
				interface_prefs.sidebar_openfiles_fullpath);
			gtk_menu_popup(GTK_MENU(tv.popup_openfiles), NULL, NULL, NULL, NULL,
																event->button, event->time);
		}
		else if (GPOINTER_TO_INT(user_data) == TREEVIEW_SYMBOL)
		{
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi.symbols_show_documents),
				interface_prefs.sidebar_openfiles_visible);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mi.symbols_show_symbols),
				interface_prefs.sidebar_symbol_visible);
			gtk_menu_popup(GTK_MENU(tv.popup_taglist), NULL, NULL, NULL, NULL,
																event->button, event->time);
			return TRUE;	/* prevent selection changed signal for symbol tags */
		}
	}
	return FALSE;
}


void treeviews_init()
{
	tv.default_tag_tree = NULL;
	tag_window = lookup_widget(main_widgets.window, "scrolledwindow2");

	prepare_openfiles();
	create_taglist_popup_menu();
	create_openfiles_popup_menu();
}


