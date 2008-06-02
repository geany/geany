/*
 *      document-private.h
 *
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#ifndef GEANY_DOCUMENT_PRIVATE_H
#define GEANY_DOCUMENT_PRIVATE_H

/* available UNDO actions, UNDO_SCINTILLA is a pseudo action to trigger Scintilla's
 * undo management */
enum
{
	UNDO_SCINTILLA = 0,
	UNDO_ENCODING,
	UNDO_BOM,
	UNDO_ACTIONS_MAX
};


typedef struct FileEncoding
{
	gchar 			*encoding;
	gboolean		 has_bom;
} FileEncoding;


#define DOCUMENT(doc_ptr) ((Document*)doc_ptr)

/* This type 'inherits' from GeanyDocument so Document* can be cast to GeanyDocument*. */
typedef struct Document
{
	struct GeanyDocument public;	/* must come first */

	/* GtkLabel shown in the notebook header. */
	GtkWidget		*tab_label;
	/* GtkLabel shown in the notebook right-click menu. */
	GtkWidget		*tabmenu_label;
	/* GtkTreeView object for this %document within the Symbols treeview of the sidebar. */
	GtkWidget		*tag_tree;
	/* GtkTreeStore object for this %document within the Symbols treeview of the sidebar. */
	GtkTreeStore	*tag_store;
	/* Iter for this %document within the Open Files treeview of the sidebar. */
	GtkTreeIter		 iter;
	/* Used by the Undo/Redo management code. */
	GTrashStack		*undo_actions;
	/* Used by the Undo/Redo management code. */
	GTrashStack		*redo_actions;
	/* Used so Undo/Redo works for encoding changes. */
	FileEncoding	 saved_encoding;
}
Document;

#endif
