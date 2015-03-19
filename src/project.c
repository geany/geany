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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/** @file project.h
 * Project Management.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "project.h"

#include "app.h"
#include "build.h"
#include "dialogs.h"
#include "document.h"
#include "editor.h"
#include "filetypesprivate.h"
#include "geanyobject.h"
#include "keyfile.h"
#include "main.h"
#include "projectprivate.h"
#include "sidebar.h"
#include "stash.h"
#include "support.h"
#include "ui_utils.h"
#include "utils.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>


ProjectPrefs project_prefs = { NULL, FALSE };


static GeanyProjectPrivate priv;
static GeanyIndentPrefs indentation;

static GSList *stash_groups = NULL;

static struct
{
	gchar *project_file_path; /* in UTF-8 */
} local_prefs = { NULL };

/* simple struct to keep references to the elements of the properties dialog */
typedef struct _PropertyDialogElements
{
	GtkWidget *dialog;
	GtkWidget *notebook;
	GtkWidget *name;
	GtkWidget *description;
	GtkWidget *directory;
	GtkWidget *patterns;
	BuildTableData build_properties;
	gint build_page_num;
} PropertyDialogElements;


static gboolean update_config(const PropertyDialogElements *e);
static gboolean load_config(const gchar *filename);
static gboolean write_config(gboolean emit_signal);
static void on_radio_long_line_custom_toggled(GtkToggleButton *radio, GtkWidget *spin_long_line);
static void apply_editor_prefs(void);
static void init_stash_prefs(void);
static void destroy_project(gboolean open_default);
static GeanyProject *create_project(void);
static void update_ui(void);
static gboolean show_project_properties(gboolean show_build);


#define SHOW_ERR(args) dialogs_show_msgbox(GTK_MESSAGE_ERROR, args)
#define SHOW_ERR1(args, more) dialogs_show_msgbox(GTK_MESSAGE_ERROR, args, more)
#define MAX_NAME_LEN 50
/* "projects" is part of the default project base path so be careful when translating
 * please avoid special characters and spaces, look at the source for details or ask Frank */
#define PROJECT_DIR _("projects")


// returns whether we have working documents open
static gboolean have_session_docs(void)
{
	gint npages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	GeanyDocument *doc = document_get_current();

	return npages > 1 || (npages == 1 && (doc->file_name || doc->changed));
}


static void project_new(gchar *file_name)
{
	gboolean config_written;

	if (!app->project && project_prefs.project_session)
	{
		/* save session in case the dialog is cancelled */
		configuration_save_default_session();
		/* don't ask if the only doc is an unmodified new doc */
		if (have_session_docs())
		{
			if (dialogs_show_question(
				_("Move the current documents into the new project's session?")))
			{
				// don't reload session on closing project
				configuration_clear_default_session();
			}
			else
			{
				if (!document_close_all())
					return;
			}
		}
	}

	g_return_if_fail(app->project == NULL);
	create_project();

	// app->project is now set
	SETPTR(app->project->file_name, g_strdup(file_name));
	SETPTR(app->project->base_path, g_path_get_dirname(file_name));
	SETPTR(app->project->name, g_path_get_basename(app->project->base_path));

	update_ui();
	config_written = show_project_properties(FALSE);

	// config may not be written if the user presses cancel in the Properties dialog
	if (!config_written)
		config_written = write_config(TRUE);

	if (!config_written)
	{
		SHOW_ERR(_("Project file could not be written"));
		destroy_project(FALSE);
	}
	else
	{
		ui_set_statusbar(TRUE, _("Project \"%s\" created."), app->project->name);
		ui_add_recent_project_file(app->project->base_path);
	}

	document_new_file_if_non_open();
	ui_focus_current_document();
}


gboolean project_load_file_with_session(const gchar *locale_file_name)
{
	if (project_load_file(locale_file_name))
	{
		if (project_prefs.project_session)
		{
			configuration_open_files();
			document_new_file_if_non_open();
			ui_focus_current_document();
		}
		return TRUE;
	}
	return FALSE;
}


/* Rename old *.geany projects to project.geany */
static gboolean convert_old_project(const gchar *dirname)
{
	GDir *dir;
	const gchar *fname;
	gboolean converted = FALSE;

	dir = g_dir_open (dirname, 0, NULL);
	while ((fname = g_dir_read_name(dir)) != NULL)
	{
		gchar *absname = g_build_filename(dirname, fname, NULL);

		if (g_file_test(absname, G_FILE_TEST_IS_REGULAR) && g_str_has_suffix(fname, ".geany"))
		{
			gchar *utf8_fname = utils_get_utf8_from_locale(fname);

			if (dialogs_show_question_full(NULL, GTK_STOCK_YES, GTK_STOCK_NO,
				_("Do you want to rename it to 'project.geany' and open it? A new project will be created otherwise."),
				_("Old Geany project '%s' found."), utf8_fname))
			{
				gchar *new_name = g_build_filename(dirname, GEANY_PROJECT_FILENAME, NULL);
				g_rename(absname, new_name);
				g_free(new_name);
				converted = TRUE;
			}

			g_free(utf8_fname);
			g_free(absname);
			break;
		}
		g_free(absname);
	}
	g_dir_close(dir);

	return converted;
}


void project_open(void)
{
	gchar *dirname;

	dirname = dialogs_show_open_dialog(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, NULL,
		local_prefs.project_file_path, TRUE);
	SETPTR(dirname, utils_get_locale_from_utf8(dirname));

	if (dirname != NULL && project_ask_close())
	{
		gchar *filename;
		gboolean project_exists;

		filename = g_build_filename(dirname, GEANY_PROJECT_FILENAME, NULL);
		project_exists = g_file_test(filename, G_FILE_TEST_IS_REGULAR);

		if (!project_exists)
			project_exists = convert_old_project(dirname);

		if (project_exists)
		{
			/* try to load the config */
			if (!project_load_file_with_session(filename))
			{
				gchar *utf8_filename = utils_get_utf8_from_locale(filename);
				SHOW_ERR1(_("Project file \"%s\" could not be loaded."), utf8_filename);
				g_free(utf8_filename);
			}
		}
		else
			project_new(filename);

		g_free(filename);
		g_free(dirname);
	}
}


/* Called when creating, opening, closing and updating projects. */
static void update_ui(void)
{
	if (main_status.quitting)
		return;

	ui_set_window_title(NULL);
	build_menu_update(NULL);
	// update project name
	sidebar_openfiles_update_all();
	ui_update_recent_project_menu();
}


static void remove_foreach_project_filetype(gpointer data, gpointer user_data)
{
	GeanyFiletype *ft = data;
	if (ft != NULL)
	{
		SETPTR(ft->priv->projfilecmds, NULL);
		SETPTR(ft->priv->projexeccmds, NULL);
		SETPTR(ft->priv->projerror_regex_string, NULL);
		ft->priv->project_list_entry = -1;
	}
}


/* open_default will make function reload default session files on close */
gboolean project_close(gboolean open_default)
{
	g_return_val_if_fail(app->project != NULL, FALSE);

	/* save project session files, etc */
	if (!write_config(FALSE))
		g_warning("Project file \"%s\" could not be written", app->project->file_name);

	if (project_prefs.project_session)
	{
		/* close all existing tabs first */
		if (!document_close_all())
			return FALSE;
	}
	ui_set_statusbar(TRUE, _("Project \"%s\" closed."), app->project->name);
	destroy_project(open_default);
	return TRUE;
}


static void destroy_project(gboolean open_default)
{
	GSList *node;

	g_return_if_fail(app->project != NULL);

	/* remove project filetypes build entries */
	if (app->project->priv->build_filetypes_list != NULL)
	{
		g_ptr_array_foreach(app->project->priv->build_filetypes_list, remove_foreach_project_filetype, NULL);
		g_ptr_array_free(app->project->priv->build_filetypes_list, FALSE);
	}

	/* remove project non filetype build menu items */
	build_remove_menu_item(GEANY_BCS_PROJ, GEANY_GBG_NON_FT, -1);
	build_remove_menu_item(GEANY_BCS_PROJ, GEANY_GBG_EXEC, -1);

	g_free(app->project->name);
	g_free(app->project->description);
	g_free(app->project->file_name);
	g_free(app->project->base_path);
	g_strfreev(app->project->file_patterns);

	g_free(app->project);
	app->project = NULL;

	foreach_slist(node, stash_groups)
		stash_group_free(node->data);

	g_slist_free(stash_groups);
	stash_groups = NULL;

	apply_editor_prefs(); /* ensure that global settings are restored */

	if (project_prefs.project_session)
	{
		/* after closing all tabs let's open the tabs found in the default config */
		if (open_default && cl_options.load_session)
		{
			configuration_reload_default_session();
			configuration_open_files();
			document_new_file_if_non_open();
			ui_focus_current_document();
		}
	}
	g_signal_emit_by_name(geany_object, "project-close");

	update_ui();
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
	static guint radio_long_line_handler_id = 0;

	e->dialog = create_project_dialog();
	e->notebook = ui_lookup_widget(e->dialog, "project_notebook");
	e->directory = ui_lookup_widget(e->dialog, "label_project_dialog_directory");
	e->name = ui_lookup_widget(e->dialog, "entry_project_dialog_name");
	e->description = ui_lookup_widget(e->dialog, "textview_project_dialog_description");
	e->patterns = ui_lookup_widget(e->dialog, "entry_project_dialog_file_patterns");

	gtk_entry_set_max_length(GTK_ENTRY(e->name), MAX_NAME_LEN);

	ui_entry_add_clear_icon(GTK_ENTRY(e->name));
	ui_entry_add_clear_icon(GTK_ENTRY(e->patterns));

	/* Workaround for bug in Glade 3.8.1, see comment above signal handler */
	if (radio_long_line_handler_id == 0)
	{
		GtkWidget *wid = ui_lookup_widget(e->dialog, "radio_long_line_custom_project");
		radio_long_line_handler_id =
			g_signal_connect(wid, "toggled",
				G_CALLBACK(on_radio_long_line_custom_toggled),
				ui_lookup_widget(e->dialog, "spin_long_line_project"));
	}
}


static gboolean show_project_properties(gboolean show_build)
{
	GeanyProject *p = app->project;
	GtkWidget *widget = NULL;
	GtkWidget *radio_long_line_custom;
	static PropertyDialogElements e;
	GSList *node;
	gchar *entry_text;
	GtkTextBuffer *buffer;
	gboolean config_written = FALSE;

	g_return_if_fail(app->project != NULL);

	if (e.dialog == NULL)
		create_properties_dialog(&e);

	insert_build_page(&e);

	foreach_slist(node, stash_groups)
		stash_group_display(node->data, e.dialog);

	/* fill the elements with the appropriate data */
	gtk_entry_set_text(GTK_ENTRY(e.name), p->name);
	gtk_label_set_text(GTK_LABEL(e.directory), p->base_path);

	radio_long_line_custom = ui_lookup_widget(e.dialog, "radio_long_line_custom_project");
	switch (p->priv->long_line_behaviour)
	{
		case 0: widget = ui_lookup_widget(e.dialog, "radio_long_line_disabled_project"); break;
		case 1: widget = ui_lookup_widget(e.dialog, "radio_long_line_default_project"); break;
		case 2: widget = radio_long_line_custom; break;
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);

	widget = ui_lookup_widget(e.dialog, "spin_long_line_project");
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), (gdouble)p->priv->long_line_column);
	on_radio_long_line_custom_toggled(GTK_TOGGLE_BUTTON(radio_long_line_custom), widget);

	/* set text */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e.description));
	gtk_text_buffer_set_text(buffer, p->description ? p->description : "", -1);

	/* set the file patterns */
	entry_text = p->file_patterns ? g_strjoinv(" ", p->file_patterns) : g_strdup("");
	gtk_entry_set_text(GTK_ENTRY(e.patterns), entry_text);
	g_free(entry_text);

	g_signal_emit_by_name(geany_object, "project-dialog-open", e.notebook);
	gtk_widget_show_all(e.dialog);

	/* note: notebook page must be shown before setting current page */
	if (show_build)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(e.notebook), e.build_page_num);
	else
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(e.notebook), 0);
		gtk_widget_grab_focus(e.name);
	}

	while (gtk_dialog_run(GTK_DIALOG(e.dialog)) == GTK_RESPONSE_OK)
	{
		if (update_config(&e))
		{
			g_signal_emit_by_name(geany_object, "project-dialog-confirmed", e.notebook);
			if (!write_config(TRUE))
				SHOW_ERR(_("Project file could not be written"));
			else
			{
				ui_set_statusbar(TRUE, _("Project \"%s\" saved."), app->project->name);
				config_written = TRUE;
				break;
			}
		}
	}

	build_free_fields(e.build_properties);
	g_signal_emit_by_name(geany_object, "project-dialog-close", e.notebook);
	gtk_notebook_remove_page(GTK_NOTEBOOK(e.notebook), e.build_page_num);
	gtk_widget_hide(e.dialog);
	return config_written;
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
			return project_close(FALSE);
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

	project->priv->long_line_behaviour = 1 /* use global settings */;
	project->priv->long_line_column = editor_prefs.long_line_column;

	app->project = project;
	return project;
}


/* Verifies data for the Properties dialog.
 * Returns: FALSE if the user needs to change any data. */
static gboolean update_config(const PropertyDialogElements *e)
{
	const gchar *name;
	gsize name_len;
	gint err_code = 0;
	GeanyProject *p;
	GtkTextIter start, end;
	GtkTextBuffer *buffer;
	GeanyDocument *doc = document_get_current();
	GeanyBuildCommand *oldvalue;
	GeanyFiletype *ft = doc ? doc->file_type : NULL;
	GtkWidget *widget;
	gchar *tmp;
	GString *str;
	GSList *node;

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

	p = app->project;
	SETPTR(p->name, g_strdup(name));

	/* get and set the project description */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(e->description));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	SETPTR(p->description, gtk_text_buffer_get_text(buffer, &start, &end, FALSE));

	foreach_slist(node, stash_groups)
		stash_group_update(node->data, e->dialog);

	/* read the project build menu */
	oldvalue = ft ? ft->priv->projfilecmds : NULL;
	build_read_project(ft, e->build_properties);

	if (ft != NULL && ft->priv->projfilecmds != oldvalue && ft->priv->project_list_entry < 0)
	{
		if (p->priv->build_filetypes_list == NULL)
			p->priv->build_filetypes_list = g_ptr_array_new();
		ft->priv->project_list_entry = p->priv->build_filetypes_list->len;
		g_ptr_array_add(p->priv->build_filetypes_list, ft);
	}
	build_menu_update(doc);

	widget = ui_lookup_widget(e->dialog, "radio_long_line_disabled_project");
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		p->priv->long_line_behaviour = 0;
	else
	{
		widget = ui_lookup_widget(e->dialog, "radio_long_line_default_project");
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
			p->priv->long_line_behaviour = 1;
		else
			/* "Custom" radio button must be checked */
			p->priv->long_line_behaviour = 2;
	}

	widget = ui_lookup_widget(e->dialog, "spin_long_line_project");
	p->priv->long_line_column = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
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

	update_ui();

	return TRUE;
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

		ui_add_recent_project_file(app->project->base_path);
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
	p->base_path = g_path_get_dirname(filename);
	p->file_patterns = g_key_file_get_string_list(config, "project", "file_patterns", NULL, NULL);

	p->priv->long_line_behaviour = utils_get_setting_integer(config, "long line marker",
		"long_line_behaviour", 1 /* follow global */);
	p->priv->long_line_column = utils_get_setting_integer(config, "long line marker",
		"long_line_column", editor_prefs.long_line_column);
	apply_editor_prefs();

	build_load_menu(config, GEANY_BCS_PROJ, (gpointer)p);
	if (project_prefs.project_session)
	{
		/* save current (non-project) session (it could have been changed since program startup) */
		configuration_save_default_session();
		/* now close all open files */
		document_close_all();
		/* read session files so they can be opened with configuration_open_files() */
		configuration_load_session_files(config, FALSE);
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
	/* Not used any more but preserved so old Geany versions can use new projects */
	g_key_file_set_string(config, "project", "base_path", p->base_path);

	if (p->description)
		g_key_file_set_string(config, "project", "description", p->description);
	if (p->file_patterns)
		g_key_file_set_string_list(config, "project", "file_patterns",
			(const gchar**) p->file_patterns, g_strv_length(p->file_patterns));

	// editor settings
	g_key_file_set_integer(config, "long line marker", "long_line_behaviour", p->priv->long_line_behaviour);
	g_key_file_set_integer(config, "long line marker", "long_line_column", p->priv->long_line_column);

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


/** Forces the project file rewrite and emission of the project-save signal. Plugins 
 * can use this function to save additional project data outside the project dialog.
 *
 *  @since 1.25
 */
void project_write_config(void)
{
	if (!write_config(TRUE))
		SHOW_ERR(_("Project file could not be written"));
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
		FALLBACK(local_prefs.project_file_path, ""));
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
		local_prefs.project_file_path = g_build_filename(g_get_home_dir(), PROJECT_DIR, NULL);
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


static void add_stash_group(StashGroup *group, gboolean apply_defaults)
{
	GKeyFile *kf;

	stash_groups = g_slist_prepend(stash_groups, group);
	if (!apply_defaults)
		return;

	kf = g_key_file_new();
	stash_group_load_from_key_file(group, kf);
	g_key_file_free(kf);
}


static void init_stash_prefs(void)
{
	StashGroup *group;

	group = stash_group_new("indentation");
	/* copy global defaults */
	indentation = *editor_get_indent_prefs(NULL);
	stash_group_set_use_defaults(group, FALSE);
	add_stash_group(group, FALSE);

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
	add_stash_group(group, TRUE);

	group = stash_group_new("editor");
	stash_group_add_toggle_button(group, &priv.line_wrapping,
		"line_wrapping", editor_prefs.line_wrapping, "check_line_wrapping1");
	stash_group_add_spin_button_integer(group, &priv.line_break_column,
		"line_break_column", editor_prefs.line_break_column, "spin_line_break1");
	stash_group_add_toggle_button(group, &priv.auto_continue_multiline,
		"auto_continue_multiline", editor_prefs.auto_continue_multiline,
		"check_auto_multiline1");
	add_stash_group(group, TRUE);
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
