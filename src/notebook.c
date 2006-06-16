/*
 *      notebook.c - this file is part of Geany, a fast and lightweight IDE
 *
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

#define GEANY_DND_NOTEBOOK_TAB_TYPE	"geany_dnd_notebook_tab"

static const GtkTargetEntry drag_targets[] = 
{
	{GEANY_DND_NOTEBOOK_TAB_TYPE, GTK_TARGET_SAME_APP | GTK_TARGET_SAME_WIDGET, 0}
};


gboolean
notebook_drag_motion_cb(GtkWidget *notebook, GdkDragContext *dc,
	gint x, gint y, guint time, gpointer user_data);


void notebook_init()
{
	GtkWidget *notebook = app->notebook;

	g_object_set(G_OBJECT(notebook), "can-focus", FALSE, NULL);

	/* There is a bug with drag reordering notebook tabs.
	 * Clicking on a notebook tab, then making a selection in the
	 * Scintilla widget will cause a strange selection bug.
	 * It seems there is a conflict; the drag cursor is shown,
	 * and the selection is blocked; however, when releasing the
	 * mouse button, the selection continues.
	 * Maybe a bug in Scintilla 1.68? - ntrel */
#ifndef TEST_TAB_DND
	return;
#endif

	// Set up drag movement callback
	g_signal_connect(G_OBJECT(notebook), "drag-motion",
		G_CALLBACK(notebook_drag_motion_cb), NULL);

	// set up drag motion for moving notebook pages
	gtk_drag_source_set(notebook, GDK_BUTTON1_MASK,
		drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
	gtk_drag_dest_set(notebook, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
		drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
}


gboolean
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

		if (ok) gtk_notebook_reorder_child(notebook,
			gtk_notebook_get_nth_page(notebook, ncurr), ndest);
	}

	oldx = x; oldy = y;
	return FALSE;
}


/* x,y are co-ordinates local to the notebook, not including border padding
 * adapted from Epiphany absolute version in ephy-notebook.c, thanks
 * notebook tab label widgets must not be NULL */
gint notebook_find_tab_num_at_pos(GtkNotebook *notebook, gint x, gint y)
{
	GtkPositionType tab_pos;
	int page_num = 0;
	GtkWidget *page;

	// deal with less than 2 pages
	switch(gtk_notebook_get_n_pages(notebook))
	{case 0: return -1; case 1: return 0;}

	tab_pos = gtk_notebook_get_tab_pos(notebook); //which edge

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
