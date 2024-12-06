/*
 *      saveactions.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 The Geany contributors
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "geanyplugin.h"
#include "gtkcompat.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <glib/gstdio.h>


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)

PLUGIN_SET_INFO(_("Save Actions"), _("This plugin provides different actions related to saving of files."),
	PACKAGE_VERSION, _("The Geany developer team"))


enum
{
	NOTEBOOK_PAGE_AUTOSAVE = 0,
	NOTEBOOK_PAGE_BACKUPCOPY,
	NOTEBOOK_PAGE_UNTITLEDDOC
};

enum
{
	NOTEBOOK_UNTITLEDDOC_RADIO_DISABLED = 0,
	NOTEBOOK_UNTITLEDDOC_RADIO_INSTANTSAVE,
	NOTEBOOK_UNTITLEDDOC_RADIO_PERSISTENT
};

static struct
{
	GtkWidget *checkbox_enable_autosave;
	GtkWidget *checkbox_enable_autosave_losing_focus;
	GtkWidget *checkbox_enable_backupcopy;

	GtkWidget *autosave_interval_spin;
	GtkWidget *autosave_print_msg_checkbox;
	GtkWidget *autosave_save_all_radio1;
	GtkWidget *autosave_save_all_radio2;

	GtkWidget *backupcopy_entry_dir;
	GtkWidget *backupcopy_entry_time;
	GtkWidget *backupcopy_spin_dir_levels;

	GtkWidget *untitled_doc_disabled_radio;
	GtkWidget *untitled_doc_instantsave_radio;
	GtkWidget *untitled_doc_persistent_radio;

	GtkWidget *untitled_doc_ft_combo;

	GtkWidget *instantsave_entry_dir;

	GtkWidget *persistent_doc_interval_spin;
	GtkWidget *persistent_doc_entry_dir;
}
pref_widgets;


#define PERSISTENT_UNTITLED_DOC_FILE_NAME_PREFIX "untitled_"


static gboolean enable_autosave;
static gboolean enable_autosave_losing_focus;
static gboolean enable_instantsave;
static gboolean enable_backupcopy;
static gboolean enable_persistent_docs;

static gint autosave_interval;
static gboolean autosave_print_msg;
static gboolean autosave_save_all;
static guint autosave_src_id = 0;

static gchar *backupcopy_backup_dir; /* path to an existing directory in locale encoding */
static gchar *backupcopy_time_fmt;
static gint backupcopy_dir_levels;

static gchar *untitled_doc_default_ft;

static gchar *instantsave_target_dir;

static gint persistent_docs_updater_interval_ms;
static guint persistent_docs_updater_src_id = 0;
static gchar *persistent_docs_target_dir;

static gchar *config_file;

/* all persistent untitled documents should be loaded into session when session is changed
(so after geany launch or when project session is started/closed) */
static gboolean session_is_changing = FALSE;


static gint count_opened_notebook_tabs(void)
{
	return gtk_notebook_get_n_pages(GTK_NOTEBOOK(geany->main_widgets->notebook));
}


static gboolean is_directory_accessible(const gchar *dirpath_locale)
{
	return dirpath_locale != NULL &&
		g_path_is_absolute(dirpath_locale) &&
		g_file_test(dirpath_locale, G_FILE_TEST_EXISTS) &&
		g_file_test(dirpath_locale, G_FILE_TEST_IS_DIR);
}


/* Ensures utf8_dir exists and is writable and
 * set target to the locale encoded form of utf8_dir */
static gboolean store_target_directory(const gchar *utf8_dir, gchar **target)
{
	gchar *tmp;

	if (G_UNLIKELY(EMPTY(utf8_dir)) || target == NULL)
		return FALSE;

	tmp = utils_get_locale_from_utf8(utf8_dir);

	if (! is_directory_accessible(tmp))
	{
		g_free(tmp);
		return FALSE;
	}
	/** TODO add utils_is_file_writeable() to the plugin API and make use of it **/

	SETPTR(*target, tmp);

	return TRUE;
}


static gchar *backupcopy_skip_root(gchar *filename)
{
	/* first skip the root (e.g. c:\ on windows) */
	const gchar *dir = g_path_skip_root(filename);

	/* if this has failed, use the filename again */
	if (dir == NULL)
		dir = filename;
	/* check again for leading / or \ */
	while (*dir == G_DIR_SEPARATOR)
		dir++;

	return (gchar *) dir;
}


static gchar *backupcopy_create_dir_parts(const gchar *filename)
{
	gint cnt_dir_parts = 0;
	gchar *cp;
	gchar *dirname;
	gchar last_char = 0;
	gint error;
	gchar *result;
	gchar *target_dir;

	if (backupcopy_dir_levels == 0)
		return g_strdup("");

	dirname = g_path_get_dirname(filename);

	cp = dirname;
	/* walk to the end of the string */
	while (*cp != '\0')
		cp++;

	/* walk backwards to find directory parts */
	while (cp > dirname)
	{
		if (*cp == G_DIR_SEPARATOR && last_char != G_DIR_SEPARATOR)
			cnt_dir_parts++;

		if (cnt_dir_parts == backupcopy_dir_levels)
			break;

		last_char = *cp;
		cp--;
	}

	result = backupcopy_skip_root(cp); /* skip leading slash/backslash and c:\ */
	target_dir = g_build_filename(backupcopy_backup_dir, result, NULL);

	error = utils_mkdir(target_dir, TRUE);
	if (error != 0)
	{
		ui_set_statusbar(FALSE, _("Backup Copy: Directory could not be created (%s)."),
			g_strerror(error));

		result = g_strdup(""); /* return an empty string in case of an error */
	}
	else
		result = g_strdup(result);

	g_free(dirname);
	g_free(target_dir);

	return result;
}


static void backupcopy_document_save_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	FILE *src, *dst;
	gchar *locale_filename_src;
	gchar *locale_filename_dst;
	gchar *basename_src;
	gchar *dir_parts_src;
	gchar *stamp;
	gchar buf[512];
	gint fd_dst = -1;

	locale_filename_src = utils_get_locale_from_utf8(doc->file_name);

	if ((src = g_fopen(locale_filename_src, "r")) == NULL)
	{
		/* it's unlikely that this happens */
		ui_set_statusbar(FALSE, _("Backup Copy: File could not be read (%s)."),
			g_strerror(errno));
		g_free(locale_filename_src);
		return;
	}

	stamp = utils_get_date_time(backupcopy_time_fmt, NULL);
	basename_src = g_path_get_basename(locale_filename_src);
	dir_parts_src = backupcopy_create_dir_parts(locale_filename_src);
	locale_filename_dst = g_strconcat(
		backupcopy_backup_dir, G_DIR_SEPARATOR_S,
		dir_parts_src, G_DIR_SEPARATOR_S,
		basename_src, ".", stamp, NULL);
	g_free(basename_src);
	g_free(dir_parts_src);

#ifdef G_OS_WIN32
	if ((dst = g_fopen(locale_filename_dst, "wb")) == NULL)
#else
	/* Use g_open() on non-Windows to set file permissions to 600 atomically.
	 * On Windows, seting file permissions would require specific Windows API. */
	fd_dst = g_open(locale_filename_dst, O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR);
	if (fd_dst == -1 || (dst = fdopen(fd_dst, "w")) == NULL)
#endif
	{
		ui_set_statusbar(FALSE, _("Backup Copy: File could not be saved (%s)."),
			g_strerror(errno));
		g_free(locale_filename_src);
		g_free(locale_filename_dst);
		g_free(stamp);
		fclose(src);
		if (fd_dst != -1)
			close(fd_dst);
		return;
	}

	while (fgets(buf, sizeof(buf), src) != NULL)
	{
		fputs(buf, dst);
	}

	fclose(src);
	fclose(dst);
	if (fd_dst != -1)
		close(fd_dst);
	g_free(locale_filename_src);
	g_free(locale_filename_dst);
	g_free(stamp);
}


static GeanyFiletype *get_doc_filetype(GeanyDocument *doc)
{
	GeanyFiletype *ft = doc->file_type;

	if (ft == NULL || ft->id == GEANY_FILETYPES_NONE)
		/* ft is NULL when a new file without template was opened, so use the
			* configured default file type */
		ft = filetypes_lookup_by_name(untitled_doc_default_ft);

	return ft;
}


static void instantsave_document_new(GeanyDocument *doc)
{
	if (doc->real_path == NULL)
	{
		const gchar *directory;
		gchar *new_filename;
		gint fd;
		GeanyFiletype *ft = get_doc_filetype(doc);

		/* construct filename */
		directory = !EMPTY(instantsave_target_dir) ? instantsave_target_dir : g_get_tmp_dir();
		new_filename = g_build_filename(directory, "gis_XXXXXX", NULL);
		if (ft != NULL && !EMPTY(ft->extension))
			SETPTR(new_filename, g_strconcat(new_filename, ".", ft->extension, NULL));

		/* create new file */
		fd = g_mkstemp(new_filename);
		if (fd == -1)
		{
			gchar *message = g_strdup_printf(
				_("Instant Save filename could not be generated (%s)."), g_strerror(errno));
			ui_set_statusbar(TRUE, "%s", message);
			g_warning("%s", message);
			g_free(message);
			g_free(new_filename);
			return;
		}

		close(fd); /* close the returned file descriptor as we only need the filename */

		doc->file_name = new_filename;

		if (ft != NULL)
			document_set_filetype(doc, ft);

		/* force saving the file to enable all the related actions(tab name, filetype, etc.) */
		document_save_file(doc, TRUE);
	}
}


static gboolean is_persistent_doc_file_name(const gchar *filename)
{
	if (filename == NULL)
		return FALSE;

	return g_str_has_prefix(filename, PERSISTENT_UNTITLED_DOC_FILE_NAME_PREFIX);
}


static gboolean is_persistent_doc_file_path(const gchar *file_path_utf8)
{
	gchar *filename, *dirname, *file_path_locale;
	gboolean matched;

	if (file_path_utf8 == NULL)
		return FALSE;

	file_path_locale = utils_get_locale_from_utf8(file_path_utf8);
	dirname = g_path_get_dirname(file_path_locale);
	matched = g_str_equal(dirname, persistent_docs_target_dir);

	g_free(file_path_locale);
	g_free(dirname);

	if (!matched)
		return FALSE;

	filename = g_path_get_basename(file_path_utf8);
	matched = is_persistent_doc_file_name(filename);

	g_free(filename);

	return matched;
}


static gchar* create_new_persistent_doc_file_name(GeanyDocument *doc, GeanyFiletype *filetype)
{
	gint i;
	gchar *extension_postfix;

	if (filetype != NULL && !EMPTY(filetype->extension))
		extension_postfix = g_strconcat(".", filetype->extension, NULL);
	else
		extension_postfix = g_strdup("");

	for (i = 1; i < 1000; i++)
	{
		gchar *next_file_name, *next_file_path;
		gboolean file_exists;

		next_file_name = g_strdup_printf("%s%d%s", PERSISTENT_UNTITLED_DOC_FILE_NAME_PREFIX, i, extension_postfix);
		next_file_path = g_strdup_printf("%s%c%s", persistent_docs_target_dir, G_DIR_SEPARATOR, next_file_name);

		file_exists = g_file_test(next_file_path, G_FILE_TEST_EXISTS);

		g_free(next_file_path);

		if (file_exists)
		{
			g_free(next_file_name);
		}
		else
		{
			g_free(extension_postfix);

			return next_file_name;
		}
	}

	g_free(extension_postfix);

	return NULL;
}


static void persistent_doc_new(GeanyDocument *doc)
{
	if (doc->real_path == NULL)
	{
		gchar *files_dir_utf8, *new_file_name_utf8, *new_file_path_utf8;
		GeanyFiletype *ft;

		if (EMPTY(persistent_docs_target_dir))
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Persistent untitled document directory does not exist or is not writable."));
			return;
		}

	 	ft = get_doc_filetype(doc);
		new_file_name_utf8 = create_new_persistent_doc_file_name(doc, ft);

		if (ft != NULL)
			document_set_filetype(doc, ft);

		if (new_file_name_utf8 == NULL)
		{
			return;
		}

		files_dir_utf8 = utils_get_utf8_from_locale(persistent_docs_target_dir);

		new_file_path_utf8 = g_strconcat(files_dir_utf8,
			G_DIR_SEPARATOR_S, new_file_name_utf8, NULL);

		document_save_file_as(doc, new_file_path_utf8);

		g_free(new_file_name_utf8);
		g_free(files_dir_utf8);
		g_free(new_file_path_utf8);
	}
}


static gboolean document_is_empty(GeanyDocument *doc)
{
	/* if total text length is 1 while line count is 2 - then the only character of whole text is a linebreak,
	which is how completely empty document saved by Geany to disk looks like */
	return sci_get_length(doc->editor->sci) == 0
		|| (sci_get_length(doc->editor->sci) == 1 && sci_get_line_count(doc->editor->sci) == 2);
}


/* Open document in idle - useful when trying to re-open document from 'document-close' callback
 *
 * @param pointer to document path (locale-encoded)
 *
 * @return always FALSE = Just a one shot execution
 *
 */
static gboolean open_document_once_idle(gpointer p_locale_file_path)
{
	gchar *locale_file_path = (gchar *) p_locale_file_path;

	document_open_file(locale_file_path, FALSE, NULL, NULL);

	g_free(locale_file_path);

	return FALSE;
}


static gint run_dialog_for_persistent_doc_tab_closing(const gchar *msg, const gchar *msg2)
{
	GtkWidget *dialog, *button;
	gint ret;

	dialog = gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", msg);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", msg2);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = ui_button_new_with_image(GTK_STOCK_CLEAR, _("_Don't save (discard)"));
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_NO);
	gtk_widget_show(button);

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	return ret;
}


static void show_dialog_for_persistent_doc_tab_closing(
	GeanyDocument *doc, const gchar *short_filename)
{
	gchar *msg, *old_file_path_locale;
	const gchar *msg2;
	gint response;

	msg = g_strdup_printf(_("Untitled document %s is not saved."), short_filename);
	msg2 = _("Do you want to save it?");

	response = run_dialog_for_persistent_doc_tab_closing(msg, msg2);
	g_free(msg);

	switch (response)
	{
		case GTK_RESPONSE_YES:
			old_file_path_locale = g_strdup(doc->real_path);

			if (dialogs_show_save_as())
			{
				if (! g_str_equal(old_file_path_locale, doc->real_path))
				{
					/* remove untitled doc file if it was saved as some other file */
					g_remove(old_file_path_locale);
				}
			}
			else
			{
				plugin_idle_add(geany_plugin, open_document_once_idle, g_strdup(old_file_path_locale));
			}

			g_free(old_file_path_locale);
			break;

		case GTK_RESPONSE_NO:
			g_remove(doc->real_path);

			ui_set_statusbar(TRUE, _("Untitled document file %s was deleted"), short_filename);
			break;

		case GTK_RESPONSE_CANCEL:
		default:
			plugin_idle_add(geany_plugin, open_document_once_idle, g_strdup(doc->real_path));
			break;
	}
}


static void persistent_doc_before_save_as_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (enable_persistent_docs)
	{
		gchar *old_file_path_utf8 = DOC_FILENAME(doc);

		if (is_persistent_doc_file_path(old_file_path_utf8))
		{
			/* we have to store old filename inside document data to be able to somehow
			pass it to document-save callback that is called directly after this one */
			plugin_set_document_data_full(geany_plugin, doc, "file-name-before-save-as",
				g_strdup(old_file_path_utf8), g_free);
		}
	}
}


static void persistent_doc_save_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	gchar *new_file_path_utf8, *old_file_path_utf8;

	new_file_path_utf8 = DOC_FILENAME(doc);
	old_file_path_utf8 = plugin_get_document_data(geany_plugin, doc, "file-name-before-save-as");

	if (old_file_path_utf8 != NULL)
	{
		if (is_persistent_doc_file_path(old_file_path_utf8)
			&& ! g_str_equal(old_file_path_utf8, new_file_path_utf8))
		{
			/* remove untitled doc file if it was saved as some other file */
			gchar *locale_old_file_path = utils_get_locale_from_utf8(old_file_path_utf8);
			g_remove(locale_old_file_path);

			g_free(locale_old_file_path);

			msgwin_status_add(_("Untitled document file %s was deleted"), old_file_path_utf8);
		}

		plugin_set_document_data(geany_plugin, doc, "file-name-before-save-as", NULL); /* clear value */
	}
}


static void load_all_persistent_docs_into_editor(void)
{
	GDir *dir;
	GError *error = NULL;
	const gchar *filename;

	dir = g_dir_open(persistent_docs_target_dir, 0, &error);
	if (dir == NULL)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Persistent untitled document directory not found"));
		return;
	}

	foreach_dir(filename, dir)
	{
		if (is_persistent_doc_file_name(filename))
		{
			gchar *locale_file_path, *file_path_utf8;
			GeanyDocument *doc;

			locale_file_path = g_build_path(G_DIR_SEPARATOR_S, persistent_docs_target_dir, filename, NULL);
			file_path_utf8 = utils_get_utf8_from_locale(locale_file_path);
			doc = document_find_by_filename(file_path_utf8);

			g_free(file_path_utf8);

			if (doc == NULL)
				doc = document_open_file(locale_file_path, FALSE, NULL, NULL);

			g_free(locale_file_path);

			/* we are closing (and thus deleting) empty documents here - mainly in order to avoid accumulation of
			empty untitled document files, that happens when new tab with empty document is created at new (empty) session start.
			Note: we cannot 'close' newly-created empty document from 'document-activate' callback,
			so this is a perfect place for it */
			if (doc != NULL && document_is_empty(doc))
				document_close(doc);
		}
	}

	g_dir_close(dir);

	/* create new empty file/tab if this is a "fresh" session start without any opened files */
	if (count_opened_notebook_tabs() == 0)
		document_new_file(NULL, NULL, NULL);
}


static gboolean load_all_persistent_docs_idle(gpointer data)
{
	load_all_persistent_docs_into_editor();

	return FALSE;
}


static gboolean reload_persistent_docs_on_session_change_idle(gpointer data)
{
	/* remember and re-open document from originaly focused tab
	(after we mess selected tab with re-loaded untitled doc files) */
	GeanyDocument *current_doc = document_get_current();

	load_all_persistent_docs_into_editor();

	if (current_doc != NULL && current_doc->real_path != NULL)
		document_open_file(current_doc->real_path, FALSE, NULL, NULL);

	return FALSE;
}


/* Save when focus out
 *
 * @param pointer ref to the current doc (struct GeanyDocument *)
 *
 * @return always FALSE = Just a one shot execution
 *
 */
static gboolean save_on_focus_out_idle(gpointer p_cur_doc)
{
	GeanyDocument *cur_doc = p_cur_doc;

	if (DOC_VALID(cur_doc) && (cur_doc->file_name != NULL))
		document_save_file(cur_doc, FALSE);

	return FALSE;
}


/* Autosave the current file when the focus out of the _editor_
 *
 * Get the SCN_FOCUSOUT signal, and then ask plugin_idle_add()
 * to save the current doc when idle
 *
 * @return always FALSE = Non block signals
 *
 */
static gboolean on_editor_notify(GObject *object, GeanyEditor *editor,
								 SCNotification *nt, gpointer data)
{
	GeanyDocument *doc = editor->document;

	if (nt->nmhdr.code == SCN_FOCUSOUT
		&& doc->file_name != NULL)
	{
		if (enable_autosave_losing_focus || (enable_persistent_docs
											&& doc->real_path != NULL
											&& is_persistent_doc_file_path(doc->file_name)))
		{
			plugin_idle_add(geany_plugin, save_on_focus_out_idle, doc);
		}
	}
	else if (nt->nmhdr.code == SCN_MODIFIED &&
		(nt->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)))
	{
		/* Untitled non-empty documents modified for the first time - empty
		 * ones are handled by on_document_new() */
		if (!doc->real_path && doc->changed)
		{
			if (enable_instantsave)
				instantsave_document_new(doc);

			if (enable_persistent_docs)
				persistent_doc_new(doc);
		}
	}

	return FALSE;
}


static void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (enable_backupcopy)
		backupcopy_document_save_cb(obj, doc, user_data);

	if (enable_persistent_docs)
		persistent_doc_save_cb(obj, doc, user_data);
}


static void on_document_new(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	/* We are only interested in empty new documents here. Other documents
	 * such as templates are handled in on_editor_notify(). */
	if (!document_is_empty(doc))
		return;

	if (enable_instantsave)
		instantsave_document_new(doc);

	if (enable_persistent_docs)
		persistent_doc_new(doc);
}


static void on_project_open(G_GNUC_UNUSED GObject *obj, G_GNUC_UNUSED GKeyFile *config,
							G_GNUC_UNUSED gpointer data)
{
	session_is_changing = TRUE;
}


static void on_project_before_close(void)
{
	session_is_changing = TRUE;
}


static void on_document_activate(G_GNUC_UNUSED GObject *obj, GeanyDocument *doc,
								 G_GNUC_UNUSED gpointer data)
{
	if (session_is_changing)
	{
		session_is_changing = FALSE;

		if (enable_persistent_docs)
			plugin_idle_add(geany_plugin, reload_persistent_docs_on_session_change_idle, NULL);
	}
}


static void persistent_doc_close_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (! enable_persistent_docs)
	{
		return;
	}

	if (! geany_is_closing_all_documents() && doc->real_path != NULL && is_persistent_doc_file_path(doc->file_name))
	{
		gchar *short_filename = document_get_basename_for_display(doc, -1);

		if (! document_is_empty(doc))
		{
			show_dialog_for_persistent_doc_tab_closing(
				doc,
				short_filename
			);
		}
		else
		{
			g_remove(doc->real_path);

			msgwin_status_add(_("Empty untitled document file %s was deleted"), short_filename);
		}

		g_free(short_filename);

	}
	else if (geany_is_closing_all_documents() && count_opened_notebook_tabs() == 1)
	{
		/* if this is a last file being closed during 'close all' - reopen all untitled doc files to keep them in editor */
		plugin_idle_add(geany_plugin, load_all_persistent_docs_idle, NULL);
	}
}


PluginCallback plugin_callbacks[] =
{
	{ "document-new", (GCallback) &on_document_new, FALSE, NULL },
	{ "document-close", (GCallback) &persistent_doc_close_cb, FALSE, NULL },
	{ "document-before-save-as", (GCallback) &persistent_doc_before_save_as_cb, FALSE, NULL },
	{ "document-save", (GCallback) &on_document_save, FALSE, NULL },
	{ "document-activate", (GCallback) &on_document_activate, FALSE, NULL },
	{ "editor-notify", (GCallback) &on_editor_notify, FALSE, NULL },
	{ "project-open", (GCallback) &on_project_open, FALSE, NULL },
	{ "project-before-close", (GCallback) &on_project_before_close, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


static gboolean auto_save(gpointer data)
{
	GeanyDocument *doc;
	GeanyDocument *cur_doc = document_get_current();
	gint i, max = count_opened_notebook_tabs();
	gint saved_files = 0;

	if (cur_doc == NULL)
		return TRUE;

	if (autosave_save_all)
	{
		for (i = 0; i < max; i++)
		{
			doc = document_get_from_page(i);

			/* skip current file (save it last), skip files without name */
			if (doc != cur_doc && doc->file_name != NULL)
				if (document_save_file(doc, FALSE))
					saved_files++;
		}
	}
	/* finally save current file, do it after all other files to get correct window title and
	 * symbol list */
	if (cur_doc->file_name != NULL)
		if (document_save_file(cur_doc, FALSE))
			saved_files++;

	if (saved_files > 0 && autosave_print_msg)
		ui_set_statusbar(FALSE, ngettext(
			"Autosave: Saved %d file automatically.",
			"Autosave: Saved %d files automatically.", saved_files),
			saved_files);

	return TRUE;
}


static void autosave_set_timeout(void)
{
	if (autosave_src_id != 0)
		g_source_remove(autosave_src_id);

	if (! enable_autosave)
		return;

	autosave_src_id = g_timeout_add(autosave_interval * 1000, (GSourceFunc) auto_save, NULL);
}


static gboolean persistent_doc_files_update(gpointer data)
{
	gint i, max = count_opened_notebook_tabs();

	for (i = 0; i < max; i++)
	{
		GeanyDocument *doc = document_get_from_page(i);

		if (doc->real_path != NULL && is_persistent_doc_file_path(doc->file_name))
		{
			document_save_file(doc, FALSE);
		}
	}

	return TRUE;
}


static void persistent_doc_files_updater_set_timeout(void)
{
	if (persistent_docs_updater_src_id != 0)
		g_source_remove(persistent_docs_updater_src_id);

	if (! enable_persistent_docs)
		return;

	persistent_docs_updater_src_id = g_timeout_add(
		persistent_docs_updater_interval_ms,
		(GSourceFunc) persistent_doc_files_update,
		NULL
	);
}


static void write_config_file_updates(GKeyFile *config)
{
	gchar *str, *config_dir;

	config_dir = g_path_get_dirname(config_file);

	if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
	}
	else
	{
		str = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(config_file, str);
		g_free(str);
	}

	g_free(config_dir);
}


void plugin_init(GeanyData *data)
{
	GKeyFile *config = g_key_file_new();
	gchar *tmp;

	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins",
		G_DIR_SEPARATOR_S, "saveactions", G_DIR_SEPARATOR_S, "saveactions.conf", NULL);

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	enable_autosave = utils_get_setting_boolean(
		config, "saveactions", "enable_autosave", FALSE);
	enable_autosave_losing_focus = utils_get_setting_boolean(
		config, "saveactions", "enable_autosave_losing_focus", FALSE);
	enable_backupcopy = utils_get_setting_boolean(
		config, "saveactions", "enable_backupcopy", FALSE);
	enable_instantsave = utils_get_setting_boolean(
		config, "saveactions", "enable_instantsave", FALSE);
	enable_persistent_docs = utils_get_setting_boolean(
		config, "saveactions", "enable_persistent_untitled_documents", FALSE);

	if (enable_instantsave && enable_persistent_docs)
	{
		enable_instantsave = FALSE;
	}

	autosave_src_id = 0; /* mark as invalid */
	autosave_interval = utils_get_setting_integer(config, "autosave", "interval", 300);
	autosave_print_msg = utils_get_setting_boolean(config, "autosave", "print_messages", FALSE);
	autosave_save_all = utils_get_setting_boolean(config, "autosave", "save_all", FALSE);

	autosave_set_timeout();

	backupcopy_dir_levels = utils_get_setting_integer(config, "backupcopy", "dir_levels", 0);
	backupcopy_time_fmt = utils_get_setting_string(
		config, "backupcopy", "time_fmt", "%Y-%m-%d-%H-%M-%S");
	tmp = utils_get_setting_string(config, "backupcopy", "backup_dir", g_get_tmp_dir());
	store_target_directory(tmp, &backupcopy_backup_dir);
	g_free(tmp);

	untitled_doc_default_ft = utils_get_setting_string(config, "untitled_document_save", "default_ft",
		filetypes[GEANY_FILETYPES_NONE]->name);

	tmp = utils_get_setting_string(config, "instantsave", "target_dir", NULL);
	store_target_directory(tmp, &instantsave_target_dir);
	g_free(tmp);

	/* START Persistent untitled documents */
	if (utils_get_setting_string(config, "untitled_document_save", "persistent_untitled_documents_target_dir", NULL) == NULL)
	{
		/* Set default target dir */
		gchar *configdir_utf8, *default_persistent_doc_dir_utf8;

		configdir_utf8 = utils_get_utf8_from_locale(geany->app->configdir);
		default_persistent_doc_dir_utf8 = g_strconcat(configdir_utf8, G_DIR_SEPARATOR_S, "plugins",
			G_DIR_SEPARATOR_S, "saveactions", G_DIR_SEPARATOR_S, "persistent_untitled_documents", NULL);
		g_free(configdir_utf8);

		g_key_file_set_string(config, "untitled_document_save", "persistent_untitled_documents_target_dir",
			default_persistent_doc_dir_utf8);

		tmp = utils_get_locale_from_utf8(default_persistent_doc_dir_utf8);
		g_free(default_persistent_doc_dir_utf8);

		utils_mkdir(tmp, TRUE);
		g_free(tmp);
	}

	tmp = utils_get_setting_string(config, "untitled_document_save", "persistent_untitled_documents_target_dir", NULL);
	SETPTR(tmp, utils_get_locale_from_utf8(tmp));
	/* Set target dir variable with value from config, regardless if dir is valid or not */
	SETPTR(persistent_docs_target_dir, g_strdup(tmp));

	if (enable_persistent_docs && ! is_directory_accessible(tmp))
	{
		/* switch functionality off, so invalid target directory cannot be actually used */
		enable_persistent_docs = FALSE;
		g_key_file_set_boolean(config, "saveactions", "enable_persistent_untitled_documents", FALSE);

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.untitled_doc_disabled_radio), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.untitled_doc_persistent_radio), FALSE);

		ui_set_statusbar(TRUE, "ERROR: persistent untitled documents disabled - bad target directory '%s'", tmp);
	}
	g_free(tmp);

	persistent_docs_updater_src_id = 0; /* mark as invalid */
	persistent_docs_updater_interval_ms = utils_get_setting_integer(config,
		"untitled_document_save", "persistent_untitled_documents_interval_ms", 1000);

	persistent_doc_files_updater_set_timeout();
	/* END Persistent untitled documents */

	session_is_changing = TRUE;

	write_config_file_updates(config);

	g_key_file_free(config);
}


static gint file_chooser_run(GtkFileChooser *dialog)
{
	if (GTK_IS_NATIVE_DIALOG(dialog))
		return gtk_native_dialog_run(GTK_NATIVE_DIALOG(dialog));
	else
		return gtk_dialog_run(GTK_DIALOG(dialog));
}


static void file_chooser_destroy(GtkFileChooser *dialog)
{
	if (GTK_IS_NATIVE_DIALOG(dialog))
		g_object_unref(dialog);
	else
		gtk_widget_destroy(GTK_WIDGET(dialog));
}


static void target_directory_button_clicked_cb(GtkButton *button, gpointer item)
{
	GtkFileChooser *dialog;
	gchar *text;

	/* initialize the dialog */
	if (geany_data->interface_prefs->use_native_windows_dialogs)
		dialog = GTK_FILE_CHOOSER(gtk_file_chooser_native_new(_("Select Directory"),
			GTK_WINDOW(geany_data->main_widgets->window),
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL, NULL));
	else
		dialog = GTK_FILE_CHOOSER(gtk_file_chooser_dialog_new(_("Select Directory"),
						NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL));

	text = utils_get_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(item)));
	if (!EMPTY(text))
		gtk_file_chooser_set_current_folder(dialog, text);

	/* run it */
	if (file_chooser_run(dialog) == GTK_RESPONSE_ACCEPT)
	{
		gchar *utf8_filename, *tmp;

		tmp = gtk_file_chooser_get_filename(dialog);
		utf8_filename = utils_get_utf8_from_locale(tmp);

		gtk_entry_set_text(GTK_ENTRY(item), utf8_filename);

		g_free(utf8_filename);
		g_free(tmp);
	}

	file_chooser_destroy(dialog);
}


static void configure_response_cb(GtkDialog *dialog, gint response, G_GNUC_UNUSED gpointer data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		const gchar *backupcopy_text_dir, *instantsave_text_dir, *text_time;
		gchar *persistent_docs_text_dir;

		enable_autosave = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_autosave));
		enable_autosave_losing_focus = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_autosave_losing_focus));
		enable_instantsave = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.untitled_doc_instantsave_radio));
		enable_backupcopy = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_backupcopy));

		autosave_interval = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.autosave_interval_spin));
		autosave_print_msg = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.autosave_print_msg_checkbox));
		autosave_save_all = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.autosave_save_all_radio2));

		backupcopy_text_dir = gtk_entry_get_text(GTK_ENTRY(pref_widgets.backupcopy_entry_dir));
		text_time = gtk_entry_get_text(GTK_ENTRY(pref_widgets.backupcopy_entry_time));
		backupcopy_dir_levels = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.backupcopy_spin_dir_levels));

		g_free(untitled_doc_default_ft);
		untitled_doc_default_ft = gtk_combo_box_text_get_active_text(
			GTK_COMBO_BOX_TEXT(pref_widgets.untitled_doc_ft_combo));

		instantsave_text_dir = gtk_entry_get_text(GTK_ENTRY(pref_widgets.instantsave_entry_dir));

		persistent_docs_updater_interval_ms = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.persistent_doc_interval_spin));
		persistent_docs_text_dir = g_strdup(
			gtk_entry_get_text(GTK_ENTRY(pref_widgets.persistent_doc_entry_dir))
		);

		g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

		g_key_file_set_boolean(config, "saveactions", "enable_autosave", enable_autosave);
		g_key_file_set_boolean(config, "saveactions", "enable_autosave_losing_focus", enable_autosave_losing_focus);
		g_key_file_set_boolean(config, "saveactions", "enable_instantsave", enable_instantsave);
		g_key_file_set_boolean(config, "saveactions", "enable_backupcopy", enable_backupcopy);

		g_key_file_set_boolean(config, "autosave", "print_messages", autosave_print_msg);
		g_key_file_set_boolean(config, "autosave", "save_all", autosave_save_all);
		g_key_file_set_integer(config, "autosave", "interval", autosave_interval);

		g_key_file_set_integer(config, "backupcopy", "dir_levels", backupcopy_dir_levels);
		g_key_file_set_string(config, "backupcopy", "time_fmt", text_time);
		SETPTR(backupcopy_time_fmt, g_strdup(text_time));
		if (enable_backupcopy)
		{
			if (!EMPTY(backupcopy_text_dir) && store_target_directory(
					backupcopy_text_dir, &backupcopy_backup_dir))
			{
				g_key_file_set_string(config, "backupcopy", "backup_dir", backupcopy_text_dir);
			}
			else
			{
				dialogs_show_msgbox(GTK_MESSAGE_ERROR,
						_("Backup directory does not exist or is not writable."));
			}
		}

		if (untitled_doc_default_ft != NULL)
			g_key_file_set_string(config, "untitled_document_save", "default_ft", untitled_doc_default_ft);

		if (enable_instantsave)
		{
			if (EMPTY(instantsave_text_dir))
			{
				g_key_file_set_string(config, "instantsave", "target_dir", "");
				SETPTR(instantsave_target_dir, NULL);
			}
			else if (store_target_directory(instantsave_text_dir, &instantsave_target_dir))
			{
				g_key_file_set_string(config, "instantsave", "target_dir", instantsave_target_dir);
			}
			else
			{
				dialogs_show_msgbox(GTK_MESSAGE_ERROR,
						_("Instantsave directory does not exist or is not writable."));
			}
		}

		g_key_file_set_integer(config, "untitled_document_save", "persistent_untitled_documents_interval_ms", persistent_docs_updater_interval_ms);
		/* If radio button (not boolean variable, which is not updated yet at this moment) is active
			- we check for target dir validity */
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.untitled_doc_persistent_radio)))
		{
			if (!EMPTY(persistent_docs_text_dir)
					&& g_str_has_suffix(persistent_docs_text_dir, G_DIR_SEPARATOR_S))
			{
				persistent_docs_text_dir[strlen(persistent_docs_text_dir) - 1] = '\0';
			}

			if (!EMPTY(persistent_docs_text_dir) && store_target_directory(
					persistent_docs_text_dir, &persistent_docs_target_dir))
			{
				/* If target dir is valid - we save both itself and "enabled" feature toggle into config file */
				g_key_file_set_string(config, "untitled_document_save", "persistent_untitled_documents_target_dir", persistent_docs_text_dir);

				enable_persistent_docs = TRUE;
				g_key_file_set_boolean(config, "saveactions", "enable_persistent_untitled_documents", TRUE);
			}
			else
			{
				/* If target dir is not valid - we prevent dialog closing and avoid saving both "target dir"
				and "enabled" settings into config file */
				g_signal_stop_emission_by_name(dialog, "response");

				dialogs_show_msgbox(GTK_MESSAGE_ERROR,
						_("Persistent untitled document directory does not exist or is not writable."));
			}
		}
		else
		{
			enable_persistent_docs = FALSE;
			g_key_file_set_boolean(config, "saveactions", "enable_persistent_untitled_documents", FALSE);
		}

		persistent_doc_files_updater_set_timeout();

		autosave_set_timeout();

		write_config_file_updates(config);

		g_key_file_free(config);
		g_free(persistent_docs_text_dir);
	}
}


static void checkbox_toggled_cb(GtkToggleButton *tb, gpointer data)
{
	gboolean enable = gtk_toggle_button_get_active(tb);

	switch (GPOINTER_TO_INT(data))
	{
		case NOTEBOOK_PAGE_AUTOSAVE:
		{
			gtk_widget_set_sensitive(pref_widgets.autosave_interval_spin, enable);
			gtk_widget_set_sensitive(pref_widgets.autosave_print_msg_checkbox, enable);
			gtk_widget_set_sensitive(pref_widgets.autosave_save_all_radio1, enable);
			gtk_widget_set_sensitive(pref_widgets.autosave_save_all_radio2, enable);
			break;
		}
		case NOTEBOOK_PAGE_BACKUPCOPY:
		{
			gtk_widget_set_sensitive(pref_widgets.backupcopy_entry_dir, enable);
			gtk_widget_set_sensitive(pref_widgets.backupcopy_entry_time, enable);
			gtk_widget_set_sensitive(pref_widgets.backupcopy_spin_dir_levels, enable);
			break;
		}
	}
}


static void radio_toggled_cb(GtkRadioButton *rb, gpointer data)
{
	gboolean enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rb));

	switch (GPOINTER_TO_INT(data))
	{
		case NOTEBOOK_UNTITLEDDOC_RADIO_DISABLED:
		{
			if (enable)
			{
				gtk_widget_set_sensitive(pref_widgets.instantsave_entry_dir, FALSE);

				gtk_widget_set_sensitive(pref_widgets.persistent_doc_entry_dir, FALSE);
				gtk_widget_set_sensitive(pref_widgets.persistent_doc_interval_spin, FALSE);

				gtk_widget_set_sensitive(pref_widgets.untitled_doc_ft_combo, FALSE);
			}
			break;
		}
		case NOTEBOOK_UNTITLEDDOC_RADIO_INSTANTSAVE:
		{
			if (enable)
			{
				gtk_widget_set_sensitive(pref_widgets.instantsave_entry_dir, TRUE);

				gtk_widget_set_sensitive(pref_widgets.persistent_doc_entry_dir, FALSE);
				gtk_widget_set_sensitive(pref_widgets.persistent_doc_interval_spin, FALSE);

				gtk_widget_set_sensitive(pref_widgets.untitled_doc_ft_combo, TRUE);
			}
			break;
		}
		case NOTEBOOK_UNTITLEDDOC_RADIO_PERSISTENT:
		{
			if (enable)
			{
				gtk_widget_set_sensitive(pref_widgets.instantsave_entry_dir, FALSE);

				gtk_widget_set_sensitive(pref_widgets.persistent_doc_entry_dir, TRUE);
				gtk_widget_set_sensitive(pref_widgets.persistent_doc_interval_spin, TRUE);

				gtk_widget_set_sensitive(pref_widgets.untitled_doc_ft_combo, TRUE);
			}
			break;
		}
	}
}


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox, *label, *notebook_vbox, *checkbox_enable;
	GtkWidget *notebook, *inner_vbox;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	notebook = gtk_notebook_new();
	gtk_widget_set_can_focus(notebook, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 5);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, TRUE, 0);

	/*
	 * Auto Save
	 */
	{
		GtkWidget *spin, *hbox, *checkbox, *checkbox_enable_as_lf, *radio1, *radio2;

		notebook_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Auto Save")), NOTEBOOK_PAGE_AUTOSAVE);

		checkbox_enable_as_lf = gtk_check_button_new_with_mnemonic(_("Enable save when losing _focus"));
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_enable_as_lf), FALSE);
		pref_widgets.checkbox_enable_autosave_losing_focus = checkbox_enable_as_lf;
		gtk_box_pack_start(GTK_BOX(inner_vbox), checkbox_enable_as_lf, FALSE, FALSE, 6);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_enable_as_lf), enable_autosave_losing_focus);

		checkbox_enable = gtk_check_button_new_with_mnemonic(_("_Enable"));
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_enable), FALSE);
		pref_widgets.checkbox_enable_autosave = checkbox_enable;
		gtk_box_pack_start(GTK_BOX(inner_vbox), checkbox_enable, FALSE, FALSE, 6);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_enable), enable_autosave);
		g_signal_connect(checkbox_enable, "toggled",
			G_CALLBACK(checkbox_toggled_cb), GINT_TO_POINTER(NOTEBOOK_PAGE_AUTOSAVE));

		label = gtk_label_new_with_mnemonic(_("Auto save _interval:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 6);

		pref_widgets.autosave_interval_spin = spin = gtk_spin_button_new_with_range(1, 1800, 1);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), autosave_interval);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);

		label = gtk_label_new(_("seconds"));

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
		gtk_box_pack_start(GTK_BOX(hbox), spin, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 5);

		checkbox = gtk_check_button_new_with_mnemonic(
			_("_Print status message if files have been automatically saved"));
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), autosave_print_msg);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), checkbox);
		gtk_box_pack_start(GTK_BOX(inner_vbox), checkbox, FALSE, FALSE, 5);
		pref_widgets.autosave_print_msg_checkbox = checkbox;

		radio1 = gtk_radio_button_new_with_mnemonic(NULL,
			_("Save only current open _file"));
		pref_widgets.autosave_save_all_radio1 = radio1;
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), radio1);
		gtk_button_set_focus_on_click(GTK_BUTTON(radio1), FALSE);
		gtk_container_add(GTK_CONTAINER(inner_vbox), radio1);

		radio2 = gtk_radio_button_new_with_mnemonic_from_widget(
			GTK_RADIO_BUTTON(radio1), _("Sa_ve all open files"));
		pref_widgets.autosave_save_all_radio2 = radio2;
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), radio2);
		gtk_button_set_focus_on_click(GTK_BUTTON(radio2), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), autosave_save_all);
		gtk_container_add(GTK_CONTAINER(inner_vbox), radio2);
	}
	/*
	 * Backup Copy
	 */
	{
		GtkWidget *hbox, *entry_dir, *entry_time, *button, *image, *spin_dir_levels;

		notebook_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Backup Copy")), NOTEBOOK_PAGE_BACKUPCOPY);

		checkbox_enable = gtk_check_button_new_with_mnemonic(_("_Enable"));
		pref_widgets.checkbox_enable_backupcopy = checkbox_enable;
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_enable), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_enable), enable_backupcopy);
		gtk_box_pack_start(GTK_BOX(inner_vbox), checkbox_enable, FALSE, FALSE, 6);
		g_signal_connect(checkbox_enable, "toggled",
			G_CALLBACK(checkbox_toggled_cb), GINT_TO_POINTER(NOTEBOOK_PAGE_BACKUPCOPY));

		label = gtk_label_new_with_mnemonic(_("_Directory to save backup files in:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		pref_widgets.backupcopy_entry_dir = entry_dir = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_dir);
		if (!EMPTY(backupcopy_backup_dir))
			gtk_entry_set_text(GTK_ENTRY(entry_dir), backupcopy_backup_dir);

		button = gtk_button_new();
		g_signal_connect(button, "clicked",
			G_CALLBACK(target_directory_button_clicked_cb), entry_dir);

		image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(button), image);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_box_pack_start(GTK_BOX(hbox), entry_dir, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);

		label = gtk_label_new_with_mnemonic(
			_("Date/_Time format for backup files (see https://docs.gtk.org/glib/method.DateTime.format.html):"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 7);

		pref_widgets.backupcopy_entry_time = entry_time = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_time);
		if (!EMPTY(backupcopy_time_fmt))
			gtk_entry_set_text(GTK_ENTRY(entry_time), backupcopy_time_fmt);
		gtk_box_pack_start(GTK_BOX(inner_vbox), entry_time, FALSE, FALSE, 0);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

		label = gtk_label_new_with_mnemonic(
			_("Directory _levels to include in the backup destination:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

		spin_dir_levels = gtk_spin_button_new_with_range(0, 20, 1);
		pref_widgets.backupcopy_spin_dir_levels = spin_dir_levels;
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_dir_levels), backupcopy_dir_levels);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin_dir_levels);
		gtk_box_pack_start(GTK_BOX(hbox), spin_dir_levels, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 7);
	}
	/*
	 * Untitled Document Save
	 */
	{
		GtkWidget *disabled_radio, *instantsave_radio, *persistent_radio, *hbox, *spin,
			*entry_dir, *button, *image, *combo, *help_label;
		guint i;
		const GSList *node;
		gchar *entry_dir_label_text;

		notebook_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Untitled Document Save")), NOTEBOOK_PAGE_UNTITLEDDOC);

		disabled_radio = gtk_radio_button_new_with_mnemonic(NULL, _("Disabled"));
		pref_widgets.untitled_doc_disabled_radio = disabled_radio;
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), disabled_radio);
		gtk_button_set_focus_on_click(GTK_BUTTON(disabled_radio), FALSE);
		gtk_container_add(GTK_CONTAINER(inner_vbox), disabled_radio);
		g_signal_connect(disabled_radio, "toggled",
			G_CALLBACK(radio_toggled_cb), GINT_TO_POINTER(NOTEBOOK_UNTITLEDDOC_RADIO_DISABLED));

		/* Instantsave */

		instantsave_radio = gtk_radio_button_new_with_mnemonic_from_widget(
			GTK_RADIO_BUTTON(disabled_radio), _("Instant Save After Creation"));
		pref_widgets.untitled_doc_instantsave_radio = instantsave_radio;
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), instantsave_radio);
		gtk_button_set_focus_on_click(GTK_BUTTON(instantsave_radio), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(instantsave_radio), enable_instantsave);
		gtk_container_add(GTK_CONTAINER(inner_vbox), instantsave_radio);
		g_signal_connect(instantsave_radio, "toggled",
			G_CALLBACK(radio_toggled_cb), GINT_TO_POINTER(NOTEBOOK_UNTITLEDDOC_RADIO_INSTANTSAVE));

		entry_dir_label_text = g_strdup_printf(
			_("_Directory to save files in (leave empty to use the default: %s):"), g_get_tmp_dir());
		label = gtk_label_new_with_mnemonic(entry_dir_label_text);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_widget_set_margin_left(label, 12);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);
		g_free(entry_dir_label_text);

		pref_widgets.instantsave_entry_dir = entry_dir = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_dir);
		if (!EMPTY(instantsave_target_dir))
			gtk_entry_set_text(GTK_ENTRY(entry_dir), instantsave_target_dir);

		button = gtk_button_new();
		g_signal_connect(button, "clicked",
			G_CALLBACK(target_directory_button_clicked_cb), entry_dir);

		image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(button), image);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(hbox), entry_dir, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_widget_set_margin_left(hbox, 12);
		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);

		help_label = gtk_label_new(
			_("<i>The plugin will not delete the files created in this directory.</i>"));
		gtk_label_set_use_markup(GTK_LABEL(help_label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(help_label), 0, 0.5);
		gtk_widget_set_margin_left(help_label, 12);
		gtk_widget_set_margin_bottom(help_label, 8);
		gtk_box_pack_start(GTK_BOX(inner_vbox), help_label, FALSE, FALSE, 0);

		/* Persistent Untitled Documents */

		persistent_radio = gtk_radio_button_new_with_mnemonic_from_widget(
			GTK_RADIO_BUTTON(disabled_radio), _("Persistent Untitled Documents"));
		pref_widgets.untitled_doc_persistent_radio = persistent_radio;
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), persistent_radio);
		gtk_button_set_focus_on_click(GTK_BUTTON(persistent_radio), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(persistent_radio), enable_persistent_docs);
		gtk_container_add(GTK_CONTAINER(inner_vbox), persistent_radio);
		g_signal_connect(persistent_radio, "toggled",
			G_CALLBACK(radio_toggled_cb), GINT_TO_POINTER(NOTEBOOK_UNTITLEDDOC_RADIO_PERSISTENT));

		label = gtk_label_new_with_mnemonic(_("_Directory to save persistent untitled documents in:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_widget_set_margin_left(label, 12);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		pref_widgets.persistent_doc_entry_dir = entry_dir = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_dir);
		if (!EMPTY(persistent_docs_target_dir))
			 gtk_entry_set_text(GTK_ENTRY(entry_dir), persistent_docs_target_dir);

		button = gtk_button_new();
		g_signal_connect(button, "clicked",
			G_CALLBACK(target_directory_button_clicked_cb), entry_dir);

		image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(button), image);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		gtk_box_pack_start(GTK_BOX(hbox), entry_dir, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		gtk_widget_set_margin_left(hbox, 12);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
		label = gtk_label_new_with_mnemonic(_("Untitled document save _interval:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
		gtk_widget_set_margin_left(hbox, 12);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 5);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
		pref_widgets.persistent_doc_interval_spin = spin = gtk_spin_button_new_with_range(1, 600000, 100);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), persistent_docs_updater_interval_ms);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);

		label = gtk_label_new(_("milliseconds"));

		gtk_box_pack_start(GTK_BOX(hbox), spin, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
		gtk_widget_set_margin_left(hbox, 12);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);

		/* Common */

		label = gtk_label_new_with_mnemonic(_("Default _filetype to use for untitled documents:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_widget_set_margin_top(label, 15);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		pref_widgets.untitled_doc_ft_combo = combo = gtk_combo_box_text_new();
		i = 0;
		foreach_slist(node, filetypes_get_sorted_by_name())
		{
			GeanyFiletype *ft = node->data;

			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), ft->name);

			if (utils_str_equal(ft->name, untitled_doc_default_ft))
				gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
			i++;
		}
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 3);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
		gtk_box_pack_start(GTK_BOX(inner_vbox), combo, FALSE, FALSE, 0);
	}

	/* manually emit the toggled signal of the enable checkboxes to update the widget sensitivity */
	g_signal_emit_by_name(pref_widgets.checkbox_enable_autosave, "toggled");
	g_signal_emit_by_name(pref_widgets.checkbox_enable_backupcopy, "toggled");
	g_signal_emit_by_name(pref_widgets.untitled_doc_disabled_radio, "toggled");
	g_signal_emit_by_name(pref_widgets.untitled_doc_instantsave_radio, "toggled");
	g_signal_emit_by_name(pref_widgets.untitled_doc_persistent_radio, "toggled");

	gtk_widget_show_all(vbox);
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);

	return vbox;
}


void plugin_cleanup(void)
{
	if (autosave_src_id != 0)
		g_source_remove(autosave_src_id);

	g_free(backupcopy_backup_dir);
	g_free(backupcopy_time_fmt);

	g_free(untitled_doc_default_ft);

	g_free(instantsave_target_dir);

	if (persistent_docs_updater_src_id != 0)
		g_source_remove(persistent_docs_updater_src_id);

	g_free(persistent_docs_target_dir);

	g_free(config_file);
}
