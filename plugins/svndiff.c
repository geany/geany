/*
 *      svndiff.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
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
 * version inside svn. Keep in mind, that it saves your version you currently working
 * on and some changes may get lost by accident. */

#include "geany.h"
#include "support.h"
#include "plugindata.h"
#include "document.h"
#include "filetypes.h"


PluginFields	*plugin_fields;
GeanyData		*geany_data;

#define utils		geany_data->utils
#define doc_array	geany_data->doc_array


VERSION_CHECK(21)

PLUGIN_INFO(_("SVNdiff"), _("Plugin to create a patch of a file against svn"), "0.0.2")


/* Callback if menu item was acitvated */
static void item_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	gchar 	*command;
	gint 	idx;
	gchar	*diff_file_name = NULL;
	gchar	*std_output = NULL;
	gchar	*std_err = NULL;
	gint	exit_code;
	GError	*error_code = NULL;

	idx = geany_data->document->get_cur_idx();

    // Stolen from export.c. Thanks for it, Enrico ;)
    if (doc_list[idx].file_name != NULL)
	{
		gchar *base_name = g_path_get_basename(doc_list[idx].file_name);
		gchar *short_name = utils->remove_ext_from_filename(base_name);
		gchar *locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);


		if (! geany_data->document->save_file(idx, TRUE))
		{
			geany_data->msgwindow->status_add(_("File %s couldn't be saved."
			"Will go on with last saved version."),base_name);
		}
		// use '' quotation for Windows compatibility
		command = g_strdup_printf("svn diff --non-interactive '%s'", locale_filename);

		diff_file_name = g_strconcat(short_name, ".svn.diff", NULL);

		g_free(base_name);
		g_free(short_name);
		g_free(locale_filename);


		if (g_spawn_command_line_sync(command, &std_output, &std_err, &exit_code, &error_code))
		{
			if (! exit_code)
			{
				if (std_output == NULL || std_output[0] != '\0')
				{
					geany_data->document->new_file(diff_file_name,
						geany_data->filetypes[GEANY_FILETYPES_DIFF], std_output);
				}
				else
				{
					geany_data->msgwindow->status_add(_("Current file has no changes."));
				}
			}
			else // SVN returns some error
			{
				// TODO print std_err or print detailed error messages based on exit_code
				geany_data->msgwindow->status_add(
					_("SVN exited with an error. Error code was: %d."), exit_code);
			}
		}
		else
		{
			geany_data->msgwindow->status_add(
				_("Something went really wrong. Is there any svn-binary in your path?"));
		}
		g_free(command);
		g_free(diff_file_name);
	}
	else
	{
		geany_data->msgwindow->status_add(
			_("File seems to don't have a name. Can't go on with processing."));
	}
	g_free(std_output);
	g_free(std_err);
}


/* Called by Geany to initialize the plugin */
void init(GeanyData *data)
{
	GtkWidget *svndiff_item;

	// Add an item to the Tools menu
	svndiff_item = gtk_menu_item_new_with_mnemonic(_("_SVNdiff"));
	gtk_widget_show(svndiff_item);
	gtk_container_add(GTK_CONTAINER(geany_data->tools_menu), svndiff_item);
	g_signal_connect(G_OBJECT(svndiff_item), "activate", G_CALLBACK(item_activated), NULL);

	// keep a pointer to the menu item, so we can remove it when the
	// plugin is unloaded
	plugin_fields->menu_item = svndiff_item;
}


/* Called by Geany before unloading the plugin. */
void cleanup()
{
	// remove the menu item added in init()
	gtk_widget_destroy(plugin_fields->menu_item);
}
