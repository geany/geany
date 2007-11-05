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


VERSION_CHECK(27)

PLUGIN_INFO(_("SVNdiff"), _("Plugin to create a patch of a file against svn"), VERSION)

static int find_by_filename(const gchar* filename)
{
	guint i;
	for (i = 0; i < doc_array->len; i++)
	{
		if ( DOC_IDX_VALID(i) && doc_list[i].file_name &&
		     strcmp(doc_list[i].file_name, filename) == 0)
			return i;
	}
	return -1;
}

/* name_prefix should be in UTF-8, and can have a path. */
static void show_output(const gchar *std_output, const gchar *name_prefix,
		const gchar *force_encoding)
{
	gchar	*text, *detect_enc = NULL;
	gint 	idx, page;
	GtkNotebook *book;
	gchar	*filename;

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
		idx = find_by_filename(filename);
		if ( idx == -1)
		{
			idx = geany_data->document->new_file(filename,
			geany_data->filetypes[GEANY_FILETYPES_DIFF], text);
		}
		else
		{
			scintilla->set_text(doc_list[idx].sci, text);
			book = GTK_NOTEBOOK(app->notebook);
			page = gtk_notebook_page_num(book, GTK_WIDGET(doc_list[idx].sci));
			gtk_notebook_set_current_page(book, page);
			doc_list[idx].changed = FALSE;
			documents->set_text_changed(idx);
		}

		geany_data->document->set_encoding(idx,
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

static gboolean make_revert(const gchar *svn_file)
{
	gchar	*std_output = NULL;
	gchar	*std_error = NULL;
	gint	exit_code;
	gchar	*command = NULL;

	// use '' quotation for Windows compatibility
	command = g_strdup_printf("svn revert '%s'", svn_file);

	if (g_spawn_command_line_sync(command, &std_output, &std_error, &exit_code, NULL))
	{
		if (! exit_code)
		{
			if (NZV(std_output))
			{
				ui->set_statusbar(FALSE, std_output);
			}
			else
			{
				ui->set_statusbar(FALSE, _("No changes were made."));
				return FALSE;
			}
		}
		else
		{	// SVN returns some error
			dialogs->show_msgbox(1,
				_("SVN exited with an error: \n%s."), g_strstrip(std_error));
			return FALSE;
		}
	}
	else
	{
		ui->set_statusbar(FALSE,
			_("Something went really wrong. Is there any svn-binary in your path?"));
		return FALSE;
	}

	return TRUE;
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
			dialogs->show_msgbox(1,
				_("SVN exited with an error: \n%s."), g_strstrip(std_error));
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


/* Make a diff from the current directory */
static void svndirectory_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	gint	idx;
	gchar	*base_name = NULL;
	gchar	*locale_filename = NULL;
	gchar	*text;

	idx = documents->get_cur_idx();

	g_return_if_fail(DOC_IDX_VALID(idx) && doc_list[idx].file_name != NULL);

	if (doc_list[idx].changed)
	{
		documents->save_file(idx, FALSE);
	}

	locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);
	base_name = g_path_get_dirname(locale_filename);

	text = make_diff(base_name);
	if (text)
		show_output(text, base_name, NULL);
	g_free(text);

	g_free(base_name);
	g_free(locale_filename);
}


/* Callback if menu item for the current project was activated */
static void svnproject_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	gint	idx;
	gchar	*locale_filename = NULL;
	gchar	*text;

	idx = documents->get_cur_idx();

	g_return_if_fail(project != NULL && NZV(project->base_path));

	if (DOC_IDX_VALID(idx) && doc_list[idx].changed && doc_list[idx].file_name != NULL)
	{
		documents->save_file(idx, FALSE);
	}

	locale_filename = utils->get_locale_from_utf8(project->base_path);
	text = make_diff(locale_filename);
	if (text)
		show_output(text, project->name, NULL);
	g_free(text);
	g_free(locale_filename);
}


/* Callback if menu item for a single file was activated */
static void svnfile_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	gint	idx;
	gchar	*locale_filename, *text;

	idx = documents->get_cur_idx();

	g_return_if_fail(DOC_IDX_VALID(idx) && doc_list[idx].file_name != NULL);

	if (doc_list[idx].changed)
	{
		documents->save_file(idx, FALSE);
	}

	locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);

	text = make_diff(locale_filename);
	if (text)
		show_output(text, doc_list[idx].file_name, doc_list[idx].encoding);
	g_free(text);
	g_free(locale_filename);
}

/* Callback if menu item for a single file was activated */
static void svnrevert_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	gint	idx;
	gchar	*locale_filename;

	idx = documents->get_cur_idx();

	g_return_if_fail(DOC_IDX_VALID(idx) && doc_list[idx].file_name != NULL);

	if (dialogs->show_question(_("Do you realy want to revert '%s'?"), doc_list[idx].file_name))
	{
		locale_filename = utils->get_locale_from_utf8(doc_list[idx].file_name);

		if (make_revert(locale_filename))
		{
			documents->reload_file(idx, NULL);
		}
		g_free(locale_filename);
	}
}

static GtkWidget *menu_svndiff_file = NULL;
static GtkWidget *menu_svndiff_dir = NULL;
static GtkWidget *menu_svndiff_project = NULL;
static GtkWidget *menu_svndiff_revert = NULL;

static void update_menu_items()
{
	document	*doc;
	gboolean	have_file;

	doc = documents->get_current();
	have_file = doc && doc->file_name && g_path_is_absolute(doc->file_name);

	gtk_widget_set_sensitive(menu_svndiff_file, have_file);
	gtk_widget_set_sensitive(menu_svndiff_dir, have_file);
	gtk_widget_set_sensitive(menu_svndiff_revert, have_file);
	gtk_widget_set_sensitive(menu_svndiff_project,
		project != NULL && NZV(project->base_path));
}


/* Called by Geany to initialize the plugin */
void init(GeanyData *data)
{
	GtkWidget	*menu_svndiff = NULL;
	GtkWidget	*menu_svndiff_menu = NULL;
 	GtkTooltips	*tooltips = NULL;
	gchar		*tmp = NULL;
	gboolean	have_svn = FALSE;

	// Check for svn inside $PATH. Thanks to Yura Siamashka <yurand2@gmail.com>
	tmp = g_find_program_in_path("svn");
	if (!tmp)
		return;
	have_svn = TRUE;
	g_free(tmp);

	tooltips = gtk_tooltips_new();

	plugin_fields->flags = PLUGIN_IS_DOCUMENT_SENSITIVE;

	menu_svndiff = gtk_image_menu_item_new_with_mnemonic(_("_SVNdiff"));
	gtk_container_add(GTK_CONTAINER(data->tools_menu), menu_svndiff);

	g_signal_connect((gpointer) menu_svndiff, "activate",
		G_CALLBACK(update_menu_items), NULL);

	menu_svndiff_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_svndiff), menu_svndiff_menu);

	// Single file
	menu_svndiff_file = gtk_menu_item_new_with_mnemonic(_("From Current _File"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_file);
	gtk_tooltips_set_tip (tooltips, menu_svndiff_file,
		_("Make a diff from the current active file"), NULL);

	g_signal_connect((gpointer) menu_svndiff_file, "activate",
		G_CALLBACK(svnfile_activated), NULL);

	// Directory
	menu_svndiff_dir = gtk_menu_item_new_with_mnemonic(_("From Current _Directory"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_dir);
	gtk_tooltips_set_tip (tooltips, menu_svndiff_dir,
		_("Make a diff from the directory of the current active file"), NULL);

	g_signal_connect((gpointer) menu_svndiff_dir, "activate",
		G_CALLBACK(svndirectory_activated), NULL);

	// Project
	menu_svndiff_project = gtk_menu_item_new_with_mnemonic(_("From Current _Project"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_project);
	gtk_tooltips_set_tip (tooltips, menu_svndiff_project,
		_("Make a diff from the current project's base path"), NULL);

	g_signal_connect((gpointer) menu_svndiff_project, "activate",
		G_CALLBACK(svnproject_activated), NULL);

	// SVN revert
	menu_svndiff_revert = gtk_menu_item_new_with_mnemonic(_("Revert changes"));
	gtk_container_add(GTK_CONTAINER (menu_svndiff_menu), menu_svndiff_revert);
	gtk_tooltips_set_tip(tooltips, menu_svndiff_revert, _("Revert all made changes at this file"), NULL);
	g_signal_connect((gpointer) menu_svndiff_revert, "activate", G_CALLBACK(svnrevert_activated), NULL);

	gtk_widget_show_all(menu_svndiff);

	plugin_fields->menu_item = menu_svndiff;
}


/* Called by Geany before unloading the plugin. */
void cleanup()
{
	// remove the menu item added in init()
	gtk_widget_destroy(plugin_fields->menu_item);
}
