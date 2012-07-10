/*
 *      document-private.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

typedef enum
{
	FILE_OK,
	FILE_CHANGED, /* also valid for deleted files */
	FILE_IGNORE
}
FileDiskStatus;


typedef struct FileEncoding
{
	gchar 			*encoding;
	gboolean		 has_bom;
}
FileEncoding;


/* Private GeanyDocument fields */
typedef struct GeanyDocumentPrivate
{
	/* GtkLabel shown in the notebook header. */
	GtkWidget		*tab_label;
	/* GtkTreeView object for this document within the Symbols treeview of the sidebar. */
	GtkWidget		*tag_tree;
	/* GtkTreeStore object for this document within the Symbols treeview of the sidebar. */
	GtkTreeStore	*tag_store;
	/* Iter for this document within the Open Files treeview of the sidebar. */
	GtkTreeIter		 iter;
	/* Used by the Undo/Redo management code. */
	GTrashStack		*undo_actions;
	/* Used by the Undo/Redo management code. */
	GTrashStack		*redo_actions;
	/* Used so Undo/Redo works for encoding changes. */
	FileEncoding	 saved_encoding;
	gboolean		 colourise_needed;	/* use document.c:queue_colourise() instead */
	gint			 line_count;		/* Number of lines in the document. */
	gint			 symbol_list_sort_mode;
	/* indicates whether a file is on a remote filesystem, works only with GIO/GVfs */
	gboolean		 is_remote;
	/* File status on disk of the document */
	FileDiskStatus	 file_disk_status;
	/* Reference to a GFileMonitor object, only used when GIO file monitoring is used. */
	gpointer		 monitor;
	/* Time of the last disk check, only used when legacy file monitoring is used. */
	time_t			 last_check;
	/* Modification time of the document on disk, only used when legacy file monitoring is used. */
	time_t			 mtime;
	/* ID of the idle callback updating the tag list */
	guint			 tag_list_update_source;
}
GeanyDocumentPrivate;

#endif
