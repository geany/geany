/*
 *      document.c - this file is part of Geany, a fast and lightweight IDE
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
 * $Id$
 */

/*
 * Document related actions: new, save, open, etc.
 * Also Scintilla search actions.
 */

#include "geany.h"

#ifdef TIME_WITH_SYS_TIME
# include <time.h>
# include <sys/time.h>
#endif
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <ctype.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "document.h"
#include "prefs.h"
#include "filetypes.h"
#include "support.h"
#include "sciwrappers.h"
#include "editor.h"
#include "dialogs.h"
#include "msgwindow.h"
#include "templates.h"
#include "treeviews.h"
#include "ui_utils.h"
#include "utils.h"
#include "encodings.h"
#include "notebook.h"
#include "main.h"
#include "vte.h"
#include "build.h"
#include "symbols.h"
#include "callbacks.h"
#include "geanyobject.h"
#include "highlighting.h"


/* dynamic array of document elements to hold all information of the notebook tabs */
GArray *doc_array;

/* Whether to colourise the document straight after styling settings are changed.
 * (e.g. when filetype is set or typenames are updated) */
static gboolean delay_colourise = FALSE;


static void document_undo_clear(gint idx);
static void document_redo_add(gint idx, guint type, gpointer data);

static gboolean update_type_keywords(ScintillaObject *sci, gint lang);


// ignore the case of filenames and paths under WIN32, causes errors if not
#ifdef G_OS_WIN32
#define filenamecmp(a,b)	strcasecmp((a), (b))
#else
#define filenamecmp(a,b)	strcmp((a), (b))
#endif

static gint find_by_tm_filename(const gchar *filename)
{
	guint i;

	for (i = 0; i < doc_array->len; i++)
	{
		TMWorkObject *tm_file = doc_list[i].tm_file;

		if (tm_file == NULL || tm_file->file_name == NULL) continue;

		if (filenamecmp(filename, tm_file->file_name) == 0)
			return i;
	}
	return -1;
}


static gchar *get_real_path_from_utf8(const gchar *utf8_filename)
{
	gchar *locale_name = utils_get_locale_from_utf8(utf8_filename);
	gchar *realname = tm_get_real_path(locale_name);

	g_free(locale_name);
	return realname;
}


/* filename is in UTF-8 for non-TagManager filenames.
 * is_tm_filename should only be used when passing a TagManager filename,
 * which is therefore locale-encoded and already a realpath().
 * Returns: the document index which has the given filename. */
gint document_find_by_filename(const gchar *filename, gboolean is_tm_filename)
{
	guint i;
	gint ret = -1;
	gchar *realname;

	if (! filename) return -1;

	if (is_tm_filename)
		return find_by_tm_filename(filename);	// more efficient

	realname = get_real_path_from_utf8(filename);	// dereference symlinks, /../ junk in path
	if (! realname) return -1;

	for (i = 0; i < doc_array->len; i++)
	{
		document *doc = &doc_list[i];
		gchar *docname;

		if (doc->file_name == NULL) continue;

		docname = get_real_path_from_utf8(doc->file_name);
		if (! docname) continue;

		if (filenamecmp(realname, docname) == 0)
		{
			ret = i;
			g_free(docname);
			break;
		}
		g_free(docname);
	}
	g_free(realname);
	return ret;
}


/* returns the document index which has sci */
gint document_find_by_sci(ScintillaObject *sci)
{
	guint i;

	if (! sci) return -1;

	for(i = 0; i < doc_array->len; i++)
	{
		if (doc_list[i].is_valid && doc_list[i].sci == sci) return i;
	}
	return -1;
}


/* returns the index of the notebook page from the document index */
gint document_get_notebook_page(gint doc_idx)
{
	if (! DOC_IDX_VALID(doc_idx)) return -1;

	return gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook),
		GTK_WIDGET(doc_list[doc_idx].sci));
}


/* returns the index of the given notebook page in the document list */
gint document_get_n_idx(guint page_num)
{
	ScintillaObject *sci;
	if (page_num >= doc_array->len) return -1;

	sci = (ScintillaObject*)gtk_notebook_get_nth_page(
				GTK_NOTEBOOK(app->notebook), page_num);

	return document_find_by_sci(sci);
}


/* returns the index of the current notebook page in the document list */
gint document_get_cur_idx()
{
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));

	if (cur_page == -1)
		return -1;
	else
	{
		ScintillaObject *sci = (ScintillaObject*)
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), cur_page);

		return document_find_by_sci(sci);
	}
}


/* returns NULL if no documents are open */
document *document_get_current()
{
	gint idx = document_get_cur_idx();

	return DOC_IDX_VALID(idx) ? &doc_list[idx] : NULL;
}


void document_init_doclist()
{
	doc_array = g_array_new(FALSE, FALSE, sizeof(document));
}


void document_finalize()
{
	g_array_free(doc_array, TRUE);
}


void document_set_text_changed(gint idx)
{
	if (DOC_IDX_VALID(idx) && ! main_status.quitting)
	{
		ui_update_tab_status(idx);
		ui_save_buttons_toggle(doc_list[idx].changed);
		ui_set_window_title(idx);
		ui_update_statusbar(idx, -1);
	}
}


void document_set_use_tabs(gint idx, gboolean use_tabs)
{
	document *doc = &doc_list[idx];

	g_return_if_fail(DOC_IDX_VALID(idx));

	doc->use_tabs = use_tabs;
	sci_set_use_tabs(doc->sci, use_tabs);
	// remove indent spaces on backspace, if using spaces to indent
	SSM(doc->sci, SCI_SETBACKSPACEUNINDENTS, ! use_tabs, 0);
}


void document_set_line_wrapping(gint idx, gboolean wrap)
{
	document *doc = &doc_list[idx];

	g_return_if_fail(DOC_IDX_VALID(idx));

	doc->line_wrapping = wrap;
	sci_set_lines_wrapped(doc->sci, wrap);
}


// Apply just the prefs that can change in the Preferences dialog
void document_apply_update_prefs(gint idx)
{
	ScintillaObject *sci = doc_list[idx].sci;

	sci_set_mark_long_lines(sci, editor_prefs.long_line_type, editor_prefs.long_line_column, editor_prefs.long_line_color);

	sci_set_tab_width(sci, editor_prefs.tab_width);

	sci_set_autoc_max_height(sci, editor_prefs.symbolcompletion_max_height);

	sci_set_indentation_guides(sci, editor_prefs.show_indent_guide);
	sci_set_visible_white_spaces(sci, editor_prefs.show_white_space);
	sci_set_visible_eols(sci, editor_prefs.show_line_endings);

	sci_set_folding_margin_visible(sci, editor_prefs.folding);

	doc_list[idx].auto_indent = (editor_prefs.indent_mode != INDENT_NONE);

	sci_assign_cmdkey(sci, SCK_HOME,
		editor_prefs.smart_home_key ? SCI_VCHOMEWRAP : SCI_HOMEWRAP);
	sci_assign_cmdkey(sci, SCK_END,  SCI_LINEENDWRAP);
}


/* Sets is_valid to FALSE and initializes some members to NULL, to mark it uninitialized.
 * The flag is_valid is set to TRUE in document_create_new_sci(). */
static void init_doc_struct(document *new_doc)
{
	new_doc->is_valid = FALSE;
	new_doc->has_tags = FALSE;
	new_doc->auto_indent = (editor_prefs.indent_mode != INDENT_NONE);
	new_doc->line_wrapping = editor_prefs.line_wrapping;
	new_doc->readonly = FALSE;
	new_doc->tag_store = NULL;
	new_doc->tag_tree = NULL;
	new_doc->file_name = NULL;
	new_doc->file_type = NULL;
	new_doc->tm_file = NULL;
	new_doc->encoding = NULL;
	new_doc->has_bom = FALSE;
	new_doc->sci = NULL;
	new_doc->undo_actions = NULL;
	new_doc->redo_actions = NULL;
	new_doc->scroll_percent = -1.0F;
}


/* returns the next free place(i.e. index) in the document list,
 * or -1 if the current doc_array is full */
static gint document_get_new_idx()
{
	guint i;

	for(i = 0; i < doc_array->len; i++)
	{
		if (doc_list[i].sci == NULL)
		{
			return (gint) i;
		}
	}
	return -1;
}


static void setup_sci_keys(ScintillaObject *sci)
{
	// disable some Scintilla keybindings to be able to redefine them cleanly
	sci_clear_cmdkey(sci, 'A' | (SCMOD_CTRL << 16)); // select all
	sci_clear_cmdkey(sci, 'D' | (SCMOD_CTRL << 16)); // duplicate
	sci_clear_cmdkey(sci, 'T' | (SCMOD_CTRL << 16)); // line transpose
	sci_clear_cmdkey(sci, 'T' | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16)); // line copy
	sci_clear_cmdkey(sci, 'L' | (SCMOD_CTRL << 16)); // line cut
	sci_clear_cmdkey(sci, 'L' | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16)); // line delete
	sci_clear_cmdkey(sci, SCK_UP | (SCMOD_CTRL << 16)); // scroll line up
	sci_clear_cmdkey(sci, SCK_DOWN | (SCMOD_CTRL << 16)); // scroll line down

	if (editor_prefs.use_gtk_word_boundaries)
	{
		// use GtkEntry-like word boundaries
		sci_assign_cmdkey(sci, SCK_RIGHT | (SCMOD_CTRL << 16), SCI_WORDRIGHTEND);
		sci_assign_cmdkey(sci, SCK_RIGHT | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16), SCI_WORDRIGHTENDEXTEND);
		sci_assign_cmdkey(sci, SCK_DELETE | (SCMOD_CTRL << 16), SCI_DELWORDRIGHTEND);
	}
	sci_assign_cmdkey(sci, SCK_UP | (SCMOD_ALT << 16), SCI_LINESCROLLUP);
	sci_assign_cmdkey(sci, SCK_DOWN | (SCMOD_ALT << 16), SCI_LINESCROLLDOWN);
	sci_assign_cmdkey(sci, SCK_UP | (SCMOD_CTRL << 16), SCI_PARAUP);
	sci_assign_cmdkey(sci, SCK_UP | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16), SCI_PARAUPEXTEND);
	sci_assign_cmdkey(sci, SCK_DOWN | (SCMOD_CTRL << 16), SCI_PARADOWN);
	sci_assign_cmdkey(sci, SCK_DOWN | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16), SCI_PARADOWNEXTEND);

	sci_clear_cmdkey(sci, SCK_BACK | (SCMOD_ALT << 16)); // clear Alt-Backspace (Undo)
}


/* creates a new tab in the notebook and does all related stuff
 * finally it returns the index of the created document */
static gint document_create_new_sci(const gchar *filename)
{
	ScintillaObject	*sci;
	PangoFontDescription *pfd;
	gchar *fname;
	gint new_idx;
	document *this;
	gint tabnum;
	gint cur_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	if (cur_pages == 1)
	{
		gint idx = document_get_cur_idx();
		// remove the empty document and open a new one
		if (doc_list[idx].file_name == NULL && ! doc_list[idx].changed) document_remove(0);
	}

	new_idx = document_get_new_idx();
	if (new_idx == -1)	// expand the array, no free places
	{
		document new_doc;
		init_doc_struct(&new_doc);
		new_idx = doc_array->len;
		g_array_append_val(doc_array, new_doc);
	}
	this = &doc_list[new_idx];

	/* SCI - Code */
	sci = SCINTILLA(scintilla_new());
	scintilla_set_id(sci, new_idx);
	this->sci = sci;

	gtk_widget_show(GTK_WIDGET(sci));

	sci_set_codepage(sci, SC_CP_UTF8);
	//SSM(sci, SCI_SETWRAPSTARTINDENT, 4, 0);
	// disable scintilla provided popup menu
	sci_use_popup(sci, FALSE);

	setup_sci_keys(sci);

	document_apply_update_prefs(new_idx);

	sci_set_tab_indents(sci, editor_prefs.use_tab_to_indent);
	sci_set_symbol_margin(sci, editor_prefs.show_markers_margin);
	sci_set_line_numbers(sci, editor_prefs.show_linenumber_margin, 0);
	sci_set_lines_wrapped(sci, editor_prefs.line_wrapping);
	sci_set_scrollbar_mode(sci, editor_prefs.show_scrollbars);
	sci_set_caret_policy_x(sci, CARET_JUMPS | CARET_EVEN, 0);
	//sci_set_caret_policy_y(sci, CARET_JUMPS | CARET_EVEN, 0);
	SSM(sci, SCI_AUTOCSETSEPARATOR, '\n', 0);
	// (dis)allow scrolling past end of document
	SSM(sci, SCI_SETENDATLASTLINE, editor_prefs.scroll_stop_at_last_line, 0);

	// signal for insert-key(works without too, but to update the right status bar)
	//g_signal_connect((GtkWidget*) sci, "key-press-event",
					//G_CALLBACK(keybindings_got_event), GINT_TO_POINTER(new_idx));
	// signal for the popup menu
	g_signal_connect(G_OBJECT(sci), "button-press-event",
					G_CALLBACK(on_editor_button_press_event), GINT_TO_POINTER(new_idx));
	g_signal_connect(G_OBJECT(sci), "motion-notify-event", G_CALLBACK(on_motion_event), NULL);

	pfd = pango_font_description_from_string(prefs.editor_font);
	fname = g_strdup_printf("!%s", pango_font_description_get_family(pfd));
	document_set_font(new_idx, fname, pango_font_description_get_size(pfd) / PANGO_SCALE);
	pango_font_description_free(pfd);
	g_free(fname);

	this->tag_store = NULL;
	this->tag_tree = NULL;

	// store important pointers in the tab list
	this->file_name = (filename) ? g_strdup(filename) : NULL;
	this->encoding = NULL;
	this->saved_encoding.encoding = NULL;
	this->saved_encoding.has_bom = FALSE;
	this->tm_file = NULL;
	this->file_type = NULL;
	this->mtime = 0;
	this->changed = FALSE;
	this->last_check = time(NULL);
	this->readonly = FALSE;
	this->line_wrapping = editor_prefs.line_wrapping;
	this->auto_indent = (editor_prefs.indent_mode != INDENT_NONE);
	this->has_tags = FALSE;

	treeviews_openfiles_add(new_idx);	// sets this->iter

	tabnum = notebook_new_tab(new_idx);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook), tabnum);

	// select document in sidebar
	{
		GtkTreeSelection *sel;

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
		gtk_tree_selection_select_iter(sel, &this->iter);
	}

	ui_document_buttons_update();

	this->is_valid = TRUE;	// do this last to prevent UI updating with NULL items.
	g_assert(doc_list[new_idx].sci == sci);
	return new_idx;
}


/* removes the given notebook tab and clears the related entry in the document list */
gboolean document_remove(guint page_num)
{
	gint idx = document_get_n_idx(page_num);

	if (DOC_IDX_VALID(idx))
	{
		if (doc_list[idx].changed && ! dialogs_show_unsaved_file(idx))
		{
			return FALSE;
		}
		notebook_remove_page(page_num);
		treeviews_remove_document(idx);
		msgwin_status_add(_("File %s closed."), DOC_FILENAME(idx));
		g_free(doc_list[idx].encoding);
		g_free(doc_list[idx].saved_encoding.encoding);
		g_free(doc_list[idx].file_name);
		tm_workspace_remove_object(doc_list[idx].tm_file, TRUE, TRUE);

		doc_list[idx].is_valid = FALSE;
		doc_list[idx].sci = NULL;
		doc_list[idx].file_name = NULL;
		doc_list[idx].file_type = NULL;
		doc_list[idx].encoding = NULL;
		doc_list[idx].has_bom = FALSE;
		doc_list[idx].tm_file = NULL;
		doc_list[idx].scroll_percent = -1.0F;
		document_undo_clear(idx);
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) == 0)
		{
			treeviews_update_tag_list(-1, FALSE);
			//on_notebook1_switch_page(GTK_NOTEBOOK(app->notebook), NULL, 0, NULL);
			ui_set_window_title(-1);
			ui_save_buttons_toggle(FALSE);
			ui_document_buttons_update();
			build_menu_update(-1);
		}
	}
	else geany_debug("Error: idx: %d page_num: %d", idx, page_num);

	return TRUE;
}


// used to keep a record of the unchanged document state encoding
static void store_saved_encoding(gint idx)
{
	g_free(doc_list[idx].saved_encoding.encoding);
	doc_list[idx].saved_encoding.encoding = g_strdup(doc_list[idx].encoding);
	doc_list[idx].saved_encoding.has_bom = doc_list[idx].has_bom;
}


/* Create a new document.
 * filename is either the UTF-8 file name, or NULL.
 * If ft is NULL and filename is not NULL, then the filetype will be guessed
 * from the given filename.
 * text is the contents of the new file in valid UTF-8 encoding, or NULL.
 * Returns: idx of new file in doc_list. */
gint document_new_file(const gchar *filename, filetype *ft, const gchar *text)
{
	gint idx = document_create_new_sci(filename);

	g_assert(idx != -1);

	sci_set_undo_collection(doc_list[idx].sci, FALSE); // avoid creation of an undo action
	if (text)
		sci_set_text(doc_list[idx].sci, text);
	else
		sci_clear_all(doc_list[idx].sci);

#ifdef G_OS_WIN32
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_CRLF);
#else
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_LF);
#endif
	document_set_use_tabs(idx, editor_prefs.use_tabs);
	sci_set_undo_collection(doc_list[idx].sci, TRUE);
	sci_empty_undo_buffer(doc_list[idx].sci);

	doc_list[idx].mtime = time(NULL);
	doc_list[idx].changed = FALSE;

	doc_list[idx].encoding = g_strdup(encodings[prefs.default_new_encoding].charset);
	// store the opened encoding for undo/redo
	store_saved_encoding(idx);

	//document_set_filetype(idx, (ft == NULL) ? filetypes[GEANY_FILETYPES_ALL] : ft);
	if (ft == NULL && filename != NULL) // guess the filetype from the filename if one is given
		ft = filetypes_detect_from_file(idx);

	document_set_filetype(idx, ft);	// also clears taglist
	if (ft == NULL)
		highlighting_set_styles(doc_list[idx].sci, GEANY_FILETYPES_ALL);
	ui_set_window_title(idx);
	build_menu_update(idx);
	document_update_tag_list(idx, FALSE);
	document_set_text_changed(idx);
	ui_document_show_hide(idx); // update the document menu

	sci_set_line_numbers(doc_list[idx].sci, editor_prefs.show_linenumber_margin, 0);
	sci_goto_pos(doc_list[idx].sci, 0, TRUE);

	// "the" SCI signal (connect after initial setup(i.e. adding text))
	g_signal_connect((GtkWidget*) doc_list[idx].sci, "sci-notify",
				G_CALLBACK(on_editor_notification), GINT_TO_POINTER(idx));

	if (geany_object)
	{
		g_signal_emit_by_name(geany_object, "document-new", idx);
	}

	msgwin_status_add(_("New file \"%s\" opened."),
		(doc_list[idx].file_name != NULL) ? doc_list[idx].file_name : GEANY_STRING_UNTITLED);

	return idx;
}


/* This is a wrapper for document_open_file_full(), see that function for details.
 * Do not use this when opening multiple files (unless using document_delay_colourise()). */
gint document_open_file(const gchar *locale_filename, gboolean readonly,
		filetype *ft, const gchar *forced_enc)
{
	return document_open_file_full(-1, locale_filename, 0, readonly, ft, forced_enc);
}


typedef struct
{
	gchar		*data;	// null-terminated file data
	gsize		 size;	// actual file size on disk
	gsize		 len;	// string length of data
	gchar		*enc;
	gboolean	 bom;
	time_t		 mtime;	// modification time, read by stat::st_mtime
	gboolean	 readonly;
} FileData;


// reload file with specified encoding
static gboolean
handle_forced_encoding(FileData *filedata, const gchar *forced_enc)
{
	GeanyEncodingIndex enc_idx;

	if (utils_str_equal(forced_enc, "UTF-8"))
	{
		if (! g_utf8_validate(filedata->data, filedata->len, NULL))
		{
			return FALSE;
		}
	}
	else
	{
		gchar *converted_text = encodings_convert_to_utf8_from_charset(
										filedata->data, filedata->len, forced_enc, FALSE);
		if (converted_text == NULL)
		{
			return FALSE;
		}
		else
		{
			g_free(filedata->data);
			filedata->data = converted_text;
			filedata->len = strlen(converted_text);
		}
	}
	enc_idx = encodings_scan_unicode_bom(filedata->data, filedata->size, NULL);
	filedata->bom = (enc_idx == GEANY_ENCODING_UTF_8);
	filedata->enc = g_strdup(forced_enc);
	return TRUE;
}


// detect encoding and convert to UTF-8 if necessary
static gboolean
handle_encoding(FileData *filedata)
{
	g_return_val_if_fail(filedata->enc == NULL, FALSE);
	g_return_val_if_fail(filedata->bom == FALSE, FALSE);

	if (filedata->size == 0)
	{
		// we have no data so assume UTF-8, filedata->len can be 0 even we have an empty
		// e.g. UTF32 file with a BOM(so size is 4, len is 0)
		filedata->enc = g_strdup("UTF-8");
	}
	else
	{
		// first check for a BOM
		GeanyEncodingIndex enc_idx =
			encodings_scan_unicode_bom(filedata->data, filedata->size, NULL);

		if (enc_idx != GEANY_ENCODING_NONE)
		{
			filedata->enc = g_strdup(encodings[enc_idx].charset);
			filedata->bom = TRUE;

			if (enc_idx != GEANY_ENCODING_UTF_8) // the BOM indicated something else than UTF-8
			{
				gchar *converted_text = encodings_convert_to_utf8_from_charset(
										filedata->data, filedata->size, filedata->enc, FALSE);
				if (converted_text != NULL)
				{
					g_free(filedata->data);
					filedata->data = converted_text;
					filedata->len = strlen(converted_text);
				}
				else
				{
					// there was a problem converting data from BOM encoding type
					g_free(filedata->enc);
					filedata->enc = NULL;
					filedata->bom = FALSE;
				}
			}
		}

		if (filedata->enc == NULL)	// either there was no BOM or the BOM encoding failed
		{
			// try UTF-8 first
			if (g_utf8_validate(filedata->data, filedata->len, NULL))
			{
				filedata->enc = g_strdup("UTF-8");
			}
			else
			{
				// detect the encoding
				gchar *converted_text = encodings_convert_to_utf8(filedata->data,
					filedata->size, &filedata->enc);

				if (converted_text == NULL)
				{
					return FALSE;
				}
				g_free(filedata->data);
				filedata->data = converted_text;
				filedata->len = strlen(converted_text);
			}
		}
	}
	return TRUE;
}


static void
handle_bom(FileData *filedata)
{
	guint bom_len;

	encodings_scan_unicode_bom(filedata->data, filedata->size, &bom_len);
	g_return_if_fail(bom_len != 0);

	// use filedata->len here because the contents are already converted into UTF-8
	filedata->len -= bom_len;
	// overwrite the BOM with the remainder of the file contents, plus the NULL terminator.
	g_memmove(filedata->data, filedata->data + bom_len, filedata->len + 1);
	filedata->data = g_realloc(filedata->data, filedata->len + 1);
}


/* loads textfile data, verifies and converts to forced_enc or UTF-8. Also handles BOM. */
static gboolean load_text_file(const gchar *locale_filename, const gchar *utf8_filename,
		FileData *filedata, const gchar *forced_enc)
{
	GError *err = NULL;
	struct stat st;
	GeanyEncodingIndex tmp_enc_idx;

	filedata->data = NULL;
	filedata->len = 0;
	filedata->enc = NULL;
	filedata->bom = FALSE;
	filedata->readonly = FALSE;

	if (g_stat(locale_filename, &st) != 0)
	{
		ui_set_statusbar(TRUE, _("Could not open file %s (%s)"), utf8_filename, g_strerror(errno));
		return FALSE;
	}

	filedata->mtime = st.st_mtime;

	if (! g_file_get_contents(locale_filename, &filedata->data, NULL, &err))
	{
		ui_set_statusbar(TRUE, "%s", err->message);
		g_error_free(err);
		return FALSE;
	}

	// use strlen to check for null chars
	filedata->size = (gsize) st.st_size;
	filedata->len = strlen(filedata->data);

	// temporarily retrieve the encoding idx based on the BOM to suppress the following warning
	// if we have a BOM
	tmp_enc_idx = encodings_scan_unicode_bom(filedata->data, filedata->size, NULL);

	/* check whether the size of the loaded data is equal to the size of the file in the filesystem */
	if (filedata->len != filedata->size && (
		tmp_enc_idx == GEANY_ENCODING_UTF_8 || // tmp_enc_idx can be UTF-7/8/16/32, UCS and None
		tmp_enc_idx == GEANY_ENCODING_UTF_7 || // filter out UTF-7/8 and None where no NULL bytes
		tmp_enc_idx == GEANY_ENCODING_NONE))   // are allowed
	{
		gchar *warn_msg = _("The file \"%s\" could not be opened properly and has been truncated. "
				"This can occur if the file contains a NULL byte. "
				"Be aware that saving it can cause data loss.\nThe file was set to read-only.");

		if (main_status.main_window_realized)
			dialogs_show_msgbox(GTK_MESSAGE_WARNING, warn_msg, utf8_filename);

		ui_set_statusbar(TRUE, warn_msg, utf8_filename);

		// set the file to read-only mode because saving it is probably dangerous
		filedata->readonly = TRUE;
	}

	/* Determine character encoding and convert to UTF-8 */
	if (forced_enc != NULL)
	{
		// the encoding should be ignored(requested by user), so open the file "as it is"
		if (utils_str_equal(forced_enc, encodings[GEANY_ENCODING_NONE].charset))
		{
			filedata->bom = FALSE;
			filedata->enc = g_strdup(encodings[GEANY_ENCODING_NONE].charset);
		}
		else if (! handle_forced_encoding(filedata, forced_enc))
		{
			ui_set_statusbar(TRUE, _("The file \"%s\" is not valid %s."), utf8_filename, forced_enc);
			utils_beep();
			g_free(filedata->data);
			return FALSE;
		}
	}
	else if (! handle_encoding(filedata))
	{
		ui_set_statusbar(TRUE,
			_("The file \"%s\" does not look like a text file or the file encoding is not supported."),
			utf8_filename);
		utils_beep();
		g_free(filedata->data);
		return FALSE;
	}

	if (filedata->bom)
		handle_bom(filedata);
	return TRUE;
}


/* Sets the cursor position on opening a file. First it sets the line when cl_options.goto_line
 * is set, otherwise it sets the line when pos is greater than zero and finally it sets the column
 * if cl_options.goto_column is set. */
static void set_cursor_position(gint idx, gint pos)
{
	if (cl_options.goto_line >= 0)
	{	// goto line which was specified on command line and then undefine the line
		sci_goto_line(doc_list[idx].sci, cl_options.goto_line - 1, TRUE);
		doc_list[idx].scroll_percent = 0.5F;
		cl_options.goto_line = -1;
	}
	else if (pos > 0)
	{
		sci_set_current_position(doc_list[idx].sci, pos, FALSE);
		doc_list[idx].scroll_percent = 0.5F;
	}

	if (cl_options.goto_column >= 0)
	{	// goto column which was specified on command line and then undefine the column
		gint cur_pos = sci_get_current_position(doc_list[idx].sci);
		sci_set_current_position(doc_list[idx].sci, cur_pos + cl_options.goto_column, FALSE);
		doc_list[idx].scroll_percent = 0.5F;
		cl_options.goto_column = -1;
	}
}


static gboolean detect_use_tabs(ScintillaObject *sci)
{
	gint line;
	gsize tabs = 0, spaces = 0;

	for (line = 0; line < sci_get_line_count(sci); line++)
	{
		gint pos = sci_get_position_from_line(sci, line);
		gchar c;

		c = sci_get_char_at(sci, pos);
		if (c == '\t')
			tabs++;
		else
		if (c == ' ')
		{
			// check at least 2 spaces
			if (sci_get_char_at(sci, pos + 1) == ' ')
				spaces++;
		}
	}
	if (spaces == 0 && tabs == 0)
		return editor_prefs.use_tabs;

	// Skew comparison by a factor of 2 in favour of default editor pref
	if (editor_prefs.use_tabs)
		return ! (spaces > tabs * 2);
	else
		return (tabs > spaces * 2);
}


/* To open a new file, set idx to -1; filename should be locale encoded.
 * To reload a file, set the idx for the document to be reloaded; filename should be NULL.
 * pos is the cursor position, which can be overridden by --line and --column.
 * forced_enc can be NULL to detect the file encoding.
 * Returns: idx of the opened file or -1 if an error occurred.
 *
 * When opening more than one file, either:
 * 1. Use document_open_files().
 * 2. Call document_delay_colourise() before document_open_file() and
 *    document_colourise_new() after opening all files.
 *
 * This avoids unnecessary recolourising, saving significant processing when a lot of files
 * are open of a filetype that supports user typenames, e.g. C. */
gint document_open_file_full(gint idx, const gchar *filename, gint pos, gboolean readonly,
		filetype *ft, const gchar *forced_enc)
{
	gint editor_mode;
	gboolean reload = (idx == -1) ? FALSE : TRUE;
	gchar *utf8_filename = NULL;
	gchar *locale_filename = NULL;
	filetype *use_ft;
	FileData filedata;

	//struct timeval tv, tv1;
	//struct timezone tz;
	//gettimeofday(&tv, &tz);

	if (reload)
	{
		utf8_filename = g_strdup(doc_list[idx].file_name);
		locale_filename = utils_get_locale_from_utf8(utf8_filename);
	}
	else
	{
		// filename must not be NULL when opening a file
		if (filename == NULL)
		{
			ui_set_statusbar(FALSE, _("Invalid filename"));
			return -1;
		}

		locale_filename = g_strdup(filename);
		// try to get the UTF-8 equivalent for the filename, fallback to filename if error
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		// if file is already open, switch to it and go
		idx = document_find_by_filename(utf8_filename, FALSE);
		if (idx >= 0)
		{
			ui_add_recent_file(utf8_filename);	// either add or reorder recent item
			gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook),
					gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook),
					(GtkWidget*) doc_list[idx].sci));
			g_free(utf8_filename);
			g_free(locale_filename);
			utils_check_disk_status(idx, TRUE);	// force a file changed check
			set_cursor_position(idx, pos);
			return idx;
		}
	}

	// if default encoding for opening files is set, use it if no forced encoding is set
	if (prefs.default_open_encoding >= 0 && forced_enc == NULL)
		forced_enc = encodings[prefs.default_open_encoding].charset;

	if (! load_text_file(locale_filename, utf8_filename, &filedata, forced_enc))
	{
		g_free(utf8_filename);
		g_free(locale_filename);
		return -1;
	}

	if (! reload) idx = document_create_new_sci(utf8_filename);
	g_return_val_if_fail(idx != -1, -1);	// really should not happen

	sci_set_undo_collection(doc_list[idx].sci, FALSE); // avoid creation of an undo action
	sci_empty_undo_buffer(doc_list[idx].sci);

	// add the text to the ScintillaObject
	sci_set_readonly(doc_list[idx].sci, FALSE);	// to allow replacing text
	sci_set_text(doc_list[idx].sci, filedata.data);	// NULL terminated data

	// detect & set line endings
	editor_mode = utils_get_line_endings(filedata.data, filedata.len);
	sci_set_eol_mode(doc_list[idx].sci, editor_mode);
	g_free(filedata.data);

	if (reload)
		document_set_use_tabs(idx, doc_list[idx].use_tabs); // resetup sci
	else
	if (! editor_prefs.detect_tab_mode)
		document_set_use_tabs(idx, editor_prefs.use_tabs);
	else
	{	// detect & set tabs/spaces
		gboolean use_tabs = detect_use_tabs(doc_list[idx].sci);

		if (use_tabs != editor_prefs.use_tabs)
			ui_set_statusbar(TRUE, _("Setting %s indentation mode."),
				(use_tabs) ? _("Tabs") : _("Spaces"));
		document_set_use_tabs(idx, use_tabs);
	}

	sci_set_undo_collection(doc_list[idx].sci, TRUE);

	doc_list[idx].mtime = filedata.mtime; // get the modification time from file and keep it
	doc_list[idx].changed = FALSE;
	g_free(doc_list[idx].encoding);	// if reloading, free old encoding
	doc_list[idx].encoding = filedata.enc;
	doc_list[idx].has_bom = filedata.bom;
	store_saved_encoding(idx);	// store the opened encoding for undo/redo

	doc_list[idx].readonly = readonly || filedata.readonly;
	sci_set_readonly(doc_list[idx].sci, doc_list[idx].readonly);

	// update line number margin width
	sci_set_line_numbers(doc_list[idx].sci, editor_prefs.show_linenumber_margin, 0);

	// set the cursor position according to pos, cl_options.goto_line and cl_options.goto_column
	set_cursor_position(idx, pos);

	if (! reload)
	{
		// "the" SCI signal (connect after initial setup(i.e. adding text))
		g_signal_connect((GtkWidget*) doc_list[idx].sci, "sci-notify",
					G_CALLBACK(on_editor_notification), GINT_TO_POINTER(idx));

		use_ft = (ft != NULL) ? ft : filetypes_detect_from_file(idx);
	}
	else
	{	// reloading
		document_undo_clear(idx);
		use_ft = ft;
	}
	// update taglist, typedef keywords and build menu if necessary
	document_set_filetype(idx, use_ft);

	document_set_text_changed(idx);	// also updates tab state
	ui_document_show_hide(idx);	// update the document menu

	// finally add current file to recent files menu, but not the files from the last session
	if (! main_status.opening_session_files)
		ui_add_recent_file(utf8_filename);

	if (! reload && geany_object)
		g_signal_emit_by_name(geany_object, "document-open", idx);

	if (reload)
		ui_set_statusbar(TRUE, _("File %s reloaded."), utf8_filename);
	else
		msgwin_status_add(_("File %s opened(%d%s)."),
				utf8_filename, gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)),
				(readonly) ? _(", read-only") : "");

	g_free(utf8_filename);
	g_free(locale_filename);
	//gettimeofday(&tv1, &tz);
	//geany_debug("%s: %d", filename, (gint)(tv1.tv_usec - tv.tv_usec));

	return idx;
}


/* Takes a new line separated list of filename URIs and opens each file.
 * length is the length of the string or -1 if it should be detected */
void document_open_file_list(const gchar *data, gssize length)
{
	gint i;
	gchar *filename;
	gchar **list;

	if (data == NULL) return;

	if (length < 0)
		length = strlen(data);

	switch (utils_get_line_endings((gchar*) data, length))
	{
		case SC_EOL_CR: list = g_strsplit(data, "\r", 0); break;
		case SC_EOL_CRLF: list = g_strsplit(data, "\r\n", 0); break;
		case SC_EOL_LF: list = g_strsplit(data, "\n", 0); break;
		default: list = g_strsplit(data, "\n", 0);
	}

	document_delay_colourise();

	for (i = 0; ; i++)
	{
		if (list[i] == NULL) break;
		filename = g_filename_from_uri(list[i], NULL, NULL);
		if (filename == NULL) continue;
		document_open_file(filename, FALSE, NULL, NULL);
		g_free(filename);
	}
	document_colourise_new();

	g_strfreev(list);
}


/* Takes a linked list of filename URIs and opens each file, ensuring the newly opened
 * documents and existing documents (if necessary) are only colourised once. */
void document_open_files(const GSList *filenames, gboolean readonly, filetype *ft,
		const gchar *forced_enc)
{
	const GSList *item;

	document_delay_colourise();

	for (item = filenames; item != NULL; item = g_slist_next(item))
	{
		document_open_file(item->data, readonly, ft, forced_enc);
	}
	document_colourise_new();
}


/* Reload document with index idx.
 * forced_enc can be NULL to detect the file encoding.
 * Returns: TRUE if successful. */
gboolean document_reload_file(gint idx, const gchar *forced_enc)
{
	gint pos = 0;

	if (! DOC_IDX_VALID(idx))
		return FALSE;

	// try to set the cursor to the position before reloading
	pos = sci_get_current_position(doc_list[idx].sci);
	idx = document_open_file_full(idx, NULL, pos, doc_list[idx].readonly,
					doc_list[idx].file_type, forced_enc);
	return (idx != -1);
}


static gboolean document_update_timestamp(gint idx)
{
	struct stat st;
	gchar *locale_filename;

	g_return_val_if_fail(DOC_IDX_VALID(idx), FALSE);

	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);
	if (g_stat(locale_filename, &st) != 0)
	{
		ui_set_statusbar(TRUE, _("Could not open file %s (%s)"), doc_list[idx].file_name,
			g_strerror(errno));
		g_free(locale_filename);
		return FALSE;
	}

	doc_list[idx].mtime = st.st_mtime; // get the modification time from file and keep it
	g_free(locale_filename);
	return TRUE;
}


/* Sets line and column to the given position byte_pos in the document.
 * byte_pos is the position counted in bytes, not characters */
static void get_line_column_from_pos(gint idx, guint byte_pos, gint *line, gint *column)
{
	gint i;
	gint line_start;

	// for some reason we can use byte count instead of character count here
	*line = sci_get_line_from_position(doc_list[idx].sci, byte_pos);
	line_start = sci_get_position_from_line(doc_list[idx].sci, *line);
	// get the column in the line
	*column = byte_pos - line_start;

	// any non-ASCII characters are encoded with two bytes(UTF-8, always in Scintilla), so
	// skip one byte(i++) and decrease the column number which is based on byte count
	for (i = line_start; i < (line_start + *column); i++)
	{
		if (sci_get_char_at(doc_list[idx].sci, i) < 0)
		{
			(*column)--;
			i++;
		}
	}
}


/* This saves the file.
 * When force is set then it is always saved, even if it is unchanged(useful when using Save As)
 * It returns whether the file could be saved or not. */
gboolean document_save_file(gint idx, gboolean force)
{
	gchar *data;
	FILE *fp;
	gint bytes_written, len;
	gchar *locale_filename = NULL;

	if (! DOC_IDX_VALID(idx)) return FALSE;
	// the "changed" flag should exclude the "readonly" flag, but check it anyway for safety
	if (! force && (! doc_list[idx].changed || doc_list[idx].readonly)) return FALSE;

	if (doc_list[idx].file_name == NULL)
	{
		ui_set_statusbar(TRUE, _("Error saving file."));
		utils_beep();
		return FALSE;
	}

	// replaces tabs by spaces
	if (prefs.replace_tabs) document_replace_tabs(idx);
	// strip trailing spaces
	if (prefs.strip_trailing_spaces) document_strip_trailing_spaces(idx);
	// ensure the file has a newline at the end
	if (prefs.final_new_line) document_ensure_final_newline(idx);

	len = sci_get_length(doc_list[idx].sci) + 1;
	if (doc_list[idx].has_bom && encodings_is_unicode_charset(doc_list[idx].encoding))
	{	// always write a UTF-8 BOM because in this moment the text itself is still in UTF-8
		// encoding, it will be converted to doc_list[idx].encoding below and this conversion
		// also changes the BOM
		data = (gchar*) g_malloc(len + 3);	// 3 chars for BOM
		data[0] = 0xef;
		data[1] = 0xbb;
		data[2] = 0xbf;
		sci_get_text(doc_list[idx].sci, len, data + 3);
		len += 3;
	}
	else
	{
		data = (gchar*) g_malloc(len);
		sci_get_text(doc_list[idx].sci, len, data);
	}

	// save in original encoding, skip when it is already UTF-8 or has the encoding "None"
	if (doc_list[idx].encoding != NULL && ! utils_str_equal(doc_list[idx].encoding, "UTF-8") &&
		! utils_str_equal(doc_list[idx].encoding, encodings[GEANY_ENCODING_NONE].charset))
	{
		GError *conv_error = NULL;
		gchar* conv_file_contents = NULL;
		gsize bytes_read;
		gsize conv_len;

		// try to convert it from UTF-8 to original encoding
		conv_file_contents = g_convert(data, len-1, doc_list[idx].encoding, "UTF-8",
													&bytes_read, &conv_len, &conv_error);

		if (conv_error != NULL)
		{
			gchar *text = g_strdup_printf(
				_("An error occurred while converting the file from UTF-8 in \"%s\". The file remains unsaved."),
				doc_list[idx].encoding);
			gchar *error_text;

			if (conv_error->code == G_CONVERT_ERROR_ILLEGAL_SEQUENCE)
			{
				gchar *context = NULL;
				gint line, column;
				gint context_len;
				gunichar unic;
				gint max_len = MIN((gint)bytes_read + 6, len - 1); // don't read over the doc length
				context = g_malloc(7); // read 6 bytes from Sci + '\0'
				sci_get_text_range(doc_list[idx].sci, bytes_read, max_len, context);

				// take only one valid Unicode character from the context and discard the leftover
				unic = g_utf8_get_char_validated(context, -1);
				context_len = g_unichar_to_utf8(unic, context);
				context[context_len] = '\0';
				get_line_column_from_pos(idx, bytes_read, &line, &column);

				error_text = g_strdup_printf(
					_("Error message: %s\nThe error occurred at \"%s\" (line: %d, column: %d)."),
					conv_error->message, context, line + 1, column);
				g_free(context);
			}
			else
				error_text = g_strdup_printf(_("Error message: %s."), conv_error->message);

			geany_debug("encoding error: %s", conv_error->message);
			dialogs_show_msgbox_with_secondary(GTK_MESSAGE_ERROR, text, error_text);
			g_error_free(conv_error);
			g_free(data);
			g_free(text);
			g_free(error_text);
			return FALSE;
		}
		else
		{
			g_free(data);
			data = conv_file_contents;
			len = conv_len;
		}
	}
	else
	{
		len = strlen(data);
	}

	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);
	fp = g_fopen(locale_filename, "wb");
	g_free(locale_filename);

	if (fp == NULL)
	{
		ui_set_statusbar(TRUE, _("Error saving file (%s)."), g_strerror(errno));
		utils_beep();
		g_free(data);
		return FALSE;
	}
	bytes_written = fwrite(data, sizeof (gchar), len, fp);
	fclose (fp);

	g_free(data);

	if (len != bytes_written)
	{
		ui_set_statusbar(TRUE, _("Error saving file."));
		utils_beep();
		return FALSE;
	}

	// store the opened encoding for undo/redo
	store_saved_encoding(idx);

	// ignore the following things if we are quitting
	if (! main_status.quitting)
	{
		gchar *base_name = g_path_get_basename(doc_list[idx].file_name);

		// set line numbers again, to reset the margin width, if
		// there are more lines than before
		sci_set_line_numbers(doc_list[idx].sci, editor_prefs.show_linenumber_margin, 0);
		sci_set_savepoint(doc_list[idx].sci);

		/* stat the file to get the timestamp, otherwise on Windows the actual
		 * timestamp can be ahead of time(NULL) */
		document_update_timestamp(idx);

		if (FILETYPE_ID(doc_list[idx].file_type) == GEANY_FILETYPES_ALL)
		{
			filetype *ft = filetypes_detect_from_file(idx);
			document_set_filetype(idx, ft);
			if (document_get_cur_idx() == idx)
			{
				app->ignore_callback = TRUE;
				filetypes_select_radio_item(doc_list[idx].file_type);
				app->ignore_callback = FALSE;
			}
		}
		else
			document_set_filetype(idx, doc_list[idx].file_type);

		tm_workspace_update(TM_WORK_OBJECT(app->tm_workspace), TRUE, TRUE, FALSE);
		gtk_label_set_text(GTK_LABEL(doc_list[idx].tab_label), base_name);
		gtk_label_set_text(GTK_LABEL(doc_list[idx].tabmenu_label), base_name);
		msgwin_status_add(_("File %s saved."), doc_list[idx].file_name);
		ui_update_statusbar(idx, -1);
		g_free(base_name);
#ifdef HAVE_VTE
		vte_cwd(doc_list[idx].file_name, FALSE);
#endif
	}
	if (geany_object)
	{
		g_signal_emit_by_name(geany_object, "document-save", idx);
	}
	return TRUE;
}


/* special search function, used from the find entry in the toolbar
 * return TRUE if text was found otherwise FALSE
 * return also TRUE if text is empty  */
gboolean document_search_bar_find(gint idx, const gchar *text, gint flags, gboolean inc)
{
	gint start_pos, search_pos;
	struct TextToFind ttf;

	g_return_val_if_fail(text != NULL, FALSE);
	if (! DOC_IDX_VALID(idx))
		return FALSE;
	if (! *text)
		return TRUE;

	start_pos = (inc) ? sci_get_selection_start(doc_list[idx].sci) :
		sci_get_selection_end(doc_list[idx].sci);	// equal if no selection

	// search cursor to end
	ttf.chrg.cpMin = start_pos;
	ttf.chrg.cpMax = sci_get_length(doc_list[idx].sci);
	ttf.lpstrText = (gchar *)text;
	search_pos = sci_find_text(doc_list[idx].sci, flags, &ttf);

	// if no match, search start to cursor
	if (search_pos == -1)
	{
		ttf.chrg.cpMin = 0;
		ttf.chrg.cpMax = start_pos + strlen(text);
		search_pos = sci_find_text(doc_list[idx].sci, flags, &ttf);
	}

	if (search_pos != -1)
	{
		// unfold maybe folded results
		sci_ensure_line_is_visible(doc_list[idx].sci,
			sci_get_line_from_position(doc_list[idx].sci, ttf.chrgText.cpMin));

		sci_set_selection_start(doc_list[idx].sci, ttf.chrgText.cpMin);
		sci_set_selection_end(doc_list[idx].sci, ttf.chrgText.cpMax);

		// we need to force scrolling in case the cursor is outside of the current visible area
		// doc_list[].scroll_percent doesn't work because sci isn't always updated while searching
		editor_scroll_to_line(doc_list[idx].sci, -1, 0.3F);
		return TRUE;
	}
	else
	{
		if (! inc)
		{
			ui_set_statusbar(FALSE, _("\"%s\" was not found."), text);
		}
		utils_beep();
		sci_goto_pos(doc_list[idx].sci, start_pos, FALSE);	// clear selection
		return FALSE;
	}
}


/* General search function, used from the find dialog.
 * Returns -1 on failure or the start position of the matching text.
 * Will skip past any selection, ignoring it. */
gint document_find_text(gint idx, const gchar *text, gint flags, gboolean search_backwards,
		gboolean scroll, GtkWidget *parent)
{
	gint selection_end, selection_start, search_pos;

	g_return_val_if_fail(text != NULL, -1);
	if (idx == -1 || ! *text) return -1;
	// Sci doesn't support searching backwards with a regex
	if (flags & SCFIND_REGEXP) search_backwards = FALSE;

	selection_start = sci_get_selection_start(doc_list[idx].sci);
	selection_end = sci_get_selection_end(doc_list[idx].sci);
	if ((selection_end - selection_start) > 0)
	{ // there's a selection so go to the end
		if (search_backwards)
			sci_goto_pos(doc_list[idx].sci, selection_start, TRUE);
		else
			sci_goto_pos(doc_list[idx].sci, selection_end, TRUE);
	}

	sci_set_search_anchor(doc_list[idx].sci);
	if (search_backwards)
		search_pos = sci_search_prev(doc_list[idx].sci, flags, text);
	else
		search_pos = sci_search_next(doc_list[idx].sci, flags, text);

	if (search_pos != -1)
	{
		// unfold maybe folded results
		sci_ensure_line_is_visible(doc_list[idx].sci,
			sci_get_line_from_position(doc_list[idx].sci, search_pos));
		if (scroll)
			doc_list[idx].scroll_percent = 0.3F;
	}
	else
	{
		gint sci_len = sci_get_length(doc_list[idx].sci);

		// if we just searched the whole text, give up searching.
		if ((selection_end == 0 && ! search_backwards) ||
			(selection_end == sci_len && search_backwards))
		{
			ui_set_statusbar(FALSE, _("\"%s\" was not found."), text);
			utils_beep();
			return -1;
		}

		// we searched only part of the document, so ask whether to wraparound.
		if (prefs.suppress_search_dialogs ||
			dialogs_show_question_full(parent, GTK_STOCK_FIND, GTK_STOCK_CANCEL,
				_("Wrap search and find again?"), _("\"%s\" was not found."), text))
		{
			gint ret;

			sci_set_current_position(doc_list[idx].sci, (search_backwards) ? sci_len : 0, FALSE);
			ret = document_find_text(idx, text, flags, search_backwards, scroll, parent);
			if (ret == -1)
			{	// return to original cursor position if not found
				sci_set_current_position(doc_list[idx].sci, selection_start, FALSE);
			}
			return ret;
		}
	}
	return search_pos;
}


/* Replaces the selection if it matches, otherwise just finds the next match.
 * Returns: start of replaced text, or -1 if no replacement was made */
gint document_replace_text(gint idx, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean search_backwards)
{
	gint selection_end, selection_start, search_pos;

	g_return_val_if_fail(find_text != NULL && replace_text != NULL, -1);
	if (idx == -1 || ! *find_text) return -1;

	// Sci doesn't support searching backwards with a regex
	if (flags & SCFIND_REGEXP) search_backwards = FALSE;

	selection_start = sci_get_selection_start(doc_list[idx].sci);
	selection_end = sci_get_selection_end(doc_list[idx].sci);
	if (selection_end == selection_start)
	{
		// no selection so just find the next match
		document_find_text(idx, find_text, flags, search_backwards, TRUE, NULL);
		return -1;
	}
	// there's a selection so go to the start before finding to search through it
	// this ensures there is a match
	if (search_backwards)
		sci_goto_pos(doc_list[idx].sci, selection_end, TRUE);
	else
		sci_goto_pos(doc_list[idx].sci, selection_start, TRUE);

	search_pos = document_find_text(idx, find_text, flags, search_backwards, TRUE, NULL);
	// return if the original selected text did not match (at the start of the selection)
	if (search_pos != selection_start) return -1;

	if (search_pos != -1)
	{
		gint replace_len;
		// search next/prev will select matching text, which we use to set the replace target
		sci_target_from_selection(doc_list[idx].sci);
		replace_len = sci_target_replace(doc_list[idx].sci, replace_text, flags & SCFIND_REGEXP);
		// select the replacement - find text will skip past the selected text
		sci_set_selection_start(doc_list[idx].sci, search_pos);
		sci_set_selection_end(doc_list[idx].sci, search_pos + replace_len);
	}
	else
	{
		// no match in the selection
		utils_beep();
	}
	return search_pos;
}


static void show_replace_summary(gint idx, gint count, const gchar *find_text,
		const gchar *replace_text, gboolean escaped_chars)
{
	gchar *escaped_find_text, *escaped_replace_text, *filename;

	if (count == 0)
	{
		ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), find_text);
		return;
	}

	filename = g_path_get_basename(DOC_FILENAME(idx));

	if (escaped_chars)
	{	// escape special characters for showing
		escaped_find_text = g_strescape(find_text, NULL);
		escaped_replace_text = g_strescape(replace_text, NULL);
		ui_set_statusbar(TRUE, _("%s: replaced %d occurrence(s) of \"%s\" with \"%s\"."),
						filename, count, escaped_find_text, escaped_replace_text);
		g_free(escaped_find_text);
		g_free(escaped_replace_text);
	}
	else
	{
		ui_set_statusbar(TRUE, _("%s: replaced %d occurrence(s) of \"%s\" with \"%s\"."),
						filename, count, find_text, replace_text);
	}
	g_free(filename);
}


/* Replace all text matches in a certain range within document idx.
 * If not NULL, *new_range_end is set to the new range endpoint after replacing,
 * or -1 if no text was found.
 * scroll_to_match is whether to scroll the last replacement in view (which also
 * clears the selection).
 * Returns: the number of replacements made. */
static guint
document_replace_range(gint idx, const gchar *find_text, const gchar *replace_text,
	gint flags, gint start, gint end, gboolean scroll_to_match, gint *new_range_end)
{
	gint count = 0;
	struct TextToFind ttf;
	ScintillaObject *sci;

	if (new_range_end != NULL)
		*new_range_end = -1;
	g_return_val_if_fail(find_text != NULL && replace_text != NULL, 0);
	if (idx == -1 || ! *find_text || doc_list[idx].readonly) return 0;

	sci = doc_list[idx].sci;

	sci_start_undo_action(sci);
	ttf.chrg.cpMin = start;
	ttf.chrg.cpMax = end;
	ttf.lpstrText = (gchar*)find_text;

	while (TRUE)
	{
		gint search_pos;
		gint find_len = 0, replace_len = 0;

		search_pos = sci_find_text(sci, flags, &ttf);
		find_len = ttf.chrgText.cpMax - ttf.chrgText.cpMin;
		if (search_pos == -1)
			break;	// no more matches
		if (find_len == 0 && ! NZV(replace_text))
			break;	// nothing to do

		if (search_pos + find_len > end)
			break;	// found text is partly out of range
		else
		{
			gint movepastEOL = 0;

			sci_target_start(sci, search_pos);
			sci_target_end(sci, search_pos + find_len);

			if (find_len <= 0)
			{
				gchar chNext = sci_get_char_at(sci, SSM(sci, SCI_GETTARGETEND, 0, 0));

				if (chNext == '\r' || chNext == '\n')
					movepastEOL = 1;
			}
			replace_len = sci_target_replace(sci, replace_text,
				flags & SCFIND_REGEXP);
			count++;
			if (search_pos == end)
				break;	// Prevent hang when replacing regex $

			// make the next search start after the replaced text
			start = search_pos + replace_len + movepastEOL;
			if (find_len == 0)
				start = SSM(sci, SCI_POSITIONAFTER, start, 0);	// prevent '[ ]*' regex rematching part of replaced text
			ttf.chrg.cpMin = start;
			end += replace_len - find_len;	// update end of range now text has changed
			ttf.chrg.cpMax = end;
		}
	}
	sci_end_undo_action(sci);

	if (count > 0)
	{	// scroll last match in view, will destroy the existing selection
		if (scroll_to_match)
			sci_goto_pos(sci, ttf.chrg.cpMin, TRUE);

		if (new_range_end != NULL)
			*new_range_end = end;
	}
	return count;
}


void document_replace_sel(gint idx, const gchar *find_text, const gchar *replace_text, gint flags,
						  gboolean escaped_chars)
{
	gint selection_end, selection_start, selection_mode, selected_lines, last_line = 0;
	gint max_column = 0, count = 0;
	gboolean replaced = FALSE;

	g_return_if_fail(find_text != NULL && replace_text != NULL);
	if (idx == -1 || ! *find_text) return;

	selection_start = sci_get_selection_start(doc_list[idx].sci);
	selection_end = sci_get_selection_end(doc_list[idx].sci);
	// do we have a selection?
	if ((selection_end - selection_start) == 0)
	{
		utils_beep();
		return;
	}

	selection_mode = sci_get_selection_mode(doc_list[idx].sci);
	selected_lines = sci_get_lines_selected(doc_list[idx].sci);
	// handle rectangle, multi line selections (it doesn't matter on a single line)
	if (selection_mode == SC_SEL_RECTANGLE && selected_lines > 1)
	{
		gint first_line, line;

		sci_start_undo_action(doc_list[idx].sci);

		first_line = sci_get_line_from_position(doc_list[idx].sci, selection_start);
		// Find the last line with chars selected (not EOL char)
		last_line = sci_get_line_from_position(doc_list[idx].sci,
			selection_end - utils_get_eol_char_len(idx));
		last_line = MAX(first_line, last_line);
		for (line = first_line; line < (first_line + selected_lines); line++)
		{
			gint line_start = sci_get_pos_at_line_sel_start(doc_list[idx].sci, line);
			gint line_end = sci_get_pos_at_line_sel_end(doc_list[idx].sci, line);

			// skip line if there is no selection
			if (line_start != INVALID_POSITION)
			{
				// don't let document_replace_range() scroll to match to keep our selection
				gint new_sel_end;

				count += document_replace_range(idx, find_text, replace_text, flags,
								line_start, line_end, FALSE, &new_sel_end);
				if (new_sel_end != -1)
				{
					replaced = TRUE;
					// this gets the greatest column within the selection after replacing
					max_column = MAX(max_column,
						new_sel_end - sci_get_position_from_line(doc_list[idx].sci, line));
				}
			}
		}
		sci_end_undo_action(doc_list[idx].sci);
	}
	else	// handle normal line selection
	{
		count += document_replace_range(idx, find_text, replace_text, flags,
						selection_start, selection_end, TRUE, &selection_end);
		if (selection_end != -1)
			replaced = TRUE;
	}

	if (replaced)
	{	// update the selection for the new endpoint

		if (selection_mode == SC_SEL_RECTANGLE && selected_lines > 1)
		{
			// now we can scroll to the selection and destroy it because we rebuild it later
			//sci_goto_pos(doc_list[idx].sci, selection_start, FALSE);

			// Note: the selection will be wrapped to last_line + 1 if max_column is greater than
			// the highest column on the last line. The wrapped selection is completely different
			// from the original one, so skip the selection at all
			/// TODO is there a better way to handle the wrapped selection?
			if ((sci_get_line_length(doc_list[idx].sci, last_line) - 1) >= max_column)
			{
				// for keeping and adjusting the selection in multi line rectangle selection we
				// need the last line of the original selection and the greatest column number after
				// replacing and set the selection end to the last line at the greatest column
				sci_set_selection_start(doc_list[idx].sci, selection_start);
				sci_set_selection_end(doc_list[idx].sci,
					sci_get_position_from_line(doc_list[idx].sci, last_line) + max_column);
				sci_set_selection_mode(doc_list[idx].sci, selection_mode);
			}
		}
		else
		{
			sci_set_selection_start(doc_list[idx].sci, selection_start);
			sci_set_selection_end(doc_list[idx].sci, selection_end);
		}
	}
	else // no replacements
		utils_beep();

	show_replace_summary(idx, count, find_text, replace_text, escaped_chars);
}


// returns TRUE if at least one replacement was made.
gboolean document_replace_all(gint idx, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean escaped_chars)
{
	gint len, count;
	g_return_val_if_fail(find_text != NULL && replace_text != NULL, FALSE);
	if (idx == -1 || ! *find_text) return FALSE;

	len = sci_get_length(doc_list[idx].sci);
	count = document_replace_range(
			idx, find_text, replace_text, flags, 0, len, TRUE, NULL);

	if (count == 0)
	{
		utils_beep();
	}
	show_replace_summary(idx, count, find_text, replace_text, escaped_chars);
	return (count > 0);
}


void document_set_font(gint idx, const gchar *font_name, gint size)
{
	gint style;

	for (style = 0; style <= 127; style++)
		sci_set_font(doc_list[idx].sci, style, font_name, size);
	// line number and braces
	sci_set_font(doc_list[idx].sci, STYLE_LINENUMBER, font_name, size);
	sci_set_font(doc_list[idx].sci, STYLE_BRACELIGHT, font_name, size);
	sci_set_font(doc_list[idx].sci, STYLE_BRACEBAD, font_name, size);
	// zoom to 100% to prevent confusion
	sci_zoom_off(doc_list[idx].sci);
}


void document_update_tag_list(gint idx, gboolean update)
{
	/* We must call treeviews_update_tag_list() before returning,
	 * to ensure that the symbol list is always updated properly (e.g.
	 * when creating a new document with a partial filename set. */
	gboolean success = FALSE;

	// if the filetype doesn't have a tag parser or it is a new file
	if (idx == -1 || doc_list[idx].file_type == NULL ||
		app->tm_workspace == NULL ||
		! filetype_has_tags(doc_list[idx].file_type) || ! doc_list[idx].file_name)
	{
		// set the default (empty) tag list
		treeviews_update_tag_list(idx, FALSE);
		return;
	}

	if (doc_list[idx].tm_file == NULL)
	{
		gchar *locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);

		doc_list[idx].tm_file = tm_source_file_new(
				locale_filename, FALSE, doc_list[idx].file_type->name);
		g_free(locale_filename);

		if (doc_list[idx].tm_file)
		{
			if (!tm_workspace_add_object(doc_list[idx].tm_file))
			{
				tm_work_object_free(doc_list[idx].tm_file);
				doc_list[idx].tm_file = NULL;
			}
			else
			{
				if (update)
					tm_source_file_update(doc_list[idx].tm_file, TRUE, FALSE, TRUE);
				success = TRUE;
			}
		}
	}
	else
	{
		success = tm_source_file_update(doc_list[idx].tm_file, TRUE, FALSE, TRUE);
		if (! success)
			geany_debug("tag list updating failed");
	}
	treeviews_update_tag_list(idx, success);
}


/* Caches the list of project typenames, as a space separated GString.
 * Returns: TRUE if typenames have changed.
 * (*types) is set to the list of typenames, or NULL if there are none. */
static gboolean get_project_typenames(const GString **types, gint lang)
{
	static GString *last_typenames = NULL;
	GString *s = NULL;

	if (app->tm_workspace)
	{
		GPtrArray *tags_array = app->tm_workspace->work_object.tags_array;

		if (tags_array)
		{
			s = symbols_find_tags_as_string(tags_array, TM_GLOBAL_TYPE_MASK, lang);
		}
	}

	if (s && last_typenames && g_string_equal(s, last_typenames))
	{
		g_string_free(s, TRUE);
		*types = last_typenames;
		return FALSE;	// project typenames haven't changed
	}
	// cache typename list for next time
	if (last_typenames)
		g_string_free(last_typenames, TRUE);
	last_typenames = s;

	*types = s;
	if (s == NULL) return FALSE;
	return TRUE;
}


/* If sci is NULL, update project typenames for all documents that support typenames,
 * if typenames have changed.
 * If sci is not NULL, then if sci supports typenames, project typenames are updated
 * if necessary, and typename keywords are set for sci.
 * Returns: TRUE if any scintilla type keywords were updated. */
static gboolean update_type_keywords(ScintillaObject *sci, gint lang)
{
	gboolean ret = FALSE;
	guint n;
	const GString *s;

	if (sci != NULL && editor_lexer_get_type_keyword_idx(sci_get_lexer(sci)) == -1)
		return FALSE;

	if (! get_project_typenames(&s, lang))
	{	// typenames have not changed
		if (s != NULL && sci != NULL)
		{
			gint keyword_idx = editor_lexer_get_type_keyword_idx(sci_get_lexer(sci));

			sci_set_keywords(sci, keyword_idx, s->str);
			if (! delay_colourise)
			{
				sci_colourise(sci, 0, -1);
			}
		}
		return FALSE;
	}
	g_return_val_if_fail(s != NULL, FALSE);

	for (n = 0; n < doc_array->len; n++)
	{
		ScintillaObject *wid = doc_list[n].sci;

		if (wid)
		{
			gint keyword_idx = editor_lexer_get_type_keyword_idx(sci_get_lexer(wid));

			if (keyword_idx > 0)
			{
				sci_set_keywords(wid, keyword_idx, s->str);
				if (! delay_colourise)
				{
					sci_colourise(wid, 0, -1);
				}
				ret = TRUE;
			}
		}
	}
	return ret;
}


/* sets the filetype of the document (sets syntax highlighting and tagging) */
void document_set_filetype(gint idx, filetype *type)
{
	gboolean colourise = FALSE;
	gboolean ft_changed;

	if (type == NULL || ! DOC_IDX_VALID(idx))
		return;

	geany_debug("%s : %s (%s)",
		(doc_list[idx].file_name != NULL) ? doc_list[idx].file_name : "unknown",
		(type->name != NULL) ? type->name : "unknown",
		(doc_list[idx].encoding != NULL) ? doc_list[idx].encoding : "unknown");

	ft_changed = (doc_list[idx].file_type != type);
	if (ft_changed)	// filetype has changed
	{
		doc_list[idx].file_type = type;

		// delete tm file object to force creation of a new one
		if (doc_list[idx].tm_file != NULL)
		{
			tm_workspace_remove_object(doc_list[idx].tm_file, TRUE, TRUE);
			doc_list[idx].tm_file = NULL;
		}
		highlighting_set_styles(doc_list[idx].sci, type->id);
		build_menu_update(idx);
		colourise = TRUE;
	}

	document_update_tag_list(idx, TRUE);
	if (! delay_colourise)
	{
		/* Check if project typename keywords have changed.
		 * If they haven't, we may need to colourise the document. */
		if (! update_type_keywords(doc_list[idx].sci, type->lang) && colourise)
			sci_colourise(doc_list[idx].sci, 0, -1);
	}
	if (ft_changed)
	{
		utils_get_current_function(-1, NULL);
		ui_update_statusbar(idx, -1);
	}
}


gchar *document_get_eol_mode(gint idx)
{
	if (idx == -1) return '\0';

	switch (sci_get_eol_mode(doc_list[idx].sci))
	{
		case SC_EOL_CRLF: return _("Win (CRLF)"); break;
		case SC_EOL_CR: return _("Mac (CR)"); break;
		case SC_EOL_LF:
		default: return _("Unix (LF)"); break;
	}
}


static void fold_all(gint idx, gboolean want_fold)
{
	gint lines, first, i;

	if (! DOC_IDX_VALID(idx) || ! editor_prefs.folding) return;

	lines = sci_get_line_count(doc_list[idx].sci);
	first = sci_get_first_visible_line(doc_list[idx].sci);

	for (i = 0; i < lines; i++)
	{
		gint level = sci_get_fold_level(doc_list[idx].sci, i);
		if (level & SC_FOLDLEVELHEADERFLAG)
		{
			if (sci_get_fold_expanded(doc_list[idx].sci, i) == want_fold)
					sci_toggle_fold(doc_list[idx].sci, i);
		}
	}
	editor_scroll_to_line(doc_list[idx].sci, first, 0.0F);
}


void document_unfold_all(gint idx)
{
	fold_all(idx, FALSE);
}


void document_fold_all(gint idx)
{
	fold_all(idx, TRUE);
}


void document_clear_indicators(gint idx)
{
	glong last_pos;

	g_return_if_fail(DOC_IDX_VALID(idx));

	last_pos = sci_get_length(doc_list[idx].sci);
	if (last_pos > 0)
	{
		sci_start_styling(doc_list[idx].sci, 0, INDIC2_MASK);
		sci_set_styling(doc_list[idx].sci, last_pos, 0);
	}
	sci_marker_delete_all(doc_list[idx].sci, 0);	// remove the yellow error line marker
}


void document_set_indicator(gint idx, gint line)
{
	gint start, end, current_mask;
	guint i = 0, len;
	gchar *linebuf;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	start = sci_get_position_from_line(doc_list[idx].sci, line);
	end = sci_get_position_from_line(doc_list[idx].sci, line + 1);

	// skip blank lines
	if ((start + 1) == end ||
		sci_get_line_length(doc_list[idx].sci, line) == utils_get_eol_char_len(idx))
		return;

	// don't set the indicator on whitespace
	len = end - start;
	linebuf = sci_get_line(doc_list[idx].sci, line);

	while (isspace(linebuf[i])) i++;
	while (len > 1 && len > i && isspace(linebuf[len-1])) len--;
	g_free(linebuf);

	current_mask = sci_get_style_at(doc_list[idx].sci, start);
	current_mask &= INDICS_MASK;
	current_mask |= INDIC2_MASK;
	sci_start_styling(doc_list[idx].sci, start + i, INDIC2_MASK);
	//geany_debug("%p\tline: %d\tstart-end: %d - %d\t%d - %i", doc_list[idx].sci, line, start, end, len, i);
	sci_set_styling(doc_list[idx].sci, len - i, current_mask);
}


void document_replace_tabs(gint idx)
{
	gint search_pos, pos_in_line, current_tab_true_length;
	gint tab_len;
	gchar *tab_str;
	struct TextToFind ttf;

	if (! DOC_IDX_VALID(idx)) return;

	sci_start_undo_action(doc_list[idx].sci);
	tab_len = sci_get_tab_width(doc_list[idx].sci);
	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(doc_list[idx].sci);
	ttf.lpstrText = (gchar*) "\t";

	while (TRUE)
	{
		search_pos = sci_find_text(doc_list[idx].sci, SCFIND_MATCHCASE, &ttf);
		if (search_pos == -1)
			break;

		pos_in_line = sci_get_col_from_position(doc_list[idx].sci,search_pos);
		current_tab_true_length = tab_len - (pos_in_line % tab_len);
		tab_str = g_strnfill(current_tab_true_length, ' ');
		sci_target_start(doc_list[idx].sci, search_pos);
		sci_target_end(doc_list[idx].sci, search_pos + 1);
		sci_target_replace(doc_list[idx].sci, tab_str, FALSE);
		ttf.chrg.cpMin = search_pos + current_tab_true_length - 1;	// next search starts after replacement
		ttf.chrg.cpMax += current_tab_true_length - 1;	// update end of range now text has changed
		g_free(tab_str);
	}
	sci_end_undo_action(doc_list[idx].sci);
}


void document_strip_line_trailing_spaces(gint idx, gint line)
{
	gint line_start = sci_get_position_from_line(doc_list[idx].sci, line);
	gint line_end = sci_get_line_end_position(doc_list[idx].sci, line);
	gint i = line_end - 1;
	gchar ch = sci_get_char_at(doc_list[idx].sci, i);

	while ((i >= line_start) && ((ch == ' ') || (ch == '\t')))
	{
		i--;
		ch = sci_get_char_at(doc_list[idx].sci, i);
	}
	if (i < (line_end-1))
	{
		sci_target_start(doc_list[idx].sci, i + 1);
		sci_target_end(doc_list[idx].sci, line_end);
		sci_target_replace(doc_list[idx].sci, "", FALSE);
	}
}


void document_strip_trailing_spaces(gint idx)
{
	gint max_lines = sci_get_line_count(doc_list[idx].sci);
	gint line;

	sci_start_undo_action(doc_list[idx].sci);

	for (line = 0; line < max_lines; line++)
	{
		document_strip_line_trailing_spaces(idx, line);
	}
	sci_end_undo_action(doc_list[idx].sci);
}


void document_ensure_final_newline(gint idx)
{
	gint max_lines = sci_get_line_count(doc_list[idx].sci);
	gboolean append_newline = (max_lines == 1);
	gint end_document = sci_get_position_from_line(doc_list[idx].sci, max_lines);

	if (max_lines > 1)
	{
		append_newline = end_document > sci_get_position_from_line(doc_list[idx].sci, max_lines - 1);
	}
	if (append_newline)
	{
		const gchar *eol = "\n";
		switch (sci_get_eol_mode(doc_list[idx].sci))
		{
			case SC_EOL_CRLF:
				eol = "\r\n";
				break;
			case SC_EOL_CR:
				eol = "\r";
				break;
		}
		sci_insert_text(doc_list[idx].sci, end_document, eol);
	}
}


void document_set_encoding(gint idx, const gchar *new_encoding)
{
	if (! DOC_IDX_VALID(idx) || new_encoding == NULL ||
		utils_str_equal(new_encoding, doc_list[idx].encoding)) return;

	g_free(doc_list[idx].encoding);
	doc_list[idx].encoding = g_strdup(new_encoding);

	ui_update_statusbar(idx, -1);
	gtk_widget_set_sensitive(lookup_widget(app->window, "menu_write_unicode_bom1"),
			encodings_is_unicode_charset(doc_list[idx].encoding));
}


/* own Undo / Redo implementation to be able to undo / redo changes
 * to the encoding or the Unicode BOM (which are Scintilla independet).
 * All Scintilla events are stored in the undo / redo buffer and are passed through. */

/* Clears the Undo and Redo buffer (to be called when reloading or closing the document) */
void document_undo_clear(gint idx)
{
	undo_action *a;

	while (g_trash_stack_height(&doc_list[idx].undo_actions) > 0)
	{
		a = g_trash_stack_pop(&doc_list[idx].undo_actions);
		if (a != NULL)
		{
			switch (a->type)
			{
				case UNDO_ENCODING: g_free(a->data); break;
				default: break;
			}

			g_free(a);
		}
	}
	doc_list[idx].undo_actions = NULL;

	while (g_trash_stack_height(&doc_list[idx].redo_actions) > 0)
	{
		a = g_trash_stack_pop(&doc_list[idx].redo_actions);
		if (a != NULL)
		{
			switch (a->type)
			{
				case UNDO_ENCODING: g_free(a->data); break;
				default: break;
			}

			g_free(a);
		}
	}
	doc_list[idx].redo_actions = NULL;

	doc_list[idx].changed = FALSE;
	if (! main_status.quitting) document_set_text_changed(idx);

	//geany_debug("%s: new undo stack height: %d, new redo stack height: %d", __func__,
				//g_trash_stack_height(&doc_list[idx].undo_actions), g_trash_stack_height(&doc_list[idx].redo_actions));
}


void document_undo_add(gint idx, guint type, gpointer data)
{
	undo_action *action;

	if (! DOC_IDX_VALID(idx)) return;

	action = g_new0(undo_action, 1);
	action->type = type;
	action->data = data;

	g_trash_stack_push(&doc_list[idx].undo_actions, action);

	doc_list[idx].changed = TRUE;
	document_set_text_changed(idx);
	ui_update_popup_reundo_items(idx);

	//geany_debug("%s: new stack height: %d, added type: %d", __func__,
				//g_trash_stack_height(&doc_list[idx].undo_actions), action->type);
}


gboolean document_can_undo(gint idx)
{
	if (! DOC_IDX_VALID(idx)) return FALSE;

	if (g_trash_stack_height(&doc_list[idx].undo_actions) > 0 || sci_can_undo(doc_list[idx].sci))
		return TRUE;
	else
		return FALSE;
}


static void update_changed_state(gint idx)
{
	doc_list[idx].changed =
		(sci_is_modified(doc_list[idx].sci) ||
		doc_list[idx].has_bom != doc_list[idx].saved_encoding.has_bom ||
		! utils_str_equal(doc_list[idx].encoding, doc_list[idx].saved_encoding.encoding));
	document_set_text_changed(idx);
}


void document_undo(gint idx)
{
	undo_action *action;

	if (! DOC_IDX_VALID(idx)) return;

	action = g_trash_stack_pop(&doc_list[idx].undo_actions);

	if (action == NULL)
	{
		// fallback, should not be necessary
		geany_debug("%s: fallback used", __func__);
		sci_undo(doc_list[idx].sci);
	}
	else
	{
		switch (action->type)
		{
			case UNDO_SCINTILLA:
			{
				document_redo_add(idx, UNDO_SCINTILLA, NULL);

				sci_undo(doc_list[idx].sci);
				break;
			}
			case UNDO_BOM:
			{
				document_redo_add(idx, UNDO_BOM, GINT_TO_POINTER(doc_list[idx].has_bom));

				doc_list[idx].has_bom = GPOINTER_TO_INT(action->data);
				ui_update_statusbar(idx, -1);
				ui_document_show_hide(idx);
				break;
			}
			case UNDO_ENCODING:
			{
				// use the "old" encoding
				document_redo_add(idx, UNDO_ENCODING, g_strdup(doc_list[idx].encoding));

				document_set_encoding(idx, (const gchar*)action->data);

				app->ignore_callback = TRUE;
				encodings_select_radio_item((const gchar*)action->data);
				app->ignore_callback = FALSE;

				g_free(action->data);
				break;
			}
			default: break;
		}
	}
	g_free(action); // free the action which was taken from the stack

	update_changed_state(idx);
	ui_update_popup_reundo_items(idx);
	//geany_debug("%s: new stack height: %d", __func__, g_trash_stack_height(&doc_list[idx].undo_actions));
}


gboolean document_can_redo(gint idx)
{
	if (! DOC_IDX_VALID(idx)) return FALSE;

	if (g_trash_stack_height(&doc_list[idx].redo_actions) > 0 || sci_can_redo(doc_list[idx].sci))
		return TRUE;
	else
		return FALSE;
}


void document_redo(gint idx)
{
	undo_action *action;

	if (! DOC_IDX_VALID(idx)) return;

	action = g_trash_stack_pop(&doc_list[idx].redo_actions);

	if (action == NULL)
	{
		// fallback, should not be necessary
		geany_debug("%s: fallback used", __func__);
		sci_redo(doc_list[idx].sci);
	}
	else
	{
		switch (action->type)
		{
			case UNDO_SCINTILLA:
			{
				document_undo_add(idx, UNDO_SCINTILLA, NULL);

				sci_redo(doc_list[idx].sci);
				break;
			}
			case UNDO_BOM:
			{
				document_undo_add(idx, UNDO_BOM, GINT_TO_POINTER(doc_list[idx].has_bom));

				doc_list[idx].has_bom = GPOINTER_TO_INT(action->data);
				ui_update_statusbar(idx, -1);
				ui_document_show_hide(idx);
				break;
			}
			case UNDO_ENCODING:
			{
				document_undo_add(idx, UNDO_ENCODING, g_strdup(doc_list[idx].encoding));

				document_set_encoding(idx, (const gchar*)action->data);

				app->ignore_callback = TRUE;
				encodings_select_radio_item((const gchar*)action->data);
				app->ignore_callback = FALSE;

				g_free(action->data);
				break;
			}
			default: break;
		}
	}
	g_free(action); // free the action which was taken from the stack

	update_changed_state(idx);
	ui_update_popup_reundo_items(idx);
	//geany_debug("%s: new stack height: %d", __func__, g_trash_stack_height(&doc_list[idx].redo_actions));
}


static void document_redo_add(gint idx, guint type, gpointer data)
{
	undo_action *action;

	if (! DOC_IDX_VALID(idx)) return;

	action = g_new0(undo_action, 1);
	action->type = type;
	action->data = data;

	g_trash_stack_push(&doc_list[idx].redo_actions, action);

	doc_list[idx].changed = TRUE;
	document_set_text_changed(idx);
	ui_update_popup_reundo_items(idx);

	//geany_debug("%s: new stack height: %d, added type: %d", __func__,
				//g_trash_stack_height(&doc_list[idx].redo_actions), action->type);
}


/* Gets the status colour of the document, or NULL if default widget
 * colouring should be used. */
GdkColor *document_get_status(gint idx)
{
	static GdkColor red = {0, 0xFFFF, 0, 0};
	static GdkColor green = {0, 0, 0x7FFF, 0};
	GdkColor *color = NULL;

	if (doc_list[idx].changed)
		color = &red;
	else if (doc_list[idx].readonly)
		color = &green;

	return color;	// return pointer to static GdkColor.
}


// useful debugging function (usually debug macros aren't enabled)
#ifdef GEANY_DEBUG
document *doc(gint idx)
{
	return DOC_IDX_VALID(idx) ? &doc_list[idx] : NULL;
}
#endif


static GArray *doc_indexes = NULL;

/* Cache the current document indexes and prevent any colourising until
 * document_colourise_new() is called. */
void document_delay_colourise()
{
	gint n;

	g_return_if_fail(delay_colourise == FALSE);
	g_return_if_fail(doc_indexes == NULL);

	// make an array containing all the current document indexes
	doc_indexes = g_array_new(FALSE, FALSE, sizeof(gint));
	for (n = 0; n < (gint) doc_array->len; n++)
	{
		if (DOC_IDX_VALID(n))
			g_array_append_val(doc_indexes, n);
	}
	delay_colourise = TRUE;
}


/* Colourise only newly opened documents and existing documents whose project typenames
 * keywords have changed.
 * document_delay_colourise() should already have been called. */
void document_colourise_new()
{
	guint n, i;
	/* A bitset representing which docs need [re]colourising.
	 * (use gint8 to save memory because gboolean = gint) */
	gint8 *doc_set = g_newa(gint8, doc_array->len);
	gboolean recolour = FALSE;	// whether to recolourise existing typenames

	g_return_if_fail(delay_colourise == TRUE);
	g_return_if_fail(doc_indexes != NULL);

	// first assume recolourising all docs
	memset(doc_set, TRUE, doc_array->len * sizeof(gint8));

	// remove existing docs from the set if they don't use typenames or typenames haven't changed
	recolour = update_type_keywords(NULL, -2);
	for (i = 0; i < doc_indexes->len; i++)
	{
		ScintillaObject *sci;

		n = g_array_index(doc_indexes, gint, i);
		sci = doc_list[n].sci;
		if (! recolour || (sci && editor_lexer_get_type_keyword_idx(sci_get_lexer(sci)) == -1))
		{
			doc_set[n] = FALSE;
		}
	}
	// colourise all in the doc_set
	for (n = 0; n < doc_array->len; n++)
	{
		if (doc_set[n] && doc_list[n].is_valid)
			sci_colourise(doc_list[n].sci, 0, -1);
	}
	delay_colourise = FALSE;
	g_array_free(doc_indexes, TRUE);
	doc_indexes = NULL;

	/* now that the current document is colourised, fold points are now accurate,
	 * so force an update of the current function/tag. */
	utils_get_current_function(-1, NULL);
	ui_update_statusbar(-1, -1);
}


/* Inserts the given colour (format should be #...), if there is a selection starting with 0x...
 * the replacement will also start with 0x... */
void document_insert_colour(gint idx, const gchar *colour)
{
	g_return_if_fail(DOC_IDX_VALID(idx));

	if (sci_can_copy(doc_list[idx].sci))
	{
		gint start = sci_get_selection_start(doc_list[idx].sci);
		const gchar *replacement = colour;

		if (sci_get_char_at(doc_list[idx].sci, start) == '0' &&
			sci_get_char_at(doc_list[idx].sci, start + 1) == 'x')
		{
			sci_set_selection_start(doc_list[idx].sci, start + 2);
			sci_set_selection_end(doc_list[idx].sci, start + 8);
			replacement++; // skip the leading "0x"
		}
		else if (sci_get_char_at(doc_list[idx].sci, start - 1) == '#')
		{	// double clicking something like #00ffff may only select 00ffff because of wordchars
			replacement++; // so skip the '#' to only replace the colour value
		}
		sci_replace_sel(doc_list[idx].sci, replacement);
	}
	else
		sci_add_text(doc_list[idx].sci, colour);
}


gint document_clone(gint old_idx, const gchar *utf8_filename)
{
	// create a new file and copy file content and properties
	gint len, idx;
	gchar *text;

	len = sci_get_length(doc_list[old_idx].sci) + 1;
	text = (gchar*) g_malloc(len);
	sci_get_text(doc_list[old_idx].sci, len, text);
	// use old file type (or maybe NULL for auto detect would be better?)
	idx = document_new_file(utf8_filename, doc_list[old_idx].file_type, text);
	g_free(text);

	// copy file properties
	doc_list[idx].line_wrapping = doc_list[old_idx].line_wrapping;
	doc_list[idx].readonly = doc_list[old_idx].readonly;
	doc_list[idx].has_bom = doc_list[old_idx].has_bom;
	document_set_encoding(idx, doc_list[old_idx].encoding);
	sci_set_lines_wrapped(doc_list[idx].sci, doc_list[idx].line_wrapping);
	sci_set_readonly(doc_list[idx].sci, doc_list[idx].readonly);

	ui_document_show_hide(idx);
	return idx;
}


