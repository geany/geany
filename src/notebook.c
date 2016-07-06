/*
 *      notebook.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/*
 * Notebook tab Drag 'n' Drop reordering and tab management.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "notebook.h"

#include "callbacks.h"
#include "documentprivate.h"
#include "geanyobject.h"
#include "geanypage.h"
#include "keybindings.h"
#include "main.h"
#include "sidebar.h"
#include "support.h"
#include "ui_utils.h"
#include "utils.h"

#include "gtkcompat.h"
#include "sciwrappers.h"
#include "math.h"

#include <gdk/gdkkeysyms.h>

enum {
	DROP_DATA_NOTEBOOK,
	DROP_DATA_FILENAME,
};

static GtkTargetEntry files_drop_targets[] = {
	{ "GTK_NOTEBOOK_TAB",  	GTK_TARGET_SAME_APP, DROP_DATA_NOTEBOOK},
	{ "STRING",				0, DROP_DATA_FILENAME },
	{ "UTF8_STRING",		0, DROP_DATA_FILENAME },
	{ "text/plain",			0, DROP_DATA_FILENAME },
	{ "text/uri-list",		0, DROP_DATA_FILENAME }
};

static const gsize MAX_MRU_DOCS = 20;
static GQueue *mru_tabs = NULL;
static guint mru_pos = 0;

static gboolean switch_in_progress = FALSE;
static GtkWidget *switch_dialog = NULL;
static GtkWidget *switch_dialog_label = NULL;

static void
on_window_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, GtkSelectionData *data, guint target_type,
		guint event_time, gpointer user_data);

static gboolean
on_window_drag_drop(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, guint event_time, gpointer user_data);

static void
swap_notebooks_cb(GtkButton *button, gpointer user_data);


static GeanyPage *page_by_sci(ScintillaObject *sci)
{
	GtkWidget *page = gtk_widget_get_parent(GTK_WIDGET(sci));
	while (!GEANY_IS_PAGE(page))
		page = gtk_widget_get_parent(page);
	return (GeanyPage *) page;
}


GtkNotebook *notebook_get_current_notebook(void)
{
	return document_get_notebook(document_get_current());
}


guint notebook_get_num_tabs(void)
{
	guint num = 0;
	GtkNotebook *notebook;

	foreach_notebook(notebook)
		num += gtk_notebook_get_n_pages(notebook);

	return num;
}


static void update_mru_tabs_head(GeanyPage *page)
{
	g_queue_remove(mru_tabs, page);
	g_queue_push_head(mru_tabs, page);

	if (g_queue_get_length(mru_tabs) > MAX_MRU_DOCS)
		g_queue_pop_tail(mru_tabs);
}


static void
on_page_selected(GeanyPage *page, gpointer data)
{
	GtkLabel      *label;
	GtkNotebook   *notebook;
	GtkWidget     *tab_widget;
	gchar         *markup;

	if (G_UNLIKELY(main_status.opening_session_files || main_status.closing_all))
		return;

	/* update MRU to make ctrl-tab between notebooks work */
	if (!switch_in_progress)
		update_mru_tabs_head(page);
}


gint notebook_order_compare(GtkNotebook *notebook1, GtkNotebook *notebook2)
{
	if (notebook1 == notebook2)
		return 0;

	if (notebook1 == notebook_get_primary())
		return -1;
	else
		return  1;
}


static GtkWidget *ui_minimal_dialog_new(GtkWindow *parent, const gchar *title)
{
	GtkWidget *dialog;

	dialog = gtk_window_new(GTK_WINDOW_POPUP);

	if (parent)
	{
		gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	}
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_set_name(dialog, "GeanyDialog");
	return dialog;
}


static gboolean is_modifier_key(guint keyval)
{
	switch (keyval)
	{
		case GDK_Shift_L:
		case GDK_Shift_R:
		case GDK_Control_L:
		case GDK_Control_R:
		case GDK_Meta_L:
		case GDK_Meta_R:
		case GDK_Alt_L:
		case GDK_Alt_R:
		case GDK_Super_L:
		case GDK_Super_R:
		case GDK_Hyper_L:
		case GDK_Hyper_R:
			return TRUE;
		default:
			return FALSE;
	}
}


/* because on_notebook_switch_page() only updates the mru list when not currently
 * switching the mru head must be updated after switching */
static gboolean on_key_release_event(GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
{
	/* user may have rebound keybinding to a different modifier than Ctrl, so check all */
	if (switch_in_progress && is_modifier_key(ev->keyval))
	{
		GeanyDocument *doc;
		GeanyPage     *page;

		switch_in_progress = FALSE;

		if (switch_dialog)
		{
			gtk_widget_destroy(switch_dialog);
			switch_dialog = NULL;
		}

		doc = document_get_current();
		page = GEANY_PAGE(gtk_widget_get_parent(GTK_WIDGET(doc->editor->sci)));
		update_mru_tabs_head((GeanyPage *) page);
		mru_pos = 0;
		document_check_disk_status(doc, TRUE);
	}
	return FALSE;
}


static GtkWidget *create_switch_dialog(void)
{
	GtkWidget *dialog, *widget, *vbox;

	dialog = ui_minimal_dialog_new(GTK_WINDOW(main_widgets.window), _("Switch to Document"));
	gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 200, -1);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);

	widget = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(vbox), widget);

	widget = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(widget), GTK_JUSTIFY_CENTER);
	gtk_container_add(GTK_CONTAINER(vbox), widget);
	switch_dialog_label = widget;

	g_signal_connect(dialog, "key-release-event", G_CALLBACK(on_key_release_event), NULL);
	return dialog;
}


static void update_filename_label(void)
{
	guint i;
	gchar *msg = NULL;
	guint queue_length;
	GeanyPage *page;

	if (!switch_dialog)
	{
		switch_dialog = create_switch_dialog();
		gtk_widget_show_all(switch_dialog);
	}

	queue_length = g_queue_get_length(mru_tabs);
	for (i = mru_pos; (i <= mru_pos + 3) && (page = g_queue_peek_nth(mru_tabs, i % queue_length)); i++)
	{
		const gchar *basename;

		basename = geany_page_get_label(page);
		if (i == mru_pos)
			msg = g_markup_printf_escaped ("<b>%s</b>", basename);
		else if (i % queue_length == mru_pos)    /* && i != mru_pos */
		{
			/* We have wrapped around and got to the starting document again */
			break;
		}
		else
		{
			gchar *markup;
			markup = g_markup_printf_escaped ("\n%s", basename);
			SETPTR(msg, g_strconcat(msg, markup, NULL));
			g_free(markup);
		}
	}
	gtk_label_set_markup(GTK_LABEL(switch_dialog_label), msg);
	g_free(msg);
}


static gboolean on_switch_timeout(G_GNUC_UNUSED gpointer data)
{
	if (!switch_in_progress || switch_dialog)
	{
		return FALSE;
	}

	update_filename_label();
	return FALSE;
}


void notebook_switch_tablastused(void)
{
	GeanyPage   *last_page;
	GtkNotebook *notebook;

	gint page_num;
	gboolean switch_start;

	/* don't bother for single documents (or even none) */
	if (g_queue_get_length(mru_tabs) < 2)
		return;

	switch_start = !switch_in_progress;

	mru_pos += 1;
	last_page = g_queue_peek_nth(mru_tabs, mru_pos);

	if (! last_page)
	{
		utils_beep();
		mru_pos = 0;
		last_page = g_queue_peek_nth(mru_tabs, mru_pos);
	}
	if (! last_page)
		return;

	switch_in_progress = TRUE;
	notebook = GTK_NOTEBOOK(gtk_widget_get_parent((GtkWidget *) last_page));
	page_num = gtk_notebook_page_num(notebook, (GtkWidget *) last_page);
	gtk_notebook_set_current_page(notebook, page_num);
	gtk_widget_grab_focus((GtkWidget *) gtk_container_get_focus_child((GtkContainer *) last_page));

	/* if there's a modifier key, we can switch back in MRU order each time unless
	 * the key is released */
	if (switch_start)
		g_timeout_add(600, on_switch_timeout, NULL);
	else
		update_filename_label();
}


gboolean notebook_switch_in_progress(void)
{
	return switch_in_progress;
}


static gboolean gtk_notebook_show_arrows(GtkNotebook *notebook)
{
	return gtk_notebook_get_scrollable(notebook);
#if 0
	/* To get this working we would need to define at least the first two fields of
	 * GtkNotebookPage since it is a private field. The better way would be to
	 * subclass GtkNotebook.
struct _FakeGtkNotebookPage
{
	GtkWidget *child;
	GtkWidget *tab_label;
};
 */
	gboolean show_arrow = FALSE;
	GList *children;

	if (! notebook->scrollable)
		return FALSE;

	children = notebook->children;
	while (children)
	{
		struct _FakeGtkNotebookPage *page = children->data;

		if (page->tab_label && ! gtk_widget_get_child_visible(page->tab_label))
			show_arrow = TRUE;

		children = children->next;
	}
	return show_arrow;
#endif
}


static gboolean is_position_on_tab_bar(GtkNotebook *notebook, GdkEventButton *event)
{
	GtkWidget *page;
	GtkWidget *tab;
	GtkWidget *nb;
	GtkPositionType tab_pos;
	gint scroll_arrow_hlength, scroll_arrow_vlength;
	gdouble x, y;

	page = gtk_notebook_get_nth_page(notebook, 0);
	g_return_val_if_fail(page != NULL, FALSE);

	tab = gtk_notebook_get_tab_label(notebook, page);
	g_return_val_if_fail(tab != NULL, FALSE);

	tab_pos = gtk_notebook_get_tab_pos(notebook);
	nb = GTK_WIDGET(notebook);

	gtk_widget_style_get(GTK_WIDGET(notebook), "scroll-arrow-hlength", &scroll_arrow_hlength,
		"scroll-arrow-vlength", &scroll_arrow_vlength, NULL);

	if (! gdk_event_get_coords((GdkEvent*) event, &x, &y))
	{
		x = event->x;
		y = event->y;
	}

	switch (tab_pos)
	{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		{
			if (event->y >= 0 && event->y <= gtk_widget_get_allocated_height(tab))
			{
				if (! gtk_notebook_show_arrows(notebook) || (
					x > scroll_arrow_hlength &&
					x < gtk_widget_get_allocated_width(nb) - scroll_arrow_hlength))
					return TRUE;
			}
			break;
		}
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		{
			if (event->x >= 0 && event->x <= gtk_widget_get_allocated_width(tab))
			{
				if (! gtk_notebook_show_arrows(notebook) || (
					y > scroll_arrow_vlength &&
					y < gtk_widget_get_allocated_height(nb) - scroll_arrow_vlength))
					return TRUE;
			}
		}
	}

	return FALSE;
}


static gint
swap_notebooks(GeanyPage *page)
{
	GtkWidget *parent, *new_parent;

	parent = gtk_widget_get_parent((GtkWidget *) page);

	if (parent == g_ptr_array_index(main_widgets.notebooks, 0))
		new_parent = g_ptr_array_index(main_widgets.notebooks, 1);
	else
		new_parent = g_ptr_array_index(main_widgets.notebooks, 0);

	return notebook_move_tab(page, GTK_NOTEBOOK(new_parent));
}


static void tab_bar_menu_activate_cb(GtkMenuItem *menuitem, GeanyPage *page)
{
	GtkNotebook *notebook_of_widget, *notebook_of_menu;

	notebook_of_menu   = GTK_NOTEBOOK(g_object_get_data(G_OBJECT(gtk_widget_get_parent(GTK_WIDGET(menuitem))), "notebook"));

	notebook_move_tab(page, notebook_of_menu);
	notebook_show_tab(page);
}


static void on_open_in_new_window_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = user_data;
	gchar *doc_path;

	g_return_if_fail(doc->is_valid);

	doc_path = utils_get_locale_from_utf8(doc->file_name);
	utils_start_new_geany_instance(doc_path);
	g_free(doc_path);
}


static void add_page_to_menu(GeanyPage *page, GtkMenu *menu)
{
	GtkWidget *item = gtk_image_menu_item_new_with_label(geany_page_get_label(page));
	GtkWidget *image = gtk_image_new_from_gicon(geany_page_get_icon(page),
	                                            GTK_ICON_SIZE_MENU);

	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);

	gtk_container_add(GTK_CONTAINER(menu), item);

	g_signal_connect_object(item, "activate",
	                        G_CALLBACK(notebook_show_tab), page, G_CONNECT_SWAPPED);
}


static void show_tab_bar_popup_menu(GdkEventButton *event, GtkNotebook *nb, GtkWidget *page)
{
	GtkWidget *menu_item;
	static GtkWidget *menu = NULL;
	GeanyDocument *doc = NULL;

	if (page)
	{
		gint page_num = gtk_notebook_page_num(nb, page);
		doc = document_get_from_page(page_num);
	}

	if (menu == NULL)
		menu = gtk_menu_new();

	/* clear the old menu items */
	gtk_container_foreach(GTK_CONTAINER(menu), (GtkCallback) gtk_widget_destroy, NULL);
	gtk_container_foreach(GTK_CONTAINER(nb), (GtkCallback) add_page_to_menu, menu);

	/* _full with NULL destroy so that the notebook won't be destroyed */
	g_object_set_data_full(G_OBJECT(menu), "notebook", nb, NULL);

	menu_item = gtk_separator_menu_item_new();
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);

	menu_item = ui_image_menu_item_new(GTK_STOCK_OPEN, _("Open in New _Window"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate",
		G_CALLBACK(on_open_in_new_window_activate), doc);
	/* disable if not on disk */
	if (doc == NULL || !doc->real_path)
		gtk_widget_set_sensitive(menu_item, FALSE);

	menu_item = ui_image_menu_item_new(GTK_STOCK_JUMP_TO, _("Open in other _notebook"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(swap_notebooks_cb), page);
	gtk_widget_set_sensitive(GTK_WIDGET(menu_item), (doc != NULL));

	menu_item = gtk_separator_menu_item_new();
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_CLOSE, NULL);
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect_object(menu_item, "activate", G_CALLBACK(gtk_widget_destroy), page, G_CONNECT_SWAPPED);
	gtk_widget_set_sensitive(GTK_WIDGET(menu_item), (doc != NULL));

	menu_item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("Close Ot_her Documents"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(on_close_other_documents1_activate), doc);
	gtk_widget_set_sensitive(GTK_WIDGET(menu_item), (doc != NULL));

	menu_item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("C_lose All"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(on_close_all1_activate), NULL);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}


static gboolean notebook_tab_bar_click_cb(GtkWidget *widget, GdkEventButton *event,
										  gpointer user_data)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(widget);
	GtkWidget *child = gtk_notebook_get_nth_page(notebook, gtk_notebook_get_current_page(notebook));

	gtk_widget_grab_focus(child);

	if (event->type == GDK_2BUTTON_PRESS)
	{
		GtkWidget *event_widget = gtk_get_event_widget((GdkEvent *) event);

		/* ignore events from the content of the page (impl. stolen from GTK2 tab scrolling)
		 * TODO: we should also ignore notebook's action widgets, but that's more work and
		 * we don't have any of them yet anyway -- and GTK 2.16 don't have those actions. */
		if (event_widget == NULL || event_widget == child || gtk_widget_is_ancestor(event_widget, child))
			return FALSE;

		if (is_position_on_tab_bar(notebook, event))
		{
			document_new_file(NULL, NULL, NULL);
			return TRUE;
		}
	}
	/* right-click is also handled here if it happened on the notebook tab bar but not
	 * on a tab directly */
	else if (event->button == 3)
	{
		show_tab_bar_popup_menu(event, notebook, NULL);
		return TRUE;
	}
	return FALSE;
}

static gint get_paned_size(GtkPaned *paned)
{
	switch (gtk_orientable_get_orientation(GTK_ORIENTABLE(paned)))
	{
		case GTK_ORIENTATION_HORIZONTAL:
			return gtk_widget_get_allocated_width(GTK_WIDGET(paned));
		case GTK_ORIENTATION_VERTICAL:
			return gtk_widget_get_allocated_height(GTK_WIDGET(paned));
		default:
			g_assert_not_reached();
	}
}

static void store_split_value(GtkPaned *paned)
{
	gint pos, max;

	pos = gtk_paned_get_position(paned);
	max = get_paned_size(paned);

	pos = nearbyint((10000.0 * pos)/max);

	g_object_set_data(G_OBJECT(paned), "split", GINT_TO_POINTER(pos));
}

gboolean on_handle_moved(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	store_split_value(GTK_PANED(widget));
}

static gboolean relayout_notebooks(gpointer data)
{
	GtkPaned *paned = GTK_PANED(data);
	GtkNotebook *n1, *n2;
	gint minimized1, minimized2;
	gint max;

	max = get_paned_size(paned);
	/* this must be called after the GtkPaned is realized and got size allocated */
	g_return_val_if_fail(gtk_widget_get_realized(GTK_WIDGET(paned)), FALSE);
	g_return_val_if_fail(max > 1, FALSE);

	n1 = GTK_NOTEBOOK(gtk_paned_get_child1(paned));
	minimized1 = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(n1), "minimized"));

	n2 = GTK_NOTEBOOK(gtk_paned_get_child2(paned));
	minimized2 = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(n2), "minimized"));

	g_signal_handlers_disconnect_by_func(paned, on_handle_moved, NULL);
	if (!minimized1 && !minimized2)
	{
		/* both notebooks show something: restore the user chosen handle position */
		gint ovr = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(paned), "override"));
		gint split = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(paned), "split"));
		/* the split value is stored as percent * 1000 */
		gfloat splitf = split/10000.0;
		if (G_UNLIKELY(ovr != -1))
		{
			gtk_paned_set_position(paned, ovr);
			store_split_value(paned);
			g_object_set_data(G_OBJECT(paned), "override", GINT_TO_POINTER(-1));
		}
		else if (split != -1)
		{
			gtk_paned_set_position(paned, (gint) (max*splitf));
			g_object_set_data(G_OBJECT(paned), "autosize", GINT_TO_POINTER(0));
		}
		else
			gtk_paned_set_position(paned, max/2);
		g_signal_connect_after(G_OBJECT(paned), "notify::position", G_CALLBACK(on_handle_moved), NULL);
	}
	else
	{
		/* one or both notebooks are emtpy. override position with automatic value
		 * but remember the postition to restore it later */
		gint pos = gtk_paned_get_position(paned);
		if (!g_object_get_data(G_OBJECT(paned), "autosize"))
			store_split_value(paned);
		if (minimized1 && minimized2)
			gtk_paned_set_position(paned, max/2);
		else if (minimized2)
			gtk_paned_set_position(paned, max);
		else
			gtk_paned_set_position(paned, 0);
		g_object_set_data(G_OBJECT(paned), "autosize", GINT_TO_POINTER(1));
	}

	return FALSE;
}

void notebook_restore_paned_position(gint position)
{
	GtkPaned *paned;

	paned = GTK_PANED(ui_lookup_widget(main_widgets.window, "hpaned2"));
	if (position != -1)
		g_object_set_data(G_OBJECT(paned), "override", GINT_TO_POINTER(position));
}


/* call this after the number of tabs in main_widgets.notebook changes. */
static void on_notebook_page_count_changed(GtkNotebook *notebook,
										   GeanyPage *page,
										   guint page_num,
										   gpointer user_data)
{
	gint added = GPOINTER_TO_INT(user_data);

	if (page && !main_status.quitting)
	{
		/* when adding a tab add it to the mru, even if it wasn't actually focused (for batch open) */
		if (added)
			update_mru_tabs_head(page);
		/* when closing a tab remove it from the mru */
		else
			g_queue_remove(mru_tabs, page);
	}

	switch (gtk_notebook_get_n_pages(notebook))
	{
		case 0:
			g_object_set_data(G_OBJECT(notebook), "minimized", GINT_TO_POINTER(1));
			break;

		case 1:
			g_object_set_data(G_OBJECT(notebook), "minimized", GINT_TO_POINTER(0));
			break;

		default:
			/* don't relayout unecessarily. need only to do this for 0->1 and 1->0 transistions */
			return;
	}
	/* Do not relayout on the very initial call */
	if (page)
		g_idle_add(relayout_notebooks, gtk_widget_get_parent(GTK_WIDGET(notebook)));
}


GPtrArray *notebook_init(void)
{
	const int max_notebooks = 2;
	gint i;
	GtkNotebook *notebook;
	GPtrArray *notebooks;
	static gchar notebook_group[] = "geany_notebooks";
	GtkWidget *paned;

	notebooks = g_ptr_array_sized_new(max_notebooks);
	g_ptr_array_add(notebooks, ui_lookup_widget(main_widgets.window, "notebook1"));
	g_ptr_array_add(notebooks, ui_lookup_widget(main_widgets.window, "notebook7"));

	paned = gtk_widget_get_parent(g_ptr_array_index(notebooks, 0));
	g_object_set_data(G_OBJECT(paned), "split",      GINT_TO_POINTER(-1));
	g_object_set_data(G_OBJECT(paned), "override",   GINT_TO_POINTER(-1));

	for (i = 0; i < max_notebooks; i++)
	{
		notebook = g_ptr_array_index(notebooks, i);

		g_signal_connect_after(notebook, "button-press-event",
			G_CALLBACK(notebook_tab_bar_click_cb), NULL);

		/* Enable DnD for dropping files and other notebook tabs into the notebook widget
		 *
		 * WORKAROUND: gtk's default behavior for GTK_DEST_DEFAULT_DROP leads to (pretty sure this
		 * is a GTK bug):
		 * Gtk-CRITICAL : gtk_selection_data_set: assertion `length <= 0' failed
		 *
		 * To workaround we install a custom "drag-drop" handler and mask GTK_DEST_DEFAULT_DROP
		 * out. The "drag-drop" handler mimics GTK code by just calling gtk_drag_get_data() */
		gtk_drag_dest_set(GTK_WIDGET(notebook), (GTK_DEST_DEFAULT_ALL & ~GTK_DEST_DEFAULT_DROP) ,
			files_drop_targets,	G_N_ELEMENTS(files_drop_targets),
			GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);
		g_signal_connect(G_OBJECT(notebook), "drag-data-received",
			G_CALLBACK(on_window_drag_data_received), NULL);
		g_signal_connect(G_OBJECT(notebook), "drag-drop",
			G_CALLBACK(on_window_drag_drop), NULL);
#if GTK_CHECK_VERSION(2,24,0)
		gtk_notebook_set_group_name(notebook, notebook_group);
#else
		gtk_notebook_set_group(notebook, (gpointer) notebook_group);
#endif

		g_signal_connect(notebook, "page-added",
			G_CALLBACK(on_notebook_page_count_changed), GINT_TO_POINTER(1));
		g_signal_connect(notebook, "page-removed",
			G_CALLBACK(on_notebook_page_count_changed), GINT_TO_POINTER(0));

		/* initialize for 0 pages */
		on_notebook_page_count_changed(notebook, NULL, 1, GINT_TO_POINTER(0));
	}

	mru_tabs = g_queue_new();
	/* in case the switch dialog misses an event while drawing the dialog */
	g_signal_connect(main_widgets.window, "key-release-event", G_CALLBACK(on_key_release_event), NULL);

	return notebooks;
}

void notebook_free(void)
{
	g_queue_free(mru_tabs);
}


static gboolean notebook_tab_click(GtkWidget *widget, GdkEventButton *event, GeanyPage *page)
{
	guint state;
	GtkNotebook *nb = GTK_NOTEBOOK(gtk_widget_get_parent(GTK_WIDGET(page)));

	/* toggle additional widgets on double click */
	if (event->type == GDK_2BUTTON_PRESS)
	{
		if (interface_prefs.notebook_double_click_hides_widgets)
			on_menu_toggle_all_additional_widgets1_activate(NULL, NULL);

		return TRUE; /* stop other handlers like notebook_tab_bar_click_cb() */
	}
	/* close tab on middle click */
	if (event->button == 2)
	{
		geany_page_try_close(page);
		return TRUE; /* stop other handlers like notebook_tab_bar_click_cb() */
	}
	/* switch last used tab on ctrl-click */
	state = keybindings_get_modifiers(event->state);
	if (event->button == 1 && state == GEANY_PRIMARY_MOD_MASK)
	{
		keybindings_send_command(GEANY_KEY_GROUP_NOTEBOOK,
			GEANY_KEYS_NOTEBOOK_SWITCHTABLASTUSED);
		return TRUE;
	}
	/* right-click is first handled here if it happened on a notebook tab */
	if (event->button == 3)
	{
		show_tab_bar_popup_menu(event, nb, (GtkWidget *) page);
		return TRUE;
	}

	return FALSE;
}


static void notebook_tab_close_button_style_set(GtkWidget *btn, GtkRcStyle *prev_style,
												gpointer data)
{
	gint w, h;

	gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(btn), GTK_ICON_SIZE_MENU, &w, &h);
	gtk_widget_set_size_request(btn, w + 2, h + 2);
}


/* Returns page number of notebook page, or -1 on error
 *
 * Note: the widget added to the notebook is *not* shown by this function, so you have to call
 * something like `gtk_widget_show(document_get_notebook_child(doc))` when finished setting up the
 * document.  This is necessary because when the notebook tab is added, the document isn't ready
 * yet, and we  need the notebook to emit ::switch-page after it actually is.  Actually this
 * doesn't prevent the signal to me emitted straight when we insert the page (this looks like a
 * GTK bug), but it emits it again when showing the child, and it's all we need. */
gint notebook_new_tab(GeanyPage *page, GtkNotebook *notebook)
{
	GtkWidget *hbox, *tab_widget, *focus_child;
	gint tabnum;
	gint cur_page;

	g_return_val_if_fail(page != NULL, -1);

	tab_widget = geany_page_get_tab_widget(page);
	/* get button press events for the tab label and the space between it and
	 * the close button, if any */
	g_signal_connect_object(tab_widget, "button-press-event", G_CALLBACK(notebook_tab_click), page, 0);

	g_signal_connect(page, "selected", G_CALLBACK(on_page_selected), NULL);

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), tab_widget, FALSE, FALSE, 0);

	if (file_prefs.show_tab_cross)
	{
		GtkWidget *image, *btn, *align;

		btn = gtk_button_new();
		gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
		gtk_button_set_focus_on_click(GTK_BUTTON(btn), FALSE);
		gtk_widget_set_name(btn, "geany-close-tab-button");

		image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
		gtk_container_add(GTK_CONTAINER(btn), image);

		align = gtk_alignment_new(1.0, 0.5, 0.0, 0.0);
		gtk_container_add(GTK_CONTAINER(align), btn);
		gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

		g_signal_connect_object(btn, "clicked", G_CALLBACK(geany_page_try_close), page, G_CONNECT_SWAPPED);
		/* button overrides event box, so make middle click on button also close tab */
		g_signal_connect_object(btn, "button-press-event", G_CALLBACK(notebook_tab_click), page, 0);
		/* handle style modification to keep button small as possible even when theme change */
		g_signal_connect(btn, "style-set", G_CALLBACK(notebook_tab_close_button_style_set), NULL);
	}

	gtk_widget_show_all(hbox);

	if (file_prefs.tab_order_beside)
		cur_page = gtk_notebook_get_current_page(notebook);
	else
		cur_page = file_prefs.tab_order_ltr ? -2 /* hack: -2 + 1 = -1, last page */ : 0;
	if (file_prefs.tab_order_ltr)
		tabnum = gtk_notebook_insert_page_menu(notebook, (GtkWidget *)page,
			hbox, NULL, cur_page + 1);
	else
		tabnum = gtk_notebook_insert_page_menu(notebook, (GtkWidget *)page,
			hbox, NULL, cur_page);

	/* enable tab DnD */
	gtk_notebook_set_tab_reorderable(notebook, (GtkWidget *) page, TRUE);
	gtk_notebook_set_tab_detachable(notebook, (GtkWidget *) page, TRUE);

	return tabnum;
}

gint notebook_move_tab(GeanyPage *page, GtkNotebook *new_notebook)
{
	GtkWidget *label, *child;
	GtkNotebook *current;
	gint page_num;

	child = (GtkWidget *) page;
	/* check the if tab is already on the target notebook */
	current = GTK_NOTEBOOK(gtk_widget_get_parent(child));
	/* It shouldn't be possible that it isn't in any notebook */
	g_return_val_if_fail(GTK_IS_NOTEBOOK(current), -1);

	if (new_notebook == current)
		return gtk_notebook_page_num(current, child);

	label  = gtk_notebook_get_tab_label(current, child);

	g_object_ref(child);
	g_object_ref(label);
	gtk_notebook_remove_page(current, gtk_notebook_page_num(current, child));
	page_num = gtk_notebook_append_page_menu(new_notebook, child, label, NULL);
	g_object_unref(child);
	g_object_unref(label);

	/* enable tab DnD */
	gtk_notebook_set_tab_reorderable(new_notebook, child, TRUE);
	gtk_notebook_set_tab_detachable(new_notebook, child, TRUE);

	return page_num;
}


static void
swap_notebooks_cb(GtkButton *button, gpointer user_data)
{
	swap_notebooks((GeanyPage *) user_data);
}


/* Bring the tab in its notebook in foreground and focus */
void notebook_show_tab(GeanyPage *page)
{
	GtkNotebook *nb = GTK_NOTEBOOK(gtk_widget_get_parent((GtkWidget *) page));

	gtk_notebook_set_current_page(nb, gtk_notebook_page_num(nb, (GtkWidget *) page));
}


/* Removes the page from its notebook and destroys it */
void notebook_destroy_tab(GeanyPage *page)
{
	GtkNotebook *nb = GTK_NOTEBOOK(gtk_widget_get_parent((GtkWidget *) page));

	gint page_num = gtk_notebook_page_num(nb, GTK_WIDGET(page));
	gtk_notebook_remove_page(nb, page_num);
}


/* Like notebook_destroy_tab() but it performs additional steps like selecting the new page
 * switched and other post-remove stuff.
 * Always use this instead of gtk_notebook_remove_page() (except during shutdown) */
void notebook_remove_tab(GeanyPage *page)
{
	GtkNotebook *nb = GTK_NOTEBOOK(gtk_widget_get_parent(GTK_WIDGET(page)));
	gint cur_page = gtk_notebook_get_current_page(nb);
	gint page_num = gtk_notebook_page_num(nb, (GtkWidget *) page);

	if (page_num == cur_page)
	{
		if (file_prefs.tab_order_ltr)
			cur_page += 1;
		else if (page_num > 0) /* never go negative, it would select the last page */
			cur_page -= 1;

		if (file_prefs.tab_close_switch_to_mru)
		{
			GeanyPage *npage = g_queue_peek_nth(mru_tabs, 1);
			if (npage)
				cur_page = gtk_notebook_page_num(nb, (GtkWidget *)npage);
		}

		gtk_notebook_set_current_page(nb, cur_page);
	}

	/* now remove the page (so we don't temporarily switch to the previous page) */
	gtk_notebook_remove_page(nb, page_num);
}

static gboolean
on_window_drag_drop(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, guint event_time, gpointer user_data)
{
	GdkAtom target = gtk_drag_dest_find_target(widget, drag_context, NULL);

	if (target != GDK_NONE)
		gtk_drag_get_data(widget, drag_context, target, event_time);

	return TRUE;
}

static void
on_window_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, GtkSelectionData *data, guint target_type,
		guint event_time, gpointer user_data)
{
	guint length;
	gboolean success = FALSE;
	/* If a notebook tab is gonna dropped just let
	 * gtk do its magic */
	if (target_type == DROP_DATA_NOTEBOOK)
		success = TRUE;
	/* otherwise it is a filename and we should handle it */
	else if (target_type == DROP_DATA_FILENAME)
	{
		length = gtk_selection_data_get_length(data);
		if (length > 0 && gtk_selection_data_get_format(data) == 8)
		{
			const gchar *seldata = (const gchar *)gtk_selection_data_get_data(data);
			if (utils_is_uri(seldata))
			{
				document_open_file_list(seldata, length, GTK_NOTEBOOK(widget));
				success = TRUE;
			}
		}
	}
	else
		geany_debug("Unknown drag and drop data\n");

	gtk_drag_finish(drag_context, success, FALSE, event_time);
}
