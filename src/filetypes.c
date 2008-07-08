/*
 *      filetypes.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * Filetype detection, file extensions and filetype menu items.
 */

#include <string.h>
#include <glib/gstdio.h>

#include "geany.h"
#include "filetypes.h"
#include "highlighting.h"
#include "support.h"
#include "templates.h"
#include "document.h"
#include "msgwindow.h"
#include "utils.h"
#include "sciwrappers.h"
#include "ui_utils.h"


/* This type 'inherits' from filetype so FullFileType* can be cast to filetype*. */
typedef struct FullFileType
{
	GeanyFiletype	public;
	/* Private fields */
	GtkWidget		*menu_item;			/* holds a pointer to the menu item for this filetype */
	gboolean		keyfile_loaded;
}
FullFileType;


GPtrArray *filetypes_array = NULL;	/* Dynamic array of filetype pointers */

GHashTable *filetypes_hash = NULL;	/* Hash of filetype pointers based on name keys */


static void create_radio_menu_item(GtkWidget *menu, const gchar *label, GeanyFiletype *ftype);


static void init_builtin_filetypes(void)
{
#define C	/* these macros are only to ease navigation */
	filetypes[GEANY_FILETYPES_C]->lang = 0;
	filetypes[GEANY_FILETYPES_C]->name = g_strdup("C");
	filetypes[GEANY_FILETYPES_C]->title = g_strdup_printf(_("%s source file"), "C");
	filetypes[GEANY_FILETYPES_C]->extension = g_strdup("c");
	filetypes[GEANY_FILETYPES_C]->pattern = utils_strv_new("*.c", "*.h", NULL);
	filetypes[GEANY_FILETYPES_C]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_C]->comment_close = g_strdup("*/");

#define CPP
	filetypes[GEANY_FILETYPES_CPP]->lang = 1;
	filetypes[GEANY_FILETYPES_CPP]->name = g_strdup("C++");
	filetypes[GEANY_FILETYPES_CPP]->title = g_strdup_printf(_("%s source file"), "C++");
	filetypes[GEANY_FILETYPES_CPP]->extension = g_strdup("cpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern = utils_strv_new("*.cpp", "*.cxx", "*.c++", "*.cc",
		"*.h", "*.hpp", "*.hxx", "*.h++", "*.hh", "*.C", NULL);
	filetypes[GEANY_FILETYPES_CPP]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_CPP]->comment_close = NULL;

#define CS
	filetypes[GEANY_FILETYPES_CS]->lang = 25;
	filetypes[GEANY_FILETYPES_CS]->name = g_strdup("C#");
	filetypes[GEANY_FILETYPES_CS]->title = g_strdup_printf(_("%s source file"), "C#");
	filetypes[GEANY_FILETYPES_CS]->extension = g_strdup("cs");
	filetypes[GEANY_FILETYPES_CS]->pattern = utils_strv_new("*.cs", "*.vala", NULL);
	filetypes[GEANY_FILETYPES_CS]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_CS]->comment_close = NULL;

#define D
	filetypes[GEANY_FILETYPES_D]->lang = 17;
	filetypes[GEANY_FILETYPES_D]->name = g_strdup("D");
	filetypes[GEANY_FILETYPES_D]->title = g_strdup_printf(_("%s source file"), "D");
	filetypes[GEANY_FILETYPES_D]->extension = g_strdup("d");
	filetypes[GEANY_FILETYPES_D]->pattern = utils_strv_new("*.d", "*.di", NULL);
	filetypes[GEANY_FILETYPES_D]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_D]->comment_close = NULL;

#define JAVA
	filetypes[GEANY_FILETYPES_JAVA]->name = g_strdup("Java");
	filetypes[GEANY_FILETYPES_JAVA]->lang = 2;
	filetypes[GEANY_FILETYPES_JAVA]->title = g_strdup_printf(_("%s source file"), "Java");
	filetypes[GEANY_FILETYPES_JAVA]->extension = g_strdup("java");
	filetypes[GEANY_FILETYPES_JAVA]->pattern = utils_strv_new("*.java", "*.jsp", NULL);
	filetypes[GEANY_FILETYPES_JAVA]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_JAVA]->comment_close = g_strdup("*/");

#define PAS /* to avoid warnings when building under Windows, the symbol PASCAL is there defined */
	filetypes[GEANY_FILETYPES_PASCAL]->lang = 4;
	filetypes[GEANY_FILETYPES_PASCAL]->name = g_strdup("Pascal");
	filetypes[GEANY_FILETYPES_PASCAL]->title = g_strdup_printf(_("%s source file"), "Pascal");
	filetypes[GEANY_FILETYPES_PASCAL]->extension = g_strdup("pas");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern = utils_strv_new("*.pas", "*.pp", "*.inc", "*.dpr",
		"*.dpk", NULL);
	filetypes[GEANY_FILETYPES_PASCAL]->comment_open = g_strdup("{");
	filetypes[GEANY_FILETYPES_PASCAL]->comment_close = g_strdup("}");

#define ASM
	filetypes[GEANY_FILETYPES_ASM]->lang = 9;
	filetypes[GEANY_FILETYPES_ASM]->name = g_strdup("ASM");
	filetypes[GEANY_FILETYPES_ASM]->title = g_strdup_printf(_("%s source file"), "Assembler");
	filetypes[GEANY_FILETYPES_ASM]->extension = g_strdup("asm");
	filetypes[GEANY_FILETYPES_ASM]->pattern = utils_strv_new("*.asm", NULL);
	filetypes[GEANY_FILETYPES_ASM]->comment_open = g_strdup(";");
	filetypes[GEANY_FILETYPES_ASM]->comment_close = NULL;

#define BASIC
	filetypes[GEANY_FILETYPES_BASIC]->lang = 26;
	filetypes[GEANY_FILETYPES_BASIC]->name = g_strdup("FreeBasic");
	filetypes[GEANY_FILETYPES_BASIC]->title = g_strdup_printf(_("%s source file"), "FreeBasic");
	filetypes[GEANY_FILETYPES_BASIC]->extension = g_strdup("bas");
	filetypes[GEANY_FILETYPES_BASIC]->pattern = utils_strv_new("*.bas", "*.bi", NULL);
	filetypes[GEANY_FILETYPES_BASIC]->comment_open = g_strdup("'");
	filetypes[GEANY_FILETYPES_BASIC]->comment_close = NULL;

#define FORTRAN
	filetypes[GEANY_FILETYPES_FORTRAN]->lang = 18;
	filetypes[GEANY_FILETYPES_FORTRAN]->name = g_strdup("Fortran");
	filetypes[GEANY_FILETYPES_FORTRAN]->title = g_strdup_printf(_("%s source file"), "Fortran (F77)");
	filetypes[GEANY_FILETYPES_FORTRAN]->extension = g_strdup("f");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern = utils_strv_new("*.f", "*.for", "*.ftn", "*.f77",
		"*.f90", "*.f95", NULL);
	filetypes[GEANY_FILETYPES_FORTRAN]->comment_open = g_strdup("c");
	filetypes[GEANY_FILETYPES_FORTRAN]->comment_close = NULL;

#define CAML
	filetypes[GEANY_FILETYPES_CAML]->lang = -2;
	filetypes[GEANY_FILETYPES_CAML]->name = g_strdup("CAML");
	filetypes[GEANY_FILETYPES_CAML]->title = g_strdup_printf(_("%s source file"), "(O)Caml");
	filetypes[GEANY_FILETYPES_CAML]->extension = g_strdup("ml");
	filetypes[GEANY_FILETYPES_CAML]->pattern = utils_strv_new("*.ml", "*.mli", NULL);
	filetypes[GEANY_FILETYPES_CAML]->comment_open = g_strdup("(*");
	filetypes[GEANY_FILETYPES_CAML]->comment_close = g_strdup("*)");

#define PERL
	filetypes[GEANY_FILETYPES_PERL]->lang = 5;
	filetypes[GEANY_FILETYPES_PERL]->name = g_strdup("Perl");
	filetypes[GEANY_FILETYPES_PERL]->title = g_strdup_printf(_("%s source file"), "Perl");
	filetypes[GEANY_FILETYPES_PERL]->extension = g_strdup("pl");
	filetypes[GEANY_FILETYPES_PERL]->pattern = utils_strv_new("*.pl", "*.perl", "*.pm", "*.agi",
		"*.pod", NULL);
	filetypes[GEANY_FILETYPES_PERL]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_PERL]->comment_close = NULL;

#define PHP
	filetypes[GEANY_FILETYPES_PHP]->lang = 6;
	filetypes[GEANY_FILETYPES_PHP]->name = g_strdup("PHP");
	filetypes[GEANY_FILETYPES_PHP]->title = g_strdup_printf(_("%s source file"), "PHP");
	filetypes[GEANY_FILETYPES_PHP]->extension = g_strdup("php");
	filetypes[GEANY_FILETYPES_PHP]->pattern = utils_strv_new("*.php", "*.php3", "*.php4", "*.php5",
		"*.phtml", NULL);
	filetypes[GEANY_FILETYPES_PHP]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_PHP]->comment_close = NULL;

#define JAVASCRIPT
	filetypes[GEANY_FILETYPES_JS]->lang = 23;
	filetypes[GEANY_FILETYPES_JS]->name = g_strdup("Javascript");
	filetypes[GEANY_FILETYPES_JS]->title = g_strdup_printf(_("%s source file"), "Javascript");
	filetypes[GEANY_FILETYPES_JS]->extension = g_strdup("js");
	filetypes[GEANY_FILETYPES_JS]->pattern = utils_strv_new("*.js", NULL);
	filetypes[GEANY_FILETYPES_JS]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_JS]->comment_close = NULL;

#define PYTHON
	filetypes[GEANY_FILETYPES_PYTHON]->lang = 7;
	filetypes[GEANY_FILETYPES_PYTHON]->name = g_strdup("Python");
	filetypes[GEANY_FILETYPES_PYTHON]->title = g_strdup_printf(_("%s source file"), "Python");
	filetypes[GEANY_FILETYPES_PYTHON]->extension = g_strdup("py");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern = utils_strv_new("*.py", "*.pyw", NULL);
	filetypes[GEANY_FILETYPES_PYTHON]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_PYTHON]->comment_close = NULL;

#define RUBY
	filetypes[GEANY_FILETYPES_RUBY]->lang = 14;
	filetypes[GEANY_FILETYPES_RUBY]->name = g_strdup("Ruby");
	filetypes[GEANY_FILETYPES_RUBY]->title = g_strdup_printf(_("%s source file"), "Ruby");
	filetypes[GEANY_FILETYPES_RUBY]->extension = g_strdup("rb");
	filetypes[GEANY_FILETYPES_RUBY]->pattern = utils_strv_new("*.rb", "*.rhtml", "*.ruby", NULL);
	filetypes[GEANY_FILETYPES_RUBY]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_RUBY]->comment_close = NULL;

#define TCL
	filetypes[GEANY_FILETYPES_TCL]->lang = 15;
	filetypes[GEANY_FILETYPES_TCL]->name = g_strdup("Tcl");
	filetypes[GEANY_FILETYPES_TCL]->title = g_strdup_printf(_("%s source file"), "Tcl");
	filetypes[GEANY_FILETYPES_TCL]->extension = g_strdup("tcl");
	filetypes[GEANY_FILETYPES_TCL]->pattern = utils_strv_new("*.tcl", "*.tk", "*.wish", NULL);
	filetypes[GEANY_FILETYPES_TCL]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_TCL]->comment_close = NULL;

#define LUA
	filetypes[GEANY_FILETYPES_LUA]->lang = 22;
	filetypes[GEANY_FILETYPES_LUA]->name = g_strdup("Lua");
	filetypes[GEANY_FILETYPES_LUA]->title = g_strdup_printf(_("%s source file"), "Lua");
	filetypes[GEANY_FILETYPES_LUA]->extension = g_strdup("lua");
	filetypes[GEANY_FILETYPES_LUA]->pattern = utils_strv_new("*.lua", NULL);
	filetypes[GEANY_FILETYPES_LUA]->comment_open = g_strdup("--");
	filetypes[GEANY_FILETYPES_LUA]->comment_close = NULL;

#define FERITE
	filetypes[GEANY_FILETYPES_FERITE]->lang = 19;
	filetypes[GEANY_FILETYPES_FERITE]->name = g_strdup("Ferite");
	filetypes[GEANY_FILETYPES_FERITE]->title = g_strdup_printf(_("%s source file"), "Ferite");
	filetypes[GEANY_FILETYPES_FERITE]->extension = g_strdup("fe");
	filetypes[GEANY_FILETYPES_FERITE]->pattern = utils_strv_new("*.fe", NULL);
	filetypes[GEANY_FILETYPES_FERITE]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_FERITE]->comment_close = g_strdup("*/");

#define HASKELL
	filetypes[GEANY_FILETYPES_HASKELL]->lang = 24;
	filetypes[GEANY_FILETYPES_HASKELL]->name = g_strdup("Haskell");
	filetypes[GEANY_FILETYPES_HASKELL]->title = g_strdup_printf(_("%s source file"), "Haskell");
	filetypes[GEANY_FILETYPES_HASKELL]->extension = g_strdup("hs");
	filetypes[GEANY_FILETYPES_HASKELL]->pattern = utils_strv_new("*.hs", "*.lhs", NULL);
	filetypes[GEANY_FILETYPES_HASKELL]->comment_open = g_strdup("--");
	filetypes[GEANY_FILETYPES_HASKELL]->comment_close = NULL;

#define SH
	filetypes[GEANY_FILETYPES_SH]->lang = 16;
	filetypes[GEANY_FILETYPES_SH]->name = g_strdup("Sh");
	filetypes[GEANY_FILETYPES_SH]->title = g_strdup(_("Shell script file"));
	filetypes[GEANY_FILETYPES_SH]->extension = g_strdup("sh");
	filetypes[GEANY_FILETYPES_SH]->pattern = utils_strv_new("*.sh", "configure", "configure.in",
		"configure.in.in", "configure.ac", "*.ksh", "*.zsh", "*.ash", "*.bash", NULL);
	filetypes[GEANY_FILETYPES_SH]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_SH]->comment_close = NULL;

#define MAKE
	filetypes[GEANY_FILETYPES_MAKE]->lang = 3;
	filetypes[GEANY_FILETYPES_MAKE]->name = g_strdup("Make");
	filetypes[GEANY_FILETYPES_MAKE]->title = g_strdup(_("Makefile"));
	filetypes[GEANY_FILETYPES_MAKE]->extension = g_strdup("mak");
	filetypes[GEANY_FILETYPES_MAKE]->pattern = utils_strv_new(
		"*.mak", "*.mk", "GNUmakefile", "makefile", "Makefile", "makefile.*", "Makefile.*", NULL);
	filetypes[GEANY_FILETYPES_MAKE]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_MAKE]->comment_close = NULL;

#define XML
	filetypes[GEANY_FILETYPES_XML]->lang = -2;
	filetypes[GEANY_FILETYPES_XML]->name = g_strdup("XML");
	filetypes[GEANY_FILETYPES_XML]->title = g_strdup(_("XML document"));
	filetypes[GEANY_FILETYPES_XML]->extension = g_strdup("xml");
	filetypes[GEANY_FILETYPES_XML]->pattern = utils_strv_new(
		"*.xml", "*.sgml", "*.xsl", "*.xslt", "*.xsd", "*.xhtml", NULL);
	filetypes[GEANY_FILETYPES_XML]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_XML]->comment_close = g_strdup("-->");

#define DOCBOOK
	filetypes[GEANY_FILETYPES_DOCBOOK]->lang = 12;
	filetypes[GEANY_FILETYPES_DOCBOOK]->name = g_strdup("Docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->title = g_strdup_printf(_("%s source file"), "Docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->extension = g_strdup("docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern = utils_strv_new("*.docbook", NULL);
	filetypes[GEANY_FILETYPES_DOCBOOK]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_DOCBOOK]->comment_close = g_strdup("-->");

#define HTML
	filetypes[GEANY_FILETYPES_HTML]->lang = 29;
	filetypes[GEANY_FILETYPES_HTML]->name = g_strdup("HTML");
	filetypes[GEANY_FILETYPES_HTML]->title = g_strdup_printf(_("%s source file"), "HTML");
	filetypes[GEANY_FILETYPES_HTML]->extension = g_strdup("html");
	filetypes[GEANY_FILETYPES_HTML]->pattern = utils_strv_new(
		"*.htm", "*.html", "*.shtml", "*.hta", "*.htd", "*.htt", "*.cfm", NULL);
	filetypes[GEANY_FILETYPES_HTML]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_HTML]->comment_close = g_strdup("-->");

#define CSS
	filetypes[GEANY_FILETYPES_CSS]->lang = 13;
	filetypes[GEANY_FILETYPES_CSS]->name = g_strdup("CSS");
	filetypes[GEANY_FILETYPES_CSS]->title = g_strdup(_("Cascading StyleSheet"));
	filetypes[GEANY_FILETYPES_CSS]->extension = g_strdup("css");
	filetypes[GEANY_FILETYPES_CSS]->pattern = utils_strv_new("*.css", NULL);
	filetypes[GEANY_FILETYPES_CSS]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_CSS]->comment_close = g_strdup("*/");

#define SQL
	filetypes[GEANY_FILETYPES_SQL]->lang = 11;
	filetypes[GEANY_FILETYPES_SQL]->name = g_strdup("SQL");
	filetypes[GEANY_FILETYPES_SQL]->title = g_strdup(_("SQL Dump file"));
	filetypes[GEANY_FILETYPES_SQL]->extension = g_strdup("sql");
	filetypes[GEANY_FILETYPES_SQL]->pattern = utils_strv_new("*.sql", NULL);
	filetypes[GEANY_FILETYPES_SQL]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_SQL]->comment_close = g_strdup("*/");

#define LATEX
	filetypes[GEANY_FILETYPES_LATEX]->lang = 8;
	filetypes[GEANY_FILETYPES_LATEX]->name = g_strdup("LaTeX");
	filetypes[GEANY_FILETYPES_LATEX]->title = g_strdup_printf(_("%s source file"), "LaTeX");
	filetypes[GEANY_FILETYPES_LATEX]->extension = g_strdup("tex");
	filetypes[GEANY_FILETYPES_LATEX]->pattern = utils_strv_new("*.tex", "*.sty", "*.idx", "*.ltx", NULL);
	filetypes[GEANY_FILETYPES_LATEX]->comment_open = g_strdup("%");
	filetypes[GEANY_FILETYPES_LATEX]->comment_close = NULL;

#define OMS
	filetypes[GEANY_FILETYPES_OMS]->lang = -2;
	filetypes[GEANY_FILETYPES_OMS]->name = g_strdup("O-Matrix");
	filetypes[GEANY_FILETYPES_OMS]->title = g_strdup_printf(_("%s source file"), "O-Matrix");
	filetypes[GEANY_FILETYPES_OMS]->extension = g_strdup("oms");
	filetypes[GEANY_FILETYPES_OMS]->pattern = utils_strv_new("*.oms", NULL);
	filetypes[GEANY_FILETYPES_OMS]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_OMS]->comment_close = NULL;

#define VHDL
	filetypes[GEANY_FILETYPES_VHDL]->lang = 21;
	filetypes[GEANY_FILETYPES_VHDL]->name = g_strdup("VHDL");
	filetypes[GEANY_FILETYPES_VHDL]->title = g_strdup_printf(_("%s source file"), "VHDL");
	filetypes[GEANY_FILETYPES_VHDL]->extension = g_strdup("vhd");
	filetypes[GEANY_FILETYPES_VHDL]->pattern = utils_strv_new("*.vhd", "*.vhdl", NULL);
	filetypes[GEANY_FILETYPES_VHDL]->comment_open = g_strdup("--");
	filetypes[GEANY_FILETYPES_VHDL]->comment_close = NULL;

#define DIFF
	filetypes[GEANY_FILETYPES_DIFF]->lang = 20;
	filetypes[GEANY_FILETYPES_DIFF]->name = g_strdup("Diff");
	filetypes[GEANY_FILETYPES_DIFF]->title = g_strdup(_("Diff file"));
	filetypes[GEANY_FILETYPES_DIFF]->extension = g_strdup("diff");
	filetypes[GEANY_FILETYPES_DIFF]->pattern = utils_strv_new("*.diff", "*.patch", "*.rej", NULL);
	filetypes[GEANY_FILETYPES_DIFF]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_DIFF]->comment_close = NULL;

#define CONF
	filetypes[GEANY_FILETYPES_CONF]->lang = 10;
	filetypes[GEANY_FILETYPES_CONF]->name = g_strdup("Conf");
	filetypes[GEANY_FILETYPES_CONF]->title = g_strdup(_("Config file"));
	filetypes[GEANY_FILETYPES_CONF]->extension = g_strdup("conf");
	filetypes[GEANY_FILETYPES_CONF]->pattern = utils_strv_new("*.conf", "*.ini", "config", "*rc",
		"*.cfg", NULL);
	filetypes[GEANY_FILETYPES_CONF]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_CONF]->comment_close = NULL;

#define HAXE
	filetypes[GEANY_FILETYPES_HAXE]->lang = 27;
	filetypes[GEANY_FILETYPES_HAXE]->name = g_strdup("Haxe");
	filetypes[GEANY_FILETYPES_HAXE]->title = g_strdup_printf(_("%s source file"), "Haxe");
	filetypes[GEANY_FILETYPES_HAXE]->extension = g_strdup("hx");
	filetypes[GEANY_FILETYPES_HAXE]->pattern = utils_strv_new("*.hx", NULL);
	filetypes[GEANY_FILETYPES_HAXE]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_HAXE]->comment_close = NULL;

#define REST
	filetypes[GEANY_FILETYPES_REST]->lang = 28;
	filetypes[GEANY_FILETYPES_REST]->name = g_strdup("reStructuredText");
	filetypes[GEANY_FILETYPES_REST]->title = g_strdup(_("reStructuredText file"));
	filetypes[GEANY_FILETYPES_REST]->extension = g_strdup("rst");
	filetypes[GEANY_FILETYPES_REST]->pattern = utils_strv_new(
		"*.rest", "*.reST", "*.rst", NULL);
	filetypes[GEANY_FILETYPES_REST]->comment_open = NULL;
	filetypes[GEANY_FILETYPES_REST]->comment_close = NULL;

#define ALL
	filetypes[GEANY_FILETYPES_NONE]->name = g_strdup("None");
	filetypes[GEANY_FILETYPES_NONE]->lang = -2;
	filetypes[GEANY_FILETYPES_NONE]->title = g_strdup(_("All files"));
	filetypes[GEANY_FILETYPES_NONE]->extension = g_strdup("*");
	filetypes[GEANY_FILETYPES_NONE]->pattern = utils_strv_new("*", NULL);
	filetypes[GEANY_FILETYPES_NONE]->comment_open = NULL;
	filetypes[GEANY_FILETYPES_NONE]->comment_close = NULL;
}


/* initialize fields. */
static GeanyFiletype *filetype_new(void)
{
	FullFileType *fft = g_new0(FullFileType, 1);
	GeanyFiletype *ft = (GeanyFiletype*) fft;

	ft->lang = -2;	/* assume no tagmanager parser */
	ft->programs = g_new0(struct build_programs, 1);
	ft->actions = g_new0(struct build_actions, 1);
	return ft;
}


/* Add a filetype pointer to the list of available filetypes,
 * and set the filetype::id field. */
static void filetype_add(GeanyFiletype *ft)
{
	g_return_if_fail(ft);
	g_return_if_fail(ft->name);

	ft->id = filetypes_array->len;	/* len will be the index for filetype_array */
	g_ptr_array_add(filetypes_array, ft);
	g_hash_table_insert(filetypes_hash, ft->name, ft);
}


/* Create the filetypes array and fill it with the known filetypes. */
void filetypes_init_types()
{
	filetype_id ft_id;

	g_return_if_fail(filetypes_array == NULL);
	g_return_if_fail(filetypes_hash == NULL);

	filetypes_array = g_ptr_array_sized_new(GEANY_MAX_BUILT_IN_FILETYPES);
	filetypes_hash = g_hash_table_new(g_str_hash, g_str_equal);

	/* Create built-in filetypes */
	for (ft_id = 0; ft_id < GEANY_MAX_BUILT_IN_FILETYPES; ft_id++)
	{
		filetypes[ft_id] = filetype_new();
	}
	init_builtin_filetypes();

	/* Add built-in filetypes to the hash now the name fields are set */
	for (ft_id = 0; ft_id < GEANY_MAX_BUILT_IN_FILETYPES; ft_id++)
	{
		filetype_add(filetypes[ft_id]);
	}
}


#define create_sub_menu(menu, item, title) \
	(menu) = gtk_menu_new(); \
	(item) = gtk_menu_item_new_with_mnemonic((title)); \
	gtk_menu_item_set_submenu(GTK_MENU_ITEM((item)), (menu)); \
	gtk_container_add(GTK_CONTAINER(filetype_menu), (item)); \
	gtk_widget_show((item));


static void create_set_filetype_menu()
{
	filetype_id ft_id;
	GtkWidget *filetype_menu = lookup_widget(main_widgets.window, "set_filetype1_menu");
	GtkWidget *sub_menu = filetype_menu;
	GtkWidget *sub_menu_programming, *sub_menu_scripts, *sub_menu_markup, *sub_menu_misc;
	GtkWidget *sub_item_programming, *sub_item_scripts, *sub_item_markup, *sub_item_misc;

	create_sub_menu(sub_menu_programming, sub_item_programming, _("_Programming Languages"));
	create_sub_menu(sub_menu_scripts, sub_item_scripts, _("_Scripting Languages"));
	create_sub_menu(sub_menu_markup, sub_item_markup, _("_Markup Languages"));
	create_sub_menu(sub_menu_misc, sub_item_misc, _("M_iscellaneous Languages"));

	/* Append all filetypes to the filetype menu */
	for (ft_id = 0; ft_id < filetypes_array->len; ft_id++)
	{
		GeanyFiletype *ft = filetypes[ft_id];
		const gchar *title = ft->title;

		/* insert separators for different filetype groups */
		switch (ft_id)
		{
			case GEANY_FILETYPES_GROUP_COMPILED:	/* programming */
			{
				sub_menu = sub_menu_programming;
				break;
			}
			case GEANY_FILETYPES_GROUP_SCRIPT:	/* scripts */
			{
				sub_menu = sub_menu_scripts;
				break;
			}
			case GEANY_FILETYPES_GROUP_MARKUP:	/* markup */
			{	/* (include also CSS, not really markup but fit quite well to HTML) */
				sub_menu = sub_menu_markup;
				break;
			}
			case GEANY_FILETYPES_GROUP_MISC:	/* misc */
			{
				sub_menu = sub_menu_misc;
				break;
			}
			case GEANY_FILETYPES_NONE:	/* none */
			{
				sub_menu = filetype_menu;
				title = _("None");
				break;
			}
			default: break;
		}
		create_radio_menu_item(sub_menu, title, ft);
	}
}


void filetypes_init()
{
	filetypes_init_types();
	create_set_filetype_menu();
}


typedef gboolean FileTypesPredicate(GeanyFiletype *ft, gpointer user_data);

/* Find a filetype that predicate returns TRUE for, otherwise return NULL. */
static GeanyFiletype *filetypes_find(gboolean source_only,
		FileTypesPredicate predicate, gpointer user_data)
{
	guint i;

	for (i = 0; i < filetypes_array->len; i++)
	{
		GeanyFiletype *ft = filetypes[i];

		if (source_only && i == GEANY_FILETYPES_NONE)
			continue;	/* None is not for source files */

		if (predicate(ft, user_data))
			return ft;
	}
	return NULL;
}


static gboolean match_basename(GeanyFiletype *ft, gpointer user_data)
{
	const gchar *base_filename = user_data;
	gint j;
	gboolean ret = FALSE;

	for (j = 0; ft->pattern[j] != NULL; j++)
	{
		GPatternSpec *pattern = g_pattern_spec_new(ft->pattern[j]);

		if (g_pattern_match_string(pattern, base_filename))
		{
			ret = TRUE;
			g_pattern_spec_free(pattern);
			break;
		}
		g_pattern_spec_free(pattern);
	}
	return ret;
}


/* Detect filetype only based on the filename extension.
 * utf8_filename can include the full path. */
GeanyFiletype *filetypes_detect_from_extension(const gchar *utf8_filename)
{
	gchar *base_filename;
	GeanyFiletype *ft;

	/* to match against the basename of the file (because of Makefile*) */
	base_filename = g_path_get_basename(utf8_filename);
#ifdef G_OS_WIN32
	/* use lower case basename */
	setptr(base_filename, g_utf8_strdown(base_filename, -1));
#endif

	ft = filetypes_find(TRUE, match_basename, base_filename);
	if (ft == NULL)
		ft = filetypes[GEANY_FILETYPES_NONE];

	g_free(base_filename);
	return ft;
}


static GeanyFiletype *find_shebang(const gchar *utf8_filename, const gchar *line)
{
	GeanyFiletype *ft = NULL;

	if (strlen(line) > 2 && line[0] == '#' && line[1] == '!')
	{
		gchar *tmp = g_path_get_basename(line + 2);
		gchar *basename_interpreter = tmp;

		if (strncmp(tmp, "env ", 4) == 0 && strlen(tmp) > 4)
		{	/* skip "env" and read the following interpreter */
			basename_interpreter +=4;
		}

		if (strncmp(basename_interpreter, "sh", 2) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "bash", 4) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "perl", 4) == 0)
			ft = filetypes[GEANY_FILETYPES_PERL];
		else if (strncmp(basename_interpreter, "python", 6) == 0)
			ft = filetypes[GEANY_FILETYPES_PYTHON];
		else if (strncmp(basename_interpreter, "php", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_PHP];
		else if (strncmp(basename_interpreter, "ruby", 4) == 0)
			ft = filetypes[GEANY_FILETYPES_RUBY];
		else if (strncmp(basename_interpreter, "tcl", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_TCL];
		else if (strncmp(basename_interpreter, "make", 4) == 0)
			ft = filetypes[GEANY_FILETYPES_MAKE];
		else if (strncmp(basename_interpreter, "zsh", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "ksh", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "csh", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "ash", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "dmd", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_D];
		else if (strncmp(basename_interpreter, "wish", 4) == 0)
			ft = filetypes[GEANY_FILETYPES_TCL];

		g_free(tmp);
	}
	/* detect XML files */
	if (utf8_filename && strncmp(line, "<?xml", 5) == 0)
	{
		/* HTML and DocBook files might also start with <?xml, so detect them based on filename
		 * extension and use the detected filetype, else assume XML */
		ft = filetypes_detect_from_extension(utf8_filename);
		if (FILETYPE_ID(ft) != GEANY_FILETYPES_HTML &&
			FILETYPE_ID(ft) != GEANY_FILETYPES_DOCBOOK &&
			FILETYPE_ID(ft) != GEANY_FILETYPES_PERL &&	/* Perl, Python and PHP only to be safe */
			FILETYPE_ID(ft) != GEANY_FILETYPES_PHP &&
			FILETYPE_ID(ft) != GEANY_FILETYPES_PYTHON)
		{
			ft = filetypes[GEANY_FILETYPES_XML];
		}
	}
	else if (strncmp(line, "<?php", 5) == 0)
	{
		ft = filetypes[GEANY_FILETYPES_PHP];
	}

	return ft;
}


/* Detect the filetype checking for a shebang, then filename extension. */
static GeanyFiletype *filetypes_detect_from_file_internal(const gchar *utf8_filename,
														  const gchar *line)
{
	GeanyFiletype *ft;

	/* try to find a shebang and if found use it prior to the filename extension
	 * also checks for <?xml */
	ft = find_shebang(utf8_filename, line);
	if (ft != NULL)
		return ft;

	if (utf8_filename == NULL)
		return filetypes[GEANY_FILETYPES_NONE];

	return filetypes_detect_from_extension(utf8_filename);
}


/* Detect the filetype for document idx. */
GeanyFiletype *filetypes_detect_from_file(GeanyDocument *doc)
{
	GeanyFiletype *ft;
	gchar *line;

	if (doc == NULL)
		return filetypes[GEANY_FILETYPES_NONE];

	line = sci_get_line(doc->editor->scintilla, 0);
	ft = filetypes_detect_from_file_internal(doc->file_name, line);
	g_free(line);
	return ft;
}


/* Detect filetype based on the filename extension.
 * utf8_filename can include the full path. */
GeanyFiletype *filetypes_detect_from_filename(const gchar *utf8_filename)
{
	gchar line[1024];
	FILE *f;
	gchar *locale_name = utils_get_locale_from_utf8(utf8_filename);

	f = g_fopen(locale_name, "r");
	g_free(locale_name);
	if (f != NULL)
	{
		if (fgets(line, sizeof(line), f) != NULL)
		{
			fclose(f);
			return filetypes_detect_from_file_internal(utf8_filename, line);
		}
		fclose(f);
	}
	return filetypes_detect_from_extension(utf8_filename);
}


void filetypes_select_radio_item(const GeanyFiletype *ft)
{
	FullFileType *fft;

	/* ignore_callback has to be set by the caller */
	g_return_if_fail(ignore_callback);

	if (ft == NULL)
		ft = filetypes[GEANY_FILETYPES_NONE];

	fft = (FullFileType*)ft;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(fft->menu_item), TRUE);
}


static void
on_filetype_change                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GeanyDocument *doc = document_get_current();
	if (ignore_callback || doc == NULL)
		return;

	document_set_filetype(doc, (GeanyFiletype*)user_data);
}


static void create_radio_menu_item(GtkWidget *menu, const gchar *label, GeanyFiletype *ftype)
{
	static GSList *group = NULL;
	GtkWidget *tmp;
	FullFileType *fft = (FullFileType*)ftype;

	tmp = gtk_radio_menu_item_new_with_label(group, label);
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(tmp));
	fft->menu_item = tmp;
	gtk_widget_show(tmp);
	gtk_container_add(GTK_CONTAINER(menu), tmp);
	g_signal_connect((gpointer) tmp, "activate", G_CALLBACK(on_filetype_change), (gpointer) ftype);
}


#if 0
/* Remove a filetype pointer from the list of available filetypes. */
static void filetype_remove(GeanyFiletype *ft)
{
	g_return_if_fail(ft);

	g_ptr_array_remove(filetypes_array, ft);

	if (!g_hash_table_remove(filetypes_hash, ft))
		g_warning("Could not remove filetype %p!", ft);
}
#endif


static void filetype_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	GeanyFiletype *ft = data;

	g_return_if_fail(ft != NULL);

	g_free(ft->name);
	g_free(ft->title);
	g_free(ft->extension);
	g_free(ft->comment_open);
	g_free(ft->comment_close);
	g_free(ft->context_action_cmd);
	g_free(ft->programs->compiler);
	g_free(ft->programs->linker);
	g_free(ft->programs->run_cmd);
	g_free(ft->programs->run_cmd2);
	g_free(ft->programs);
	g_free(ft->actions);

	g_strfreev(ft->pattern);
	g_free(ft);
}


/* frees the array and all related pointers */
void filetypes_free_types()
{
	g_return_if_fail(filetypes_array != NULL);
	g_return_if_fail(filetypes_hash != NULL);

	g_ptr_array_foreach(filetypes_array, filetype_free, NULL);
	g_ptr_array_free(filetypes_array, TRUE);
	g_hash_table_destroy(filetypes_hash);
}


static void load_settings(gint ft_id, GKeyFile *config, GKeyFile *configh)
{
	gchar *result;
	GError *error = NULL;
	gboolean tmp;

	/* default extension */
	result = g_key_file_get_string(configh, "settings", "extension", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "extension", NULL);
	if (result != NULL)
	{
		setptr(filetypes[ft_id]->extension, result);
	}

	/* read comment notes */
	result = g_key_file_get_string(configh, "settings", "comment_open", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "comment_open", NULL);
	if (result != NULL)
	{
		g_free(filetypes[ft_id]->comment_open);
		filetypes[ft_id]->comment_open = result;
	}

	result = g_key_file_get_string(configh, "settings", "comment_close", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "comment_close", NULL);
	if (result != NULL)
	{
		g_free(filetypes[ft_id]->comment_close);
		filetypes[ft_id]->comment_close = result;
	}

	tmp = g_key_file_get_boolean(configh, "settings", "comment_use_indent", &error);
	if (error)
	{
		g_error_free(error);
		error = NULL;
		tmp = g_key_file_get_boolean(config, "settings", "comment_use_indent", &error);
		if (error) g_error_free(error);
		else filetypes[ft_id]->comment_use_indent = tmp;
	}
	else filetypes[ft_id]->comment_use_indent = tmp;

	/* read context action */
	result = g_key_file_get_string(configh, "settings", "context_action_cmd", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "context_action_cmd", NULL);
	if (result != NULL)
	{
		filetypes[ft_id]->context_action_cmd = result;
	}

	/* read build settings */
	result = g_key_file_get_string(configh, "build_settings", "compiler", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "compiler", NULL);
	if (result != NULL)
	{
		filetypes[ft_id]->programs->compiler = result;
		filetypes[ft_id]->actions->can_compile = TRUE;
	}

	result = g_key_file_get_string(configh, "build_settings", "linker", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "linker", NULL);
	if (result != NULL)
	{
		filetypes[ft_id]->programs->linker = result;
		filetypes[ft_id]->actions->can_link = TRUE;
	}

	result = g_key_file_get_string(configh, "build_settings", "run_cmd", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "run_cmd", NULL);
	if (result != NULL)
	{
		filetypes[ft_id]->programs->run_cmd = result;
		filetypes[ft_id]->actions->can_exec = TRUE;
	}

	result = g_key_file_get_string(configh, "build_settings", "run_cmd2", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "run_cmd2", NULL);
	if (result != NULL)
	{
		filetypes[ft_id]->programs->run_cmd2 = result;
		filetypes[ft_id]->actions->can_exec = TRUE;
	}
}


/* simple wrapper function to print file errors in DEBUG mode */
static void load_system_keyfile(GKeyFile *key_file, const gchar *file, GKeyFileFlags flags,
								G_GNUC_UNUSED GError **just_for_compatibility)
{
	GError *error = NULL;
	gboolean done = g_key_file_load_from_file(key_file, file, flags, &error);
	if (! done && error != NULL)
	{
		geany_debug("Failed to open %s (%s)", file, error->message);
		g_error_free(error);
		error = NULL;
	}
}


/* Load the configuration file for the associated filetype id.
 * This should only be called when the filetype is needed, to save loading
 * 20+ configuration files all at once. */
void filetypes_load_config(gint ft_id, gboolean reload)
{
	GKeyFile *config, *config_home;
	FullFileType *fft = (FullFileType*)filetypes[ft_id];

	g_return_if_fail(ft_id >= 0 && ft_id < (gint) filetypes_array->len);

	/* when reloading, proceed only if the settings were already loaded */
	if (reload && ! fft->keyfile_loaded)
		return;

	/* when not reloading, load the settings only once */
	if (! reload && fft->keyfile_loaded)
		return;
	fft->keyfile_loaded = TRUE;

	config = g_key_file_new();
	config_home = g_key_file_new();
	{
		/* highlighting uses GEANY_FILETYPES_NONE for common settings */
		gchar *ext = (ft_id != GEANY_FILETYPES_NONE) ?
			filetypes_get_conf_extension(ft_id) : g_strdup("common");
		gchar *f0 = g_strconcat(app->datadir, G_DIR_SEPARATOR_S "filetypes.", ext, NULL);
		gchar *f = g_strconcat(app->configdir,
			G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.", ext, NULL);

		load_system_keyfile(config, f0, G_KEY_FILE_KEEP_COMMENTS, NULL);
		g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

		g_free(ext);
		g_free(f);
		g_free(f0);
	}

	load_settings(ft_id, config, config_home);
	if (! reload)
		/* reloading highlighting settings not yet supported */
		highlighting_init_styles(ft_id, config, config_home);

	g_key_file_free(config);
	g_key_file_free(config_home);
}


gchar *filetypes_get_conf_extension(gint filetype_idx)
{
	gchar *result;

	/* Handle any special extensions different from lowercase filetype->name */
	switch (filetype_idx)
	{
		case GEANY_FILETYPES_CPP: result = g_strdup("cpp"); break;
		case GEANY_FILETYPES_CS: result = g_strdup("cs"); break;
		case GEANY_FILETYPES_MAKE: result = g_strdup("makefile"); break;
		case GEANY_FILETYPES_OMS: result = g_strdup("oms"); break;
		default: result = g_ascii_strdown(filetypes[filetype_idx]->name, -1); break;
	}
	return result;
}


void filetypes_save_commands()
{
	gchar *conf_prefix = g_strconcat(app->configdir,
		G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.", NULL);
	gint i;

	for (i = 0; i < GEANY_FILETYPES_NONE; i++)
	{
		struct build_programs *bp = filetypes[i]->programs;
		GKeyFile *config_home;
		gchar *fname, *ext, *data;

		if (! bp->modified) continue;

		ext = filetypes_get_conf_extension(i);
		fname = g_strconcat(conf_prefix, ext, NULL);
		g_free(ext);

		config_home = g_key_file_new();
		g_key_file_load_from_file(config_home, fname, G_KEY_FILE_KEEP_COMMENTS, NULL);

		if (bp->compiler && *bp->compiler)
			g_key_file_set_string(config_home, "build_settings", "compiler", bp->compiler);
		if (bp->linker && *bp->linker)
			g_key_file_set_string(config_home, "build_settings", "linker", bp->linker);
		if (bp->run_cmd && *bp->run_cmd)
			g_key_file_set_string(config_home, "build_settings", "run_cmd", bp->run_cmd);
		if (bp->run_cmd2 && *bp->run_cmd2)
			g_key_file_set_string(config_home, "build_settings", "run_cmd2", bp->run_cmd2);

		data = g_key_file_to_data(config_home, NULL, NULL);
		utils_write_file(fname, data);
		g_free(data);
		g_key_file_free(config_home);
		g_free(fname);
	}
	g_free(conf_prefix);
}


/* create one file filter which has each file pattern of each filetype */
GtkFileFilter *filetypes_create_file_filter_all_source()
{
	GtkFileFilter *new_filter;
	guint i, j;

	new_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(new_filter, _("All Source"));

	for (i = 0; i < filetypes_array->len; i++)
	{
		if (i == GEANY_FILETYPES_NONE)
			continue;

		for (j = 0; filetypes[i]->pattern[j]; j++)
		{
			gtk_file_filter_add_pattern(new_filter, filetypes[i]->pattern[j]);
		}
	}
	return new_filter;
}


GtkFileFilter *filetypes_create_file_filter(const GeanyFiletype *ft)
{
	GtkFileFilter *new_filter;
	gint i;

	g_return_val_if_fail(ft != NULL, NULL);

	new_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(new_filter, ft->title);

	for (i = 0; ft->pattern[i]; i++)
	{
		gtk_file_filter_add_pattern(new_filter, ft->pattern[i]);
	}

	return new_filter;
}


/* Indicates whether there is a tag parser for the filetype or not. */
gboolean filetype_has_tags(GeanyFiletype *ft)
{
	g_return_val_if_fail(ft != NULL, FALSE);

	return ft->lang >= 0;
}


/** Find a filetype pointer from its @c name field. */
GeanyFiletype *filetypes_lookup_by_name(const gchar *name)
{
	GeanyFiletype *ft;

	g_return_val_if_fail(NZV(name), NULL);

	ft = g_hash_table_lookup(filetypes_hash, name);
	if (ft == NULL)
		geany_debug("Could not find filetype '%s'.", name);
	return ft;
}


