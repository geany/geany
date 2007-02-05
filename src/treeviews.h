/*
 *      treeviws.h
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */



#ifndef GEANY_TREEVIEWS_H
#define GEANY_TREEVIEWS_H 1


struct SidebarTreeviews
{
	GtkListStore	*store_openfiles;
	GtkWidget		*tree_openfiles;
	GtkWidget		*popup_taglist;
	GtkWidget		*popup_openfiles;
} tv;


void treeviews_update_tag_list(gint idx, gboolean update);

void treeviews_prepare_openfiles();

void treeviews_openfiles_add(gint idx);

void treeviews_openfiles_update(gint idx);

void treeviews_openfiles_update_all();

void treeviews_remove_document(gint idx);

void treeviews_create_openfiles_popup_menu();

void treeviews_create_taglist_popup_menu();

/* compares the given data (GINT_TO_PONTER(idx)) with the idx from the selected row of openfiles
 * treeview, in case of a match the row is selected and TRUE is returned
 * (called indirectly from gtk_tree_model_foreach()) */
gboolean treeviews_find_node(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);

#endif
