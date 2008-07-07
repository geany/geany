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
#include "ui_utils.h"

#include "plugindata.h"
#include "pluginmacros.h"


PluginFields	*plugin_fields;
GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(69)

PLUGIN_SET_INFO(_("Auto Save"), _("Save automatically all open files in a given time interval."),
	VERSION, _("The Geany developer team"))


static gint interval;
static gboolean print_msg;
static gboolean save_all;
static guint src_id = G_MAXUINT;
static gchar *config_file;


gboolean auto_save(gpointer data)
{
	GeanyDocument *doc;
	GeanyDocument *cur_doc = p_document->get_current();
	gint i, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(geany->main_widgets->notebook));
	gint saved_files = 0;

	if (save_all)
	{
		for (i = 0; i < max; i++)
		{
			doc = p_document->get_from_page(i);

			/* skip current file to save it lastly, skip files without name */
			if (doc != cur_doc && cur_doc->file_name != NULL)
				if (p_document->save_file(doc, FALSE))
					saved_files++;
		}
	}
	/* finally save current file, do it after all other files to get correct window title and
	 * symbol list */
	if (cur_doc->file_name != NULL)
		if (p_document->save_file(cur_doc, FALSE))
			saved_files++;

	if (saved_files > 0 && print_msg)
		p_ui->set_statusbar(FALSE, ngettext(
			"Autosave: Saved %d file automatically.",
			"Autosave: Saved %d files automatically.", saved_files),
			saved_files);

	return TRUE;
}


void set_timeout(void)
{
	if (src_id != G_MAXUINT)
		g_source_remove(src_id);
	src_id = g_timeout_add(interval * 1000, (GSourceFunc)auto_save, NULL);

}


void plugin_init(GeanyData *data)
{
	GKeyFile *config = g_key_file_new();
	GError *error = NULL;
	config_file = g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
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


static struct
{
	GtkWidget *interval_spin;
	GtkWidget *print_msg_checkbox;
	GtkWidget *save_all_radio;
}
pref_widgets;

static void
on_configure_response(GtkDialog *dialog, gint response, G_GNUC_UNUSED gpointer user_data)
{
	if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(config_file);

		interval = gtk_spin_button_get_value_as_int((GTK_SPIN_BUTTON(pref_widgets.interval_spin)));
		print_msg = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.print_msg_checkbox));
		save_all = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pref_widgets.save_all_radio));

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
			/* write config to file */
			data = g_key_file_to_data(config, NULL, NULL);
			p_utils->write_file(config_file, data);
			g_free(data);
		}

		set_timeout(); /* apply the changes */

		g_free(config_dir);
		g_key_file_free(config);
	}
}


GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox, *label, *spin, *hbox, *checkbox, *radio1, *radio2;

	vbox = gtk_vbox_new(FALSE, 6);

	label = gtk_label_new(_("Auto save interval:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	spin = gtk_spin_button_new_with_range(1, 1800, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), interval);
	pref_widgets.interval_spin = spin;

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
	pref_widgets.print_msg_checkbox = checkbox;

	radio1 = gtk_radio_button_new_with_label(NULL,
		_("Save only current open file"));
	gtk_button_set_focus_on_click(GTK_BUTTON(radio1), FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), radio1);

	radio2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio1),
		_("Save all open files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(radio2), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), save_all);
	gtk_container_add(GTK_CONTAINER(vbox), radio2);
	pref_widgets.save_all_radio = radio2;

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
	return vbox;
}


void plugin_cleanup(void)
{
	g_source_remove(src_id);
	g_free(config_file);
}
