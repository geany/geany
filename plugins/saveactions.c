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
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
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
#include <errno.h>
#include <glib/gstdio.h>


GeanyPlugin		*geany_plugin;
GeanyData		*geany_data;


GHashTable		*persistent_temp_new_old_file_relation_table;


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)

PLUGIN_SET_INFO(_("Save Actions"), _("This plugin provides different actions related to saving of files."),
	PACKAGE_VERSION, _("The Geany developer team"))


enum
{
	NOTEBOOK_PAGE_AUTOSAVE = 0,
	NOTEBOOK_PAGE_INSTANTSAVE,
	NOTEBOOK_PAGE_BACKUPCOPY,
	NOTEBOOK_PAGE_PERSISTENTTEMPFILES
};

static struct
{
	GtkWidget *checkbox_enable_autosave;
	GtkWidget *checkbox_enable_autosave_losing_focus;
	GtkWidget *checkbox_enable_instantsave;
	GtkWidget *checkbox_enable_backupcopy;
	GtkWidget *checkbox_enable_persistent_temp_files;

	GtkWidget *autosave_interval_spin;
	GtkWidget *autosave_print_msg_checkbox;
	GtkWidget *autosave_save_all_radio1;
	GtkWidget *autosave_save_all_radio2;

	GtkWidget *instantsave_ft_combo;
	GtkWidget *instantsave_entry_dir;

	GtkWidget *backupcopy_entry_dir;
	GtkWidget *backupcopy_entry_time;
	GtkWidget *backupcopy_spin_dir_levels;

	GtkWidget *persistent_temp_files_interval_spin;
	GtkWidget *persistent_temp_files_entry_dir;
}
pref_widgets;


static gboolean enable_autosave;
static gboolean enable_autosave_losing_focus;
static gboolean enable_instantsave;
static gboolean enable_backupcopy;
static gboolean enable_persistent_temp_files;

static gint autosave_interval;
static gboolean autosave_print_msg;
static gboolean autosave_save_all;
static guint autosave_src_id = 0;

static gchar *instantsave_default_ft;
static gchar *instantsave_target_dir;

static gchar *backupcopy_backup_dir; /* path to an existing directory in locale encoding */
static gchar *backupcopy_time_fmt;
static gint backupcopy_dir_levels;

static gint persistent_temp_files_interval_ms;
static guint persistent_temp_files_saver_src_id = 0;
static gchar *persistent_temp_files_target_dir;

static gchar *config_file;


/* Ensures utf8_dir exists and is writable and
 * set target to the locale encoded form of utf8_dir */
static gboolean store_target_directory(const gchar *utf8_dir, gchar **target)
{
	gchar *tmp;

	if (G_UNLIKELY(EMPTY(utf8_dir)) || target == NULL)
		return FALSE;

	tmp = utils_get_locale_from_utf8(utf8_dir);

	if (! g_path_is_absolute(tmp) ||
		! g_file_test(tmp, G_FILE_TEST_EXISTS) ||
		! g_file_test(tmp, G_FILE_TEST_IS_DIR))
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

	if (! enable_backupcopy)
		return;

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


static void save_new_document_to_disk(GeanyDocument *doc,
	const gchar *directory, const gchar *filename_pattern, const gchar *default_ft)
{
	gchar *new_filename;
	gint fd;
	GeanyFiletype *ft = doc->file_type;

	if (ft == NULL || ft->id == GEANY_FILETYPES_NONE)
		/* ft is NULL when a new file without template was opened, so use the
		* configured default file type */
		ft = filetypes_lookup_by_name(default_ft);

	/* construct filename */
	new_filename = g_build_filename(directory, filename_pattern, NULL);
	if (ft != NULL && !EMPTY(ft->extension))
		SETPTR(new_filename, g_strconcat(new_filename, ".", ft->extension, NULL));

	/* create new file */
	fd = g_mkstemp(new_filename);
	if (fd == -1)
	{
		gchar *message = g_strdup_printf(
			_("Filename could not be generated (%s)."), g_strerror(errno));
		ui_set_statusbar(TRUE, "%s", message);
		g_warning("%s", message);
		g_free(message);
		g_free(new_filename);
		return;
	}

	close(fd); /* close the returned file descriptor as we only need the filename */

	doc->file_name = new_filename;

	if (ft != NULL && ft->id == GEANY_FILETYPES_NONE)
		document_set_filetype(doc, filetypes_lookup_by_name(default_ft));

	/* force saving the file to enable all the related actions(tab name, filetype, etc.) */
	document_save_file(doc, TRUE);
}


static void instantsave_document_new_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (enable_instantsave && doc->file_name == NULL)
	{
		save_new_document_to_disk(
			doc,
			!EMPTY(instantsave_target_dir) ? instantsave_target_dir : g_get_tmp_dir(),
			_("gis_XXXXXX"),
			instantsave_default_ft
		);
	}
}


static void persistent_temp_files_document_new_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (enable_persistent_temp_files && doc->file_name == NULL)
	{
		if (EMPTY(persistent_temp_files_target_dir))
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Persistent temp files directory does not exist or is not writable."));
			return;
		}

		save_new_document_to_disk(
			doc,
			persistent_temp_files_target_dir,
			_("gtemp_XXXXXX"),
			filetypes[GEANY_FILETYPES_NONE]->name
		);
	}
}


static gboolean is_temp_saved_file(const gchar *filename)
{
	static const gchar *temp_saved_file_prefix = "gtemp_";

	return g_str_has_prefix(filename, temp_saved_file_prefix);
}


static gboolean is_temp_saved_file_doc(GeanyDocument *doc)
{
	gchar *file_path, *filename;
	gboolean matched;

	file_path = doc->file_name;

	if (file_path == NULL)
	{
		return FALSE;
	}

	filename = g_path_get_basename(file_path);
	matched = is_temp_saved_file(filename);

	g_free(filename);

	return matched;
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


static gint run_unsaved_dialog_for_persistent_temp_files_tab_closing(const gchar *msg, const gchar *msg2)
{
	GtkWidget *dialog, *button;
	gint ret;

	dialog = gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", msg);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", msg2);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = ui_button_new_with_image(GTK_STOCK_CLEAR, _("_Don't save"));
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_NO);
	gtk_widget_show(button);

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	return ret;
}


static void show_unsaved_dialog_for_persistent_temp_files_tab_closing(GeanyDocument *doc, 
	const gchar *file_path, const gchar *locale_file_path, const gchar *short_filename)
{
	gchar *msg, *old_file_path, *new_file_path;
	const gchar *msg2;
	gint response;

	msg = g_strdup_printf(_("The file '%s' is not saved."), short_filename);
	msg2 = _("Do you want to save it before closing?");

	response = run_unsaved_dialog_for_persistent_temp_files_tab_closing(msg, msg2);
	g_free(msg);

	switch (response)
	{
		case GTK_RESPONSE_YES:
			old_file_path = g_strdup(file_path);

			if (dialogs_show_save_as())
			{
				new_file_path = doc->file_name;

				if (! g_str_equal(old_file_path, new_file_path))
				{
				 	//remove temp file if it was saved as some other file
					g_remove(locale_file_path);
				}
			}
			else
			{
				plugin_idle_add(geany_plugin, open_document_once_idle, g_strdup(locale_file_path));
			}

			g_free(old_file_path);
			break;

		case GTK_RESPONSE_NO:
			g_remove(locale_file_path);

			ui_set_statusbar(TRUE, _("Temp file %s was deleted"), short_filename);
			break;

		case GTK_RESPONSE_CANCEL:
		default:
			plugin_idle_add(geany_plugin, open_document_once_idle, g_strdup(locale_file_path));
			break;
	}
}


static void persistent_temp_files_document_close_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	gchar *file_path, *locale_file_path, *short_filename;

	file_path = doc->file_name;

	if (enable_persistent_temp_files && file_path != NULL && ! geany_is_quitting())
	{
		if (is_temp_saved_file_doc(doc))
		{
			short_filename = document_get_basename_for_display(doc, -1);
			locale_file_path = utils_get_locale_from_utf8(file_path);

			if (! document_is_empty(doc))
			{
				show_unsaved_dialog_for_persistent_temp_files_tab_closing(
					doc,
					file_path,
					locale_file_path,
					short_filename
				);
			}
			else
			{
				g_remove(locale_file_path);

				ui_set_statusbar(TRUE, _("Empty temp file %s was deleted"), short_filename);
			}

			g_free(short_filename);
			g_free(locale_file_path);
		}
	}
}


static void persistent_temp_files_document_before_save_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (enable_persistent_temp_files)
	{
		gchar *old_file_path, *new_file_path, *old_file_name;
		GtkWidget *current_page, *page_label;
		GtkNotebook *notebook;
		gint current_page_num;

		notebook = GTK_NOTEBOOK(geany->main_widgets->notebook);
		current_page_num = gtk_notebook_get_current_page(notebook);

		current_page = gtk_notebook_get_nth_page(notebook, current_page_num);
		page_label = gtk_notebook_get_tab_label(notebook, current_page);

		old_file_path = gtk_widget_get_tooltip_text(page_label);
		new_file_path = DOC_FILENAME(doc);

		if (old_file_path == NULL)
		{
			ui_set_statusbar(TRUE, _("plugin error: failed to delete initial temp file "
				"('failed to get notebook tab label')"));

			return;
		}

		old_file_name = g_path_get_basename(old_file_path);

		if (is_temp_saved_file(old_file_name) && ! g_str_equal(old_file_path, new_file_path))
		{
			g_hash_table_insert(
				persistent_temp_new_old_file_relation_table,
				g_strdup(new_file_path),
				g_strdup(old_file_path)
			);
		}

		g_free(old_file_name);
	}
}


static void persistent_temp_files_document_save_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (enable_persistent_temp_files)
	{
		gchar *new_file_path, *old_file_path;

		new_file_path = DOC_FILENAME(doc);
		old_file_path = g_hash_table_lookup(persistent_temp_new_old_file_relation_table, new_file_path);

		if (old_file_path != NULL)
		{
			gchar *old_file_name = g_path_get_basename(old_file_path);

			if (is_temp_saved_file(old_file_name) && ! g_str_equal(old_file_path, new_file_path))
			{
				//remove temp file if it was saved as some other file
				gchar *locale_old_file_path = utils_get_locale_from_utf8(old_file_path);
				g_remove(locale_old_file_path);

				g_free(locale_old_file_path);

				ui_set_statusbar(TRUE, _("Temp file %s was deleted"), old_file_path);
			}

			g_free(old_file_name);

			g_hash_table_remove(persistent_temp_new_old_file_relation_table, new_file_path);
		}
	}
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
static gboolean on_document_focus_out(GObject *object, GeanyEditor *editor,
								 SCNotification *nt, gpointer data)
{
	GeanyDocument *doc;

	doc = editor->document;

	if (nt->nmhdr.code == SCN_FOCUSOUT
		&& doc->file_name != NULL)
	{
		if (enable_autosave_losing_focus || (enable_persistent_temp_files && is_temp_saved_file_doc(doc)))
		{
			plugin_idle_add(geany_plugin, save_on_focus_out_idle, doc);
		}
	}

	return FALSE;
}


PluginCallback plugin_callbacks[] =
{
	{ "document-new", (GCallback) &instantsave_document_new_cb, FALSE, NULL },
	{ "document-new", (GCallback) &persistent_temp_files_document_new_cb, FALSE, NULL },
	{ "document-close", (GCallback) &persistent_temp_files_document_close_cb, FALSE, NULL },
	{ "document-before-save", (GCallback) &persistent_temp_files_document_before_save_cb, FALSE, NULL },
	{ "document-save", (GCallback) &persistent_temp_files_document_save_cb, FALSE, NULL },
	{ "document-save", (GCallback) &backupcopy_document_save_cb, FALSE, NULL },
	{ "editor-notify", (GCallback) &on_document_focus_out, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


static gboolean auto_save(gpointer data)
{
	GeanyDocument *doc;
	GeanyDocument *cur_doc = document_get_current();
	gint i, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(geany->main_widgets->notebook));
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


static gboolean persistent_temp_files_save(gpointer data)
{
	GeanyDocument *doc;
	gint i, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(geany->main_widgets->notebook));

	for (i = 0; i < max; i++)
	{
		doc = document_get_from_page(i);

		if (is_temp_saved_file_doc(doc))
		{
			document_save_file(doc, FALSE);
		}
	}

	return TRUE;
}

static void persistent_temp_files_saver_set_timeout(void)
{
	if (persistent_temp_files_saver_src_id != 0)
		g_source_remove(persistent_temp_files_saver_src_id);

	if (! enable_persistent_temp_files)
		return;

	persistent_temp_files_saver_src_id = g_timeout_add(
		persistent_temp_files_interval_ms,
		(GSourceFunc) persistent_temp_files_save,
		NULL
	);
}


void plugin_init(GeanyData *data)
{
	GKeyFile *config = g_key_file_new();
	gchar *tmp, *default_persistent_temp_files_dir;

	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins",
		G_DIR_SEPARATOR_S, "saveactions", G_DIR_SEPARATOR_S, "saveactions.conf", NULL);

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

	enable_autosave = utils_get_setting_boolean(
		config, "saveactions", "enable_autosave", FALSE);
	enable_autosave_losing_focus = utils_get_setting_boolean(
		config, "saveactions", "enable_autosave_losing_focus", FALSE);
	enable_instantsave = utils_get_setting_boolean(
		config, "saveactions", "enable_instantsave", FALSE);
	enable_backupcopy = utils_get_setting_boolean(
		config, "saveactions", "enable_backupcopy", FALSE);
	enable_persistent_temp_files = utils_get_setting_boolean(
		config, "saveactions", "enable_persistent_temp_files", FALSE);

	instantsave_default_ft = utils_get_setting_string(config, "instantsave", "default_ft",
		filetypes[GEANY_FILETYPES_NONE]->name);
	tmp = utils_get_setting_string(config, "instantsave", "target_dir", NULL);
	store_target_directory(tmp, &instantsave_target_dir);
	g_free(tmp);

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

	default_persistent_temp_files_dir = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins",
		G_DIR_SEPARATOR_S, "saveactions", G_DIR_SEPARATOR_S, "persistent_temp_files", NULL);
	tmp = utils_get_setting_string(config, "persistent_temp_files", "target_dir", default_persistent_temp_files_dir);
	g_free(default_persistent_temp_files_dir);
	store_target_directory(tmp, &persistent_temp_files_target_dir);
	g_free(tmp);

	persistent_temp_files_saver_src_id = 0; /* mark as invalid */
	persistent_temp_files_interval_ms = utils_get_setting_integer(config, "persistent_temp_files", "interval_ms", 1000);

	persistent_temp_files_saver_set_timeout();

	persistent_temp_new_old_file_relation_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

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
		gchar *str;
		const gchar *backupcopy_text_dir, *instantsave_text_dir, *persistent_temp_files_text_dir, *text_time;
		gchar *config_dir = g_path_get_dirname(config_file);

		enable_autosave = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_autosave));
		enable_autosave_losing_focus = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_autosave_losing_focus));
		enable_instantsave = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_instantsave));
		enable_backupcopy = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_backupcopy));
		enable_persistent_temp_files = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_persistent_temp_files));

		autosave_interval = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.autosave_interval_spin));
		autosave_print_msg = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.autosave_print_msg_checkbox));
		autosave_save_all = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.autosave_save_all_radio2));

		g_free(instantsave_default_ft);
		instantsave_default_ft = gtk_combo_box_text_get_active_text(
			GTK_COMBO_BOX_TEXT(pref_widgets.instantsave_ft_combo));
		instantsave_text_dir = gtk_entry_get_text(GTK_ENTRY(pref_widgets.instantsave_entry_dir));

		backupcopy_text_dir = gtk_entry_get_text(GTK_ENTRY(pref_widgets.backupcopy_entry_dir));
		text_time = gtk_entry_get_text(GTK_ENTRY(pref_widgets.backupcopy_entry_time));
		backupcopy_dir_levels = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.backupcopy_spin_dir_levels));

		persistent_temp_files_interval_ms = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.persistent_temp_files_interval_spin));
		persistent_temp_files_text_dir = gtk_entry_get_text(GTK_ENTRY(pref_widgets.persistent_temp_files_entry_dir));


		g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

		g_key_file_set_boolean(config, "saveactions", "enable_autosave", enable_autosave);
		g_key_file_set_boolean(config, "saveactions", "enable_autosave_losing_focus", enable_autosave_losing_focus);
		g_key_file_set_boolean(config, "saveactions", "enable_instantsave", enable_instantsave);
		g_key_file_set_boolean(config, "saveactions", "enable_backupcopy", enable_backupcopy);
		g_key_file_set_boolean(config, "saveactions", "enable_persistent_temp_files", enable_persistent_temp_files);

		g_key_file_set_boolean(config, "autosave", "print_messages", autosave_print_msg);
		g_key_file_set_boolean(config, "autosave", "save_all", autosave_save_all);
		g_key_file_set_integer(config, "autosave", "interval", autosave_interval);

		if (instantsave_default_ft != NULL)
			g_key_file_set_string(config, "instantsave", "default_ft", instantsave_default_ft);
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

		g_key_file_set_integer(config, "persistent_temp_files", "interval_ms", persistent_temp_files_interval_ms);
		if (enable_persistent_temp_files)
		{
			if (!EMPTY(persistent_temp_files_text_dir) && store_target_directory(
					persistent_temp_files_text_dir, &persistent_temp_files_target_dir))
			{
				g_key_file_set_string(config, "persistent_temp_files", "target_dir", persistent_temp_files_text_dir);
			}
			else
			{
				dialogs_show_msgbox(GTK_MESSAGE_ERROR,
						_("Persistent temp files directory does not exist or is not writable."));
			}
		}

		persistent_temp_files_saver_set_timeout();


		if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils_mkdir(config_dir, TRUE) != 0)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			/* write config to file */
			str = g_key_file_to_data(config, NULL, NULL);
			utils_write_file(config_file, str);
			g_free(str);
		}

		autosave_set_timeout(); /* apply the changes */

		g_free(config_dir);
		g_key_file_free(config);
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
		case NOTEBOOK_PAGE_INSTANTSAVE:
		{
			gtk_widget_set_sensitive(pref_widgets.instantsave_ft_combo, enable);
			/* 'Instantsave' and 'Persistent temp files' are mutually exclusive */
			gtk_widget_set_sensitive(pref_widgets.checkbox_enable_persistent_temp_files, !enable);
			break;
		}
		case NOTEBOOK_PAGE_BACKUPCOPY:
		{
			gtk_widget_set_sensitive(pref_widgets.backupcopy_entry_dir, enable);
			gtk_widget_set_sensitive(pref_widgets.backupcopy_entry_time, enable);
			gtk_widget_set_sensitive(pref_widgets.backupcopy_spin_dir_levels, enable);
			break;
		}
		case NOTEBOOK_PAGE_PERSISTENTTEMPFILES:
		{
			gtk_widget_set_sensitive(pref_widgets.persistent_temp_files_entry_dir, enable);
			gtk_widget_set_sensitive(pref_widgets.persistent_temp_files_interval_spin, enable);
			/* 'Instantsave' and 'Persistent temp files' are mutually exclusive */
			gtk_widget_set_sensitive(pref_widgets.checkbox_enable_instantsave, !enable);
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
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, TRUE, TRUE, 0);

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
	 * Instant Save
	 */
	{
		GtkWidget *combo, *hbox, *entry_dir, *button, *image, *help_label;
		guint i;
		const GSList *node;
		gchar *entry_dir_label_text;

		notebook_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Instant Save")), NOTEBOOK_PAGE_INSTANTSAVE);

		checkbox_enable = gtk_check_button_new_with_mnemonic(_("_Enable"));
		pref_widgets.checkbox_enable_instantsave = checkbox_enable;
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_enable), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_enable), enable_instantsave);
		gtk_box_pack_start(GTK_BOX(inner_vbox), checkbox_enable, FALSE, FALSE, 6);
		g_signal_connect(checkbox_enable, "toggled",
			G_CALLBACK(checkbox_toggled_cb), GINT_TO_POINTER(NOTEBOOK_PAGE_INSTANTSAVE));

		label = gtk_label_new_with_mnemonic(_("Default _filetype to use for new files:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		pref_widgets.instantsave_ft_combo = combo = gtk_combo_box_text_new();
		i = 0;
		foreach_slist(node, filetypes_get_sorted_by_name())
		{
			GeanyFiletype *ft = node->data;

			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), ft->name);

			if (utils_str_equal(ft->name, instantsave_default_ft))
				gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
			i++;
		}
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 3);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
		gtk_box_pack_start(GTK_BOX(inner_vbox), combo, FALSE, FALSE, 0);

		entry_dir_label_text = g_strdup_printf(
			_("_Directory to save files in (leave empty to use the default: %s):"), g_get_tmp_dir());
		label = gtk_label_new_with_mnemonic(entry_dir_label_text);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
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
		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);

		help_label = gtk_label_new(
			_("<i>If you set the Instant Save directory to a directory "
			  "which is not automatically cleared,\nyou will need to cleanup instantly saved files "
			  "manually. The Instant Save plugin will not delete the created files.</i>"));
		gtk_label_set_use_markup(GTK_LABEL(help_label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(help_label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), help_label, FALSE, FALSE, 0);
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
			_("Date/_Time format for backup files (for a list of available conversion specifiers see https://docs.gtk.org/glib/method.DateTime.format.html):"));
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
	 * Persistent Temp Files
	 */
	{
		GtkWidget *hbox, *spin, *entry_dir, *button, *image;

		notebook_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Persistent Temp Files")), NOTEBOOK_PAGE_PERSISTENTTEMPFILES);

		checkbox_enable = gtk_check_button_new_with_mnemonic(_("_Enable"));
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_enable), FALSE);
		pref_widgets.checkbox_enable_persistent_temp_files = checkbox_enable;
		gtk_box_pack_start(GTK_BOX(inner_vbox), checkbox_enable, FALSE, FALSE, 6);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_enable), enable_persistent_temp_files);
		g_signal_connect(checkbox_enable, "toggled",
		G_CALLBACK(checkbox_toggled_cb), GINT_TO_POINTER(NOTEBOOK_PAGE_PERSISTENTTEMPFILES));

		label = gtk_label_new_with_mnemonic(_("_Directory to save persistent temp files in:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		pref_widgets.persistent_temp_files_entry_dir = entry_dir = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_dir);
		if (!EMPTY(persistent_temp_files_target_dir))
			gtk_entry_set_text(GTK_ENTRY(entry_dir), persistent_temp_files_target_dir);

		button = gtk_button_new();
		g_signal_connect(button, "clicked",
			G_CALLBACK(target_directory_button_clicked_cb), entry_dir);

		image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(button), image);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
		gtk_box_pack_start(GTK_BOX(hbox), entry_dir, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
		label = gtk_label_new_with_mnemonic(_("Temp files save _interval:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 5);

		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
		pref_widgets.persistent_temp_files_interval_spin = spin = gtk_spin_button_new_with_range(1, 600000, 100);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), persistent_temp_files_interval_ms);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);

		label = gtk_label_new(_("milliseconds"));

		gtk_box_pack_start(GTK_BOX(hbox), spin, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);
	}

	/* manually emit the toggled signal of the enable checkboxes to update the widget sensitivity */
	g_signal_emit_by_name(pref_widgets.checkbox_enable_autosave, "toggled");
	g_signal_emit_by_name(pref_widgets.checkbox_enable_instantsave, "toggled");
	g_signal_emit_by_name(pref_widgets.checkbox_enable_backupcopy, "toggled");
	g_signal_emit_by_name(pref_widgets.checkbox_enable_persistent_temp_files, "toggled");

	gtk_widget_show_all(vbox);
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);

	return vbox;
}


void plugin_cleanup(void)
{
	if (autosave_src_id != 0)
		g_source_remove(autosave_src_id);

	g_free(instantsave_default_ft);
	g_free(instantsave_target_dir);

	g_free(backupcopy_backup_dir);
	g_free(backupcopy_time_fmt);

	if (persistent_temp_files_saver_src_id != 0)
		g_source_remove(persistent_temp_files_saver_src_id);

	g_free(persistent_temp_files_target_dir);

	g_free(config_file);

	g_hash_table_destroy(persistent_temp_new_old_file_relation_table);
}
