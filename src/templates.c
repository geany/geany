/*
 *      templates.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
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

/*
 * Templates to insert into the current document, or filetype templates to create a new
 * document from.
 */

#include <time.h>
#include <string.h>

#include "geany.h"

#include "templates.h"
#include "prefs.h"
#include "support.h"
#include "utils.h"
#include "document.h"
#include "filetypes.h"
#include "ui_utils.h"


#define TEMPLATE_DATE_FORMAT "%Y-%m-%d"
#define TEMPLATE_DATETIME_FORMAT "%d.%m.%Y %H:%M:%S %Z"

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

static const gchar templates_filetype_none[] = "";

static const gchar templates_filetype_c[] = "{fileheader}\n\n\
#include <stdio.h>\n\
\n\
int main(int argc, char** argv)\n\
{\n\
	\n\
	return 0;\n\
}\n\
";

static const gchar templates_filetype_cpp[] = "{fileheader}\n\n\
#include <iostream>\n\
\n\
int main(int argc, char** argv)\n\
{\n\
	\n\
	return 0;\n\
}\n\
";

static const gchar templates_filetype_d[] = "{fileheader}\n\n\
import std.stdio;\n\
\n\
int main(char[][] args)\n\
{\n\
	\n\
	return 0;\n\
}\n\
";

static const gchar templates_filetype_php[] = "\
<?php\n\
{fileheader}\
?>\n\n\
<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\
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

static const gchar templates_filetype_html[] = "{fileheader}\n\
<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\
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

static const gchar templates_filetype_pascal[] = "{fileheader}\n\n\
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

static const gchar templates_filetype_java[] = "{fileheader}\n\n\
public class {untitled} {\n\
\n\
	public static void main (String args[]) {\
		\n\
		\n\
	}\n\
}\n\
";

static const gchar templates_filetype_ruby[] = "{fileheader}\n\n\
class StdClass\n\
	def initialize\n\
		\n\
	end\n\
end\n\
\n\
x = StdClass.new\n\
";

static gchar *templates[GEANY_MAX_TEMPLATES];
static gchar *ft_templates[GEANY_MAX_FILE_TYPES] = {NULL};


// some simple macros to reduce code size and make the code readable
#define TEMPLATES_GET_FILENAME(shortname) \
	g_strconcat(app->configdir, \
		G_DIR_SEPARATOR_S GEANY_TEMPLATES_SUBDIR G_DIR_SEPARATOR_S, shortname, NULL)

#define TEMPLATES_CREATE_FILE(fname, text)	\
	if (! g_file_test(fname, G_FILE_TEST_EXISTS)) \
		utils_write_file(fname, text)

#define TEMPLATES_READ_FILE(fname, contents_ptr) \
	g_file_get_contents(fname, contents_ptr, NULL, NULL);


// prototype, because this function should never be used outside of templates.c
static gchar *templates_replace_all(gchar *source, const gchar *year, const gchar *date);


static void init_general_templates(const gchar *year, const gchar *date)
{
	gchar *template_filename_fileheader = TEMPLATES_GET_FILENAME("fileheader");
	gchar *template_filename_gpl = TEMPLATES_GET_FILENAME("gpl");
	gchar *template_filename_bsd = TEMPLATES_GET_FILENAME("bsd");
	gchar *template_filename_function = TEMPLATES_GET_FILENAME("function");
	gchar *template_filename_changelog = TEMPLATES_GET_FILENAME("changelog");

	// create the template files in the configuration directory, if they don't exist
	TEMPLATES_CREATE_FILE(template_filename_fileheader, templates_fileheader);
	TEMPLATES_CREATE_FILE(template_filename_gpl, templates_gpl_notice);
	TEMPLATES_CREATE_FILE(template_filename_bsd, templates_bsd_notice);
	TEMPLATES_CREATE_FILE(template_filename_function, templates_function_description);
	TEMPLATES_CREATE_FILE(template_filename_changelog, templates_changelog);

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

	// free the whole stuff
	g_free(template_filename_fileheader);
	g_free(template_filename_gpl);
	g_free(template_filename_bsd);
	g_free(template_filename_function);
	g_free(template_filename_changelog);
}


static void init_ft_templates(const gchar *year, const gchar *date)
{
	filetype_id ft_id;

	for (ft_id = 0; ft_id < GEANY_MAX_FILE_TYPES; ft_id++)
	{
		gchar *ext = filetypes_get_conf_extension(ft_id);
		gchar *shortname = g_strconcat("filetype.", ext, NULL);
		gchar *fname = TEMPLATES_GET_FILENAME(shortname);

		switch (ft_id)
		{
			case GEANY_FILETYPES_ALL:	TEMPLATES_CREATE_FILE(fname, templates_filetype_none); break;
			case GEANY_FILETYPES_C:		TEMPLATES_CREATE_FILE(fname, templates_filetype_c); break;
			case GEANY_FILETYPES_CPP:	TEMPLATES_CREATE_FILE(fname, templates_filetype_cpp); break;
			case GEANY_FILETYPES_D:		TEMPLATES_CREATE_FILE(fname, templates_filetype_d); break;
			case GEANY_FILETYPES_JAVA:	TEMPLATES_CREATE_FILE(fname, templates_filetype_java); break;
			case GEANY_FILETYPES_PASCAL:TEMPLATES_CREATE_FILE(fname, templates_filetype_pascal); break;
			case GEANY_FILETYPES_PHP:	TEMPLATES_CREATE_FILE(fname, templates_filetype_php); break;
			case GEANY_FILETYPES_HTML:	TEMPLATES_CREATE_FILE(fname, templates_filetype_html); break;
			case GEANY_FILETYPES_RUBY:	TEMPLATES_CREATE_FILE(fname, templates_filetype_ruby); break;
			default: break;
		}
		TEMPLATES_READ_FILE(fname, &ft_templates[ft_id]);
		ft_templates[ft_id] = templates_replace_all(ft_templates[ft_id], year, date);

		g_free(fname);
		g_free(shortname);
		g_free(ext);
	}
}


static void
on_new_with_template                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	filetype *ft = user_data;
	gchar *template = templates_get_template_new_file(ft);

	document_new_file(NULL, ft, template);
	g_free(template);
}


// template items for the new file menu
static void create_new_menu_items()
{
	GtkWidget *template_menu = lookup_widget(app->window, "menu_new_with_template1_menu");
	filetype_id ft_id;

	for (ft_id = 0; ft_id < GEANY_MAX_FILE_TYPES; ft_id++)
	{
		GtkWidget *tmp_menu, *tmp_button;
		filetype *ft = filetypes[ft_id];
		const gchar *label = ft->title;

		if (ft_templates[ft_id] == NULL)
			continue;
		if (ft_id == GEANY_FILETYPES_ALL)
			label = _("None");
		tmp_menu = gtk_menu_item_new_with_label(label);
		tmp_button = gtk_menu_item_new_with_label(label);
		gtk_widget_show(tmp_menu);
		gtk_widget_show(tmp_button);
		gtk_container_add(GTK_CONTAINER(template_menu), tmp_menu);
		gtk_container_add(GTK_CONTAINER(ui_widgets.new_file_menu), tmp_button);
		g_signal_connect((gpointer) tmp_menu, "activate",
			G_CALLBACK(on_new_with_template), (gpointer) ft);
		g_signal_connect((gpointer) tmp_button, "activate",
			G_CALLBACK(on_new_with_template), (gpointer) ft);
	}
}


void templates_init(void)
{
	gchar *year = utils_get_date_time("%Y", NULL);
	gchar *date = utils_get_date_time(TEMPLATE_DATE_FORMAT, NULL);

	init_general_templates(year, date);
	init_ft_templates(year, date);
	create_new_menu_items();

	g_free(date);
	g_free(year);
}


/* indent is used to make some whitespace between comment char and real start of the line
 * e.g. indent = 8 prints " *     here comes the text of the line"
 * indent is meant to be the whole amount of characters before the real line content follows, i.e.
 * 6 characters are filled with whitespace when the comment characters include " *" */
static gchar *make_comment_block(const gchar *comment_text, gint filetype_idx, guint indent)
{
	gchar *frame_start = "";	// to add before comment_text
	gchar *frame_end = "";		// to add after comment_text
	gchar *line_prefix;			// to add before every line in comment_text
	gchar *result;
	gchar *tmp;
	gchar *prefix;
	gchar **lines;
	guint i;

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

		case GEANY_FILETYPES_JS:
		case GEANY_FILETYPES_HAXE:
		{
			line_prefix = "//";
			break;
		}

		case GEANY_FILETYPES_LATEX:
		{
			line_prefix = "%";
			break;
		}

		case GEANY_FILETYPES_HASKELL:
		case GEANY_FILETYPES_VHDL:
		case GEANY_FILETYPES_LUA:
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

		case GEANY_FILETYPES_BASIC:
		{
			line_prefix = "'";
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
			frame_start = "/*\n";
			frame_end = " */\n";
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


gchar *templates_get_template_fileheader(gint filetype_idx, const gchar *fname)
{
	gchar *template = g_strdup(templates[GEANY_TEMPLATE_FILEHEADER]);
	gchar *shortname;
	gchar *result;
	gchar *date = utils_get_date_time(TEMPLATE_DATETIME_FORMAT, NULL);
	filetype_id ft_id = filetype_idx;
	filetype *ft = filetypes[ft_id];

	filetypes_load_config(ft_id);	// load any user extension setting

	if (fname == NULL)
	{
		if (ft_id == GEANY_FILETYPES_ALL)
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

	result = make_comment_block(template, ft_id, 8);

	g_free(template);
	g_free(shortname);
	g_free(date);
	return result;
}


static gchar *get_file_template(filetype *ft)
{
	filetype_id ft_id = FILETYPE_ID(ft);

	return g_strdup(ft_templates[ft_id]);
}


gchar *templates_get_template_new_file(struct filetype *ft)
{
	gchar *ft_template = NULL;
	gchar *file_header = NULL;

	if (FILETYPE_ID(ft) == GEANY_FILETYPES_ALL)
		return get_file_template(ft);

	file_header = templates_get_template_fileheader(ft->id, NULL);	// file template only used for new files
	ft_template = get_file_template(ft);
	ft_template = utils_str_replace(ft_template, "{fileheader}", file_header);
	g_free(file_header);
	return ft_template;
}


gchar *templates_get_template_generic(gint template)
{
	return g_strdup(templates[template]);
}


gchar *templates_get_template_function(gint filetype_idx, const gchar *func_name)
{
	gchar *template = g_strdup(templates[GEANY_TEMPLATE_FUNCTION]);
	gchar *date = utils_get_date_time(TEMPLATE_DATE_FORMAT, NULL);
	gchar *datetime = utils_get_date_time(TEMPLATE_DATETIME_FORMAT, NULL);
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
	gchar *date = utils_get_date_time(TEMPLATE_DATETIME_FORMAT, NULL);
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


static gchar *templates_replace_all(gchar *text, const gchar *year, const gchar *date)
{
	text = utils_str_replace(text, "{year}", year);
	text = utils_str_replace(text, "{date}", date);
	text = utils_str_replace(text, "{version}", prefs.template_version);
	text = utils_str_replace(text, "{initial}", prefs.template_initial);
	text = utils_str_replace(text, "{developer}", prefs.template_developer);
	text = utils_str_replace(text, "{mail}", prefs.template_mail);
	text = utils_str_replace(text, "{company}", prefs.template_company);
	text = utils_str_replace(text, "{untitled}", GEANY_STRING_UNTITLED);
	text = utils_str_replace(text, "{geanyversion}", "Geany " VERSION);

	return text;
}

