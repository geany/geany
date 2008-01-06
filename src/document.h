/*
 *      document.h - this file is part of Geany, a fast and lightweight IDE
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
 *  $Id$
 */


#ifndef GEANY_DOCUMENT_H
#define GEANY_DOCUMENT_H 1

#include "Scintilla.h"
#include "ScintillaWidget.h"


typedef struct FileEncoding
{
	gchar 			*encoding;
	gboolean		 has_bom;
} FileEncoding;


/* Structure for representing an open tab with all its related stuff. */
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
	GtkTreeIter		 iter;	// open files item for this document
	gboolean		 readonly;
	gboolean		 changed;
	gboolean		 line_wrapping;
	gboolean		 auto_indent;
	gfloat			 scroll_percent;	// % to scroll view by on paint, if positive.
	time_t			 last_check;	// to remember the last disk check
	time_t			 mtime;
	GTrashStack		*undo_actions;
	GTrashStack		*redo_actions;
	FileEncoding	 saved_encoding;
	gboolean		 use_tabs;
} document;


/* dynamic array of document elements to hold all information of the notebook tabs */
extern GArray *doc_array;

/* doc_list wraps doc_array so it can be used with C array syntax.
 * Example: doc_list[0].sci = NULL; */
#define doc_list ((document *)doc_array->data)

#define DOC_IDX_VALID(idx) \
	((idx) >= 0 && (guint)(idx) < doc_array->len && doc_list[idx].is_valid)

#define DOC_FILENAME(doc_idx) \
	((doc_list[doc_idx].file_name != NULL) ? \
	(doc_list[doc_idx].file_name) : GEANY_STRING_UNTITLED)


/* returns the document index which has the given filename.
 * is_tm_filename is needed when passing TagManager filenames because they are
 * dereferenced, and would not match the link filename. */
gint document_find_by_filename(const gchar *filename, gboolean is_tm_filename);


/* returns the document index which has sci */
gint document_find_by_sci(ScintillaObject *sci);


/* returns the index of the notebook page from the document index */
gint document_get_notebook_page(gint doc_idx);

/* returns the index of the given notebook page in the document list */
gint document_get_n_idx(guint page_num);

/* returns the index of the current notebook page in the document list */
gint document_get_cur_idx();

/* returns NULL if no documents are open */
document *document_get_current();


void document_init_doclist();

void document_finalize();


void document_set_text_changed(gint idx);


// Apply just the prefs that can change in the Preferences dialog
void document_apply_update_prefs(gint idx);


/* removes the given notebook tab and clears the related entry in the document list */
gboolean document_remove(guint page_num);


/* See document.c. */
gint document_new_file(const gchar *filename, filetype *ft, const gchar *text);

gint document_clone(gint old_idx, const gchar *utf8_filename);


/* See document.c. */
gint document_open_file(const gchar *locale_filename, gboolean readonly,
		filetype *ft, const gchar *forced_enc);

gint document_open_file_full(gint idx, const gchar *filename, gint pos, gboolean readonly,
		filetype *ft, const gchar *forced_enc);

/* Takes a new line separated list of filename URIs and opens each file.
 * length is the length of the string or -1 if it should be detected */
void document_open_file_list(const gchar *data, gssize length);

/* Takes a linked list of filename URIs and opens each file, ensuring the newly opened
 * documents and existing documents (if necessary) are only colourised once. */
void document_open_files(const GSList *filenames, gboolean readonly, filetype *ft,
		const gchar *forced_enc);


gboolean document_reload_file(gint idx, const gchar *forced_enc);


/* This saves the file.
 * When force is set then it is always saved, even if it is unchanged(useful when using Save As)
 * It returns whether the file could be saved or not. */
gboolean document_save_file(gint idx, gboolean force);


gboolean document_search_bar_find(gint idx, const gchar *text, gint flags, gboolean inc);

/* General search function, used from the find dialog.
 * Returns -1 on failure or the start position of the matching text. */
gint document_find_text(gint idx, const gchar *text, gint flags, gboolean search_backwards,
	gboolean scroll, GtkWidget *parent);

gint document_replace_text(gint idx, const gchar *find_text, const gchar *replace_text,
	gint flags, gboolean search_backwards);

gboolean document_replace_all(gint idx, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean escaped_chars);

void document_replace_sel(gint idx, const gchar *find_text, const gchar *replace_text, gint flags,
						  gboolean escaped_chars);

void document_set_font(gint idx, const gchar *font_name, gint size);

void document_update_tag_list(gint idx, gboolean update);

/* sets the filetype of the document (sets syntax highlighting and tagging) */
void document_set_filetype(gint idx, filetype *type);

gchar *document_get_eol_mode(gint idx);

void document_fold_all(gint idx);

void document_unfold_all(gint idx);

void document_set_indicator(gint idx, gint line);

void document_clear_indicators(gint idx);

void document_replace_tabs(gint idx);

void document_strip_line_trailing_spaces(gint idx, gint line);

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


GdkColor *document_get_status(gint idx);


void document_delay_colourise();

void document_colourise_new();

void document_insert_colour(gint idx, const gchar *colour);

void document_set_use_tabs(gint idx, gboolean use_tabs);

void document_set_line_wrapping(gint idx, gboolean wrap);

#endif
