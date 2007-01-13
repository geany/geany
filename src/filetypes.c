/*
 *      filetypes.c - this file is part of Geany, a fast and lightweight IDE
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


#include <string.h>

#include "geany.h"
#include "filetypes.h"
#include "highlighting.h"
#include "support.h"
#include "callbacks.h"
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


static void filetypes_create_menu_item(GtkWidget *menu, gchar *label, filetype *ftype);
static void filetypes_create_newmenu_item(GtkWidget *menu, gchar *label, filetype *ftype);
static void filetypes_init_build_programs(filetype *ftype);

static GtkWidget *radio_items[GEANY_MAX_FILE_TYPES];


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


/* inits the filetype array and fill it with the known filetypes
 * and create the filetype menu*/
void filetypes_init_types()
{
	GtkWidget *filetype_menu = lookup_widget(app->window, "set_filetype1_menu");
	GtkWidget *template_menu = lookup_widget(app->window, "menu_new_with_template1_menu");

#define C	// these macros are only to ease navigation
	filetypes[GEANY_FILETYPES_C] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_C]->id = GEANY_FILETYPES_C;
	filetypes[GEANY_FILETYPES_C]->uid = FILETYPE_UID_C;
	filetypes[GEANY_FILETYPES_C]->item = NULL;
	filetypes[GEANY_FILETYPES_C]->lang = 0;
	filetypes[GEANY_FILETYPES_C]->name = g_strdup("C");
	filetypes[GEANY_FILETYPES_C]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_C]->title = g_strdup(_("C source file"));
	filetypes[GEANY_FILETYPES_C]->extension = g_strdup("c");
	filetypes[GEANY_FILETYPES_C]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_C]->pattern[0] = g_strdup("*.c");
	filetypes[GEANY_FILETYPES_C]->pattern[1] = g_strdup("*.h");
	filetypes[GEANY_FILETYPES_C]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_C]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_C]->comment_close = g_strdup("*/");
	filetypes[GEANY_FILETYPES_C]->style_func_ptr = styleset_c;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_C]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_C]->title, filetypes[GEANY_FILETYPES_C]);

#define CPP
	filetypes[GEANY_FILETYPES_CPP] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_CPP]->id = GEANY_FILETYPES_CPP;
	filetypes[GEANY_FILETYPES_CPP]->uid = FILETYPE_UID_CPP;
	filetypes[GEANY_FILETYPES_CPP]->item = NULL;
	filetypes[GEANY_FILETYPES_CPP]->lang = 1;
	filetypes[GEANY_FILETYPES_CPP]->name = g_strdup("C++");
	filetypes[GEANY_FILETYPES_CPP]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CPP]->title = g_strdup(_("C++ source file"));
	filetypes[GEANY_FILETYPES_CPP]->extension = g_strdup("cpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern = g_new0(gchar*, 11);
	filetypes[GEANY_FILETYPES_CPP]->pattern[0] = g_strdup("*.cpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern[1] = g_strdup("*.cxx");
	filetypes[GEANY_FILETYPES_CPP]->pattern[2] = g_strdup("*.c++");
	filetypes[GEANY_FILETYPES_CPP]->pattern[3] = g_strdup("*.cc");
	filetypes[GEANY_FILETYPES_CPP]->pattern[4] = g_strdup("*.h");
	filetypes[GEANY_FILETYPES_CPP]->pattern[5] = g_strdup("*.hpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern[6] = g_strdup("*.hxx");
	filetypes[GEANY_FILETYPES_CPP]->pattern[7] = g_strdup("*.h++");
	filetypes[GEANY_FILETYPES_CPP]->pattern[8] = g_strdup("*.hh");
	filetypes[GEANY_FILETYPES_CPP]->pattern[9] = g_strdup("*.C");
	filetypes[GEANY_FILETYPES_CPP]->pattern[10] = NULL;
	filetypes[GEANY_FILETYPES_CPP]->style_func_ptr = styleset_cpp;
	filetypes[GEANY_FILETYPES_CPP]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_CPP]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_CPP]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_CPP]->title, filetypes[GEANY_FILETYPES_CPP]);

#define D
	filetypes[GEANY_FILETYPES_D] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_D]->id = GEANY_FILETYPES_D;
	filetypes[GEANY_FILETYPES_D]->uid = FILETYPE_UID_D;
	filetypes[GEANY_FILETYPES_D]->item = NULL;
	filetypes[GEANY_FILETYPES_D]->lang = 17;
	filetypes[GEANY_FILETYPES_D]->name = g_strdup("D");
	filetypes[GEANY_FILETYPES_D]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_D]->title = g_strdup(_("D source file"));
	filetypes[GEANY_FILETYPES_D]->extension = g_strdup("d");
	filetypes[GEANY_FILETYPES_D]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_D]->pattern[0] = g_strdup("*.d");
	filetypes[GEANY_FILETYPES_D]->pattern[1] = g_strdup("*.di");
	filetypes[GEANY_FILETYPES_D]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_D]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_D]->comment_close = NULL;
	filetypes[GEANY_FILETYPES_D]->style_func_ptr = styleset_d;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_D]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_D]->title, filetypes[GEANY_FILETYPES_D]);

#define JAVA
	filetypes[GEANY_FILETYPES_JAVA] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_JAVA]->id = GEANY_FILETYPES_JAVA;
	filetypes[GEANY_FILETYPES_JAVA]->name = g_strdup("Java");
	filetypes[GEANY_FILETYPES_JAVA]->uid = FILETYPE_UID_JAVA;
	filetypes[GEANY_FILETYPES_JAVA]->item = NULL;
	filetypes[GEANY_FILETYPES_JAVA]->lang = 2;
	filetypes[GEANY_FILETYPES_JAVA]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_JAVA]->title = g_strdup(_("Java source file"));
	filetypes[GEANY_FILETYPES_JAVA]->extension = g_strdup("java");
	filetypes[GEANY_FILETYPES_JAVA]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_JAVA]->pattern[0] = g_strdup("*.java");
	filetypes[GEANY_FILETYPES_JAVA]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_JAVA]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_JAVA]->comment_close = g_strdup("*/");
	filetypes[GEANY_FILETYPES_JAVA]->style_func_ptr = styleset_java;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_JAVA]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_JAVA]->title, filetypes[GEANY_FILETYPES_JAVA]);

#define PAS // to avoid warnings when building under Windows, the symbol PASCAL is there defined
	filetypes[GEANY_FILETYPES_PASCAL] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_PASCAL]->id = GEANY_FILETYPES_PASCAL;
	filetypes[GEANY_FILETYPES_PASCAL]->uid = FILETYPE_UID_PASCAL;
	filetypes[GEANY_FILETYPES_PASCAL]->item = NULL;
	filetypes[GEANY_FILETYPES_PASCAL]->lang = 4;
	filetypes[GEANY_FILETYPES_PASCAL]->name = g_strdup("Pascal");
	filetypes[GEANY_FILETYPES_PASCAL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PASCAL]->title = g_strdup(_("Pascal source file"));
	filetypes[GEANY_FILETYPES_PASCAL]->extension = g_strdup("pas");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern = g_new0(gchar*, 6);
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[0] = g_strdup("*.pas");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[1] = g_strdup("*.pp");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[2] = g_strdup("*.inc");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[3] = g_strdup("*.dpr");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[4] = g_strdup("*.dpk");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[5] = NULL;
	filetypes[GEANY_FILETYPES_PASCAL]->style_func_ptr = styleset_pascal;
	filetypes[GEANY_FILETYPES_PASCAL]->comment_open = g_strdup("{");
	filetypes[GEANY_FILETYPES_PASCAL]->comment_close = g_strdup("}");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_PASCAL]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PASCAL]->title, filetypes[GEANY_FILETYPES_PASCAL]);

#define ASM
	filetypes[GEANY_FILETYPES_ASM] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_ASM]->id = GEANY_FILETYPES_ASM;
	filetypes[GEANY_FILETYPES_ASM]->uid = FILETYPE_UID_ASM;
	filetypes[GEANY_FILETYPES_ASM]->item = NULL;
	filetypes[GEANY_FILETYPES_ASM]->lang = 9;
	filetypes[GEANY_FILETYPES_ASM]->name = g_strdup("ASM");
	filetypes[GEANY_FILETYPES_ASM]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_ASM]->title = g_strdup(_("Assembler source file"));
	filetypes[GEANY_FILETYPES_ASM]->extension = g_strdup("asm");
	filetypes[GEANY_FILETYPES_ASM]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_ASM]->pattern[0] = g_strdup("*.asm");
	filetypes[GEANY_FILETYPES_ASM]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_ASM]->style_func_ptr = styleset_asm;
	filetypes[GEANY_FILETYPES_ASM]->comment_open = g_strdup(";");
	filetypes[GEANY_FILETYPES_ASM]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_ASM]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_ASM]->title, filetypes[GEANY_FILETYPES_ASM]);

#define FORTRAN
	filetypes[GEANY_FILETYPES_FORTRAN] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_FORTRAN]->id = GEANY_FILETYPES_FORTRAN;
	filetypes[GEANY_FILETYPES_FORTRAN]->uid = FILETYPE_UID_FORTRAN;
	filetypes[GEANY_FILETYPES_FORTRAN]->item = NULL;
	filetypes[GEANY_FILETYPES_FORTRAN]->lang = 18;
	filetypes[GEANY_FILETYPES_FORTRAN]->name = g_strdup("Fortran");
	filetypes[GEANY_FILETYPES_FORTRAN]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_FORTRAN]->title = g_strdup(_("Fortran source file (F77)"));
	filetypes[GEANY_FILETYPES_FORTRAN]->extension = g_strdup("f");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern = g_new0(gchar*, 7);
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern[0] = g_strdup("*.f");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern[1] = g_strdup("*.for");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern[2] = g_strdup("*.ftn");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern[3] = g_strdup("*.f77");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern[4] = g_strdup("*.f90");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern[5] = g_strdup("*.f95");
	filetypes[GEANY_FILETYPES_FORTRAN]->pattern[6] = NULL;
	filetypes[GEANY_FILETYPES_FORTRAN]->style_func_ptr = styleset_fortran;
	filetypes[GEANY_FILETYPES_FORTRAN]->comment_open = g_strdup("c");
	filetypes[GEANY_FILETYPES_FORTRAN]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_FORTRAN]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_FORTRAN]->title, filetypes[GEANY_FILETYPES_FORTRAN]);

#define CAML
	filetypes[GEANY_FILETYPES_CAML] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_CAML]->id = GEANY_FILETYPES_CAML;
	filetypes[GEANY_FILETYPES_CAML]->uid = FILETYPE_UID_CAML;
	filetypes[GEANY_FILETYPES_CAML]->item = NULL;
	filetypes[GEANY_FILETYPES_CAML]->lang = -2;
	filetypes[GEANY_FILETYPES_CAML]->name = g_strdup("CAML");
	filetypes[GEANY_FILETYPES_CAML]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_CAML]->title = g_strdup(_("(O)Caml source file"));
	filetypes[GEANY_FILETYPES_CAML]->extension = g_strdup("ml");
	filetypes[GEANY_FILETYPES_CAML]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_CAML]->pattern[0] = g_strdup("*.ml");
	filetypes[GEANY_FILETYPES_CAML]->pattern[1] = g_strdup("*.mli");
	filetypes[GEANY_FILETYPES_CAML]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_CAML]->style_func_ptr = styleset_caml;
	filetypes[GEANY_FILETYPES_CAML]->comment_open = g_strdup("(*");
	filetypes[GEANY_FILETYPES_CAML]->comment_close = g_strdup("*)");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_CAML]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_CAML]->title, filetypes[GEANY_FILETYPES_CAML]);

#define PERL
	filetypes[GEANY_FILETYPES_PERL] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_PERL]->id = GEANY_FILETYPES_PERL;
	filetypes[GEANY_FILETYPES_PERL]->uid = FILETYPE_UID_PERL;
	filetypes[GEANY_FILETYPES_PERL]->item = NULL;
	filetypes[GEANY_FILETYPES_PERL]->lang = 5;
	filetypes[GEANY_FILETYPES_PERL]->name = g_strdup("Perl");
	filetypes[GEANY_FILETYPES_PERL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PERL]->title = g_strdup(_("Perl source file"));
	filetypes[GEANY_FILETYPES_PERL]->extension = g_strdup("perl");
	filetypes[GEANY_FILETYPES_PERL]->pattern = g_new0(gchar*, 5);
	filetypes[GEANY_FILETYPES_PERL]->pattern[0] = g_strdup("*.pl");
	filetypes[GEANY_FILETYPES_PERL]->pattern[1] = g_strdup("*.perl");
	filetypes[GEANY_FILETYPES_PERL]->pattern[2] = g_strdup("*.pm");
	filetypes[GEANY_FILETYPES_PERL]->pattern[3] = g_strdup("*.agi");
	filetypes[GEANY_FILETYPES_PERL]->pattern[4] = NULL;
	filetypes[GEANY_FILETYPES_PERL]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_PERL]->comment_close = NULL;
	filetypes[GEANY_FILETYPES_PERL]->style_func_ptr = styleset_perl;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_PERL]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PERL]->title, filetypes[GEANY_FILETYPES_PERL]);

#define PHP
	filetypes[GEANY_FILETYPES_PHP] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_PHP]->id = GEANY_FILETYPES_PHP;
	filetypes[GEANY_FILETYPES_PHP]->uid = FILETYPE_UID_PHP;
	filetypes[GEANY_FILETYPES_PHP]->item = NULL;
	filetypes[GEANY_FILETYPES_PHP]->lang = 6;
	filetypes[GEANY_FILETYPES_PHP]->name = g_strdup("PHP");
	filetypes[GEANY_FILETYPES_PHP]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PHP]->title = g_strdup(_("PHP source file"));
	filetypes[GEANY_FILETYPES_PHP]->extension = g_strdup("php");
	filetypes[GEANY_FILETYPES_PHP]->pattern = g_new0(gchar*, 6);
	filetypes[GEANY_FILETYPES_PHP]->pattern[0] = g_strdup("*.php");
	filetypes[GEANY_FILETYPES_PHP]->pattern[1] = g_strdup("*.php3");
	filetypes[GEANY_FILETYPES_PHP]->pattern[2] = g_strdup("*.php4");
	filetypes[GEANY_FILETYPES_PHP]->pattern[3] = g_strdup("*.php5");
	filetypes[GEANY_FILETYPES_PHP]->pattern[4] = g_strdup("*.phtml");
	filetypes[GEANY_FILETYPES_PHP]->pattern[5] = NULL;
	filetypes[GEANY_FILETYPES_PHP]->style_func_ptr = styleset_php;
	filetypes[GEANY_FILETYPES_PHP]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_PHP]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_PHP]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PHP]->title, filetypes[GEANY_FILETYPES_PHP]);

#define JAVASCRIPT
	filetypes[GEANY_FILETYPES_JS] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_JS]->id = GEANY_FILETYPES_JS;
	filetypes[GEANY_FILETYPES_JS]->uid = FILETYPE_UID_JS;
	filetypes[GEANY_FILETYPES_JS]->item = NULL;
	filetypes[GEANY_FILETYPES_JS]->lang = 23;
	filetypes[GEANY_FILETYPES_JS]->name = g_strdup("Javascript");
	filetypes[GEANY_FILETYPES_JS]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_JS]->title = g_strdup(_("Javascript source file"));
	filetypes[GEANY_FILETYPES_JS]->extension = g_strdup("js");
	filetypes[GEANY_FILETYPES_JS]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_JS]->pattern[0] = g_strdup("*.js");
	filetypes[GEANY_FILETYPES_JS]->pattern[1] = g_strdup("*.jsp"); /// TODO what is jsp actually?
	filetypes[GEANY_FILETYPES_JS]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_JS]->style_func_ptr = styleset_js;
	filetypes[GEANY_FILETYPES_JS]->comment_open = g_strdup("//");
	filetypes[GEANY_FILETYPES_JS]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_JS]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_JS]->title, filetypes[GEANY_FILETYPES_JS]);

#define PYTHON
	filetypes[GEANY_FILETYPES_PYTHON] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_PYTHON]->id = GEANY_FILETYPES_PYTHON;
	filetypes[GEANY_FILETYPES_PYTHON]->uid = FILETYPE_UID_PYTHON;
	filetypes[GEANY_FILETYPES_PYTHON]->item = NULL;
	filetypes[GEANY_FILETYPES_PYTHON]->lang = 7;
	filetypes[GEANY_FILETYPES_PYTHON]->name = g_strdup("Python");
	filetypes[GEANY_FILETYPES_PYTHON]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PYTHON]->title = g_strdup(_("Python source file"));
	filetypes[GEANY_FILETYPES_PYTHON]->extension = g_strdup("py");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[0] = g_strdup("*.py");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[1] = g_strdup("*.pyw");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_PYTHON]->style_func_ptr = styleset_python;
	filetypes[GEANY_FILETYPES_PYTHON]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_PYTHON]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_PYTHON]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PYTHON]->title, filetypes[GEANY_FILETYPES_PYTHON]);

#define RUBY
	filetypes[GEANY_FILETYPES_RUBY] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_RUBY]->id = GEANY_FILETYPES_RUBY;
	filetypes[GEANY_FILETYPES_RUBY]->uid = FILETYPE_UID_RUBY;
	filetypes[GEANY_FILETYPES_RUBY]->item = NULL;
	filetypes[GEANY_FILETYPES_RUBY]->lang = 14;
	filetypes[GEANY_FILETYPES_RUBY]->name = g_strdup("Ruby");
	filetypes[GEANY_FILETYPES_RUBY]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_RUBY]->title = g_strdup(_("Ruby source file"));
	filetypes[GEANY_FILETYPES_RUBY]->extension = g_strdup("rb");
	filetypes[GEANY_FILETYPES_RUBY]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_RUBY]->pattern[0] = g_strdup("*.rb");
	filetypes[GEANY_FILETYPES_RUBY]->pattern[1] = g_strdup("*.rhtml");
	filetypes[GEANY_FILETYPES_RUBY]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_RUBY]->style_func_ptr = styleset_ruby;
	filetypes[GEANY_FILETYPES_RUBY]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_RUBY]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_RUBY]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_RUBY]->title, filetypes[GEANY_FILETYPES_RUBY]);

#define TCL
	filetypes[GEANY_FILETYPES_TCL] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_TCL]->id = GEANY_FILETYPES_TCL;
	filetypes[GEANY_FILETYPES_TCL]->uid = FILETYPE_UID_TCL;
	filetypes[GEANY_FILETYPES_TCL]->item = NULL;
	filetypes[GEANY_FILETYPES_TCL]->lang = 15;
	filetypes[GEANY_FILETYPES_TCL]->name = g_strdup("Tcl");
	filetypes[GEANY_FILETYPES_TCL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_TCL]->title = g_strdup(_("Tcl source file"));
	filetypes[GEANY_FILETYPES_TCL]->extension = g_strdup("tcl");
	filetypes[GEANY_FILETYPES_TCL]->pattern = g_new0(gchar*, 4);
	filetypes[GEANY_FILETYPES_TCL]->pattern[0] = g_strdup("*.tcl");
	filetypes[GEANY_FILETYPES_TCL]->pattern[1] = g_strdup("*.tk");
	filetypes[GEANY_FILETYPES_TCL]->pattern[2] = g_strdup("*.wish");
	filetypes[GEANY_FILETYPES_TCL]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_TCL]->style_func_ptr = styleset_tcl;
	filetypes[GEANY_FILETYPES_TCL]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_TCL]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_TCL]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_TCL]->title, filetypes[GEANY_FILETYPES_TCL]);

#define LUA
	filetypes[GEANY_FILETYPES_LUA] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_LUA]->id = GEANY_FILETYPES_LUA;
	filetypes[GEANY_FILETYPES_LUA]->uid = FILETYPE_UID_LUA;
	filetypes[GEANY_FILETYPES_LUA]->item = NULL;
	filetypes[GEANY_FILETYPES_LUA]->lang = 22;
	filetypes[GEANY_FILETYPES_LUA]->name = g_strdup("Lua");
	filetypes[GEANY_FILETYPES_LUA]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_LUA]->title = g_strdup(_("Lua source file"));
	filetypes[GEANY_FILETYPES_LUA]->extension = g_strdup("lua");
	filetypes[GEANY_FILETYPES_LUA]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_LUA]->pattern[0] = g_strdup("*.lua");
	filetypes[GEANY_FILETYPES_LUA]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_LUA]->style_func_ptr = styleset_lua;
	filetypes[GEANY_FILETYPES_LUA]->comment_open = g_strdup("--");
	filetypes[GEANY_FILETYPES_LUA]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_LUA]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_LUA]->title, filetypes[GEANY_FILETYPES_LUA]);

#define FERITE
	filetypes[GEANY_FILETYPES_FERITE] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_FERITE]->id = GEANY_FILETYPES_FERITE;
	filetypes[GEANY_FILETYPES_FERITE]->uid = FILETYPE_UID_FERITE;
	filetypes[GEANY_FILETYPES_FERITE]->item = NULL;
	filetypes[GEANY_FILETYPES_FERITE]->lang = 19;
	filetypes[GEANY_FILETYPES_FERITE]->name = g_strdup("Ferite");
	filetypes[GEANY_FILETYPES_FERITE]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_FERITE]->title = g_strdup(_("Ferite source file"));
	filetypes[GEANY_FILETYPES_FERITE]->extension = g_strdup("fe");
	filetypes[GEANY_FILETYPES_FERITE]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_FERITE]->pattern[0] = g_strdup("*.fe");
	filetypes[GEANY_FILETYPES_FERITE]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_FERITE]->style_func_ptr = styleset_ferite;
	filetypes[GEANY_FILETYPES_FERITE]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_FERITE]->comment_close = g_strdup("*/");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_FERITE]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_FERITE]->title, filetypes[GEANY_FILETYPES_FERITE]);

#define SH
	filetypes[GEANY_FILETYPES_SH] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_SH]->id = GEANY_FILETYPES_SH;
	filetypes[GEANY_FILETYPES_SH]->uid = FILETYPE_UID_SH;
	filetypes[GEANY_FILETYPES_SH]->item = NULL;
	filetypes[GEANY_FILETYPES_SH]->lang = 16;
	filetypes[GEANY_FILETYPES_SH]->name = g_strdup("Sh");
	filetypes[GEANY_FILETYPES_SH]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_SH]->title = g_strdup(_("Shell script file"));
	filetypes[GEANY_FILETYPES_SH]->extension = g_strdup("sh");
	filetypes[GEANY_FILETYPES_SH]->pattern = g_new0(gchar*, 8);
	filetypes[GEANY_FILETYPES_SH]->pattern[0] = g_strdup("*.sh");
	filetypes[GEANY_FILETYPES_SH]->pattern[1] = g_strdup("configure");
	filetypes[GEANY_FILETYPES_SH]->pattern[2] = g_strdup("configure.in");
	filetypes[GEANY_FILETYPES_SH]->pattern[3] = g_strdup("configure.in.in");
	filetypes[GEANY_FILETYPES_SH]->pattern[4] = g_strdup("configure.ac");
	filetypes[GEANY_FILETYPES_SH]->pattern[5] = g_strdup("*.ksh");
	filetypes[GEANY_FILETYPES_SH]->pattern[6] = g_strdup("*.zsh");
	filetypes[GEANY_FILETYPES_SH]->pattern[7] = NULL;
	filetypes[GEANY_FILETYPES_SH]->style_func_ptr = styleset_sh;
	filetypes[GEANY_FILETYPES_SH]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_SH]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_SH]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_SH]->title, filetypes[GEANY_FILETYPES_SH]);

#define MAKE
	filetypes[GEANY_FILETYPES_MAKE] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_MAKE]->id = GEANY_FILETYPES_MAKE;
	filetypes[GEANY_FILETYPES_MAKE]->uid = FILETYPE_UID_MAKE;
	filetypes[GEANY_FILETYPES_MAKE]->item = NULL;
	filetypes[GEANY_FILETYPES_MAKE]->lang = 3;
	filetypes[GEANY_FILETYPES_MAKE]->name = g_strdup("Make");
	filetypes[GEANY_FILETYPES_MAKE]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_MAKE]->title = g_strdup(_("Makefile"));
	filetypes[GEANY_FILETYPES_MAKE]->extension = g_strdup("mak");
	{
		gchar *patterns[] = {"*.mak", "*.mk", "GNUmakefile", "makefile", "Makefile",
			"makefile.*", "Makefile.*", NULL};
		filetypes[GEANY_FILETYPES_MAKE]->pattern = g_strdupv(patterns);
	}
	filetypes[GEANY_FILETYPES_MAKE]->style_func_ptr = styleset_makefile;
	filetypes[GEANY_FILETYPES_MAKE]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_MAKE]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_MAKE]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_MAKE]->title, filetypes[GEANY_FILETYPES_MAKE]);

#define XML
	filetypes[GEANY_FILETYPES_XML] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_XML]->id = GEANY_FILETYPES_XML;
	filetypes[GEANY_FILETYPES_XML]->uid = FILETYPE_UID_XML;
	filetypes[GEANY_FILETYPES_XML]->item = NULL;
	filetypes[GEANY_FILETYPES_XML]->lang = -2;
	filetypes[GEANY_FILETYPES_XML]->name = g_strdup("XML");
	filetypes[GEANY_FILETYPES_XML]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_XML]->title = g_strdup(_("XML source file"));
	filetypes[GEANY_FILETYPES_XML]->extension = g_strdup("xml");
	{
		gchar *patterns[] = {"*.xml", "*.sgml", "*.xsl", "*.xslt", NULL};
		filetypes[GEANY_FILETYPES_XML]->pattern = g_strdupv(patterns);
	}
	filetypes[GEANY_FILETYPES_XML]->style_func_ptr = styleset_xml;
	filetypes[GEANY_FILETYPES_XML]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_XML]->comment_close = g_strdup("-->");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_XML]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_XML]->title, filetypes[GEANY_FILETYPES_XML]);

#define DOCBOOK
	filetypes[GEANY_FILETYPES_DOCBOOK] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_DOCBOOK]->id = GEANY_FILETYPES_DOCBOOK;
	filetypes[GEANY_FILETYPES_DOCBOOK]->uid = FILETYPE_UID_DOCBOOK;
	filetypes[GEANY_FILETYPES_DOCBOOK]->item = NULL;
	filetypes[GEANY_FILETYPES_DOCBOOK]->lang = 12;
	filetypes[GEANY_FILETYPES_DOCBOOK]->name = g_strdup("Docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_DOCBOOK]->title = g_strdup(_("Docbook source file"));
	filetypes[GEANY_FILETYPES_DOCBOOK]->extension = g_strdup("docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern[0] = g_strdup("*.docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_DOCBOOK]->style_func_ptr = styleset_docbook;
	filetypes[GEANY_FILETYPES_DOCBOOK]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_DOCBOOK]->comment_close = g_strdup("-->");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_DOCBOOK]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_DOCBOOK]->title, filetypes[GEANY_FILETYPES_DOCBOOK]);

#define HTML
	filetypes[GEANY_FILETYPES_HTML] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_HTML]->id = GEANY_FILETYPES_HTML;
	filetypes[GEANY_FILETYPES_HTML]->uid = FILETYPE_UID_HTML;
	filetypes[GEANY_FILETYPES_HTML]->item = NULL;
	filetypes[GEANY_FILETYPES_HTML]->lang = -2;
	filetypes[GEANY_FILETYPES_HTML]->name = g_strdup("HTML");
	filetypes[GEANY_FILETYPES_HTML]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_HTML]->title = g_strdup(_("HTML source file"));
	filetypes[GEANY_FILETYPES_HTML]->extension = g_strdup("html");
	{
		gchar *patterns[] = {"*.htm", "*.html", "*.shtml", "*.hta", "*.htd", "*.htt",
			"*.cfm", NULL};
		filetypes[GEANY_FILETYPES_HTML]->pattern = g_strdupv(patterns);
	}
	filetypes[GEANY_FILETYPES_HTML]->style_func_ptr = styleset_html;
	filetypes[GEANY_FILETYPES_HTML]->comment_open = g_strdup("<!--");
	filetypes[GEANY_FILETYPES_HTML]->comment_close = g_strdup("-->");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_HTML]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_HTML]->title, filetypes[GEANY_FILETYPES_HTML]);

#define CSS
	filetypes[GEANY_FILETYPES_CSS] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_CSS]->id = GEANY_FILETYPES_CSS;
	filetypes[GEANY_FILETYPES_CSS]->uid = FILETYPE_UID_CSS;
	filetypes[GEANY_FILETYPES_CSS]->item = NULL;
	filetypes[GEANY_FILETYPES_CSS]->lang = 13;
	filetypes[GEANY_FILETYPES_CSS]->name = g_strdup("CSS");
	filetypes[GEANY_FILETYPES_CSS]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CSS]->title = g_strdup(_("Cascading StyleSheet"));
	filetypes[GEANY_FILETYPES_CSS]->extension = g_strdup("css");
	filetypes[GEANY_FILETYPES_CSS]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_CSS]->pattern[0] = g_strdup("*.css");
	filetypes[GEANY_FILETYPES_CSS]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_CSS]->style_func_ptr = styleset_css;
	filetypes[GEANY_FILETYPES_CSS]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_CSS]->comment_close = g_strdup("*/");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_CSS]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_CSS]->title, filetypes[GEANY_FILETYPES_CSS]);

#define SQL
	filetypes[GEANY_FILETYPES_SQL] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_SQL]->id = GEANY_FILETYPES_SQL;
	filetypes[GEANY_FILETYPES_SQL]->uid = FILETYPE_UID_SQL;
	filetypes[GEANY_FILETYPES_SQL]->item = NULL;
	filetypes[GEANY_FILETYPES_SQL]->lang = 11;
	filetypes[GEANY_FILETYPES_SQL]->name = g_strdup("SQL");
	filetypes[GEANY_FILETYPES_SQL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_SQL]->title = g_strdup(_("SQL Dump file"));
	filetypes[GEANY_FILETYPES_SQL]->extension = g_strdup("sql");
	filetypes[GEANY_FILETYPES_SQL]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_SQL]->pattern[0] = g_strdup("*.sql");
	filetypes[GEANY_FILETYPES_SQL]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_SQL]->style_func_ptr = styleset_sql;
	filetypes[GEANY_FILETYPES_SQL]->comment_open = g_strdup("/*");
	filetypes[GEANY_FILETYPES_SQL]->comment_close = g_strdup("*/");
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_SQL]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_SQL]->title, filetypes[GEANY_FILETYPES_SQL]);

#define LATEX
	filetypes[GEANY_FILETYPES_LATEX] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_LATEX]->id = GEANY_FILETYPES_LATEX;
	filetypes[GEANY_FILETYPES_LATEX]->uid = FILETYPE_UID_LATEX;
	filetypes[GEANY_FILETYPES_LATEX]->item = NULL;
	filetypes[GEANY_FILETYPES_LATEX]->lang = 8;
	filetypes[GEANY_FILETYPES_LATEX]->name = g_strdup("LaTeX");
	filetypes[GEANY_FILETYPES_LATEX]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_LATEX]->title = g_strdup(_("LaTeX source file"));
	filetypes[GEANY_FILETYPES_LATEX]->extension = g_strdup("tex");
	filetypes[GEANY_FILETYPES_LATEX]->pattern = g_new0(gchar*, 4);
	filetypes[GEANY_FILETYPES_LATEX]->pattern[0] = g_strdup("*.tex");
	filetypes[GEANY_FILETYPES_LATEX]->pattern[1] = g_strdup("*.sty");
	filetypes[GEANY_FILETYPES_LATEX]->pattern[2] = g_strdup("*.idx");
	filetypes[GEANY_FILETYPES_LATEX]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_LATEX]->style_func_ptr = styleset_latex;
	filetypes[GEANY_FILETYPES_LATEX]->comment_open = g_strdup("%");
	filetypes[GEANY_FILETYPES_LATEX]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_LATEX]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_LATEX]->title, filetypes[GEANY_FILETYPES_LATEX]);

#define OMS
	filetypes[GEANY_FILETYPES_OMS] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_OMS]->id = GEANY_FILETYPES_OMS;
	filetypes[GEANY_FILETYPES_OMS]->uid = FILETYPE_UID_OMS;
	filetypes[GEANY_FILETYPES_OMS]->item = NULL;
	filetypes[GEANY_FILETYPES_OMS]->lang = -2;
	filetypes[GEANY_FILETYPES_OMS]->name = g_strdup("O-Matrix");
	filetypes[GEANY_FILETYPES_OMS]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_OMS]->title = g_strdup(_("O-Matrix source file"));
	filetypes[GEANY_FILETYPES_OMS]->extension = g_strdup("oms");
	filetypes[GEANY_FILETYPES_OMS]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_OMS]->pattern[0] = g_strdup("*.oms");
	filetypes[GEANY_FILETYPES_OMS]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_OMS]->style_func_ptr = styleset_oms;
	filetypes[GEANY_FILETYPES_OMS]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_OMS]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_OMS]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_OMS]->title, filetypes[GEANY_FILETYPES_OMS]);

#define VHDL
	filetypes[GEANY_FILETYPES_VHDL] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_VHDL]->id = GEANY_FILETYPES_VHDL;
	filetypes[GEANY_FILETYPES_VHDL]->uid = FILETYPE_UID_VHDL;
	filetypes[GEANY_FILETYPES_VHDL]->item = NULL;
	filetypes[GEANY_FILETYPES_VHDL]->lang = 21;
	filetypes[GEANY_FILETYPES_VHDL]->name = g_strdup("VHDL");
	filetypes[GEANY_FILETYPES_VHDL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_VHDL]->title = g_strdup(_("VHDL source file"));
	filetypes[GEANY_FILETYPES_VHDL]->extension = g_strdup("vhd");
	filetypes[GEANY_FILETYPES_VHDL]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_VHDL]->pattern[0] = g_strdup("*.vhd");
	filetypes[GEANY_FILETYPES_VHDL]->pattern[1] = g_strdup("*.vhdl");
	filetypes[GEANY_FILETYPES_VHDL]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_VHDL]->style_func_ptr = styleset_vhdl;
	filetypes[GEANY_FILETYPES_VHDL]->comment_open = g_strdup("--");
	filetypes[GEANY_FILETYPES_VHDL]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_VHDL]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_VHDL]->title, filetypes[GEANY_FILETYPES_VHDL]);

#define DIFF
	filetypes[GEANY_FILETYPES_DIFF] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_DIFF]->id = GEANY_FILETYPES_DIFF;
	filetypes[GEANY_FILETYPES_DIFF]->uid = FILETYPE_UID_DIFF;
	filetypes[GEANY_FILETYPES_DIFF]->item = NULL;
	filetypes[GEANY_FILETYPES_DIFF]->lang = 20;
	filetypes[GEANY_FILETYPES_DIFF]->name = g_strdup("Diff");
	filetypes[GEANY_FILETYPES_DIFF]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_DIFF]->title = g_strdup(_("Diff file"));
	filetypes[GEANY_FILETYPES_DIFF]->extension = g_strdup("diff");
	filetypes[GEANY_FILETYPES_DIFF]->pattern = g_new0(gchar*, 3);
	filetypes[GEANY_FILETYPES_DIFF]->pattern[0] = g_strdup("*.diff");
	filetypes[GEANY_FILETYPES_DIFF]->pattern[1] = g_strdup("*.patch");
	filetypes[GEANY_FILETYPES_DIFF]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_DIFF]->style_func_ptr = styleset_diff;
	filetypes[GEANY_FILETYPES_DIFF]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_DIFF]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_DIFF]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_DIFF]->title, filetypes[GEANY_FILETYPES_DIFF]);

#define CONF
	filetypes[GEANY_FILETYPES_CONF] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_CONF]->id = GEANY_FILETYPES_CONF;
	filetypes[GEANY_FILETYPES_CONF]->uid = FILETYPE_UID_CONF;
	filetypes[GEANY_FILETYPES_CONF]->item = NULL;
	filetypes[GEANY_FILETYPES_CONF]->lang = 10;
	filetypes[GEANY_FILETYPES_CONF]->name = g_strdup("Conf");
	filetypes[GEANY_FILETYPES_CONF]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CONF]->title = g_strdup(_("Config file"));
	filetypes[GEANY_FILETYPES_CONF]->extension = g_strdup("conf");
	filetypes[GEANY_FILETYPES_CONF]->pattern = g_new0(gchar*, 6);
	filetypes[GEANY_FILETYPES_CONF]->pattern[0] = g_strdup("*.conf");
	filetypes[GEANY_FILETYPES_CONF]->pattern[1] = g_strdup("*.ini");
	filetypes[GEANY_FILETYPES_CONF]->pattern[2] = g_strdup("config");
	filetypes[GEANY_FILETYPES_CONF]->pattern[3] = g_strdup("*rc");
	filetypes[GEANY_FILETYPES_CONF]->pattern[4] = g_strdup("*.cfg");
	filetypes[GEANY_FILETYPES_CONF]->pattern[5] = NULL;
	filetypes[GEANY_FILETYPES_CONF]->style_func_ptr = styleset_conf;
	filetypes[GEANY_FILETYPES_CONF]->comment_open = g_strdup("#");
	filetypes[GEANY_FILETYPES_CONF]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_CONF]);
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_CONF]->title, filetypes[GEANY_FILETYPES_CONF]);

#define ALL
	filetypes[GEANY_FILETYPES_ALL] = g_new0(filetype, 1);
	filetypes[GEANY_FILETYPES_ALL]->id = GEANY_FILETYPES_ALL;
	filetypes[GEANY_FILETYPES_ALL]->name = g_strdup("None");
	filetypes[GEANY_FILETYPES_ALL]->uid = FILETYPE_UID_ALL;
	filetypes[GEANY_FILETYPES_ALL]->item = NULL;
	filetypes[GEANY_FILETYPES_ALL]->lang = -2;
	filetypes[GEANY_FILETYPES_ALL]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_ALL]->title = g_strdup(_("All files"));
	filetypes[GEANY_FILETYPES_ALL]->extension = g_strdup("*");
	filetypes[GEANY_FILETYPES_ALL]->pattern = g_new0(gchar*, 2);
	filetypes[GEANY_FILETYPES_ALL]->pattern[0] = g_strdup("*");
	filetypes[GEANY_FILETYPES_ALL]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_ALL]->style_func_ptr = styleset_none;
	filetypes[GEANY_FILETYPES_ALL]->comment_open = NULL;
	filetypes[GEANY_FILETYPES_ALL]->comment_close = NULL;
	filetypes_init_build_programs(filetypes[GEANY_FILETYPES_ALL]);
	filetypes_create_menu_item(filetype_menu, _("None"), filetypes[GEANY_FILETYPES_ALL]);

	// now add the items for the new file menu
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_C]->title,
																filetypes[GEANY_FILETYPES_C]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_CPP]->title,
																filetypes[GEANY_FILETYPES_CPP]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_D]->title,
																filetypes[GEANY_FILETYPES_D]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_JAVA]->title,
																filetypes[GEANY_FILETYPES_JAVA]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_PASCAL]->title,
																filetypes[GEANY_FILETYPES_PASCAL]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_RUBY]->title,
																filetypes[GEANY_FILETYPES_RUBY]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_PHP]->title,
																filetypes[GEANY_FILETYPES_PHP]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_HTML]->title,
																filetypes[GEANY_FILETYPES_HTML]);

}


/* convenience function - NULLs and zeros struct members */
static void filetypes_init_build_programs(filetype *ftype)
{
	ftype->programs = g_new0(struct build_programs, 1);

	ftype->actions = g_new0(struct build_actions, 1);
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


/* simple filetype selection based on the filename extension */
filetype *filetypes_get_from_filename(gint idx)
{
	GPatternSpec *pattern;
	filetype *ft;
	gchar *filename;
	gchar *base_filename, *utf8_filename;
	gint i, j;

	if (! DOC_IDX_VALID(idx))
		return filetypes[GEANY_FILETYPES_ALL];

	// try to find a shebang and if found use it prior to the filename extension
	ft = find_shebang(idx);
	if (ft != NULL) return ft;

	if (doc_list[idx].file_name == NULL)
		return filetypes[GEANY_FILETYPES_ALL];
	else
		filename = doc_list[idx].file_name;

	// try to get the UTF-8 equivalent for the filename
	utf8_filename = g_locale_to_utf8(filename, -1, NULL, NULL, NULL);
	if (utf8_filename == NULL)
	{
		return filetypes[GEANY_FILETYPES_ALL];
	}

	// to match against the basename of the file(because of Makefile*)
	base_filename = g_path_get_basename(utf8_filename);
	g_free(utf8_filename);

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


static void filetypes_create_menu_item(GtkWidget *menu, gchar *label, filetype *ftype)
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


static void filetypes_create_newmenu_item(GtkWidget *menu, gchar *label, filetype *ftype)
{
	GtkWidget *tmp_menu = gtk_menu_item_new_with_label(label);
	GtkWidget *tmp_button = gtk_menu_item_new_with_label(label);
	gtk_widget_show(tmp_menu);
	gtk_widget_show(tmp_button);
	gtk_container_add(GTK_CONTAINER(menu), tmp_menu);
	gtk_container_add(GTK_CONTAINER(app->new_file_menu), tmp_button);
	g_signal_connect((gpointer) tmp_menu, "activate", G_CALLBACK(on_new_with_template), (gpointer) ftype);
	g_signal_connect((gpointer) tmp_button, "activate", G_CALLBACK(on_new_with_template), (gpointer) ftype);
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

	switch (filetype_idx)
	{
		case GEANY_FILETYPES_ALL: result = g_strdup("common"); break;
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


GtkFileFilter *filetypes_create_file_filter(filetype *ft)
{
	GtkFileFilter *new_filter;
	gint i;

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


