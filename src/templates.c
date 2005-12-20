/*
 *      templates.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

#include <time.h>

#include "geany.h"

#include "templates.h"
#include "support.h"
#include "utils.h"

// some simple macros to reduce code size and make the code readable
#define templates_get_filename(x) g_strconcat(app->configdir, G_DIR_SEPARATOR_S, x, NULL)
#define templates_create_file(x, y)	if (! g_file_test(x, G_FILE_TEST_EXISTS)) utils_write_file(x, y)
#define templates_read_file(x, y) g_file_get_contents(x, y, NULL, NULL);


// prototype, because this function should never be used outside of templates.c
gchar *templates_replace_all(gchar *source, gchar *year, gchar *date);

static gchar *templates[GEANY_MAX_TEMPLATES];


void templates_init(void)
{
	gchar *template_filename_fileheader = templates_get_filename("template.fileheader");
	gchar *template_filename_fileheader_pascal =templates_get_filename("template.fileheader.pascal");
	gchar *template_filename_gpl = templates_get_filename("template.gpl");
	gchar *template_filename_gpl_pascal = templates_get_filename("template.gpl.pascal");
	gchar *template_filename_function = templates_get_filename("template.function");
	gchar *template_filename_function_pascal = templates_get_filename("template.function.pascal");
	gchar *template_filename_multiline = templates_get_filename("template.multiline");
	gchar *template_filename_multiline_pascal = templates_get_filename("template.multiline.pascal");
	gchar *template_filename_changelog = templates_get_filename("template.changelog");
	gchar *template_filename_filetype_none = templates_get_filename("template.filetype.none");
	gchar *template_filename_filetype_c = templates_get_filename("template.filetype.c");
	gchar *template_filename_filetype_cpp = templates_get_filename("template.filetype.cpp");
	gchar *template_filename_filetype_java = templates_get_filename("template.filetype.java");
	gchar *template_filename_filetype_pascal = templates_get_filename("template.filetype.pascal");
	gchar *template_filename_filetype_php = templates_get_filename("template.filetype.php");
	gchar *template_filename_filetype_latex = templates_get_filename("template.filetype.latex");

	time_t tp = time(NULL);
	const struct tm *tm = localtime(&tp);
	gchar *year = g_malloc0(5);
	gchar *date = utils_get_date();
	strftime(year, 5, "%Y", tm);

	// create the template files in the configuration directory, if they don't exist
	templates_create_file(template_filename_fileheader, templates_fileheader);
	templates_create_file(template_filename_fileheader_pascal, templates_fileheader_pascal);
	templates_create_file(template_filename_gpl, templates_gpl_notice);
	templates_create_file(template_filename_gpl_pascal, templates_gpl_notice_pascal);
	templates_create_file(template_filename_function, templates_function_description);
	templates_create_file(template_filename_function_pascal, templates_function_description_pascal);
	templates_create_file(template_filename_multiline, templates_multiline);
	templates_create_file(template_filename_multiline_pascal, templates_multiline_pascal);
	templates_create_file(template_filename_changelog, templates_changelog);
	templates_create_file(template_filename_filetype_none, templates_filetype_none);
	templates_create_file(template_filename_filetype_c, templates_filetype_c);
	templates_create_file(template_filename_filetype_cpp, templates_filetype_cpp);
	templates_create_file(template_filename_filetype_java, templates_filetype_java);
	templates_create_file(template_filename_filetype_pascal, templates_filetype_pascal);
	templates_create_file(template_filename_filetype_php, templates_filetype_php);
	templates_create_file(template_filename_filetype_latex, templates_filetype_none);

	// read the contents
	templates_read_file(template_filename_fileheader, &templates[GEANY_TEMPLATE_FILEHEADER]);
	templates[GEANY_TEMPLATE_FILEHEADER] = templates_replace_all(templates[GEANY_TEMPLATE_FILEHEADER], year, date);

	templates_read_file(template_filename_fileheader_pascal, &templates[GEANY_TEMPLATE_FILEHEADER_PASCAL]);
	templates[GEANY_TEMPLATE_FILEHEADER_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_FILEHEADER_PASCAL], year, date);

	templates_read_file(template_filename_gpl, &templates[GEANY_TEMPLATE_GPL]);
	//templates[GEANY_TEMPLATE_GPL] = templates_replace_all(templates[GEANY_TEMPLATE_GPL], year, date);

	templates_read_file(template_filename_gpl_pascal, &templates[GEANY_TEMPLATE_GPL_PASCAL]);
	//templates[GEANY_TEMPLATE_GPL_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_GPL_PASCAL], year, date);

	templates_read_file(template_filename_function, &templates[GEANY_TEMPLATE_FUNCTION]);
	templates[GEANY_TEMPLATE_FUNCTION] = templates_replace_all(templates[GEANY_TEMPLATE_FUNCTION], year, date);

	templates_read_file(template_filename_function_pascal, &templates[GEANY_TEMPLATE_FUNCTION_PASCAL]);
	templates[GEANY_TEMPLATE_FUNCTION_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_FUNCTION_PASCAL], year, date);

	templates_read_file(template_filename_multiline, &templates[GEANY_TEMPLATE_MULTILINE]);
	//templates[GEANY_TEMPLATE_MULTILINE] = templates_replace_all(templates[GEANY_TEMPLATE_MULTILINE], year, date);

	templates_read_file(template_filename_multiline_pascal, &templates[GEANY_TEMPLATE_MULTILINE_PASCAL]);
	//templates[GEANY_TEMPLATE_MULTILINE_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_MULTILINE_PASCAL], year, date);

	templates_read_file(template_filename_changelog, &templates[GEANY_TEMPLATE_CHANGELOG]);
	templates[GEANY_TEMPLATE_CHANGELOG] = templates_replace_all(templates[GEANY_TEMPLATE_CHANGELOG], year, date);


	templates_read_file(template_filename_filetype_none, &templates[GEANY_TEMPLATE_FILETYPE_NONE]);
	templates[GEANY_TEMPLATE_FILETYPE_NONE] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_NONE], year, date);

	templates_read_file(template_filename_filetype_c, &templates[GEANY_TEMPLATE_FILETYPE_C]);
	templates[GEANY_TEMPLATE_FILETYPE_C] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_C], year, date);

	templates_read_file(template_filename_filetype_cpp, &templates[GEANY_TEMPLATE_FILETYPE_CPP]);
	templates[GEANY_TEMPLATE_FILETYPE_CPP] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_CPP], year, date);

	templates_read_file(template_filename_filetype_java, &templates[GEANY_TEMPLATE_FILETYPE_JAVA]);
	templates[GEANY_TEMPLATE_FILETYPE_JAVA] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_JAVA], year, date);

	templates_read_file(template_filename_filetype_pascal, &templates[GEANY_TEMPLATE_FILETYPE_PASCAL]);
	templates[GEANY_TEMPLATE_FILETYPE_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_PASCAL], year, date);

	templates_read_file(template_filename_filetype_php, &templates[GEANY_TEMPLATE_FILETYPE_PHP]);
	templates[GEANY_TEMPLATE_FILETYPE_PHP] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_PHP], year, date);

	templates_read_file(template_filename_filetype_latex, &templates[GEANY_TEMPLATE_FILETYPE_LATEX]);
	templates[GEANY_TEMPLATE_FILETYPE_LATEX] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_LATEX], year, date);



	// free the whole stuff
	g_free(date);
	g_free(year);
	g_free(template_filename_fileheader);
	g_free(template_filename_fileheader_pascal);
	g_free(template_filename_gpl);
	g_free(template_filename_gpl_pascal);
	g_free(template_filename_function);
	g_free(template_filename_function_pascal);
	g_free(template_filename_multiline);
	g_free(template_filename_multiline_pascal);
	g_free(template_filename_changelog);
	g_free(template_filename_filetype_none);
	g_free(template_filename_filetype_c);
	g_free(template_filename_filetype_cpp);
	g_free(template_filename_filetype_java);
	g_free(template_filename_filetype_php);
	g_free(template_filename_filetype_pascal);
	g_free(template_filename_filetype_latex);
}


/* Returns a template chosen by template with GEANY_STRING_UNTITLED.extension
 * as filename if idx is -1, or the real filename if idx is greater than -1.
 * The flag gpl decides wether a GPL notice is appended or not
*/
gchar *templates_get_template_fileheader(gint template, const gchar *extension, gint idx)
{
	gchar *result = g_strdup(templates[template]);
	gchar *filename;
	gchar *date = utils_get_date_time();

	if (idx == -1)
	{
		if (extension != NULL)
			filename = g_strconcat(GEANY_STRING_UNTITLED, ".", extension, NULL);
		else
			filename = g_strdup(GEANY_STRING_UNTITLED);
	}
	else
	{
		filename = g_path_get_basename(doc_list[idx].file_name);
	}
	result = utils_str_replace(result, "{filename}", filename);

	if (template == GEANY_TEMPLATE_FILEHEADER_PASCAL)
	{
		result = utils_str_replace(result, "{gpl}", templates[GEANY_TEMPLATE_GPL_PASCAL]);
	}
	else
	{
		result = utils_str_replace(result, "{gpl}", templates[GEANY_TEMPLATE_GPL]);
	}
	result = utils_str_replace(result, "{datetime}", date);

	g_free(filename);
	g_free(date);
	return result;
}


gchar *templates_get_template_generic(gint template)
{
	return g_strdup(templates[template]);
}


gchar *templates_get_template_function(gint template, gchar *func_name)
{
	gchar *result = g_strdup(templates[template]);
	gchar *date = utils_get_date();
	gchar *datetime = utils_get_date_time();

	result = utils_str_replace(result, "{date}", date);
	result = utils_str_replace(result, "{datetime}", datetime);
	result = utils_str_replace(result, "{functionname}", (func_name) ? func_name : "");

	g_free(date);
	g_free(datetime);
	return result;
}

gchar *templates_get_template_gpl(gint template)
{
	if (template == GEANY_TEMPLATE_GPL_PASCAL)
	{
		return g_strconcat("{\n", templates[template], "}\n", NULL);
	}
	else
	{
		return g_strconcat("/*\n", templates[template], "*/\n", NULL);
	}
}


gchar *templates_get_template_changelog(void)
{
	gchar *date = utils_get_date_time();
	gchar *result = g_strdup(templates[GEANY_TEMPLATE_CHANGELOG]);
	result = utils_str_replace(result, "{date}", date);

	g_free(date);
	return result;
}


void templates_free_templates(void)
{
	gint i;
	for (i = 0; i < GEANY_MAX_TEMPLATES; i++)
	{
		g_free(templates[i]);
	}
}


gchar *templates_replace_all(gchar *text, gchar *year, gchar *date)
{
	text = utils_str_replace(text, "{year}", year);
	text = utils_str_replace(text, "{date}", date);
	text = utils_str_replace(text, "{version}", app->pref_template_version);
	text = utils_str_replace(text, "{initial}", app->pref_template_initial);
	text = utils_str_replace(text, "{developer}", app->pref_template_developer);
	text = utils_str_replace(text, "{mail}", app->pref_template_mail);
	text = utils_str_replace(text, "{company}", app->pref_template_company);
	text = utils_str_replace(text, "{untitled}", GEANY_STRING_UNTITLED);

	return text;
}
