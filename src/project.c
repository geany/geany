/*
 *      project.c - this file is part of Geany, a fast and lightweight IDE
 *
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
 *
 * $Id$
 */

/*
 * Project Management.
 */

#include "geany.h"

#include <string.h>

#include "project.h"
#include "dialogs.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "msgwindow.h"
#ifdef G_OS_WIN32
# include "win32.h"
#endif


static gboolean entries_modified;

// simple struct to keep references to the elements of the properties dialog
typedef struct _PropertyDialogElements
{
	GtkWidget *dialog;
	GtkWidget *name;
	GtkWidget *description;
	GtkWidget *file_name;
	GtkWidget *base_path;
	GtkWidget *run_cmd;
	GtkWidget *patterns;
} PropertyDialogElements;



static void on_properties_dialog_response(GtkDialog *dialog, gint response,
										  PropertyDialogElements *e);
static void on_file_save_button_clicked(GtkButton *button, GtkWidget *entry);
static void on_folder_open_button_clicked(GtkButton *button, GtkWidget *entry);
static void on_file_open_button_clicked(GtkButton *button, GtkWidget *entry);
#ifndef G_OS_WIN32
static void on_open_dialog_response(GtkDialog *dialog, gint response, gpointer user_data);
#endif
static gboolean close_open_project();
static gboolean load_config(const gchar *filename);
static gboolean write_config();
static void on_name_entry_changed(GtkEditable *editable, PropertyDialogElements *e);
static void on_entries_changed(GtkEditable *editable, PropertyDialogElements *e);


// avoid using __VA_ARGS__ because older gcc 2.x versions probably don't support C99
#define SHOW_ERR(args...) dialogs_show_msgbox(GTK_MESSAGE_ERROR, args)
#define MAX_NAME_LEN 50
// "projects" is part of the default project base path so be carefully when translating
// please avoid special characters and spaces, look at the source for details or ask Frank
#define PROJECT_DIR _("projects")


void project_new()
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *bbox;
	GtkWidget *label;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));
	PropertyDialogElements *e;
	gint response;

	if (! close_open_project()) return;

	g_return_if_fail(app->project == NULL);

	e = g_new0(PropertyDialogElements, 1);
	e->dialog = gtk_dialog_new_with_buttons(_("New Project"), GTK_WINDOW(app->window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	bbox = gtk_hbox_new(FALSE, 0);
	button = gtk_button_new();
	image = gtk_image_new_from_stock("gtk-new", GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new_with_mnemonic(_("C_reate"));
	gtk_box_pack_start(GTK_BOX(bbox), image, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(bbox), label, FALSE, FALSE, 3);
	gtk_container_add(GTK_CONTAINER(button), bbox);
	gtk_dialog_add_action_widget(GTK_DIALOG(e->dialog), button, GTK_RESPONSE_OK);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(e->dialog));

	entries_modified = FALSE;

	table = gtk_table_new(3, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("Name:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->name = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(e->name), MAX_NAME_LEN);
	gtk_table_attach(GTK_TABLE(table), e->name, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	label = gtk_label_new(_("Filename:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->file_name = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(e->file_name), 30);
	button = gtk_button_new();
	g_signal_connect((gpointer) button, "clicked",
				G_CALLBACK(on_file_save_button_clicked), e->file_name);
	image = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	bbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), e->file_name);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), bbox, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	label = gtk_label_new(_("Base path:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->base_path = gtk_entry_new();
	gtk_tooltips_set_tip(tooltips, e->base_path,
		_("Base directory of all files that make up the project. "
		"This can be a new path, or an existing directory tree."), NULL);
	button = gtk_button_new();
	g_signal_connect((gpointer) button, "clicked",
				G_CALLBACK(on_folder_open_button_clicked), e->base_path);
	image = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	bbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), e->base_path);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), bbox, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	gtk_container_add(GTK_CONTAINER(vbox), table);

	// signals
	g_signal_connect((gpointer) e->name, "changed", G_CALLBACK(on_name_entry_changed), e);
	// run the callback manually to initialise the base_path and file_name fields
	on_name_entry_changed(GTK_EDITABLE(e->name), e);

	g_signal_connect((gpointer) e->file_name, "changed", G_CALLBACK(on_entries_changed), e);
	g_signal_connect((gpointer) e->base_path, "changed", G_CALLBACK(on_entries_changed), e);

	gtk_widget_show_all(e->dialog);
	response = gtk_dialog_run(GTK_DIALOG(e->dialog));
	on_properties_dialog_response(GTK_DIALOG(e->dialog), response, e);
}


void project_open()
{
	gchar *dir = g_strconcat(GEANY_HOME_DIR, G_DIR_SEPARATOR_S, PROJECT_DIR, NULL);
#ifdef G_OS_WIN32
	gchar *file;
#else
	GtkWidget *dialog;
	GtkFileFilter *filter;
	gint response;
#endif
	if (! close_open_project()) return;

#ifdef G_OS_WIN32
	file = win32_show_project_open_dialog(_("Open Project"), dir, FALSE);
	if (file != NULL)
	{
		load_config(file);
		g_free(file);
	}
#else

	dialog = gtk_file_chooser_dialog_new(_("Open Project"), GTK_WINDOW(app->window),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	// set default Open, so pressing enter can open multiple files
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(app->window));
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

	// add FileFilters
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Project files"));
	gtk_file_filter_add_pattern(filter, "*." GEANY_PROJECT_EXT);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);

	gtk_widget_show_all(dialog);
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	on_open_dialog_response(GTK_DIALOG(dialog), response, NULL);
#endif

	g_free(dir);
}


void project_close()
{
	g_return_if_fail(app->project != NULL);

	/// TODO handle open project files

	write_config();
	msgwin_status_add(_("Project \"%s\" closed."), app->project->name);

	g_free(app->project->name);
	g_free(app->project->description);
	g_free(app->project->file_name);
	g_free(app->project->base_path);
	g_free(app->project->run_cmd);

	g_free(app->project);
	app->project = NULL;
}


void project_properties()
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *bbox;
	GtkWidget *label;
	GtkWidget *swin;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));
	PropertyDialogElements *e = g_new(PropertyDialogElements, 1);
	GeanyProject *p = app->project;
	gint response;

	g_return_if_fail(app->project != NULL);

	e->dialog = gtk_dialog_new_with_buttons(_("Project Properties"), GTK_WINDOW(app->window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	gtk_dialog_add_buttons(GTK_DIALOG(e->dialog), GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(e->dialog));

	entries_modified = FALSE;

	table = gtk_table_new(6, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("Name:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->name = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(e->name), MAX_NAME_LEN);
	gtk_table_attach(GTK_TABLE(table), e->name, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	label = gtk_label_new(_("Filename:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->file_name = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(e->file_name), FALSE);	// read-only
	gtk_table_attach(GTK_TABLE(table), e->file_name, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	label = gtk_label_new(_("Description:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->description = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(e->description), GTK_WRAP_WORD);
	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(swin, 250, 80);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(e->description));
	gtk_table_attach(GTK_TABLE(table), swin, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	label = gtk_label_new(_("Base path:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->base_path = gtk_entry_new();
	button = gtk_button_new();
	g_signal_connect((gpointer) button, "clicked",
				G_CALLBACK(on_folder_open_button_clicked), e->base_path);
	image = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	bbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), e->base_path);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), bbox, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	label = gtk_label_new(_("Run command:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->run_cmd = gtk_entry_new();
	gtk_tooltips_set_tip(tooltips, e->run_cmd,
		_("Command-line to run in the project base directory. "
		"Options can be appended to the command. "
		"Leave blank to use the default run command."), NULL);
	button = gtk_button_new();
	g_signal_connect((gpointer) button, "clicked",
				G_CALLBACK(on_file_open_button_clicked), e->run_cmd);
	image = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	bbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), e->run_cmd);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), bbox, 1, 2, 4, 5,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	label = gtk_label_new(_("File patterns:"));
	// <small>Separate multiple patterns by a new line</small>
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->patterns = gtk_text_view_new();
	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(swin, -1, 80);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
				GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(e->patterns));
	gtk_table_attach(GTK_TABLE(table), swin, 1, 2, 5, 6,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	gtk_container_add(GTK_CONTAINER(vbox), table);

	// fill the elements with the appropriate data
	gtk_entry_set_text(GTK_ENTRY(e->name), p->name);

	if (p->description != NULL)
	{	// set text
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->description));
		gtk_text_buffer_set_text(buffer, p->description, -1);
	}

	if (p->file_patterns != NULL)
	{	// set the file patterns
		gint i;
		gint len = g_strv_length(p->file_patterns);
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->patterns));
		GString *str = g_string_sized_new(len * 4);

		for (i = 0; i < len; i++)
		{
			if (p->file_patterns[i] != NULL)
			{
				g_string_append(str, p->file_patterns[i]);
				g_string_append_c(str, '\n');
			}
		}
		gtk_text_buffer_set_text(buffer, str->str, -1);
		g_string_free(str, TRUE);
	}

	gtk_entry_set_text(GTK_ENTRY(e->file_name), p->file_name);
	gtk_entry_set_text(GTK_ENTRY(e->base_path), p->base_path);
	if (p->run_cmd != NULL)
		gtk_entry_set_text(GTK_ENTRY(e->run_cmd), p->run_cmd);

	gtk_widget_show_all(e->dialog);
	response = gtk_dialog_run(GTK_DIALOG(e->dialog));
	on_properties_dialog_response(GTK_DIALOG(e->dialog), response, e);
}


/* checks whether there is an already open project and asks the user if he wants to close it or
 * abort the current action. Returns FALSE when the current action(the caller) should be cancelled
 * and TRUE if we can go ahead */
static gboolean close_open_project()
{
	if (app->project != NULL)
	{
		gchar *msg =
			_("The '%s' project is already open. "
				"Do you want to close it before proceeding?");

		if (dialogs_show_question(msg, app->project->name))
		{
			project_close();
			return TRUE;
		}
		else
			return FALSE;
	}
	else
		return TRUE;
}


/* Also used for New Project dialog response. */
static void on_properties_dialog_response(GtkDialog *dialog, gint response,
										  PropertyDialogElements *e)
{
	if (response == GTK_RESPONSE_OK && e != NULL)
	{
		const gchar *name, *file_name, *base_path;
		gint name_len;
		gboolean new_project = FALSE;
		GeanyProject *p;

		name = gtk_entry_get_text(GTK_ENTRY(e->name));
		name_len = strlen(name);
		if (name_len == 0)
		{
			SHOW_ERR(_("The specified project name is too short."));
			gtk_widget_grab_focus(e->name);
			return;
		}
		else if (name_len > MAX_NAME_LEN)
		{
			SHOW_ERR(_("The specified project name is too long (max. %d characters)."), MAX_NAME_LEN);
			gtk_widget_grab_focus(e->name);
			return;
		}

		file_name = gtk_entry_get_text(GTK_ENTRY(e->file_name));
		if (strlen(file_name) == 0)
		{
			SHOW_ERR(_("You have specified an invalid project filename."));
			gtk_widget_grab_focus(e->file_name);
			return;
		}

		base_path = gtk_entry_get_text(GTK_ENTRY(e->base_path));
		if (strlen(base_path) == 0)
		{
			SHOW_ERR(_("You have specified an invalid project base path."));
			gtk_widget_grab_focus(e->base_path);
			return;
		}
		else
		{	// check whether the given directory actually exists
			gchar *locale_path = utils_get_locale_from_utf8(base_path);
			if (! g_file_test(locale_path, G_FILE_TEST_IS_DIR))
			{
				if (dialogs_show_question(
					_("The specified project base path does not exist. Should it be created?")))
				{
					utils_mkdir(locale_path, TRUE);
				}
				else
				{
					g_free(locale_path);
					gtk_widget_grab_focus(e->base_path);
					return;
				}
			}
			g_free(locale_path);
		}

		// finally test whether the given project file can be written
		if (utils_write_file(file_name, "") != 0)
		{
			SHOW_ERR(_("Project file could not be written."));
			gtk_widget_grab_focus(e->file_name);
			return;
		}

		if (app->project == NULL)
		{
			app->project = g_new0(GeanyProject, 1);
			new_project = TRUE;
		}
		p = app->project;

		if (p->name != NULL) g_free(p->name);
		p->name = g_strdup(name);

		if (p->file_name != NULL) g_free(p->file_name);
		p->file_name = g_strdup(file_name);

		if (p->base_path != NULL) g_free(p->base_path);
		p->base_path = g_strdup(base_path);

		if (! new_project)	// save properties specific fields
		{
			GtkTextIter start, end;
			gchar *tmp;
			GtkTextBuffer *buffer;

			if (p->run_cmd != NULL) g_free(p->run_cmd);
			p->run_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(e->run_cmd)));

			// get and set the project description
			buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->description));
			gtk_text_buffer_get_start_iter(buffer, &start);
			gtk_text_buffer_get_end_iter(buffer, &end);
			g_free(p->description);
			p->description = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

			// get and set the project file patterns
			buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->patterns));
			gtk_text_buffer_get_start_iter(buffer, &start);
			gtk_text_buffer_get_end_iter(buffer, &end);
			tmp = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
			g_strfreev(p->file_patterns);
			p->file_patterns = g_strsplit(tmp, "\n", -1);
			g_free(tmp);
		}
		write_config();
		if (new_project)
			msgwin_status_add(_("Project \"%s\" created."), p->name);
		else
			msgwin_status_add(_("Project \"%s\" saved."), p->name);
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(e);
}


#ifndef G_OS_WIN32
static void run_dialog(GtkWidget *dialog, GtkWidget *entry)
{
	// set filename in the file chooser dialog
	const gchar *utf8_filename = gtk_entry_get_text(GTK_ENTRY(entry));
	gchar *locale_filename = utils_get_locale_from_utf8(utf8_filename);

	if (g_path_is_absolute(locale_filename))
	{
		if (g_file_test(locale_filename, G_FILE_TEST_EXISTS))
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), utf8_filename);
	}
	else
	if (gtk_file_chooser_get_action(GTK_FILE_CHOOSER(dialog)) != GTK_FILE_CHOOSER_ACTION_OPEN)
	{
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), utf8_filename);
	}
	g_free(locale_filename);

	// run it
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		gchar *tmp_utf8_filename = utils_get_utf8_from_locale(filename);

		gtk_entry_set_text(GTK_ENTRY(entry), tmp_utf8_filename);

		g_free(tmp_utf8_filename);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}
#endif


static void on_file_save_button_clicked(GtkButton *button, GtkWidget *entry)
{
#ifdef G_OS_WIN32
	gchar *path = win32_show_project_open_dialog(_("Choose Project Filename"),
						gtk_entry_get_text(GTK_ENTRY(entry)), TRUE);
	if (path != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), path);
		g_free(path);
	}
#else
	GtkWidget *dialog;

	// initialise the dialog
	dialog = gtk_file_chooser_dialog_new(_("Choose Project Filename"), NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	run_dialog(dialog, entry);
#endif
}


static void on_folder_open_button_clicked(GtkButton *button, GtkWidget *entry)
{
#ifdef G_OS_WIN32
	gchar *path = win32_show_project_folder_dialog(_("Choose Project Base Path"),
						gtk_entry_get_text(GTK_ENTRY(entry)));
	if (path != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), path);
		g_free(path);
	}
#else
	GtkWidget *dialog;

	// initialise the dialog
	dialog = gtk_file_chooser_dialog_new(_("Choose Project Base Path"), NULL,
					GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	run_dialog(dialog, entry);
#endif
}


static void on_file_open_button_clicked(GtkButton *button, GtkWidget *entry)
{
#ifdef G_OS_WIN32
	gchar *path = win32_show_project_open_dialog(_("Choose Project Run Command"),
						gtk_entry_get_text(GTK_ENTRY(entry)), FALSE);
	if (path != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), path);
		g_free(path);
	}
#else
	GtkWidget *dialog;

	// initialise the dialog
	dialog = gtk_file_chooser_dialog_new(_("Choose Project Run Command"), NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	run_dialog(dialog, entry);
#endif
}


/* sets the project base path and the project file name according to the project name */
static void on_name_entry_changed(GtkEditable *editable, PropertyDialogElements *e)
{
	gchar *base_path;
	gchar *file_name;
	gchar *name;

	if (entries_modified)
		return;

	name = gtk_editable_get_chars(editable, 0, -1);
	if (name != NULL && strlen(name) > 0)
	{
		base_path = g_strconcat(
			GEANY_HOME_DIR, G_DIR_SEPARATOR_S, PROJECT_DIR, G_DIR_SEPARATOR_S,
			name, G_DIR_SEPARATOR_S, NULL);
		file_name = g_strconcat(
			GEANY_HOME_DIR, G_DIR_SEPARATOR_S, PROJECT_DIR, G_DIR_SEPARATOR_S,
			name, "." GEANY_PROJECT_EXT, NULL);
		g_free(name);
	}
	else
	{
		base_path = g_strconcat(
			GEANY_HOME_DIR, G_DIR_SEPARATOR_S, PROJECT_DIR, G_DIR_SEPARATOR_S, NULL);
		file_name = g_strconcat(
			GEANY_HOME_DIR, G_DIR_SEPARATOR_S, PROJECT_DIR, G_DIR_SEPARATOR_S, NULL);
	}

	gtk_entry_set_text(GTK_ENTRY(e->base_path), base_path);
	gtk_entry_set_text(GTK_ENTRY(e->file_name), file_name);

	entries_modified = FALSE;

	g_free(base_path);
	g_free(file_name);
}


static void on_entries_changed(GtkEditable *editable, PropertyDialogElements *e)
{
	entries_modified = TRUE;
}


#ifndef G_OS_WIN32
static void on_open_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		// try to load the config
		if (load_config(filename))
		{
			gtk_widget_destroy(GTK_WIDGET(dialog));
			msgwin_status_add(_("Project \"%s\" opened."), app->project->name);
		}
		else
		{
			SHOW_ERR(_("Project file could not be loaded."));
			gtk_widget_grab_focus(GTK_WIDGET(dialog));
		}
		g_free(filename);
	}
	else
		gtk_widget_destroy(GTK_WIDGET(dialog));
}
#endif


/* Reads the given filename and creates a new project with the data found in the file.
 * At this point there should not be an already opened project in Geany otherwise it will just
 * return.
 * The filename is expected in the locale encoding. */
static gboolean load_config(const gchar *filename)
{
	GKeyFile *config;
	GeanyProject *p;

	// there should not be an open project
	g_return_val_if_fail(app->project == NULL && filename != NULL, FALSE);

	p = app->project = g_new0(GeanyProject, 1);

	config = g_key_file_new();
	if (! g_key_file_load_from_file(config, filename, G_KEY_FILE_KEEP_COMMENTS, NULL))
	{
		g_key_file_free(config);
		return FALSE;
	}

	p->name = utils_get_setting_string(config, "project", "name", GEANY_STRING_UNTITLED);
	p->description = utils_get_setting_string(config, "project", "description", "");
	p->file_name = utils_get_utf8_from_locale(filename);
	p->base_path = utils_get_setting_string(config, "project", "base_path", "");
	p->run_cmd = utils_get_setting_string(config, "project", "run_cmd", "");
	p->file_patterns = g_key_file_get_string_list(config, "project", "file_patterns", NULL, NULL);

	g_key_file_free(config);

	return TRUE;
}


// Returns: TRUE if project file was written successfully.
static gboolean write_config()
{
	GeanyProject *p;
	GKeyFile *config;
	gchar *filename;
	gchar *data;
	gboolean ret = FALSE;

	g_return_val_if_fail(app->project != NULL, FALSE);

	p = app->project;

	config = g_key_file_new();
	// try to load an existing config to keep manually added comments
	filename = utils_get_locale_from_utf8(p->file_name);
	g_key_file_load_from_file(config, filename, G_KEY_FILE_KEEP_COMMENTS, NULL);

	g_key_file_set_string(config, "project", "name", p->name);
	g_key_file_set_string(config, "project", "base_path", p->base_path);

	if (p->description)
		g_key_file_set_string(config, "project", "description", p->description);
	if (p->run_cmd)
		g_key_file_set_string(config, "project", "run_cmd", p->run_cmd);
	if (p->file_patterns)
		g_key_file_set_string_list(config, "project", "file_patterns",
			(const gchar**) p->file_patterns, g_strv_length(p->file_patterns));

	// write the file
	data = g_key_file_to_data(config, NULL, NULL);
	ret = (utils_write_file(filename, data) == 0);

	g_free(data);
	g_free(filename);
	g_key_file_free(config);

	return ret;
}


const gchar *project_get_make_dir()
{
	if (app->project != NULL)
		return app->project->base_path;
	else
		return NULL;
}


