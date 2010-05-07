/*
 *      templates.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * Templates to insert into the current document, or filetype templates to create a new
 * document from.
 */

#include <time.h>
#include <string.h>

#include "geany.h"

#include "templates.h"
#include "support.h"
#include "utils.h"
#include "document.h"
#include "filetypes.h"
#include "ui_utils.h"
#include "toolbar.h"
#include "geanymenubuttonaction.h"
#include "project.h"


GeanyTemplatePrefs template_prefs;

static GtkWidget *new_with_template_menu = NULL;	/* submenu used for both file menu and toolbar */


/* TODO: implement custom insertion templates, put these into files in data/templates */

/* default templates, only for initial tempate file creation on first start of Geany */
static const gchar templates_gpl_notice[] = "\
This program is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public License as published by\n\
the Free Software Foundation; either version 2 of the License, or\n\
(at your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License\n\
along with this program; if not, write to the Free Software\n\
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,\n\
MA 02110-1301, USA.\n\
";

static const gchar templates_bsd_notice[] = "\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are\n\
met:\n\
\n\
* Redistributions of source code must retain the above copyright\n\
  notice, this list of conditions and the following disclaimer.\n\
* Redistributions in binary form must reproduce the above\n\
  copyright notice, this list of conditions and the following disclaimer\n\
  in the documentation and/or other materials provided with the\n\
  distribution.\n\
* Neither the name of the {company} nor the names of its\n\
  contributors may be used to endorse or promote products derived from\n\
  this software without specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n\
\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n\
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n\
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n\
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n\
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n\
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n\
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n\
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
";

static const gchar templates_function_description[] = "\
\n\
name: {functionname}\n\
@param\n\
@return\n\
";

static const gchar templates_multiline[] = "\
 \n\
 \n\
";

static const gchar templates_fileheader[] = "\
{filename}\n\
\n\
Copyright {year} {developer} <{mail}>\n\
\n\
{gpl}\
";

static const gchar templates_changelog[] = "\
{date}  {developer}  <{mail}>\n\
\n\
 * \n\n\n";

static gchar *templates[GEANY_MAX_TEMPLATES];

/* We should probably remove filetype templates support soon - users can use custom
 * file templates instead. */
static gchar *ft_templates[GEANY_MAX_BUILT_IN_FILETYPES] = {NULL};


static void replace_static_values(GString *text);
static gchar *get_template_fileheader(GeanyFiletype *ft);

/* called by templates_replace_common */
static void templates_replace_default_dates(GString *text);
static void templates_replace_command(GString *text, const gchar *file_name,
	const gchar *file_type, const gchar *func_name);


/* some simple macros to reduce code size and make the code readable */
#define TEMPLATES_GET_FILENAME(shortname) \
	g_strconcat(app->configdir, \
		G_DIR_SEPARATOR_S GEANY_TEMPLATES_SUBDIR G_DIR_SEPARATOR_S, shortname, NULL)

#define TEMPLATES_READ_FILE(fname, contents_ptr) \
	g_file_get_contents(fname, contents_ptr, NULL, NULL);


static void create_template_file_if_necessary(const gchar *filename, const gchar *content)
{
	if (! g_file_test(filename, G_FILE_TEST_EXISTS))
	{
		if (file_prefs.default_eol_character != SC_EOL_LF)
		{
			/* Replace the \n characters in the default template text by the proper
			 * platform-specific line ending characters. */
			GString *tmp = g_string_new(content);
			const gchar *eol_str = (file_prefs.default_eol_character == SC_EOL_CR) ? "\r" : "\r\n";

			utils_string_replace_all(tmp, "\n", eol_str);
			utils_write_file(filename, tmp->str);
			g_string_free(tmp, TRUE);
		}
		else
			utils_write_file(filename, content);
	}
}


/* FIXME the callers should use GStrings instead of char arrays */
static gchar *replace_all(gchar *text, const gchar *year, const gchar *date, const gchar *datetime)
{
	GString *str;

	if (text == NULL)
		return NULL;

	str = g_string_new(text);

	g_free(text);
	templates_replace_valist(str,
		"{year}", year,
		"{date}", date,
		"{datetime}", datetime,
		NULL);

	return g_string_free(str, FALSE);
}


static void init_general_templates(const gchar *year, const gchar *date, const gchar *datetime)
{
	gchar *template_filename_fileheader = TEMPLATES_GET_FILENAME("fileheader");
	gchar *template_filename_gpl = TEMPLATES_GET_FILENAME("gpl");
	gchar *template_filename_bsd = TEMPLATES_GET_FILENAME("bsd");
	gchar *template_filename_function = TEMPLATES_GET_FILENAME("function");
	gchar *template_filename_changelog = TEMPLATES_GET_FILENAME("changelog");

	/* create the template files in the configuration directory, if they don't exist */
	create_template_file_if_necessary(template_filename_fileheader, templates_fileheader);
	create_template_file_if_necessary(template_filename_gpl, templates_gpl_notice);
	create_template_file_if_necessary(template_filename_bsd, templates_bsd_notice);
	create_template_file_if_necessary(template_filename_function, templates_function_description);
	create_template_file_if_necessary(template_filename_changelog, templates_changelog);

	/* read the contents */
	TEMPLATES_READ_FILE(template_filename_fileheader, &templates[GEANY_TEMPLATE_FILEHEADER]);
	templates[GEANY_TEMPLATE_FILEHEADER] = replace_all(templates[GEANY_TEMPLATE_FILEHEADER], year, date, datetime);

	TEMPLATES_READ_FILE(template_filename_gpl, &templates[GEANY_TEMPLATE_GPL]);
	templates[GEANY_TEMPLATE_GPL] = replace_all(templates[GEANY_TEMPLATE_GPL], year, date, datetime);

	TEMPLATES_READ_FILE(template_filename_bsd, &templates[GEANY_TEMPLATE_BSD]);
	templates[GEANY_TEMPLATE_BSD] = replace_all(templates[GEANY_TEMPLATE_BSD], year, date, datetime);

	TEMPLATES_READ_FILE(template_filename_function, &templates[GEANY_TEMPLATE_FUNCTION]);
	templates[GEANY_TEMPLATE_FUNCTION] = replace_all(templates[GEANY_TEMPLATE_FUNCTION], year, date, datetime);

	TEMPLATES_READ_FILE(template_filename_changelog, &templates[GEANY_TEMPLATE_CHANGELOG]);
	templates[GEANY_TEMPLATE_CHANGELOG] = replace_all(templates[GEANY_TEMPLATE_CHANGELOG], year, date, datetime);

	/* free the whole stuff */
	g_free(template_filename_fileheader);
	g_free(template_filename_gpl);
	g_free(template_filename_bsd);
	g_free(template_filename_function);
	g_free(template_filename_changelog);
}


static void init_ft_templates(const gchar *year, const gchar *date, const gchar *datetime)
{
	filetype_id ft_id;

	for (ft_id = 0; ft_id < GEANY_MAX_BUILT_IN_FILETYPES; ft_id++)
	{
		gchar *ext = filetypes_get_conf_extension(ft_id);
		gchar *shortname = g_strconcat("filetype.", ext, NULL);
		gchar *fname = TEMPLATES_GET_FILENAME(shortname);

		TEMPLATES_READ_FILE(fname, &ft_templates[ft_id]);
		ft_templates[ft_id] = replace_all(ft_templates[ft_id], year, date, datetime);

		g_free(fname);
		g_free(shortname);
		g_free(ext);
	}
}


static void
on_new_with_filetype_template(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyFiletype *ft = user_data;
	gchar *template = templates_get_template_new_file(ft);

	document_new_file(NULL, ft, template);
	g_free(template);
}


/* TODO: remove filetype template support after 0.19 */
static gboolean create_new_filetype_items(void)
{
	GSList *node;
	gboolean ret = FALSE;
	GtkWidget *menu = NULL;

	foreach_slist(node, filetypes_by_title)
	{
		GeanyFiletype *ft = node->data;
		GtkWidget *item;

		if (ft->id >= GEANY_MAX_BUILT_IN_FILETYPES || ft_templates[ft->id] == NULL)
			continue;

		if (!menu)
		{
			item = gtk_menu_item_new_with_label(_("Old"));
			menu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
			gtk_widget_show_all(item);
			gtk_container_add(GTK_CONTAINER(new_with_template_menu), item);
		}
		item = gtk_menu_item_new_with_label(ft->title);
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate", G_CALLBACK(on_new_with_filetype_template), ft);
		ret = TRUE;
	}
	return ret;
}


void templates_replace_common(GString *template, const gchar *fname,
							  GeanyFiletype *ft, const gchar *func_name)
{
	gchar *shortname;

	if (fname == NULL)
	{
		if (!ft->extension)
			shortname = g_strdup(GEANY_STRING_UNTITLED);
		else
			shortname = g_strconcat(GEANY_STRING_UNTITLED, ".", ft->extension, NULL);
	}
	else
		shortname = g_path_get_basename(fname);

	templates_replace_valist(template,
		"{filename}", shortname,
		"{project}", app->project ? app->project->name : "",
		"{description}", app->project ? app->project->description : "",
		NULL);
	g_free(shortname);

	templates_replace_default_dates(template);
	templates_replace_command(template, fname, ft->name, func_name);
	/* Bug: command results could have {ob} {cb} strings in! */
	/* replace braces last */
	templates_replace_valist(template,
		"{ob}", "{",
		"{cb}", "}",
		NULL);
}


static gchar *get_template_from_file(const gchar *locale_fname, const gchar *doc_filename,
									 GeanyFiletype *ft)
{
	gchar *content;
	GString *template = NULL;

	g_file_get_contents(locale_fname, &content, NULL, NULL);

	if (content != NULL)
	{
		gchar *file_header;

		template = g_string_new(content);

		file_header = get_template_fileheader(ft);
		templates_replace_valist(template,
			"{fileheader}", file_header,
			NULL);
		templates_replace_common(template, doc_filename, ft, NULL);

		utils_free_pointers(2, file_header, content, NULL);
		return g_string_free(template, FALSE);
	}
	return NULL;
}


static void
on_new_with_file_template(GtkMenuItem *menuitem, G_GNUC_UNUSED gpointer user_data)
{
	gchar *fname = ui_menu_item_get_text(menuitem);
	GeanyFiletype *ft;
	gchar *template;
	const gchar *extension = strrchr(fname, '.'); /* easy way to get the file extension */
	gchar *new_filename = g_strconcat(GEANY_STRING_UNTITLED, extension, NULL);
	gchar *path;

	ft = filetypes_detect_from_extension(fname);
	setptr(fname, utils_get_locale_from_utf8(fname));

	/* fname is just the basename from the menu item, so prepend the custom files path */
	path = g_build_path(G_DIR_SEPARATOR_S, app->configdir, GEANY_TEMPLATES_SUBDIR,
		"files", fname, NULL);
	template = get_template_from_file(path, new_filename, ft);
	if (!template)
	{
		/* try the system path */
		g_free(path);
		path = g_build_path(G_DIR_SEPARATOR_S, app->datadir, GEANY_TEMPLATES_SUBDIR,
			"files", fname, NULL);
		template = get_template_from_file(path, new_filename, ft);
	}
	if (template)
		document_new_file(new_filename, ft, template);
	else
	{
		setptr(fname, utils_get_utf8_from_locale(fname));
		ui_set_statusbar(TRUE, _("Could not find file '%s'."), fname);
	}
	g_free(template);
	g_free(path);
	g_free(new_filename);
	g_free(fname);
}


static void add_file_item(const gchar *fname, GtkWidget *menu)
{
	GtkWidget *tmp_button;
	gchar *label;

	g_return_if_fail(fname);
	g_return_if_fail(menu);

	label = utils_get_utf8_from_locale(fname);

	tmp_button = gtk_menu_item_new_with_label(label);
	gtk_widget_show(tmp_button);
	gtk_container_add(GTK_CONTAINER(menu), tmp_button);
	g_signal_connect(tmp_button, "activate", G_CALLBACK(on_new_with_file_template), NULL);

	g_free(label);
}


static gboolean add_custom_template_items(void)
{
	GSList *list = utils_get_config_files(GEANY_TEMPLATES_SUBDIR G_DIR_SEPARATOR_S "files");
	GSList *node;

	foreach_slist(node, list)
	{
		gchar *fname = node->data;

		add_file_item(fname, new_with_template_menu);
		g_free(fname);
	}
	g_slist_free(list);
	return list != NULL;
}


static void create_file_template_menu(void)
{
	GtkWidget *sep = NULL;

	new_with_template_menu = gtk_menu_new();

	if (add_custom_template_items())
	{
		sep = gtk_separator_menu_item_new();
		gtk_container_add(GTK_CONTAINER(new_with_template_menu), sep);
	}
	if (create_new_filetype_items() && sep)
	{
		gtk_widget_show(sep);
	}
	/* unless the file menu is showing, menu should be in the toolbar widget */
	geany_menu_button_action_set_menu(GEANY_MENU_BUTTON_ACTION(
		toolbar_get_action_by_name("New")), new_with_template_menu);
}


static void on_file_menu_show(GtkWidget *item)
{
	geany_menu_button_action_set_menu(
		GEANY_MENU_BUTTON_ACTION(toolbar_get_action_by_name("New")), NULL);
	item = ui_lookup_widget(main_widgets.window, "menu_new_with_template1");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), new_with_template_menu);
}


static void on_file_menu_hide(GtkWidget *item)
{
	item = ui_lookup_widget(main_widgets.window, "menu_new_with_template1");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), NULL);
	geany_menu_button_action_set_menu(
		GEANY_MENU_BUTTON_ACTION(toolbar_get_action_by_name("New")), new_with_template_menu);
}


/* reload templates if any file in the templates path is saved */
static void on_document_save(G_GNUC_UNUSED GObject *object, GeanyDocument *doc)
{
	const gchar *path = utils_build_path(app->configdir, GEANY_TEMPLATES_SUBDIR, NULL);

	g_return_if_fail(NZV(doc->real_path));

	if (strncmp(doc->real_path, path, strlen(path)) == 0)
	{
		/* reload templates */
		templates_free_templates();
		templates_init();
	}
}


/* warning: also called when reloading template settings */
void templates_init(void)
{
	gchar *year = utils_get_date_time(template_prefs.year_format, NULL);
	gchar *date = utils_get_date_time(template_prefs.date_format, NULL);
	gchar *datetime = utils_get_date_time(template_prefs.datetime_format, NULL);
	static gboolean init_done = FALSE;

	init_general_templates(year, date, datetime);
	init_ft_templates(year, date, datetime);

	g_free(date);
	g_free(datetime);
	g_free(year);

	create_file_template_menu();
	/* we hold our own ref for the menu as it has no parent whilst being moved */
	g_object_ref(new_with_template_menu);

	/* only connect signals to persistent objects once */
	if (!init_done)
	{
		GtkWidget *item;
		/* reparent the template menu as needed */
		item = ui_lookup_widget(main_widgets.window, "file1");
		item = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
		g_signal_connect(item, "show", G_CALLBACK(on_file_menu_show), NULL);
		g_signal_connect(item, "hide", G_CALLBACK(on_file_menu_hide), NULL);

		g_signal_connect(geany_object, "document-save", G_CALLBACK(on_document_save), NULL);
	}
	init_done = TRUE;
}


/* indent is used to make some whitespace between comment char and real start of the line
 * e.g. indent = 8 prints " *     here comes the text of the line"
 * indent is meant to be the whole amount of characters before the real line content follows, i.e.
 * 6 characters are filled with whitespace when the comment characters include " *" */
/* TODO make this function operating on a GString */
static gchar *make_comment_block(const gchar *comment_text, gint filetype_idx, guint indent)
{
	gchar *frame_start;			/* to add before comment_text */
	gchar *frame_end;			/* to add after comment_text */
	const gchar *line_prefix;	/* to add before every line in comment_text */
	gchar *result;
	gchar *tmp;
	gchar *prefix;
	gchar **lines;
	guint i, len;
	GeanyFiletype *ft = filetypes_index(filetype_idx);

	g_return_val_if_fail(ft != NULL, NULL);

	if (NZV(ft->comment_open))
	{
		if (NZV(ft->comment_close))
		{
			frame_start = g_strconcat(ft->comment_open, "\n", NULL);
			frame_end = g_strconcat(ft->comment_close, "\n", NULL);
			line_prefix = "";
		}
		else
		{
			frame_start = NULL;
			frame_end = NULL;
			line_prefix = ft->comment_open;
		}
	}
	else
	{	/* use C-like multi-line comments as fallback */
		frame_start = g_strdup("/*\n");
		frame_end = g_strdup("*/\n");
		line_prefix = "";
	}

	/* do some magic to nicely format C-like multi-line comments */
	if (NZV(frame_start) && frame_start[1] == '*')
	{
		/* prefix the string with a space */
		setptr(frame_end, g_strconcat(" ", frame_end, NULL));
		line_prefix = " *";
	}

	/* construct the real prefix with given amount of whitespace */
	i = (indent > strlen(line_prefix)) ? (indent - strlen(line_prefix)) : strlen(line_prefix);
	tmp = g_strnfill(i, ' ');
	prefix = g_strconcat(line_prefix, tmp, NULL);
	g_free(tmp);

	/* add line_prefix to every line of comment_text */
	lines = g_strsplit(comment_text, "\n", -1);
	len = g_strv_length(lines) - 1;
	for (i = 0; i < len; i++)
	{
		tmp = lines[i];
		lines[i] = g_strconcat(prefix, tmp, NULL);
		g_free(tmp);
	}
	tmp = g_strjoinv("\n", lines);

	/* add frame_start and frame_end */
	if (frame_start != NULL)
		result = g_strconcat(frame_start, tmp, frame_end, NULL);
	else
		result = g_strconcat(tmp, frame_end, NULL);

	utils_free_pointers(4, prefix, tmp, frame_start, frame_end, NULL);
	g_strfreev(lines);
	return result;
}


gchar *templates_get_template_licence(GeanyDocument *doc, gint licence_type)
{
	GString *template;
	gchar *result = NULL;

	g_return_val_if_fail(doc != NULL, NULL);
	g_return_val_if_fail(licence_type == GEANY_TEMPLATE_GPL || licence_type == GEANY_TEMPLATE_BSD, NULL);

	template = g_string_new(templates[licence_type]);
	replace_static_values(template);
	templates_replace_default_dates(template);
	templates_replace_command(template, DOC_FILENAME(doc), doc->file_type->name, NULL);

	result = make_comment_block(template->str, FILETYPE_ID(doc->file_type), 8);

	g_string_free(template, TRUE);

	return result;
}


static gchar *get_template_fileheader(GeanyFiletype *ft)
{
	GString *template = g_string_new(templates[GEANY_TEMPLATE_FILEHEADER]);
	gchar *result;

	filetypes_load_config(ft->id, FALSE);	/* load any user extension setting */

	templates_replace_valist(template,
		"{gpl}", templates[GEANY_TEMPLATE_GPL],
		"{bsd}", templates[GEANY_TEMPLATE_BSD],
		NULL);

	/* we don't replace other wildcards here otherwise they would get done twice for files */
	result = make_comment_block(template->str, ft->id, 8);
	g_string_free(template, TRUE);
	return result;
}


/* TODO change the signature to take a GeanyDocument? this would break plugin API/ABI */
gchar *templates_get_template_fileheader(gint filetype_idx, const gchar *fname)
{
	GeanyFiletype *ft = filetypes[filetype_idx];
	gchar *str = get_template_fileheader(ft);
	GString *template = g_string_new(str);

	g_free(str);
	templates_replace_common(template, fname, ft, NULL);
	return g_string_free(template, FALSE);
}


gchar *templates_get_template_new_file(GeanyFiletype *ft)
{
	GString *ft_template;
	gchar *file_header = NULL;

	g_return_val_if_fail(ft != NULL, NULL);
	g_return_val_if_fail(ft->id < GEANY_MAX_BUILT_IN_FILETYPES, NULL);

	ft_template = g_string_new(ft_templates[ft->id]);
	if (FILETYPE_ID(ft) == GEANY_FILETYPES_NONE)
	{
		replace_static_values(ft_template);
	}
	else
	{	/* file template only used for new files */
		file_header = get_template_fileheader(ft);
		templates_replace_valist(ft_template, "{fileheader}", file_header, NULL);
	}
	templates_replace_common(ft_template, NULL, ft, NULL);

	g_free(file_header);
	return g_string_free(ft_template, FALSE);
}


gchar *templates_get_template_function(GeanyDocument *doc, const gchar *func_name)
{
	gchar *result;
	GString *text;

	func_name = (func_name != NULL) ? func_name : "";
	text = g_string_new(templates[GEANY_TEMPLATE_FUNCTION]);

	templates_replace_valist(text, "{functionname}", func_name, NULL);
	templates_replace_default_dates(text);
	templates_replace_command(text, DOC_FILENAME(doc), doc->file_type->name, func_name);

	result = make_comment_block(text->str, doc->file_type->id, 3);

	g_string_free(text, TRUE);
	return result;
}


gchar *templates_get_template_changelog(GeanyDocument *doc)
{
	GString *result = g_string_new(templates[GEANY_TEMPLATE_CHANGELOG]);
	const gchar *file_type_name = (doc != NULL) ? doc->file_type->name : "";

	replace_static_values(result);
	templates_replace_default_dates(result);
	templates_replace_command(result, DOC_FILENAME(doc), file_type_name, NULL);

	return g_string_free(result, FALSE);
}


void templates_free_templates(void)
{
	gint i;
	GList *children, *item;

	for (i = 0; i < GEANY_MAX_TEMPLATES; i++)
	{
		g_free(templates[i]);
	}
	for (i = 0; i < GEANY_MAX_BUILT_IN_FILETYPES; i++)
	{
		g_free(ft_templates[i]);
	}
	/* destroy "New with template" sub menu items (in case we want to reload the templates) */
	children = gtk_container_get_children(GTK_CONTAINER(new_with_template_menu));
	foreach_list(item, children)
	{
		gtk_widget_destroy(GTK_WIDGET(item->data));
	}
	g_list_free(children);

	geany_menu_button_action_set_menu(
		GEANY_MENU_BUTTON_ACTION(toolbar_get_action_by_name("New")), NULL);
	g_object_unref(new_with_template_menu);
	new_with_template_menu = NULL;
}


static void replace_static_values(GString *text)
{
	utils_string_replace_all(text, "{version}", template_prefs.version);
	utils_string_replace_all(text, "{initial}", template_prefs.initials);
	utils_string_replace_all(text, "{developer}", template_prefs.developer);
	utils_string_replace_all(text, "{mail}", template_prefs.mail);
	utils_string_replace_all(text, "{company}", template_prefs.company);
	utils_string_replace_all(text, "{untitled}", GEANY_STRING_UNTITLED);
	utils_string_replace_all(text, "{geanyversion}", "Geany " VERSION);
}


/* Replaces all static template wildcards (version, mail, company, name, ...)
 * plus those wildcard, value pairs which are passed, e.g.
 *
 * templates_replace_valist(text, "{some_wildcard}", "some value",
 *      "{another_wildcard}", "another value", NULL);
 *
 * The argument list must be terminated with NULL. */
void templates_replace_valist(GString *text, const gchar *first_wildcard, ...)
{
	va_list args;
	const gchar *key, *value;

	g_return_if_fail(text != NULL);

	va_start(args, first_wildcard);

	key = first_wildcard;
	value = va_arg(args, gchar*);

	while (key != NULL)
	{
		utils_string_replace_all(text, key, value);

		key = va_arg(args, gchar*);
		if (key == NULL || text == NULL)
			break;
		value = va_arg(args, gchar*);
	}
	va_end(args);

	replace_static_values(text);
}


static void templates_replace_default_dates(GString *text)
{
	gchar *year = utils_get_date_time(template_prefs.year_format, NULL);
	gchar *date = utils_get_date_time(template_prefs.date_format, NULL);
	gchar *datetime = utils_get_date_time(template_prefs.datetime_format, NULL);

	g_return_if_fail(text != NULL);

	templates_replace_valist(text,
		"{year}", year,
		"{date}", date,
		"{datetime}", datetime,
		NULL);

	utils_free_pointers(3, year, date, datetime, NULL);
}


static gchar *run_command(const gchar *command, const gchar *file_name,
						  const gchar *file_type, const gchar *func_name)
{
	gchar *result = NULL;
	gchar **argv;

	if (g_shell_parse_argv(command, NULL, &argv, NULL))
	{
		GError *error = NULL;
		gchar **env;

		file_name = (file_name != NULL) ? file_name : "";
		file_type = (file_type != NULL) ? file_type : "";
		func_name = (func_name != NULL) ? func_name : "";

		env = utils_copy_environment(NULL,
			"GEANY_FILENAME", file_name,
			"GEANY_FILETYPE", file_type,
			"GEANY_FUNCNAME", func_name,
			NULL);
		if (! utils_spawn_sync(NULL, argv, env, G_SPAWN_SEARCH_PATH,
				NULL, NULL, &result, NULL, NULL, &error))
		{
			g_warning("templates_replace_command: %s", error->message);
			g_error_free(error);
			return NULL;
		}
		g_strfreev(argv);
		g_strfreev(env);
	}
	return result;
}


static void templates_replace_command(GString *text, const gchar *file_name,
							   const gchar *file_type, const gchar *func_name)
{
	gchar *match = NULL;
	gchar *wildcard = NULL;
	gchar *cmd;
	gchar *result;

	g_return_if_fail(text != NULL);

	while ((match = strstr(text->str, "{command:")) != NULL)
	{
		cmd = match;
		while (*match != '}' && *match != '\0')
			match++;

		wildcard = g_strndup(cmd, match - cmd + 1);
		cmd = g_strndup(wildcard + 9, strlen(wildcard) - 10);

		result = run_command(cmd, file_name, file_type, func_name);
		if (result != NULL)
		{
			utils_string_replace_first(text, wildcard, result);
			g_free(result);
		}
		else
			utils_string_replace_first(text, wildcard, "");

		g_free(wildcard);
		g_free(cmd);
	}
}
