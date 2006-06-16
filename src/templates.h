/*
 *      templates.h - this file is part of Geany, a fast and lightweight IDE
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

#ifndef GEANY_TEMPLATES_H
#define GEANY_TEMPLATES_H 1


void templates_init(void);

gchar *templates_get_template_fileheader(gint template, const gchar *extension, gint idx);

gchar *templates_get_template_changelog(void);

gchar *templates_get_template_generic(gint template);

gchar *templates_get_template_function(gint template, const gchar *func_name);

gchar *templates_get_template_gpl(gint template);

void templates_free_templates(void);


enum
{
	GEANY_TEMPLATE_GPL_PASCAL = 0,
	GEANY_TEMPLATE_GPL_ROUTE,
	GEANY_TEMPLATE_GPL,
	GEANY_TEMPLATE_FILEHEADER_PASCAL,
	GEANY_TEMPLATE_FILEHEADER_ROUTE,
	GEANY_TEMPLATE_FILEHEADER,
	GEANY_TEMPLATE_CHANGELOG,
	GEANY_TEMPLATE_FUNCTION,
	GEANY_TEMPLATE_FUNCTION_PASCAL,
	GEANY_TEMPLATE_FUNCTION_ROUTE,
	GEANY_TEMPLATE_MULTILINE,
	GEANY_TEMPLATE_MULTILINE_PASCAL,
	GEANY_TEMPLATE_MULTILINE_ROUTE,

	GEANY_TEMPLATE_FILETYPE_NONE,
	GEANY_TEMPLATE_FILETYPE_C,
	GEANY_TEMPLATE_FILETYPE_CPP,
	GEANY_TEMPLATE_FILETYPE_JAVA,
	GEANY_TEMPLATE_FILETYPE_PHP,
	GEANY_TEMPLATE_FILETYPE_PASCAL,
	GEANY_TEMPLATE_FILETYPE_RUBY,

	GEANY_MAX_TEMPLATES
};




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
 */\n\n";

static const gchar templates_fileheader_pascal[] = "\
{\n\
      {filename}\n\
\n\
      Copyright {year} {developer} <{mail}>\n\
\n\
{gpl}\
}\n\n";

static const gchar templates_fileheader_route[] = "\
#\n\
#      {filename}\n\
#\n\
#      Copyright {year} {developer} <{mail}>\n\
#\n\
{gpl}\
#\n\n";

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

#endif
