/*
 *      vcdiff.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2008 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2007-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2007-2008 Yura Siamashka <yurand2(at)gmail(dot)com>
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

/* Version Diff plugin */
/* This small plugin uses svn/git/etc. to generate a diff against the current
 * version inside version control.
 * Which VC program to use is detected by looking for a version control subdirectory,
 * e.g. ".svn". */

#include "geany.h"
#include <string.h>

#include "support.h"
#include "plugindata.h"
#include "document.h"
#include "editor.h"
#include "filetypes.h"
#include "utils.h"
#include "project.h"
#include "ui_utils.h"
#include "pluginmacros.h"

#define project	geany->app->project


GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)

PLUGIN_SET_INFO(_("Version Diff"), _("Creates a patch of a file against version control."), VERSION,
	_("The Geany developer team"))


static GtkWidget *main_menu_item = NULL;


enum
{
	VC_COMMAND_DIFF_FILE,
	VC_COMMAND_DIFF_DIR,
	VC_COMMAND_DIFF_PROJECT
};

/* The addresses of these strings act as enums, their contents are not used. */
static const gchar DIRNAME[] = "*DIRNAME*";
static const gchar FILENAME[] = "*FILENAME*";
static const gchar BASE_FILENAME[] = "*BASE_FILENAME*";


static const gchar* SVN_CMD_DIFF_FILE[] = {"svn", "diff", "--non-interactive", FILENAME, NULL};
static const gchar* SVN_CMD_DIFF_DIR[]  = {"svn", "diff", "--non-interactive", DIRNAME, NULL};
static const gchar* SVN_CMD_DIFF_PROJECT[] = {"svn", "diff", "--non-interactive", DIRNAME, NULL};

static void* SVN_COMMANDS[] = { SVN_CMD_DIFF_FILE, SVN_CMD_DIFF_DIR, SVN_CMD_DIFF_PROJECT };


static const gchar* CVS_CMD_DIFF_FILE[] = {"cvs", "diff", "-u", BASE_FILENAME, NULL};
static const gchar* CVS_CMD_DIFF_DIR[]  = {"cvs", "diff", "-u",NULL};
static const gchar* CVS_CMD_DIFF_PROJECT[] = {"cvs", "diff", "-u", NULL};

static void* CVS_COMMANDS[] = { CVS_CMD_DIFF_FILE, CVS_CMD_DIFF_DIR, CVS_CMD_DIFF_PROJECT };


static const gchar* GIT_CMD_DIFF_FILE[] = {"git", "diff", "HEAD", "--", BASE_FILENAME,  NULL};
static const gchar* GIT_CMD_DIFF_DIR[]  = {"git", "diff", "HEAD", NULL};
static const gchar* GIT_CMD_DIFF_PROJECT[] = {"git", "diff", "HEAD", NULL};

static const gchar* GIT_ENV_DIFF_FILE[] = {"PAGER=cat", NULL};
static const gchar* GIT_ENV_DIFF_DIR[]  = {"PAGER=cat", NULL};
static const gchar* GIT_ENV_DIFF_PROJECT[] = {"PAGER=cat", NULL};

static void* GIT_COMMANDS[] = { GIT_CMD_DIFF_FILE, GIT_CMD_DIFF_DIR, GIT_CMD_DIFF_PROJECT };
static void* GIT_ENV[]      = { GIT_ENV_DIFF_FILE, GIT_ENV_DIFF_DIR, GIT_ENV_DIFF_PROJECT };


typedef struct VC_RECORD
{
	void** commands;
	void** envs;
	const gchar *program;
	const gchar *subdir;	/* version control subdirectory, e.g. ".svn" */
	gboolean check_parents;	/* check parent dirs to find subdir */
} VC_RECORD;


static void *NO_ENV[] = {NULL, NULL, NULL};

/* Adding another VC system should be as easy as adding another entry in this array. */
static VC_RECORD VC[] = {
	{SVN_COMMANDS, NO_ENV, "svn", ".svn", FALSE},
	{CVS_COMMANDS, NO_ENV, "cvs", "CVS", FALSE},
	{GIT_COMMANDS, GIT_ENV, "git", ".git", TRUE},
};


static gboolean find_subdir(const gchar* filename, const gchar *subdir)
{
	gboolean ret = FALSE;
	gchar *base;
	gchar *gitdir;
	gchar *base_prev = g_strdup(":");

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		base = g_strdup(filename);
	else
		base = g_path_get_dirname(filename);

	while (strcmp(base, base_prev) != 0)
	{
		gitdir = g_build_path("/", base, subdir, NULL);
		ret = g_file_test(gitdir, G_FILE_TEST_IS_DIR);
		g_free(gitdir);
		if (ret)
			break;
		g_free(base_prev);
		base_prev = base;
		base = g_path_get_dirname(base);
	}

	g_free(base);
	g_free(base_prev);
	return ret;
}


static gboolean check_filename(const gchar* filename, VC_RECORD *vc)
{
	gboolean ret;
	gchar *base;
	gchar *dir;
	gchar *path;

	if (! filename)
		return FALSE;

	path = g_find_program_in_path(vc->program);
	if (!path)
		return FALSE;
	g_free(path);

	if (vc->check_parents)
		return find_subdir(filename, vc->subdir);

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		base = g_strdup(filename);
	else
		base = g_path_get_dirname(filename);
	dir = g_build_path("/", base, vc->subdir, NULL);

	ret = g_file_test(dir, G_FILE_TEST_IS_DIR);

	g_free(base);
	g_free(dir);

	return ret;
}


static void* find_cmd_env(gint cmd_type, gboolean cmd, const gchar* filename)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(VC); i++)
	{
		if (check_filename(filename, &VC[i]))
		{
			if (cmd)
				return VC[i].commands[cmd_type];
			else
				return VC[i].envs[cmd_type];
		}
	}
	return NULL;
}


static void* get_cmd_env(gint cmd_type, gboolean cmd, const gchar* filename, int *size)
{
	int i;
	gint len = 0;
	gchar** argv;
	gchar** ret;
	gchar* dir;
	gchar* base_filename;

	argv = find_cmd_env(cmd_type, cmd, filename);
	if (!argv)
		return NULL;

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
	{
		dir = g_strdup(filename);
	}
	else
	{
		dir = g_path_get_dirname(filename);
	}
	base_filename = g_path_get_basename(filename);

	while(1)
	{
		if (argv[len] == NULL)
			break;
		len++;
	}
	ret = g_malloc(sizeof(gchar*) * (len+1));
	memset(ret, 0, sizeof(gchar*) * (len+1));
	for (i = 0; i < len; i++)
	{
		if (argv[i] == DIRNAME)
		{
			ret[i] = g_strdup(dir);
		}
		else if (argv[i] == FILENAME)
		{
			ret[i] = g_strdup(filename);
		}
		else if (argv[i] == BASE_FILENAME)
		{
			ret[i] = g_strdup(base_filename);
		}
		else
			ret[i] = g_strdup(argv[i]);
	}

	*size = len;

	g_free(dir);
	g_free(base_filename);
	return ret;
}


/* utf8_name_prefix can have a path. */
static void show_output(const gchar *std_output, const gchar *utf8_name_prefix,
		const gchar *force_encoding)
{
	gchar	*text, *detect_enc = NULL;
	gint 	page;
	GeanyDocument *doc;
	GtkNotebook *book;
	gchar	*filename;

	filename = g_path_get_basename(utf8_name_prefix);
	setptr(filename, g_strconcat(filename, ".vc.diff", NULL));

	/* need to convert input text from the encoding of the original file into
	 * UTF-8 because internally Geany always needs UTF-8 */
	if (force_encoding)
	{
		text = p_encodings->convert_to_utf8_from_charset(
			std_output, (gsize)-1, force_encoding, TRUE);
	}
	else
	{
		text = p_encodings->convert_to_utf8(std_output, (gsize)-1, &detect_enc);
	}
	if (text)
	{
		GeanyIndentType indent_type =
			p_document->get_current()->editor->indent_type;
			
		doc = p_document->find_by_filename(filename);
		if (doc == NULL)
		{
			GeanyFiletype *ft = p_filetypes->lookup_by_name("Diff");
			doc = p_document->new_file(filename, ft, text);
		}
		else
		{
			p_sci->set_text(doc->editor->sci, text);
			book = GTK_NOTEBOOK(geany->main_widgets->notebook);
			page = gtk_notebook_page_num(book, GTK_WIDGET(doc->editor->sci));
			gtk_notebook_set_current_page(book, page);
			p_document->set_text_changed(doc, FALSE);
		}
		p_editor->set_indent_type(doc->editor, indent_type);

		p_document->set_encoding(doc,
			force_encoding ? force_encoding : detect_enc);
	}
	else
	{
		p_ui->set_statusbar(FALSE, _("Input conversion of the diff output failed."));
	}
	g_free(text);
	g_free(detect_enc);
	g_free(filename);
}


static gchar *make_diff(const gchar *filename, gint cmd)
{
	gchar	*std_output = NULL;
	gchar	*std_error = NULL;
	gchar	*text = NULL;
	gchar   *dir;
	gint	 argc = 0;
	gchar  **env  = get_cmd_env(cmd, FALSE, filename, &argc);
	gchar  **argv = get_cmd_env(cmd, TRUE, filename, &argc);
	gint	 exit_code = 0;
	GError	*error = NULL;

	if (! argv)
	{
		if (env)
			g_strfreev(env);
		return NULL;
	}

	if (g_file_test(filename, G_FILE_TEST_IS_DIR))
	{
		dir = g_strdup(filename);
	}
	else
	{
		dir = g_path_get_dirname(filename);
	}

	if (p_utils->spawn_sync(dir, argv, env, G_SPAWN_SEARCH_PATH, NULL, NULL,
			&std_output, &std_error, &exit_code, &error))
	{
		/* CVS dump stuff to stderr when diff nested dirs */
		if (strcmp(argv[0], "cvs") != 0 && NZV(std_error))
		{
			p_dialogs->show_msgbox(1,
				_("%s exited with an error: \n%s."), argv[0], g_strstrip(std_error));
		}
		else if (NZV(std_output))
		{
			text = std_output;
		}
		else
		{
			p_ui->set_statusbar(FALSE, _("No changes were made."));
		}
		/* win32_spawn() returns sometimes TRUE but error is set anyway, has to be fixed */
		if (error != NULL)
		{
			g_error_free(error);
		}
	}
	else
	{
		gchar *msg;

		if (error != NULL)
		{
			msg = g_strdup(error->message);
			g_error_free(error);
		}
		else
		{	/* if we don't have an exact error message, print at least the failing command */
			msg = g_strdup_printf(_("unknown error while trying to spawn a process for %s"),
				argv[0]);
		}
		p_ui->set_statusbar(FALSE, _("An error occurred (%s)."), msg);
		g_free(msg);
	}

	g_free(dir);
	g_free(std_error);
	g_strfreev(env);
	g_strfreev(argv);
	return text;
}


/* Make a diff from the current directory */
static void vcdirectory_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc;
	gchar	*base_name = NULL;
	gchar	*locale_filename = NULL;
	gchar	*text;

	doc = p_document->get_current();

	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	if (doc->changed)
	{
		p_document->save_file(doc, FALSE);
	}

	locale_filename = p_utils->get_locale_from_utf8(doc->file_name);
	base_name = g_path_get_dirname(locale_filename);

	text = make_diff(base_name, VC_COMMAND_DIFF_DIR);
	if (text)
	{
		setptr(base_name, p_utils->get_utf8_from_locale(base_name));
		show_output(text, base_name, NULL);
		g_free(text);
	}

	g_free(base_name);
	g_free(locale_filename);
}


/* Callback if menu item for the current project was activated */
static void vcproject_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc;
	gchar	*locale_filename = NULL;
	gchar	*text;

	doc = p_document->get_current();

	g_return_if_fail(project != NULL && NZV(project->base_path));

	if (doc != NULL && doc->changed && doc->file_name != NULL)
	{
		p_document->save_file(doc, FALSE);
	}

	locale_filename = p_utils->get_locale_from_utf8(project->base_path);
	text = make_diff(locale_filename, VC_COMMAND_DIFF_PROJECT);
	if (text)
	{
		show_output(text, project->name, NULL);
		g_free(text);
	}
	g_free(locale_filename);
}


/* Callback if menu item for a single file was activated */
static void vcfile_activated(GtkMenuItem *menuitem, gpointer gdata)
{
	GeanyDocument *doc;
	gchar *locale_filename, *text;

	doc = p_document->get_current();

	g_return_if_fail(doc != NULL && doc->file_name != NULL);

	if (doc->changed)
	{
		p_document->save_file(doc, FALSE);
	}

	locale_filename = p_utils->get_locale_from_utf8(doc->file_name);

	text = make_diff(locale_filename, VC_COMMAND_DIFF_FILE);
	if (text)
	{
		show_output(text, doc->file_name, doc->encoding);
		g_free(text);
	}
	g_free(locale_filename);
}


static GtkWidget *menu_vcdiff_file = NULL;
static GtkWidget *menu_vcdiff_dir = NULL;
static GtkWidget *menu_vcdiff_project = NULL;

static void update_menu_items(void)
{
	GeanyDocument *doc;
	gboolean	have_file;
	gboolean    have_vc = FALSE;

	doc = p_document->get_current();
	have_file = doc && doc->file_name && g_path_is_absolute(doc->file_name);
	if (find_cmd_env(VC_COMMAND_DIFF_FILE, TRUE, doc->file_name))
		have_vc = TRUE;

	gtk_widget_set_sensitive(menu_vcdiff_file, have_vc && have_file);
	gtk_widget_set_sensitive(menu_vcdiff_dir, have_vc && have_file);
	gtk_widget_set_sensitive(menu_vcdiff_project,
		project != NULL && NZV(project->base_path));
}


/* Called by Geany to initialize the plugin */
void plugin_init(GeanyData *data)
{
	GtkWidget	*menu_vcdiff = NULL;
	GtkWidget	*menu_vcdiff_menu = NULL;
 	GtkTooltips	*tooltips = NULL;

	tooltips = gtk_tooltips_new();

	menu_vcdiff = gtk_image_menu_item_new_with_mnemonic(_("_Version Diff"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_vcdiff);

	g_signal_connect(menu_vcdiff, "activate", G_CALLBACK(update_menu_items), NULL);

	menu_vcdiff_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_vcdiff), menu_vcdiff_menu);

	/* Single file */
	menu_vcdiff_file = gtk_menu_item_new_with_mnemonic(_("From Current _File"));
	gtk_container_add(GTK_CONTAINER (menu_vcdiff_menu), menu_vcdiff_file);
	gtk_tooltips_set_tip (tooltips, menu_vcdiff_file,
		_("Make a diff from the current active file"), NULL);

	g_signal_connect(menu_vcdiff_file, "activate", G_CALLBACK(vcfile_activated), NULL);

	/* Directory */
	menu_vcdiff_dir = gtk_menu_item_new_with_mnemonic(_("From Current _Directory"));
	gtk_container_add(GTK_CONTAINER (menu_vcdiff_menu), menu_vcdiff_dir);
	gtk_tooltips_set_tip (tooltips, menu_vcdiff_dir,
		_("Make a diff from the directory of the current active file"), NULL);

	g_signal_connect(menu_vcdiff_dir, "activate", G_CALLBACK(vcdirectory_activated), NULL);

	/* Project */
	menu_vcdiff_project = gtk_menu_item_new_with_mnemonic(_("From Current _Project"));
	gtk_container_add(GTK_CONTAINER (menu_vcdiff_menu), menu_vcdiff_project);
	gtk_tooltips_set_tip (tooltips, menu_vcdiff_project,
		_("Make a diff from the current project's base path"), NULL);

	g_signal_connect(menu_vcdiff_project, "activate", G_CALLBACK(vcproject_activated), NULL);

	gtk_widget_show_all(menu_vcdiff);

	p_ui->add_document_sensitive(menu_vcdiff);
	main_menu_item = menu_vcdiff;
}


/* Called by Geany before unloading the plugin. */
void plugin_cleanup(void)
{
	/* remove the menu item added in plugin_init() */
	gtk_widget_destroy(main_menu_item);
}
