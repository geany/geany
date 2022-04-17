/*
 *      document.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 The Geany contributors
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 *  Document related actions: new, save, open, etc.
 *  Also Scintilla search actions.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "document.h"

#include "app.h"
#include "callbacks.h" /* for ignore_callback */
#include "dialogs.h"
#include "documentprivate.h"
#include "encodings.h"
#include "encodingsprivate.h"
#include "filetypesprivate.h"
#include "geany.h" /* FIXME: why is this needed for DOC_FILENAME()? should come from documentprivate.h/document.h */
#include "geanyobject.h"
#include "geanywraplabel.h"
#include "highlighting.h"
#include "main.h"
#include "msgwindow.h"
#include "navqueue.h"
#include "notebook.h"
#include "project.h"
#include "sciwrappers.h"
#include "sidebar.h"
#include "support.h"
#include "symbols.h"
#include "ui_utils.h"
#include "utils.h"
#include "vte.h"
#include "win32.h"

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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>


#define USE_GIO_FILE_OPERATIONS (!file_prefs.use_safe_file_saving && file_prefs.use_gio_unsafe_file_saving)


GeanyFilePrefs file_prefs;
GPtrArray *documents_array = NULL;


/* an undo action, also used for redo actions */
typedef struct
{
	GTrashStack *next;	/* pointer to the next stack element(required for the GTrashStack) */
	guint type;			/* to identify the action */
	gpointer *data; 	/* the old value (before the change), in case of a redo action
						 * it contains the new value */
} undo_action;

/* Custom document info bar response IDs */
enum
{
	RESPONSE_DOCUMENT_RELOAD = 1,
	RESPONSE_DOCUMENT_SAVE,
};


static guint doc_id_counter = 0;


static void document_undo_clear_stack(GTrashStack **stack);
static void document_undo_clear(GeanyDocument *doc);
static void document_undo_add_internal(GeanyDocument *doc, guint type, gpointer data);
static void document_redo_add(GeanyDocument *doc, guint type, gpointer data);
static gboolean remove_page(guint page_num);
static GtkWidget* document_show_message(GeanyDocument *doc, GtkMessageType msgtype,
	void (*response_cb)(GtkWidget *info_bar, gint response_id, GeanyDocument *doc),
	const gchar *btn_1, GtkResponseType response_1,
	const gchar *btn_2, GtkResponseType response_2,
	const gchar *btn_3, GtkResponseType response_3,
	const gchar *extra_text, const gchar *format, ...) G_GNUC_PRINTF(11, 12);


/**
 * Finds a document whose @c real_path field matches the given filename.
 *
 * @param realname The filename to search, which should be identical to the
 * string returned by @c utils_get_real_path().
 *
 * @return @transfer{none} @nullable The matching document, or @c NULL.
 * @note This is only really useful when passing a @c TMSourceFile::file_name.
 * @see GeanyDocument::real_path.
 * @see document_find_by_filename().
 *
 * @since 0.15
 **/
GEANY_API_SYMBOL
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
	gchar *realname = utils_get_real_path(locale_name);

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
 *  @return @transfer{none} @nullable The matching document, or @c NULL.
 *  @see document_find_by_real_path().
 **/
GEANY_API_SYMBOL
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


/** Lookup an old document by its ID.
 * Useful when the corresponding document may have been closed since the
 * ID was retrieved.
 * @param id The ID of the document to find
 * @return @transfer{none} @c NULL if the document is no longer open.
 *
 * Example:
 * @code
 * static guint id;
 * GeanyDocument *doc = ...;
 * id = doc->id;	// store ID
 * ...
 * // time passes - the document may have been closed by now
 * GeanyDocument *doc = document_find_by_id(id);
 * gboolean still_open = (doc != NULL);
 * @endcode
 * @since 1.25. */
GEANY_API_SYMBOL
GeanyDocument *document_find_by_id(guint id)
{
	guint i;

	if (!id)
		return NULL;

	foreach_document(i)
	{
		if (documents[i]->id == id)
			return documents[i];
	}
	return NULL;
}


/* gets the widget the main_widgets.notebook consider is its child for this document */
static GtkWidget *document_get_notebook_child(GeanyDocument *doc)
{
	GtkWidget *parent;
	GtkWidget *child;

	g_return_val_if_fail(doc != NULL, NULL);

	child = GTK_WIDGET(doc->editor->sci);
	parent = gtk_widget_get_parent(child);
	/* search for the direct notebook child, mirroring document_get_from_page() */
	while (parent && ! GTK_IS_NOTEBOOK(parent))
	{
		child = parent;
		parent = gtk_widget_get_parent(child);
	}

	return child;
}


/** Gets the notebook page index for a document.
 * @param doc The document.
 * @return The index.
 * @since 0.19 */
GEANY_API_SYMBOL
gint document_get_notebook_page(GeanyDocument *doc)
{
	GtkWidget *child = document_get_notebook_child(doc);

	return gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook), child);
}


/*
 * Recursively searches a containers children until it finds a
 * Scintilla widget, or NULL if one was not found.
 */
static ScintillaObject *locate_sci_in_container(GtkWidget *container)
{
	ScintillaObject *sci = NULL;
	GList *children, *iter;

	g_return_val_if_fail(GTK_IS_CONTAINER(container), NULL);

	children = gtk_container_get_children(GTK_CONTAINER(container));
	for (iter = children; iter != NULL; iter = g_list_next(iter))
	{
		if (IS_SCINTILLA(iter->data))
		{
			sci = SCINTILLA(iter->data);
			break;
		}
		else if (GTK_IS_CONTAINER(iter->data))
		{
			sci = locate_sci_in_container(iter->data);
			if (IS_SCINTILLA(sci))
				break;
			sci = NULL;
		}
	}
	g_list_free(children);

	return sci;
}


/* Finds the document for the given notebook page widget */
GeanyDocument *document_get_from_notebook_child(GtkWidget *page)
{
	ScintillaObject *sci;

	g_return_val_if_fail(GTK_IS_BOX(page), NULL);

	sci = locate_sci_in_container(page);
	g_return_val_if_fail(IS_SCINTILLA(sci), NULL);

	return document_find_by_sci(sci);
}


/**
 *  Finds the document for the given notebook page @a page_num.
 *
 *  @param page_num The notebook page number to search.
 *
 *  @return @transfer{none} @nullable The corresponding document for the given notebook page, or @c NULL.
 **/
GEANY_API_SYMBOL
GeanyDocument *document_get_from_page(guint page_num)
{
	GtkWidget *parent;

	if (page_num >= documents_array->len)
		return NULL;

	parent = gtk_notebook_get_nth_page(GTK_NOTEBOOK(main_widgets.notebook), page_num);

	return document_get_from_notebook_child(parent);
}


/**
 *  Finds the current document.
 *
 *  @return @transfer{none} @nullable A pointer to the current document or @c NULL if there are no opened documents.
 **/
GEANY_API_SYMBOL
GeanyDocument *document_get_current(void)
{
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.notebook));

	if (cur_page == -1)
		return NULL;
	else
		return document_get_from_page((guint) cur_page);
}


void document_init_doclist(void)
{
	documents_array = g_ptr_array_new();
}


void document_finalize(void)
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
GEANY_API_SYMBOL
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
GEANY_API_SYMBOL
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
	if (doc->priv->colourise_needed)
		return;

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
	doc->priv->tag_filter = g_strdup("");
	doc->priv->symbols_group_by_type = TRUE;
	doc->id = ++doc_id_counter;
	doc->index = new_idx;
	doc->file_name = g_strdup(utf8_filename);
	doc->editor = editor_create(doc);
#ifndef USE_GIO_FILEMON
	doc->priv->last_check = time(NULL);
#endif

	g_datalist_init(&doc->priv->data);

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
GEANY_API_SYMBOL
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

	/* if we're closing all, document_account_for_unsaved() has been called already, no need to ask again. */
	if (! main_status.closing_all && doc->changed && ! dialogs_show_unsaved_file(doc))
		return FALSE;

	/* tell any plugins that the document is about to be closed */
	g_signal_emit_by_name(geany_object, "document-close", doc);

	/* Checking real_path makes it likely the file exists on disk */
	if (! main_status.closing_all && doc->real_path != NULL)
		ui_add_recent_document(doc);

	g_datalist_clear(&doc->priv->data);

	doc->is_valid = FALSE;
	doc->id = 0;

	if (main_status.quitting)
	{
		/* we need to destroy the ScintillaWidget so our handlers on it are
		 * disconnected before we free any data they may use (like the editor).
		 * when not quitting, this is handled by removing the notebook page. */
		gtk_notebook_remove_page(GTK_NOTEBOOK(main_widgets.notebook), page_num);
	}
	else
	{
		notebook_remove_page(page_num);
		sidebar_remove_document(doc);
		navqueue_remove_file(doc->file_name);
		msgwin_status_add(_("File %s closed."), DOC_FILENAME(doc));
	}
	g_free(doc->encoding);
	g_free(doc->priv->saved_encoding.encoding);
	g_free(doc->priv->tag_filter);
	g_free(doc->file_name);
	g_free(doc->real_path);
	if (doc->tm_file)
	{
		tm_workspace_remove_source_file(doc->tm_file);
		tm_source_file_free(doc->tm_file);
	}

	if (doc->priv->tag_tree)
		gtk_widget_destroy(doc->priv->tag_tree);

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
GEANY_API_SYMBOL
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
 *  @param utf8_filename @nullable The file name in UTF-8 encoding, or @c NULL to open a file as "untitled".
 *  @param ft @nullable The filetype to set or @c NULL to detect it from @a filename if not @c NULL.
 *  @param text @nullable The initial content of the file (in UTF-8 encoding), or @c NULL.
 *
 *  @return @transfer{none} The new document.
 **/
GEANY_API_SYMBOL
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

	/* now the document is fully ready, display it (see notebook_new_tab()) */
	gtk_widget_show(document_get_notebook_child(doc));

	ui_set_window_title(doc);
	build_menu_update(doc);
	document_set_text_changed(doc, FALSE);
	ui_document_show_hide(doc); /* update the document menu */

	sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin);
	/* bring it in front, jump to the start and grab the focus */
	editor_goto_pos(doc->editor, 0, FALSE);
	document_try_focus(doc, NULL);

#ifdef USE_GIO_FILEMON
	monitor_file_setup(doc);
#else
	doc->priv->mtime = 0;
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
 *  @param ft @nullable The filetype for the document or @c NULL to auto-detect the filetype.
 *  @param forced_enc @nullable The file encoding to use or @c NULL to auto-detect the file encoding.
 *
 *  @return @transfer{none} @nullable The document opened or @c NULL.
 **/
GEANY_API_SYMBOL
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


static gboolean get_mtime(const gchar *locale_filename, time_t *time)
{
	GError *error = NULL;
	const gchar *err_msg = NULL;

	if (USE_GIO_FILE_OPERATIONS)
	{
		GFile *file = g_file_new_for_path(locale_filename);
		GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE, NULL, &error);

		if (info)
		{
			GTimeVal timeval;

			g_file_info_get_modification_time(info, &timeval);
			g_object_unref(info);
			*time = timeval.tv_sec;
		}
		else if (error)
			err_msg = error->message;

		g_object_unref(file);
	}
	else
	{
		GStatBuf st;

		if (g_stat(locale_filename, &st) == 0)
			*time = st.st_mtime;
		else
			err_msg = g_strerror(errno);
	}

	if (err_msg)
	{
		gchar *utf8_filename = utils_get_utf8_from_locale(locale_filename);

		ui_set_statusbar(TRUE, _("Could not open file %s (%s)"),
			utf8_filename, err_msg);
		g_free(utf8_filename);
	}

	if (error)
		g_error_free(error);

	return err_msg == NULL;
}


/* loads textfile data, verifies and converts to forced_enc or UTF-8. Also handles BOM. */
static gboolean load_text_file(const gchar *locale_filename, const gchar *display_filename,
	FileData *filedata, const gchar *forced_enc)
{
	GError *err = NULL;

	filedata->data = NULL;
	filedata->len = 0;
	filedata->enc = NULL;
	filedata->bom = FALSE;
	filedata->readonly = FALSE;

	if (!get_mtime(locale_filename, &filedata->mtime))
		return FALSE;

	if (USE_GIO_FILE_OPERATIONS)
	{
		GFile *file = g_file_new_for_path(locale_filename);

		g_file_load_contents(file, NULL, &filedata->data, &filedata->len, NULL, &err);
		g_object_unref(file);
	}
	else
		g_file_get_contents(locale_filename, &filedata->data, &filedata->len, &err);

	if (err)
	{
		ui_set_statusbar(TRUE, "%s", err->message);
		g_error_free(err);
		return FALSE;
	}

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
		gint pos = sci_get_line_indent_position(sci, line);

		/* We probably don't have style info yet, because we're generally called just after
		 * the document got created, so we can't use highlighting_is_code_style().
		 * That's not good, but the assumption below that concerning lines start with an
		 * asterisk (common continuation character for C/C++/Java/...) should do the trick
		 * without removing too much legitimate lines. */
		if (sci_get_char_at(sci, pos) == '*')
			continue;

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
	UndoReloadData *undo_reload_data;
	gboolean add_undo_reload_action;

	g_return_val_if_fail(doc == NULL || doc->is_valid, NULL);

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
			SETPTR(doc->real_path, utils_get_real_path(locale_filename));

			doc->priv->is_remote = utils_is_remote_path(locale_filename);
			monitor_file_setup(doc);
		}

		if (! reload || ! file_prefs.keep_edit_history_on_reload)
		{
			sci_set_undo_collection(doc->editor->sci, FALSE); /* avoid creation of an undo action */
			sci_empty_undo_buffer(doc->editor->sci);
			undo_reload_data = NULL;
		}
		else
		{
			undo_reload_data = (UndoReloadData*) g_malloc(sizeof(UndoReloadData));

			/* We will be adding a UNDO_RELOAD action to the undo stack that undoes
			 * this reload. To do that, we keep collecting undo actions during
			 * reloading, and at the end add an UNDO_RELOAD action that performs
			 * all these actions in bulk. To keep track of how many undo actions
			 * were added during this time, we compare the current undo-stack height
			 * with its height at the end of the process. Note that g_trash_stack_height()
			 * is O(N), which is a little ugly, but this seems like the most maintainable
			 * option. */
			undo_reload_data->actions_count = g_trash_stack_height(&doc->priv->undo_actions);

			/* We use add_undo_reload_action to track any changes to the document that
			 * require adding an undo action to revert the reload, but that do not
			 * generate an undo action themselves. */
			add_undo_reload_action = FALSE;
		}

		/* add the text to the ScintillaObject */
		sci_set_readonly(doc->editor->sci, FALSE);	/* to allow replacing text */
		sci_set_text(doc->editor->sci, filedata.data);	/* NULL terminated data */
		queue_colourise(doc);	/* Ensure the document gets colourised. */

		/* detect & set line endings */
		editor_mode = utils_get_line_endings(filedata.data, filedata.len);
		if (undo_reload_data)
		{
			undo_reload_data->eol_mode = editor_get_eol_char_mode(doc->editor);
			/* Force adding an undo-reload action if the EOL mode changed. */
			if (editor_mode != undo_reload_data->eol_mode)
				add_undo_reload_action = TRUE;
		}
		sci_set_eol_mode(doc->editor->sci, editor_mode);
		g_free(filedata.data);

		sci_set_undo_collection(doc->editor->sci, TRUE);

		/* If reloading and the current and new encodings or BOM states differ,
		 * add appropriate undo actions. */
		if (undo_reload_data)
		{
			if (! utils_str_equal(doc->encoding, filedata.enc))
				document_undo_add(doc, UNDO_ENCODING, g_strdup(doc->encoding));
			if (doc->has_bom != filedata.bom)
				document_undo_add(doc, UNDO_BOM, GINT_TO_POINTER(doc->has_bom));
		}

		doc->priv->mtime = filedata.mtime; /* get the modification time from file and keep it */
		g_free(doc->encoding);	/* if reloading, free old encoding */
		doc->encoding = filedata.enc;
		doc->has_bom = filedata.bom;
		store_saved_encoding(doc);	/* store the opened encoding for undo/redo */

		doc->readonly = readonly || filedata.readonly;
		sci_set_readonly(doc->editor->sci, doc->readonly);
		doc->priv->protected = 0;

		/* update line number margin width */
		doc->priv->line_count = sci_get_line_count(doc->editor->sci);
		sci_set_line_numbers(doc->editor->sci, editor_prefs.show_linenumber_margin);

		if (! reload)
		{

			/* "the" SCI signal (connect after initial setup(i.e. adding text)) */
			g_signal_connect(doc->editor->sci, "sci-notify", G_CALLBACK(editor_sci_notify_cb),
				doc->editor);

			use_ft = (ft != NULL) ? ft : filetypes_detect_from_document(doc);
		}
		else
		{	/* reloading */
			if (undo_reload_data)
			{
				/* Calculate the number of undo actions that are part of the reloading
				 * process, and add the UNDO_RELOAD action. */
				undo_reload_data->actions_count =
					g_trash_stack_height(&doc->priv->undo_actions) - undo_reload_data->actions_count;

				/* We only add an undo-reload action if the document has actually changed.
				 * At the time of writing, this condition is moot because sci_set_text
				 * generates an undo action even when the text hasn't really changed, so
				 * actions_count is always greater than zero. In the future this might change.
				 * It's arguable whether we should add an undo-reload action unconditionally,
				 * especially since it's possible (if unlikely) that there had only
				 * been "invisible" changes to the document, such as changes in encoding and
				 * EOL mode, but for the time being that's how we roll. */
				if (undo_reload_data->actions_count > 0 || add_undo_reload_action)
					document_undo_add(doc, UNDO_RELOAD, undo_reload_data);
				else
					g_free(undo_reload_data);

				/* We didn't save the document per-se, but its contents are now
				 * synchronized with the file on disk, hence set a save point here.
				 * We need to do this in this case only, because we don't clear
				 * Scintilla's undo stack. */
				sci_set_savepoint(doc->editor->sci);
			}
			else
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
			msgwin_status_add(_("File %s opened (%d%s)."),
				display_filename, gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)),
				(readonly) ? _(", read-only") : "");
		}

		/* now the document is fully ready, display it (see notebook_new_tab()) */
		gtk_widget_show(document_get_notebook_child(doc));
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
	gchar **list;

	g_return_if_fail(data != NULL);

	list = g_strsplit(data, utils_get_eol_char(utils_get_line_endings(data, length)), 0);

	/* stop at the end or first empty item, because last item is empty but not null */
	for (i = 0; list[i] != NULL && list[i][0] != '\0'; i++)
	{
		gchar *filename = utils_get_path_from_uri(list[i]);

		if (filename == NULL)
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
 *  @param filenames @elementtype{filename} A list of filenames to load, in locale encoding.
 *  @param readonly Whether to open the document in read-only mode.
 *  @param ft @nullable The filetype for the document or @c NULL to auto-detect the filetype.
 *  @param forced_enc @nullable The file encoding to use or @c NULL to auto-detect the file encoding.
 **/
GEANY_API_SYMBOL
void document_open_files(const GSList *filenames, gboolean readonly, GeanyFiletype *ft,
		const gchar *forced_enc)
{
	const GSList *item;

	for (item = filenames; item != NULL; item = g_slist_next(item))
	{
		document_open_file(item->data, readonly, ft, forced_enc);
	}
}


static void on_keep_edit_history_on_reload_response(GtkWidget *bar, gint response_id, GeanyDocument *doc)
{
	if (response_id == GTK_RESPONSE_NO)
	{
		file_prefs.keep_edit_history_on_reload = FALSE;
		document_reload_force(doc, doc->encoding);
	}
	else if (response_id == GTK_RESPONSE_CANCEL)
	{
		/* this condition cannot be reached via info bar buttons, but by our code
		 * to replace this bar with a higher priority one */
		file_prefs.show_keep_edit_history_on_reload_msg = TRUE;
	}
	doc->priv->info_bars[MSG_TYPE_POST_RELOAD] = NULL;
	gtk_widget_destroy(bar);
}


/**
 *  Reloads the document with the specified file encoding.
 *  @a forced_enc or @c NULL to auto-detect the file encoding.
 *
 *  @param doc The document to reload.
 *  @param forced_enc @nullable The file encoding to use or @c NULL to auto-detect the file encoding.
 *
 *  @return @c TRUE if the document was actually reloaded or @c FALSE otherwise.
 **/
GEANY_API_SYMBOL
gboolean document_reload_force(GeanyDocument *doc, const gchar *forced_enc)
{
	gint pos = 0;
	GeanyDocument *new_doc;
	GtkWidget *bar;

	g_return_val_if_fail(doc != NULL, FALSE);

	/* Cancel resave bar if still open from previous file deletion */
	if (doc->priv->info_bars[MSG_TYPE_RESAVE] != NULL)
		gtk_info_bar_response(GTK_INFO_BAR(doc->priv->info_bars[MSG_TYPE_RESAVE]), GTK_RESPONSE_CANCEL);

	/* Use cancel because the response handler would call this recursively */
	if (doc->priv->info_bars[MSG_TYPE_RELOAD] != NULL)
		gtk_info_bar_response(GTK_INFO_BAR(doc->priv->info_bars[MSG_TYPE_RELOAD]), GTK_RESPONSE_CANCEL);

	/* try to set the cursor to the position before reloading */
	pos = sci_get_current_position(doc->editor->sci);
	new_doc = document_open_file_full(doc, NULL, pos, doc->readonly, doc->file_type, forced_enc);

	if (file_prefs.keep_edit_history_on_reload && file_prefs.show_keep_edit_history_on_reload_msg)
	{
		bar = document_show_message(doc, GTK_MESSAGE_INFO,
						on_keep_edit_history_on_reload_response,
						GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
						_("Discard history"), GTK_RESPONSE_NO,
						NULL, 0, _("The buffer's previous state is stored in the history and "
						"undoing restores it. You can disable this by discarding the history upon "
						"reload. This message will not be displayed again but "
						"your choice can be changed in the various preferences."),
						_("The file has been reloaded."));
		doc->priv->info_bars[MSG_TYPE_POST_RELOAD] = bar;
		file_prefs.show_keep_edit_history_on_reload_msg = FALSE;
	}

	return (new_doc != NULL);
}


/* also used for reloading when forced_enc is NULL */
gboolean document_reload_prompt(GeanyDocument *doc, const gchar *forced_enc)
{
	gchar *base_name;
	gboolean prompt, result = FALSE;

	g_return_val_if_fail(doc != NULL, FALSE);

	/* No need to reload "untitled" (non-file-backed) documents */
	if (doc->file_name == NULL)
		return FALSE;

	if (forced_enc == NULL)
		forced_enc = doc->encoding;

	base_name = g_path_get_basename(doc->file_name);
	/* don't prompt if edit history is maintained, or if file hasn't been edited at all */
	prompt = !file_prefs.keep_edit_history_on_reload &&
			(doc->changed || (document_can_undo(doc) || document_can_redo(doc)));

	if (!prompt || dialogs_show_question_full(NULL, _("_Reload"), GTK_STOCK_CANCEL,
		doc->changed ? _("Any unsaved changes will be lost.") :
			_("Undo history will be lost."),
		_("Are you sure you want to reload '%s'?"), base_name))
	{
		result = document_reload_force(doc, forced_enc);
		if (forced_enc != NULL)
			ui_update_statusbar(doc, -1);
	}
	g_free(base_name);
	return result;
}


static void document_update_timestamp(GeanyDocument *doc, const gchar *locale_filename)
{
#ifndef USE_GIO_FILEMON
	g_return_if_fail(doc != NULL);

	get_mtime(locale_filename, &doc->priv->mtime); /* get the modification time from file and keep it */
#endif
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

	filebase = g_regex_escape_string(GEANY_STRING_UNTITLED, -1);
	if (doc->file_type->extension)
		SETPTR(filebase, g_strconcat("\\b", filebase, "\\.\\w+", NULL));
	else
		SETPTR(filebase, g_strconcat("\\b", filebase, "\\b", NULL));

	filename = g_path_get_basename(doc->file_name);

	/* only search the first 3 lines */
	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_position_from_line(doc->editor->sci, 4);
	ttf.lpstrText = filebase;

	if (search_find_text(doc->editor->sci, GEANY_FIND_MATCHCASE | GEANY_FIND_REGEXP, &ttf, NULL) != -1)
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
GEANY_API_SYMBOL
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


static void protect_document(GeanyDocument *doc)
{
	/* do not call queue_colourise because to we want to keep the text-changed indication! */
	if (!doc->priv->protected++)
		sci_set_readonly(doc->editor->sci, TRUE);

	ui_update_tab_status(doc);
}


static void unprotect_document(GeanyDocument *doc)
{
	g_return_if_fail(doc->priv->protected > 0);

	if (!--doc->priv->protected && doc->readonly == FALSE)
		sci_set_readonly(doc->editor->sci, FALSE);

	ui_update_tab_status(doc);
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
 * @param utf8_fname @nullable The new name for the document, in UTF-8, or @c NULL.
 * @return @c TRUE if the file was saved or @c FALSE if the file could not be saved.
 *
 * @see document_save_file().
 *
 *  @since 0.16
 **/
GEANY_API_SYMBOL
gboolean document_save_file_as(GeanyDocument *doc, const gchar *utf8_fname)
{
	gboolean ret;
	gboolean new_file;

	g_return_val_if_fail(doc != NULL, FALSE);

	new_file = document_need_save_as(doc) || (utf8_fname != NULL && strcmp(doc->file_name, utf8_fname) != 0);
	if (utf8_fname != NULL)
		SETPTR(doc->file_name, g_strdup(utf8_fname));

	/* reset real path, it's retrieved again in document_save() */
	SETPTR(doc->real_path, NULL);

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

	if (new_file)
	{
		// assume user wants to throw away read-only setting
		sci_set_readonly(doc->editor->sci, FALSE);
		doc->readonly = FALSE;
		if (doc->priv->protected > 0)
			unprotect_document(doc);
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
			gint line, column;
			gint context_len;
			gunichar unic;
			/* don't read over the doc length */
			gint max_len = MIN((gint)bytes_read + 6, (gint)*len - 1);
			gchar context[7]; /* read 6 bytes from Sci + '\0' */
			sci_get_text_range(doc->editor->sci, bytes_read, max_len, context);

			/* take only one valid Unicode character from the context and discard the leftover */
			unic = g_utf8_get_char_validated(context, -1);
			context_len = g_unichar_to_utf8(unic, context);
			context[context_len] = '\0';
			get_line_column_from_pos(doc, bytes_read, &line, &column);

			error_text = g_strdup_printf(
				_("Error message: %s\nThe error occurred at \"%s\" (line: %d, column: %d)."),
				conv_error->message, context, line + 1, column);
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
								 const gchar *data, gsize len)
{
	GError *error = NULL;

	if (file_prefs.use_safe_file_saving)
	{
		/* Use old GLib API for safe saving (GVFS-safe, but alters ownership and permissons).
		 * This is the only option that handles disk space exhaustion. */
		if (g_file_set_contents(locale_filename, data, len, &error))
			geany_debug("Wrote %s with g_file_set_contents().", locale_filename);
	}
	else if (USE_GIO_FILE_OPERATIONS)
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
			gsize bytes_written;

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
								 const gchar *data, gsize len)
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
		doc->real_path = utils_get_real_path(locale_filename);
		doc->priv->is_remote = utils_is_remote_path(locale_filename);
		monitor_file_setup(doc);
	}
	return NULL;
}


static gboolean save_file_handle_infobars(GeanyDocument *doc, gboolean force)
{
	GtkWidget *bar = NULL;

	document_show_tab(doc);

	if (doc->priv->info_bars[MSG_TYPE_RELOAD])
	{
		if (!dialogs_show_question_full(NULL, _("_Overwrite"), GTK_STOCK_CANCEL,
			_("Overwrite?"),
			_("The file '%s' on the disk is more recent than the current buffer."),
			doc->file_name))
			return FALSE;
		bar = doc->priv->info_bars[MSG_TYPE_RELOAD];
	}
	else if (doc->priv->info_bars[MSG_TYPE_RESAVE])
	{
		if (!dialogs_show_question_full(NULL, GTK_STOCK_SAVE, GTK_STOCK_CANCEL,
			_("Try to resave the file?"),
			_("File \"%s\" was not found on disk!"),
			doc->file_name))
			return FALSE;
		bar = doc->priv->info_bars[MSG_TYPE_RESAVE];
	}
	else
	{
		g_assert_not_reached();
		return FALSE;
	}
	gtk_info_bar_response(GTK_INFO_BAR(bar), RESPONSE_DOCUMENT_SAVE);
	return TRUE;
}


/**
 *  Saves the document.
 *  Also shows the Save As dialog if necessary.
 *  If the file is not modified, this function may do nothing unless @a force is set to @c TRUE.
 *
 *  Saving may include replacing tabs with spaces,
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
GEANY_API_SYMBOL
gboolean document_save_file(GeanyDocument *doc, gboolean force)
{
	gchar *errmsg;
	gchar *data;
	gsize len;
	gchar *locale_filename;
	const GeanyFilePrefs *fp;

	g_return_val_if_fail(doc != NULL, FALSE);

	if (document_need_save_as(doc))
	{
		/* ensure doc is the current tab before showing the dialog */
		document_show_tab(doc);
		return dialogs_show_save_as();
	}

	if (!force && !doc->changed)
		return FALSE;
	if (doc->readonly)
	{
		ui_set_statusbar(TRUE,
			_("Cannot save read-only document '%s'!"), DOC_FILENAME(doc));
		return FALSE;
	}
	document_check_disk_status(doc, TRUE);
	if (doc->priv->protected)
		return save_file_handle_infobars(doc, force);

	fp = project_get_file_prefs();
	/* replaces tabs with spaces but only if the current file is not a Makefile */
	if (fp->replace_tabs && doc->file_type->id != GEANY_FILETYPES_MAKE)
		editor_replace_tabs(doc->editor, TRUE);
	/* strip trailing spaces */
	if (fp->strip_trailing_spaces)
		editor_strip_trailing_spaces(doc->editor, TRUE);
	/* ensure the file has a newline at the end */
	if (fp->final_new_line)
		editor_ensure_final_newline(doc->editor);
	/* ensure newlines are consistent */
	if (fp->ensure_convert_new_lines)
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
			SETPTR(errmsg,
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
gboolean document_search_bar_find(GeanyDocument *doc, const gchar *text, gboolean inc,
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
	search_pos = sci_find_text(doc->editor->sci, 0, &ttf);

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
		search_pos = sci_find_text(doc->editor->sci, 0, &ttf);
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
		GeanyFindFlags flags, gboolean search_backwards, GeanyMatchInfo **match_,
		gboolean scroll, GtkWidget *parent)
{
	gint selection_end, selection_start, search_pos;

	g_return_val_if_fail(doc != NULL && text != NULL, -1);
	if (! *text)
		return -1;

	/* Sci doesn't support searching backwards with a regex */
	if (flags & GEANY_FIND_REGEXP)
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
		search_pos = search_find_prev(doc->editor->sci, text, flags, match_);
	else
		search_pos = search_find_next(doc->editor->sci, text, flags, match_);

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
			ret = document_find_text(doc, text, original_text, flags, search_backwards, match_, scroll, parent);
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
		const gchar *replace_text, GeanyFindFlags flags, gboolean search_backwards)
{
	gint selection_end, selection_start, search_pos;
	GeanyMatchInfo *match = NULL;

	g_return_val_if_fail(doc != NULL && find_text != NULL && replace_text != NULL, -1);

	if (! *find_text)
		return -1;

	/* Sci doesn't support searching backwards with a regex */
	if (flags & GEANY_FIND_REGEXP)
		search_backwards = FALSE;

	if (!original_find_text)
		original_find_text = find_text;

	selection_start = sci_get_selection_start(doc->editor->sci);
	selection_end = sci_get_selection_end(doc->editor->sci);
	if (selection_end == selection_start)
	{
		/* no selection so just find the next match */
		document_find_text(doc, find_text, original_find_text, flags, search_backwards, NULL, TRUE, NULL);
		return -1;
	}
	/* there's a selection so go to the start before finding to search through it
	 * this ensures there is a match */
	if (search_backwards)
		sci_goto_pos(doc->editor->sci, selection_end, TRUE);
	else
		sci_goto_pos(doc->editor->sci, selection_start, TRUE);

	search_pos = document_find_text(doc, find_text, original_find_text, flags, search_backwards, &match, TRUE, NULL);
	/* return if the original selected text did not match (at the start of the selection) */
	if (search_pos != selection_start)
	{
		if (search_pos != -1)
			geany_match_info_free(match);
		return -1;
	}

	if (search_pos != -1)
	{
		gint replace_len = search_replace_match(doc->editor->sci, match, replace_text);
		/* select the replacement - find text will skip past the selected text */
		sci_set_selection_start(doc->editor->sci, search_pos);
		sci_set_selection_end(doc->editor->sci, search_pos + replace_len);
		geany_match_info_free(match);
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
	GeanyFindFlags flags, gint start, gint end, gboolean scroll_to_match, gint *new_range_end)
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
						  const gchar *original_find_text, const gchar *original_replace_text, GeanyFindFlags flags)
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
		const gchar *original_find_text, const gchar *original_replace_text, GeanyFindFlags flags)
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
		doc->tm_file = tm_source_file_new(locale_filename, name);
		g_free(locale_filename);

		if (doc->tm_file)
			tm_workspace_add_source_file_noupdate(doc->tm_file);
	}

	/* early out if there's no tm source file and we couldn't create one */
	if (doc->tm_file == NULL)
	{
		/* We must call sidebar_update_tag_list() before returning,
		 * to ensure that the symbol list is always updated properly (e.g.
		 * when creating a new document with a partial filename set. */
		sidebar_update_tag_list(doc, FALSE);
		return;
	}

	/* Parse Scintilla's buffer directly using TagManager
	 * Note: this buffer *MUST NOT* be modified */
	len = sci_get_length(doc->editor->sci);
	buffer_ptr = (guchar *) SSM(doc->editor->sci, SCI_GETCHARACTERPOINTER, 0, 0);
	tm_workspace_update_source_file_buffer(doc->tm_file, buffer_ptr, len);

	sidebar_update_tag_list(doc, TRUE);
	document_highlight_tags(doc);
}


/* Re-highlights type keywords without re-parsing the whole document. */
void document_highlight_tags(GeanyDocument *doc)
{
	GString *keywords_str;
	gint keyword_idx;

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
		case GEANY_FILETYPES_RUST:
		case GEANY_FILETYPES_GO:
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
	if (!app->tm_workspace->tags_array)
		return;

	/* get any type keywords and tell scintilla about them
	 * this will cause the type keywords to be colourized in scintilla */
	keywords_str = symbols_find_typenames_as_string(doc->file_type->lang, FALSE);
	if (keywords_str)
	{
		gchar *keywords = g_string_free(keywords_str, FALSE);
		guint hash = g_str_hash(keywords);

		if (hash != doc->priv->keyword_hash)
		{
			sci_set_keywords(doc->editor->sci, keyword_idx, keywords);
			queue_colourise(doc); /* force re-highlighting the entire document */
			doc->priv->keyword_hash = hash;
		}
		g_free(keywords);
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
			tm_workspace_remove_source_file(doc->tm_file);
			tm_source_file_free(doc->tm_file);
			doc->tm_file = NULL;
		}
		/* load tags files before highlighting (some lexers highlight global typenames) */
		if (type->id != GEANY_FILETYPES_NONE)
			symbols_global_tags_loaded(type->id);

		highlighting_set_styles(doc->editor->sci, type);
		editor_set_indentation_guides(doc->editor);
		build_menu_update(doc);
		queue_colourise(doc);
		if (type->priv->symbol_list_sort_mode == SYMBOLS_SORT_USE_PREVIOUS)
			doc->priv->symbol_list_sort_mode = interface_prefs.symbols_sort_mode;
		else
			doc->priv->symbol_list_sort_mode = type->priv->symbol_list_sort_mode;
	}

	document_update_tags(doc);
}


/** Sets the filetype of the document (which controls syntax highlighting and tags)
 * @param doc The document to use.
 * @param type The filetype. */
GEANY_API_SYMBOL
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
		if ((! old_ft || old_ft->id == GEANY_FILETYPES_NONE) &&
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
GEANY_API_SYMBOL
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

/* Clears an Undo or Redo buffer. */
void document_undo_clear_stack(GTrashStack **stack)
{
	while (g_trash_stack_height(stack) > 0)
	{
		undo_action *a = g_trash_stack_pop(stack);

		if (G_LIKELY(a != NULL))
		{
			switch (a->type)
			{
				case UNDO_ENCODING:
				case UNDO_RELOAD:
					g_free(a->data); break;
				default: break;
			}
			g_free(a);
		}
	}
	*stack = NULL;
}

/* Clears the Undo and Redo buffer (to be called when reloading or closing the document) */
void document_undo_clear(GeanyDocument *doc)
{
	document_undo_clear_stack(&doc->priv->undo_actions);
	document_undo_clear_stack(&doc->priv->redo_actions);

	if (! main_status.quitting && doc->editor != NULL)
		document_set_text_changed(doc, FALSE);
}


/* Adds an undo action without clearing the redo stack. This function should
 * not be called directly, generally (use document_undo_add() instead), but is
 * used by document_redo() in order not to erase the redo stack while moving
 * an action from the redo stack to the undo stack. */
void document_undo_add_internal(GeanyDocument *doc, guint type, gpointer data)
{
	undo_action *action;

	g_return_if_fail(doc != NULL);

	action = g_new0(undo_action, 1);
	action->type = type;
	action->data = data;

	g_trash_stack_push(&doc->priv->undo_actions, action);

	/* avoid unnecessary redraws */
	if (type != UNDO_SCINTILLA || !doc->changed)
		document_set_text_changed(doc, TRUE);

	ui_update_popup_reundo_items(doc);
}

/* note: this is called on SCN_MODIFIED notifications */
void document_undo_add(GeanyDocument *doc, guint type, gpointer data)
{
	/* Clear the redo actions stack before adding the undo action. */
	document_undo_clear_stack(&doc->priv->redo_actions);

	document_undo_add_internal(doc, type, data);
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
				g_free(action->data);

				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);
				break;
			}
			case UNDO_EOL:
			{
				undo_action *next_action;

				document_redo_add(doc, UNDO_EOL, GINT_TO_POINTER(sci_get_eol_mode(doc->editor->sci)));

				sci_set_eol_mode(doc->editor->sci, GPOINTER_TO_INT(action->data));

				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);

				/* When undoing, UNDO_EOL is always followed by UNDO_SCINTILLA
				 * which undos the line endings in the editor and should be
				 * performed together with UNDO_EOL. */
				next_action = g_trash_stack_peek(&doc->priv->undo_actions);
				if (next_action && next_action->type == UNDO_SCINTILLA)
					document_undo(doc);
				break;
			}
			case UNDO_RELOAD:
			{
				UndoReloadData *data = (UndoReloadData*)action->data;
				gint eol_mode = data->eol_mode;
				guint i;

				/* We reuse 'data' for the redo action, so read the current EOL mode
				 * into it before proceeding. */
				data->eol_mode = editor_get_eol_char_mode(doc->editor);

				/* Undo the rest of the actions which are part of the reloading process. */
				for (i = 0; i < data->actions_count; i++)
					document_undo(doc);

				/* Restore the previous EOL mode. */
				sci_set_eol_mode(doc->editor->sci, eol_mode);
				/* This might affect the status bar and document menu, so update them. */
				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);

				document_redo_add(doc, UNDO_RELOAD, data);
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
				undo_action *next_action;

				document_undo_add_internal(doc, UNDO_SCINTILLA, NULL);

				sci_redo(doc->editor->sci);

				/* When redoing an EOL change, the UNDO_SCINTILLA which changes
				 * the line ends in the editor is followed by UNDO_EOL
				 * which should be performed together with UNDO_SCINTILLA. */
				next_action = g_trash_stack_peek(&doc->priv->redo_actions);
				if (next_action != NULL && next_action->type == UNDO_EOL)
					document_redo(doc);
				break;
			}
			case UNDO_BOM:
			{
				document_undo_add_internal(doc, UNDO_BOM, GINT_TO_POINTER(doc->has_bom));

				doc->has_bom = GPOINTER_TO_INT(action->data);
				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);
				break;
			}
			case UNDO_ENCODING:
			{
				document_undo_add_internal(doc, UNDO_ENCODING, g_strdup(doc->encoding));

				document_set_encoding(doc, (const gchar*)action->data);
				g_free(action->data);

				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);
				break;
			}
			case UNDO_EOL:
			{
				document_undo_add_internal(doc, UNDO_EOL, GINT_TO_POINTER(sci_get_eol_mode(doc->editor->sci)));

				sci_set_eol_mode(doc->editor->sci, GPOINTER_TO_INT(action->data));

				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);
				break;
			}
			case UNDO_RELOAD:
			{
				UndoReloadData *data = (UndoReloadData*)action->data;
				gint eol_mode = data->eol_mode;
				guint i;

				/* We reuse 'data' for the undo action, so read the current EOL mode
				 * into it before proceeding. */
				data->eol_mode = editor_get_eol_char_mode(doc->editor);

				/* Redo the rest of the actions which are part of the reloading process. */
				for (i = 0; i < data->actions_count; i++)
					document_redo(doc);

				/* Restore the previous EOL mode. */
				sci_set_eol_mode(doc->editor->sci, eol_mode);
				/* This might affect the status bar and document menu, so update them. */
				ui_update_statusbar(doc, -1);
				ui_document_show_hide(doc);

				document_undo_add_internal(doc, UNDO_RELOAD, data);
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

	if (type != UNDO_SCINTILLA || !doc->changed)
		document_set_text_changed(doc, TRUE);

	ui_update_popup_reundo_items(doc);
}


enum
{
	STATUS_CHANGED,
	STATUS_DISK_CHANGED,
	STATUS_READONLY
};

static struct
{
	const gchar *name;
	GdkColor color;
	gboolean loaded;
} document_status_styles[] = {
	{ "geany-document-status-changed",      {0}, FALSE },
	{ "geany-document-status-disk-changed", {0}, FALSE },
	{ "geany-document-status-readonly",     {0}, FALSE }
};


static gint document_get_status_id(GeanyDocument *doc)
{
	if (doc->changed)
		return STATUS_CHANGED;
#ifdef USE_GIO_FILEMON
	else if (doc->priv->file_disk_status == FILE_CHANGED)
#else
	else if (doc->priv->protected)
#endif
		return STATUS_DISK_CHANGED;
	else if (doc->readonly)
		return STATUS_READONLY;

	return -1;
}


/* returns an identifier that is to be set as a widget name or class to get it styled
 * depending on the document status (changed, readonly, etc.)
 * a NULL return value means default (unchanged) style */
const gchar *document_get_status_widget_class(GeanyDocument *doc)
{
	gint status;

	g_return_val_if_fail(doc != NULL, NULL);

	status = document_get_status_id(doc);
	if (status < 0)
		return NULL;
	else
		return document_status_styles[status].name;
}


/**
 *  Gets the status color of the document, or @c NULL if default widget coloring should be used.
 *  Returned colors are red if the document has changes, green if the document is read-only
 *  or simply @c NULL if the document is unmodified but writable.
 *
 *  @param doc The document to use.
 *
 *  @return @nullable The color for the document or @c NULL if the default color should be used.
 *          The color object is owned by Geany and should not be modified or freed.
 *
 *  @since 0.16
 */
GEANY_API_SYMBOL
const GdkColor *document_get_status_color(GeanyDocument *doc)
{
	gint status;

	g_return_val_if_fail(doc != NULL, NULL);

	status = document_get_status_id(doc);
	if (status < 0)
		return NULL;
	if (! document_status_styles[status].loaded)
	{
		GdkRGBA color;
		GtkWidgetPath *path = gtk_widget_path_new();
		GtkStyleContext *ctx = gtk_style_context_new();
		gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
		gtk_widget_path_append_type(path, GTK_TYPE_BOX);
		gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
		gtk_widget_path_append_type(path, GTK_TYPE_LABEL);
		gtk_widget_path_iter_set_name(path, -1, document_status_styles[status].name);
		gtk_style_context_set_screen(ctx, gtk_widget_get_screen(GTK_WIDGET(doc->editor->sci)));
		gtk_style_context_set_path(ctx, path);
		gtk_style_context_get_color(ctx, gtk_style_context_get_state(ctx), &color);
		document_status_styles[status].color.red   = 0xffff * color.red;
		document_status_styles[status].color.green = 0xffff * color.green;
		document_status_styles[status].color.blue  = 0xffff * color.blue;
		document_status_styles[status].loaded = TRUE;
		gtk_widget_path_unref(path);
		g_object_unref(ctx);
	}
	return &document_status_styles[status].color;
}


/** Accessor function for @ref GeanyData::documents_array items.
 * @warning Always check the returned document is valid (@c doc->is_valid).
 * @param idx @c GeanyData::documents_array index.
 * @return @transfer{none} @nullable The document, or @c NULL if @a idx is out of range.
 *
 *  @since 0.16
 */
GEANY_API_SYMBOL
GeanyDocument *document_index(gint idx)
{
	return (idx >= 0 && idx < (gint) documents_array->len) ? documents[idx] : NULL;
}


GeanyDocument *document_clone(GeanyDocument *old_doc)
{
	gchar *text;
	GeanyDocument *doc;
	ScintillaObject *old_sci;

	g_return_val_if_fail(old_doc, NULL);
	old_sci = old_doc->editor->sci;
	if (sci_has_selection(old_sci))
		text = sci_get_selection_contents(old_sci);
	else
		text = sci_get_contents(old_sci, -1);

	doc = document_new_file(NULL, old_doc->file_type, text);
	g_free(text);
	document_set_text_changed(doc, TRUE);

	/* copy file properties */
	doc->editor->line_wrapping = old_doc->editor->line_wrapping;
	doc->editor->line_breaking = old_doc->editor->line_breaking;
	doc->editor->auto_indent = old_doc->editor->auto_indent;
	editor_set_indent(doc->editor, old_doc->editor->indent_type,
		old_doc->editor->indent_width);
	doc->readonly = old_doc->readonly;
	doc->has_bom = old_doc->has_bom;
	doc->priv->protected = 0;
	document_set_encoding(doc, old_doc->encoding);
	sci_set_lines_wrapped(doc->editor->sci, doc->editor->line_wrapping);
	sci_set_readonly(doc->editor->sci, doc->readonly);

	/* update ui */
	ui_document_show_hide(doc);
	return doc;
}


/* @return TRUE if all files were saved or had their changes discarded. */
gboolean document_account_for_unsaved(void)
{
	guint p, page_count;

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

	return TRUE;
}


static void force_close_all(void)
{
	guint i;

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


/* *
 * Shows a message related to a document.
 *
 * Use this whenever the user needs to see a document-related message,
 * for example when the file was externally modified or deleted.
 *
 * Any of the buttons can be @c NULL.  If not @c NULL, @a btn_1's
 * @a response_1 response will be the default for the @c GtkInfoBar or
 * @c GtkDialog.
 *
 * @param doc @c GeanyDocument.
 * @param msgtype The type of message.
 * @param response_cb A callback function called when there's a response.
 * @param btn_1 The first action area button.
 * @param response_1 The response for @a btn_1.
 * @param btn_2 The second action area button.
 * @param response_2 The response for @a btn_2.
 * @param btn_3 The third action area button.
 * @param response_3 The response for @a btn_3.
 * @param extra_text Text to show below the main message.
 * @param format The text format for the main message.
 * @param ... Used with @a format as in @c printf.
 *
 * @since 1.25
 * */
static GtkWidget* document_show_message(GeanyDocument *doc, GtkMessageType msgtype,
	void (*response_cb)(GtkWidget *info_bar, gint response_id, GeanyDocument *doc),
	const gchar *btn_1, GtkResponseType response_1,
	const gchar *btn_2, GtkResponseType response_2,
	const gchar *btn_3, GtkResponseType response_3,
	const gchar *extra_text, const gchar *format, ...)
{
	va_list args;
	gchar *text, *markup;
	GtkWidget *hbox, *icon, *label, *content_area;
	GtkWidget *info_widget, *parent;
	parent = document_get_notebook_child(doc);

	va_start(args, format);
	text = g_strdup_vprintf(format, args);
	va_end(args);

	markup = g_markup_printf_escaped("<span size=\"larger\">%s</span>", text);
	g_free(text);

	info_widget = gtk_info_bar_new();
	/* must be done now else Gtk-WARNING: widget not within a GtkWindow */
	gtk_box_pack_start(GTK_BOX(parent), info_widget, FALSE, TRUE, 0);

	gtk_info_bar_set_message_type(GTK_INFO_BAR(info_widget), msgtype);

	if (btn_1)
		gtk_info_bar_add_button(GTK_INFO_BAR(info_widget), btn_1, response_1);
	if (btn_2)
		gtk_info_bar_add_button(GTK_INFO_BAR(info_widget), btn_2, response_2);
	if (btn_3)
		gtk_info_bar_add_button(GTK_INFO_BAR(info_widget), btn_3, response_3);

	content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(info_widget));

	label = geany_wrap_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);

	g_signal_connect(info_widget, "response", G_CALLBACK(response_cb), doc);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start(GTK_BOX(content_area), hbox, TRUE, TRUE, 0);

	switch (msgtype)
	{
		case GTK_MESSAGE_INFO:
			icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
			break;
		case GTK_MESSAGE_WARNING:
			icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
			break;
		case GTK_MESSAGE_QUESTION:
			icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
			break;
		case GTK_MESSAGE_ERROR:
			icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
			break;
		default:
			icon = NULL;
			break;
	}

	if (icon)
		gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, TRUE, 0);

	if (extra_text)
	{
		GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
		GtkWidget *extra_label = geany_wrap_label_new(extra_text);

		gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), extra_label, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	}
	else
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

	gtk_box_reorder_child(GTK_BOX(parent), info_widget, 0);

	gtk_widget_show_all(info_widget);

	return info_widget;
}


static void on_monitor_reload_file_response(GtkWidget *bar, gint response_id, GeanyDocument *doc)
{
	gboolean close = FALSE;

	// disable info bar so actions complete normally
	unprotect_document(doc);
	doc->priv->info_bars[MSG_TYPE_RELOAD] = NULL;

	if (response_id == RESPONSE_DOCUMENT_RELOAD)
	{
		close = doc->changed ?
			document_reload_prompt(doc, doc->encoding) :
			document_reload_force(doc, doc->encoding);
	}
	else if (response_id == RESPONSE_DOCUMENT_SAVE)
	{
		close = document_save_file(doc, TRUE);	// force overwrite
	}
	else if (response_id == GTK_RESPONSE_CANCEL)
	{
		document_set_text_changed(doc, TRUE);
		close = TRUE;
	}
	if (!close)
	{
		doc->priv->info_bars[MSG_TYPE_RELOAD] = bar;
		protect_document(doc);
		return;
	}
	gtk_widget_destroy(bar);
}


static gboolean on_sci_key(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	GtkInfoBar *bar = GTK_INFO_BAR(data);

	g_return_val_if_fail(event->type == GDK_KEY_PRESS, FALSE);

	switch (event->keyval)
	{
		case GDK_KEY_Tab:
		case GDK_KEY_ISO_Left_Tab:
		{
			GtkWidget *action_area = gtk_info_bar_get_action_area(bar);
			GtkDirectionType dir = event->keyval == GDK_KEY_Tab ? GTK_DIR_TAB_FORWARD : GTK_DIR_TAB_BACKWARD;
			gtk_widget_child_focus(action_area, dir);
			return TRUE;
		}
		case GDK_KEY_Escape:
		{
			gtk_info_bar_response(bar, GTK_RESPONSE_CANCEL);
			return TRUE;
		}
		default:
			return FALSE;
	}
}


/* Sets up a signal handler to intercept some keys during the lifetime of the GtkInfoBar */
static void enable_key_intercept(GeanyDocument *doc, GtkWidget *bar)
{
	/* automatically focus editor again on bar close */
	g_signal_connect_object(bar, "destroy", G_CALLBACK(gtk_widget_grab_focus), doc->editor->sci,
			G_CONNECT_SWAPPED);
	g_signal_connect_object(doc->editor->sci, "key-press-event", G_CALLBACK(on_sci_key), bar, 0);
}


static void monitor_reload_file(GeanyDocument *doc)
{
	if (! doc->changed && file_prefs.reload_clean_doc_on_file_change)
	{
		document_reload_force(doc, doc->encoding);
		return;
	}

	gchar *base_name = g_path_get_basename(doc->file_name);

	/* show this message only once */
	if (doc->priv->info_bars[MSG_TYPE_RELOAD] == NULL)
	{
		GtkWidget *bar;

		bar = document_show_message(doc, GTK_MESSAGE_QUESTION, on_monitor_reload_file_response,
				_("_Reload"), RESPONSE_DOCUMENT_RELOAD,
				_("_Overwrite"), RESPONSE_DOCUMENT_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("Do you want to reload it?"),
				_("The file '%s' on the disk is more recent than the current buffer."),
				base_name);

		protect_document(doc);
		doc->priv->info_bars[MSG_TYPE_RELOAD] = bar;
		enable_key_intercept(doc, bar);
	}
	g_free(base_name);
}


static void on_monitor_resave_missing_file_response(GtkWidget *bar,
                                                    gint response_id,
                                                    GeanyDocument *doc)
{
	gboolean close = TRUE;

	unprotect_document(doc);

	if (response_id == RESPONSE_DOCUMENT_SAVE)
		close = dialogs_show_save_as();

	if (close)
	{
		doc->priv->info_bars[MSG_TYPE_RESAVE] = NULL;
		gtk_widget_destroy(bar);
	}
	else
	{
		/* protect back the document if save didn't occur */
		protect_document(doc);
	}
}


static void monitor_resave_missing_file(GeanyDocument *doc)
{
	if (doc->priv->info_bars[MSG_TYPE_RESAVE] == NULL)
	{
		GtkWidget *bar = doc->priv->info_bars[MSG_TYPE_RELOAD];

		if (bar != NULL) /* the "file on disk is newer" warning is now moot */
			gtk_info_bar_response(GTK_INFO_BAR(bar), GTK_RESPONSE_CANCEL);

		bar = document_show_message(doc, GTK_MESSAGE_WARNING,
				on_monitor_resave_missing_file_response,
				GTK_STOCK_SAVE, RESPONSE_DOCUMENT_SAVE,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL, GTK_RESPONSE_NONE,
				_("Try to resave the file?"),
				_("File \"%s\" was not found on disk!"),
				doc->file_name);

		protect_document(doc);
		document_set_text_changed(doc, TRUE);
		/* don't prompt more than once */
		SETPTR(doc->real_path, NULL);
		doc->priv->info_bars[MSG_TYPE_RESAVE] = bar;
		enable_key_intercept(doc, bar);
	}
}


/* Set force to force a disk check, otherwise it is ignored if there was a check
 * in the last file_prefs.disk_check_timeout seconds.
 * @return @c TRUE if the file has changed. */
gboolean document_check_disk_status(GeanyDocument *doc, gboolean force)
{
	gboolean ret = FALSE;
	gboolean use_gio_filemon;
	time_t mtime = 0;
	gchar *locale_filename;
	FileDiskStatus old_status;

	g_return_val_if_fail(doc != NULL, FALSE);

	/* ignore remote files and documents that have never been saved to disk */
	if (notebook_switch_in_progress() || file_prefs.disk_check_timeout == 0
			|| doc->real_path == NULL || doc->priv->is_remote)
		return FALSE;

	use_gio_filemon = (doc->priv->monitor != NULL);

	if (use_gio_filemon)
	{
		if (doc->priv->file_disk_status != FILE_CHANGED && ! force)
			return FALSE;
	}
	else
	{
		time_t cur_time = time(NULL);

		if (! force && doc->priv->last_check > (cur_time - file_prefs.disk_check_timeout))
			return FALSE;

		doc->priv->last_check = cur_time;
	}

	locale_filename = utils_get_locale_from_utf8(doc->file_name);
	if (!get_mtime(locale_filename, &mtime))
	{
		monitor_resave_missing_file(doc);
		/* doc may be closed now */
		ret = TRUE;
	}
	else if (doc->priv->mtime < mtime)
	{
		/* make sure the user is not prompted again after he cancelled the "reload file?" message */
		doc->priv->mtime = mtime;
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
GEANY_API_SYMBOL
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
GEANY_API_SYMBOL
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
GEANY_API_SYMBOL
gint document_compare_by_tab_order_reverse(gconstpointer a, gconstpointer b)
{
	return -1 * document_compare_by_tab_order(a, b);
}


void document_grab_focus(GeanyDocument *doc)
{
	g_return_if_fail(doc != NULL);

	gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
}

static void        *copy_(void *src) { return src; }
static void         free_(void *doc) { }

/** @gironly
 * Gets the GType of GeanyDocument
 *
 * @return the GeanyDocument type */
GEANY_API_SYMBOL
GType document_get_type (void);

G_DEFINE_BOXED_TYPE(GeanyDocument, document, copy_, free_);


gpointer document_get_data(const GeanyDocument *doc, const gchar *key)
{
	return g_datalist_get_data(&doc->priv->data, key);
}


void document_set_data(GeanyDocument *doc, const gchar *key, gpointer data)
{
	g_datalist_set_data(&doc->priv->data, key, data);
}


void document_set_data_full(GeanyDocument *doc, const gchar *key,
	gpointer data, GDestroyNotify free_func)
{
	g_datalist_set_data_full(&doc->priv->data, key, data, free_func);
}
