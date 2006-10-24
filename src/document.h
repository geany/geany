/*
 *      document.h - this file is part of Geany, a fast and lightweight IDE
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
 *  $Id$
 */


#ifndef GEANY_DOCUMENT_H
#define GEANY_DOCUMENT_H 1

#ifndef PLAT_GTK
#   define PLAT_GTK 1	// needed for ScintillaWidget.h
#endif

#include "Scintilla.h"
#include "ScintillaWidget.h"

#include "geany.h"
#include "filetypes.h"


typedef struct FileEncoding
{
	gchar 			*encoding;
	gboolean		 has_bom;
} FileEncoding;


/* structure for representing an open tab with all its related stuff. */
typedef struct document
{
	gboolean		 is_valid;
	gboolean		 has_tags;
	// the filename is encoded in UTF-8, but every GLibC function expect the locale representation
	gchar 			*file_name;
	gchar 			*encoding;
	gboolean		 has_bom;
	filetype		*file_type;
	TMWorkObject	*tm_file;
	ScintillaObject	*sci;
	GtkWidget		*tab_label;
	GtkWidget		*tabmenu_label;
	GtkWidget		*tag_tree;
	GtkTreeStore	*tag_store;
	GtkTreeIter		 iter;
	gboolean		 readonly;
	gboolean		 changed;
	gboolean		 do_overwrite;
	gboolean		 line_breaking;
	gboolean		 use_auto_indention;
	time_t			 last_check;	// to remember the last disk check
	time_t			 mtime;
	GTrashStack		*undo_actions;
	GTrashStack		*redo_actions;
	FileEncoding	 saved_encoding;
} document;


/* dynamic array of document elements to hold all information of the notebook tabs */
extern GArray *doc_array;

/* doc_list wraps doc_array so it can be used with C array syntax.
 * Example: doc_list[0].sci = NULL; */
#define doc_list ((document *)doc_array->data)

#define DOC_IDX_VALID(idx) \
	((idx) >= 0 && (guint)(idx) < doc_array->len && doc_list[idx].is_valid)


/* returns the index of the notebook page which has the given filename */
gint document_find_by_filename(const gchar*, gboolean is_tm_filename);


/* returns the index of the notebook page which has sci */
gint document_find_by_sci(ScintillaObject*);


/* returns the index of the given notebook page in the document list */
gint document_get_n_idx(guint page_num);

/* returns the index of the current notebook page in the document list */
gint document_get_cur_idx();

/* returns NULL if no documents are open */
document *document_get_current();


void document_init_doclist();

void document_finalize();


void document_set_text_changed(gint);


// Apply just the prefs that can change in the Preferences dialog
void document_apply_update_prefs(ScintillaObject *sci);


/* removes the given notebook tab and clears the related entry in the document list */
gboolean document_remove(guint page_num);


/* This creates a new document, by clearing the text widget and setting the
   current filename to NULL. */
void document_new_file(filetype *ft);


/* If idx is set to -1, it creates a new tab, opens the file from filename and
 * set the cursor to pos.
 * If idx is greater than -1, it reloads the file in the tab corresponding to
 * idx and set the cursor to position 0. In this case, filename should be NULL
 * It returns the idx of the opened file or -1 if an error occurred.
 */
int document_open_file(gint, const gchar*, gint, gboolean, filetype*, const gchar*);

int document_reload_file(gint idx, const gchar *forced_enc);


/* This saves the file.
 * When force is set then it is always saved, even if it is unchanged(useful when using Save As)
 * It returns whether the file could be saved or not. */
gboolean document_save_file(gint idx, gboolean force);

/* special search function, used from the find entry in the toolbar */
void document_search_bar_find(gint idx, const gchar *text, gint flags, gboolean find_button,
		gboolean inc);

/* General search function, used from the find dialog.
 * Returns -1 on failure or the start position of the matching text. */
gint document_find_text(gint idx, const gchar *text, gint flags, gboolean search_backwards);

gint document_replace_text(gint idx, const gchar *find_text, const gchar *replace_text,
	gint flags, gboolean search_backwards);

void document_replace_all(gint, const gchar*, const gchar*, gint, gboolean);

void document_replace_sel(gint, const gchar*, const gchar*, gint, gboolean);

void document_set_font(gint, const gchar*, gint);

void document_update_tag_list(gint, gboolean);

void document_set_filetype(gint, filetype*);

gchar *document_get_eol_mode(gint);

gchar *document_prepare_template(filetype *ft);

void document_fold_all(gint idx);

void document_unfold_all(gint idx);

void document_set_indicator(gint idx, gint line);

void document_clear_indicators(gint idx);

/* simple file print */
void document_print(gint idx);

void document_replace_tabs(gint idx);

void document_strip_trailing_spaces(gint idx);

void document_ensure_final_newline(gint idx);

void document_set_encoding(gint idx, const gchar *new_encoding);


/* own Undo / Redo implementation to be able to undo / redo changes
 * to the encoding or the Unicode BOM (which are Scintilla independet).
 * All Scintilla events are stored in the undo / redo buffer and are passed through. */

// available UNDO actions, UNDO_SCINTILLA is a pseudo action to trigger Scintilla's undo management
enum
{
	UNDO_SCINTILLA = 0,
	UNDO_ENCODING,
	UNDO_BOM,
	UNDO_ACTIONS_MAX
};

// an undo action, also used for redo actions
typedef struct
{
	GTrashStack *next;	// pointer to the next stack element(required for the GTrashStack)
	guint type;			// to identify the action
	gpointer *data; 	// the old value (before the change), in case of a redo action it contains
						// the new value
} undo_action;

gboolean document_can_undo(gint idx);

gboolean document_can_redo(gint idx);

void document_undo(gint idx);

void document_redo(gint idx);

void document_undo_add(gint idx, guint type, gpointer data);

#endif
