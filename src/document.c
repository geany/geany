/*
 *      document.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 */

/*
 *  Document related actions: new, save, open, etc.
 *  Also Scintilla search actions.
 */

#include "geany.h"

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <stdlib.h>

/* gstdio.h also includes sys/stat.h */
#include <glib/gstdio.h>

/* uncomment to use GIO based file monitoring, though it is not completely stable yet */
/*#define USE_GIO_FILEMON 1*/
#include <gio/gio.h>

#include "document.h"
#include "documentprivate.h"
#include "filetypes.h"
#include "support.h"
#include "sciwrappers.h"
#include "editor.h"
#include "dialogs.h"
#include "msgwindow.h"
#include "templates.h"
#include "sidebar.h"
#include "ui_utils.h"
#include "utils.h"
#include "encodings.h"
#include "notebook.h"
#include "main.h"
#include "vte.h"
#include "build.h"
#include "symbols.h"
#include "highlighting.h"
#include "navqueue.h"
#include "win32.h"
#include "search.h"
#include "filetypesprivate.h"

#include "SciLexer.h"


GeanyFilePrefs file_prefs;

/** Dynamic array of GeanyDocument pointers holding information about the notebook tabs.
 * Once a pointer is added to this, it is never freed. This means you can keep a pointer
 * to a document over time, but it might no longer represent a notebook tab. To check this,
 * check @c doc_ptr->is_valid. Of course, the pointer may represent a different
 * file by then.
 *
 * You also need to check @c GeanyDocument::is_valid when iterating over this array,
 * although usually you would just use the foreach_document() macro.
 *
 * Never assume that the order of document pointers is the same as the order of notebook tabs.
 * Notebook tabs can be reordered. Use @c document_get_from_page(). */
GPtrArray *documents_array = NULL;


/* an undo action, also used for redo actions */
typedef struct
{
	GTrashStack *next;	/* pointer to the next stack element(required for the GTrashStack) */
	guint type;			/* to identify the action */
	gpointer *data; 	/* the old value (before the change), in case of a redo action
						 * it contains the new value */
} undo_action;


static void document_undo_clear(GeanyDocument *doc);
static void document_redo_add(GeanyDocument *doc, guint type, gpointer data);
static gboolean remove_page(guint page_num);


/**
 * Finds a document whose @c real_path field matches the given filename.
 *
 * @param realname The filename to search, which should be identical to the
 * string returned by @c tm_get_real_path().
 *
 * @return The matching document, or @c NULL.
 * @note This is only really useful when passing a @c TMWorkObject::file_name.
 * @see GeanyDocument::real_path.
 * @see document_find_by_filename().
 *
 * @since 0.15
 **/
GeanyDocument* document_find_by_real_path(const gchar *realname)
{
	guint i;

	if (! realname)
		return NULL;	/* file doesn't exist on disk */

	for (i = 0; i < documents_array->len; i++)
	{
		GeanyDocument *doc = documents[i];

		if (! doc->is_valid || ! doc->real_path)
			continue;

		if (utils_filenamecmp(realname, doc->real_path) == 0)
		{
			return doc;
		}
	}
	return NULL;
}


/* dereference symlinks, /../ junk in path and return locale encoding */
static gchar *get_real_path_from_utf8(const gchar *utf8_filename)
{
	gchar *locale_name = utils_get_locale_from_utf8(utf8_filename);
	gchar *realname = tm_get_real_path(locale_name);

	g_free(locale_name);
	return realname;
}


/**
 *  Finds a document with the given filename.
 *  This matches either an exact GeanyDocument::file_name string, or variant
 *  filenames with relative elements in the path (e.g. @c "/dir/..//name" will
 *  match @c "/name").
 *
 *  @param utf8_filename The filename to search (in UTF-8 encoding).
 *
 *  @return The matching document, or @c NULL.
 *  @see document_find_by_real_path().
 **/
GeanyDocument *document_find_by_filename(const gchar *utf8_filename)
{
	guint i;
	GeanyDocument *doc;
	gchar *realname;

	g_return_val_if_fail(utf8_filename != NULL, NULL);

	/* First search GeanyDocument::file_name, so we can find documents with a
	 * filename set but not saved on disk, like vcdiff produces */
	for (i = 0; i < documents_array->len; i++)
	{
		doc = documents[i];

		if (! doc->is_valid || doc->file_name == NULL)
			continue;

		if (utils_filenamecmp(utf8_filename, doc->file_name) == 0)
		{
			return doc;
		}
	}
	/* Now try matching based on the realpath(), which is unique per file on disk */
	realname = get_real_path_from_utf8(utf8_filename);
	doc = document_find_by_real_path(realname);
	g_free(realname);
	return doc;
}


/* returns the document which has sci, or NULL. */
GeanyDocument *document_find_by_sci(ScintillaObject *sci)
{
	guint i;

	g_return_val_if_fail(sci != NULL, NULL);

	for (i = 0; i < documents_array->len; i++)
	{
		if (documents[i]->is_valid && documents[i]->editor->sci == sci)
			return documents[i];
	}
	return NULL;
}


/** Gets the notebook page index for a document.
 * @param doc The document.
 * @return The index.
 * @since 0.19 */
gint document_get_notebook_page(GeanyDocument *doc)
{
	g_return_val_if_fail(doc != NULL, -1);

	return gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook),
		GTK_WIDGET(doc->editor->sci));
}


/**
 *  Finds the document for the given notebook page @a page_num.
 *
 *  @param page_num The notebook page number to search.
 *
 *  @return The corresponding document for the given notebook page, or @c NULL.
 **/
GeanyDocument *document_get_from_page(guint page_num)
{
	ScintillaObject *sci;

	if (page_num >= documents_array->len)
		return NULL;

	sci = (ScintillaObject*)gtk_notebook_get_nth_page(
				GTK_NOTEBOOK(main_widgets.notebook), page_num);

	return document_find_by_sci(sci);
}


/**
 *  Finds the current document.
 *
 *  @return A pointer to the current document or @c NULL if there are no opened documents.
 **/
GeanyDocument *document_get_current(void)
{
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.notebook));

	if (cur_page == -1)
		return NULL;
	else
	{
		ScintillaObject *sci = (ScintillaObject*)
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_widgets.notebook), cur_page);

		return document_find_by_sci(sci);
	}
}


void document_init_doclist()
{
	documents_array = g_ptr_array_new();
}


void document_finalize()
{
	guint i;

	for (i = 0; i < documents_array->len; i++)
		g_free(documents[i]);
	g_ptr_array_free(documents_array, TRUE);
}


/**
 *  Returns the last part of the filename of the given GeanyDocument. The result is also
 *  truncated to a maximum of @a length characters in case the filename is very long.
 *
 *  @param doc The document to use.
 *  @param length The length of the resulting string or -1 to use a default value.
 *
 *  @return The ellipsized last part of the filename of @a doc, should be freed when no
 *          longer needed.
 *
 *  @since 0.17
 */
/* TODO make more use of this */
gchar *document_get_basename_for_display(GeanyDocument *doc, gint length)
{
	gchar *base_name, *short_name;

	g_return_val_if_fail(doc != NULL, NULL);

	if (length < 0)
		length = 30;

	base_name = g_path_get_basename(DOC_FILENAME(doc));
	short_name = utils_str_middle_truncate(base_name, (guint)length);

	g_free(base_name);

	return short_name;
}


void document_update_tab_label(GeanyDocument *doc)
{
	gchar *short_name;
	GtkWidget *parent;

	g_return_if_fail(doc != NULL);

	short_name = document_get_basename_for_display(doc, -1);

	/* we need to use the event box for the tooltip, labels don't get the necessary events */
	parent = gtk_widget_get_parent(doc->priv->tab_label);
	parent = gtk_widget_get_parent(parent);

	gtk_label_set_text(GTK_LABEL(doc->priv->tab_label), short_name);

	gtk_widget_set_tooltip_text(parent, DOC_FILENAME(doc));

	g_free(short_name);
}


/**
 *  Updates the tab labels, the status bar, the window title and some save-sensitive buttons
 *  according to the document's save state.
 *  This is called by Geany mostly when opening or saving files.
 *
 * @param doc The document to use.
 * @param changed Whether the document state should indicate changes have been made.
 **/
void document_set_text_changed(GeanyDocument *doc, gboolean changed)
{
	g_return_if_fail(doc != NULL);

	doc->changed = changed;

	if (! main_status.quitting)
	{
		ui_update_tab_status(doc);
		ui_save_buttons_toggle(changed);
		ui_set_window_title(doc);
		ui_update_statusbar(doc, -1);
	}
}


/* returns the next free place in the document list,
 * or -1 if the documents_array is full */
static gint document_get_new_idx(void)
{
	guint i;

	for (i = 0; i < documents_array->len; i++)
	{
		if (documents[i]->editor == NULL)
		{
			return (gint) i;
		}
	}
	return -1;
}


static void queue_colourise(GeanyDocument *doc)
{
	/* Colourise the editor before it is next drawn */
	doc->priv->colourise_needed = TRUE;

	/* If the editor doesn't need drawing (e.g. after saving the current
	 * document), we need to force a redraw, so the expose event is triggered.
	 * This ensures we don't start colourising before all documents are opened/saved,
	 * only once the editor is drawn. */
	gtk_widget_queue_draw(GTK_WIDGET(doc->editor->sci));
}


#ifdef USE_GIO_FILEMON
static void monitor_file_changed_cb(G_GNUC_UNUSED GFileMonitor *monitor, G_GNUC_UNUSED GFile *file,
									G_GNUC_UNUSED GFile *other_file, GFileMonitorEvent event,
									GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	if (file_prefs.disk_check_timeout == 0)
		return;

	geany_debug("%s: event: %d previous file status: %d",
		G_STRFUNC, event, doc->priv->file_disk_status);
	switch (event)
	{
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		{
			if (doc->priv->file_disk_status == FILE_IGNORE)
				doc->priv->file_disk_status = FILE_OK;
			else
				doc->priv->file_disk_status = FILE_CHANGED;
			g_message("%s: FILE_CHANGED", G_STRFUNC);
			break;
		}
		case G_FILE_MONITOR_EVENT_DELETED:
		{
			doc->priv->file_disk_status = FILE_CHANGED;
			g_message("%s: FILE_MISSING", G_STRFUNC);
			break;
		}
		default:
			break;
	}
	if (doc->priv->file_disk_status != FILE_OK)
	{
		ui_update_tab_status(doc);
	}
}
#endif


static void document_stop_file_monitoring(GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	if (doc->priv->monitor != NULL)
	{
		g_object_unref(doc->priv->monitor);
		doc->priv->monitor = NULL;
	}
}


static void monitor_file_setup(GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);
	/* Disable file monitoring completely for remote files (i.e. remote GIO files) as GFileMonitor
	 * doesn't work at all for remote files and legacy polling is too slow. */
	if (! doc->priv->is_remote)
	{
#ifdef USE_GIO_FILEMON
		gchar *locale_filename;

		/* stop any previous monitoring */
		document_stop_file_monitoring(doc);

		locale_filename = utils_get_locale_from_utf8(doc->file_name);
		if (locale_filename != NULL && g_file_test(locale_filename, G_FILE_TEST_EXISTS))
		{
			/* get a file monitor and connect to the 'changed' signal */
			GFile *file = g_file_new_for_path(locale_filename);
			doc->priv->monitor = g_file_monitor_file(file, G_FILE_MONITOR_NONE, NULL, NULL);
			g_signal_connect(doc->priv->monitor, "changed",
				G_CALLBACK(monitor_file_changed_cb), doc);

			/* we set the rate limit according to the GUI pref but it's most probably not used */
			g_file_monitor_set_rate_limit(doc->priv->monitor, file_prefs.disk_check_timeout * 1000);

			g_object_unref(file);
		}
		g_free(locale_filename);
#endif
	}
	doc->priv->file_disk_status = FILE_OK;
}


void document_try_focus(GeanyDocument *doc, GtkWidget *source_widget)
{
	/* doc might not be valid e.g. if user closed a tab whilst Geany is opening files */
	if (DOC_VALID(doc))
	{
		GtkWidget *sci = GTK_WIDGET(doc->editor->sci);
		GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

		if (source_widget == NULL)
			source_widget = doc->priv->tag_tree;

		if (focusw == source_widget)
			gtk_widget_grab_focus(sci);
	}
}


static gboolean on_idle_focus(gpointer doc)
{
	document_try_focus(doc, NULL);
	return FALSE;
}


/* Creates a new document and editor, adding a tab in the notebook.
 * @return The created document */
static GeanyDocument *document_create(const gchar *utf8_filename)
{
	GeanyDocument *doc;
	gint new_idx;
	gint cur_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));

	if (cur_pages == 1)
	{
		doc = document_get_current();
		/* remove the empty document first */
		if (doc != NULL && doc->file_name == NULL && ! doc->changed)
			/* prevent immediately opening another new doc with
			 * new_document_after_close pref */
			remove_page(0);
	}

	new_idx = document_get_new_idx();
	if (new_idx == -1)	/* expand the array, no free places */
	{
		doc = g_new0(GeanyDocument, 1);

		new_idx = documents_array->len;
		g_ptr_array_add(documents_array, doc);
	}
	
	doc = documents[new_idx];
	
	/* initialize default document settings */
	doc->priv = g_new0(GeanyDocumentPrivate, 1);
	doc->index = new_idx;
	doc->file_name = g_strdup(utf8_filename);
	doc->editor = editor_create(doc);
#ifndef USE_GIO_FILEMON
	doc->priv->last_check = time(NULL);
#endif

	sidebar_openfiles_add(doc);	/* sets doc->iter */

	notebook_new_tab(doc);

	/* select document in sidebar */
	{
		GtkTreeSelection *sel;

		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
		gtk_tree_selection_select_iter(sel, &doc->priv->iter);
	}

	ui_document_buttons_update();

	doc->is_valid = TRUE;	/* do this last to prevent UI updating with NULL items. */
	return doc;
}


/**
 *  Closes the given document.
 *
 *  @param doc The document to remove.
 *
 *  @return @c TRUE if the document was actually removed or @c FALSE otherwise.
 *
 * @since 0.15
 **/
gboolean document_close(GeanyDocument *doc)
{
	g_return_val_if_fail(doc, FALSE);

	return document_remove_page(document_get_notebook_page(doc));
}


/* Call document_remove_page() instead, this is only needed for document_create()
 * to prevent re-opening a new document when the last document is closed (if enabled). */
static gboolean remove_page(guint page_num)
{
	GeanyDocument *doc = document_get_from_page(page_num);

	g_return_val_if_fail(doc != NULL, FALSE);

	if (doc->changed && ! dialogs_show_unsaved_file(doc))
		return FALSE;

	/* tell any plugins that the document is about to be closed */
	g_signal_emit_by_name(geany_object, "document-close", doc);

	/* Checking real_path makes it likely the file exists on disk */
	if (! main_status.closing_all && doc->real_path != NULL)
		ui_add_recent_document(doc);

	doc->is_valid = FALSE;

	if (! main_status.quitting)
	{
		notebook_remove_page(page_num);
		sidebar_remove_document(doc);
		navqueue_remove_file(doc->file_name);
		msgwin_status_add(_("File %s closed."), DOC_FILENAME(doc));
	}
	g_free(doc->encoding);
	g_free(doc->priv->saved_encoding.encoding);
	g_free(doc->file_name);
	g_free(doc->real_path);
	tm_workspace_remove_object(doc->tm_file, TRUE, TRUE);

	editor_destroy(doc->editor);
	doc->editor = NULL; /* needs to be NULL for document_undo_clear() call below */

	document_stop_file_monitoring(doc);

	document_undo_clear(doc);

	g_free(doc->priv);

	/* reset document settings to defaults for re-use */
	memset(doc, 0, sizeof(GeanyDocument));

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) == 0)
	{
		sidebar_update_tag_list(NULL, FALSE);
		ui_set_window_title(NULL);
		ui_save_buttons_toggle(FALSE);
		ui_update_popup_reundo_items(NULL);
		ui_document_buttons_update();
		build_menu_update(NULL);
	}
	return TRUE;
}


/**
 *  Removes the given notebook tab at @a page_num and clears all related information
 *  in the document list.
 *
 *  @param page_num The notebook page number to remove.
 *
 *  @return @c TRUE if the document was actually removed or @c FALSE otherwise.
 **/
gboolean document_remove_page(guint page_num)
{
	gboolean done = remove_page(page_num);

	if (done && ui_prefs.new_document_after_close)
		document_new_file_if_non_open();

	return done;
}


/* used to keep a record of the unchanged document state encoding */
static void store_saved_encoding(GeanyDocument *doc)
{
	g_free(doc->priv->saved_encoding.encoding);
	doc->priv->saved_encoding.encoding = g_strdup(doc->encoding);
	doc->priv->saved_encoding.has_bom = doc->has_bom;
}


/* Opens a new empty document only if there are no other documents open */
GeanyDocument *document_new_file_if_non_open(void)
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) == 0)
		return document_new_file(NULL, NULL, NULL);

	return NULL;
}


/**
 *  Creates a new document.
 *  Line endings in @a text will be converted to the default setting.
 *  Afterwards, the @c "document-new" signal is emitted for plugins.
 *
 *  @param utf8_filename The file name in UTF-8 encoding, or @c NULL to open a file as "untitled".
 *  @param ft The filetype to set or @c NULL to detect it from @a filename if not @c NULL.
 *  @param text The initial content of the file (in UTF-8 encoding), or @c NULL.
 *
 *  @return The new document.
 **/
GeanyDocument *document_new_file(const gchar *utf8_filename, GeanyFiletype *ft, const gchar *text)
{
	GeanyDocument *doc;

	if (utf8_filename && g_path_is_absolute(utf8_filename))
	{
		gchar *tmp;
		tmp = utils_strdupa(utf8_filename);	/* work around const */
		utils_tidy_path(tmp);
		utf8_filename = tmp;
	}
	doc = document_create(utf8_filename);

	g_assert(doc != NULL);

	sci_set_undo_collection(doc->editor->sci, FALSE); /* avoid creation of an undo action */
	if (text)
	{
		GString *template = g_string_new(text);
		utils_ensure_same_eol_characters(template, file_prefs.default_eol_character);

		sci_set_text(doc->editor->sci, template->str);
		g_string_free(template, TRUE);
	}
	else
		sci_clear_all(doc->editor->sci);

	sci_set_eol_mode(doc->editor->sci, file_prefs.default_eol_character);

	sci_set_undo_collection(doc->editor->sci, TRUE);
	sci_empty_undo_buffer(doc->editor->sci);

	doc->encoding = g_strdup(encodings[file_prefs.default_new_encoding].charset);
	/* store the opened encoding for undo/redo */
	store_saved_encoding(doc);

	if (ft == NULL && utf8_filename != NULL) /* guess the filetype from the filename if one is given */
		ft = filetypes_detect_from_document(doc);

	document_set_filetype(doc, ft); /* also re-parses tags */

	ui_set_window_title(doc);
	build_menu_update(doc);
	document_set_text_changed(doc, FALSE);
	ui_document_show_hide(doc); /* update the document menu */

	sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin, 0);
	/* bring it in front, jump to the start and grab the focus */
	editor_goto_pos(doc->editor, 0, FALSE);
	document_try_focus(doc, NULL);

#ifdef USE_GIO_FILEMON
	monitor_file_setup(doc);
#else
	doc->priv->mtime = time(NULL);
#endif

	/* "the" SCI signal (connect after initial setup(i.e. adding text)) */
	g_signal_connect(doc->editor->sci, "sci-notify", G_CALLBACK(editor_sci_notify_cb), doc->editor);

	g_signal_emit_by_name(geany_object, "document-new", doc);

	msgwin_status_add(_("New file \"%s\" opened."),
		DOC_FILENAME(doc));

	return doc;
}


/**
 *  Opens a document specified by @a locale_filename.
 *  Afterwards, the @c "document-open" signal is emitted for plugins.
 *
 *  @param locale_filename The filename of the document to load, in locale encoding.
 *  @param readonly Whether to open the document in read-only mode.
 *  @param ft The filetype for the document or @c NULL to auto-detect the filetype.
 *  @param forced_enc The file encoding to use or @c NULL to auto-detect the file encoding.
 *
 *  @return The document opened or @c NULL.
 **/
GeanyDocument *document_open_file(const gchar *locale_filename, gboolean readonly,
		GeanyFiletype *ft, const gchar *forced_enc)
{
	return document_open_file_full(NULL, locale_filename, 0, readonly, ft, forced_enc);
}


typedef struct
{
	gchar		*data;	/* null-terminated file data */
	gsize		 len;	/* string length of data */
	gchar		*enc;
	gboolean	 bom;
	time_t		 mtime;	/* modification time, read by stat::st_mtime */
	gboolean	 readonly;
} FileData;


/* loads textfile data, verifies and converts to forced_enc or UTF-8. Also handles BOM. */
static gboolean load_text_file(const gchar *locale_filename, const gchar *display_filename,
	FileData *filedata, const gchar *forced_enc)
{
	GError *err = NULL;
	struct stat st;

	filedata->data = NULL;
	filedata->len = 0;
	filedata->enc = NULL;
	filedata->bom = FALSE;
	filedata->readonly = FALSE;

	if (g_stat(locale_filename, &st) != 0)
	{
		ui_set_statusbar(TRUE, _("Could not open file %s (%s)"),
			display_filename, g_strerror(errno));
		return FALSE;
	}

	filedata->mtime = st.st_mtime;

	if (! g_file_get_contents(locale_filename, &filedata->data, NULL, &err))
	{
		ui_set_statusbar(TRUE, "%s", err->message);
		g_error_free(err);
		return FALSE;
	}

	filedata->len = (gsize) st.st_size;
	if (! encodings_convert_to_utf8_auto(&filedata->data, &filedata->len, forced_enc,
				&filedata->enc, &filedata->bom, &filedata->readonly))
	{
		if (forced_enc)
		{
			ui_set_statusbar(TRUE, _("The file \"%s\" is not valid %s."),
				display_filename, forced_enc);
		}
		else
		{
			ui_set_statusbar(TRUE,
	_("The file \"%s\" does not look like a text file or the file encoding is not supported."),
			display_filename);
		}
		g_free(filedata->data);
		return FALSE;
	}

	if (filedata->readonly)
	{
		const gchar *warn_msg = _(
			"The file \"%s\" could not be opened properly and has been truncated. " \
			"This can occur if the file contains a NULL byte. " \
			"Be aware that saving it can cause data loss.\nThe file was set to read-only.");

		if (main_status.main_window_realized)
			dialogs_show_msgbox(GTK_MESSAGE_WARNING, warn_msg, display_filename);

		ui_set_statusbar(TRUE, warn_msg, display_filename);
	}

	return TRUE;
}


/* Sets the cursor position on opening a file. First it sets the line when cl_options.goto_line
 * is set, otherwise it sets the line when pos is greater than zero and finally it sets the column
 * if cl_options.goto_column is set.
 *
 * returns the new position which may have changed */
static gint set_cursor_position(GeanyEditor *editor, gint pos)
{
	if (cl_options.goto_line >= 0)
	{	/* goto line which was specified on command line and then undefine the line */
		sci_goto_line(editor->sci, cl_options.goto_line - 1, TRUE);
		editor->scroll_percent = 0.5F;
		cl_options.goto_line = -1;
	}
	else if (pos > 0)
	{
		sci_set_current_position(editor->sci, pos, FALSE);
		editor->scroll_percent = 0.5F;
	}

	if (cl_options.goto_column >= 0)
	{	/* goto column which was specified on command line and then undefine the column */

		gint new_pos = sci_get_current_position(editor->sci) + cl_options.goto_column;
		sci_set_current_position(editor->sci, new_pos, FALSE);
		editor->scroll_percent = 0.5F;
		cl_options.goto_column = -1;
		return new_pos;
	}
	return sci_get_current_position(editor->sci);
}


/* Count lines that start with some hard tabs then a soft tab. */
static gboolean detect_tabs_and_spaces(GeanyEditor *editor)
{
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);
	ScintillaObject *sci = editor->sci;
	gsize count = 0;
	struct Sci_TextToFind ttf;
	gchar *soft_tab = g_strnfill((gsize)iprefs->width, ' ');
	gchar *regex = g_strconcat("^\t+", soft_tab, "[^ ]", NULL);

	g_free(soft_tab);

	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(sci);
	ttf.lpstrText = regex;
	while (1)
	{
		gint pos;

		pos = sci_find_text(sci, SCFIND_REGEXP, &ttf);
		if (pos == -1)
			break;	/* no more matches */
		count++;
		ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;	/* search after this match */
	}
	g_free(regex);
	/* The 0.02 is a low weighting to ignore a few possibly accidental occurrences */
	return count > sci_get_line_count(sci) * 0.02;
}


/* Detect the indent type based on counting the leading indent characters for each line.
 * Returns whether detection succeeded, and the detected type in *type_ upon success */
gboolean document_detect_indent_type(GeanyDocument *doc, GeanyIndentType *type_)
{
	GeanyEditor *editor = doc->editor;
	ScintillaObject *sci = editor->sci;
	gint line, line_count;
	gsize tabs = 0, spaces = 0;

	if (detect_tabs_and_spaces(editor))
	{
		*type_ = GEANY_INDENT_TYPE_BOTH;
		return TRUE;
	}

	line_count = sci_get_line_count(sci);
	for (line = 0; line < line_count; line++)
	{
		gint pos = sci_get_position_from_line(sci, line);
		gchar c;

		/* most code will have indent total <= 24, otherwise it's more likely to be
		 * alignment than indentation */
		if (sci_get_line_indentation(sci, line) > 24)
			continue;

		c = sci_get_char_at(sci, pos);
		if (c == '\t')
			tabs++;
		/* check for at least 2 spaces */
		else if (c == ' ' && sci_get_char_at(sci, pos + 1) == ' ')
			spaces++;
	}
	if (spaces == 0 && tabs == 0)
		return FALSE;

	/* the factors may need to be tweaked */
	if (spaces > tabs * 4)
		*type_ = GEANY_INDENT_TYPE_SPACES;
	else if (tabs > spaces * 4)
		*type_ = GEANY_INDENT_TYPE_TABS;
	else
		*type_ = GEANY_INDENT_TYPE_BOTH;

	return TRUE;
}


/* Detect the indent width based on counting the leading indent characters for each line.
 * Returns whether detection succeeded, and the detected width in *width_ upon success */
static gboolean detect_indent_width(GeanyEditor *editor, GeanyIndentType type, gint *width_)
{
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);
	ScintillaObject *sci = editor->sci;
	gint line, line_count;
	gint widths[7] = { 0 }; /* width can be from 2 to 8 */
	gint count, width, i;

	/* can't easily detect the supposed width of a tab, guess the default is OK */
	if (type == GEANY_INDENT_TYPE_TABS)
		return FALSE;

	/* force 8 at detection time for tab & spaces -- anyway we don't use tabs at this point */
	sci_set_tab_width(sci, 8);

	line_count = sci_get_line_count(sci);
	for (line = 0; line < line_count; line++)
	{
		width = sci_get_line_indentation(sci, line);
		/* most code will have indent total <= 24, otherwise it's more likely to be
		 * alignment than indentation */
		if (width > 24)
			continue;
		/* < 2 is no indentation */
		if (width < 2)
			continue;

		for (i = G_N_ELEMENTS(widths) - 1; i >= 0; i--)
		{
			if ((width % (i + 2)) == 0)
				widths[i]++;
		}
	}
	count = 0;
	width = iprefs->width;
	for (i = G_N_ELEMENTS(widths) - 1; i >= 0; i--)
	{
		/* give large indents higher weight not to be fooled by spurious indents */
		if (widths[i] >= count * 1.5)
		{
			width = i + 2;
			count = widths[i];
		}
	}

	if (count == 0)
		return FALSE;

	*width_ = width;
	return TRUE;
}


/* same as detect_indent_width() but uses editor's indent type */
gboolean document_detect_indent_width(GeanyDocument *doc, gint *width_)
{
	return detect_indent_width(doc->editor, doc->editor->indent_type, width_);
}


void document_apply_indent_settings(GeanyDocument *doc)
{
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(NULL);
	GeanyIndentType type = iprefs->type;
	gint width = iprefs->width;

	if (iprefs->detect_type && document_detect_indent_type(doc, &type))
	{
		if (type != iprefs->type)
		{
			const gchar *name = NULL;

			switch (type)
			{
				case GEANY_INDENT_TYPE_SPACES:
					name = _("Spaces");
					break;
				case GEANY_INDENT_TYPE_TABS:
					name = _("Tabs");
					break;
				case GEANY_INDENT_TYPE_BOTH:
					name = _("Tabs and Spaces");
					break;
			}
			/* For translators: first wildcard is the indentation mode (Spaces, Tabs, Tabs
			 * and Spaces), the second one is the filename */
			ui_set_statusbar(TRUE, _("Setting %s indentation mode for %s."), name,
				DOC_FILENAME(doc));
		}
	}
	else if (doc->file_type->indent_type > -1)
		type = doc->file_type->indent_type;

	if (iprefs->detect_width && detect_indent_width(doc->editor, type, &width))
	{
		if (width != iprefs->width)
		{
			ui_set_statusbar(TRUE, _("Setting indentation width to %d for %s."), width,
				DOC_FILENAME(doc));
		}
	}
	else if (doc->file_type->indent_width > -1)
		width = doc->file_type->indent_width;

	editor_set_indent(doc->editor, type, width);
}


void document_show_tab(GeanyDocument *doc)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook),
		document_get_notebook_page(doc));
}


/* To open a new file, set doc to NULL; filename should be locale encoded.
 * To reload a file, set the doc for the document to be reloaded; filename should be NULL.
 * pos is the cursor position, which can be overridden by --line and --column.
 * forced_enc can be NULL to detect the file encoding.
 * Returns: doc of the opened file or NULL if an error occurred. */
GeanyDocument *document_open_file_full(GeanyDocument *doc, const gchar *filename, gint pos,
		gboolean readonly, GeanyFiletype *ft, const gchar *forced_enc)
{
	gint editor_mode;
	gboolean reload = (doc == NULL) ? FALSE : TRUE;
	gchar *utf8_filename = NULL;
	gchar *display_filename = NULL;
	gchar *locale_filename = NULL;
	GeanyFiletype *use_ft;
	FileData filedata;

	if (reload)
	{
		utf8_filename = g_strdup(doc->file_name);
		locale_filename = utils_get_locale_from_utf8(utf8_filename);
	}
	else
	{
		/* filename must not be NULL when opening a file */
		g_return_val_if_fail(filename, NULL);

#ifdef G_OS_WIN32
		/* if filename is a shortcut, try to resolve it */
		locale_filename = win32_get_shortcut_target(filename);
#else
		locale_filename = g_strdup(filename);
#endif
		/* remove relative junk */
		utils_tidy_path(locale_filename);

		/* try to get the UTF-8 equivalent for the filename, fallback to filename if error */
		utf8_filename = utils_get_utf8_from_locale(locale_filename);

		/* if file is already open, switch to it and go */
		doc = document_find_by_filename(utf8_filename);
		if (doc != NULL)
		{
			ui_add_recent_document(doc);	/* either add or reorder recent item */
			/* show the doc before reload dialog */
			document_show_tab(doc);
			document_check_disk_status(doc, TRUE);	/* force a file changed check */
		}
	}
	if (reload || doc == NULL)
	{	/* doc possibly changed */
		display_filename = utils_str_middle_truncate(utf8_filename, 100);

		if (! load_text_file(locale_filename, display_filename, &filedata, forced_enc))
		{
			g_free(display_filename);
			g_free(utf8_filename);
			g_free(locale_filename);
			return NULL;
		}

		if (! reload)
		{
			doc = document_create(utf8_filename);
			g_return_val_if_fail(doc != NULL, NULL); /* really should not happen */

			/* file exists on disk, set real_path */
			setptr(doc->real_path, tm_get_real_path(locale_filename));

			doc->priv->is_remote = utils_is_remote_path(locale_filename);
			monitor_file_setup(doc);
		}

		sci_set_undo_collection(doc->editor->sci, FALSE); /* avoid creation of an undo action */
		sci_empty_undo_buffer(doc->editor->sci);

		/* add the text to the ScintillaObject */
		sci_set_readonly(doc->editor->sci, FALSE);	/* to allow replacing text */
		sci_set_text(doc->editor->sci, filedata.data);	/* NULL terminated data */
		queue_colourise(doc);	/* Ensure the document gets colourised. */

		/* detect & set line endings */
		editor_mode = utils_get_line_endings(filedata.data, filedata.len);
		sci_set_eol_mode(doc->editor->sci, editor_mode);
		g_free(filedata.data);

		sci_set_undo_collection(doc->editor->sci, TRUE);

		doc->priv->mtime = filedata.mtime; /* get the modification time from file and keep it */
		g_free(doc->encoding);	/* if reloading, free old encoding */
		doc->encoding = filedata.enc;
		doc->has_bom = filedata.bom;
		store_saved_encoding(doc);	/* store the opened encoding for undo/redo */

		doc->readonly = readonly || filedata.readonly;
		sci_set_readonly(doc->editor->sci, doc->readonly);

		/* update line number margin width */
		doc->priv->line_count = sci_get_line_count(doc->editor->sci);
		sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin, 0);

		if (! reload)
		{

			/* "the" SCI signal (connect after initial setup(i.e. adding text)) */
			g_signal_connect(doc->editor->sci, "sci-notify", G_CALLBACK(editor_sci_notify_cb),
				doc->editor);

			use_ft = (ft != NULL) ? ft : filetypes_detect_from_document(doc);
		}
		else
		{	/* reloading */
			document_undo_clear(doc);

			use_ft = ft;
		}
		/* update taglist, typedef keywords and build menu if necessary */
		document_set_filetype(doc, use_ft);

		/* set indentation settings after setting the filetype */
		if (reload)
			editor_set_indent(doc->editor, doc->editor->indent_type, doc->editor->indent_width); /* resetup sci */
		else
			document_apply_indent_settings(doc);

		document_set_text_changed(doc, FALSE);	/* also updates tab state */
		ui_document_show_hide(doc);	/* update the document menu */

		/* finally add current file to recent files menu, but not the files from the last session */
		if (! main_status.opening_session_files)
			ui_add_recent_document(doc);

		if (reload)
		{
			g_signal_emit_by_name(geany_object, "document-reload", doc);
			ui_set_statusbar(TRUE, _("File %s reloaded."), display_filename);
		}
		else
		{
			g_signal_emit_by_name(geany_object, "document-open", doc);
			/* For translators: this is the status window message for opening a file. %d is the number
			 * of the newly opened file, %s indicates whether the file is opened read-only
			 * (it is replaced with the string ", read-only"). */
			msgwin_status_add(_("File %s opened(%d%s)."),
				display_filename, gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)),
				(readonly) ? _(", read-only") : "");
		}
	}

	g_free(display_filename);
	g_free(utf8_filename);
	g_free(locale_filename);

	/* set the cursor position according to pos, cl_options.goto_line and cl_options.goto_column */
	pos = set_cursor_position(doc->editor, pos);
	/* now bring the file in front */
	editor_goto_pos(doc->editor, pos, FALSE);

	/* finally, let the editor widget grab the focus so you can start coding
	 * right away */
	g_idle_add(on_idle_focus, doc);
	return doc;
}


/* Takes a new line separated list of filename URIs and opens each file.
 * length is the length of the string */
void document_open_file_list(const gchar *data, gsize length)
{
	guint i;
	gchar *filename;
	gchar **list;

	g_return_if_fail(data != NULL);

	list = g_strsplit(data, utils_get_eol_char(utils_get_line_endings(data, length)), 0);

	for (i = 0; list[i] != NULL; i++)
	{
		filename = g_filename_from_uri(list[i], NULL, NULL);
		if (G_UNLIKELY(filename == NULL))
			continue;
		document_open_file(filename, FALSE, NULL, NULL);
		g_free(filename);
	}

	g_strfreev(list);
}


/**
 *  Opens each file in the list @a filenames.
 *  Internally, document_open_file() is called for every list item.
 *
 *  @param filenames A list of filenames to load, in locale encoding.
 *  @param readonly Whether to open the document in read-only mode.
 *  @param ft The filetype for the document or @c NULL to auto-detect the filetype.
 *  @param forced_enc The file encoding to use or @c NULL to auto-detect the file encoding.
 **/
void document_open_files(const GSList *filenames, gboolean readonly, GeanyFiletype *ft,
		const gchar *forced_enc)
{
	const GSList *item;

	for (item = filenames; item != NULL; item = g_slist_next(item))
	{
		document_open_file(item->data, readonly, ft, forced_enc);
	}
}


/**
 *  Reloads the document with the specified file encoding
 *  @a forced_enc or @c NULL to auto-detect the file encoding.
 *
 *  @param doc The document to reload.
 *  @param forced_enc The file encoding to use or @c NULL to auto-detect the file encoding.
 *
 *  @return @c TRUE if the document was actually reloaded or @c FALSE otherwise.
 **/
gboolean document_reload_file(GeanyDocument *doc, const gchar *forced_enc)
{
	gint pos = 0;
	GeanyDocument *new_doc;

	g_return_val_if_fail(doc != NULL, FALSE);

	/* try to set the cursor to the position before reloading */
	pos = sci_get_current_position(doc->editor->sci);
	new_doc = document_open_file_full(doc, NULL, pos, doc->readonly, doc->file_type, forced_enc);

	return (new_doc != NULL);
}


static gboolean document_update_timestamp(GeanyDocument *doc, const gchar *locale_filename)
{
#ifndef USE_GIO_FILEMON
	struct stat st;

	g_return_val_if_fail(doc != NULL, FALSE);

	/* stat the file to get the timestamp, otherwise on Windows the actual
	 * timestamp can be ahead of time(NULL) */
	if (g_stat(locale_filename, &st) != 0)
	{
		ui_set_statusbar(TRUE, _("Could not open file %s (%s)"), doc->file_name,
			g_strerror(errno));
		return FALSE;
	}

	doc->priv->mtime = st.st_mtime; /* get the modification time from file and keep it */
#endif
	return TRUE;
}


/* Sets line and column to the given position byte_pos in the document.
 * byte_pos is the position counted in bytes, not characters */
static void get_line_column_from_pos(GeanyDocument *doc, guint byte_pos, gint *line, gint *column)
{
	gint i;
	gint line_start;

	/* for some reason we can use byte count instead of character count here */
	*line = sci_get_line_from_position(doc->editor->sci, byte_pos);
	line_start = sci_get_position_from_line(doc->editor->sci, *line);
	/* get the column in the line */
	*column = byte_pos - line_start;

	/* any non-ASCII characters are encoded with two bytes(UTF-8, always in Scintilla), so
	 * skip one byte(i++) and decrease the column number which is based on byte count */
	for (i = line_start; i < (line_start + *column); i++)
	{
		if (sci_get_char_at(doc->editor->sci, i) < 0)
		{
			(*column)--;
			i++;
		}
	}
}


static void replace_header_filename(GeanyDocument *doc)
{
	gchar *filebase;
	gchar *filename;
	struct Sci_TextToFind ttf;

	g_return_if_fail(doc != NULL);
	g_return_if_fail(doc->file_type != NULL);

	if (doc->file_type->extension)
		filebase = g_strconcat("\\<", GEANY_STRING_UNTITLED, "\\.\\w+", NULL);
	else
		filebase = g_strdup(GEANY_STRING_UNTITLED);

	filename = g_path_get_basename(doc->file_name);

	/* only search the first 3 lines */
	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_position_from_line(doc->editor->sci, 3);
	ttf.lpstrText = filebase;

	if (search_find_text(doc->editor->sci, SCFIND_MATCHCASE | SCFIND_REGEXP, &ttf) != -1)
	{
		sci_set_target_start(doc->editor->sci, ttf.chrgText.cpMin);
		sci_set_target_end(doc->editor->sci, ttf.chrgText.cpMax);
		sci_replace_target(doc->editor->sci, filename, FALSE);
	}
	g_free(filebase);
	g_free(filename);
}


/**
 *  Renames the file in @a doc to @a new_filename. Only the file on disk is actually renamed,
 *  you still have to call @ref document_save_file_as() to change the @a doc object.
 *  It also stops monitoring for file changes to prevent receiving too many file change events
 *  while renaming. File monitoring is setup again in @ref document_save_file_as().
 *
 *  @param doc The current document which should be renamed.
 *  @param new_filename The new filename in UTF-8 encoding.
 *
 *  @since 0.16
 **/
void document_rename_file(GeanyDocument *doc, const gchar *new_filename)
{
	gchar *old_locale_filename = utils_get_locale_from_utf8(doc->file_name);
	gchar *new_locale_filename = utils_get_locale_from_utf8(new_filename);
	gint result;

	/* stop file monitoring to avoid getting events for deleting/creating files,
	 * it's re-setup in document_save_file_as() */
	document_stop_file_monitoring(doc);

	result = g_rename(old_locale_filename, new_locale_filename);
	if (result != 0)
	{
		dialogs_show_msgbox_with_secondary(GTK_MESSAGE_ERROR,
			_("Error renaming file."), g_strerror(errno));
	}
	g_free(old_locale_filename);
	g_free(new_locale_filename);
}


/* Return TRUE if the document doesn't have a full filename set.
 * This makes filenames without a path show the save as dialog, e.g. for file templates.
 * Otherwise just use the set filename instead of asking the user - e.g. for command-line
 * new files. */
gboolean document_need_save_as(GeanyDocument *doc)
{
	g_return_val_if_fail(doc != NULL, FALSE);

	return (doc->file_name == NULL || !g_path_is_absolute(doc->file_name));
}


/**
 * Saves the document, detecting the filetype.
 *
 * @param doc The document for the file to save.
 * @param utf8_fname The new name for the document, in UTF-8, or NULL.
 * @return @c TRUE if the file was saved or @c FALSE if the file could not be saved.
 *
 * @see document_save_file().
 *
 *  @since 0.16
 **/
gboolean document_save_file_as(GeanyDocument *doc, const gchar *utf8_fname)
{
	gboolean ret;

	g_return_val_if_fail(doc != NULL, FALSE);

	if (utf8_fname != NULL)
		setptr(doc->file_name, g_strdup(utf8_fname));

	/* reset real path, it's retrieved again in document_save() */
	setptr(doc->real_path, NULL);

	/* detect filetype */
	if (doc->file_type->id == GEANY_FILETYPES_NONE)
	{
		GeanyFiletype *ft = filetypes_detect_from_document(doc);

		document_set_filetype(doc, ft);
		if (document_get_current() == doc)
		{
			ignore_callback = TRUE;
			filetypes_select_radio_item(doc->file_type);
			ignore_callback = FALSE;
		}
	}
	replace_header_filename(doc);

	ret = document_save_file(doc, TRUE);

	/* file monitoring support, add file monitoring after the file has been saved
	 * to ignore any earlier events */
	monitor_file_setup(doc);
	doc->priv->file_disk_status = FILE_IGNORE;

	if (ret)
		ui_add_recent_document(doc);
	return ret;
}


static gsize save_convert_to_encoding(GeanyDocument *doc, gchar **data, gsize *len)
{
	GError *conv_error = NULL;
	gchar* conv_file_contents = NULL;
	gsize bytes_read;
	gsize conv_len;

	g_return_val_if_fail(data != NULL && *data != NULL, FALSE);
	g_return_val_if_fail(len != NULL, FALSE);

	/* try to convert it from UTF-8 to original encoding */
	conv_file_contents = g_convert(*data, *len - 1, doc->encoding, "UTF-8",
												&bytes_read, &conv_len, &conv_error);

	if (conv_error != NULL)
	{
		gchar *text = g_strdup_printf(
_("An error occurred while converting the file from UTF-8 in \"%s\". The file remains unsaved."),
			doc->encoding);
		gchar *error_text;

		if (conv_error->code == G_CONVERT_ERROR_ILLEGAL_SEQUENCE)
		{
			gchar *context = NULL;
			gint line, column;
			gint context_len;
			gunichar unic;
			/* don't read over the doc length */
			gint max_len = MIN((gint)bytes_read + 6, (gint)*len - 1);
			context = g_malloc(7); /* read 6 bytes from Sci + '\0' */
			sci_get_text_range(doc->editor->sci, bytes_read, max_len, context);

			/* take only one valid Unicode character from the context and discard the leftover */
			unic = g_utf8_get_char_validated(context, -1);
			context_len = g_unichar_to_utf8(unic, context);
			context[context_len] = '\0';
			get_line_column_from_pos(doc, bytes_read, &line, &column);

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
		g_free(text);
		g_free(error_text);
		return FALSE;
	}
	else
	{
		g_free(*data);
		*data = conv_file_contents;
		*len = conv_len;
	}
	return TRUE;
}


static gchar *write_data_to_disk(const gchar *locale_filename,
								 const gchar *data, gint len)
{
	GError *error = NULL;

	if (file_prefs.use_safe_file_saving)
	{
		/* Use old GLib API for safe saving (GVFS-safe, but alters ownership and permissons).
		 * This is the only option that handles disk space exhaustion. */
		if (g_file_set_contents(locale_filename, data, len, &error))
			geany_debug("Wrote %s with g_file_set_contents().", locale_filename);
	}
	else if (file_prefs.use_gio_unsafe_file_saving)
	{
		GFile *fp;

		/* Use GIO API to save file (GVFS-safe)
		 * It is best in most GVFS setups but don't seem to work correctly on some more complex
		 * setups (saving from some VM to their host, over some SMB shares, etc.) */
		fp = g_file_new_for_path(locale_filename);
		g_file_replace_contents(fp, data, len, NULL, file_prefs.gio_unsafe_save_backup,
			G_FILE_CREATE_NONE, NULL, NULL, &error);
		g_object_unref(fp);
	}
	else
	{
		FILE *fp;
		int save_errno;
		gchar *display_name = g_filename_display_name(locale_filename);

		/* Use POSIX API for unsafe saving (GVFS-unsafe) */
		/* The error handling is taken from glib-2.26.0 gfileutils.c */
		errno = 0;
		fp = g_fopen(locale_filename, "wb");
		if (fp == NULL)
		{
			save_errno = errno;

			g_set_error(&error,
				G_FILE_ERROR,
				g_file_error_from_errno(save_errno),
				_("Failed to open file '%s' for writing: fopen() failed: %s"),
				display_name,
				g_strerror(save_errno));
		}
		else
		{
			gint bytes_written;

			errno = 0;
			bytes_written = fwrite(data, sizeof(gchar), len, fp);

			if (len != bytes_written)
			{
				save_errno = errno;

				g_set_error(&error,
					G_FILE_ERROR,
					g_file_error_from_errno(save_errno),
					_("Failed to write file '%s': fwrite() failed: %s"),
					display_name,
					g_strerror(save_errno));
			}

			errno = 0;
			/* preserve the fwrite() error if any */
			if (fclose(fp) != 0 && error == NULL)
			{
				save_errno = errno;

				g_set_error(&error,
					G_FILE_ERROR,
					g_file_error_from_errno(save_errno),
					_("Failed to close file '%s': fclose() failed: %s"),
					display_name,
					g_strerror(save_errno));
			}
		}

		g_free(display_name);
	}
	if (error != NULL)
	{
		gchar *msg = g_strdup(error->message);
		g_error_free(error);
		/* geany will warn about file truncation for unsafe saving below */
		return msg;
	}
	return NULL;
}


static gchar *save_doc(GeanyDocument *doc, const gchar *locale_filename,
								 const gchar *data, gint len)
{
	gchar *err;

	g_return_val_if_fail(doc != NULL, g_strdup(g_strerror(EINVAL)));
	g_return_val_if_fail(data != NULL, g_strdup(g_strerror(EINVAL)));

	err = write_data_to_disk(locale_filename, data, len);
	if (err)
		return err;

	/* now the file is on disk, set real_path */
	if (doc->real_path == NULL)
	{
		doc->real_path = tm_get_real_path(locale_filename);
		doc->priv->is_remote = utils_is_remote_path(locale_filename);
		monitor_file_setup(doc);
	}
	return NULL;
}


/**
 *  Saves the document.
 *  Also shows the Save As dialog if necessary.
 *  If the file is not modified, this function may do nothing unless @a force is set to @c TRUE.
 *
 *  Saving may include replacing tabs by spaces,
 *  stripping trailing spaces and adding a final new line at the end of the file, depending
 *  on user preferences. Then the @c "document-before-save" signal is emitted,
 *  allowing plugins to modify the document before it is saved, and data is
 *  actually written to disk.
 *
 *  On successful saving:
 *  - GeanyDocument::real_path is set.
 *  - The filetype is set again or auto-detected if it wasn't set yet.
 *  - The @c "document-save" signal is emitted for plugins.
 *
 *  @warning You should ensure @c doc->file_name has an absolute path unless you want the
 *  Save As dialog to be shown. A @c NULL value also shows the dialog. This behaviour was
 *  added in Geany 1.22.
 *
 *  @param doc The document to save.
 *  @param force Whether to save the file even if it is not modified.
 *
 *  @return @c TRUE if the file was saved or @c FALSE if the file could not or should not be saved.
 **/
gboolean document_save_file(GeanyDocument *doc, gboolean force)
{
	gchar *errmsg;
	gchar *data;
	gsize len;
	gchar *locale_filename;

	g_return_val_if_fail(doc != NULL, FALSE);

	if (document_need_save_as(doc))
	{
		/* ensure doc is the current tab before showing the dialog */
		document_show_tab(doc);
		return dialogs_show_save_as();
	}

	/* the "changed" flag should exclude the "readonly" flag, but check it anyway for safety */
	if (! force && ! ui_prefs.allow_always_save && (! doc->changed || doc->readonly))
		return FALSE;

	/* replaces tabs by spaces but only if the current file is not a Makefile */
	if (file_prefs.replace_tabs && doc->file_type->id != GEANY_FILETYPES_MAKE)
		editor_replace_tabs(doc->editor);
	/* strip trailing spaces */
	if (file_prefs.strip_trailing_spaces)
		editor_strip_trailing_spaces(doc->editor);
	/* ensure the file has a newline at the end */
	if (file_prefs.final_new_line)
		editor_ensure_final_newline(doc->editor);
	/* ensure newlines are consistent */
	if (file_prefs.ensure_convert_new_lines)
		sci_convert_eols(doc->editor->sci, sci_get_eol_mode(doc->editor->sci));

	/* notify plugins which may wish to modify the document before it's saved */
	g_signal_emit_by_name(geany_object, "document-before-save", doc);

	len = sci_get_length(doc->editor->sci) + 1;
	if (doc->has_bom && encodings_is_unicode_charset(doc->encoding))
	{	/* always write a UTF-8 BOM because in this moment the text itself is still in UTF-8
		 * encoding, it will be converted to doc->encoding below and this conversion
		 * also changes the BOM */
		data = (gchar*) g_malloc(len + 3);	/* 3 chars for BOM */
		data[0] = (gchar) 0xef;
		data[1] = (gchar) 0xbb;
		data[2] = (gchar) 0xbf;
		sci_get_text(doc->editor->sci, len, data + 3);
		len += 3;
	}
	else
	{
		data = (gchar*) g_malloc(len);
		sci_get_text(doc->editor->sci, len, data);
	}

	/* save in original encoding, skip when it is already UTF-8 or has the encoding "None" */
	if (doc->encoding != NULL && ! utils_str_equal(doc->encoding, "UTF-8") &&
		! utils_str_equal(doc->encoding, encodings[GEANY_ENCODING_NONE].charset))
	{
		if (! save_convert_to_encoding(doc, &data, &len))
		{
			g_free(data);
			return FALSE;
		}
	}
	else
	{
		len = strlen(data);
	}

	locale_filename = utils_get_locale_from_utf8(doc->file_name);

	/* ignore file changed notification when the file is written */
	doc->priv->file_disk_status = FILE_IGNORE;

	/* actually write the content of data to the file on disk */
	errmsg = save_doc(doc, locale_filename, data, len);
	g_free(data);

	if (errmsg != NULL)
	{
		ui_set_statusbar(TRUE, _("Error saving file (%s)."), errmsg);

		if (!file_prefs.use_safe_file_saving)
		{
			setptr(errmsg,
				g_strdup_printf(_("%s\n\nThe file on disk may now be truncated!"), errmsg));
		}
		dialogs_show_msgbox_with_secondary(GTK_MESSAGE_ERROR, _("Error saving file."), errmsg);
		doc->priv->file_disk_status = FILE_OK;
		utils_beep();
		g_free(locale_filename);
		g_free(errmsg);
		return FALSE;
	}

	/* store the opened encoding for undo/redo */
	store_saved_encoding(doc);

	/* ignore the following things if we are quitting */
	if (! main_status.quitting)
	{
		sci_set_savepoint(doc->editor->sci);

		if (file_prefs.disk_check_timeout > 0)
			document_update_timestamp(doc, locale_filename);

		/* update filetype-related things */
		document_set_filetype(doc, doc->file_type);

		document_update_tab_label(doc);

		msgwin_status_add(_("File %s saved."), doc->file_name);
		ui_update_statusbar(doc, -1);
#ifdef HAVE_VTE
		vte_cwd((doc->real_path != NULL) ? doc->real_path : doc->file_name, FALSE);
#endif
	}
	g_free(locale_filename);

	g_signal_emit_by_name(geany_object, "document-save", doc);

	return TRUE;
}


/* special search function, used from the find entry in the toolbar
 * return TRUE if text was found otherwise FALSE
 * return also TRUE if text is empty  */
gboolean document_search_bar_find(GeanyDocument *doc, const gchar *text, gint flags, gboolean inc,
		gboolean backwards)
{
	gint start_pos, search_pos;
	struct Sci_TextToFind ttf;

	g_return_val_if_fail(text != NULL, FALSE);
	g_return_val_if_fail(doc != NULL, FALSE);
	if (! *text)
		return TRUE;

	start_pos = (inc || backwards) ? sci_get_selection_start(doc->editor->sci) :
		sci_get_selection_end(doc->editor->sci);	/* equal if no selection */

	/* search cursor to end or start */
	ttf.chrg.cpMin = start_pos;
	ttf.chrg.cpMax = backwards ? 0 : sci_get_length(doc->editor->sci);
	ttf.lpstrText = (gchar *)text;
	search_pos = sci_find_text(doc->editor->sci, flags, &ttf);

	/* if no match, search start (or end) to cursor */
	if (search_pos == -1)
	{
		if (backwards)
		{
			ttf.chrg.cpMin = sci_get_length(doc->editor->sci);
			ttf.chrg.cpMax = start_pos;
		}
		else
		{
			ttf.chrg.cpMin = 0;
			ttf.chrg.cpMax = start_pos + strlen(text);
		}
		search_pos = sci_find_text(doc->editor->sci, flags, &ttf);
	}

	if (search_pos != -1)
	{
		gint line = sci_get_line_from_position(doc->editor->sci, ttf.chrgText.cpMin);

		/* unfold maybe folded results */
		sci_ensure_line_is_visible(doc->editor->sci, line);

		sci_set_selection_start(doc->editor->sci, ttf.chrgText.cpMin);
		sci_set_selection_end(doc->editor->sci, ttf.chrgText.cpMax);

		if (! editor_line_in_view(doc->editor, line))
		{	/* we need to force scrolling in case the cursor is outside of the current visible area
			 * GeanyDocument::scroll_percent doesn't work because sci isn't always updated
			 * while searching */
			editor_scroll_to_line(doc->editor, -1, 0.3F);
		}
		else
			sci_scroll_caret(doc->editor->sci); /* may need horizontal scrolling */
		return TRUE;
	}
	else
	{
		if (! inc)
		{
			ui_set_statusbar(FALSE, _("\"%s\" was not found."), text);
		}
		utils_beep();
		sci_goto_pos(doc->editor->sci, start_pos, FALSE);	/* clear selection */
		return FALSE;
	}
}


/* General search function, used from the find dialog.
 * Returns -1 on failure or the start position of the matching text.
 * Will skip past any selection, ignoring it.
 *
 * @param text Text to find.
 * @param original_text Text as it was entered by user, or @c NULL to use @c text
 */
gint document_find_text(GeanyDocument *doc, const gchar *text, const gchar *original_text,
		gint flags, gboolean search_backwards, gboolean scroll, GtkWidget *parent)
{
	gint selection_end, selection_start, search_pos;

	g_return_val_if_fail(doc != NULL && text != NULL, -1);
	if (! *text)
		return -1;

	/* Sci doesn't support searching backwards with a regex */
	if (flags & SCFIND_REGEXP)
		search_backwards = FALSE;

	if (!original_text)
		original_text = text;

	selection_start = sci_get_selection_start(doc->editor->sci);
	selection_end = sci_get_selection_end(doc->editor->sci);
	if ((selection_end - selection_start) > 0)
	{ /* there's a selection so go to the end */
		if (search_backwards)
			sci_goto_pos(doc->editor->sci, selection_start, TRUE);
		else
			sci_goto_pos(doc->editor->sci, selection_end, TRUE);
	}

	sci_set_search_anchor(doc->editor->sci);
	if (search_backwards)
		search_pos = sci_search_prev(doc->editor->sci, flags, text);
	else
		search_pos = search_find_next(doc->editor->sci, text, flags);

	if (search_pos != -1)
	{
		/* unfold maybe folded results */
		sci_ensure_line_is_visible(doc->editor->sci,
			sci_get_line_from_position(doc->editor->sci, search_pos));
		if (scroll)
			doc->editor->scroll_percent = 0.3F;
	}
	else
	{
		gint sci_len = sci_get_length(doc->editor->sci);

		/* if we just searched the whole text, give up searching. */
		if ((selection_end == 0 && ! search_backwards) ||
			(selection_end == sci_len && search_backwards))
		{
			ui_set_statusbar(FALSE, _("\"%s\" was not found."), original_text);
			utils_beep();
			return -1;
		}

		/* we searched only part of the document, so ask whether to wraparound. */
		if (search_prefs.always_wrap ||
			dialogs_show_question_full(parent, GTK_STOCK_FIND, GTK_STOCK_CANCEL,
				_("Wrap search and find again?"), _("\"%s\" was not found."), original_text))
		{
			gint ret;

			sci_set_current_position(doc->editor->sci, (search_backwards) ? sci_len : 0, FALSE);
			ret = document_find_text(doc, text, original_text, flags, search_backwards, scroll, parent);
			if (ret == -1)
			{	/* return to original cursor position if not found */
				sci_set_current_position(doc->editor->sci, selection_start, FALSE);
			}
			return ret;
		}
	}
	return search_pos;
}


/* Replaces the selection if it matches, otherwise just finds the next match.
 * Returns: start of replaced text, or -1 if no replacement was made
 *
 * @param find_text Text to find.
 * @param original_find_text Text to find as it was entered by user, or @c NULL to use @c find_text
 */
gint document_replace_text(GeanyDocument *doc, const gchar *find_text, const gchar *original_find_text,
		const gchar *replace_text, gint flags, gboolean search_backwards)
{
	gint selection_end, selection_start, search_pos;

	g_return_val_if_fail(doc != NULL && find_text != NULL && replace_text != NULL, -1);

	if (! *find_text)
		return -1;

	/* Sci doesn't support searching backwards with a regex */
	if (flags & SCFIND_REGEXP)
		search_backwards = FALSE;

	if (!original_find_text)
		original_find_text = find_text;

	selection_start = sci_get_selection_start(doc->editor->sci);
	selection_end = sci_get_selection_end(doc->editor->sci);
	if (selection_end == selection_start)
	{
		/* no selection so just find the next match */
		document_find_text(doc, find_text, original_find_text, flags, search_backwards, TRUE, NULL);
		return -1;
	}
	/* there's a selection so go to the start before finding to search through it
	 * this ensures there is a match */
	if (search_backwards)
		sci_goto_pos(doc->editor->sci, selection_end, TRUE);
	else
		sci_goto_pos(doc->editor->sci, selection_start, TRUE);

	search_pos = document_find_text(doc, find_text, original_find_text, flags, search_backwards, TRUE, NULL);
	/* return if the original selected text did not match (at the start of the selection) */
	if (search_pos != selection_start)
		return -1;

	if (search_pos != -1)
	{
		gint replace_len;
		/* search next/prev will select matching text, which we use to set the replace target */
		sci_target_from_selection(doc->editor->sci);
		replace_len = search_replace_target(doc->editor->sci, replace_text, flags & SCFIND_REGEXP);
		/* select the replacement - find text will skip past the selected text */
		sci_set_selection_start(doc->editor->sci, search_pos);
		sci_set_selection_end(doc->editor->sci, search_pos + replace_len);
	}
	else
	{
		/* no match in the selection */
		utils_beep();
	}
	return search_pos;
}


static void show_replace_summary(GeanyDocument *doc, gint count, const gchar *original_find_text,
	const gchar *original_replace_text)
{
	gchar *filename;

	if (count == 0)
	{
		ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), original_find_text);
		return;
	}

	filename = g_path_get_basename(DOC_FILENAME(doc));
	ui_set_statusbar(TRUE, ngettext(
		"%s: replaced %d occurrence of \"%s\" with \"%s\".",
		"%s: replaced %d occurrences of \"%s\" with \"%s\".",
		count), filename, count, original_find_text, original_replace_text);
	g_free(filename);
}


/* Replace all text matches in a certain range within document.
 * If not NULL, *new_range_end is set to the new range endpoint after replacing,
 * or -1 if no text was found.
 * scroll_to_match is whether to scroll the last replacement in view (which also
 * clears the selection).
 * Returns: the number of replacements made. */
static guint
document_replace_range(GeanyDocument *doc, const gchar *find_text, const gchar *replace_text,
	gint flags, gint start, gint end, gboolean scroll_to_match, gint *new_range_end)
{
	gint count = 0;
	struct Sci_TextToFind ttf;
	ScintillaObject *sci;

	if (new_range_end != NULL)
		*new_range_end = -1;

	g_return_val_if_fail(doc != NULL && find_text != NULL && replace_text != NULL, 0);

	if (! *find_text || doc->readonly)
		return 0;

	sci = doc->editor->sci;

	ttf.chrg.cpMin = start;
	ttf.chrg.cpMax = end;
	ttf.lpstrText = (gchar*)find_text;

	sci_start_undo_action(sci);
	count = search_replace_range(sci, &ttf, flags, replace_text);
	sci_end_undo_action(sci);

	if (count > 0)
	{	/* scroll last match in view, will destroy the existing selection */
		if (scroll_to_match)
			sci_goto_pos(sci, ttf.chrg.cpMin, TRUE);

		if (new_range_end != NULL)
			*new_range_end = ttf.chrg.cpMax;
	}
	return count;
}


void document_replace_sel(GeanyDocument *doc, const gchar *find_text, const gchar *replace_text,
						  const gchar *original_find_text, const gchar *original_replace_text, gint flags)
{
	gint selection_end, selection_start, selection_mode, selected_lines, last_line = 0;
	gint max_column = 0, count = 0;
	gboolean replaced = FALSE;

	g_return_if_fail(doc != NULL && find_text != NULL && replace_text != NULL);

	if (! *find_text)
		return;

	selection_start = sci_get_selection_start(doc->editor->sci);
	selection_end = sci_get_selection_end(doc->editor->sci);
	/* do we have a selection? */
	if ((selection_end - selection_start) == 0)
	{
		utils_beep();
		return;
	}

	selection_mode = sci_get_selection_mode(doc->editor->sci);
	selected_lines = sci_get_lines_selected(doc->editor->sci);
	/* handle rectangle, multi line selections (it doesn't matter on a single line) */
	if (selection_mode == SC_SEL_RECTANGLE && selected_lines > 1)
	{
		gint first_line, line;

		sci_start_undo_action(doc->editor->sci);

		first_line = sci_get_line_from_position(doc->editor->sci, selection_start);
		/* Find the last line with chars selected (not EOL char) */
		last_line = sci_get_line_from_position(doc->editor->sci,
			selection_end - editor_get_eol_char_len(doc->editor));
		last_line = MAX(first_line, last_line);
		for (line = first_line; line < (first_line + selected_lines); line++)
		{
			gint line_start = sci_get_pos_at_line_sel_start(doc->editor->sci, line);
			gint line_end = sci_get_pos_at_line_sel_end(doc->editor->sci, line);

			/* skip line if there is no selection */
			if (line_start != INVALID_POSITION)
			{
				/* don't let document_replace_range() scroll to match to keep our selection */
				gint new_sel_end;

				count += document_replace_range(doc, find_text, replace_text, flags,
								line_start, line_end, FALSE, &new_sel_end);
				if (new_sel_end != -1)
				{
					replaced = TRUE;
					/* this gets the greatest column within the selection after replacing */
					max_column = MAX(max_column,
						new_sel_end - sci_get_position_from_line(doc->editor->sci, line));
				}
			}
		}
		sci_end_undo_action(doc->editor->sci);
	}
	else	/* handle normal line selection */
	{
		count += document_replace_range(doc, find_text, replace_text, flags,
						selection_start, selection_end, TRUE, &selection_end);
		if (selection_end != -1)
			replaced = TRUE;
	}

	if (replaced)
	{	/* update the selection for the new endpoint */

		if (selection_mode == SC_SEL_RECTANGLE && selected_lines > 1)
		{
			/* now we can scroll to the selection and destroy it because we rebuild it later */
			/*sci_goto_pos(doc->editor->sci, selection_start, FALSE);*/

			/* Note: the selection will be wrapped to last_line + 1 if max_column is greater than
			 * the highest column on the last line. The wrapped selection is completely different
			 * from the original one, so skip the selection at all */
			/* TODO is there a better way to handle the wrapped selection? */
			if ((sci_get_line_length(doc->editor->sci, last_line) - 1) >= max_column)
			{	/* for keeping and adjusting the selection in multi line rectangle selection we
				 * need the last line of the original selection and the greatest column number after
				 * replacing and set the selection end to the last line at the greatest column */
				sci_set_selection_start(doc->editor->sci, selection_start);
				sci_set_selection_end(doc->editor->sci,
					sci_get_position_from_line(doc->editor->sci, last_line) + max_column);
				sci_set_selection_mode(doc->editor->sci, selection_mode);
			}
		}
		else
		{
			sci_set_selection_start(doc->editor->sci, selection_start);
			sci_set_selection_end(doc->editor->sci, selection_end);
		}
	}
	else /* no replacements */
		utils_beep();

	show_replace_summary(doc, count, original_find_text, original_replace_text);
}


/* returns number of replacements made. */
gint document_replace_all(GeanyDocument *doc, const gchar *find_text, const gchar *replace_text,
		const gchar *original_find_text, const gchar *original_replace_text, gint flags)
{
	gint len, count;
	g_return_val_if_fail(doc != NULL && find_text != NULL && replace_text != NULL, FALSE);

	if (! *find_text)
		return FALSE;

	len = sci_get_length(doc->editor->sci);
	count = document_replace_range(
			doc, find_text, replace_text, flags, 0, len, TRUE, NULL);

	show_replace_summary(doc, count, original_find_text, original_replace_text);
	return count;
}


/*
 * Parses or re-parses the document's buffer and updates the type
 * keywords and symbol list.
 *
 * @param doc The document.
 */
void document_update_tags(GeanyDocument *doc)
{
	guchar *buffer_ptr;
	gsize len;
	GString *keywords_str;
	gchar *keywords;
	gint keyword_idx;

	g_return_if_fail(DOC_VALID(doc));
	g_return_if_fail(app->tm_workspace != NULL);

	/* early out if it's a new file or doesn't support tags */
	if (! doc->file_name || ! doc->file_type || !filetype_has_tags(doc->file_type))
	{
		/* We must call sidebar_update_tag_list() before returning,
		 * to ensure that the symbol list is always updated properly (e.g.
		 * when creating a new document with a partial filename set. */
		sidebar_update_tag_list(doc, FALSE);
		return;
	}

	/* create a new TM file if there isn't one yet */
	if (! doc->tm_file)
	{
		gchar *locale_filename = utils_get_locale_from_utf8(doc->file_name);
		const gchar *name;

		/* lookup the name rather than using filetype name to support custom filetypes */
		name = tm_source_file_get_lang_name(doc->file_type->lang);
		doc->tm_file = tm_source_file_new(locale_filename, FALSE, name);
		g_free(locale_filename);

		if (doc->tm_file && !tm_workspace_add_object(doc->tm_file))
		{
			tm_work_object_free(doc->tm_file);
			doc->tm_file = NULL;
		}
	}

	/* early out if there's no work object and we couldn't create one */
	if (doc->tm_file == NULL)
	{
		/* We must call sidebar_update_tag_list() before returning,
		 * to ensure that the symbol list is always updated properly (e.g.
		 * when creating a new document with a partial filename set. */
		sidebar_update_tag_list(doc, FALSE);
		return;
	}

	len = sci_get_length(doc->editor->sci);
	/* tm_source_file_buffer_update() below don't support 0-length data,
	 * so just empty the tags array and leave */
	if (len < 1)
	{
		tm_tags_array_free(doc->tm_file->tags_array, FALSE);
		sidebar_update_tag_list(doc, FALSE);
		return;
	}

	/* Parse Scintilla's buffer directly using TagManager
	 * Note: this buffer *MUST NOT* be modified */
	buffer_ptr = (guchar *) scintilla_send_message(doc->editor->sci, SCI_GETCHARACTERPOINTER, 0, 0);
	tm_source_file_buffer_update(doc->tm_file, buffer_ptr, len, TRUE);

	sidebar_update_tag_list(doc, TRUE);

	/* some filetypes support type keywords (such as struct names), but not
	 * necessarily all filetypes for a particular scintilla lexer.  this
	 * tells us whether the filetype supports keywords, and if so
	 * which index to use for the scintilla keywords set. */
	switch (doc->file_type->id)
	{
		case GEANY_FILETYPES_C:
		case GEANY_FILETYPES_CPP:
		case GEANY_FILETYPES_CS:
		case GEANY_FILETYPES_D:
		case GEANY_FILETYPES_JAVA:
		case GEANY_FILETYPES_OBJECTIVEC:
		case GEANY_FILETYPES_VALA:
		{

			/* index of the keyword set in the Scintilla lexer, for
			 * example in LexCPP.cxx, see "cppWordLists" global array.
			 * TODO: this magic number should be a member of the filetype */
			keyword_idx = 3;
			break;
		}
		default:
			return; /* early out if type keywords are not supported */
	}

	/* get any type keywords and tell scintilla about them
	 * this will cause the type keywords to be colourized in scintilla */
	keywords_str = symbols_find_tags_as_string(app->tm_workspace->work_object.tags_array,
		TM_GLOBAL_TYPE_MASK, doc->file_type->lang);
	if (keywords_str)
	{
		keywords = g_string_free(keywords_str, FALSE);
		sci_set_keywords(doc->editor->sci, keyword_idx, keywords);
		g_free(keywords);
		queue_colourise(doc); /* force re-highlighting the entire document */
	}
}


static gboolean on_document_update_tag_list_idle(gpointer data)
{
	GeanyDocument *doc = data;

	if (! DOC_VALID(doc))
		return FALSE;

	if (! main_status.quitting)
		document_update_tags(doc);

	doc->priv->tag_list_update_source = 0;

	/* don't update the tags until another modification of the buffer */
	return FALSE;
}


void document_update_tag_list_in_idle(GeanyDocument *doc)
{
	if (editor_prefs.autocompletion_update_freq <= 0 || ! filetype_has_tags(doc->file_type))
		return;

	/* prevent "stacking up" callback handlers, we only need one to run soon */
	if (doc->priv->tag_list_update_source != 0)
		g_source_remove(doc->priv->tag_list_update_source);

	doc->priv->tag_list_update_source = g_timeout_add_full(G_PRIORITY_LOW,
		editor_prefs.autocompletion_update_freq, on_document_update_tag_list_idle, doc, NULL);
}


static void document_load_config(GeanyDocument *doc, GeanyFiletype *type,
		gboolean filetype_changed)
{
	g_return_if_fail(doc);
	if (type == NULL)
		type = filetypes[GEANY_FILETYPES_NONE];

	if (filetype_changed)
	{
		doc->file_type = type;

		/* delete tm file object to force creation of a new one */
		if (doc->tm_file != NULL)
		{
			tm_workspace_remove_object(doc->tm_file, TRUE, TRUE);
			doc->tm_file = NULL;
		}
		/* load tags files before highlighting (some lexers highlight global typenames) */
		if (type->id != GEANY_FILETYPES_NONE)
			symbols_global_tags_loaded(type->id);

		highlighting_set_styles(doc->editor->sci, type);
		editor_set_indentation_guides(doc->editor);
		build_menu_update(doc);
		queue_colourise(doc);
		doc->priv->symbol_list_sort_mode = type->priv->symbol_list_sort_mode;
	}

	document_update_tags(doc);
}


/** Sets the filetype of the document (which controls syntax highlighting and tags)
 * @param doc The document to use.
 * @param type The filetype. */
void document_set_filetype(GeanyDocument *doc, GeanyFiletype *type)
{
	gboolean ft_changed;
	GeanyFiletype *old_ft;

	g_return_if_fail(doc);
	if (type == NULL)
		type = filetypes[GEANY_FILETYPES_NONE];

	old_ft = doc->file_type;
	geany_debug("%s : %s (%s)",
		(doc->file_name != NULL) ? doc->file_name : "unknown",
		type->name,
		(doc->encoding != NULL) ? doc->encoding : "unknown");

	ft_changed = (doc->file_type != type); /* filetype has changed */
	document_load_config(doc, type, ft_changed);

	if (ft_changed)
	{
		const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(NULL);

		/* assume that if previous filetype was none and the settings are the default ones, this
		 * is the first time the filetype is carefully set, so we should apply indent settings */
		if (old_ft && old_ft->id == GEANY_FILETYPES_NONE &&
			doc->editor->indent_type == iprefs->type &&
			doc->editor->indent_width == iprefs->width)
		{
			document_apply_indent_settings(doc);
			ui_document_show_hide(doc);
		}

		sidebar_openfiles_update(doc); /* to update the icon */
		g_signal_emit_by_name(geany_object, "document-filetype-set", doc, old_ft);
	}
}


void document_reload_config(GeanyDocument *doc)
{
	document_load_config(doc, doc->file_type, TRUE);
}


/**
 *  Sets the encoding of a document.
 *  This function only set the encoding of the %document, it does not any conversions. The new
 *  encoding is used when e.g. saving the file.
 *
 *  @param doc The document to use.
 *  @param new_encoding The encoding to be set for the document.
 **/
void document_set_encoding(GeanyDocument *doc, const gchar *new_encoding)
{
	if (doc == NULL || new_encoding == NULL ||
		utils_str_equal(new_encoding, doc->encoding))
		return;

	g_free(doc->encoding);
	doc->encoding = g_strdup(new_encoding);

	ui_update_statusbar(doc, -1);
	gtk_widget_set_sensitive(ui_lookup_widget(main_widgets.window, "menu_write_unicode_bom1"),
			encodings_is_unicode_charset(doc->encoding));
}


/* own Undo / Redo implementation to be able to undo / redo changes
 * to the encoding or the Unicode BOM (which are Scintilla independet).
 * All Scintilla events are stored in the undo / redo buffer and are passed through. */

/* Clears the Undo and Redo buffer (to be called when reloading or closing the document) */
void document_undo_clear(GeanyDocument *doc)
{
	undo_action *a;

	while (g_trash_stack_height(&doc->priv->undo_actions) > 0)
	{
		a = g_trash_stack_pop(&doc->priv->undo_actions);
		if (G_LIKELY(a != NULL))
		{
			switch (a->type)
			{
				case UNDO_ENCODING: g_free(a->data); break;
				default: break;
			}
			g_free(a);
		}
	}
	doc->priv->undo_actions = NULL;

	while (g_trash_stack_height(&doc->priv->redo_actions) > 0)
	{
		a = g_trash_stack_pop(&doc->priv->redo_actions);
		if (G_LIKELY(a != NULL))
		{
			switch (a->type)
			{
				case UNDO_ENCODING: g_free(a->data); break;
				default: break;
			}
			g_free(a);
		}
	}
	doc->priv->redo_actions = NULL;

	if (! main_status.quitting && doc->editor != NULL)
		document_set_text_changed(doc, FALSE);
}


void document_undo_add(GeanyDocument *doc, guint type, gpointer data)
{
	undo_action *action;

	g_return_if_fail(doc != NULL);

	action = g_new0(undo_action, 1);
	action->type = type;
	action->data = data;

	g_trash_stack_push(&doc->priv->undo_actions, action);

	document_set_text_changed(doc, TRUE);
	ui_update_popup_reundo_items(doc);
}


gboolean document_can_undo(GeanyDocument *doc)
{
	g_return_val_if_fail(doc != NULL, FALSE);

	if (g_trash_stack_height(&doc->priv->undo_actions) > 0 || sci_can_undo(doc->editor->sci))
		return TRUE;
	else
		return FALSE;
}


static void update_changed_state(GeanyDocument *doc)
{
	doc->changed =
		(sci_is_modified(doc->editor->sci) ||
		doc->has_bom != doc->priv->saved_encoding.has_bom ||
		! utils_str_equal(doc->encoding, doc->priv->saved_encoding.encoding));
	document_set_text_changed(doc, doc->changed);
}


void document_undo(GeanyDocument *doc)
{
	undo_action *action;

	g_return_if_fail(doc != NULL);

	action = g_trash_stack_pop(&doc->priv->undo_actions);

	if (G_UNLIKELY(action == NULL))
	{
		/* fallback, should not be necessary */
		geany_debug("%s: fallback used", G_STRFUNC);
		sci_undo(doc->editor->sci);
	}
	else
	{
		switch (action->type)
		{
			case UNDO_SCINTILLA:
			{
				document_redo_add(doc, UNDO_SCINTILLA, NULL);

				sci_undo(doc->editor->sci);
				break;
			}
			case UNDO_BOM:
			{
				document_redo_add(doc, UNDO_BOM, GINT_TO_POINTER(doc->has_bom));

				doc->has_bom = GPOINTER_TO_INT(action->data);
				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);
				break;
			}
			case UNDO_ENCODING:
			{
				/* use the "old" encoding */
				document_redo_add(doc, UNDO_ENCODING, g_strdup(doc->encoding));

				document_set_encoding(doc, (const gchar*)action->data);

				ignore_callback = TRUE;
				encodings_select_radio_item((const gchar*)action->data);
				ignore_callback = FALSE;

				g_free(action->data);
				break;
			}
			default: break;
		}
	}
	g_free(action); /* free the action which was taken from the stack */

	update_changed_state(doc);
	ui_update_popup_reundo_items(doc);
}


gboolean document_can_redo(GeanyDocument *doc)
{
	g_return_val_if_fail(doc != NULL, FALSE);

	if (g_trash_stack_height(&doc->priv->redo_actions) > 0 || sci_can_redo(doc->editor->sci))
		return TRUE;
	else
		return FALSE;
}


void document_redo(GeanyDocument *doc)
{
	undo_action *action;

	g_return_if_fail(doc != NULL);

	action = g_trash_stack_pop(&doc->priv->redo_actions);

	if (G_UNLIKELY(action == NULL))
	{
		/* fallback, should not be necessary */
		geany_debug("%s: fallback used", G_STRFUNC);
		sci_redo(doc->editor->sci);
	}
	else
	{
		switch (action->type)
		{
			case UNDO_SCINTILLA:
			{
				document_undo_add(doc, UNDO_SCINTILLA, NULL);

				sci_redo(doc->editor->sci);
				break;
			}
			case UNDO_BOM:
			{
				document_undo_add(doc, UNDO_BOM, GINT_TO_POINTER(doc->has_bom));

				doc->has_bom = GPOINTER_TO_INT(action->data);
				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);
				break;
			}
			case UNDO_ENCODING:
			{
				document_undo_add(doc, UNDO_ENCODING, g_strdup(doc->encoding));

				document_set_encoding(doc, (const gchar*)action->data);

				ignore_callback = TRUE;
				encodings_select_radio_item((const gchar*)action->data);
				ignore_callback = FALSE;

				g_free(action->data);
				break;
			}
			default: break;
		}
	}
	g_free(action); /* free the action which was taken from the stack */

	update_changed_state(doc);
	ui_update_popup_reundo_items(doc);
}


static void document_redo_add(GeanyDocument *doc, guint type, gpointer data)
{
	undo_action *action;

	g_return_if_fail(doc != NULL);

	action = g_new0(undo_action, 1);
	action->type = type;
	action->data = data;

	g_trash_stack_push(&doc->priv->redo_actions, action);

	document_set_text_changed(doc, TRUE);
	ui_update_popup_reundo_items(doc);
}


/**
 *  Gets the status color of the document, or @c NULL if default widget coloring should be used.
 *  Returned colors are red if the document has changes, green if the document is read-only
 *  or simply @c NULL if the document is unmodified but writable.
 *
 *  @param doc The document to use.
 *
 *  @return The color for the document or @c NULL if the default color should be used. The color
 *          object is owned by Geany and should not be modified or freed.
 *
 *  @since 0.16
 */
const GdkColor *document_get_status_color(GeanyDocument *doc)
{
	static GdkColor red = {0, 0xFFFF, 0, 0};
	static GdkColor green = {0, 0, 0x7FFF, 0};
#ifdef USE_GIO_FILEMON
	static GdkColor orange = {0, 0xFFFF, 0x7FFF, 0};
#endif
	GdkColor *color = NULL;

	g_return_val_if_fail(doc != NULL, NULL);

	if (doc->changed)
		color = &red;
#ifdef USE_GIO_FILEMON
	else if (doc->priv->file_disk_status == FILE_CHANGED)
		color = &orange;
#endif
	else if (doc->readonly)
		color = &green;

	return color;	/* return pointer to static GdkColor. */
}


/** Accessor function for @ref GeanyData::documents_array items.
 * @warning Always check the returned document is valid (@c doc->is_valid).
 * @param idx @c documents_array index.
 * @return The document, or @c NULL if @a idx is out of range.
 *
 *  @since 0.16
 */
GeanyDocument *document_index(gint idx)
{
	return (idx >= 0 && idx < (gint) documents_array->len) ? documents[idx] : NULL;
}


/* create a new file and copy file content and properties */
GeanyDocument *document_clone(GeanyDocument *old_doc, const gchar *utf8_filename)
{
	gint len;
	gchar *text;
	GeanyDocument *doc;

	g_return_val_if_fail(old_doc != NULL, NULL);

	len = sci_get_length(old_doc->editor->sci) + 1;
	text = (gchar*) g_malloc(len);
	sci_get_text(old_doc->editor->sci, len, text);
	/* use old file type (or maybe NULL for auto detect would be better?) */
	doc = document_new_file(utf8_filename, old_doc->file_type, text);
	g_free(text);

	/* copy file properties */
	doc->editor->line_wrapping = old_doc->editor->line_wrapping;
	doc->readonly = old_doc->readonly;
	doc->has_bom = old_doc->has_bom;
	document_set_encoding(doc, old_doc->encoding);
	sci_set_lines_wrapped(doc->editor->sci, doc->editor->line_wrapping);
	sci_set_readonly(doc->editor->sci, doc->readonly);

	ui_document_show_hide(doc);
	return doc;
}


/* @note If successful, this should always be followed up with a call to
 * document_close_all().
 * @return TRUE if all files were saved or had their changes discarded. */
gboolean document_account_for_unsaved(void)
{
	guint i, p, page_count;

	page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	/* iterate over documents in tabs order */
	for (p = 0; p < page_count; p++)
	{
		GeanyDocument *doc = document_get_from_page(p);

		if (DOC_VALID(doc) && doc->changed)
		{
			if (! dialogs_show_unsaved_file(doc))
				return FALSE;
		}
	}
	/* all documents should now be accounted for, so ignore any changes */
	foreach_document (i)
	{
		documents[i]->changed = FALSE;
	}
	return TRUE;
}


static void force_close_all(void)
{
	guint i, len = documents_array->len;

	/* check all documents have been accounted for */
	for (i = 0; i < len; i++)
	{
		if (documents[i]->is_valid)
		{
			g_return_if_fail(!documents[i]->changed);
		}
	}
	main_status.closing_all = TRUE;

	foreach_document(i)
	{
		document_close(documents[i]);
	}

	main_status.closing_all = FALSE;
}


gboolean document_close_all(void)
{
	if (! document_account_for_unsaved())
		return FALSE;

	force_close_all();

	return TRUE;
}


static void monitor_reload_file(GeanyDocument *doc)
{
	gchar *base_name = g_path_get_basename(doc->file_name);
	gint ret;

	ret = dialogs_show_prompt(NULL,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		_("_Reload"), GTK_RESPONSE_ACCEPT,
		_("Do you want to reload it?"),
		_("The file '%s' on the disk is more recent than\nthe current buffer."),
		base_name);
	g_free(base_name);

	if (ret == GTK_RESPONSE_ACCEPT)
		document_reload_file(doc, doc->encoding);
	else if (ret == GTK_RESPONSE_CLOSE)
		document_close(doc);
}


static gboolean monitor_resave_missing_file(GeanyDocument *doc)
{
	gboolean want_reload = FALSE;
	gboolean file_saved = FALSE;
	gint ret;

	ret = dialogs_show_prompt(NULL,
		_("Close _without saving"), GTK_RESPONSE_CLOSE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		_("Try to resave the file?"),
		_("File \"%s\" was not found on disk!"),
		doc->file_name);
	if (ret == GTK_RESPONSE_ACCEPT)
	{
		file_saved = dialogs_show_save_as();
		want_reload = TRUE;
	}
	else if (ret == GTK_RESPONSE_CLOSE)
	{
		document_close(doc);
	}
	if (ret != GTK_RESPONSE_CLOSE && ! file_saved)
	{
		/* file is missing - set unsaved state */
		document_set_text_changed(doc, TRUE);
		/* don't prompt more than once */
		setptr(doc->real_path, NULL);
	}

	return want_reload;
}


/* Set force to force a disk check, otherwise it is ignored if there was a check
 * in the last file_prefs.disk_check_timeout seconds.
 * @return @c TRUE if the file has changed. */
gboolean document_check_disk_status(GeanyDocument *doc, gboolean force)
{
	gboolean ret = FALSE;
	gboolean use_gio_filemon;
	time_t cur_time = 0;
	struct stat st;
	gchar *locale_filename;
	FileDiskStatus old_status;

	g_return_val_if_fail(doc != NULL, FALSE);

	/* ignore remote files and documents that have never been saved to disk */
	if (file_prefs.disk_check_timeout == 0 || doc->real_path == NULL || doc->priv->is_remote)
		return FALSE;

	use_gio_filemon = (doc->priv->monitor != NULL);

	if (use_gio_filemon)
	{
		if (doc->priv->file_disk_status != FILE_CHANGED && ! force)
			return FALSE;
	}
	else
	{
		cur_time = time(NULL);
		if (! force && doc->priv->last_check > (cur_time - file_prefs.disk_check_timeout))
			return FALSE;

		doc->priv->last_check = cur_time;
	}

	locale_filename = utils_get_locale_from_utf8(doc->file_name);
	if (g_stat(locale_filename, &st) != 0)
	{
		monitor_resave_missing_file(doc);
		/* doc may be closed now */
		ret = TRUE;
	}
	else if (! use_gio_filemon && /* ignore check when using GIO */
		doc->priv->mtime > cur_time)
	{
		g_warning("%s: Something is wrong with the time stamps.", G_STRFUNC);
		/* Note: on Windows st.st_mtime can be newer than cur_time */
	}
	else if (doc->priv->mtime < st.st_mtime)
	{
		doc->priv->mtime = st.st_mtime;
		monitor_reload_file(doc);
		/* doc may be closed now */
		ret = TRUE;
	}
	g_free(locale_filename);

	if (DOC_VALID(doc))
	{	/* doc can get invalid when a document was closed */
		old_status = doc->priv->file_disk_status;
		doc->priv->file_disk_status = FILE_OK;
		if (old_status != doc->priv->file_disk_status)
			ui_update_tab_status(doc);
	}
	return ret;
}


/** Compares documents by their display names.
 * This matches @c GCompareFunc for use with e.g. @c g_ptr_array_sort().
 * @note 'Display name' means the base name of the document's filename.
 *
 * @param a @c GeanyDocument**.
 * @param b @c GeanyDocument**.
 * @warning The arguments take the address of each document pointer.
 * @return Negative value if a < b; zero if a = b; positive value if a > b.
 *
 * @since 0.21
 */
gint document_compare_by_display_name(gconstpointer a, gconstpointer b)
{
	GeanyDocument *doc_a = *((GeanyDocument**) a);
	GeanyDocument *doc_b = *((GeanyDocument**) b);
	gchar *base_name_a, *base_name_b;
	gint result;

	base_name_a = g_path_get_basename(DOC_FILENAME(doc_a));
	base_name_b = g_path_get_basename(DOC_FILENAME(doc_b));

	result = strcmp(base_name_a, base_name_b);

	g_free(base_name_a);
	g_free(base_name_b);

	return result;
}


/** Compares documents by their tab order.
 * This matches @c GCompareFunc for use with e.g. @c g_ptr_array_sort().
 *
 * @param a @c GeanyDocument**.
 * @param b @c GeanyDocument**.
 * @warning The arguments take the address of each document pointer.
 * @return Negative value if a < b; zero if a = b; positive value if a > b.
 *
 * @since 0.21 (GEANY_API_VERSION 209)
 */
gint document_compare_by_tab_order(gconstpointer a, gconstpointer b)
{
	GeanyDocument *doc_a = *((GeanyDocument**) a);
	GeanyDocument *doc_b = *((GeanyDocument**) b);
	gint notebook_position_doc_a;
	gint notebook_position_doc_b;

	notebook_position_doc_a = document_get_notebook_page(doc_a);
	notebook_position_doc_b = document_get_notebook_page(doc_b);

	if (notebook_position_doc_a < notebook_position_doc_b)
		return -1;
	if (notebook_position_doc_a > notebook_position_doc_b)
		return 1;
	/* equality */
	return 0;
}


/** Compares documents by their tab order, in reverse order.
 * This matches @c GCompareFunc for use with e.g. @c g_ptr_array_sort().
 *
 * @param a @c GeanyDocument**.
 * @param b @c GeanyDocument**.
 * @warning The arguments take the address of each document pointer.
 * @return Negative value if a < b; zero if a = b; positive value if a > b.
 *
 * @since 0.21 (GEANY_API_VERSION 209)
 */
gint document_compare_by_tab_order_reverse(gconstpointer a, gconstpointer b)
{
	GeanyDocument *doc_a = *((GeanyDocument**) a);
	GeanyDocument *doc_b = *((GeanyDocument**) b);
	gint notebook_position_doc_a;
	gint notebook_position_doc_b;

	notebook_position_doc_a = document_get_notebook_page(doc_a);
	notebook_position_doc_b = document_get_notebook_page(doc_b);

	if (notebook_position_doc_a < notebook_position_doc_b)
		return 1;
	if (notebook_position_doc_a > notebook_position_doc_b)
		return -1;
	/* equality */
	return 0;
}


void document_grab_focus(GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
}
