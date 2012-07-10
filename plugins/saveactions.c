/*
 *      saveactions.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "geanyplugin.h"

#include <unistd.h>
#include <errno.h>
#include <glib/gstdio.h>


GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)

PLUGIN_SET_INFO(_("Save Actions"), _("This plugin provides different actions related to saving of files."),
	VERSION, _("The Geany developer team"))


enum
{
	NOTEBOOK_PAGE_AUTOSAVE = 0,
	NOTEBOOK_PAGE_INSTANTSAVE,
	NOTEBOOK_PAGE_BACKUPCOPY
};

static struct
{
	GtkWidget *checkbox_enable_autosave;
	GtkWidget *checkbox_enable_instantsave;
	GtkWidget *checkbox_enable_backupcopy;

	GtkWidget *autosave_interval_spin;
	GtkWidget *autosave_print_msg_checkbox;
	GtkWidget *autosave_save_all_radio1;
	GtkWidget *autosave_save_all_radio2;

	GtkWidget *instantsave_ft_combo;

	GtkWidget *backupcopy_entry_dir;
	GtkWidget *backupcopy_entry_time;
	GtkWidget *backupcopy_spin_dir_levels;
}
pref_widgets;


static gboolean enable_autosave;
static gboolean enable_instantsave;
static gboolean enable_backupcopy;

static gint autosave_interval;
static gboolean autosave_print_msg;
static gboolean autosave_save_all;
static guint autosave_src_id = 0;

static gchar *instantsave_default_ft;

static gchar *backupcopy_backup_dir; /* path to an existing directory in locale encoding */
static gchar *backupcopy_time_fmt;
static gint backupcopy_dir_levels;

static gchar *config_file;


/* Ensures utf8_dir exists and is writable and
 * set backup_dir to the locale encoded form of utf8_dir */
static gboolean backupcopy_set_backup_dir(const gchar *utf8_dir)
{
	gchar *tmp;

	if (G_UNLIKELY(! NZV(utf8_dir)))
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

	SETPTR(backupcopy_backup_dir, tmp);

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

	if ((dst = g_fopen(locale_filename_dst, "wb")) == NULL)
	{
		ui_set_statusbar(FALSE, _("Backup Copy: File could not be saved (%s)."),
			g_strerror(errno));
		g_free(locale_filename_src);
		g_free(locale_filename_dst);
		g_free(stamp);
		fclose(src);
		return;
	}

	while (fgets(stamp, sizeof(stamp), src) != NULL)
	{
		fputs(stamp, dst);
	}

	fclose(src);
	fclose(dst);
	g_free(locale_filename_src);
	g_free(locale_filename_dst);
	g_free(stamp);
}


static void instantsave_document_new_cb(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
    if (enable_instantsave && doc->file_name == NULL)
    {
		gchar *new_filename;
		gint fd;
		GeanyFiletype *ft = doc->file_type;

		fd = g_file_open_tmp("gis_XXXXXX", &new_filename, NULL);
		if (fd != -1)
			close(fd); /* close the returned file descriptor as we only need the filename */

		if (ft == NULL || ft->id == GEANY_FILETYPES_NONE)
			/* ft is NULL when a new file without template was opened, so use the
			 * configured default file type */
			ft = filetypes_lookup_by_name(instantsave_default_ft);

		if (ft != NULL)
			/* add the filetype's default extension to the new filename */
			SETPTR(new_filename, g_strconcat(new_filename, ".", ft->extension, NULL));

		doc->file_name = new_filename;

		if (doc->file_type->id == GEANY_FILETYPES_NONE)
			document_set_filetype(doc, filetypes_lookup_by_name(instantsave_default_ft));

		/* force saving the file to enable all the related actions(tab name, filetype, etc.) */
		document_save_file(doc, TRUE);
    }
}


PluginCallback plugin_callbacks[] =
{
	{ "document-new", (GCallback) &instantsave_document_new_cb, FALSE, NULL },
	{ "document-save", (GCallback) &backupcopy_document_save_cb, FALSE, NULL },
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
	if (! enable_autosave)
		return;

	if (autosave_src_id != 0)
		g_source_remove(autosave_src_id);
	autosave_src_id = g_timeout_add(autosave_interval * 1000, (GSourceFunc) auto_save, NULL);
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
	enable_instantsave = utils_get_setting_boolean(
		config, "saveactions", "enable_instantsave", FALSE);
	enable_backupcopy = utils_get_setting_boolean(
		config, "saveactions", "enable_backupcopy", FALSE);

	instantsave_default_ft = utils_get_setting_string(config, "instantsave", "default_ft",
		filetypes[GEANY_FILETYPES_NONE]->name);

	autosave_src_id = 0; /* mark as invalid */
	autosave_interval = utils_get_setting_integer(config, "autosave", "interval", 300);
	autosave_print_msg = utils_get_setting_boolean(config, "autosave", "print_messages", FALSE);
	autosave_save_all = utils_get_setting_boolean(config, "autosave", "save_all", FALSE);
	if (enable_autosave)
		autosave_set_timeout();

	backupcopy_dir_levels = utils_get_setting_integer(config, "backupcopy", "dir_levels", 0);
	backupcopy_time_fmt = utils_get_setting_string(
		config, "backupcopy", "time_fmt", "%Y-%m-%d-%H-%M-%S");
	tmp = utils_get_setting_string(config, "backupcopy", "backup_dir", g_get_tmp_dir());
	backupcopy_set_backup_dir(tmp);

	g_key_file_free(config);
	g_free(tmp);
}


static void backupcopy_dir_button_clicked_cb(GtkButton *button, gpointer item)
{
	/** TODO add win32_show_pref_file_dialog to the plugin API and use it **/
/*
#ifdef G_OS_WIN32
	win32_show_pref_file_dialog(item);
#else
*/
	GtkWidget *dialog;
	gchar *text;

	/* initialize the dialog */
	dialog = gtk_file_chooser_dialog_new(_("Select Directory"), NULL,
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	text = utils_get_locale_from_utf8(gtk_entry_get_text(GTK_ENTRY(item)));
	if (NZV(text))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), text);

	/* run it */
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *utf8_filename, *tmp;

		tmp = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		utf8_filename = utils_get_utf8_from_locale(tmp);

		gtk_entry_set_text(GTK_ENTRY(item), utf8_filename);

		g_free(utf8_filename);
		g_free(tmp);
	}

	gtk_widget_destroy(dialog);
}


static void configure_response_cb(GtkDialog *dialog, gint response, G_GNUC_UNUSED gpointer data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *str;
		const gchar *text_dir, *text_time;
		gchar *config_dir = g_path_get_dirname(config_file);

		enable_autosave = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_autosave));
		enable_instantsave = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_instantsave));
		enable_backupcopy = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.checkbox_enable_backupcopy));

		autosave_interval = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.autosave_interval_spin));
		autosave_print_msg = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.autosave_print_msg_checkbox));
		autosave_save_all = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(pref_widgets.autosave_save_all_radio2));

		g_free(instantsave_default_ft);
		instantsave_default_ft = gtk_combo_box_get_active_text(
			GTK_COMBO_BOX(pref_widgets.instantsave_ft_combo));

		text_dir = gtk_entry_get_text(GTK_ENTRY(pref_widgets.backupcopy_entry_dir));
		text_time = gtk_entry_get_text(GTK_ENTRY(pref_widgets.backupcopy_entry_time));
		backupcopy_dir_levels = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(pref_widgets.backupcopy_spin_dir_levels));


		g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

		g_key_file_set_boolean(config, "saveactions", "enable_autosave", enable_autosave);
		g_key_file_set_boolean(config, "saveactions", "enable_instantsave", enable_instantsave);
		g_key_file_set_boolean(config, "saveactions", "enable_backupcopy", enable_backupcopy);

		g_key_file_set_boolean(config, "autosave", "print_messages", autosave_print_msg);
		g_key_file_set_boolean(config, "autosave", "save_all", autosave_save_all);
		g_key_file_set_integer(config, "autosave", "interval", autosave_interval);

		if (instantsave_default_ft != NULL)
			g_key_file_set_string(config, "instantsave", "default_ft", instantsave_default_ft);

		g_key_file_set_integer(config, "backupcopy", "dir_levels", backupcopy_dir_levels);
		g_key_file_set_string(config, "backupcopy", "time_fmt", text_time);
		SETPTR(backupcopy_time_fmt, g_strdup(text_time));
		if (enable_backupcopy)
		{
			if (NZV(text_dir) && backupcopy_set_backup_dir(text_dir))
			{
				g_key_file_set_string(config, "backupcopy", "backup_dir", text_dir);
			}
			else
			{
				dialogs_show_msgbox(GTK_MESSAGE_ERROR,
						_("Backup directory does not exist or is not writable."));
			}
		}


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

		if (enable_autosave)
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


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox, *label, *notebook_vbox, *checkbox_enable;
	GtkWidget *notebook, *inner_vbox;

	vbox = gtk_vbox_new(FALSE, 6);

	notebook = gtk_notebook_new();
	GTK_WIDGET_UNSET_FLAGS(notebook, GTK_CAN_FOCUS);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 5);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, TRUE, 0);

	/*
	 * Auto Save
	 */
	{
		GtkWidget *spin, *hbox, *checkbox, *radio1, *radio2;

		notebook_vbox = gtk_vbox_new(FALSE, 2);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Auto Save")), NOTEBOOK_PAGE_AUTOSAVE);

		checkbox_enable = gtk_check_button_new_with_mnemonic(_("_Enable"));
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_enable), FALSE);
		pref_widgets.checkbox_enable_autosave = checkbox_enable;
		gtk_box_pack_start(GTK_BOX(inner_vbox), checkbox_enable, FALSE, FALSE, 6);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_enable), enable_autosave);
		g_signal_connect(checkbox_enable, "toggled",
			G_CALLBACK(checkbox_toggled_cb), GINT_TO_POINTER(NOTEBOOK_PAGE_AUTOSAVE));

		label = gtk_label_new_with_mnemonic(_("Auto save _interval:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_container_add(GTK_CONTAINER(inner_vbox), label);

		pref_widgets.autosave_interval_spin = spin = gtk_spin_button_new_with_range(1, 1800, 1);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), autosave_interval);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), spin);

		label = gtk_label_new(_("seconds"));

		hbox = gtk_hbox_new(FALSE, 5);
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
		GtkWidget *combo;
		guint i;
		const GSList *node;

		notebook_vbox = gtk_vbox_new(FALSE, 2);
		inner_vbox = gtk_vbox_new(FALSE, 5);
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

		label = gtk_label_new_with_mnemonic(_("_Filetype to use for newly opened files:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		pref_widgets.instantsave_ft_combo = combo = gtk_combo_box_new_text();
		i = 0;
		foreach_slist(node, filetypes_get_sorted_by_name())
		{
			GeanyFiletype *ft = node->data;

			gtk_combo_box_append_text(GTK_COMBO_BOX(combo), ft->name);

			if (utils_str_equal(ft->name, instantsave_default_ft))
				gtk_combo_box_set_active(GTK_COMBO_BOX(combo), i);
			i++;
		}
		gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(combo), 3);
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), combo);
		gtk_box_pack_start(GTK_BOX(inner_vbox), combo, FALSE, FALSE, 0);
	}
	/*
	 * Backup Copy
	 */
	{
		GtkWidget *hbox, *entry_dir, *entry_time, *button, *image, *spin_dir_levels;

		notebook_vbox = gtk_vbox_new(FALSE, 2);
		inner_vbox = gtk_vbox_new(FALSE, 5);
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
		if (NZV(backupcopy_backup_dir))
			gtk_entry_set_text(GTK_ENTRY(entry_dir), backupcopy_backup_dir);

		button = gtk_button_new();
		g_signal_connect(button, "clicked",
			G_CALLBACK(backupcopy_dir_button_clicked_cb), entry_dir);

		image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(button), image);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start_defaults(GTK_BOX(hbox), entry_dir);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(inner_vbox), hbox, FALSE, FALSE, 0);

		label = gtk_label_new_with_mnemonic(
			_("Date/_Time format for backup files (\"man strftime\" for details):"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 7);

		pref_widgets.backupcopy_entry_time = entry_time = gtk_entry_new();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_time);
		if (NZV(backupcopy_time_fmt))
			gtk_entry_set_text(GTK_ENTRY(entry_time), backupcopy_time_fmt);
		gtk_box_pack_start(GTK_BOX(inner_vbox), entry_time, FALSE, FALSE, 0);

		hbox = gtk_hbox_new(FALSE, 6);

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

	/* manually emit the toggled signal of the enable checkboxes to update the widget sensitivity */
	g_signal_emit_by_name(pref_widgets.checkbox_enable_autosave, "toggled");
	g_signal_emit_by_name(pref_widgets.checkbox_enable_instantsave, "toggled");
	g_signal_emit_by_name(pref_widgets.checkbox_enable_backupcopy, "toggled");

	gtk_widget_show_all(vbox);
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);

	return vbox;
}


void plugin_cleanup(void)
{
	if (autosave_src_id != 0)
		g_source_remove(autosave_src_id);

	g_free(instantsave_default_ft);

	g_free(backupcopy_backup_dir);
	g_free(backupcopy_time_fmt);

	g_free(config_file);
}
