/*
 *      autosave.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *
 * $Id$
 */


#include "geany.h"
#include "support.h"
#include "document.h"

#include "plugindata.h"
#include "pluginmacros.h"


PluginFields	*plugin_fields;
GeanyData		*geany_data;


VERSION_CHECK(32)

PLUGIN_INFO(_("Auto Save"), _("Save automatically all open files in a given time interval."),
	VERSION, _("The Geany developer team"))


static gint interval;
static gboolean print_msg;
static gboolean save_all;
static guint src_id = G_MAXUINT;
static gchar *config_file;


gboolean auto_save(gpointer data)
{
	gint cur_idx = p_document->get_cur_idx();
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
	gint saved_files = 0;

	if (save_all)
	{
		for (i = 0; i < max; i++)
		{
			idx = p_document->get_n_idx(i);

			// skip current file to save it lastly, skip files without name
			if (idx != cur_idx && doc_list[idx].file_name != NULL)
				if (p_document->save_file(idx, FALSE))
					saved_files++;
		}
	}
	// finally save current file, do it after all other files to get correct window title and
	// symbol list
	if (doc_list[cur_idx].file_name != NULL)
		if (p_document->save_file(cur_idx, FALSE))
			saved_files++;

	if (saved_files > 0 && print_msg)
		p_ui->set_statusbar(FALSE, _("Autosave: Saved %d files automatically."), saved_files);

	return TRUE;
}


void set_timeout()
{
	if (src_id != G_MAXUINT)
		g_source_remove(src_id);
	src_id = g_timeout_add(interval * 1000, (GSourceFunc)auto_save, NULL);

}


void init(GeanyData *data)
{
	GKeyFile *config = g_key_file_new();
	GError *error = NULL;
	config_file = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"autosave", G_DIR_SEPARATOR_S, "autosave.conf", NULL);

	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);
	interval = g_key_file_get_integer(config, "autosave", "interval", &error);
	if (error != NULL)
	{
		g_error_free(error);
		interval = 300;
	}
	print_msg = g_key_file_get_boolean(config, "autosave", "print_messages", NULL);
	save_all = g_key_file_get_boolean(config, "autosave", "save_all", NULL);

	set_timeout();

	g_key_file_free(config);
}


void configure(GtkWidget *parent)
{
	GtkWidget *dialog, *label, *spin, *vbox, *hbox, *checkbox, *radio1, *radio2;

	dialog = gtk_dialog_new_with_buttons(_("Auto Save"),
		GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = p_ui->dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	label = gtk_label_new(_("Auto save interval:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	spin = gtk_spin_button_new_with_range(1, 1800, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), interval);

	label = gtk_label_new(_("seconds"));

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(hbox), spin, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);

	checkbox = gtk_check_button_new_with_label(
		_("Print status message if files have been automatically saved"));
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), print_msg);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 5);

	radio1 = gtk_radio_button_new_with_label(NULL,
		_("Save only current open file"));
	gtk_button_set_focus_on_click(GTK_BUTTON(radio1), FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), radio1);

	radio2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio1),
		_("Save all open files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(radio2), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), save_all);
	gtk_container_add(GTK_CONTAINER(vbox), radio2);

	gtk_widget_show_all(vbox);

	// run the dialog and check for the response code
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(config_file);

		interval = gtk_spin_button_get_value_as_int((GTK_SPIN_BUTTON(spin)));
		print_msg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox));
		save_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio2));

		g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

		g_key_file_set_integer(config, "autosave", "interval", interval);
		g_key_file_set_boolean(config, "autosave", "print_messages", print_msg);
		g_key_file_set_boolean(config, "autosave", "save_all", save_all);

		if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && p_utils->mkdir(config_dir, TRUE) != 0)
		{
			p_dialogs->show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			// write config to file
			data = g_key_file_to_data(config, NULL, NULL);
			p_utils->write_file(config_file, data);
			g_free(data);
		}

		set_timeout(); // apply the changes

		g_free(config_dir);
		g_key_file_free(config);
	}
	gtk_widget_destroy(dialog);
}


void cleanup()
{
	g_source_remove(src_id);
	g_free(config_file);
}
