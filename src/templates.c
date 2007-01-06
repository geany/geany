/*
 *      templates.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Troeger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
#include <string.h>

#include "geany.h"

#include "templates.h"
#include "support.h"
#include "utils.h"
#include "document.h"


// default templates, only for initial tempate file creation on first start of Geany
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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.\n\
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

static const gchar templates_filetype_pascal[] = "\n\
program {untitled};\n\
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
public class {untitled} {\n\
\n\
	public static void main (String args[]) {\
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
#define TEMPLATES_GET_FILENAME(x) g_strconcat(app->configdir, G_DIR_SEPARATOR_S, x, NULL)
#define TEMPLATES_CREATE_FILE(x, y)	if (! g_file_test(x, G_FILE_TEST_EXISTS)) utils_write_file(x, y)
#define TEMPLATES_READ_FILE(x, y) g_file_get_contents(x, y, NULL, NULL);


// prototype, because this function should never be used outside of templates.c
static gchar *templates_replace_all(gchar *source, gchar *year, gchar *date);


void templates_init(void)
{
	gchar *template_filename_fileheader = TEMPLATES_GET_FILENAME("template.fileheader");
	gchar *template_filename_gpl = TEMPLATES_GET_FILENAME("template.gpl");
	gchar *template_filename_bsd = TEMPLATES_GET_FILENAME("template.bsd");
	gchar *template_filename_function = TEMPLATES_GET_FILENAME("template.function");
	gchar *template_filename_changelog = TEMPLATES_GET_FILENAME("template.changelog");
	gchar *template_filename_filetype_none = TEMPLATES_GET_FILENAME("template.filetype.none");
	gchar *template_filename_filetype_c = TEMPLATES_GET_FILENAME("template.filetype.c");
	gchar *template_filename_filetype_cpp = TEMPLATES_GET_FILENAME("template.filetype.cpp");
	gchar *template_filename_filetype_d = TEMPLATES_GET_FILENAME("template.filetype.d");
	gchar *template_filename_filetype_java = TEMPLATES_GET_FILENAME("template.filetype.java");
	gchar *template_filename_filetype_pascal = TEMPLATES_GET_FILENAME("template.filetype.pascal");
	gchar *template_filename_filetype_php = TEMPLATES_GET_FILENAME("template.filetype.php");
	gchar *template_filename_filetype_html = TEMPLATES_GET_FILENAME("template.filetype.html");
	gchar *template_filename_filetype_ruby = TEMPLATES_GET_FILENAME("template.filetype.ruby");

	time_t tp = time(NULL);
	const struct tm *tm = localtime(&tp);
	gchar *year = g_malloc0(5);
	gchar *date = utils_get_date();
	strftime(year, 5, "%Y", tm);

	// create the template files in the configuration directory, if they don't exist
	TEMPLATES_CREATE_FILE(template_filename_fileheader, templates_fileheader);
	TEMPLATES_CREATE_FILE(template_filename_gpl, templates_gpl_notice);
	TEMPLATES_CREATE_FILE(template_filename_bsd, templates_bsd_notice);
	TEMPLATES_CREATE_FILE(template_filename_function, templates_function_description);
	TEMPLATES_CREATE_FILE(template_filename_changelog, templates_changelog);
	TEMPLATES_CREATE_FILE(template_filename_filetype_none, templates_filetype_none);
	TEMPLATES_CREATE_FILE(template_filename_filetype_c, templates_filetype_c);
	TEMPLATES_CREATE_FILE(template_filename_filetype_cpp, templates_filetype_cpp);
	TEMPLATES_CREATE_FILE(template_filename_filetype_d, templates_filetype_d);
	TEMPLATES_CREATE_FILE(template_filename_filetype_java, templates_filetype_java);
	TEMPLATES_CREATE_FILE(template_filename_filetype_pascal, templates_filetype_pascal);
	TEMPLATES_CREATE_FILE(template_filename_filetype_php, templates_filetype_php);
	TEMPLATES_CREATE_FILE(template_filename_filetype_html, templates_filetype_html);
	TEMPLATES_CREATE_FILE(template_filename_filetype_ruby, templates_filetype_ruby);

	// read the contents
	TEMPLATES_READ_FILE(template_filename_fileheader, &templates[GEANY_TEMPLATE_FILEHEADER]);
	templates[GEANY_TEMPLATE_FILEHEADER] = templates_replace_all(templates[GEANY_TEMPLATE_FILEHEADER], year, date);

	TEMPLATES_READ_FILE(template_filename_gpl, &templates[GEANY_TEMPLATE_GPL]);
	templates[GEANY_TEMPLATE_GPL] = templates_replace_all(templates[GEANY_TEMPLATE_GPL], year, date);

	TEMPLATES_READ_FILE(template_filename_bsd, &templates[GEANY_TEMPLATE_BSD]);
	templates[GEANY_TEMPLATE_BSD] = templates_replace_all(templates[GEANY_TEMPLATE_BSD], year, date);

	TEMPLATES_READ_FILE(template_filename_function, &templates[GEANY_TEMPLATE_FUNCTION]);
	templates[GEANY_TEMPLATE_FUNCTION] = templates_replace_all(templates[GEANY_TEMPLATE_FUNCTION], year, date);

	TEMPLATES_READ_FILE(template_filename_changelog, &templates[GEANY_TEMPLATE_CHANGELOG]);
	templates[GEANY_TEMPLATE_CHANGELOG] = templates_replace_all(templates[GEANY_TEMPLATE_CHANGELOG], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_none, &templates[GEANY_TEMPLATE_FILETYPE_NONE]);
	templates[GEANY_TEMPLATE_FILETYPE_NONE] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_NONE], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_c, &templates[GEANY_TEMPLATE_FILETYPE_C]);
	templates[GEANY_TEMPLATE_FILETYPE_C] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_C], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_d, &templates[GEANY_TEMPLATE_FILETYPE_D]);
	templates[GEANY_TEMPLATE_FILETYPE_D] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_D], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_cpp, &templates[GEANY_TEMPLATE_FILETYPE_CPP]);
	templates[GEANY_TEMPLATE_FILETYPE_CPP] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_CPP], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_java, &templates[GEANY_TEMPLATE_FILETYPE_JAVA]);
	templates[GEANY_TEMPLATE_FILETYPE_JAVA] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_JAVA], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_pascal, &templates[GEANY_TEMPLATE_FILETYPE_PASCAL]);
	templates[GEANY_TEMPLATE_FILETYPE_PASCAL] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_PASCAL], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_php, &templates[GEANY_TEMPLATE_FILETYPE_PHP]);
	templates[GEANY_TEMPLATE_FILETYPE_PHP] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_PHP], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_html, &templates[GEANY_TEMPLATE_FILETYPE_HTML]);
	templates[GEANY_TEMPLATE_FILETYPE_HTML] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_HTML], year, date);

	TEMPLATES_READ_FILE(template_filename_filetype_ruby, &templates[GEANY_TEMPLATE_FILETYPE_RUBY]);
	templates[GEANY_TEMPLATE_FILETYPE_RUBY] = templates_replace_all(templates[GEANY_TEMPLATE_FILETYPE_RUBY], year, date);


	// free the whole stuff
	g_free(date);
	g_free(year);
	g_free(template_filename_fileheader);
	g_free(template_filename_gpl);
	g_free(template_filename_bsd);
	g_free(template_filename_function);
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


/* indent is used to make some whitespace between comment char and real start of the line
 * e.g. indent = 8 prints " *     here comes the text of the line"
 * indent is meant to be the whole amount of characters before the real line content follows, i.e.
 * 6 characters are filled with whitespace when the comment characters include " *" */
static gchar *make_comment_block(const gchar *comment_text, gint filetype_idx, gint indent)
{
	gchar *frame_start = "";	// to add before comment_text
	gchar *frame_end = "";		// to add after comment_text
	gchar *line_prefix;			// to add before every line in comment_text
	gchar *result;
	gchar *tmp;
	gchar *prefix;
	gchar **lines;
	gint i;

	/// TODO the following switch could be replaced by some intelligent code which reads
	/// frame_start, frame_end and line_prefix from the filetype definition files
	switch (filetype_idx)
	{
		case GEANY_FILETYPES_HTML:
		case GEANY_FILETYPES_XML:
		case GEANY_FILETYPES_DOCBOOK:
		{
			frame_start = "<!--\n";
			frame_end = "-->\n";
			line_prefix = "";
			break;
		}

		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		case GEANY_FILETYPES_DIFF:
		case GEANY_FILETYPES_TCL:
		case GEANY_FILETYPES_OMS:
		case GEANY_FILETYPES_CONF:
		{
			line_prefix = "#";
			break;
		}

		case GEANY_FILETYPES_LATEX:
		{
			line_prefix = "%";
			break;
		}

		case GEANY_FILETYPES_VHDL:
		{
			line_prefix = "--";
			break;
		}

		case GEANY_FILETYPES_FORTRAN:
		{
			line_prefix = "c";
			break;
		}

		case GEANY_FILETYPES_ASM:
		{
			line_prefix = ";";
			break;
		}

		case GEANY_FILETYPES_PASCAL:
		{
			frame_start = "{\n";
			frame_end = "}\n";
			line_prefix = "";
			break;
		}

		case GEANY_FILETYPES_PHP:
		{
			frame_start = "<?\n/*\n";
			frame_end = " */\n?>\n";
			line_prefix = " *";
			break;
		}

		case GEANY_FILETYPES_CAML:
		{
			frame_start = "(*\n";
			frame_end = " *)\n";
			line_prefix = " *";
			break;
		}

		case GEANY_FILETYPES_ALL:
		{
			line_prefix = "";
			break;
		}

		default: // guess /* */ is appropriate
		{
			frame_start = "/*\n";
			frame_end = " */\n";
			line_prefix = " *";
		}
	}

	// construct the real prefix with given amount of whitespace
	i = (indent > strlen(line_prefix)) ? (indent - strlen(line_prefix)) : strlen(line_prefix);
	tmp = g_strnfill(i, ' ');
	prefix = g_strconcat(line_prefix, tmp, NULL);
	g_free(tmp);


	// add line_prefix to every line of comment_text
	lines = g_strsplit(comment_text, "\n", -1);
	for (i = 0; i < (g_strv_length(lines) - 1); i++)
	{
		tmp = lines[i];
		lines[i] = g_strconcat(prefix, tmp, NULL);
		g_free(tmp);
	}
	tmp = g_strjoinv("\n", lines);

	// add frame_start and frame_end
	result = g_strconcat(frame_start, tmp, frame_end, NULL);

	g_free(prefix);
	g_free(tmp);
	g_strfreev(lines);
	return result;
}


gchar *templates_get_template_licence(gint filetype_idx, gint licence_type)
{
	if (licence_type != GEANY_TEMPLATE_GPL && licence_type != GEANY_TEMPLATE_BSD)
		return NULL;

	return make_comment_block(templates[licence_type], filetype_idx, 8);
}


static gchar *get_file_header(filetype *ft, const gchar *fname)
{
	gchar *template = g_strdup(templates[GEANY_TEMPLATE_FILEHEADER]);
	gchar *shortname;
	gchar *result;
	gchar *date = utils_get_date_time();

	if (fname == NULL)
	{
		if (FILETYPE_ID(ft) == GEANY_FILETYPES_ALL)
			shortname = g_strdup(GEANY_STRING_UNTITLED);
		else
			shortname = g_strconcat(GEANY_STRING_UNTITLED, ".", ft->extension, NULL);
	}
	else
		shortname = g_path_get_basename(fname);

	template = utils_str_replace(template, "{filename}", shortname);

	template = utils_str_replace(template, "{gpl}", templates[GEANY_TEMPLATE_GPL]);

	template = utils_str_replace(template, "{bsd}", templates[GEANY_TEMPLATE_BSD]);

	template = utils_str_replace(template, "{datetime}", date);

	result = make_comment_block(template, FILETYPE_ID(ft), 8);

	g_free(template);
	g_free(shortname);
	g_free(date);
	return result;
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


gchar *templates_get_template_function(gint filetype_idx, const gchar *func_name)
{
	gchar *template = g_strdup(templates[GEANY_TEMPLATE_FUNCTION]);
	gchar *date = utils_get_date();
	gchar *datetime = utils_get_date_time();
	gchar *result;

	template = utils_str_replace(template, "{date}", date);
	template = utils_str_replace(template, "{datetime}", datetime);
	template = utils_str_replace(template, "{functionname}", (func_name) ? func_name : "");

	result = make_comment_block(template, filetype_idx, 3);

	g_free(template);
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
