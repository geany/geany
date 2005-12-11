/*
 *      treeviws.h
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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



typedef struct treeviews {
	//GtkListStore	*store_taglist;
	GtkTreeStore	*store_taglist;
	GtkListStore	*store_openfiles;
	GtkWidget		*tree_taglist;
	GtkWidget		*tree_openfiles;
	GtkWidget		*popup_openfiles;
	GtkTreeIter		 tag_function;
	GtkTreeIter		 tag_macro;
	GtkTreeIter		 tag_member;
	GtkTreeIter		 tag_variable;
	GtkTreeIter		 tag_namespace;
	GtkTreeIter		 tag_struct;
	GtkTreeIter		 tag_other;
} treeviews;

treeviews tv;



void treeviews_prepare_taglist(void);

gint treeviews_sort_tag_list(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data);

void treeviews_init_tag_list(void);

void treeviews_prepare_openfiles(void);

GtkTreeIter treeviews_openfiles_add(gint idx, const gchar *string);

void treeviews_openfiles_remove(GtkTreeIter iter);

void treeviews_create_openfiles_popup_menu(void);

/* compares the given data (GINT_TO_PONTER(idx)) with the idx from the selected row of openfiles
 * treeview, in case of a match the row is selected and TRUE is returned
 * (called indirectly from gtk_tree_model_foreach()) */
gboolean treeviews_find_node(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);

#endif
