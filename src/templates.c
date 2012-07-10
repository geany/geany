/*
 *      templates.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/*
 * Templates to insert into the current document, or file templates to create a new
 * document from.
 */

#include <time.h>
#include <string.h>

#include "geany.h"

#include "templates.h"
#include "support.h"
#include "utils.h"
#include "document.h"
#include "encodings.h"
#include "editor.h"
#include "filetypes.h"
#include "ui_utils.h"
#include "toolbar.h"
#include "geanymenubuttonaction.h"
#include "project.h"


GeanyTemplatePrefs template_prefs;

static GtkWidget *new_with_template_menu = NULL;	/* submenu used for both file menu and toolbar */

/* TODO: implement custom insertion templates instead? */
static gchar *templates[GEANY_MAX_TEMPLATES];


static void replace_static_values(GString *text);
static gchar *get_template_fileheader(GeanyFiletype *ft);

/* called by templates_replace_common */
static void templates_replace_default_dates(GString *text);
static void templates_replace_command(GString *text, const gchar *file_name,
	const gchar *file_type, const gchar *func_name);


static gchar *read_file(const gchar *locale_fname)
{
	gchar *contents;
	gsize length;
	GString *str;

	if (! g_file_get_contents(locale_fname, &contents, &length, NULL))
		return NULL;

	if (! encodings_convert_to_utf8_auto(&contents, &length, NULL, NULL, NULL, NULL))
	{
		gchar *utf8_fname = utils_get_utf8_from_locale(locale_fname);

		ui_set_statusbar(TRUE, _("Failed to convert template file \"%s\" to UTF-8"), utf8_fname);
		g_free(utf8_fname);
		g_free(contents);
		return NULL;
	}

	str = g_string_new(contents);
	g_free(contents);

	/* convert to LF endings for consistency in mixing templates */
	utils_ensure_same_eol_characters(str, SC_EOL_LF);
	return g_string_free(str, FALSE);
}


static void read_template(const gchar *name, gint id)
{
	gchar *fname = g_build_path(G_DIR_SEPARATOR_S, app->configdir,
		GEANY_TEMPLATES_SUBDIR, name, NULL);

	/* try system if user template doesn't exist */
	if (!g_file_test(fname, G_FILE_TEST_EXISTS))
		SETPTR(fname, g_build_path(G_DIR_SEPARATOR_S, app->datadir,
			GEANY_TEMPLATES_SUBDIR, name, NULL));

	templates[id] = read_file(fname);
	g_free(fname);
}


/* called when inserting templates into an existing document */
static void convert_eol_characters(GString *template, GeanyDocument *doc)
{
	gint doc_eol_mode;

	if (doc == NULL)
		doc = document_get_current();

	g_return_if_fail(doc != NULL);

	doc_eol_mode = editor_get_eol_char_mode(doc->editor);
	utils_ensure_same_eol_characters(template, doc_eol_mode);
}


static void init_general_templates(void)
{
	/* read the contents */
	read_template("fileheader", GEANY_TEMPLATE_FILEHEADER);
	read_template("gpl", GEANY_TEMPLATE_GPL);
	read_template("bsd", GEANY_TEMPLATE_BSD);
	read_template("function", GEANY_TEMPLATE_FUNCTION);
	read_template("changelog", GEANY_TEMPLATE_CHANGELOG);
}


void templates_replace_common(GString *tmpl, const gchar *fname,
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

	templates_replace_valist(tmpl,
		"{filename}", shortname,
		"{project}", app->project ? app->project->name : "",
		"{description}", app->project ? app->project->description : "",
		NULL);
	g_free(shortname);

	templates_replace_default_dates(tmpl);
	templates_replace_command(tmpl, fname, ft->name, func_name);
	/* Bug: command results could have {ob} {cb} strings in! */
	/* replace braces last */
	templates_replace_valist(tmpl,
		"{ob}", "{",
		"{cb}", "}",
		NULL);
}


static gchar *get_template_from_file(const gchar *locale_fname, const gchar *doc_filename,
									 GeanyFiletype *ft)
{
	gchar *content;
	GString *template = NULL;

	content = read_file(locale_fname);

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
	SETPTR(fname, utils_get_locale_from_utf8(fname));

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
	{
		/* line endings will be converted */
		document_new_file(new_filename, ft, template);
	}
	else
	{
		SETPTR(fname, utils_get_utf8_from_locale(fname));
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
	new_with_template_menu = gtk_menu_new();
	add_custom_template_items();

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
	gchar *path = g_build_filename(app->configdir, GEANY_TEMPLATES_SUBDIR, NULL);

	g_return_if_fail(NZV(doc->real_path));

	if (strncmp(doc->real_path, path, strlen(path)) == 0)
	{
		/* reload templates */
		templates_free_templates();
		templates_init();
	}
	g_free(path);
}


/* warning: also called when reloading template settings */
void templates_init(void)
{
	static gboolean init_done = FALSE;

	init_general_templates();

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
static void make_comment_block(GString *comment_text, gint filetype_idx, guint indent)
{
	gchar *frame_start;			/* to add before comment_text */
	gchar *frame_end;			/* to add after comment_text */
	const gchar *line_prefix;	/* to add before every line in comment_text */
	gchar *tmp;
	gchar *prefix;
	gchar **lines;
	gsize i, len;
	gint template_eol_mode;
	const gchar *template_eol_char;
	GeanyFiletype *ft = filetypes_index(filetype_idx);
	const gchar *co;
	const gchar *cc;

	g_return_if_fail(comment_text != NULL);
	g_return_if_fail(ft != NULL);

	template_eol_mode = utils_get_line_endings(comment_text->str, comment_text->len);
	template_eol_char = utils_get_eol_char(template_eol_mode);

	filetype_get_comment_open_close(ft, FALSE, &co, &cc);
	if (NZV(co))
	{
		if (NZV(cc))
		{
			frame_start = g_strconcat(co, template_eol_char, NULL);
			frame_end = g_strconcat(cc, template_eol_char, NULL);
			line_prefix = "";
		}
		else
		{
			frame_start = NULL;
			frame_end = NULL;
			line_prefix = co;
		}
	}
	else
	{	/* use C-like multi-line comments as fallback */
		frame_start = g_strconcat("/*", template_eol_char, NULL);
		frame_end = g_strconcat("*/", template_eol_char, NULL);
		line_prefix = "";
	}

	/* do some magic to nicely format C-like multi-line comments */
	if (NZV(frame_start) && frame_start[1] == '*')
	{
		/* prefix the string with a space */
		SETPTR(frame_end, g_strconcat(" ", frame_end, NULL));
		line_prefix = " *";
	}

	/* construct the real prefix with given amount of whitespace */
	i = (indent > strlen(line_prefix)) ? (indent - strlen(line_prefix)) : strlen(line_prefix);
	tmp = g_strnfill(i, ' ');
	prefix = g_strconcat(line_prefix, tmp, NULL);
	g_free(tmp);

	/* add line_prefix to every line of comment_text */
	lines = g_strsplit(comment_text->str, template_eol_char, -1);
	len = g_strv_length(lines);
	if (len > 0)	/* prevent unsigned wraparound if comment_text is empty */
	{
		for (i = 0; i < len - 1; i++)
		{
			tmp = lines[i];
			lines[i] = g_strconcat(prefix, tmp, NULL);
			g_free(tmp);
		}
	}
	tmp = g_strjoinv(template_eol_char, lines);

	/* clear old contents */
	g_string_erase(comment_text, 0, -1);

	/* add frame_end */
	if (frame_start != NULL)
		g_string_append(comment_text, frame_start);
	/* add the new main content */
	g_string_append(comment_text, tmp);
	/* add frame_start  */
	if (frame_end != NULL)
		g_string_append(comment_text, frame_end);

	utils_free_pointers(4, prefix, tmp, frame_start, frame_end, NULL);
	g_strfreev(lines);
}


gchar *templates_get_template_licence(GeanyDocument *doc, gint licence_type)
{
	GString *template;

	g_return_val_if_fail(doc != NULL, NULL);
	g_return_val_if_fail(licence_type == GEANY_TEMPLATE_GPL || licence_type == GEANY_TEMPLATE_BSD, NULL);

	template = g_string_new(templates[licence_type]);
	replace_static_values(template);
	templates_replace_default_dates(template);
	templates_replace_command(template, DOC_FILENAME(doc), doc->file_type->name, NULL);

	make_comment_block(template, doc->file_type->id, GEANY_TEMPLATES_INDENT);
	convert_eol_characters(template, doc);

	return g_string_free(template, FALSE);
}


static gchar *get_template_fileheader(GeanyFiletype *ft)
{
	GString *template = g_string_new(templates[GEANY_TEMPLATE_FILEHEADER]);

	filetypes_load_config(ft->id, FALSE);	/* load any user extension setting */

	templates_replace_valist(template,
		"{gpl}", templates[GEANY_TEMPLATE_GPL],
		"{bsd}", templates[GEANY_TEMPLATE_BSD],
		NULL);

	/* we don't replace other wildcards here otherwise they would get done twice for files */
	make_comment_block(template, ft->id, GEANY_TEMPLATES_INDENT);
	return g_string_free(template, FALSE);
}


/* TODO change the signature to take a GeanyDocument? this would break plugin API/ABI */
gchar *templates_get_template_fileheader(gint filetype_idx, const gchar *fname)
{
	GeanyFiletype *ft = filetypes[filetype_idx];
	gchar *str = get_template_fileheader(ft);
	GString *template = g_string_new(str);

	g_free(str);
	templates_replace_common(template, fname, ft, NULL);
	convert_eol_characters(template, NULL);
	return g_string_free(template, FALSE);
}


gchar *templates_get_template_function(GeanyDocument *doc, const gchar *func_name)
{
	GString *text;

	func_name = (func_name != NULL) ? func_name : "";
	text = g_string_new(templates[GEANY_TEMPLATE_FUNCTION]);

	templates_replace_valist(text, "{functionname}", func_name, NULL);
	templates_replace_default_dates(text);
	templates_replace_command(text, DOC_FILENAME(doc), doc->file_type->name, func_name);

	make_comment_block(text, doc->file_type->id, GEANY_TEMPLATES_INDENT);
	convert_eol_characters(text, doc);

	return g_string_free(text, FALSE);
}


gchar *templates_get_template_changelog(GeanyDocument *doc)
{
	GString *result = g_string_new(templates[GEANY_TEMPLATE_CHANGELOG]);
	const gchar *file_type_name = (doc != NULL) ? doc->file_type->name : "";

	replace_static_values(result);
	templates_replace_default_dates(result);
	templates_replace_command(result, DOC_FILENAME(doc), file_type_name, NULL);
	convert_eol_characters(result, doc);

	return g_string_free(result, FALSE);
}


void templates_free_templates(void)
{
	gint i;
	GList *children, *item;

	/* disconnect the menu from the action widget, so destroying the items below doesn't
	 * trigger rebuilding of the menu on each item destroy */
	geany_menu_button_action_set_menu(
		GEANY_MENU_BUTTON_ACTION(toolbar_get_action_by_name("New")), NULL);

	for (i = 0; i < GEANY_MAX_TEMPLATES; i++)
	{
		g_free(templates[i]);
	}
	/* destroy "New with template" sub menu items (in case we want to reload the templates) */
	children = gtk_container_get_children(GTK_CONTAINER(new_with_template_menu));
	foreach_list(item, children)
	{
		gtk_widget_destroy(GTK_WIDGET(item->data));
	}
	g_list_free(children);

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

		wildcard = g_strndup(cmd, (gsize) (match - cmd + 1));
		cmd = g_strndup(wildcard + 9, strlen(wildcard) - 10);

		result = run_command(cmd, file_name, file_type, func_name);
		if (result != NULL)
		{
			result = g_strstrip(result);
			utils_string_replace_first(text, wildcard, result);
			g_free(result);
		}
		else
			utils_string_replace_first(text, wildcard, "");

		g_free(wildcard);
		g_free(cmd);
	}
}
