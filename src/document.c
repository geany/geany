/*
 *      document.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
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

#include "document.h"
#include "support.h"
#include "sciwrappers.h"
#include "sci_cb.h"
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


/* dynamic array of document elements to hold all information of the notebook tabs */
GArray *doc_array;


/* Returns -1 if no text found or the new range endpoint after replacing. */
static gint
document_replace_range(gint idx, const gchar *find_text, const gchar *replace_text,
	gint flags, gint start, gint end, gboolean escaped_chars);

static void document_undo_clear(gint idx);
static void document_redo_add(gint idx, guint type, gpointer data);



/* returns the index of the notebook page which has the given filename
 * is_tm_filename is needed when passing TagManager filenames because they are
 * dereferenced, and would not match the link filename. */
gint document_find_by_filename(const gchar *filename, gboolean is_tm_filename)
{
	guint i;

	if (! filename) return -1;

	for(i = 0; i < doc_array->len; i++)
	{
		gchar *dl_fname = (is_tm_filename && doc_list[i].tm_file) ?
							doc_list[i].tm_file->file_name : doc_list[i].file_name;
#ifdef G_OS_WIN32
		// ignore the case of filenames and paths under WIN32, causes errors if not
		if (dl_fname && ! strcasecmp(dl_fname, filename)) return i;
#else
		if (dl_fname && utils_str_equal(dl_fname, filename)) return i;
#endif
	}
	return -1;
}


/* returns the index of the notebook page which has sci */
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
	ScintillaObject *sci = (ScintillaObject*)gtk_notebook_get_nth_page(GTK_NOTEBOOK(app->notebook), cur_page);

	if (cur_page == -1)
		return -1;
	else
		return document_find_by_sci(sci);
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
	if (DOC_IDX_VALID(idx) && ! app->quitting)
	{
		ui_update_tab_status(idx);
		ui_save_buttons_toggle(doc_list[idx].changed);
		ui_set_window_title(idx);
		ui_update_statusbar(idx, -1);
	}
}


// Apply just the prefs that can change in the Preferences dialog
void document_apply_update_prefs(ScintillaObject *sci)
{
	sci_set_mark_long_lines(sci, app->long_line_type, app->long_line_column, app->long_line_color);

	sci_set_tab_width(sci, app->pref_editor_tab_width);
	sci_set_use_tabs(sci, app->pref_editor_use_tabs);

	sci_set_autoc_max_height(sci, app->autocompletion_max_height);

	sci_set_indentionguides(sci, app->pref_editor_show_indent_guide);
	sci_set_visible_white_spaces(sci, app->pref_editor_show_white_space);
	sci_set_visible_eols(sci, app->pref_editor_show_line_endings);

	sci_set_folding_margin_visible(sci, app->pref_editor_folding);
}


/* Sets is_valid to FALSE and initializes some members to NULL, to mark it uninitialized.
 * The flag is_valid is set to TRUE in document_create_new_sci(). */
static void init_doc_struct(document *new_doc)
{
	new_doc->is_valid = FALSE;
	new_doc->has_tags = FALSE;
	new_doc->use_auto_indention = app->pref_editor_use_auto_indention;
	new_doc->line_breaking = app->pref_editor_line_breaking;
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
	sci_assign_cmdkey(sci, SCK_HOME, SCI_VCHOMEWRAP);
	sci_assign_cmdkey(sci, SCK_END,  SCI_LINEENDWRAP);
	// disable select all to be able to redefine it
	sci_clear_cmdkey(sci, 'A' | (SCMOD_CTRL << 16));

	document_apply_update_prefs(sci);

	sci_set_tab_indents(sci, app->use_tab_to_indent);
	sci_set_symbol_margin(sci, app->show_markers_margin);
	sci_set_line_numbers(sci, app->show_linenumber_margin, 0);
	sci_set_lines_wrapped(sci, app->pref_editor_line_breaking);

	// signal for insert-key(works without too, but to update the right status bar)
	//g_signal_connect((GtkWidget*) sci, "key-press-event",
					//G_CALLBACK(keybindings_got_event), GINT_TO_POINTER(new_idx));
	// signal for the popup menu
	g_signal_connect((GtkWidget*) sci, "button-press-event",
					G_CALLBACK(on_editor_button_press_event), GINT_TO_POINTER(new_idx));

	pfd = pango_font_description_from_string(app->editor_font);
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
	this->line_breaking = app->pref_editor_line_breaking;
	this->use_auto_indention = app->pref_editor_use_auto_indention;
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

	ui_close_buttons_toggle();

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
		tm_workspace_remove_object(doc_list[idx].tm_file, TRUE);

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
			ui_update_tag_list(-1, FALSE);
			//on_notebook1_switch_page(GTK_NOTEBOOK(app->notebook), NULL, 0, NULL);
			ui_set_window_title(-1);
			ui_save_buttons_toggle(FALSE);
			ui_close_buttons_toggle();
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


/* This creates a new document, by clearing the text widget and setting the
   current filename to NULL. */
void document_new_file(filetype *ft)
{
	gint idx = document_create_new_sci(NULL);
	gchar *template = templates_get_template_new_file(ft);

	g_assert(idx != -1);

	sci_set_undo_collection(doc_list[idx].sci, FALSE); // avoid creation of an undo action
	sci_clear_all(doc_list[idx].sci);
	sci_set_text(doc_list[idx].sci, template);
	g_free(template);

#ifdef G_OS_WIN32
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_CRLF);
#else
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_LF);
#endif
	sci_set_undo_collection(doc_list[idx].sci, TRUE);
	sci_empty_undo_buffer(doc_list[idx].sci);

	doc_list[idx].encoding = g_strdup(encodings[app->pref_editor_default_encoding].charset);
	// store the opened encoding for undo/redo
	store_saved_encoding(idx);

	//document_set_filetype(idx, (ft == NULL) ? filetypes[GEANY_FILETYPES_ALL] : ft);
	document_set_filetype(idx, ft);	// also clears taglist
	if (ft == NULL) filetypes[GEANY_FILETYPES_ALL]->style_func_ptr(doc_list[idx].sci);
	ui_set_window_title(idx);
	build_menu_update(idx);

	doc_list[idx].mtime = time(NULL);
	doc_list[idx].changed = FALSE;

	document_update_tag_list(idx, FALSE);
	document_set_text_changed(idx);
	ui_document_show_hide(idx); //update the document menu

	sci_set_line_numbers(doc_list[idx].sci, app->show_linenumber_margin, 0);
	sci_goto_pos(doc_list[idx].sci, 0, TRUE);

	// "the" SCI signal (connect after initial setup(i.e. adding text))
	g_signal_connect((GtkWidget*) doc_list[idx].sci, "sci-notify",
				G_CALLBACK(on_editor_notification), GINT_TO_POINTER(idx));

	msgwin_status_add(_("New file opened."));
}


typedef struct
{
	gchar		*data;	// null-terminated file data
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
	enc_idx = encodings_scan_unicode_bom(filedata->data, filedata->len, NULL);
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

	if (filedata->len == 0)
	{
		// we have no data so assume UTF-8
		filedata->enc = g_strdup("UTF-8");
	}
	else
	{
		// first check for a BOM
		GeanyEncodingIndex enc_idx =
			encodings_scan_unicode_bom(filedata->data, filedata->len, NULL);

		if (enc_idx != GEANY_ENCODING_NONE)
		{
			filedata->enc = g_strdup(encodings[enc_idx].charset);
			filedata->bom = TRUE;

			if (enc_idx != GEANY_ENCODING_UTF_8) // the BOM indicated something else than UTF-8
			{
				gchar *converted_text = encodings_convert_to_utf8_from_charset(
										filedata->data, filedata->len, filedata->enc, FALSE);
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
					filedata->len, &filedata->enc);

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

	encodings_scan_unicode_bom(filedata->data, filedata->len, &bom_len);
	g_return_if_fail(bom_len != 0);

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

	filedata->data = NULL;
	filedata->len = 0;
	filedata->enc = NULL;
	filedata->bom = FALSE;
	filedata->readonly = FALSE;

	if (stat(locale_filename, &st) != 0)
	{
		msgwin_status_add(_("Could not open file %s (%s)"), utf8_filename, g_strerror(errno));
		return FALSE;
	}

	filedata->mtime = st.st_mtime;

#ifdef G_OS_WIN32
	if (! g_file_get_contents(utf8_filename, &filedata->data, NULL, &err))
#else
	if (! g_file_get_contents(locale_filename, &filedata->data, NULL, &err))
#endif
	{
		msgwin_status_add(err->message);
		g_error_free(err);
		return FALSE;
	}

	// use strlen to check for null chars
	filedata->len = strlen(filedata->data);

	/* check whether the size of the loaded data is equal to the size of the file in the filesystem */
	if (filedata->len != (gsize) st.st_size)
	{
		gchar *warn_msg = _("The file \"%s\" could not be opened properly and has been truncated. "
				"This can occur if the file contains a NULL byte. "
				"Be aware that saving it can cause data loss.\nThe file was set to read-only.");

		if (app->main_window_realized)
			dialogs_show_msgbox(GTK_MESSAGE_WARNING, warn_msg, utf8_filename);

		msgwin_status_add(warn_msg, utf8_filename);

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
			msgwin_status_add(_("The file \"%s\" is not valid %s."), utf8_filename, forced_enc);
			utils_beep();
			g_free(filedata->data);
			return FALSE;
		}
	}
	else if (! handle_encoding(filedata))
	{
		msgwin_status_add(
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


/* To open a new file, set idx to -1; filename should be locale encoded.
 * To reload a file, set the idx for the document to be reloaded; filename should be NULL.
 * Returns: idx of the opened file or -1 if an error occurred.
 */
int document_open_file(gint idx, const gchar *filename, gint pos, gboolean readonly, filetype *ft,
					   const gchar *forced_enc)
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
		// filename must not be NULL when it is a new file
		if (filename == NULL)
		{
			ui_set_statusbar(_("Invalid filename"));
			return -1;
		}

		// try to get the UTF-8 equivalent for the filename, fallback to filename if error
		locale_filename = g_strdup(filename);
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
			return idx;
		}
	}

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
	sci_set_text(doc_list[idx].sci, filedata.data);	// NULL terminated data

	// detect & set line endings
	editor_mode = utils_get_line_endings(filedata.data, filedata.len);
	sci_set_eol_mode(doc_list[idx].sci, editor_mode);
	g_free(filedata.data);

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
	sci_set_line_numbers(doc_list[idx].sci, app->show_linenumber_margin, 0);

	if (cl_options.goto_line >= 0)
	{	// goto line which was specified on command line and then undefine the line
		sci_goto_line(doc_list[idx].sci, cl_options.goto_line - 1, TRUE);
		doc_list[idx].scroll_percent = 0.5F;
		cl_options.goto_line = -1;
	}
	else if (pos >= 0)
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

	if (! reload)
	{
		// "the" SCI signal (connect after initial setup(i.e. adding text))
		g_signal_connect((GtkWidget*) doc_list[idx].sci, "sci-notify",
					G_CALLBACK(on_editor_notification), GINT_TO_POINTER(idx));

		use_ft = (ft != NULL) ? ft : filetypes_get_from_filename(idx);
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
	if (! app->opening_session_files) ui_add_recent_file(utf8_filename);

	if (reload)
		msgwin_status_add(_("File %s reloaded."), utf8_filename);
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

	for (i = 0; ; i++)
	{
		if (list[i] == NULL) break;
		filename = g_filename_from_uri(list[i], NULL, NULL);
		if (filename == NULL) continue;
		document_open_file(-1, filename, 0, FALSE, NULL, NULL);
		g_free(filename);
	}

	g_strfreev(list);
}


gint document_reload_file(gint idx, const gchar *forced_enc)
{
	gint pos = 0;

	if (idx < 0 || ! doc_list[idx].is_valid)
		return -1;

	// try to set the cursor to the position before reloading
	pos = sci_get_current_position(doc_list[idx].sci);
	return document_open_file(idx, NULL, pos, doc_list[idx].readonly,
					doc_list[idx].file_type, forced_enc);
}


static gboolean document_update_timestamp(gint idx)
{
	struct stat st;
	gchar *locale_filename;

	g_return_val_if_fail(DOC_IDX_VALID(idx), FALSE);

	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);

	if (stat(locale_filename, &st) != 0)
	{
		msgwin_status_add(_("Could not open file %s (%s)"), doc_list[idx].file_name,
			g_strerror(errno));
		g_free(locale_filename);
		return FALSE;
	}

	doc_list[idx].mtime = st.st_mtime; // get the modification time from file and keep it
	g_free(locale_filename);
	return TRUE;
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
	// the changed flag should exclude the readonly flag, but check it anyway for safety
	if (! force && (! doc_list[idx].changed || doc_list[idx].readonly)) return FALSE;

	if (doc_list[idx].file_name == NULL)
	{
		msgwin_status_add(_("Error saving file."));
		utils_beep();
		return FALSE;
	}

	// replaces tabs by spaces
	if (app->pref_editor_replace_tabs) document_replace_tabs(idx);
	// strip trailing spaces
	if (app->pref_editor_trail_space) document_strip_trailing_spaces(idx);
	// ensure the file has a newline at the end
	if (app->pref_editor_new_line) document_ensure_final_newline(idx);
	// ensure there a really the same EOL chars
	sci_convert_eols(doc_list[idx].sci, sci_get_eol_mode(doc_list[idx].sci));

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
		gsize conv_len;

		// try to convert it from UTF-8 to original encoding
		conv_file_contents = g_convert(data, len-1, doc_list[idx].encoding, "UTF-8",
													NULL, &conv_len, &conv_error);

		if (conv_error != NULL)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
	_("An error occurred while converting the file from UTF-8 in \"%s\". The file remains unsaved."
	  "\nError message: %s\n"),
			doc_list[idx].encoding, conv_error->message);
			geany_debug("encoding error: %s)", conv_error->message);
			g_error_free(conv_error);
			g_free(data);
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
	fp = fopen(locale_filename, "wb"); // this should fix the windows \n problem
	g_free(locale_filename);
	if (fp == NULL)
	{
		msgwin_status_add(_("Error saving file (%s)."), strerror(errno));
		utils_beep();
		g_free(data);
		return FALSE;
	}
	bytes_written = fwrite(data, sizeof (gchar), len, fp);
	fclose (fp);

	g_free(data);

	if (len != bytes_written)
	{
		msgwin_status_add(_("Error saving file."));
		utils_beep();
		return FALSE;
	}

	// store the opened encoding for undo/redo
	store_saved_encoding(idx);

	// ignore the following things if we are quitting
	if (! app->quitting)
	{
		gchar *basename = g_path_get_basename(doc_list[idx].file_name);

		// set line numbers again, to reset the margin width, if
		// there are more lines than before
		sci_set_line_numbers(doc_list[idx].sci, app->show_linenumber_margin, 0);
		sci_set_savepoint(doc_list[idx].sci);

		/* stat the file to get the timestamp, otherwise on Windows the actual
		 * timestamp can be ahead of time(NULL) */
		document_update_timestamp(idx);

		if (doc_list[idx].file_type == NULL || doc_list[idx].file_type->id == GEANY_FILETYPES_ALL)
		{
			doc_list[idx].file_type = filetypes_get_from_filename(idx);
			filetypes_select_radio_item(doc_list[idx].file_type);
		}
		document_set_filetype(idx, doc_list[idx].file_type);
		tm_workspace_update(TM_WORK_OBJECT(app->tm_workspace), TRUE, TRUE, FALSE);
		gtk_label_set_text(GTK_LABEL(doc_list[idx].tab_label), basename);
		gtk_label_set_text(GTK_LABEL(doc_list[idx].tabmenu_label), basename);
		msgwin_status_add(_("File %s saved."), doc_list[idx].file_name);
		ui_update_statusbar(idx, -1);
		g_free(basename);
#ifdef HAVE_VTE
		vte_cwd(doc_list[idx].file_name, FALSE);
#endif

	}
	return TRUE;
}


/* special search function, used from the find entry in the toolbar */
void document_search_bar_find(gint idx, const gchar *text, gint flags, gboolean inc)
{
	gint start_pos, search_pos;
	struct TextToFind ttf;

	g_return_if_fail(text != NULL);
	if (! DOC_IDX_VALID(idx) || ! *text) return;

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
		sci_set_selection_start(doc_list[idx].sci, ttf.chrgText.cpMin);
		sci_set_selection_end(doc_list[idx].sci, ttf.chrgText.cpMax);
		doc_list[idx].scroll_percent = 0.3F;
	}
	else
	{
		if (! inc)
		{
			ui_set_statusbar(_("\"%s\" was not found."), text);
		}
		utils_beep();
		sci_goto_pos(doc_list[idx].sci, start_pos, FALSE);	// clear selection
	}
}


/* General search function, used from the find dialog.
 * Returns -1 on failure or the start position of the matching text.
 * Will skip past any selection, ignoring it. */
gint document_find_text(gint idx, const gchar *text, gint flags, gboolean search_backwards,
		gboolean scroll)
{
	gint selection_end, selection_start, search_pos, first_visible_line;

	g_return_val_if_fail(text != NULL, -1);
	if (idx == -1 || ! *text) return -1;
	// Sci doesn't support searching backwards with a regex
	if (flags & SCFIND_REGEXP) search_backwards = FALSE;

	first_visible_line = sci_get_first_visible_line(doc_list[idx].sci);

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
			ui_set_statusbar(_("\"%s\" was not found."), text);
			utils_beep();
			return -1;
		}

		// we searched only part of the document, so ask whether to wraparound.
		if (app->pref_main_suppress_search_dialogs ||
			dialogs_show_question_full(GTK_STOCK_FIND, GTK_STOCK_CANCEL,
				_("Wrap search and find again?"), _("\"%s\" was not found."), text))
		{
			gint ret;

			sci_goto_pos(doc_list[idx].sci, (search_backwards) ? sci_len : 0, TRUE);
			ret = document_find_text(idx, text, flags, search_backwards, scroll);
			if (ret == -1)
			{	// return to original cursor position if not found
				gint new_visible_line;

				sci_goto_pos(doc_list[idx].sci, selection_start, FALSE);
				new_visible_line = sci_get_first_visible_line(doc_list[idx].sci);
				sci_scroll_lines(doc_list[idx].sci, first_visible_line - new_visible_line);
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
		document_find_text(idx, find_text, flags, search_backwards, TRUE);
		return -1;
	}
	// there's a selection so go to the start before finding to search through it
	// this ensures there is a match
	if (search_backwards)
		sci_goto_pos(doc_list[idx].sci, selection_end, TRUE);
	else
		sci_goto_pos(doc_list[idx].sci, selection_start, TRUE);

	search_pos = document_find_text(idx, find_text, flags, search_backwards, TRUE);
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
		ui_set_statusbar("%s", _("No matches found."));
		return;
	}

	filename = g_path_get_basename(DOC_FILENAME(idx));

	if (escaped_chars)
	{	// escape special characters for showing
		escaped_find_text = g_strescape(find_text, NULL);
		escaped_replace_text = g_strescape(replace_text, NULL);
		msgwin_status_add(_("%s: replaced %d occurrences of \"%s\" with \"%s\"."),
						filename, count, escaped_find_text, escaped_replace_text);
		g_free(escaped_find_text);
		g_free(escaped_replace_text);
	}
	else
	{
		msgwin_status_add(_("%s: replaced %d occurrences of \"%s\" with \"%s\"."),
						filename, count, find_text, replace_text);
	}
	g_free(filename);
}


/* Returns -1 if no text found or the new range endpoint after replacing. */
static gint
document_replace_range(gint idx, const gchar *find_text, const gchar *replace_text,
	gint flags, gint start, gint end, gboolean escaped_chars)
{
	gint search_pos;
	gint count = 0;
	gint find_len = 0, replace_len = 0;
	gboolean match_found = FALSE;
	struct TextToFind ttf;

	g_return_val_if_fail(find_text != NULL && replace_text != NULL, FALSE);
	if (idx == -1 || ! *find_text) return FALSE;

	sci_start_undo_action(doc_list[idx].sci);
	ttf.chrg.cpMin = start;
	ttf.chrg.cpMax = end;
	ttf.lpstrText = (gchar*)find_text;

	while (TRUE)
	{
		search_pos = sci_find_text(doc_list[idx].sci, flags, &ttf);
		if (search_pos == -1) break;
		find_len = ttf.chrgText.cpMax - ttf.chrgText.cpMin;

		if (search_pos + find_len > end)
			break; //found text is partly out of range
		else
		{
			match_found = TRUE;
			sci_target_start(doc_list[idx].sci, search_pos);
			sci_target_end(doc_list[idx].sci, search_pos + find_len);
			replace_len = sci_target_replace(doc_list[idx].sci, replace_text,
				flags & SCFIND_REGEXP);
			ttf.chrg.cpMin = search_pos + replace_len; //next search starts after replacement
			end += replace_len - find_len; //update end of range now text has changed
			ttf.chrg.cpMax = end;
			count++;
		}
	}
	sci_end_undo_action(doc_list[idx].sci);

	show_replace_summary(idx, count, find_text, replace_text, escaped_chars);

	if (match_found)
	{
		// scroll last match in view.
		sci_goto_pos(doc_list[idx].sci, ttf.chrg.cpMin, TRUE);
		return end;
	}
	else
		return -1; // no text was found
}


void document_replace_sel(gint idx, const gchar *find_text, const gchar *replace_text, gint flags,
						  gboolean escaped_chars)
{
	gint selection_end, selection_start;

	g_return_if_fail(find_text != NULL && replace_text != NULL);
	if (idx == -1 || ! *find_text) return;

	selection_start = sci_get_selection_start(doc_list[idx].sci);
	selection_end = sci_get_selection_end(doc_list[idx].sci);
	if ((selection_end - selection_start) == 0)
	{
		utils_beep();
		return;
	}

	selection_end = document_replace_range(idx, find_text, replace_text, flags,
		selection_start, selection_end, escaped_chars);
	if (selection_end == -1)
	{
		// no replacements
		utils_beep();
	}
	else
	{
		// update the selection for the new endpoint
		sci_set_selection_start(doc_list[idx].sci, selection_start);
		sci_set_selection_end(doc_list[idx].sci, selection_end);
	}
}


// returns TRUE if at least one replacement was made.
gboolean document_replace_all(gint idx, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean escaped_chars)
{
	gint len;
	g_return_val_if_fail(find_text != NULL && replace_text != NULL, FALSE);
	if (idx == -1 || ! *find_text) return FALSE;

	len = sci_get_length(doc_list[idx].sci);
	if (document_replace_range(idx, find_text, replace_text, flags, 0, len, escaped_chars) == -1)
	{
		utils_beep();
		return FALSE;
	}
	return TRUE;
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
	// if the filetype doesn't have a tag parser or it is a new file
	if (idx == -1 || doc_list[idx].file_type == NULL ||
		! doc_list[idx].file_type->has_tags || ! doc_list[idx].file_name)
	{
		// set the default (empty) tag list
		ui_update_tag_list(idx, FALSE);
		return;
	}

	if (doc_list[idx].tm_file == NULL)
	{
		gchar *locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);
		doc_list[idx].tm_file = tm_source_file_new(locale_filename, FALSE,
												   doc_list[idx].file_type->name);
		g_free(locale_filename);
		if (! doc_list[idx].tm_file) return;
		tm_workspace_add_object(doc_list[idx].tm_file);
		if (update)
			tm_source_file_update(doc_list[idx].tm_file, TRUE, FALSE, TRUE);
		ui_update_tag_list(idx, TRUE);
	}
	else
	{
		if (tm_source_file_update(doc_list[idx].tm_file, TRUE, FALSE, TRUE))
		{
			ui_update_tag_list(idx, TRUE);
		}
		else
		{
			geany_debug("tag list updating failed");
		}
	}
}


static GString *get_project_typenames()
{
	GString *s = NULL;

	if (app->tm_workspace)
	{
		GPtrArray *tags_array = app->tm_workspace->work_object.tags_array;

		if (tags_array)
		{
			s = symbols_find_tags_as_string(tags_array, TM_GLOBAL_TYPE_MASK);
		}
	}
	return s;
}


/* Returns: whether sci_colourise has been called for sci */
static gboolean update_type_keywords(ScintillaObject *sci)
{
	gboolean ret = FALSE;

	if (sci_cb_lexer_get_type_keyword_idx(sci_get_lexer(sci)) != -1)
	{
		guint n;
		GString *s = get_project_typenames();

		if (s == NULL) return FALSE;

		for (n = 0; n < doc_array->len; n++)
		{
			ScintillaObject *wid = doc_list[n].sci;

			if (wid)
			{
				gint keyword_idx = sci_cb_lexer_get_type_keyword_idx(sci_get_lexer(wid));

				if (keyword_idx > 0)
				{
					sci_set_keywords(wid, keyword_idx, s->str);
					sci_colourise(wid, 0, -1);
					if (sci == wid)
						ret = TRUE;
				}
			}
		}
		g_string_free(s, TRUE);
	}
	return ret;
}


/* sets the filetype of the document (sets syntax highlighting and tagging) */
void document_set_filetype(gint idx, filetype *type)
{
	gboolean colourise = FALSE;

	if (type == NULL || ! DOC_IDX_VALID(idx))
		return;

	geany_debug("%s : %s (%s)",	doc_list[idx].file_name, type->name, doc_list[idx].encoding);

	if (doc_list[idx].file_type != type)	// filetype has changed
	{
		doc_list[idx].file_type = type;

		// delete tm file object to force creation of a new one
		if (doc_list[idx].tm_file != NULL)
		{
			tm_workspace_remove_object(doc_list[idx].tm_file, TRUE);
			doc_list[idx].tm_file = NULL;
		}
		type->style_func_ptr(doc_list[idx].sci);	// set new styles
		build_menu_update(idx);
		colourise = TRUE;
	}

	document_update_tag_list(idx, TRUE);
	colourise &= ! update_type_keywords(doc_list[idx].sci);
	if (colourise)
		sci_colourise(doc_list[idx].sci, 0, -1);
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


void document_unfold_all(gint idx)
{
	gint lines, pos, i;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	lines = sci_get_line_count(doc_list[idx].sci);
	pos = sci_get_current_position(doc_list[idx].sci);

	for (i = 0; i < lines; i++)
	{
		sci_ensure_line_is_visible(doc_list[idx].sci, i);
	}
}


void document_fold_all(gint idx)
{
	gint lines, pos, i;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	lines = sci_get_line_count(doc_list[idx].sci);
	pos = sci_get_current_position(doc_list[idx].sci);

	for (i = 0; i < lines; i++)
	{
		gint level = sci_get_fold_level(doc_list[idx].sci, i);
		if (level & SC_FOLDLEVELHEADERFLAG)
		{
			if (sci_get_fold_expanded(doc_list[idx].sci, i))
					sci_toggle_fold(doc_list[idx].sci, i);
		}
	}
}


void document_clear_indicators(gint idx)
{
	glong last_pos = sci_get_length(doc_list[idx].sci);
	if (last_pos > 0)
	{
		sci_start_styling(doc_list[idx].sci, 0, INDIC2_MASK);
		sci_set_styling(doc_list[idx].sci, last_pos, 0);
	}
	// delete the yellow marker if still set
	if (! app->show_markers_margin) sci_marker_delete_all(doc_list[idx].sci, 0);
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


/* simple file print */
void document_print(gint idx)
{
	gchar *cmdline;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_name == NULL) return;

	cmdline = g_strdup(app->tools_print_cmd);
	cmdline = utils_str_replace(cmdline, "%f", doc_list[idx].file_name);

	if (dialogs_show_question(_("The file \"%s\" will be printed with the following command:\n\n%s"),
								doc_list[idx].file_name, cmdline))
	{
		gint rc;
		// system() is not the best way, but the only one I found to get the following working:
		// a2ps -1 --medium=A4 -o - %f | xfprint4
		rc = system(cmdline);
		if (rc != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Printing of \"%s\" failed (return code: %d)."),
								doc_list[idx].file_name, rc);
		}
		else
		{
			msgwin_status_add(_("File %s printed."), doc_list[idx].file_name);
		}
	}
	g_free(cmdline);
}


void document_replace_tabs(gint idx)
{
	gint search_pos;
	gint tab_len;
	gchar *tab_str;
	struct TextToFind ttf;

	if (! DOC_IDX_VALID(idx)) return;

	sci_start_undo_action(doc_list[idx].sci);
	tab_len = sci_get_tab_width(doc_list[idx].sci);
	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(doc_list[idx].sci);
	ttf.lpstrText = (gchar*)"\t";
	tab_str = g_strnfill(tab_len, ' ');

	while (TRUE)
	{
		search_pos = sci_find_text(doc_list[idx].sci, SCFIND_MATCHCASE, &ttf);
		if (search_pos == -1) break;

		sci_target_start(doc_list[idx].sci, search_pos);
		sci_target_end(doc_list[idx].sci, search_pos + 1);
		sci_target_replace(doc_list[idx].sci, tab_str, FALSE);
		ttf.chrg.cpMin = search_pos + tab_len - 1;	// next search starts after replacement
		ttf.chrg.cpMax += tab_len - 1;	// update end of range now text has changed
	}
	sci_end_undo_action(doc_list[idx].sci);
	g_free(tab_str);
}


void document_strip_trailing_spaces(gint idx)
{
	gint max_lines = sci_get_line_count(doc_list[idx].sci);
	gint line;

	for (line = 0; line < max_lines; line++)
	{
		gint line_start = sci_get_position_from_line(doc_list[idx].sci, line);
		gint line_end = sci_get_line_end_from_position(doc_list[idx].sci, line);
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
	if (! app->quitting) document_set_text_changed(idx);

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
