/*
 *      svndiff.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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

/* SVNdiff plugin */
/* This small plugin uses svn to generate a diff against the current
 * version inside svn.*/

#include "geany.h"
#include "support.h"
#include "plugindata.h"
#include "document.h"
#include "filetypes.h"
#include "utils.h"
#include "project.h"

PluginFields	*plugin_fields;
GeanyData		*geany_data;

#define utils		geany_data->utils
#define ui			geany_data->ui
#define doc_array	geany_data->doc_array


VERSION_CHECK(25)

PLUGIN_INFO(_("SVNdiff"), _("Plugin to create a patch of a file against svn"), "0.0.4")

/* Callback if menu item for the current procet or directory was acitvated */
static void svndirectory_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	guint	idx, new_idx;
	gchar	*base_name = NULL;
	gchar 	*command = NULL;
	gchar	*project_name = NULL;
	gchar	*std_output = NULL;
	gchar	*std_err = NULL;
	gint	exit_code;
	GError	*error = NULL;
	gchar	*filename = NULL;
	gchar	*locale_filename = NULL;
	gchar	*text = NULL;
	gchar	*dir_enc = NULL;

	GeanyProject *geany_project = geany_data->app->project;

	idx = geany_data->document->get_cur_idx();

	if (geany_project != NULL && NZV(geany_project->base_path))
	{
		if (doc_list[idx].file_name != NULL)
		{
			geany_data->document->save_file(idx, FALSE);
		}
		base_name = geany_project->base_path;
		project_name = geany_project->name;
	}
	else if (doc_list[idx].file_name != NULL)
	{
		if (doc_list[idx].changed)
		{
			geany_data->document->save_file(idx, FALSE);
		}
		locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);
		base_name = g_path_get_dirname(locale_filename);
	}
	else if (doc_list[idx].file_name == NULL)
	{
		if ( geany_data->dialogs->show_save_as() )
		{
			locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);
			base_name = g_path_get_dirname(locale_filename);
		}
	}

	if (base_name != NULL)
	{
		command = g_strconcat("svn diff ", base_name, NULL);

		if (project_name != NULL)
		{
			filename = g_strconcat(project_name,".diff", NULL);
		}
		else
		{
			filename = g_strdup("dir.diff");
		}

		if (g_spawn_command_line_sync(command, &std_output, &std_err, &exit_code, &error))
 		{
 			if (! exit_code)
 			{
 				if (std_output == NULL || std_output[0] != '\0')
 				{
					if (filename != NULL)
					// Be carefull with mixed up encodings
					{
						text = geany_data->encoding->convert_to_utf8(std_output, -1, &dir_enc);
						new_idx = geany_data->document->new_file(filename, NULL, std_output);
						geany_data->document->set_encoding(new_idx, dir_enc);
						g_free(text);
						g_free(dir_enc);
						g_free(filename);
					}

 				}
 				else
 				{
					ui->set_statusbar(FALSE, _("No changes were made."));
 				}
 			}
 			else
 			{	// SVN returns some error
				ui->set_statusbar(FALSE,
 				_("Something went really wrong. Is there any svn-binary in your path?"));
 			}
 		}
	}
	else
	{
		ui->set_statusbar(FALSE, _("Could not determinate a path working in"));
	}
}

/* Callback if menu item for a single file was acitvated */
static void svnfile_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	gchar 	*command;
	gint 	idx;
	gchar	*diff_file_name = NULL;
	gchar	*std_output = NULL;
	gchar	*std_err = NULL;
	gint	exit_code;
	GError	*error = NULL;
	gint 	new_idx;
	gchar	*text = NULL;
	gchar	*base_name = NULL;
	gchar 	*short_name = NULL;
	gchar 	*locale_filename = NULL;

	idx = geany_data->document->get_cur_idx();

	if (doc_list[idx].file_name == NULL)
	{
		geany_data->dialogs->show_save_as();
	}
	else if (doc_list[idx].changed)
	{
		geany_data->document->save_file(idx, FALSE);
	}

    // Stolen from export.c. Thanks for it, Enrico ;)
    if (doc_list[idx].file_name != NULL)
	{
		base_name = g_path_get_basename(doc_list[idx].file_name);
		short_name = utils->remove_ext_from_filename(base_name);
		locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);


		// use '' quotation for Windows compatibility
		command = g_strdup_printf("svn diff --non-interactive '%s'", locale_filename);

		diff_file_name = g_strconcat(short_name, ".svn.diff", NULL);

		g_free(base_name);
		g_free(short_name);
		g_free(locale_filename);


		if (g_spawn_command_line_sync(command, &std_output, &std_err, &exit_code, &error))
		{
			if (! exit_code)
			{
				if (std_output == NULL || std_output[0] != '\0')
				{

					// need to convert input text from the encoding of the original file into
					// UTF-8 because internally Geany always needs UTF-8
					text = geany_data->encoding->convert_to_utf8_from_charset(
						std_output, -1, doc_list[idx].encoding, TRUE);

					if (text == NULL)
					{
						ui->set_statusbar(FALSE, _("Could not parse the output of svn diff"));
					}
					else
					{
						new_idx = geany_data->document->new_file(diff_file_name,
							geany_data->filetypes[GEANY_FILETYPES_DIFF], text);
						geany_data->document->set_encoding(new_idx, doc_list[idx].encoding);
						g_free(text);
					}
				}
				else
				{
					ui->set_statusbar(FALSE, _("Current file has no changes."));
				}
			}
			else // SVN returns some error
			{
				/// TODO print std_err or print detailed error messages based on exit_code
				ui->set_statusbar(FALSE,
					_("SVN exited with an error. Error code was: %d."), exit_code);
			}
		}
		else
		{
			ui->set_statusbar(FALSE,
				_("Something went really wrong. Is there any svn-binary in your path?"));
		}
		g_free(command);
		g_free(diff_file_name);
	}
	else
	{
		ui->set_statusbar(FALSE,
			_("File is unnamed. Can't go on with processing."));
	}
	g_free(std_output);
	g_free(std_err);
	g_free(error);
}


/* Called by Geany to initialize the plugin */
void init(GeanyData *data)
{
	GtkWidget *menu_svndiff = NULL;
	GtkWidget *menu_svndiff_menu = NULL;
	GtkWidget *menu_svndiff_dir = NULL;
	GtkWidget *menu_svndiff_file = NULL;
 	GtkTooltips *tooltips = NULL;

	tooltips = gtk_tooltips_new();
	//// Add an item to the Tools menu
	//svndiff_item = gtk_menu_item_new_with_mnemonic(_("_SVNdiff"));
	//gtk_widget_show(svndiff_item);
	//gtk_container_add(GTK_CONTAINER(geany_data->tools_menu), svndiff_item);
	//g_signal_connect(G_OBJECT(svndiff_item), "activate", G_CALLBACK(svnfile_activated), NULL);


	plugin_fields->flags = PLUGIN_IS_DOCUMENT_SENSITIVE;

	menu_svndiff = gtk_image_menu_item_new_with_mnemonic(_("_SVNdiff"));
	gtk_container_add(GTK_CONTAINER(data->tools_menu), menu_svndiff);

	menu_svndiff_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_svndiff), menu_svndiff_menu);

	// Directory
	menu_svndiff_dir = gtk_menu_item_new_with_mnemonic(_("From current _project"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_dir);
	gtk_tooltips_set_tip (tooltips, menu_svndiff_dir,
		_("Makes a svn diff from the a complete projekt or if there is no "
		"project avaible of the directory of current activeted file"), NULL);

	g_signal_connect((gpointer) menu_svndiff_dir, "activate",
		G_CALLBACK(svndirectory_activated), NULL);

	// Singe file
	menu_svndiff_file = gtk_menu_item_new_with_mnemonic(_("From single _file"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_file);
	gtk_tooltips_set_tip (tooltips, menu_svndiff_file,
		_("Makes a svn diff from the of current activeted file"), NULL);

	g_signal_connect((gpointer) menu_svndiff_file, "activate",
		G_CALLBACK(svnfile_activated), NULL);

	gtk_widget_show_all(menu_svndiff);

	plugin_fields->menu_item = menu_svndiff;
}


/* Called by Geany before unloading the plugin. */
void cleanup()
{
	// remove the menu item added in init()
	gtk_widget_destroy(plugin_fields->menu_item);
}
