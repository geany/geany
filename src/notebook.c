/*
 *      notebook.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

#include "geany.h"
#include "notebook.h"
#include "document.h"
#include "ui_utils.h"
#include "treeviews.h"

#define GEANY_DND_NOTEBOOK_TAB_TYPE	"geany_dnd_notebook_tab"

static const GtkTargetEntry drag_targets[] =
{
	{GEANY_DND_NOTEBOOK_TAB_TYPE, GTK_TARGET_SAME_APP | GTK_TARGET_SAME_WIDGET, 0}
};


static gboolean
notebook_drag_motion_cb(GtkWidget *widget, GdkDragContext *dc,
	gint x, gint y, guint time, gpointer user_data);

static void
notebook_page_reordered_cb(GtkNotebook *notebook, GtkWidget *child, guint page_num,
	gpointer user_data);

#if ! GTK_CHECK_VERSION(2, 8, 0)
static gboolean
notebook_motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event,
	gpointer user_data);
#endif

static gint
notebook_find_tab_num_at_pos(GtkNotebook *notebook, gint x, gint y);

static void
notebook_tab_close_clicked_cb(GtkButton *button, gpointer user_data);

static void setup_tab_dnd();


static void focus_sci(GtkWidget *widget, gpointer user_data)
{
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx)) return;

	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
}


void notebook_init()
{
	// focus the current document after clicking on a tab
	g_signal_connect_after(G_OBJECT(app->notebook), "button-release-event",
		G_CALLBACK(focus_sci), NULL);

	setup_tab_dnd();
}


static void setup_tab_dnd()
{
	GtkWidget *notebook = app->notebook;

	/* Due to a segfault with manual tab DnD setup on GTK 2.10, we must
	*  use the built in gtk_notebook_set_tab_reorderable from GTK 2.10.
	*  This means a binary compiled against < 2.10 but run on >= 2.10
	*  will not have tab DnD support, but this is necessary until
	*  there is a fix for the older tab DnD code or GTK 2.10. */
	if (gtk_check_version(2, 10, 0) == NULL) // null means version ok
	{
#if GTK_CHECK_VERSION(2, 10, 0)
		g_signal_connect(G_OBJECT(notebook), "page-reordered",
			G_CALLBACK(notebook_page_reordered_cb), NULL);
#endif
		return;
	}

	// Set up drag movement callback
	g_signal_connect(G_OBJECT(notebook), "drag-motion",
		G_CALLBACK(notebook_drag_motion_cb), NULL);

	/* There is a bug on GTK 2.6 with drag reordering of notebook tabs.
	 * Clicking (not dragging) on a notebook tab, then making a selection in the
	 * Scintilla widget will cause a strange selection bug.
	 * It seems there is a conflict; the drag cursor is shown,
	 * and the selection is blocked; however, when releasing the
	 * mouse button, the selection continues.
	 * Bug is present with gtk+2.6.8, not gtk+2.8.x - ntrel */
#if ! GTK_CHECK_VERSION(2, 8, 0)
	// handle higher gtk+ runtime than build environment
	if (gtk_check_version(2, 8, 0) != NULL) // null means version ok
	{
		// workaround GTK+2.6 drag start bug when over sci widget:
		gtk_widget_add_events(notebook, GDK_POINTER_MOTION_MASK);
		g_signal_connect(G_OBJECT(notebook), "motion-notify-event",
			G_CALLBACK(notebook_motion_notify_event_cb), NULL);
	}
#endif

	// set up drag motion for moving notebook pages
	gtk_drag_dest_set(notebook, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
		drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
	// set drag source, but for GTK+2.6 it's changed in motion-notify-event handler
	gtk_drag_source_set(notebook, GDK_BUTTON1_MASK,
		drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
}


#if ! GTK_CHECK_VERSION(2, 8, 0)
/* This is used to disable tab DnD when the cursor is over the
 * Scintilla widget, and re-enable tab DnD when over the notebook tabs
 */
static gboolean
notebook_motion_notify_event_cb(GtkWidget *widget, GdkEventMotion *event,
	gpointer user_data)
{
	static gboolean drag_enabled = TRUE; // stores current state
	GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook),
			gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook)));

	if (page == NULL || event->x < 0 || event->y < 0) return FALSE;

	if (event->window == page->window) // cursor over sci widget
	{
		if (drag_enabled) gtk_drag_source_unset(widget); // disable
		drag_enabled = FALSE;
	}
	else // assume cursor over notebook tab
	{
		if (! drag_enabled)
			gtk_drag_source_set(widget, GDK_BUTTON1_MASK,
				drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
		drag_enabled = TRUE;
	}
	return FALSE; // propagate event
}
#endif


static void
notebook_page_reordered_cb(GtkNotebook *notebook, GtkWidget *child, guint page_num,
	gpointer user_data)
{
	treeviews_openfiles_update_all();
}


static gboolean
notebook_drag_motion_cb(GtkWidget *widget, GdkDragContext *dc,
	gint x, gint y, guint time, gpointer user_data)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(widget);
	static gint oldx, oldy; // for determining direction of mouse drag
	gint ndest = notebook_find_tab_num_at_pos(notebook, x, y);
	gint ncurr = gtk_notebook_get_current_page(notebook);

	if (ndest >= 0)
	if (ndest != ncurr)
	{
		gboolean ok = FALSE;
		// prevent oscillation between non-homogeneous sized tabs
		switch(gtk_notebook_get_tab_pos(notebook))
		{
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		ok = ((ndest > ncurr) && (y > oldy)) || ((ndest < ncurr) && (y < oldy));
		break;

		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		ok = ((ndest > ncurr) && (x > oldx)) || ((ndest < ncurr) && (x < oldx));
		break;
		}

		if (ok)
		{
			gtk_notebook_reorder_child(notebook,
							gtk_notebook_get_nth_page(notebook, ncurr), ndest);
			notebook_page_reordered_cb(NULL, NULL, ndest, NULL);
		}
	}

	oldx = x; oldy = y;
	return FALSE;
}


/* Adapted from Epiphany absolute version in ephy-notebook.c, thanks.
 * x,y are co-ordinates local to the notebook (not including border padding)
 * notebook tab label widgets must not be NULL.
 * N.B. This only checks the dimension that the tabs are in,
 * e.g. for GTK_POS_TOP it does not check the y coordinate. */
static gint
notebook_find_tab_num_at_pos(GtkNotebook *notebook, gint x, gint y)
{
	GtkPositionType tab_pos;
	int page_num = 0;
	GtkWidget *page;

	// deal with less than 2 pages
	switch(gtk_notebook_get_n_pages(notebook))
	{case 0: return -1; case 1: return 0;}

	tab_pos = gtk_notebook_get_tab_pos(notebook); // which edge

	while ((page = gtk_notebook_get_nth_page(notebook, page_num)))
	{
		gint max_x, max_y;
		GtkWidget *tab = gtk_notebook_get_tab_label(notebook, page);

		g_return_val_if_fail(tab != NULL, -1);

		if (!GTK_WIDGET_MAPPED(GTK_WIDGET(tab)))
		{ // skip hidden tabs, e.g. tabs scrolled out of view
			page_num++;
			continue;
		}

		// subtract notebook pos to remove possible border padding
		max_x = tab->allocation.x + tab->allocation.width -
			GTK_WIDGET(notebook)->allocation.x;
		max_y = tab->allocation.y + tab->allocation.height -
			GTK_WIDGET(notebook)->allocation.y;

		if (((tab_pos == GTK_POS_TOP)
			|| (tab_pos == GTK_POS_BOTTOM))
			&&(x<=max_x)) return page_num;
		else if (((tab_pos == GTK_POS_LEFT)
			|| (tab_pos == GTK_POS_RIGHT))
			&& (y<=max_y)) return page_num;

		page_num++;
	}
	return -1;
}


/* Returns index of notebook page, or -1 on error */
gint notebook_new_tab(gint doc_idx, gchar *title, GtkWidget *page)
{
	GtkWidget *hbox, *but;
	GtkWidget *align;
	gint tabnum;
	document *this = &(doc_list[doc_idx]);

	g_return_val_if_fail(doc_idx >= 0 && this != NULL, -1);

	this->tab_label = gtk_label_new(title);

	hbox = gtk_hbox_new(FALSE, 0);
	but = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(but),
		ui_new_image_from_inline(GEANY_IMAGE_SMALL_CROSS, FALSE));
	gtk_container_set_border_width(GTK_CONTAINER(but), 0);
	gtk_widget_set_size_request(but, 19, 18);

	align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
	gtk_container_add(GTK_CONTAINER(align), but);

	gtk_button_set_relief(GTK_BUTTON(but), GTK_RELIEF_NONE);
	gtk_box_pack_start(GTK_BOX(hbox), this->tab_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);
	gtk_widget_show_all(hbox);

	this->tabmenu_label = gtk_label_new(title);
	gtk_misc_set_alignment(GTK_MISC(this->tabmenu_label), 0.0, 0);

	if (app->tab_order_ltr)
		tabnum = gtk_notebook_append_page_menu(GTK_NOTEBOOK(app->notebook),
			GTK_WIDGET(page), hbox, this->tabmenu_label);
	else
		tabnum = gtk_notebook_insert_page_menu(GTK_NOTEBOOK(app->notebook),
			GTK_WIDGET(page), hbox, this->tabmenu_label, 0);

	// signal for clicking the tab-close button
	g_signal_connect(G_OBJECT(but), "clicked",
		G_CALLBACK(notebook_tab_close_clicked_cb), page);

	// This is where tab DnD is enabled for GTK 2.10 and higher
#if GTK_CHECK_VERSION(2, 10, 0)
	if (gtk_check_version(2, 10, 0) == NULL) // null means version ok
	{
		gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(app->notebook), page, TRUE);
	}
#endif
	return tabnum;
}


static void
notebook_tab_close_clicked_cb(GtkButton *button, gpointer user_data)
{
	gint cur_page = gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook),
		GTK_WIDGET(user_data));
	document_remove(cur_page);
}
