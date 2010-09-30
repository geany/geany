/*
 *      document.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
#if USE_GIO_FILEMON
# ifdef HAVE_GIO
#  include <gio/gio.h>
# else
#  undef USE_GIO_FILEMON
# endif
#endif

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
static gboolean update_tags_from_buffer(GeanyDocument *doc);


/* ignore the case of filenames and paths under WIN32, causes errors if not */
#ifdef G_OS_WIN32
#define filenamecmp(a, b)	utils_str_casecmp((a), (b))
#else
#define filenamecmp(a, b)	strcmp((a), (b))
#endif

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

		if (! doc->is_valid || G_UNLIKELY(! doc->real_path))
			continue;

		if (filenamecmp(realname, doc->real_path) == 0)
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

		if (! doc->is_valid || G_UNLIKELY(doc->file_name == NULL))
			continue;

		if (filenamecmp(utf8_filename, doc->file_name) == 0)
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
	short_name = utils_str_middle_truncate(base_name, length);

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

	ui_widget_set_tooltip_text(parent, DOC_FILENAME(doc));

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


/* Sets is_valid to FALSE and initializes some members to NULL, to mark it uninitialized.
 * The flag is_valid is set to TRUE in document_create(). */
static void init_doc_struct(GeanyDocument *new_doc)
{
	GeanyDocumentPrivate *priv;

	memset(new_doc, 0, sizeof(GeanyDocument));

	new_doc->is_valid = FALSE;
	new_doc->has_tags = FALSE;
	new_doc->readonly = FALSE;
	new_doc->file_name = NULL;
	new_doc->file_type = NULL;
	new_doc->tm_file = NULL;
	new_doc->encoding = NULL;
	new_doc->has_bom = FALSE;
	new_doc->editor = NULL;
	new_doc->changed = FALSE;
	new_doc->real_path = NULL;

	new_doc->priv = g_new0(GeanyDocumentPrivate, 1);
	priv = new_doc->priv;
	priv->tag_store = NULL;
	priv->tag_tree = NULL;
	priv->saved_encoding.encoding = NULL;
	priv->saved_encoding.has_bom = FALSE;
	priv->undo_actions = NULL;
	priv->redo_actions = NULL;
	priv->line_count = 0;
#if ! defined(USE_GIO_FILEMON)
	priv->last_check = time(NULL);
#endif
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


#if USE_GIO_FILEMON
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
#if USE_GIO_FILEMON
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


static gboolean remove_page(guint page_num);

/* Creates a new document and editor, adding a tab in the notebook.
 * @return The created document */
static GeanyDocument *document_create(const gchar *utf8_filename)
{
	GeanyDocument *doc;
	gint new_idx;
	gint cur_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));

	if (cur_pages == 1)
	{
		GeanyDocument *cur = document_get_current();
		/* remove the empty document first */
		if (cur != NULL && cur->file_name == NULL && ! cur->changed)
			/* prevent immediately opening another new doc with
			 * new_document_after_close pref */
			remove_page(0);
	}

	new_idx = document_get_new_idx();
	if (new_idx == -1)	/* expand the array, no free places */
	{
		GeanyDocument *new_doc = g_new0(GeanyDocument, 1);

		new_idx = documents_array->len;
		g_ptr_array_add(documents_array, new_doc);
	}
	doc = documents[new_idx];
	init_doc_struct(doc);	/* initialize default document settings */
	doc->index = new_idx;

	doc->file_name = g_strdup(utf8_filename);

	doc->editor = editor_create(doc);

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


/* Call document_remove_page() instead, this is only needed for document_create(). */
static gboolean remove_page(guint page_num)
{
	GeanyDocument *doc = document_get_from_page(page_num);

	if (G_UNLIKELY(doc == NULL))
	{
		g_warning("%s: page_num: %d", G_STRFUNC, page_num);
		return FALSE;
	}

	if (doc->changed && ! dialogs_show_unsaved_file(doc))
	{
		return FALSE;
	}

	/* tell any plugins that the document is about to be closed */
	g_signal_emit_by_name(geany_object, "document-close", doc);

	/* Checking real_path makes it likely the file exists on disk */
	if (! main_status.closing_all && doc->real_path != NULL)
		ui_add_recent_file(doc->file_name);

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
	doc->editor = NULL;

	document_stop_file_monitoring(doc);

	doc->file_name = NULL;
	doc->real_path = NULL;
	doc->file_type = NULL;
	doc->encoding = NULL;
	doc->has_bom = FALSE;
	doc->tm_file = NULL;
	document_undo_clear(doc);
	g_free(doc->priv);

	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) == 0)
	{
		sidebar_update_tag_list(NULL, FALSE);
		/*on_notebook1_switch_page(GTK_NOTEBOOK(main_widgets.notebook), NULL, 0, NULL);*/
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

	document_set_filetype(doc, ft);	/* also clears taglist */

	ui_set_window_title(doc);
	build_menu_update(doc);
	document_update_tag_list(doc, FALSE);
	document_set_text_changed(doc, FALSE);
	ui_document_show_hide(doc); /* update the document menu */

	sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin, 0);
	/* bring it in front, jump to the start and grab the focus */
	editor_goto_pos(doc->editor, 0, FALSE);
	document_try_focus(doc, NULL);

#if USE_GIO_FILEMON
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
	gsize		 size;	/* actual file size on disk */
	gsize		 len;	/* string length of data */
	gchar		*enc;
	gboolean	 bom;
	time_t		 mtime;	/* modification time, read by stat::st_mtime */
	gboolean	 readonly;
} FileData;


/* reload file with specified encoding */
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
										filedata->data, filedata->size, forced_enc, FALSE);
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


/* detect encoding and convert to UTF-8 if necessary */
static gboolean
handle_encoding(FileData *filedata, GeanyEncodingIndex enc_idx)
{
	g_return_val_if_fail(filedata->enc == NULL, FALSE);
	g_return_val_if_fail(filedata->bom == FALSE, FALSE);

	if (filedata->size == 0)
	{
		/* we have no data so assume UTF-8, filedata->len can be 0 even we have an empty
		 * e.g. UTF32 file with a BOM(so size is 4, len is 0) */
		filedata->enc = g_strdup("UTF-8");
	}
	else
	{
		/* first check for a BOM */
		if (enc_idx != GEANY_ENCODING_NONE)
		{
			filedata->enc = g_strdup(encodings[enc_idx].charset);
			filedata->bom = TRUE;

			if (enc_idx != GEANY_ENCODING_UTF_8) /* the BOM indicated something else than UTF-8 */
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
					/* there was a problem converting data from BOM encoding type */
					g_free(filedata->enc);
					filedata->enc = NULL;
					filedata->bom = FALSE;
				}
			}
		}

		if (filedata->enc == NULL)	/* either there was no BOM or the BOM encoding failed */
		{
			/* try UTF-8 first */
			if ((filedata->size == filedata->len) &&
				g_utf8_validate(filedata->data, filedata->len, NULL))
			{
				filedata->enc = g_strdup("UTF-8");
			}
			else
			{
				/* detect the encoding */
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

	/* use filedata->len here because the contents are already converted into UTF-8 */
	filedata->len -= bom_len;
	/* overwrite the BOM with the remainder of the file contents, plus the NULL terminator. */
	g_memmove(filedata->data, filedata->data + bom_len, filedata->len + 1);
	filedata->data = g_realloc(filedata->data, filedata->len + 1);
}


/* loads textfile data, verifies and converts to forced_enc or UTF-8. Also handles BOM. */
static gboolean load_text_file(const gchar *locale_filename, const gchar *display_filename,
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

	/* use strlen to check for null chars */
	filedata->size = (gsize) st.st_size;
	filedata->len = strlen(filedata->data);

	/* temporarily retrieve the encoding idx based on the BOM to suppress the following warning
	 * if we have a BOM */
	tmp_enc_idx = encodings_scan_unicode_bom(filedata->data, filedata->size, NULL);

	/* check whether the size of the loaded data is equal to the size of the file in the
	 * filesystem file size may be 0 to allow opening files in /proc/ which have typically a
	 * file size of 0 bytes */
	if (filedata->len != filedata->size && filedata->size != 0 && (
		tmp_enc_idx == GEANY_ENCODING_UTF_8 || /* tmp_enc_idx can be UTF-7/8/16/32, UCS and None */
		tmp_enc_idx == GEANY_ENCODING_UTF_7))  /* filter UTF-7/8 where no NULL bytes are allowed */
	{
		const gchar *warn_msg = _(
			"The file \"%s\" could not be opened properly and has been truncated. " \
			"This can occur if the file contains a NULL byte. " \
			"Be aware that saving it can cause data loss.\nThe file was set to read-only.");

		if (main_status.main_window_realized)
			dialogs_show_msgbox(GTK_MESSAGE_WARNING, warn_msg, display_filename);

		ui_set_statusbar(TRUE, warn_msg, display_filename);

		/* set the file to read-only mode because saving it is probably dangerous */
		filedata->readonly = TRUE;
	}

	/* Determine character encoding and convert to UTF-8 */
	if (forced_enc != NULL)
	{
		/* the encoding should be ignored(requested by user), so open the file "as it is" */
		if (utils_str_equal(forced_enc, encodings[GEANY_ENCODING_NONE].charset))
		{
			filedata->bom = FALSE;
			filedata->enc = g_strdup(encodings[GEANY_ENCODING_NONE].charset);
		}
		else if (! handle_forced_encoding(filedata, forced_enc))
		{
			/* For translators: the second wildcard is an encoding name, e.g.
			 * The file \"test.txt\" is not valid UTF-8. */
			ui_set_statusbar(TRUE, _("The file \"%s\" is not valid %s."),
				display_filename, forced_enc);
			utils_beep();
			g_free(filedata->data);
			return FALSE;
		}
	}
	else if (! handle_encoding(filedata, tmp_enc_idx))
	{
		ui_set_statusbar(TRUE,
	_("The file \"%s\" does not look like a text file or the file encoding is not supported."),
			display_filename);
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
	gchar *soft_tab = g_strnfill(iprefs->width, ' ');
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


/* Detect the indent type based on counting the leading indent characters for each line. */
static GeanyIndentType detect_indent_type(GeanyEditor *editor)
{
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);
	ScintillaObject *sci = editor->sci;
	guint line, line_count;
	gsize tabs = 0, spaces = 0;

	if (detect_tabs_and_spaces(editor))
		return GEANY_INDENT_TYPE_BOTH;

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
		else
		if (c == ' ')
		{
			/* check for at least 2 spaces */
			if (sci_get_char_at(sci, pos + 1) == ' ')
				spaces++;
		}
	}
	if (spaces == 0 && tabs == 0)
		return iprefs->type;

	/* the factors may need to be tweaked */
	if (spaces > tabs * 4)
		return GEANY_INDENT_TYPE_SPACES;
	else if (tabs > spaces * 4)
		return GEANY_INDENT_TYPE_TABS;
	else
		return GEANY_INDENT_TYPE_BOTH;
}


void document_apply_indent_settings(GeanyDocument *doc)
{
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(NULL);
	GeanyIndentType type = iprefs->type;

	switch (FILETYPE_ID(doc->file_type))
	{
		case GEANY_FILETYPES_MAKE:
			/* force using tabs for indentation for Makefiles */
			editor_set_indent(doc->editor, GEANY_INDENT_TYPE_TABS, iprefs->width);
			return;
		case GEANY_FILETYPES_F77:
			/* force using spaces for indentation for Fortran 77 */
			editor_set_indent(doc->editor, GEANY_INDENT_TYPE_SPACES, iprefs->width);
			return;
	}
	if (iprefs->detect_type)
	{
		type = detect_indent_type(doc->editor);

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
	editor_set_indent(doc->editor, type, iprefs->width);
}


#if 0
static gboolean auto_update_tag_list(gpointer data)
{
	GeanyDocument *doc = data;

	if (! doc || ! doc->is_valid || doc->tm_file == NULL)
		return FALSE;

	if (gtk_window_get_focus(GTK_WINDOW(main_widgets.window)) != GTK_WIDGET(doc->editor->sci))
		return TRUE;

	if (update_tags_from_buffer(doc))
		sidebar_update_tag_list(doc, TRUE);

	return TRUE;
}
#endif


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
		if (filename == NULL)
		{
			ui_set_statusbar(FALSE, _("Invalid filename"));
			return NULL;
		}

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
			ui_add_recent_file(utf8_filename);	/* either add or reorder recent item */
			/* show the doc before reload dialog */
			gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook),
				gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook),
				(GtkWidget*) doc->editor->sci));
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
			ui_add_recent_file(utf8_filename);

		if (! reload)
			g_signal_emit_by_name(geany_object, "document-open", doc);

		if (reload)
			ui_set_statusbar(TRUE, _("File %s reloaded."), display_filename);
		else
			/* For translators: this is the status window message for opening a file. %d is the number
			 * of the newly opened file, %s indicates whether the file is opened read-only
			 * (it is replaced with the string ", read-only"). */
			msgwin_status_add(_("File %s opened(%d%s)."),
				display_filename, gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)),
				(readonly) ? _(", read-only") : "");
	}

	g_free(display_filename);
	g_free(utf8_filename);
	g_free(locale_filename);

	/* TODO This could be used to automatically update the symbol list,
	 * based on a configurable interval */
	/*g_timeout_add(10000, auto_update_tag_list, doc);*/

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
 * length is the length of the string or -1 if it should be detected */
void document_open_file_list(const gchar *data, gssize length)
{
	gint i;
	gchar *filename;
	gchar **list;

	g_return_if_fail(data != NULL);

	if (length < 0)
		length = strlen(data);

	switch (utils_get_line_endings(data, length))
	{
		case SC_EOL_CR: list = g_strsplit(data, "\r", 0); break;
		case SC_EOL_CRLF: list = g_strsplit(data, "\r\n", 0); break;
		case SC_EOL_LF: list = g_strsplit(data, "\n", 0); break;
		default: list = g_strsplit(data, "\n", 0);
	}

	for (i = 0; ; i++)
	{
		if (list[i] == NULL)
			break;
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
#if ! USE_GIO_FILEMON
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
	if (FILETYPE_ID(doc->file_type) == GEANY_FILETYPES_NONE)
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
		ui_add_recent_file(doc->file_name);
	return ret;
}


static gsize save_convert_to_encoding(GeanyDocument *doc, gchar **data, gsize *len)
{
	GError *conv_error = NULL;
	gchar* conv_file_contents = NULL;
	gsize bytes_read;
	gsize conv_len;

	g_return_val_if_fail(data != NULL || *data == NULL, FALSE);
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


static gchar *write_data_to_disk(GeanyDocument *doc, const gchar *locale_filename,
								 const gchar *data, gint len)
{
	FILE *fp;
	gint bytes_written;
	gint err = 0;
	GError *error = NULL;

	g_return_val_if_fail(doc != NULL, g_strdup(g_strerror(EINVAL)));
	g_return_val_if_fail(data != NULL, g_strdup(g_strerror(EINVAL)));

	if (! file_prefs.use_safe_file_saving)
	{
		fp = g_fopen(locale_filename, "wb");
		if (G_UNLIKELY(fp == NULL))
			return g_strdup(g_strerror(errno));

		bytes_written = fwrite(data, sizeof(gchar), len, fp);

		if (G_UNLIKELY(len != bytes_written))
			err = errno;

		fclose(fp);

		if (err != 0)
			return g_strdup(g_strerror(err));
	}
	else
	{
		g_file_set_contents(locale_filename, data, len, &error);
		if (error != NULL)
		{
			gchar *msg = g_strdup(error->message);
			g_error_free(error);
			return msg;
		}
	}

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
 *  Saves the document. Saving includes replacing tabs by spaces,
 *  stripping trailing spaces and adding a final new line at the end of the file (all only if
 *  user enabled these features). Then the @c "document-before-save" signal is emitted,
 *  allowing plugins to modify the document before it is saved, and data is
 *  actually written to disk. The filetype is set again or auto-detected if it wasn't set yet.
 *  Afterwards, the @c "document-save" signal is emitted for plugins.
 *
 *  If the file is not modified, this functions does nothing unless force is set to @c TRUE.
 *
 *  @param doc The document to save.
 *  @param force Whether to save the file even if it is not modified (e.g. for Save As).
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

	/* the "changed" flag should exclude the "readonly" flag, but check it anyway for safety */
	if (! force && ! ui_prefs.allow_always_save && (! doc->changed || doc->readonly))
		return FALSE;

	if (G_UNLIKELY(doc->file_name == NULL))
	{
		ui_set_statusbar(TRUE, _("Error saving file."));
		utils_beep();
		return FALSE;
	}

	/* replaces tabs by spaces but only if the current file is not a Makefile */
	if (file_prefs.replace_tabs && FILETYPE_ID(doc->file_type) != GEANY_FILETYPES_MAKE)
		editor_replace_tabs(doc->editor);
	/* strip trailing spaces */
	if (file_prefs.strip_trailing_spaces)
		editor_strip_trailing_spaces(doc->editor);
	/* ensure the file has a newline at the end */
	if (file_prefs.final_new_line)
		editor_ensure_final_newline(doc->editor);

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
	errmsg = write_data_to_disk(doc, locale_filename, data, len);
	g_free(data);

	if (errmsg != NULL)
	{
		ui_set_statusbar(TRUE, _("Error saving file (%s)."), errmsg);
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
gboolean document_search_bar_find(GeanyDocument *doc, const gchar *text, gint flags, gboolean inc)
{
	gint start_pos, search_pos;
	struct Sci_TextToFind ttf;

	g_return_val_if_fail(text != NULL, FALSE);
	g_return_val_if_fail(doc != NULL, FALSE);
	if (! *text)
		return TRUE;

	start_pos = (inc) ? sci_get_selection_start(doc->editor->sci) :
		sci_get_selection_end(doc->editor->sci);	/* equal if no selection */

	/* search cursor to end */
	ttf.chrg.cpMin = start_pos;
	ttf.chrg.cpMax = sci_get_length(doc->editor->sci);
	ttf.lpstrText = (gchar *)text;
	search_pos = sci_find_text(doc->editor->sci, flags, &ttf);

	/* if no match, search start to cursor */
	if (search_pos == -1)
	{
		ttf.chrg.cpMin = 0;
		ttf.chrg.cpMax = start_pos + strlen(text);
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
 * Will skip past any selection, ignoring it. */
gint document_find_text(GeanyDocument *doc, const gchar *text, gint flags, gboolean search_backwards,
		gboolean scroll, GtkWidget *parent)
{
	gint selection_end, selection_start, search_pos;

	g_return_val_if_fail(doc != NULL && text != NULL, -1);
	if (! *text)
		return -1;

	/* Sci doesn't support searching backwards with a regex */
	if (flags & SCFIND_REGEXP)
		search_backwards = FALSE;

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
			ui_set_statusbar(FALSE, _("\"%s\" was not found."), text);
			utils_beep();
			return -1;
		}

		/* we searched only part of the document, so ask whether to wraparound. */
		if (search_prefs.suppress_dialogs ||
			dialogs_show_question_full(parent, GTK_STOCK_FIND, GTK_STOCK_CANCEL,
				_("Wrap search and find again?"), _("\"%s\" was not found."), text))
		{
			gint ret;

			sci_set_current_position(doc->editor->sci, (search_backwards) ? sci_len : 0, FALSE);
			ret = document_find_text(doc, text, flags, search_backwards, scroll, parent);
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
 * Returns: start of replaced text, or -1 if no replacement was made */
gint document_replace_text(GeanyDocument *doc, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean search_backwards)
{
	gint selection_end, selection_start, search_pos;

	g_return_val_if_fail(doc != NULL && find_text != NULL && replace_text != NULL, -1);

	if (! *find_text)
		return -1;

	/* Sci doesn't support searching backwards with a regex */
	if (flags & SCFIND_REGEXP)
		search_backwards = FALSE;

	selection_start = sci_get_selection_start(doc->editor->sci);
	selection_end = sci_get_selection_end(doc->editor->sci);
	if (selection_end == selection_start)
	{
		/* no selection so just find the next match */
		document_find_text(doc, find_text, flags, search_backwards, TRUE, NULL);
		return -1;
	}
	/* there's a selection so go to the start before finding to search through it
	 * this ensures there is a match */
	if (search_backwards)
		sci_goto_pos(doc->editor->sci, selection_end, TRUE);
	else
		sci_goto_pos(doc->editor->sci, selection_start, TRUE);

	search_pos = document_find_text(doc, find_text, flags, search_backwards, TRUE, NULL);
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


static void show_replace_summary(GeanyDocument *doc, gint count, const gchar *find_text,
		const gchar *replace_text, gboolean escaped_chars)
{
	gchar *escaped_find_text, *escaped_replace_text, *filename;

	if (count == 0)
	{
		ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), find_text);
		return;
	}

	filename = g_path_get_basename(DOC_FILENAME(doc));

	if (escaped_chars)
	{	/* escape special characters for showing */
		escaped_find_text = g_strescape(find_text, NULL);
		escaped_replace_text = g_strescape(replace_text, NULL);
		ui_set_statusbar(TRUE, ngettext(
			"%s: replaced %d occurrence of \"%s\" with \"%s\".",
			"%s: replaced %d occurrences of \"%s\" with \"%s\".",
			count),	filename, count, escaped_find_text, escaped_replace_text);
		g_free(escaped_find_text);
		g_free(escaped_replace_text);
	}
	else
	{
		ui_set_statusbar(TRUE, ngettext(
			"%s: replaced %d occurrence of \"%s\" with \"%s\".",
			"%s: replaced %d occurrences of \"%s\" with \"%s\".",
			count), filename, count, find_text, replace_text);
	}
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
						  gint flags, gboolean escaped_chars)
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

	show_replace_summary(doc, count, find_text, replace_text, escaped_chars);
}


/* returns number of replacements made. */
gint document_replace_all(GeanyDocument *doc, const gchar *find_text, const gchar *replace_text,
		gint flags, gboolean escaped_chars)
{
	gint len, count;
	g_return_val_if_fail(doc != NULL && find_text != NULL && replace_text != NULL, FALSE);

	if (! *find_text)
		return FALSE;

	len = sci_get_length(doc->editor->sci);
	count = document_replace_range(
			doc, find_text, replace_text, flags, 0, len, TRUE, NULL);

	show_replace_summary(doc, count, find_text, replace_text, escaped_chars);
	return count;
}


static gboolean update_tags_from_buffer(GeanyDocument *doc)
{
	gboolean result;
#if 1
		/* old code */
		result = tm_source_file_update(doc->tm_file, TRUE, FALSE, TRUE);
#else
		gsize len = sci_get_length(doc->editor->sci) + 1;
		gchar *text = g_malloc(len);

		/* we copy the whole text into memory instead using a direct char pointer from
		 * Scintilla because tm_source_file_buffer_update() does modify the string slightly */
		sci_get_text(doc->editor->sci, len, text);
		result = tm_source_file_buffer_update(doc->tm_file, (guchar*) text, len, TRUE);
		g_free(text);
#endif
	return result;
}


void document_update_tag_list(GeanyDocument *doc, gboolean update)
{
	/* We must call sidebar_update_tag_list() before returning,
	 * to ensure that the symbol list is always updated properly (e.g.
	 * when creating a new document with a partial filename set. */
	gboolean success = FALSE;

	/* if the filetype doesn't have a tag parser or it is a new file */
	if (doc == NULL || doc->file_type == NULL || app->tm_workspace == NULL ||
		! filetype_has_tags(doc->file_type) || ! doc->file_name)
	{
		/* set the default (empty) tag list */
		sidebar_update_tag_list(doc, FALSE);
		return;
	}

	if (doc->tm_file == NULL)
	{
		gchar *locale_filename = utils_get_locale_from_utf8(doc->file_name);
		const gchar *name;

		/* lookup the name rather than using filetype name to support custom filetypes */
		name = tm_source_file_get_lang_name(doc->file_type->lang);
		doc->tm_file = tm_source_file_new(locale_filename, FALSE, name);
		g_free(locale_filename);

		if (doc->tm_file)
		{
			if (!tm_workspace_add_object(doc->tm_file))
			{
				tm_work_object_free(doc->tm_file);
				doc->tm_file = NULL;
			}
			else
			{
				if (update)
					update_tags_from_buffer(doc);
				success = TRUE;
			}
		}
	}
	else
	{
		success = update_tags_from_buffer(doc);
		if (G_UNLIKELY(! success))
			geany_debug("tag list updating failed");
	}
	sidebar_update_tag_list(doc, success);
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
		return FALSE;	/* project typenames haven't changed */
	}
	/* cache typename list for next time */
	if (last_typenames)
		g_string_free(last_typenames, TRUE);
	last_typenames = s;

	*types = s;
	if (s == NULL)
		return FALSE;
	return TRUE;
}


/* If sci is NULL, update project typenames for all documents that support typenames,
 * if typenames have changed.
 * If sci is not NULL, then if sci supports typenames, project typenames are updated
 * if necessary, and typename keywords are set for sci.
 * Returns: TRUE if any scintilla type keywords were updated. */
static gboolean update_type_keywords(GeanyDocument *doc, gint lang)
{
	gboolean ret = FALSE;
	guint n;
	const GString *s;
	ScintillaObject *sci;

	g_return_val_if_fail(doc != NULL, FALSE);
	sci = doc->editor->sci;

	switch (FILETYPE_ID(doc->file_type))
	{	/* continue working with the following languages, skip on all others */
		case GEANY_FILETYPES_C:
		case GEANY_FILETYPES_CPP:
		case GEANY_FILETYPES_CS:
		case GEANY_FILETYPES_D:
		case GEANY_FILETYPES_JAVA:
		case GEANY_FILETYPES_VALA:
			break;
		default:
			return FALSE;
	}

	sci = doc->editor->sci;
	if (sci != NULL && editor_lexer_get_type_keyword_idx(sci_get_lexer(sci)) == -1)
		return FALSE;

	if (! get_project_typenames(&s, lang))
	{	/* typenames have not changed */
		if (s != NULL && sci != NULL)
		{
			gint keyword_idx = editor_lexer_get_type_keyword_idx(sci_get_lexer(sci));

			sci_set_keywords(sci, keyword_idx, s->str);
			queue_colourise(doc);
		}
		return FALSE;
	}
	g_return_val_if_fail(s != NULL, FALSE);

	for (n = 0; n < documents_array->len; n++)
	{
		if (documents[n]->is_valid)
		{
			ScintillaObject *wid = documents[n]->editor->sci;
			gint keyword_idx = editor_lexer_get_type_keyword_idx(sci_get_lexer(wid));

			if (keyword_idx > 0)
			{
				sci_set_keywords(wid, keyword_idx, s->str);
				queue_colourise(documents[n]);
				ret = TRUE;
			}
		}
	}
	return ret;
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

	document_update_tag_list(doc, TRUE);

	/* Update session typename keywords. */
	update_type_keywords(doc, type->lang);
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
		(type->name != NULL) ? type->name : "unknown",
		(doc->encoding != NULL) ? doc->encoding : "unknown");

	ft_changed = (doc->file_type != type); /* filetype has changed */
	document_load_config(doc, type, ft_changed);

	if (ft_changed)
		g_signal_emit_by_name(geany_object, "document-filetype-set", doc, old_ft);
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
#if USE_GIO_FILEMON
	static GdkColor orange = {0, 0xFFFF, 0x7FFF, 0};
#endif
	GdkColor *color = NULL;

	g_return_val_if_fail(doc != NULL, NULL);

	if (doc->changed)
		color = &red;
#if USE_GIO_FILEMON
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
	guint i, p, page_count, len = documents_array->len;
	GeanyDocument *doc;

	page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	for (p = 0; p < page_count; p++)
	{
		doc = document_get_from_page(p);
		if (DOC_VALID(doc) && doc->changed)
		{
			if (! dialogs_show_unsaved_file(doc))
				return FALSE;
		}
	}
	/* all documents should now be accounted for, so ignore any changes */
	for (i = 0; i < len; i++)
	{
		doc = documents[i];
		if (doc->is_valid && doc->changed)
		{
			doc->changed = FALSE;
		}
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
		NULL,
		_("File \"%s\" was not found on disk! Try to resave the file?"),
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
	else if (! use_gio_filemon && /* ignore these checks when using GIO */
			 (G_UNLIKELY(doc->priv->mtime > cur_time) || G_UNLIKELY(st.st_mtime > cur_time)))
	{
		g_warning("%s: Something is wrong with the time stamps.", G_STRFUNC);
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


