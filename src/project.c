/*
 *      project.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** @file project.h
 * Project Management.
 */

#include "geany.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "project.h"
#include "projectprivate.h"

#include "dialogs.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "document.h"
#include "msgwindow.h"
#include "main.h"
#include "keyfile.h"
#include "win32.h"
#include "build.h"
#include "editor.h"
#include "stash.h"
#include "sidebar.h"
#include "filetypes.h"


ProjectPrefs project_prefs = { NULL, FALSE, FALSE };


static GeanyProjectPrivate priv;
static GeanyIndentPrefs indentation;

static GSList *stash_groups = NULL;

static struct
{
	gchar *project_file_path; /* in UTF-8 */
} local_prefs = { NULL };

static gboolean entries_modified;

/* simple struct to keep references to the elements of the properties dialog */
typedef struct _PropertyDialogElements
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *name;
	GtkWidget *description;
	GtkWidget *file_name;
	GtkWidget *base_path;
	GtkWidget *patterns;
	BuildTableData build_properties;
	gint build_page_num;
} PropertyDialogElements;


static gboolean update_config(const PropertyDialogElements *e, gboolean new_project);
static void on_file_save_button_clicked(GtkButton *button, PropertyDialogElements *e);
static gboolean load_config(const gchar *filename);
static gboolean write_config(gboolean emit_signal);
static void on_name_entry_changed(GtkEditable *editable, PropertyDialogElements *e);
static void on_entries_changed(GtkEditable *editable, PropertyDialogElements *e);
static void on_radio_long_line_custom_toggled(GtkToggleButton *radio, GtkWidget *spin_long_line);
static void apply_editor_prefs(void);
static void init_stash_prefs(void);


#define SHOW_ERR(args) dialogs_show_msgbox(GTK_MESSAGE_ERROR, args)
#define SHOW_ERR1(args, more) dialogs_show_msgbox(GTK_MESSAGE_ERROR, args, more)
#define MAX_NAME_LEN 50
/* "projects" is part of the default project base path so be careful when translating
 * please avoid special characters and spaces, look at the source for details or ask Frank */
#define PROJECT_DIR _("projects")


/* TODO: this should be ported to Glade like the project preferences dialog,
 * then we can get rid of the PropertyDialogElements struct altogether as
 * widgets pointers can be accessed through ui_lookup_widget(). */
void project_new(void)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *image;
	GtkWidget *button;
	GtkWidget *bbox;
	GtkWidget *label;
	PropertyDialogElements *e;

	if (! project_ask_close())
		return;

	g_return_if_fail(app->project == NULL);

	e = g_new0(PropertyDialogElements, 1);
	e->dialog = gtk_dialog_new_with_buttons(_("New Project"), GTK_WINDOW(main_widgets.window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	gtk_widget_set_name(e->dialog, "GeanyDialogProject");
	bbox = gtk_hbox_new(FALSE, 0);
	button = gtk_button_new();
	image = gtk_image_new_from_stock(GTK_STOCK_NEW, GTK_ICON_SIZE_BUTTON);
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
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->name = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->name));
	gtk_entry_set_max_length(GTK_ENTRY(e->name), MAX_NAME_LEN);

	ui_table_add_row(GTK_TABLE(table), 0, label, e->name, NULL);

	label = gtk_label_new(_("Filename:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->file_name = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->file_name));
	gtk_entry_set_width_chars(GTK_ENTRY(e->file_name), 30);
	button = gtk_button_new();
	g_signal_connect(button, "clicked", G_CALLBACK(on_file_save_button_clicked), e);
	image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(button), image);
	bbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start_defaults(GTK_BOX(bbox), e->file_name);
	gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);

	ui_table_add_row(GTK_TABLE(table), 1, label, bbox, NULL);

	label = gtk_label_new(_("Base path:"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	e->base_path = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(e->base_path));
	gtk_widget_set_tooltip_text(e->base_path,
		_("Base directory of all files that make up the project. "
		"This can be a new path, or an existing directory tree. "
		"You can use paths relative to the project filename."));
	bbox = ui_path_box_new(_("Choose Project Base Path"),
		GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_ENTRY(e->base_path));

	ui_table_add_row(GTK_TABLE(table), 2, label, bbox, NULL);

	gtk_container_add(GTK_CONTAINER(vbox), table);

	/* signals */
	g_signal_connect(e->name, "changed", G_CALLBACK(on_name_entry_changed), e);
	/* run the callback manually to initialise the base_path and file_name fields */
	on_name_entry_changed(GTK_EDITABLE(e->name), e);

	g_signal_connect(e->file_name, "changed", G_CALLBACK(on_entries_changed), e);
	g_signal_connect(e->base_path, "changed", G_CALLBACK(on_entries_changed), e);

	gtk_widget_show_all(e->dialog);

	while (gtk_dialog_run(GTK_DIALOG(e->dialog)) == GTK_RESPONSE_OK)
	{
		if (update_config(e, TRUE))
		{
			if (!write_config(TRUE))
				SHOW_ERR(_("Project file could not be written"));
			else
			{
				ui_set_statusbar(TRUE, _("Project \"%s\" created."), app->project->name);

				ui_add_recent_project_file(app->project->file_name);
				break;
			}
		}
	}
	gtk_widget_destroy(e->dialog);
	g_free(e);
}


gboolean project_load_file_with_session(const gchar *locale_file_name)
{
	if (project_load_file(locale_file_name))
	{
		if (project_prefs.project_session)
		{
			configuration_open_files();
			/* open a new file if no other file was opened */
			document_new_file_if_non_open();
			ui_focus_current_document();
		}
		return TRUE;
	}
	return FALSE;
}


#ifndef G_OS_WIN32
static void run_open_dialog(GtkDialog *dialog)
{
	while (gtk_dialog_run(dialog) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

		/* try to load the config */
		if (! project_load_file_with_session(filename))
		{
			gchar *utf8_filename = utils_get_utf8_from_locale(filename);

			SHOW_ERR1(_("Project file \"%s\" could not be loaded."), utf8_filename);
			gtk_widget_grab_focus(GTK_WIDGET(dialog));
			g_free(utf8_filename);
			g_free(filename);
			continue;
		}
		g_free(filename);
		break;
	}
}
#endif


void project_open(void)
{
	const gchar *dir = local_prefs.project_file_path;
#ifdef G_OS_WIN32
	gchar *file;
#else
	GtkWidget *dialog;
	GtkFileFilter *filter;
	gchar *locale_path;
#endif
	if (! project_ask_close()) return;

#ifdef G_OS_WIN32
	file = win32_show_project_open_dialog(main_widgets.window, _("Open Project"), dir, FALSE, TRUE);
	if (file != NULL)
	{
		/* try to load the config */
		if (! project_load_file_with_session(file))
		{
			SHOW_ERR1(_("Project file \"%s\" could not be loaded."), file);
		}
		g_free(file);
	}
#else

	dialog = gtk_file_chooser_dialog_new(_("Open Project"), GTK_WINDOW(main_widgets.window),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_widget_set_name(dialog, "GeanyDialogProject");

	/* set default Open, so pressing enter can open multiple files */
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_widgets.window));
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

	/* add FileFilters */
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All files"));
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Project files"));
	gtk_file_filter_add_pattern(filter, "*." GEANY_PROJECT_EXT);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	locale_path = utils_get_locale_from_utf8(dir);
	if (g_file_test(locale_path, G_FILE_TEST_EXISTS) &&
		g_file_test(locale_path, G_FILE_TEST_IS_DIR))
	{
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), locale_path);
	}
	g_free(locale_path);

	gtk_widget_show_all(dialog);
	run_open_dialog(GTK_DIALOG(dialog));
	gtk_widget_destroy(GTK_WIDGET(dialog));
#endif
}


/* Called when creating, opening, closing and updating projects. */
static void update_ui(void)
{
	if (main_status.quitting)
		return;

	ui_set_window_title(NULL);
	build_menu_update(NULL);
	sidebar_openfiles_update_all();
}


static void remove_foreach_project_filetype(gpointer data, gpointer user_data)
{
	GeanyFiletype *ft = data;
	if (ft != NULL)
	{
		SETPTR(ft->projfilecmds, NULL);
		SETPTR(ft->projexeccmds, NULL);
		SETPTR(ft->projerror_regex_string, NULL);
		ft->project_list_entry = -1;
	}
}


/* open_default will make function reload default session files on close */
void project_close(gboolean open_default)
{
	GSList *node;

	g_return_if_fail(app->project != NULL);

	ui_set_statusbar(TRUE, _("Project \"%s\" closed."), app->project->name);

	/* use write_config() to save project session files */
	if (!write_config(FALSE))
		g_warning("Project file \"%s\" could not be written", app->project->file_name);

	/* remove project filetypes build entries */
	if (app->project->build_filetypes_list != NULL)
	{
		g_ptr_array_foreach(app->project->build_filetypes_list, remove_foreach_project_filetype, NULL);
		g_ptr_array_free(app->project->build_filetypes_list, FALSE);
	}

	/* remove project non filetype build menu items */
	build_remove_menu_item(GEANY_BCS_PROJ, GEANY_GBG_NON_FT, -1);
	build_remove_menu_item(GEANY_BCS_PROJ, GEANY_GBG_EXEC, -1);

	g_free(app->project->name);
	g_free(app->project->description);
	g_free(app->project->file_name);
	g_free(app->project->base_path);

	g_free(app->project);
	app->project = NULL;

	foreach_slist(node, stash_groups)
		stash_group_free(node->data);

	g_slist_free(stash_groups);
	stash_groups = NULL;

	apply_editor_prefs(); /* ensure that global settings are restored */

	if (project_prefs.project_session)
	{
		/* close all existing tabs first */
		document_close_all();

		/* after closing all tabs let's open the tabs found in the default config */
		if (open_default && cl_options.load_session)
		{
			configuration_reload_default_session();
			configuration_open_files();
			/* open a new file if no other file was opened */
			document_new_file_if_non_open();
			ui_focus_current_document();
		}
	}
	g_signal_emit_by_name(geany_object, "project-close");

	update_ui();
}


/* Shows the file chooser dialog when base path button is clicked
 * FIXME: this should be connected in Glade but 3.8.1 has a bug
 * where it won't pass any objects as user data (#588824). */
G_MODULE_EXPORT void
on_project_properties_base_path_button_clicked(GtkWidget *button,
	GtkWidget *base_path_entry)
{
	GtkWidget *dialog;

	g_return_if_fail(base_path_entry != NULL);
	g_return_if_fail(GTK_IS_WIDGET(base_path_entry));

	dialog = gtk_file_chooser_dialog_new(_("Choose Project Base Path"),
		NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gtk_entry_set_text(GTK_ENTRY(base_path_entry),
			gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));
	}

	gtk_widget_destroy(dialog);
}


static void insert_build_page(PropertyDialogElements *e)
{
	GtkWidget *build_table, *label;
	GeanyDocument *doc = document_get_current();
	GeanyFiletype *ft = NULL;

	if (doc != NULL)
		ft = doc->file_type;

	build_table = build_commands_table(doc, GEANY_BCS_PROJ, &(e->build_properties), ft);
	gtk_container_set_border_width(GTK_CONTAINER(build_table), 6);
	label = gtk_label_new(_("Build"));
	e->build_page_num = gtk_notebook_append_page(GTK_NOTEBOOK(e->notebook),
		build_table, label);
}


static void create_properties_dialog(PropertyDialogElements *e)
{
	GtkWidget *base_path_button;
	static guint base_path_button_handler_id = 0;
	static guint radio_long_line_handler_id = 0;

	e->dialog = create_project_dialog();
	e->notebook = ui_lookup_widget(e->dialog, "project_notebook");
	e->file_name = ui_lookup_widget(e->dialog, "label_project_dialog_filename");
	e->name = ui_lookup_widget(e->dialog, "entry_project_dialog_name");
	e->description = ui_lookup_widget(e->dialog, "textview_project_dialog_description");
	e->base_path = ui_lookup_widget(e->dialog, "entry_project_dialog_base_path");
	e->patterns = ui_lookup_widget(e->dialog, "entry_project_dialog_file_patterns");

	gtk_entry_set_max_length(GTK_ENTRY(e->name), MAX_NAME_LEN);

	ui_entry_add_clear_icon(GTK_ENTRY(e->name));
	ui_entry_add_clear_icon(GTK_ENTRY(e->base_path));
	ui_entry_add_clear_icon(GTK_ENTRY(e->patterns));

	/* Workaround for bug in Glade 3.8.1, see comment above signal handler */
	if (base_path_button_handler_id == 0)
	{
		base_path_button = ui_lookup_widget(e->dialog, "button_project_dialog_base_path");
		base_path_button_handler_id =
			g_signal_connect(base_path_button, "clicked",
			G_CALLBACK(on_project_properties_base_path_button_clicked),
			e->base_path);
	}

	/* Same as above, should be in Glade but can't due to bug in 3.8.1 */
	if (radio_long_line_handler_id == 0)
	{
		radio_long_line_handler_id =
			g_signal_connect(ui_lookup_widget(e->dialog,
			"radio_long_line_custom_project"), "toggled",
			G_CALLBACK(on_radio_long_line_custom_toggled),
			ui_lookup_widget(e->dialog, "spin_long_line_project"));
	}
}


static void show_project_properties(gboolean show_build)
{
	GeanyProject *p = app->project;
	GtkWidget *widget = NULL;
	GtkWidget *radio_long_line_custom;
	static PropertyDialogElements e;
	GSList *node;

	g_return_if_fail(app->project != NULL);

	entries_modified = FALSE;

	if (e.dialog == NULL)
		create_properties_dialog(&e);

	insert_build_page(&e);

	foreach_slist(node, stash_groups)
		stash_group_display(node->data, e.dialog);

	/* fill the elements with the appropriate data */
	gtk_entry_set_text(GTK_ENTRY(e.name), p->name);
	gtk_label_set_text(GTK_LABEL(e.file_name), p->file_name);
	gtk_entry_set_text(GTK_ENTRY(e.base_path), p->base_path);

	radio_long_line_custom = ui_lookup_widget(e.dialog, "radio_long_line_custom_project");
	switch (p->long_line_behaviour)
	{
		case 0: widget = ui_lookup_widget(e.dialog, "radio_long_line_disabled_project"); break;
		case 1: widget = ui_lookup_widget(e.dialog, "radio_long_line_default_project"); break;
		case 2: widget = radio_long_line_custom; break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	widget = ui_lookup_widget(e.dialog, "spin_long_line_project");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), (gdouble)p->long_line_column);
	on_radio_long_line_custom_toggled(GTK_TOGGLE_BUTTON(radio_long_line_custom), widget);

	if (p->description != NULL)
	{	/* set text */
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e.description));
		gtk_text_buffer_set_text(buffer, p->description, -1);
	}

	if (p->file_patterns != NULL)
	{	/* set the file patterns */
		gchar *str;

		str = g_strjoinv(" ", p->file_patterns);
		gtk_entry_set_text(GTK_ENTRY(e.patterns), str);
		g_free(str);
	}

	g_signal_emit_by_name(geany_object, "project-dialog-open", e.notebook);
	gtk_widget_show_all(e.dialog);

	/* note: notebook page must be shown before setting current page */
	if (show_build)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(e.notebook), e.build_page_num);
	else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(e.notebook), 0);

	while (gtk_dialog_run(GTK_DIALOG(e.dialog)) == GTK_RESPONSE_OK)
	{
		if (update_config(&e, FALSE))
		{
			g_signal_emit_by_name(geany_object, "project-dialog-confirmed", e.notebook);
			if (!write_config(TRUE))
				SHOW_ERR(_("Project file could not be written"));
			else
			{
				ui_set_statusbar(TRUE, _("Project \"%s\" saved."), app->project->name);
				break;
			}
		}
	}

	build_free_fields(e.build_properties);
	g_signal_emit_by_name(geany_object, "project-dialog-close", e.notebook);
	gtk_notebook_remove_page(GTK_NOTEBOOK(e.notebook), e.build_page_num);
	gtk_widget_hide(e.dialog);
}


void project_properties(void)
{
	show_project_properties(FALSE);
}


void project_build_properties(void)
{
	show_project_properties(TRUE);
}


/* checks whether there is an already open project and asks the user if he wants to close it or
 * abort the current action. Returns FALSE when the current action(the caller) should be cancelled
 * and TRUE if we can go ahead */
gboolean project_ask_close(void)
{
	if (app->project != NULL)
	{
		if (dialogs_show_question_full(NULL, GTK_STOCK_CLOSE, GTK_STOCK_CANCEL,
			_("Do you want to close it before proceeding?"),
			_("The '%s' project is open."), app->project->name))
		{
			project_close(FALSE);
			return TRUE;
		}
		else
			return FALSE;
	}
	else
		return TRUE;
}


static GeanyProject *create_project(void)
{
	GeanyProject *project = g_new0(GeanyProject, 1);

	memset(&priv, 0, sizeof priv);
	priv.indentation = &indentation;
	project->priv = &priv;

	init_stash_prefs();

	project->file_patterns = NULL;

	project->long_line_behaviour = 1 /* use global settings */;
	project->long_line_column = editor_prefs.long_line_column;

	app->project = project;
	return project;
}


/* Verifies data for New & Properties dialogs.
 * Returns: FALSE if the user needs to change any data. */
static gboolean update_config(const PropertyDialogElements *e, gboolean new_project)
{
	const gchar *name, *file_name, *base_path;
	gchar *locale_filename;
	gsize name_len;
	gint err_code = 0;
	GeanyProject *p;

	g_return_val_if_fail(e != NULL, TRUE);

	name = gtk_entry_get_text(GTK_ENTRY(e->name));
	name_len = strlen(name);
	if (name_len == 0)
	{
		SHOW_ERR(_("The specified project name is too short."));
		gtk_widget_grab_focus(e->name);
		return FALSE;
	}
	else if (name_len > MAX_NAME_LEN)
	{
		SHOW_ERR1(_("The specified project name is too long (max. %d characters)."), MAX_NAME_LEN);
		gtk_widget_grab_focus(e->name);
		return FALSE;
	}

	if (new_project)
		file_name = gtk_entry_get_text(GTK_ENTRY(e->file_name));
	else
		file_name = gtk_label_get_text(GTK_LABEL(e->file_name));

	if (G_UNLIKELY(! NZV(file_name)))
	{
		SHOW_ERR(_("You have specified an invalid project filename."));
		gtk_widget_grab_focus(e->file_name);
		return FALSE;
	}

	locale_filename = utils_get_locale_from_utf8(file_name);
	base_path = gtk_entry_get_text(GTK_ENTRY(e->base_path));
	if (NZV(base_path))
	{	/* check whether the given directory actually exists */
		gchar *locale_path = utils_get_locale_from_utf8(base_path);

		if (! g_path_is_absolute(locale_path))
		{	/* relative base path, so add base dir of project file name */
			gchar *dir = g_path_get_dirname(locale_filename);
			SETPTR(locale_path, g_strconcat(dir, G_DIR_SEPARATOR_S, locale_path, NULL));
			g_free(dir);
		}

		if (! g_file_test(locale_path, G_FILE_TEST_IS_DIR))
		{
			gboolean create_dir;

			create_dir = dialogs_show_question_full(NULL, GTK_STOCK_OK, GTK_STOCK_CANCEL,
				_("Create the project's base path directory?"),
				_("The path \"%s\" does not exist."),
				base_path);

			if (create_dir)
				err_code = utils_mkdir(locale_path, TRUE);

			if (! create_dir || err_code != 0)
			{
				if (err_code != 0)
					SHOW_ERR1(_("Project base directory could not be created (%s)."),
						g_strerror(err_code));
				gtk_widget_grab_focus(e->base_path);
				utils_free_pointers(2, locale_path, locale_filename, NULL);
				return FALSE;
			}
		}
		g_free(locale_path);
	}
	/* finally test whether the given project file can be written */
	if ((err_code = utils_is_file_writable(locale_filename)) != 0 ||
		(err_code = g_file_test(locale_filename, G_FILE_TEST_IS_DIR) ? EISDIR : 0) != 0)
	{
		SHOW_ERR1(_("Project file could not be written (%s)."), g_strerror(err_code));
		gtk_widget_grab_focus(e->file_name);
		g_free(locale_filename);
		return FALSE;
	}
	g_free(locale_filename);

	if (app->project == NULL)
	{
		create_project();
		new_project = TRUE;
	}
	p = app->project;

	SETPTR(p->name, g_strdup(name));
	SETPTR(p->file_name, g_strdup(file_name));
	/* use "." if base_path is empty */
	SETPTR(p->base_path, g_strdup(NZV(base_path) ? base_path : "./"));

	if (! new_project)	/* save properties specific fields */
	{
		GtkTextIter start, end;
		GtkTextBuffer *buffer;
		GeanyDocument *doc = document_get_current();
		GeanyBuildCommand *oldvalue;
		GeanyFiletype *ft = doc ? doc->file_type : NULL;
		GtkWidget *widget;
		gchar *tmp;
		GString *str;
		GSList *node;

		/* get and set the project description */
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->description));
		gtk_text_buffer_get_start_iter(buffer, &start);
		gtk_text_buffer_get_end_iter(buffer, &end);
		SETPTR(p->description, g_strdup(gtk_text_buffer_get_text(buffer, &start, &end, FALSE)));

		foreach_slist(node, stash_groups)
			stash_group_update(node->data, e->dialog);

		/* read the project build menu */
		oldvalue = ft ? ft->projfilecmds : NULL;
		build_read_project(ft, e->build_properties);

		if (ft != NULL && ft->projfilecmds != oldvalue && ft->project_list_entry < 0)
		{
			if (p->build_filetypes_list == NULL)
				p->build_filetypes_list = g_ptr_array_new();
			ft->project_list_entry = p->build_filetypes_list->len;
			g_ptr_array_add(p->build_filetypes_list, ft);
		}
		build_menu_update(doc);

		widget = ui_lookup_widget(e->dialog, "radio_long_line_disabled_project");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			p->long_line_behaviour = 0;
		else
		{
			widget = ui_lookup_widget(e->dialog, "radio_long_line_default_project");
			if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
				p->long_line_behaviour = 1;
			else
				/* "Custom" radio button must be checked */
				p->long_line_behaviour = 2;
		}

		widget = ui_lookup_widget(e->dialog, "spin_long_line_project");
		p->long_line_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
		apply_editor_prefs();

		/* get and set the project file patterns */
		tmp = g_strdup(gtk_entry_get_text(GTK_ENTRY(e->patterns)));
		g_strfreev(p->file_patterns);
		g_strstrip(tmp);
		str = g_string_new(tmp);
		do {} while (utils_string_replace_all(str, "  ", " "));
		p->file_patterns = g_strsplit(str->str, " ", -1);
		g_string_free(str, TRUE);
		g_free(tmp);
	}

	update_ui();

	return TRUE;
}


#ifndef G_OS_WIN32
static void run_dialog(GtkWidget *dialog, GtkWidget *entry)
{
	/* set filename in the file chooser dialog */
	const gchar *utf8_filename = gtk_entry_get_text(GTK_ENTRY(entry));
	gchar *locale_filename = utils_get_locale_from_utf8(utf8_filename);

	if (g_path_is_absolute(locale_filename))
	{
		if (g_file_test(locale_filename, G_FILE_TEST_EXISTS))
		{
			/* if the current filename is a directory, we must use
			 * gtk_file_chooser_set_current_folder(which expects a locale filename) otherwise
			 * we end up in the parent directory */
			if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
				gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), locale_filename);
			else
				gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), utf8_filename);
		}
		else /* if the file doesn't yet exist, use at least the current directory */
		{
			gchar *locale_dir = g_path_get_dirname(locale_filename);
			gchar *name = g_path_get_basename(utf8_filename);

			if (g_file_test(locale_dir, G_FILE_TEST_EXISTS))
				gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), locale_dir);
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);

			g_free(name);
			g_free(locale_dir);
		}
	}
	else if (gtk_file_chooser_get_action(GTK_FILE_CHOOSER(dialog)) != GTK_FILE_CHOOSER_ACTION_OPEN)
	{
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), utf8_filename);
	}
	g_free(locale_filename);

	/* run it */
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


static void on_file_save_button_clicked(GtkButton *button, PropertyDialogElements *e)
{
#ifdef G_OS_WIN32
	gchar *path = win32_show_project_open_dialog(e->dialog, _("Choose Project Filename"),
						gtk_entry_get_text(GTK_ENTRY(e->file_name)), TRUE, TRUE);
	if (path != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(e->file_name), path);
		g_free(path);
	}
#else
	GtkWidget *dialog;

	/* initialise the dialog */
	dialog = gtk_file_chooser_dialog_new(_("Choose Project Filename"), NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_widget_set_name(dialog, "GeanyDialogProject");
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	run_dialog(dialog, e->file_name);
#endif
}


/* sets the project base path and the project file name according to the project name */
static void on_name_entry_changed(GtkEditable *editable, PropertyDialogElements *e)
{
	gchar *base_path;
	gchar *file_name;
	gchar *name;
	const gchar *project_dir = local_prefs.project_file_path;

	if (entries_modified)
		return;

	name = gtk_editable_get_chars(editable, 0, -1);
	if (NZV(name))
	{
		base_path = g_strconcat(project_dir, G_DIR_SEPARATOR_S,
			name, G_DIR_SEPARATOR_S, NULL);
		if (project_prefs.project_file_in_basedir)
			file_name = g_strconcat(project_dir, G_DIR_SEPARATOR_S, name, G_DIR_SEPARATOR_S,
				name, "." GEANY_PROJECT_EXT, NULL);
		else
			file_name = g_strconcat(project_dir, G_DIR_SEPARATOR_S,
				name, "." GEANY_PROJECT_EXT, NULL);
	}
	else
	{
		base_path = g_strconcat(project_dir, G_DIR_SEPARATOR_S, NULL);
		file_name = g_strconcat(project_dir, G_DIR_SEPARATOR_S, NULL);
	}
	g_free(name);

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


static void on_radio_long_line_custom_toggled(GtkToggleButton *radio, GtkWidget *spin_long_line)
{
	gtk_widget_set_sensitive(spin_long_line, gtk_toggle_button_get_active(radio));
}


gboolean project_load_file(const gchar *locale_file_name)
{
	g_return_val_if_fail(locale_file_name != NULL, FALSE);

	if (load_config(locale_file_name))
	{
		gchar *utf8_filename = utils_get_utf8_from_locale(locale_file_name);

		ui_set_statusbar(TRUE, _("Project \"%s\" opened."), app->project->name);

		ui_add_recent_project_file(utf8_filename);
		g_free(utf8_filename);
		return TRUE;
	}
	else
	{
		gchar *utf8_filename = utils_get_utf8_from_locale(locale_file_name);

		ui_set_statusbar(TRUE, _("Project file \"%s\" could not be loaded."), utf8_filename);
		g_free(utf8_filename);
	}
	return FALSE;
}


/* Reads the given filename and creates a new project with the data found in the file.
 * At this point there should not be an already opened project in Geany otherwise it will just
 * return.
 * The filename is expected in the locale encoding. */
static gboolean load_config(const gchar *filename)
{
	GKeyFile *config;
	GeanyProject *p;
	GSList *node;

	/* there should not be an open project */
	g_return_val_if_fail(app->project == NULL && filename != NULL, FALSE);

	config = g_key_file_new();
	if (! g_key_file_load_from_file(config, filename, G_KEY_FILE_NONE, NULL))
	{
		g_key_file_free(config);
		return FALSE;
	}

	p = create_project();

	foreach_slist(node, stash_groups)
		stash_group_load_from_key_file(node->data, config);

	p->name = utils_get_setting_string(config, "project", "name", GEANY_STRING_UNTITLED);
	p->description = utils_get_setting_string(config, "project", "description", "");
	p->file_name = utils_get_utf8_from_locale(filename);
	p->base_path = utils_get_setting_string(config, "project", "base_path", "");
	p->file_patterns = g_key_file_get_string_list(config, "project", "file_patterns", NULL, NULL);

	p->long_line_behaviour = utils_get_setting_integer(config, "long line marker",
		"long_line_behaviour", 1 /* follow global */);
	p->long_line_column = utils_get_setting_integer(config, "long line marker",
		"long_line_column", editor_prefs.long_line_column);
	apply_editor_prefs();

	build_load_menu(config, GEANY_BCS_PROJ, (gpointer)p);
	if (project_prefs.project_session)
	{
		/* save current (non-project) session (it could has been changed since program startup) */
		configuration_save_default_session();
		/* now close all open files */
		document_close_all();
		/* read session files so they can be opened with configuration_open_files() */
		configuration_load_session_files(config, FALSE);
		ui_focus_current_document();
	}
	g_signal_emit_by_name(geany_object, "project-open", config);
	g_key_file_free(config);

	update_ui();
	return TRUE;
}


static void apply_editor_prefs(void)
{
	guint i;

	foreach_document(i)
		editor_apply_update_prefs(documents[i]->editor);
}


/* Write the project settings as well as the project session files into its configuration files.
 * emit_signal defines whether the project-save signal should be emitted. When write_config()
 * is called while closing a project, this is used to skip emitting the signal because
 * project-close will be emitted afterwards.
 * Returns: TRUE if project file was written successfully. */
static gboolean write_config(gboolean emit_signal)
{
	GeanyProject *p;
	GKeyFile *config;
	gchar *filename;
	gchar *data;
	gboolean ret = FALSE;
	GSList *node;

	g_return_val_if_fail(app->project != NULL, FALSE);

	p = app->project;

	config = g_key_file_new();
	/* try to load an existing config to keep manually added comments */
	filename = utils_get_locale_from_utf8(p->file_name);
	g_key_file_load_from_file(config, filename, G_KEY_FILE_NONE, NULL);

	foreach_slist(node, stash_groups)
		stash_group_save_to_key_file(node->data, config);

	g_key_file_set_string(config, "project", "name", p->name);
	g_key_file_set_string(config, "project", "base_path", p->base_path);

	if (p->description)
		g_key_file_set_string(config, "project", "description", p->description);
	if (p->file_patterns)
		g_key_file_set_string_list(config, "project", "file_patterns",
			(const gchar**) p->file_patterns, g_strv_length(p->file_patterns));

	g_key_file_set_integer(config, "long line marker", "long_line_behaviour", p->long_line_behaviour);
	g_key_file_set_integer(config, "long line marker", "long_line_column", p->long_line_column);

	/* store the session files into the project too */
	if (project_prefs.project_session)
		configuration_save_session_files(config);
	build_save_menu(config, (gpointer)p, GEANY_BCS_PROJ);
	if (emit_signal)
	{
		g_signal_emit_by_name(geany_object, "project-save", config);
	}
	/* write the file */
	data = g_key_file_to_data(config, NULL, NULL);
	ret = (utils_write_file(filename, data) == 0);

	g_free(data);
	g_free(filename);
	g_key_file_free(config);

	return ret;
}


/* Constructs the project's base path which is used for "Make all" and "Execute".
 * The result is an absolute string in UTF-8 encoding which is either the same as
 * base path if it is absolute or it is built out of project file name's dir and base_path.
 * If there is no project or project's base_path is invalid, NULL will be returned.
 * The returned string should be freed when no longer needed. */
gchar *project_get_base_path(void)
{
	GeanyProject *project = app->project;

	if (project && NZV(project->base_path))
	{
		if (g_path_is_absolute(project->base_path))
			return g_strdup(project->base_path);
		else
		{	/* build base_path out of project file name's dir and base_path */
			gchar *path;
			gchar *dir = g_path_get_dirname(project->file_name);

			if (utils_str_equal(project->base_path, "./"))
				return dir;
			else
				path = g_strconcat(dir, G_DIR_SEPARATOR_S, project->base_path, NULL);
			g_free(dir);
			return path;
		}
	}
	return NULL;
}


/* This is to save project-related global settings, NOT project file settings. */
void project_save_prefs(GKeyFile *config)
{
	GeanyProject *project = app->project;

	if (cl_options.load_session)
	{
		const gchar *utf8_filename = (project == NULL) ? "" : project->file_name;

		g_key_file_set_string(config, "project", "session_file", utf8_filename);
	}
	g_key_file_set_string(config, "project", "project_file_path",
		NVL(local_prefs.project_file_path, ""));
}


void project_load_prefs(GKeyFile *config)
{
	if (cl_options.load_session)
	{
		g_return_if_fail(project_prefs.session_file == NULL);
		project_prefs.session_file = utils_get_setting_string(config, "project",
			"session_file", "");
	}
	local_prefs.project_file_path = utils_get_setting_string(config, "project",
		"project_file_path", NULL);
	if (local_prefs.project_file_path == NULL)
	{
		local_prefs.project_file_path = g_strconcat(g_get_home_dir(),
			G_DIR_SEPARATOR_S, PROJECT_DIR, NULL);
	}
}


/* Initialize project-related preferences in the Preferences dialog. */
void project_setup_prefs(void)
{
	GtkWidget *path_entry = ui_lookup_widget(ui_widgets.prefs_dialog, "project_file_path_entry");
	GtkWidget *path_btn = ui_lookup_widget(ui_widgets.prefs_dialog, "project_file_path_button");
	static gboolean callback_setup = FALSE;

	g_return_if_fail(local_prefs.project_file_path != NULL);

	gtk_entry_set_text(GTK_ENTRY(path_entry), local_prefs.project_file_path);
	if (! callback_setup)
	{	/* connect the callback only once */
		callback_setup = TRUE;
		ui_setup_open_button_callback(path_btn, NULL,
			GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, GTK_ENTRY(path_entry));
	}
}


/* Update project-related preferences after using the Preferences dialog. */
void project_apply_prefs(void)
{
	GtkWidget *path_entry = ui_lookup_widget(ui_widgets.prefs_dialog, "project_file_path_entry");
	const gchar *str;

	str = gtk_entry_get_text(GTK_ENTRY(path_entry));
	SETPTR(local_prefs.project_file_path, g_strdup(str));
}


static void add_stash_group(StashGroup *group)
{
	stash_groups = g_slist_prepend(stash_groups, group);
}


static void init_stash_prefs(void)
{
	StashGroup *group;
	GKeyFile *kf;

	group = stash_group_new("indentation");
	/* copy global defaults */
	indentation = *editor_get_indent_prefs(NULL);
	stash_group_set_use_defaults(group, FALSE);
	add_stash_group(group);

	stash_group_add_spin_button_integer(group, &indentation.width,
		"indent_width", 4, "spin_indent_width_project");
	stash_group_add_radio_buttons(group, (gint*)(gpointer)&indentation.type,
		"indent_type", GEANY_INDENT_TYPE_TABS,
		"radio_indent_spaces_project", GEANY_INDENT_TYPE_SPACES,
		"radio_indent_tabs_project", GEANY_INDENT_TYPE_TABS,
		"radio_indent_both_project", GEANY_INDENT_TYPE_BOTH,
		NULL);
	/* This is a 'hidden' pref for backwards-compatibility */
	stash_group_add_integer(group, &indentation.hard_tab_width,
		"indent_hard_tab_width", 8);
	stash_group_add_toggle_button(group, &indentation.detect_type,
		"detect_indent", FALSE, "check_detect_indent_type_project");
	stash_group_add_toggle_button(group, &indentation.detect_width,
		"detect_indent_width", FALSE, "check_detect_indent_width_project");
	stash_group_add_combo_box(group, (gint*)(gpointer)&indentation.auto_indent_mode,
		"indent_mode", GEANY_AUTOINDENT_CURRENTCHARS, "combo_auto_indent_mode_project");

	group = stash_group_new("file_prefs");
	stash_group_add_toggle_button(group, &priv.final_new_line,
		"final_new_line", file_prefs.final_new_line, "check_new_line1");
	stash_group_add_toggle_button(group, &priv.ensure_convert_new_lines,
		"ensure_convert_new_lines", file_prefs.ensure_convert_new_lines, "check_ensure_convert_new_lines1");
	stash_group_add_toggle_button(group, &priv.strip_trailing_spaces,
		"strip_trailing_spaces", file_prefs.strip_trailing_spaces, "check_trailing_spaces1");
	stash_group_add_toggle_button(group, &priv.replace_tabs,
		"replace_tabs", file_prefs.replace_tabs, "check_replace_tabs1");
	add_stash_group(group);
	/* apply defaults */
	kf = g_key_file_new();
	stash_group_load_from_key_file(group, kf);
	g_key_file_free(kf);
}


#define COPY_PREF(dest, prefname)\
	(dest.prefname = priv.prefname)

const GeanyFilePrefs *project_get_file_prefs(void)
{
	static GeanyFilePrefs fp;

	if (!app->project)
		return &file_prefs;

	fp = file_prefs;
	COPY_PREF(fp, final_new_line);
	COPY_PREF(fp, ensure_convert_new_lines);
	COPY_PREF(fp, strip_trailing_spaces);
	COPY_PREF(fp, replace_tabs);
	return &fp;
}


void project_init(void)
{
}


void project_finalize(void)
{
}
