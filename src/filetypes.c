/*
 *      filetypes.c - this file is part of Geany, a fast and lightweight IDE
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
 * Filetype detection, file extensions and filetype menu items.
 */

#include <string.h>

#include "geany.h"
#include "filetypes.h"
#include "highlighting.h"
#include "support.h"
#include "templates.h"
#include "msgwindow.h"
#include "utils.h"
#include "document.h"
#include "sciwrappers.h"


/* This is the order of unique ids used in the config file.
 * The order must not be changed but can be appended to. */
enum
{
	FILETYPE_UID_C = 0,		// 0
	FILETYPE_UID_CPP,		// 1
	FILETYPE_UID_JAVA,		// 2
	FILETYPE_UID_PERL,		// 3
	FILETYPE_UID_PHP,		// 4
	FILETYPE_UID_XML,		// 5
	FILETYPE_UID_DOCBOOK,	// 6
	FILETYPE_UID_PYTHON,	// 7
	FILETYPE_UID_LATEX,		// 8
	FILETYPE_UID_PASCAL,	// 9
	FILETYPE_UID_SH,		// 10
	FILETYPE_UID_MAKE,		// 11
	FILETYPE_UID_CSS,		// 12
	FILETYPE_UID_CONF,		// 13
	FILETYPE_UID_ASM,		// 14
	FILETYPE_UID_SQL,		// 15
	FILETYPE_UID_CAML,		// 16
	FILETYPE_UID_OMS,		// 17
	FILETYPE_UID_RUBY,		// 18
	FILETYPE_UID_TCL,		// 19
	FILETYPE_UID_ALL,		// 20
	FILETYPE_UID_D,			// 21
	FILETYPE_UID_FORTRAN,	// 22
	FILETYPE_UID_DIFF,		// 23
	FILETYPE_UID_FERITE,	// 24
	FILETYPE_UID_HTML,		// 25
	FILETYPE_UID_VHDL,		// 26
	FILETYPE_UID_JS,		// 27
	FILETYPE_UID_LUA		// 28
};


static GtkWidget *radio_items[GEANY_MAX_FILE_TYPES];

static void filetypes_create_menu_item(GtkWidget *menu, const gchar *label, filetype *ftype);


// If uid is valid, return corresponding filetype, otherwise NULL.
filetype *filetypes_get_from_uid(gint uid)
{
	switch (uid)
	{
		case FILETYPE_UID_C:		return filetypes[GEANY_FILETYPES_C];
		case FILETYPE_UID_CPP:		return filetypes[GEANY_FILETYPES_CPP];
		case FILETYPE_UID_JAVA:		return filetypes[GEANY_FILETYPES_JAVA];
		case FILETYPE_UID_PERL:		return filetypes[GEANY_FILETYPES_PERL];
		case FILETYPE_UID_PHP:		return filetypes[GEANY_FILETYPES_PHP];
		case FILETYPE_UID_XML:		return filetypes[GEANY_FILETYPES_XML];
		case FILETYPE_UID_DOCBOOK:	return filetypes[GEANY_FILETYPES_DOCBOOK];
		case FILETYPE_UID_PYTHON:	return filetypes[GEANY_FILETYPES_PYTHON];
		case FILETYPE_UID_LATEX:	return filetypes[GEANY_FILETYPES_LATEX];
		case FILETYPE_UID_PASCAL:	return filetypes[GEANY_FILETYPES_PASCAL];
		case FILETYPE_UID_SH:		return filetypes[GEANY_FILETYPES_SH];
		case FILETYPE_UID_MAKE:		return filetypes[GEANY_FILETYPES_MAKE];
		case FILETYPE_UID_CSS:		return filetypes[GEANY_FILETYPES_CSS];
		case FILETYPE_UID_CONF:		return filetypes[GEANY_FILETYPES_CONF];
		case FILETYPE_UID_ASM:		return filetypes[GEANY_FILETYPES_ASM];
		case FILETYPE_UID_SQL:		return filetypes[GEANY_FILETYPES_SQL];
		case FILETYPE_UID_CAML:		return filetypes[GEANY_FILETYPES_CAML];
		case FILETYPE_UID_OMS:		return filetypes[GEANY_FILETYPES_OMS];
		case FILETYPE_UID_RUBY:		return filetypes[GEANY_FILETYPES_RUBY];
		case FILETYPE_UID_TCL:		return filetypes[GEANY_FILETYPES_TCL];
		case FILETYPE_UID_ALL:		return filetypes[GEANY_FILETYPES_ALL];
		case FILETYPE_UID_D:		return filetypes[GEANY_FILETYPES_D];
		case FILETYPE_UID_FORTRAN:	return filetypes[GEANY_FILETYPES_FORTRAN];
		case FILETYPE_UID_DIFF:		return filetypes[GEANY_FILETYPES_DIFF];
		case FILETYPE_UID_FERITE:	return filetypes[GEANY_FILETYPES_FERITE];
		case FILETYPE_UID_HTML:		return filetypes[GEANY_FILETYPES_HTML];
		case FILETYPE_UID_VHDL:		return filetypes[GEANY_FILETYPES_VHDL];
		case FILETYPE_UID_JS:		return filetypes[GEANY_FILETYPES_JS];
		case FILETYPE_UID_LUA:		return filetypes[GEANY_FILETYPES_LUA];
		default: 					return NULL;
	}
}


/* Create the filetype array and fill it with the known filetypes. */
void filetypes_init_types()
{
	filetype_id ft_id;

	for (ft_id = 0; ft_id < GEANY_MAX_FILE_TYPES; ft_id++)
	{
		filetypes[ft_id] = g_new0(filetype, 1);
		filetypes[ft_id]->programs = g_new0(struct build_programs, 1);
		filetypes[ft_id]->actions = g_new0(struct build_actions, 1);
	}

#define C	// these macros are only to ease navigation
	filetypes[GEANY_FILETYPES_C]->id = GEANY_FILETYPES_C;
	filetypes[GEANY_FILETYPES_C]->uid = FILETYPE_UID_C;
	filetypes[GEANY_FILETYPES_C]->lang = 0;
	filetypes[GEANY_FILETYPES_C]->name = g_strdup("C");
	filetypes[GEANY_FILETYPES_C]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_C]->title = g_strdup(_("C source file"));
	filetypes[GEANY_FILETYPES_C]->extension = g_strdup("c");
	filetypes[GEANY_FILETYPES_C]->pattern = utils_strv_new("*.c", "*.h", NULL);
	filetypes[GEANY_FILETYPES_C]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_C]->comment_close = g_strdup("*/");
	filetypes[GEANY_FILETYPES_C]->style_func_ptr = styleset_c;

#define CPP
	filetypes[GEANY_FILETYPES_CPP]->id = GEANY_FILETYPES_CPP;
	filetypes[GEANY_FILETYPES_CPP]->uid = FILETYPE_UID_CPP;
	filetypes[GEANY_FILETYPES_CPP]->lang = 1;
	filetypes[GEANY_FILETYPES_CPP]->name = g_strdup("C++");
	filetypes[GEANY_FILETYPES_CPP]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CPP]->title = g_strdup(_("C++ source file"));
	filetypes[GEANY_FILETYPES_CPP]->extension = g_strdup("cpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern = utils_strv_new("*.cpp", "*.cxx", "*.c++", "*.cc",
		"*.h", "*.hpp", "*.hxx", "*.h++", "*.hh", "*.C", NULL);
	filetypes[GEANY_FILETYPES_CPP]->style_func_ptr = styleset_cpp;
	filetypes[GEANY_FILETYPES_CPP]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_CPP]->comment_close = NULL;

#define D
	filetypes[GEANY_FILETYPES_D]->id = GEANY_FILETYPES_D;
	filetypes[GEANY_FILETYPES_D]->uid = FILETYPE_UID_D;
	filetypes[GEANY_FILETYPES_D]->lang = 17;
	filetypes[GEANY_FILETYPES_D]->name = g_strdup("D");
	filetypes[GEANY_FILETYPES_D]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_D]->title = g_strdup(_("D source file"));
	filetypes[GEANY_FILETYPES_D]->extension = g_strdup("d");
	filetypes[GEANY_FILETYPES_D]->pattern = utils_strv_new("*.d", "*.di", NULL);
	filetypes[GEANY_FILETYPES_D]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_D]->comment_close = NULL;
	filetypes[GEANY_FILETYPES_D]->style_func_ptr = styleset_d;

#define JAVA
	filetypes[GEANY_FILETYPES_JAVA]->id = GEANY_FILETYPES_JAVA;
	filetypes[GEANY_FILETYPES_JAVA]->name = g_strdup("Java");
	filetypes[GEANY_FILETYPES_JAVA]->uid = FILETYPE_UID_JAVA;
	filetypes[GEANY_FILETYPES_JAVA]->lang = 2;
	filetypes[GEANY_FILETYPES_JAVA]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_JAVA]->title = g_strdup(_("Java source file"));
	filetypes[GEANY_FILETYPES_JAVA]->extension = g_strdup("java");
	filetypes[GEANY_FILETYPES_JAVA]->pattern = utils_strv_new("*.java", "*.jsp", NULL);
	filetypes[GEANY_FILETYPES_JAVA]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_JAVA]->comment_close = g_strdup("*/");
	filetypes[GEANY_FILETYPES_JAVA]->style_func_ptr = styleset_java;

#define PAS // to avoid warnings when building under Windows, the symbol PASCAL is there defined
	filetypes[GEANY_FILETYPES_PASCAL]->id = GEANY_FILETYPES_PASCAL;
	filetypes[GEANY_FILETYPES_PASCAL]->uid = FILETYPE_UID_PASCAL;
	filetypes[GEANY_FILETYPES_PASCAL]->lang = 4;
	filetypes[GEANY_FILETYPES_PASCAL]->name = g_strdup("Pascal");
	filetypes[GEANY_FILETYPES_PASCAL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PASCAL]->title = g_strdup(_("Pascal source file"));
	filetypes[GEANY_FILETYPES_PASCAL]->extension = g_strdup("pas");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern = utils_strv_new("*.pas", "*.pp", "*.inc", "*.dpr",
		"*.dpk", NULL);
	filetypes[GEANY_FILETYPES_PASCAL]->style_func_ptr = styleset_pascal;
	filetypes[GEANY_FILETYPES_PASCAL]->comment_open = g_strdup("{");
	filetypes[GEANY_FILETYPES_PASCAL]->comment_close = g_strdup("}");

#define ASM
	filetypes[GEANY_FILETYPES_ASM]->id = GEANY_FILETYPES_ASM;
	filetypes[GEANY_FILETYPES_ASM]->uid = FILETYPE_UID_ASM;
	filetypes[GEANY_FILETYPES_ASM]->lang = 9;
	filetypes[GEANY_FILETYPES_ASM]->name = g_strdup("ASM");
	filetypes[GEANY_FILETYPES_ASM]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_ASM]->title = g_strdup(_("Assembler source file"));
	filetypes[GEANY_FILETYPES_ASM]->extension = g_strdup("asm");
	filetypes[GEANY_FILETYPES_ASM]->pattern = utils_strv_new("*.asm", NULL);
	filetypes[GEANY_FILETYPES_ASM]->style_func_ptr = styleset_asm;
	filetypes[GEANY_FILETYPES_ASM]->comment_open = g_strdup(";");
	filetypes[GEANY_FILETYPES_ASM]->comment_close = NULL;

#define FORTRAN
	filetypes[GEANY_FILETYPES_FORTRAN]->id = GEANY_FILETYPES_FORTRAN;
	filetypes[GEANY_FILETYPES_FORTRAN]->uid = FILETYPE_UID_FORTRAN;
	filetypes[GEANY_FILETYPES_FORTRAN]->lang = 18;
	filetypes[GEANY_FILETYPES_FORTRAN]->name = g_strdup("Fortran");
	filetypes[GEANY_FILETYPES_FORTRAN]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_FORTRAN]->title = g_strdup(_("Fortran source file (F77)"));
	filetypes[GEANY_FILETYPES_FORTRAN]->extension = g_strdup("f");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern = utils_strv_new("*.f", "*.for", "*.ftn", "*.f77",
		"*.f90", "*.f95", NULL);
	filetypes[GEANY_FILETYPES_FORTRAN]->style_func_ptr = styleset_fortran;
	filetypes[GEANY_FILETYPES_FORTRAN]->comment_open = g_strdup("c");
	filetypes[GEANY_FILETYPES_FORTRAN]->comment_close = NULL;

#define CAML
	filetypes[GEANY_FILETYPES_CAML]->id = GEANY_FILETYPES_CAML;
	filetypes[GEANY_FILETYPES_CAML]->uid = FILETYPE_UID_CAML;
	filetypes[GEANY_FILETYPES_CAML]->lang = -2;
	filetypes[GEANY_FILETYPES_CAML]->name = g_strdup("CAML");
	filetypes[GEANY_FILETYPES_CAML]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_CAML]->title = g_strdup(_("(O)Caml source file"));
	filetypes[GEANY_FILETYPES_CAML]->extension = g_strdup("ml");
	filetypes[GEANY_FILETYPES_CAML]->pattern = utils_strv_new("*.ml", "*.mli", NULL);
	filetypes[GEANY_FILETYPES_CAML]->style_func_ptr = styleset_caml;
	filetypes[GEANY_FILETYPES_CAML]->comment_open = g_strdup("(*");
	filetypes[GEANY_FILETYPES_CAML]->comment_close = g_strdup("*)");

#define PERL
	filetypes[GEANY_FILETYPES_PERL]->id = GEANY_FILETYPES_PERL;
	filetypes[GEANY_FILETYPES_PERL]->uid = FILETYPE_UID_PERL;
	filetypes[GEANY_FILETYPES_PERL]->lang = 5;
	filetypes[GEANY_FILETYPES_PERL]->name = g_strdup("Perl");
	filetypes[GEANY_FILETYPES_PERL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PERL]->title = g_strdup(_("Perl source file"));
	filetypes[GEANY_FILETYPES_PERL]->extension = g_strdup("perl");
	filetypes[GEANY_FILETYPES_PERL]->pattern = utils_strv_new("*.pl", "*.perl", "*.pm", "*.agi",
		"*.pod", NULL);
	filetypes[GEANY_FILETYPES_PERL]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_PERL]->comment_close = NULL;
	filetypes[GEANY_FILETYPES_PERL]->style_func_ptr = styleset_perl;

#define PHP
	filetypes[GEANY_FILETYPES_PHP]->id = GEANY_FILETYPES_PHP;
	filetypes[GEANY_FILETYPES_PHP]->uid = FILETYPE_UID_PHP;
	filetypes[GEANY_FILETYPES_PHP]->lang = 6;
	filetypes[GEANY_FILETYPES_PHP]->name = g_strdup("PHP");
	filetypes[GEANY_FILETYPES_PHP]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PHP]->title = g_strdup(_("PHP source file"));
	filetypes[GEANY_FILETYPES_PHP]->extension = g_strdup("php");
	filetypes[GEANY_FILETYPES_PHP]->pattern = utils_strv_new("*.php", "*.php3", "*.php4", "*.php5",
		"*.phtml", NULL);
	filetypes[GEANY_FILETYPES_PHP]->style_func_ptr = styleset_php;
	filetypes[GEANY_FILETYPES_PHP]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_PHP]->comment_close = NULL;

#define JAVASCRIPT
	filetypes[GEANY_FILETYPES_JS]->id = GEANY_FILETYPES_JS;
	filetypes[GEANY_FILETYPES_JS]->uid = FILETYPE_UID_JS;
	filetypes[GEANY_FILETYPES_JS]->lang = 23;
	filetypes[GEANY_FILETYPES_JS]->name = g_strdup("Javascript");
	filetypes[GEANY_FILETYPES_JS]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_JS]->title = g_strdup(_("Javascript source file"));
	filetypes[GEANY_FILETYPES_JS]->extension = g_strdup("js");
	filetypes[GEANY_FILETYPES_JS]->pattern = utils_strv_new("*.js", NULL);
	filetypes[GEANY_FILETYPES_JS]->style_func_ptr = styleset_js;
	filetypes[GEANY_FILETYPES_JS]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_JS]->comment_close = NULL;

#define PYTHON
	filetypes[GEANY_FILETYPES_PYTHON]->id = GEANY_FILETYPES_PYTHON;
	filetypes[GEANY_FILETYPES_PYTHON]->uid = FILETYPE_UID_PYTHON;
	filetypes[GEANY_FILETYPES_PYTHON]->lang = 7;
	filetypes[GEANY_FILETYPES_PYTHON]->name = g_strdup("Python");
	filetypes[GEANY_FILETYPES_PYTHON]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PYTHON]->title = g_strdup(_("Python source file"));
	filetypes[GEANY_FILETYPES_PYTHON]->extension = g_strdup("py");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern = utils_strv_new("*.py", "*.pyw", NULL);
	filetypes[GEANY_FILETYPES_PYTHON]->style_func_ptr = styleset_python;
	filetypes[GEANY_FILETYPES_PYTHON]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_PYTHON]->comment_close = NULL;

#define RUBY
	filetypes[GEANY_FILETYPES_RUBY]->id = GEANY_FILETYPES_RUBY;
	filetypes[GEANY_FILETYPES_RUBY]->uid = FILETYPE_UID_RUBY;
	filetypes[GEANY_FILETYPES_RUBY]->lang = 14;
	filetypes[GEANY_FILETYPES_RUBY]->name = g_strdup("Ruby");
	filetypes[GEANY_FILETYPES_RUBY]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_RUBY]->title = g_strdup(_("Ruby source file"));
	filetypes[GEANY_FILETYPES_RUBY]->extension = g_strdup("rb");
	filetypes[GEANY_FILETYPES_RUBY]->pattern = utils_strv_new("*.rb", "*.rhtml", NULL);
	filetypes[GEANY_FILETYPES_RUBY]->style_func_ptr = styleset_ruby;
	filetypes[GEANY_FILETYPES_RUBY]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_RUBY]->comment_close = NULL;

#define TCL
	filetypes[GEANY_FILETYPES_TCL]->id = GEANY_FILETYPES_TCL;
	filetypes[GEANY_FILETYPES_TCL]->uid = FILETYPE_UID_TCL;
	filetypes[GEANY_FILETYPES_TCL]->lang = 15;
	filetypes[GEANY_FILETYPES_TCL]->name = g_strdup("Tcl");
	filetypes[GEANY_FILETYPES_TCL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_TCL]->title = g_strdup(_("Tcl source file"));
	filetypes[GEANY_FILETYPES_TCL]->extension = g_strdup("tcl");
	filetypes[GEANY_FILETYPES_TCL]->pattern = utils_strv_new("*.tcl", "*.tk", "*.wish", NULL);
	filetypes[GEANY_FILETYPES_TCL]->style_func_ptr = styleset_tcl;
	filetypes[GEANY_FILETYPES_TCL]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_TCL]->comment_close = NULL;

#define LUA
	filetypes[GEANY_FILETYPES_LUA]->id = GEANY_FILETYPES_LUA;
	filetypes[GEANY_FILETYPES_LUA]->uid = FILETYPE_UID_LUA;
	filetypes[GEANY_FILETYPES_LUA]->lang = 22;
	filetypes[GEANY_FILETYPES_LUA]->name = g_strdup("Lua");
	filetypes[GEANY_FILETYPES_LUA]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_LUA]->title = g_strdup(_("Lua source file"));
	filetypes[GEANY_FILETYPES_LUA]->extension = g_strdup("lua");
	filetypes[GEANY_FILETYPES_LUA]->pattern = utils_strv_new("*.lua", NULL);
	filetypes[GEANY_FILETYPES_LUA]->style_func_ptr = styleset_lua;
	filetypes[GEANY_FILETYPES_LUA]->comment_open = g_strdup("--");
	filetypes[GEANY_FILETYPES_LUA]->comment_close = NULL;

#define FERITE
	filetypes[GEANY_FILETYPES_FERITE]->id = GEANY_FILETYPES_FERITE;
	filetypes[GEANY_FILETYPES_FERITE]->uid = FILETYPE_UID_FERITE;
	filetypes[GEANY_FILETYPES_FERITE]->lang = 19;
	filetypes[GEANY_FILETYPES_FERITE]->name = g_strdup("Ferite");
	filetypes[GEANY_FILETYPES_FERITE]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_FERITE]->title = g_strdup(_("Ferite source file"));
	filetypes[GEANY_FILETYPES_FERITE]->extension = g_strdup("fe");
	filetypes[GEANY_FILETYPES_FERITE]->pattern = utils_strv_new("*.fe", NULL);
	filetypes[GEANY_FILETYPES_FERITE]->style_func_ptr = styleset_ferite;
	filetypes[GEANY_FILETYPES_FERITE]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_FERITE]->comment_close = g_strdup("*/");

#define SH
	filetypes[GEANY_FILETYPES_SH]->id = GEANY_FILETYPES_SH;
	filetypes[GEANY_FILETYPES_SH]->uid = FILETYPE_UID_SH;
	filetypes[GEANY_FILETYPES_SH]->lang = 16;
	filetypes[GEANY_FILETYPES_SH]->name = g_strdup("Sh");
	filetypes[GEANY_FILETYPES_SH]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_SH]->title = g_strdup(_("Shell script file"));
	filetypes[GEANY_FILETYPES_SH]->extension = g_strdup("sh");
	filetypes[GEANY_FILETYPES_SH]->pattern = utils_strv_new("*.sh", "configure", "configure.in",
		"configure.in.in", "configure.ac", "*.ksh", "*.zsh", NULL);
	filetypes[GEANY_FILETYPES_SH]->style_func_ptr = styleset_sh;
	filetypes[GEANY_FILETYPES_SH]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_SH]->comment_close = NULL;

#define MAKE
	filetypes[GEANY_FILETYPES_MAKE]->id = GEANY_FILETYPES_MAKE;
	filetypes[GEANY_FILETYPES_MAKE]->uid = FILETYPE_UID_MAKE;
	filetypes[GEANY_FILETYPES_MAKE]->lang = 3;
	filetypes[GEANY_FILETYPES_MAKE]->name = g_strdup("Make");
	filetypes[GEANY_FILETYPES_MAKE]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_MAKE]->title = g_strdup(_("Makefile"));
	filetypes[GEANY_FILETYPES_MAKE]->extension = g_strdup("mak");
	filetypes[GEANY_FILETYPES_MAKE]->pattern = utils_strv_new(
		"*.mak", "*.mk", "GNUmakefile", "makefile", "Makefile", "makefile.*", "Makefile.*", NULL);
	filetypes[GEANY_FILETYPES_MAKE]->style_func_ptr = styleset_makefile;
	filetypes[GEANY_FILETYPES_MAKE]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_MAKE]->comment_close = NULL;

#define XML
	filetypes[GEANY_FILETYPES_XML]->id = GEANY_FILETYPES_XML;
	filetypes[GEANY_FILETYPES_XML]->uid = FILETYPE_UID_XML;
	filetypes[GEANY_FILETYPES_XML]->lang = -2;
	filetypes[GEANY_FILETYPES_XML]->name = g_strdup("XML");
	filetypes[GEANY_FILETYPES_XML]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_XML]->title = g_strdup(_("XML source file"));
	filetypes[GEANY_FILETYPES_XML]->extension = g_strdup("xml");
	filetypes[GEANY_FILETYPES_XML]->pattern = utils_strv_new(
		"*.xml", "*.sgml", "*.xsl", "*.xslt", "*.xsd", NULL);
	filetypes[GEANY_FILETYPES_XML]->style_func_ptr = styleset_xml;
	filetypes[GEANY_FILETYPES_XML]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_XML]->comment_close = g_strdup("-->");

#define DOCBOOK
	filetypes[GEANY_FILETYPES_DOCBOOK]->id = GEANY_FILETYPES_DOCBOOK;
	filetypes[GEANY_FILETYPES_DOCBOOK]->uid = FILETYPE_UID_DOCBOOK;
	filetypes[GEANY_FILETYPES_DOCBOOK]->lang = 12;
	filetypes[GEANY_FILETYPES_DOCBOOK]->name = g_strdup("Docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_DOCBOOK]->title = g_strdup(_("Docbook source file"));
	filetypes[GEANY_FILETYPES_DOCBOOK]->extension = g_strdup("docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern = utils_strv_new("*.docbook", NULL);
	filetypes[GEANY_FILETYPES_DOCBOOK]->style_func_ptr = styleset_docbook;
	filetypes[GEANY_FILETYPES_DOCBOOK]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_DOCBOOK]->comment_close = g_strdup("-->");

#define HTML
	filetypes[GEANY_FILETYPES_HTML]->id = GEANY_FILETYPES_HTML;
	filetypes[GEANY_FILETYPES_HTML]->uid = FILETYPE_UID_HTML;
	filetypes[GEANY_FILETYPES_HTML]->lang = -2;
	filetypes[GEANY_FILETYPES_HTML]->name = g_strdup("HTML");
	filetypes[GEANY_FILETYPES_HTML]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_HTML]->title = g_strdup(_("HTML source file"));
	filetypes[GEANY_FILETYPES_HTML]->extension = g_strdup("html");
	filetypes[GEANY_FILETYPES_HTML]->pattern = utils_strv_new(
		"*.htm", "*.html", "*.shtml", "*.hta", "*.htd", "*.htt", "*.cfm", NULL);
	filetypes[GEANY_FILETYPES_HTML]->style_func_ptr = styleset_html;
	filetypes[GEANY_FILETYPES_HTML]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_HTML]->comment_close = g_strdup("-->");

#define CSS
	filetypes[GEANY_FILETYPES_CSS]->id = GEANY_FILETYPES_CSS;
	filetypes[GEANY_FILETYPES_CSS]->uid = FILETYPE_UID_CSS;
	filetypes[GEANY_FILETYPES_CSS]->lang = 13;
	filetypes[GEANY_FILETYPES_CSS]->name = g_strdup("CSS");
	filetypes[GEANY_FILETYPES_CSS]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CSS]->title = g_strdup(_("Cascading StyleSheet"));
	filetypes[GEANY_FILETYPES_CSS]->extension = g_strdup("css");
	filetypes[GEANY_FILETYPES_CSS]->pattern = utils_strv_new("*.css", NULL);
	filetypes[GEANY_FILETYPES_CSS]->style_func_ptr = styleset_css;
	filetypes[GEANY_FILETYPES_CSS]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_CSS]->comment_close = g_strdup("*/");

#define SQL
	filetypes[GEANY_FILETYPES_SQL]->id = GEANY_FILETYPES_SQL;
	filetypes[GEANY_FILETYPES_SQL]->uid = FILETYPE_UID_SQL;
	filetypes[GEANY_FILETYPES_SQL]->lang = 11;
	filetypes[GEANY_FILETYPES_SQL]->name = g_strdup("SQL");
	filetypes[GEANY_FILETYPES_SQL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_SQL]->title = g_strdup(_("SQL Dump file"));
	filetypes[GEANY_FILETYPES_SQL]->extension = g_strdup("sql");
	filetypes[GEANY_FILETYPES_SQL]->pattern = utils_strv_new("*.sql", NULL);
	filetypes[GEANY_FILETYPES_SQL]->style_func_ptr = styleset_sql;
	filetypes[GEANY_FILETYPES_SQL]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_SQL]->comment_close = g_strdup("*/");

#define LATEX
	filetypes[GEANY_FILETYPES_LATEX]->id = GEANY_FILETYPES_LATEX;
	filetypes[GEANY_FILETYPES_LATEX]->uid = FILETYPE_UID_LATEX;
	filetypes[GEANY_FILETYPES_LATEX]->lang = 8;
	filetypes[GEANY_FILETYPES_LATEX]->name = g_strdup("LaTeX");
	filetypes[GEANY_FILETYPES_LATEX]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_LATEX]->title = g_strdup(_("LaTeX source file"));
	filetypes[GEANY_FILETYPES_LATEX]->extension = g_strdup("tex");
	filetypes[GEANY_FILETYPES_LATEX]->pattern = utils_strv_new("*.tex", "*.sty", "*.idx", NULL);
	filetypes[GEANY_FILETYPES_LATEX]->style_func_ptr = styleset_latex;
	filetypes[GEANY_FILETYPES_LATEX]->comment_open = g_strdup("%");
	filetypes[GEANY_FILETYPES_LATEX]->comment_close = NULL;

#define OMS
	filetypes[GEANY_FILETYPES_OMS]->id = GEANY_FILETYPES_OMS;
	filetypes[GEANY_FILETYPES_OMS]->uid = FILETYPE_UID_OMS;
	filetypes[GEANY_FILETYPES_OMS]->lang = -2;
	filetypes[GEANY_FILETYPES_OMS]->name = g_strdup("O-Matrix");
	filetypes[GEANY_FILETYPES_OMS]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_OMS]->title = g_strdup(_("O-Matrix source file"));
	filetypes[GEANY_FILETYPES_OMS]->extension = g_strdup("oms");
	filetypes[GEANY_FILETYPES_OMS]->pattern = utils_strv_new("*.oms", NULL);
	filetypes[GEANY_FILETYPES_OMS]->style_func_ptr = styleset_oms;
	filetypes[GEANY_FILETYPES_OMS]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_OMS]->comment_close = NULL;

#define VHDL
	filetypes[GEANY_FILETYPES_VHDL]->id = GEANY_FILETYPES_VHDL;
	filetypes[GEANY_FILETYPES_VHDL]->uid = FILETYPE_UID_VHDL;
	filetypes[GEANY_FILETYPES_VHDL]->lang = 21;
	filetypes[GEANY_FILETYPES_VHDL]->name = g_strdup("VHDL");
	filetypes[GEANY_FILETYPES_VHDL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_VHDL]->title = g_strdup(_("VHDL source file"));
	filetypes[GEANY_FILETYPES_VHDL]->extension = g_strdup("vhd");
	filetypes[GEANY_FILETYPES_VHDL]->pattern = utils_strv_new("*.vhd", "*.vhdl", NULL);
	filetypes[GEANY_FILETYPES_VHDL]->style_func_ptr = styleset_vhdl;
	filetypes[GEANY_FILETYPES_VHDL]->comment_open = g_strdup("--");
	filetypes[GEANY_FILETYPES_VHDL]->comment_close = NULL;

#define DIFF
	filetypes[GEANY_FILETYPES_DIFF]->id = GEANY_FILETYPES_DIFF;
	filetypes[GEANY_FILETYPES_DIFF]->uid = FILETYPE_UID_DIFF;
	filetypes[GEANY_FILETYPES_DIFF]->lang = 20;
	filetypes[GEANY_FILETYPES_DIFF]->name = g_strdup("Diff");
	filetypes[GEANY_FILETYPES_DIFF]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_DIFF]->title = g_strdup(_("Diff file"));
	filetypes[GEANY_FILETYPES_DIFF]->extension = g_strdup("diff");
	filetypes[GEANY_FILETYPES_DIFF]->pattern = utils_strv_new("*.diff", "*.patch", "*.rej", NULL);
	filetypes[GEANY_FILETYPES_DIFF]->style_func_ptr = styleset_diff;
	filetypes[GEANY_FILETYPES_DIFF]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_DIFF]->comment_close = NULL;

#define CONF
	filetypes[GEANY_FILETYPES_CONF]->id = GEANY_FILETYPES_CONF;
	filetypes[GEANY_FILETYPES_CONF]->uid = FILETYPE_UID_CONF;
	filetypes[GEANY_FILETYPES_CONF]->lang = 10;
	filetypes[GEANY_FILETYPES_CONF]->name = g_strdup("Conf");
	filetypes[GEANY_FILETYPES_CONF]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CONF]->title = g_strdup(_("Config file"));
	filetypes[GEANY_FILETYPES_CONF]->extension = g_strdup("conf");
	filetypes[GEANY_FILETYPES_CONF]->pattern = utils_strv_new("*.conf", "*.ini", "config", "*rc",
		"*.cfg", NULL);
	filetypes[GEANY_FILETYPES_CONF]->style_func_ptr = styleset_conf;
	filetypes[GEANY_FILETYPES_CONF]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_CONF]->comment_close = NULL;

#define ALL
	filetypes[GEANY_FILETYPES_ALL]->id = GEANY_FILETYPES_ALL;
	filetypes[GEANY_FILETYPES_ALL]->name = g_strdup("None");
	filetypes[GEANY_FILETYPES_ALL]->uid = FILETYPE_UID_ALL;
	filetypes[GEANY_FILETYPES_ALL]->lang = -2;
	filetypes[GEANY_FILETYPES_ALL]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_ALL]->title = g_strdup(_("All files"));
	filetypes[GEANY_FILETYPES_ALL]->extension = g_strdup("*");
	filetypes[GEANY_FILETYPES_ALL]->pattern = utils_strv_new("*", NULL);
	filetypes[GEANY_FILETYPES_ALL]->style_func_ptr = styleset_none;
	filetypes[GEANY_FILETYPES_ALL]->comment_open = NULL;
	filetypes[GEANY_FILETYPES_ALL]->comment_close = NULL;
}


/* Calls filetypes_init_types() and creates the filetype menu. */
void filetypes_init()
{
	filetype_id ft_id;
	GtkWidget *filetype_menu = lookup_widget(app->window, "set_filetype1_menu");

	filetypes_init_types();

	// Append all filetypes to the filetype menu
	for (ft_id = 0; ft_id < GEANY_MAX_FILE_TYPES; ft_id++)
	{
		filetype *ft = filetypes[ft_id];
		const gchar *title = (ft_id == GEANY_FILETYPES_ALL) ? _("None") : ft->title;

		ft->item = NULL;
		filetypes_create_menu_item(filetype_menu, title, ft);
	}
}


static filetype *find_shebang(gint idx)
{
	gchar *line = sci_get_line(doc_list[idx].sci, 0);
	filetype *ft = NULL;

	if (strlen(line) > 2 && line[0] == '#' && line[1]=='!')
	{
		/// TODO does g_path_get_basename() also work under Win32 for Unix filenames?
		gchar *basename_interpreter = g_path_get_basename(line + 2);

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
		else if (strncmp(basename_interpreter, "zsh", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "ksh", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "csh", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_SH];
		else if (strncmp(basename_interpreter, "dmd", 3) == 0)
			ft = filetypes[GEANY_FILETYPES_D];
		// what else to add?

		g_free(basename_interpreter);
	}

	g_free(line);
	return ft;
}


/* Detect the filetype for document idx, checking for a shebang, then filename extension. */
filetype *filetypes_detect_from_file(gint idx)
{
	filetype *ft;

	if (! DOC_IDX_VALID(idx))
		return filetypes[GEANY_FILETYPES_ALL];

	// try to find a shebang and if found use it prior to the filename extension
	ft = find_shebang(idx);
	if (ft != NULL) return ft;

	if (doc_list[idx].file_name == NULL)
		return filetypes[GEANY_FILETYPES_ALL];

	return filetypes_detect_from_filename(doc_list[idx].file_name);
}


/* Detect filetype based on the filename extension.
 * utf8_filename can include the full path. */
filetype *filetypes_detect_from_filename(const gchar *utf8_filename)
{
	GPatternSpec *pattern;
	gchar *base_filename;
	gint i, j;

	// to match against the basename of the file(because of Makefile*)
	base_filename = g_path_get_basename(utf8_filename);

	for(i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		for (j = 0; filetypes[i]->pattern[j] != NULL; j++)
		{
			pattern = g_pattern_spec_new(filetypes[i]->pattern[j]);
			if (g_pattern_match_string(pattern, base_filename))
			{
				g_free(base_filename);
				g_pattern_spec_free(pattern);
				return filetypes[i];
			}
			g_pattern_spec_free(pattern);
		}
	}

	g_free(base_filename);
	return filetypes[GEANY_FILETYPES_ALL];
}


void filetypes_select_radio_item(const filetype *ft)
{
	// app->ignore_callback has to be set by the caller
	if (ft == NULL)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
			radio_items[filetypes[GEANY_FILETYPES_ALL]->id]), TRUE);
	else
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(radio_items[ft->id]), TRUE);
}


static void
on_filetype_change                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (app->ignore_callback || idx < 0 || ! doc_list[idx].is_valid) return;

	document_set_filetype(idx, (filetype*)user_data);
}


static void filetypes_create_menu_item(GtkWidget *menu, const gchar *label, filetype *ftype)
{
	static GSList *group = NULL;
	GtkWidget *tmp;

	tmp = gtk_radio_menu_item_new_with_label(group, label);
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(tmp));
	radio_items[ftype->id] = tmp;
	gtk_widget_show(tmp);
	gtk_container_add(GTK_CONTAINER(menu), tmp);
	g_signal_connect((gpointer) tmp, "activate", G_CALLBACK(on_filetype_change), (gpointer) ftype);
}


/* frees the array and all related pointers */
void filetypes_free_types()
{
	gint i;

	for(i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		if (filetypes[i])
		{
			g_free(filetypes[i]->name);
			g_free(filetypes[i]->title);
			g_free(filetypes[i]->extension);
			g_free(filetypes[i]->comment_open);
			g_free(filetypes[i]->comment_close);
			g_free(filetypes[i]->context_action_cmd);
			g_free(filetypes[i]->programs->compiler);
			g_free(filetypes[i]->programs->linker);
			g_free(filetypes[i]->programs->run_cmd);
			g_free(filetypes[i]->programs->run_cmd2);
			g_free(filetypes[i]->programs);
			g_free(filetypes[i]->actions);

			g_strfreev(filetypes[i]->pattern);
			g_free(filetypes[i]);
		}
	}
}


void filetypes_get_config(GKeyFile *config, GKeyFile *configh, gint ft)
{
	gchar *result;
	GError *error = NULL;
	gboolean tmp;

	if (config == NULL || configh == NULL || ft < 0 || ft >= GEANY_MAX_FILE_TYPES) return;

	// read comment notes
	result = g_key_file_get_string(configh, "settings", "comment_open", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "comment_open", NULL);
	if (result != NULL)
	{
		g_free(filetypes[ft]->comment_open);
		filetypes[ft]->comment_open = result;
	}

	result = g_key_file_get_string(configh, "settings", "comment_close", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "comment_close", NULL);
	if (result != NULL)
	{
		g_free(filetypes[ft]->comment_close);
		filetypes[ft]->comment_close = result;
	}

	tmp = g_key_file_get_boolean(configh, "settings", "comment_use_indent", &error);
	if (error)
	{
		g_error_free(error);
		error = NULL;
		tmp = g_key_file_get_boolean(config, "settings", "comment_use_indent", &error);
		if (error) g_error_free(error);
		else filetypes[ft]->comment_use_indent = tmp;
	}
	else filetypes[ft]->comment_use_indent = tmp;

	// read context action
	result = g_key_file_get_string(configh, "settings", "context_action_cmd", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "context_action_cmd", NULL);
	if (result != NULL)
	{
		filetypes[ft]->context_action_cmd = result;
	}

	// read build settings
	result = g_key_file_get_string(configh, "build_settings", "compiler", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "compiler", NULL);
	if (result != NULL)
	{
		filetypes[ft]->programs->compiler = result;
		filetypes[ft]->actions->can_compile = TRUE;
	}

	result = g_key_file_get_string(configh, "build_settings", "linker", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "linker", NULL);
	if (result != NULL)
	{
		filetypes[ft]->programs->linker = result;
		filetypes[ft]->actions->can_link = TRUE;
	}

	result = g_key_file_get_string(configh, "build_settings", "run_cmd", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "run_cmd", NULL);
	if (result != NULL)
	{
		filetypes[ft]->programs->run_cmd = result;
		filetypes[ft]->actions->can_exec = TRUE;
	}

	result = g_key_file_get_string(configh, "build_settings", "run_cmd2", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "build_settings", "run_cmd2", NULL);
	if (result != NULL)
	{
		filetypes[ft]->programs->run_cmd2 = result;
		filetypes[ft]->actions->can_exec = TRUE;
	}
}


gchar *filetypes_get_conf_extension(gint filetype_idx)
{
	gchar *result, *tmp = g_strdup(filetypes[filetype_idx]->name);

	// Handle any special extensions different from lowercase filetype->name
	switch (filetype_idx)
	{
		case GEANY_FILETYPES_CPP: result = g_strdup("cpp"); break;
		case GEANY_FILETYPES_MAKE: result = g_strdup("makefile"); break;
		case GEANY_FILETYPES_OMS: result = g_strdup("oms"); break;
		default: result = g_ascii_strdown(tmp, -1); break;
	}
	g_free(tmp);
	return result;
}


void filetypes_save_commands()
{
	gchar *conf_prefix = g_strconcat(app->configdir,
		G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.", NULL);
	gint i;

	for (i = 0; i < GEANY_FILETYPES_ALL; i++)
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
	gint i, j;

	new_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(new_filter, _("All Source"));

	for (i = 0; i < GEANY_FILETYPES_ALL; i++)
	{
		for (j = 0; filetypes[i]->pattern[j]; j++)
		{
			gtk_file_filter_add_pattern(new_filter, filetypes[i]->pattern[j]);
		}
	}

	return new_filter;
}


GtkFileFilter *filetypes_create_file_filter(filetype *ft)
{
	GtkFileFilter *new_filter;
	gint i;

	g_return_val_if_fail(ft != NULL, NULL);

	new_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(new_filter, ft->title);

	// (GEANY_FILETYPES_MAX_PATTERNS - 1) because the last field in pattern is NULL
	//for (i = 0; i < (GEANY_MAX_PATTERNS - 1) && ft->pattern[i]; i++)
	for (i = 0; ft->pattern[i]; i++)
	{
		gtk_file_filter_add_pattern(new_filter, ft->pattern[i]);
	}

	return new_filter;
}


