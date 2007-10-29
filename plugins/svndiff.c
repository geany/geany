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
#include "pluginmacros.h"

PluginFields	*plugin_fields;
GeanyData		*geany_data;


VERSION_CHECK(25)

PLUGIN_INFO(_("SVNdiff"), _("Plugin to create a patch of a file against svn"), VERSION)


/* name_prefix should be in UTF-8, and can have a path. */
static void show_output(const gchar *std_output, const gchar *name_prefix,
		const gchar *force_encoding)
{
	gchar *text, *detect_enc = NULL;
	gint new_idx;
	gchar *filename;
	
	filename = g_path_get_basename(name_prefix);
	setptr(filename, g_strconcat(filename, ".svn.diff", NULL));

	// need to convert input text from the encoding of the original file into
	// UTF-8 because internally Geany always needs UTF-8
	if (force_encoding)
	{
		text = geany_data->encoding->convert_to_utf8_from_charset(
			std_output, -1, force_encoding, TRUE);
	}
	else
	{
		text = geany_data->encoding->convert_to_utf8(std_output, -1, &detect_enc);
	}
	if (text)
	{
		new_idx = geany_data->document->new_file(filename,
			geany_data->filetypes[GEANY_FILETYPES_DIFF], text);

		geany_data->document->set_encoding(new_idx,
			force_encoding ? force_encoding : detect_enc);
	}
	else
	{
		ui->set_statusbar(FALSE, _("Could not parse the output of svn diff"));
	}
	g_free(text);
	g_free(detect_enc);
	g_free(filename);
}


static gchar *make_diff(const gchar *svn_file)
{
	gchar	*std_output = NULL;
	gchar	*std_error = NULL;
	gint	exit_code;
	gchar	*command, *text = NULL;

	// use '' quotation for Windows compatibility
	command = g_strdup_printf("svn diff --non-interactive '%s'", svn_file);

	if (g_spawn_command_line_sync(command, &std_output, &std_error, &exit_code, NULL))
	{
		if (! exit_code)
		{
			if (NZV(std_output))
			{
				text = std_output;
			}
			else
			{
				ui->set_statusbar(FALSE, _("No changes were made."));
			}
		}
		else
		{	// SVN returns some error
			ui->set_statusbar(FALSE,
				_("SVN exited with an error: %s."), g_strstrip(std_error));
		}
	}
	else
	{
		ui->set_statusbar(FALSE,
			_("Something went really wrong. Is there any svn-binary in your path?"));
	}
	g_free(std_error);
	g_free(command);
	return text;
}

	
/* Callback if menu item for the current project or directory was activated */
static void svndirectory_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	guint	idx;
	gchar	*base_name = NULL;
	gchar	*locale_filename = NULL;
	const gchar *project_name = NULL;

	idx = documents->get_cur_idx();

	if (project != NULL && NZV(project->base_path))
	{
		if (doc_list[idx].file_name != NULL)
		{
			documents->save_file(idx, FALSE);
		}
		base_name = g_strdup(project->base_path);
		project_name = project->name;
	}
	else if (doc_list[idx].file_name != NULL)
	{
		if (doc_list[idx].changed)
		{
			documents->save_file(idx, FALSE);
		}
		locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);
		base_name = g_path_get_dirname(locale_filename);
	}
	else if (doc_list[idx].file_name == NULL)
	{
		if ( dialogs->show_save_as() )
		{
			locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);
			base_name = g_path_get_dirname(locale_filename);
		}
	}

	if (base_name != NULL)
	{
		const gchar *filename;
		gchar *text;
	
		if (project_name != NULL)
		{
			filename = project_name;
		}
		else
		{
			filename = base_name;
		}
		text = make_diff(base_name);
		if (text)
			show_output(text, filename, NULL);
		g_free(text);
	}
	else
	{
		ui->set_statusbar(FALSE, _("Could not determine a path to work in"));
	}
	g_free(locale_filename);
	g_free(base_name);
}


/* Callback if menu item for a single file was activated */
static void svnfile_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	gint 	idx;

	idx = documents->get_cur_idx();

	if (doc_list[idx].file_name == NULL)
	{
		dialogs->show_save_as();
	}
	else if (doc_list[idx].changed)
	{
		documents->save_file(idx, FALSE);
	}

    if (doc_list[idx].file_name != NULL)
	{
		gchar *locale_filename, *text;

		locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);

		text = make_diff(locale_filename);
		if (text)
			show_output(text, doc_list[idx].file_name, doc_list[idx].encoding);
		g_free(text);
		g_free(locale_filename);
	}
	else
	{
		ui->set_statusbar(FALSE,
			_("File is unnamed. Can't go on with processing."));
	}
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
	menu_svndiff_dir = gtk_menu_item_new_with_mnemonic(_("From Current _Project"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_dir);
	gtk_tooltips_set_tip (tooltips, menu_svndiff_dir,
		_("Make a diff from the current project's base path, or if there is no "
		"project open, from the directory of the current active file"), NULL);

	g_signal_connect((gpointer) menu_svndiff_dir, "activate",
		G_CALLBACK(svndirectory_activated), NULL);

	// Single file
	menu_svndiff_file = gtk_menu_item_new_with_mnemonic(_("From Current _File"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_file);
	gtk_tooltips_set_tip (tooltips, menu_svndiff_file,
		_("Make a diff from the current active file"), NULL);

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
