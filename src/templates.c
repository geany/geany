/*
 *      templates.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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

#include <time.h>

#include "geany.h"

#include "templates.h"
#include "support.h"
#include "utils.h"
#include "document.h"


// default templates, only for initial tempate file creation on first start of Geany
static const gchar templates_gpl_notice[] = "\
 *      This program is free software; you can redistribute it and/or modify\n\
 *      it under the terms of the GNU General Public License as published by\n\
 *      the Free Software Foundation; either version 2 of the License, or\n\
 *      (at your option) any later version.\n\
 *\n\
 *      This program is distributed in the hope that it will be useful,\n\
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
 *      GNU General Public License for more details.\n\
 *\n\
 *      You should have received a copy of the GNU General Public License\n\
 *      along with this program; if not, write to the Free Software\n\
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.\n\
";

static const gchar templates_gpl_notice_pascal[] = "\
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
      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.\n\
";

static const gchar templates_gpl_notice_route[] = "\
#      This program is free software; you can redistribute it and/or modify\n\
#      it under the terms of the GNU General Public License as published by\n\
#      the Free Software Foundation; either version 2 of the License, or\n\
#      (at your option) any later version.\n\
#\n\
#      This program is distributed in the hope that it will be useful,\n\
#      but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
#      GNU General Public License for more details.\n\
#\n\
#      You should have received a copy of the GNU General Public License\n\
#      along with this program; if not, write to the Free Software\n\
#      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.\n\
";

static const gchar templates_function_description[] = "\
/* \n\
 * name: {functionname}\n\
 * @param\n\
 * @return\n\
 */\n";

static const gchar templates_function_description_pascal[] = "\
{\n\
 name: {functionname}\n\
 @param\n\
 @return\n\
}\n";

static const gchar templates_function_description_route[] = "\
#\n\
# name: {functionname}\n\
# @param\n\
# @return\n\
";

static const gchar templates_multiline[] = "\
/* \n\
 * \n\
 */";

static const gchar templates_multiline_pascal[] = "\
{\n\
 \n\
}";

static const gchar templates_multiline_route[] = "\
#\n\
#";

static const gchar templates_fileheader[] = "\
/*\n\
 *      {filename}\n\
 *\n\
 *      Copyright {year} {developer} <{mail}>\n\
 *\n\
{gpl}\
 */\n";

static const gchar templates_fileheader_pascal[] = "\
{\n\
      {filename}\n\
\n\
      Copyright {year} {developer} <{mail}>\n\
\n\
{gpl}\
}\n";

static const gchar templates_fileheader_route[] = "\
#\n\
#      {filename}\n\
#\n\
#      Copyright {year} {developer} <{mail}>\n\
#\n\
{gpl}\
#\n";

static const gchar templates_changelog[] = "\
{date}  {developer}  <{mail}>\n\
\n\
 * \n\n\n";

static const gchar templates_filetype_none[] = "";

static const gchar templates_filetype_c[] = "\n\
#include <stdio.h>\n\
\n\
int main(int argc, char** argv)\n\
{\n\
	\n\
	return 0;\n\
}\n\
";

static const gchar templates_filetype_cpp[] = "\n\
#include <iostream>\n\
\n\
int main(int argc, char** argv)\n\
{\n\
	\n\
	return 0;\n\
}\n\
";

static const gchar templates_filetype_d[] = "\n\
import std.stdio;\n\
\n\
int main(char[][] args)\n\
{\n\
	\n\
	return 0;\n\
}\n\
";

static const gchar templates_filetype_php[] = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\
  \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n\
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n\
\n\
<head>\n\
	<title>{untitled}</title>\n\
	<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\" />\n\
	<meta name=\"generator\" content=\"{geanyversion}\" />\n\
</head>\n\
\n\
<body>\n\
	\n\
</body>\n\
</html>\n\
";

static const gchar templates_filetype_html[] = "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\
  \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n\
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n\
\n\
<head>\n\
	<title>{untitled}</title>\n\
	<meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\" />\n\
	<meta name=\"generator\" content=\"{geanyversion}\" />\n\
</head>\n\
\n\
<body>\n\
	\n\
</body>\n\
</html>\n\
";

static const gchar templates_filetype_pascal[] = "program {untitled};\n\
\n\
uses crt;\n\
var i : byte;\n\
\n\
BEGIN\n\
	\n\
	\n\
END.\n\
";

static const gchar templates_filetype_java[] = "\n\
\n\
public class {untitled} {\n\
\n\
	public static void main (String args[]) {\n\
		\n\
		\n\
	}\n\
}\n\
";

static const gchar templates_filetype_ruby[] = "\n\
\n\
class StdClass\n\
	def initialize\n\
		\n\
	end\n\
end\n\
\n\
x = StdClass.new\n\
";


static gchar *templates[GEANY_MAX_TEMPLATES];


// some simple macros to reduce code size and make the code readable
#define templates_get_filename(x) g_strconcat(app->configdir, G_DIR_SEPARATOR_S, x, NULL)
#define templates_create_file(x, y)	if (! g_file_test(x, G_FILE_TEST_EXISTS)) utils_write_file(x, y)
#define templates_read_file(x, y) g_file_get_contents(x, y, NULL, NULL);


// prototype, because this function should never be used outside of templates.c
static gchar *templates_replace_all(gchar *source, gchar *year, gchar *date);


void templates_init(void)
{
	gchar *template_filename_fileheader = templates_get_filename("template.fileheader");
	gchar *template_filename_fileheader_pascal =templates_get_filename("template.fileheader.pascal");
	gchar *template_filename_fileheader_route =templates_get_filename("template.fileheader.route");
	gchar *template_filename_gpl = templates_get_filename("template.gpl");
	gchar *template_filename_gpl_pascal = templates_get_filename("template.gpl.pascal");
	gchar *template_filename_gpl_route = templates_get_filename("template.gpl.route");
	gchar *template_filename_function = templates_get_filename("template.function");
	gchar *template_filename_function_pascal = templates_get_filename("template.function.pascal");
	gchar *template_filename_function_route = templates_get_filename("template.function.route");
	gchar *template_filename_multiline = templates_get_filename("template.multiline");
	gchar *template_filename_multiline_pascal = templates_get_filename("template.multiline.pascal");
	gchar *template_filename_multiline_route = templates_get_filename("template.multiline.route");
	gchar *template_filename_changelog = templates_get_filename("template.changelog");
	gchar *template_filename_filetype_none = templates_get_filename("template.filetype.none");
	gchar *template_filename_filetype_c = templates_get_filename("template.filetype.c");
	gchar *template_filename_filetype_cpp = templates_get_filename("template.filetype.cpp");
	gchar *template_filename_filetype_d = templates_get_filename("template.filetype.d");
	gchar *template_filename_filetype_java = templates_get_filename("template.filetype.java");
	gchar *template_filename_filetype_pascal = templates_get_filename("template.filetype.pascal");
	gchar *template_filename_filetype_php = templates_get_filename("template.filetype.php");
	gchar *template_filename_filetype_html = templates_get_filename("template.filetype.html");
	gchar *template_filename_filetype_ruby = templates_get_filename("template.filetype.ruby");

	time_t tp = time(NULL);
	const struct tm *tm = localtime(&tp);
	gchar *year = g_malloc0(5);
	gchar *date = utils_get_date();
	strftime(year, 5, "%Y", tm);

	// create the template files in the configuration directory, if they don't exist
	templates_create_file(template_filename_fileheader, templates_fileheader);
	templates_create_file(template_filename_fileheader_pascal, templates_fileheader_pascal);
	templates_create_file(template_filename_fileheader_route, templates_fileheader_route);
	templates_create_file(template_filename_gpl, templates_gpl_notice);
	templates_create_file(template_filename_gpl_pascal, templates_gpl_notice_pascal);
	templates_create_file(template_filename_gpl_route, templates_gpl_notice_route);
	templates_create_file(template_filename_function, templates_function_description);
	templates_create_file(template_filename_function_pascal, templates_function_description_pascal);
	templates_create_file(template_filename_function_route, templates_function_description_route);
	templates_create_file(template_filename_multiline, templates_multiline);
	templates_create_file(template_filename_multiline_pascal, templates_multiline_pascal);
	templates_create_file(template_filename_multiline_route, templates_multiline_route);
	templates_create_file(template_filename_changelog, templates_changelog);
	templates_create_file(template_filename_filetype_none, templates_filetype_none);
	templates_create_file(template_filename_filetype_c, templates_filetype_c);
	templates_create_file(template_filename_filetype_cpp, templates_filetype_cpp);
	templates_create_file(template_filename_filetype_d, templates_filetype_d);
	templates_create_file(template_filename_filetype_java, templates_filetype_java);
	templates_create_file(template_filename_filetype_pascal, templates_filetype_pascal);
	templates_create_file(template_filename_filetype_php, templates_filetype_php);
	templates_create_file(template_filename_filetype_html, templates_filetype_html);
	templates_create_file(template_filename_filetype_ruby, templates_filetype_ruby);

	// read the contents
	templates_read_file(template_filename_fileheader, &templates[GEANY_TEMPLATE_FILEHEADER]);
	templates[GEANY_TEMPLATE_FILEHEADER] = templates_replace_all(templates[GEANY_TEMPLATE_FILEHEADER], year, date);

	templates_read_file(template_filename_fileheader_pascal, &templates[GEANY_TEMPLATE_FILEHEADER_PASCAL]);
	templates[GEANY_TEMPLATE_FILEHEADER_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_FILEHEADER_PASCAL], year, date);

	templates_read_file(template_filename_fileheader_route, &templates[GEANY_TEMPLATE_FILEHEADER_ROUTE]);
	templates[GEANY_TEMPLATE_FILEHEADER_ROUTE] = templates_replace_all(templates[GEANY_TEMPLATE_FILEHEADER_ROUTE], year, date);

	templates_read_file(template_filename_gpl, &templates[GEANY_TEMPLATE_GPL]);
	//templates[GEANY_TEMPLATE_GPL] = templates_replace_all(templates[GEANY_TEMPLATE_GPL], year, date);

	templates_read_file(template_filename_gpl_pascal, &templates[GEANY_TEMPLATE_GPL_PASCAL]);
	//templates[GEANY_TEMPLATE_GPL_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_GPL_PASCAL], year, date);

	templates_read_file(template_filename_gpl_route, &templates[GEANY_TEMPLATE_GPL_ROUTE]);
	//templates[GEANY_TEMPLATE_GPL_ROUTE] = templates_replace_all(templates[GEANY_TEMPLATE_GPL_ROUTE], year, date);

	templates_read_file(template_filename_function, &templates[GEANY_TEMPLATE_FUNCTION]);
	templates[GEANY_TEMPLATE_FUNCTION] = templates_replace_all(templates[GEANY_TEMPLATE_FUNCTION], year, date);

	templates_read_file(template_filename_function_pascal, &templates[GEANY_TEMPLATE_FUNCTION_PASCAL]);
	templates[GEANY_TEMPLATE_FUNCTION_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_FUNCTION_PASCAL], year, date);

	templates_read_file(template_filename_function_route, &templates[GEANY_TEMPLATE_FUNCTION_ROUTE]);
	templates[GEANY_TEMPLATE_FUNCTION_ROUTE] = templates_replace_all(templates[GEANY_TEMPLATE_FUNCTION_ROUTE], year, date);

	templates_read_file(template_filename_multiline, &templates[GEANY_TEMPLATE_MULTILINE]);
	//templates[GEANY_TEMPLATE_MULTILINE] = templates_replace_all(templates[GEANY_TEMPLATE_MULTILINE], year, date);

	templates_read_file(template_filename_multiline_pascal, &templates[GEANY_TEMPLATE_MULTILINE_PASCAL]);
	//templates[GEANY_TEMPLATE_MULTILINE_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_MULTILINE_PASCAL], year, date);

	templates_read_file(template_filename_multiline_route, &templates[GEANY_TEMPLATE_MULTILINE_ROUTE]);
	//templates[GEANY_TEMPLATE_MULTILINE_ROUTE] = templates_replace_all(templates[GEANY_TEMPLATE_MULTILINE_ROUTE], year, date);

	templates_read_file(template_filename_changelog, &templates[GEANY_TEMPLATE_CHANGELOG]);
	templates[GEANY_TEMPLATE_CHANGELOG] = templates_replace_all(templates[GEANY_TEMPLATE_CHANGELOG], year, date);


	templates_read_file(template_filename_filetype_none, &templates[GEANY_TEMPLATE_FILETYPE_NONE]);
	templates[GEANY_TEMPLATE_FILETYPE_NONE] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_NONE], year, date);

	templates_read_file(template_filename_filetype_c, &templates[GEANY_TEMPLATE_FILETYPE_C]);
	templates[GEANY_TEMPLATE_FILETYPE_C] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_C], year, date);

	templates_read_file(template_filename_filetype_d, &templates[GEANY_TEMPLATE_FILETYPE_D]);
	templates[GEANY_TEMPLATE_FILETYPE_D] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_D], year, date);

	templates_read_file(template_filename_filetype_cpp, &templates[GEANY_TEMPLATE_FILETYPE_CPP]);
	templates[GEANY_TEMPLATE_FILETYPE_CPP] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_CPP], year, date);

	templates_read_file(template_filename_filetype_java, &templates[GEANY_TEMPLATE_FILETYPE_JAVA]);
	templates[GEANY_TEMPLATE_FILETYPE_JAVA] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_JAVA], year, date);

	templates_read_file(template_filename_filetype_pascal, &templates[GEANY_TEMPLATE_FILETYPE_PASCAL]);
	templates[GEANY_TEMPLATE_FILETYPE_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_PASCAL], year, date);

	templates_read_file(template_filename_filetype_php, &templates[GEANY_TEMPLATE_FILETYPE_PHP]);
	templates[GEANY_TEMPLATE_FILETYPE_PHP] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_PHP], year, date);

	templates_read_file(template_filename_filetype_html, &templates[GEANY_TEMPLATE_FILETYPE_HTML]);
	templates[GEANY_TEMPLATE_FILETYPE_HTML] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_HTML], year, date);

	templates_read_file(template_filename_filetype_ruby, &templates[GEANY_TEMPLATE_FILETYPE_RUBY]);
	templates[GEANY_TEMPLATE_FILETYPE_RUBY] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_RUBY], year, date);


	// free the whole stuff
	g_free(date);
	g_free(year);
	g_free(template_filename_fileheader);
	g_free(template_filename_fileheader_pascal);
	g_free(template_filename_fileheader_route);
	g_free(template_filename_gpl);
	g_free(template_filename_gpl_pascal);
	g_free(template_filename_gpl_route);
	g_free(template_filename_function);
	g_free(template_filename_function_pascal);
	g_free(template_filename_function_route);
	g_free(template_filename_multiline);
	g_free(template_filename_multiline_pascal);
	g_free(template_filename_multiline_route);
	g_free(template_filename_changelog);
	g_free(template_filename_filetype_none);
	g_free(template_filename_filetype_c);
	g_free(template_filename_filetype_cpp);
	g_free(template_filename_filetype_d);
	g_free(template_filename_filetype_java);
	g_free(template_filename_filetype_php);
	g_free(template_filename_filetype_html);
	g_free(template_filename_filetype_pascal);
	g_free(template_filename_filetype_ruby);
}


/* double_comment is a hack for PHP/HTML for whether to first add C style commenting.
 * In future we could probably remove the need for this by making most templates
 * automatically commented (so template files are not commented) */
static gchar *make_comment_block(const gchar *comment_text, gint filetype_idx,
		gboolean double_comment)
{
	switch (filetype_idx)
	{
		case GEANY_FILETYPES_ALL:
		return g_strdup(comment_text);	// no need to add to the text

		case GEANY_FILETYPES_PHP:
		case GEANY_FILETYPES_HTML:
		{	// double comment
			gchar *tmp = (double_comment) ?
				make_comment_block(comment_text, GEANY_FILETYPES_C, FALSE) :
				g_strdup(comment_text);
			gchar *block = (filetype_idx == GEANY_FILETYPES_PHP) ?
				g_strconcat("<?php\n", tmp, "?>\n", NULL) :
				g_strconcat("<!--\n", tmp, "-->\n", NULL);
			g_free(tmp);
			return block;
		}

		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		return g_strconcat("#\n", comment_text, "#\n", NULL);

		case GEANY_FILETYPES_PASCAL:
		return g_strconcat("{\n", comment_text, "}\n", NULL);

		default:
		return g_strconcat("/*\n", comment_text, " */\n", NULL);
	}
}


gchar *templates_get_template_gpl(gint filetype_idx)
{
	const gchar *text;

	switch (filetype_idx)
	{
		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		text = templates[GEANY_TEMPLATE_GPL_ROUTE];
		break;

		case GEANY_FILETYPES_PASCAL:
		case GEANY_FILETYPES_ALL:
		text = templates[GEANY_TEMPLATE_GPL_PASCAL];
		break;

		case GEANY_FILETYPES_HTML:
		case GEANY_FILETYPES_PHP:
		default:
		text = templates[GEANY_TEMPLATE_GPL];
		break;
	}
	return make_comment_block(text, filetype_idx, TRUE);
}


/* Returns a template chosen by template with GEANY_STRING_UNTITLED.extension
 * as filename if idx is -1, or the real filename if idx is greater than -1.
 * The flag gpl decides whether a GPL notice is appended or not */
static gchar *
prepare_file_header(gint template, const gchar *extension, const gchar *filename)
{
	gchar *result = g_strdup(templates[template]);
	gchar *shortname;
	gchar *date = utils_get_date_time();

	if (filename == NULL)
	{
		if (extension != NULL)
			shortname = g_strconcat(GEANY_STRING_UNTITLED, ".", extension, NULL);
		else
			shortname = g_strdup(GEANY_STRING_UNTITLED);
	}
	else
	{
		shortname = g_path_get_basename(filename);
	}
	result = utils_str_replace(result, "{filename}", shortname);

	if (template == GEANY_TEMPLATE_FILEHEADER_PASCAL)
	{
		result = utils_str_replace(result, "{gpl}", templates[GEANY_TEMPLATE_GPL_PASCAL]);
	}
	else if (template == GEANY_TEMPLATE_FILEHEADER_ROUTE)
	{
		result = utils_str_replace(result, "{gpl}", templates[GEANY_TEMPLATE_GPL_ROUTE]);
	}
	else
	{
		result = utils_str_replace(result, "{gpl}", templates[GEANY_TEMPLATE_GPL]);
	}
	result = utils_str_replace(result, "{datetime}", date);

	g_free(shortname);
	g_free(date);
	return result;
}


// ft, fname can be NULL
static gchar *get_file_header(filetype *ft, const gchar *fname)
{
	gchar *text = NULL;

	switch (FILETYPE_ID(ft))
	{
		case GEANY_FILETYPES_ALL:	// ft may be NULL
		{
			text = prepare_file_header(GEANY_TEMPLATE_FILEHEADER, NULL, fname);
			break;
		}
		case GEANY_FILETYPES_PHP:
		case GEANY_FILETYPES_HTML:
		{
			gchar *tmp = prepare_file_header(
					GEANY_TEMPLATE_FILEHEADER, ft->extension, fname);
			text = make_comment_block(tmp, ft->id, FALSE);
			g_free(tmp);
			break;
		}
		case GEANY_FILETYPES_PASCAL:
		{	// Pascal: comments are in { } brackets
			text = prepare_file_header(
					GEANY_TEMPLATE_FILEHEADER_PASCAL, ft->extension, fname);
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		{
			text = prepare_file_header(
					GEANY_TEMPLATE_FILEHEADER_ROUTE, ft->extension, fname);
			break;
		}
		default:
		{	// -> C, C++, Java, ...
			text = prepare_file_header(
					GEANY_TEMPLATE_FILEHEADER, ft->extension, fname);
		}
	}
	return text;
}


gchar *templates_get_template_fileheader(gint idx)
{
	gchar *fname;
	filetype *ft;

	g_return_val_if_fail(DOC_IDX_VALID(idx), NULL);
	ft = doc_list[idx].file_type;
	fname = doc_list[idx].file_name;

	return get_file_header(ft, fname);
}


static gchar *get_file_template(filetype *ft)
{
	switch (FILETYPE_ID(ft))
	{
		case GEANY_FILETYPES_ALL:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_NONE); break;
		case GEANY_FILETYPES_C:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_C); break;
		case GEANY_FILETYPES_CPP:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_CPP); break;
		case GEANY_FILETYPES_PHP:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_PHP); break;
		case GEANY_FILETYPES_JAVA:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_JAVA); break;
		case GEANY_FILETYPES_PASCAL:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_PASCAL); break;
		case GEANY_FILETYPES_RUBY:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_RUBY); break;
		case GEANY_FILETYPES_D:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_D); break;
		case GEANY_FILETYPES_HTML:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_HTML); break;
		default: return NULL;
	}
}


gchar *templates_get_template_new_file(filetype *ft)
{
	gchar *template = NULL;
	gchar *ft_template = NULL;
	gchar *file_header = NULL;

	if (FILETYPE_ID(ft) == GEANY_FILETYPES_ALL)
		return get_file_template(ft);

	file_header = get_file_header(ft, NULL);	// file template only used for new files
	ft_template = get_file_template(ft);
	template = g_strconcat(file_header, "\n", ft_template, NULL);
	g_free(ft_template);
	g_free(file_header);
	return template;
}


gchar *templates_get_template_generic(gint template)
{
	return g_strdup(templates[template]);
}


gchar *templates_get_template_function(gint template, const gchar *func_name)
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


static gchar *templates_replace_all(gchar *text, gchar *year, gchar *date)
{
	text = utils_str_replace(text, "{year}", year);
	text = utils_str_replace(text, "{date}", date);
	text = utils_str_replace(text, "{version}", app->pref_template_version);
	text = utils_str_replace(text, "{initial}", app->pref_template_initial);
	text = utils_str_replace(text, "{developer}", app->pref_template_developer);
	text = utils_str_replace(text, "{mail}", app->pref_template_mail);
	text = utils_str_replace(text, "{company}", app->pref_template_company);
	text = utils_str_replace(text, "{untitled}", GEANY_STRING_UNTITLED);
	text = utils_str_replace(text, "{geanyversion}", "Geany " VERSION);

	return text;
}
