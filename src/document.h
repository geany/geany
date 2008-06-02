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

/**
 *  @file document.h
 *  Document related actions: new, save, open, etc.
 *  Also Scintilla search actions.
 **/


#ifndef GEANY_DOCUMENT_H
#define GEANY_DOCUMENT_H 1

#include "Scintilla.h"
#include "ScintillaWidget.h"

#if defined(G_OS_WIN32)
# define GEANY_DEFAULT_EOL_CHARACTER SC_EOL_CRLF
#elif defined(G_OS_UNIX)
# define GEANY_DEFAULT_EOL_CHARACTER SC_EOL_LF
#else
# define GEANY_DEFAULT_EOL_CHARACTER SC_EOL_CR
#endif


typedef struct GeanyFilePrefs
{
	gint			default_new_encoding;
	gint			default_open_encoding;
	gboolean		final_new_line;
	gboolean		strip_trailing_spaces;
	gboolean		replace_tabs;
	gboolean		tab_order_ltr;
	gboolean		show_tab_cross;
	guint			mru_length;
	gint			default_eol_character;
	gint			disk_check_timeout;
}
GeanyFilePrefs;

extern GeanyFilePrefs file_prefs;


/**
 *  Structure for representing an open tab with all its properties.
 **/
typedef struct GeanyDocument
{
	/** General flag to represent this document is active and all properties are set correctly. */
	gboolean		 is_valid;
	/** Whether this %document support source code symbols(tags) to show in the sidebar. */
	gboolean		 has_tags;
	/** The UTF-8 encoded file name. Be careful glibc and GLib functions expect the locale
	    representation of the file name which can be different from this.
	    For conversion into locale encoding for use with file functions of GLib, you can use
	    @ref utils_get_locale_from_utf8. */
	gchar 			*file_name;
	/** The encoding of the %document, must be a valid string representation of an encoding, can
	 *  be retrieved with @ref encodings_get_charset_from_index. */
	gchar 			*encoding;
	/** Internally used flag to indicate whether the file of this %document has a byte-order-mark. */
	gboolean		 has_bom;
	/** The filetype for this %document, it's only a reference to one of the elements of the global
	 *  filetypes array. */
	GeanyFiletype	*file_type;
	/** TMWorkObject object for this %document. */
	TMWorkObject	*tm_file;
	/** The Scintilla object for this %document. */
	ScintillaObject	*sci;
	/** Whether this %document is read-only. */
	gboolean		 readonly;
	/** Whether this %document has been changed since it was last saved. */
	gboolean		 changed;
	/** %Document-specific line wrapping setting. */
	gboolean		 line_wrapping;
	/** %Document-specific indentation setting. */
	gboolean		 auto_indent;
	/** Percentage to scroll view by on paint, if positive. */
	gfloat			 scroll_percent;
	/** Time of the last disk check. */
	time_t			 last_check;
	/** Modification time of this %document on disk. */
	time_t			 mtime;
	/** %Document-specific indentation setting. */
	gboolean		 use_tabs;
	gboolean		 line_breaking;	/**< Whether to split long lines as you type. */
}
GeanyDocument;


/** Dynamic array of GeanyDocument pointers holding information about the notebook tabs. */
extern GPtrArray *documents_array;

/**
 *  This wraps documents_array so it can be used with C array syntax.
 *  Example: documents[0]->sci = NULL;
 **/
#define documents ((GeanyDocument **)documents_array->pdata)

/**
 *  DOC_IDX_VALID checks whether the passed index points to a valid %document object by checking
 *  important properties. It returns FALSE if the index is not valid and then this index
 *  must not be used.
 **/
#define DOC_IDX_VALID(doc_idx) \
	((doc_idx) >= 0 && (guint)(doc_idx) < documents_array->len && documents[doc_idx]->is_valid)

/**
 *  DOC_FILENAME) returns the filename of the %document corresponding to the passed index or
 *  GEANY_STRING_UNTITLED (e.g. _("untitled")) if the %document's filename was not yet set.
 *  This macro never returns NULL.
 **/
#define DOC_FILENAME(doc_idx) \
	((documents[doc_idx]->file_name != NULL) ? (documents[doc_idx]->file_name) : \
		GEANY_STRING_UNTITLED)


gint document_find_by_filename(const gchar *filename, gboolean is_tm_filename);

gint document_find_by_sci(ScintillaObject *sci);

gint document_get_notebook_page(gint doc_idx);

gint document_get_n_idx(guint page_num);

gint document_get_cur_idx(void);

GeanyDocument *document_get_current(void);

void document_init_doclist(void);

void document_finalize(void);

void document_set_text_changed(gint idx);

void document_apply_update_prefs(gint idx);

gboolean document_remove(guint page_num);

gboolean document_account_for_unsaved(void);

gboolean document_close_all(void);

gint document_new_file_if_non_open();

gint document_new_file(const gchar *filename, GeanyFiletype *ft, const gchar *text);

gint document_clone(gint old_idx, const gchar *utf8_filename);

gint document_open_file(const gchar *locale_filename, gboolean readonly,
		GeanyFiletype *ft, const gchar *forced_enc);

gint document_open_file_full(gint idx, const gchar *filename, gint pos, gboolean readonly,
		GeanyFiletype *ft, const gchar *forced_enc);

void document_open_file_list(const gchar *data, gssize length);

void document_open_files(const GSList *filenames, gboolean readonly, GeanyFiletype *ft,
		const gchar *forced_enc);

gboolean document_reload_file(gint idx, const gchar *forced_enc);


gboolean document_save_file_as(gint idx);

gboolean document_save_file(gint idx, gboolean force);

gboolean document_search_bar_find(gint idx, const gchar *text, gint flags, gboolean inc);

gint document_find_text(gint idx, const gchar *text, gint flags, gboolean search_backwards,
		gboolean scroll, GtkWidget *parent);

gint document_replace_text(gint idx, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean search_backwards);

gboolean document_replace_all(gint idx, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean escaped_chars);

void document_replace_sel(gint idx, const gchar *find_text, const gchar *replace_text, gint flags,
						  gboolean escaped_chars);

void document_update_tag_list(gint idx, gboolean update);

void document_set_filetype(gint idx, GeanyFiletype *type);

void document_set_encoding(gint idx, const gchar *new_encoding);


/* own Undo / Redo implementation to be able to undo / redo changes
 * to the encoding or the Unicode BOM (which are Scintilla independent).
 * All Scintilla events are stored in the undo / redo buffer and are passed through. */

gboolean document_can_undo(gint idx);

gboolean document_can_redo(gint idx);

void document_undo(gint idx);

void document_redo(gint idx);

void document_undo_add(gint idx, guint type, gpointer data);


GdkColor *document_get_status_color(gint idx);

void document_delay_colourise(void);

void document_colourise_new(void);

#endif
