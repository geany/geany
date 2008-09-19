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

/**
 * @file filetypes.h
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
#include "editor.h"
#include "msgwindow.h"
#include "utils.h"
#include "sciwrappers.h"
#include "ui_utils.h"


/* Private GeanyFiletype fields */
typedef struct GeanyFiletypePrivate
{
	GtkWidget		*menu_item;			/* holds a pointer to the menu item for this filetype */
	gboolean		keyfile_loaded;
}
GeanyFiletypePrivate;


GPtrArray *filetypes_array = NULL;	/* Dynamic array of filetype pointers */

GHashTable *filetypes_hash = NULL;	/* Hash of filetype pointers based on name keys */


static void create_radio_menu_item(GtkWidget *menu, const gchar *label, GeanyFiletype *ftype);


static void init_builtin_filetypes(void)
{
	GeanyFiletype *ft;

#define C	/* these macros are only to ease navigation */
	ft = filetypes[GEANY_FILETYPES_C];
	ft->lang = 0;
	ft->name = g_strdup("C");
	ft->title = g_strdup_printf(_("%s source file"), "C");
	ft->extension = g_strdup("c");
	ft->pattern = utils_strv_new("*.c", "*.h", NULL);
	ft->comment_open = g_strdup("/*");
	ft->comment_close = g_strdup("*/");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define CPP
	ft = filetypes[GEANY_FILETYPES_CPP];
	ft->lang = 1;
	ft->name = g_strdup("C++");
	ft->title = g_strdup_printf(_("%s source file"), "C++");
	ft->extension = g_strdup("cpp");
	ft->pattern = utils_strv_new("*.cpp", "*.cxx", "*.c++", "*.cc",
		"*.h", "*.hpp", "*.hxx", "*.h++", "*.hh", "*.C", NULL);
	ft->comment_open = g_strdup("//");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define CS
	ft = filetypes[GEANY_FILETYPES_CS];
	ft->lang = 25;
	ft->name = g_strdup("C#");
	ft->title = g_strdup_printf(_("%s source file"), "C#");
	ft->extension = g_strdup("cs");
	ft->pattern = utils_strv_new("*.cs", "*.vala", NULL);
	ft->comment_open = g_strdup("//");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define D
	ft = filetypes[GEANY_FILETYPES_D];
	ft->lang = 17;
	ft->name = g_strdup("D");
	ft->title = g_strdup_printf(_("%s source file"), "D");
	ft->extension = g_strdup("d");
	ft->pattern = utils_strv_new("*.d", "*.di", NULL);
	ft->comment_open = g_strdup("//");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define JAVA
	ft = filetypes[GEANY_FILETYPES_JAVA];
	ft->lang = 2;
	ft->name = g_strdup("Java");
	ft->title = g_strdup_printf(_("%s source file"), "Java");
	ft->extension = g_strdup("java");
	ft->pattern = utils_strv_new("*.java", "*.jsp", NULL);
	ft->comment_open = g_strdup("/*");
	ft->comment_close = g_strdup("*/");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define PAS /* to avoid warnings when building under Windows, the symbol PASCAL is there defined */
	ft = filetypes[GEANY_FILETYPES_PASCAL];
	ft->lang = 4;
	ft->name = g_strdup("Pascal");
	ft->title = g_strdup_printf(_("%s source file"), "Pascal");
	ft->extension = g_strdup("pas");
	ft->pattern = utils_strv_new("*.pas", "*.pp", "*.inc", "*.dpr",
		"*.dpk", NULL);
	ft->comment_open = g_strdup("{");
	ft->comment_close = g_strdup("}");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define ASM
	ft = filetypes[GEANY_FILETYPES_ASM];
	ft->lang = 9;
	ft->name = g_strdup("ASM");
	ft->title = g_strdup_printf(_("%s source file"), "Assembler");
	ft->extension = g_strdup("asm");
	ft->pattern = utils_strv_new("*.asm", NULL);
	ft->comment_open = g_strdup(";");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define BASIC
	ft = filetypes[GEANY_FILETYPES_BASIC];
	ft->lang = 26;
	ft->name = g_strdup("FreeBasic");
	ft->title = g_strdup_printf(_("%s source file"), "FreeBasic");
	ft->extension = g_strdup("bas");
	ft->pattern = utils_strv_new("*.bas", "*.bi", NULL);
	ft->comment_open = g_strdup("'");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define FORTRAN
	ft = filetypes[GEANY_FILETYPES_FORTRAN];
	ft->lang = 18;
	ft->name = g_strdup("Fortran");
	ft->title = g_strdup_printf(_("%s source file"), "Fortran (F90)");
	ft->extension = g_strdup("f90");
	ft->pattern = utils_strv_new("*.f90", "*.f95", "*.f03", NULL);
	ft->comment_open = g_strdup("c");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define F77
	ft = filetypes[GEANY_FILETYPES_F77];
	ft->lang = 30;
	ft->name = g_strdup("F77");
	ft->title = g_strdup_printf(_("%s source file"), "Fortran (F77)");
	ft->extension = g_strdup("f");
	ft->pattern = utils_strv_new("*.f", "*.for", "*.ftn", "*.f77", NULL);
	ft->comment_open = g_strdup("c");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define GLSL
	ft = filetypes[GEANY_FILETYPES_GLSL];
	ft->lang = 31;
	ft->name = g_strdup("GLSL");
	ft->title = g_strdup_printf(_("%s source file"), "GLSL");
	ft->extension = g_strdup("glsl");
	ft->pattern = utils_strv_new("*.glsl", "*.frag", "*.vert", NULL);
	ft->comment_open = g_strdup("/*");
	ft->comment_close = g_strdup("*/");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define CAML
	ft = filetypes[GEANY_FILETYPES_CAML];
	ft->lang = -2;
	ft->name = g_strdup("CAML");
	ft->title = g_strdup_printf(_("%s source file"), "(O)Caml");
	ft->extension = g_strdup("ml");
	ft->pattern = utils_strv_new("*.ml", "*.mli", NULL);
	ft->comment_open = g_strdup("(*");
	ft->comment_close = g_strdup("*)");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define PERL
	ft = filetypes[GEANY_FILETYPES_PERL];
	ft->lang = 5;
	ft->name = g_strdup("Perl");
	ft->title = g_strdup_printf(_("%s source file"), "Perl");
	ft->extension = g_strdup("pl");
	ft->pattern = utils_strv_new("*.pl", "*.perl", "*.pm", "*.agi",
		"*.pod", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define PHP
	ft = filetypes[GEANY_FILETYPES_PHP];
	ft->lang = 6;
	ft->name = g_strdup("PHP");
	ft->title = g_strdup_printf(_("%s source file"), "PHP");
	ft->extension = g_strdup("php");
	ft->pattern = utils_strv_new("*.php", "*.php3", "*.php4", "*.php5",
		"*.phtml", NULL);
	ft->comment_open = g_strdup("//");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define JAVASCRIPT
	ft = filetypes[GEANY_FILETYPES_JS];
	ft->lang = 23;
	ft->name = g_strdup("Javascript");
	ft->title = g_strdup_printf(_("%s source file"), "Javascript");
	ft->extension = g_strdup("js");
	ft->pattern = utils_strv_new("*.js", NULL);
	ft->comment_open = g_strdup("//");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define PYTHON
	ft = filetypes[GEANY_FILETYPES_PYTHON];
	ft->lang = 7;
	ft->name = g_strdup("Python");
	ft->title = g_strdup_printf(_("%s source file"), "Python");
	ft->extension = g_strdup("py");
	ft->pattern = utils_strv_new("*.py", "*.pyw", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define RUBY
	ft = filetypes[GEANY_FILETYPES_RUBY];
	ft->lang = 14;
	ft->name = g_strdup("Ruby");
	ft->title = g_strdup_printf(_("%s source file"), "Ruby");
	ft->extension = g_strdup("rb");
	ft->pattern = utils_strv_new("*.rb", "*.rhtml", "*.ruby", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define TCL
	ft = filetypes[GEANY_FILETYPES_TCL];
	ft->lang = 15;
	ft->name = g_strdup("Tcl");
	ft->title = g_strdup_printf(_("%s source file"), "Tcl");
	ft->extension = g_strdup("tcl");
	ft->pattern = utils_strv_new("*.tcl", "*.tk", "*.wish", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define LUA
	ft = filetypes[GEANY_FILETYPES_LUA];
	ft->lang = 22;
	ft->name = g_strdup("Lua");
	ft->title = g_strdup_printf(_("%s source file"), "Lua");
	ft->extension = g_strdup("lua");
	ft->pattern = utils_strv_new("*.lua", NULL);
	ft->comment_open = g_strdup("--");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define FERITE
	ft = filetypes[GEANY_FILETYPES_FERITE];
	ft->lang = 19;
	ft->name = g_strdup("Ferite");
	ft->title = g_strdup_printf(_("%s source file"), "Ferite");
	ft->extension = g_strdup("fe");
	ft->pattern = utils_strv_new("*.fe", NULL);
	ft->comment_open = g_strdup("/*");
	ft->comment_close = g_strdup("*/");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define HASKELL
	ft = filetypes[GEANY_FILETYPES_HASKELL];
	ft->lang = 24;
	ft->name = g_strdup("Haskell");
	ft->title = g_strdup_printf(_("%s source file"), "Haskell");
	ft->extension = g_strdup("hs");
	ft->pattern = utils_strv_new("*.hs", "*.lhs", NULL);
	ft->comment_open = g_strdup("--");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define SH
	ft = filetypes[GEANY_FILETYPES_SH];
	ft->lang = 16;
	ft->name = g_strdup("Sh");
	ft->title = g_strdup(_("Shell script file"));
	ft->extension = g_strdup("sh");
	ft->pattern = utils_strv_new("*.sh", "configure", "configure.in",
		"configure.in.in", "configure.ac", "*.ksh", "*.zsh", "*.ash", "*.bash", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define MAKE
	ft = filetypes[GEANY_FILETYPES_MAKE];
	ft->lang = 3;
	ft->name = g_strdup("Make");
	ft->title = g_strdup(_("Makefile"));
	ft->extension = g_strdup("mak");
	ft->pattern = utils_strv_new(
		"*.mak", "*.mk", "GNUmakefile", "makefile", "Makefile", "makefile.*", "Makefile.*", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define XML
	ft = filetypes[GEANY_FILETYPES_XML];
	ft->lang = -2;
	ft->name = g_strdup("XML");
	ft->title = g_strdup(_("XML document"));
	ft->extension = g_strdup("xml");
	ft->pattern = utils_strv_new(
		"*.xml", "*.sgml", "*.xsl", "*.xslt", "*.xsd", "*.xhtml", NULL);
	ft->comment_open = g_strdup("<!--");
	ft->comment_close = g_strdup("-->");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;

#define DOCBOOK
	ft = filetypes[GEANY_FILETYPES_DOCBOOK];
	ft->lang = 12;
	ft->name = g_strdup("Docbook");
	ft->title = g_strdup_printf(_("%s source file"), "Docbook");
	ft->extension = g_strdup("docbook");
	ft->pattern = utils_strv_new("*.docbook", NULL);
	ft->comment_open = g_strdup("<!--");
	ft->comment_close = g_strdup("-->");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;

#define HTML
	ft = filetypes[GEANY_FILETYPES_HTML];
	ft->lang = 29;
	ft->name = g_strdup("HTML");
	ft->title = g_strdup_printf(_("%s source file"), "HTML");
	ft->extension = g_strdup("html");
	ft->pattern = utils_strv_new(
		"*.htm", "*.html", "*.shtml", "*.hta", "*.htd", "*.htt", "*.cfm", NULL);
	ft->comment_open = g_strdup("<!--");
	ft->comment_close = g_strdup("-->");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;

#define CSS
	ft = filetypes[GEANY_FILETYPES_CSS];
	ft->lang = 13;
	ft->name = g_strdup("CSS");
	ft->title = g_strdup(_("Cascading StyleSheet"));
	ft->extension = g_strdup("css");
	ft->pattern = utils_strv_new("*.css", NULL);
	ft->comment_open = g_strdup("/*");
	ft->comment_close = g_strdup("*/");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;	/* not really markup but fit quite well to HTML */

#define SQL
	ft = filetypes[GEANY_FILETYPES_SQL];
	ft->lang = 11;
	ft->name = g_strdup("SQL");
	ft->title = g_strdup(_("SQL Dump file"));
	ft->extension = g_strdup("sql");
	ft->pattern = utils_strv_new("*.sql", NULL);
	ft->comment_open = g_strdup("/*");
	ft->comment_close = g_strdup("*/");
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define LATEX
	ft = filetypes[GEANY_FILETYPES_LATEX];
	ft->lang = 8;
	ft->name = g_strdup("LaTeX");
	ft->title = g_strdup_printf(_("%s source file"), "LaTeX");
	ft->extension = g_strdup("tex");
	ft->pattern = utils_strv_new("*.tex", "*.sty", "*.idx", "*.ltx", NULL);
	ft->comment_open = g_strdup("%");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define OMS
	ft = filetypes[GEANY_FILETYPES_OMS];
	ft->lang = -2;
	ft->name = g_strdup("O-Matrix");
	ft->title = g_strdup_printf(_("%s source file"), "O-Matrix");
	ft->extension = g_strdup("oms");
	ft->pattern = utils_strv_new("*.oms", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define VHDL
	ft = filetypes[GEANY_FILETYPES_VHDL];
	ft->lang = 21;
	ft->name = g_strdup("VHDL");
	ft->title = g_strdup_printf(_("%s source file"), "VHDL");
	ft->extension = g_strdup("vhd");
	ft->pattern = utils_strv_new("*.vhd", "*.vhdl", NULL);
	ft->comment_open = g_strdup("--");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define DIFF
	ft = filetypes[GEANY_FILETYPES_DIFF];
	ft->lang = 20;
	ft->name = g_strdup("Diff");
	ft->title = g_strdup(_("Diff file"));
	ft->extension = g_strdup("diff");
	ft->pattern = utils_strv_new("*.diff", "*.patch", "*.rej", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define CONF
	ft = filetypes[GEANY_FILETYPES_CONF];
	ft->lang = 10;
	ft->name = g_strdup("Conf");
	ft->title = g_strdup(_("Config file"));
	ft->extension = g_strdup("conf");
	ft->pattern = utils_strv_new("*.conf", "*.ini", "config", "*rc",
		"*.cfg", NULL);
	ft->comment_open = g_strdup("#");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define HAXE
	ft = filetypes[GEANY_FILETYPES_HAXE];
	ft->lang = 27;
	ft->name = g_strdup("Haxe");
	ft->title = g_strdup_printf(_("%s source file"), "Haxe");
	ft->extension = g_strdup("hx");
	ft->pattern = utils_strv_new("*.hx", NULL);
	ft->comment_open = g_strdup("//");
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define REST
	ft = filetypes[GEANY_FILETYPES_REST];
	ft->lang = 28;
	ft->name = g_strdup("reStructuredText");
	ft->title = g_strdup(_("reStructuredText file"));
	ft->extension = g_strdup("rst");
	ft->pattern = utils_strv_new(
		"*.rest", "*.reST", "*.rst", NULL);
	ft->comment_open = NULL;
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define ALL
	ft = filetypes[GEANY_FILETYPES_NONE];
	ft->lang = -2;
	ft->name = g_strdup("None");
	ft->title = g_strdup(_("All files"));
	ft->extension = g_strdup("*");
	ft->pattern = utils_strv_new("*", NULL);
	ft->comment_open = NULL;
	ft->comment_close = NULL;
	ft->group = GEANY_FILETYPE_GROUP_NONE;
}


/* initialize fields. */
static GeanyFiletype *filetype_new(void)
{
	GeanyFiletype *ft = g_new0(GeanyFiletype, 1);

	ft->lang = -2;	/* assume no tagmanager parser */
	ft->programs = g_new0(struct build_programs, 1);
	ft->actions = g_new0(struct build_actions, 1);

	ft->priv = g_new0(GeanyFiletypePrivate, 1);
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
		switch (ft->group)
		{
			case GEANY_FILETYPE_GROUP_COMPILED:	/* programming */
				sub_menu = sub_menu_programming;
				break;

			case GEANY_FILETYPE_GROUP_SCRIPT:	/* scripts */
				sub_menu = sub_menu_scripts;
				break;

			case GEANY_FILETYPE_GROUP_MARKUP:	/* markup */
				sub_menu = sub_menu_markup;
				break;

			case GEANY_FILETYPE_GROUP_MISC:	/* misc */
				sub_menu = sub_menu_misc;
				break;

			case GEANY_FILETYPE_GROUP_NONE:	/* none */
				sub_menu = filetype_menu;
				title = _("None");
				break;

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


/* Detect the filetype for the document, checking for a shebang, then filename extension. */
GeanyFiletype *filetypes_detect_from_document(GeanyDocument *doc)
{
	GeanyFiletype *ft;
	gchar *line;

	if (doc == NULL)
		return filetypes[GEANY_FILETYPES_NONE];

	line = sci_get_line(doc->editor->sci, 0);
	ft = filetypes_detect_from_file_internal(doc->file_name, line);
	g_free(line);
	return ft;
}


#ifdef HAVE_PLUGINS
/* Currently only used by external plugins (e.g. geanyprj). */
/** Detect filetype based on a shebang line in the file, or the filename extension. */
GeanyFiletype *filetypes_detect_from_file(const gchar *utf8_filename)
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
#endif


void filetypes_select_radio_item(const GeanyFiletype *ft)
{
	/* ignore_callback has to be set by the caller */
	g_return_if_fail(ignore_callback);

	if (ft == NULL)
		ft = filetypes[GEANY_FILETYPES_NONE];

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ft->priv->menu_item), TRUE);
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

	tmp = gtk_radio_menu_item_new_with_label(group, label);
	group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(tmp));
	ftype->priv->menu_item = tmp;
	gtk_widget_show(tmp);
	gtk_container_add(GTK_CONTAINER(menu), tmp);
	g_signal_connect(tmp, "activate", G_CALLBACK(on_filetype_change), (gpointer) ftype);
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
	GeanyFiletypePrivate *pft;

	g_return_if_fail(ft_id >= 0 && ft_id < (gint) filetypes_array->len);

	pft = filetypes[ft_id]->priv;

	/* when reloading, proceed only if the settings were already loaded */
	if (reload && ! pft->keyfile_loaded)
		return;

	/* when not reloading, load the settings only once */
	if (! reload && pft->keyfile_loaded)
		return;
	pft->keyfile_loaded = TRUE;

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


/** Find a filetype pointer from its @c name field.
 * @param name Filetype name.
 * @return The filetype found, or @c NULL. */
GeanyFiletype *filetypes_lookup_by_name(const gchar *name)
{
	GeanyFiletype *ft;

	g_return_val_if_fail(NZV(name), NULL);

	ft = g_hash_table_lookup(filetypes_hash, name);
	if (ft == NULL)
		geany_debug("Could not find filetype '%s'.", name);
	return ft;
}


