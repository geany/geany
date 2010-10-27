/*
 *      filetypes.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* Note: we use filetype_id for some function arguments, but GeanyFiletype is better; we should
 * only use GeanyFiletype for API functions. */

#include <string.h>
#include <glib/gstdio.h>

#include "geany.h"
#include "filetypes.h"
#include "filetypesprivate.h"
#include "highlighting.h"
#include "support.h"
#include "templates.h"
#include "document.h"
#include "editor.h"
#include "msgwindow.h"
#include "utils.h"
#include "sciwrappers.h"
#include "ui_utils.h"
#include "symbols.h"

#include <stdlib.h>


GPtrArray *filetypes_array = NULL;	/* Dynamic array of filetype pointers */

static GHashTable *filetypes_hash = NULL;	/* Hash of filetype pointers based on name keys */

/** List of filetype pointers sorted by name, but with @c filetypes_index(GEANY_FILETYPES_NONE)
 * first, as this is usually treated specially.
 * The list does not change (after filetypes have been initialized), so you can use
 * @code g_slist_nth_data(filetypes_by_title, n) @endcode and expect the same result at different times. */
GSList *filetypes_by_title = NULL;


static void create_radio_menu_item(GtkWidget *menu, GeanyFiletype *ftype);


enum TitleType
{
	TITLE_SOURCE_FILE,
	TITLE_FILE
};

/* Save adding many translation strings if the filetype name doesn't need translating */
static void filetype_make_title(GeanyFiletype *ft, enum TitleType type)
{
	const gchar *fmt = NULL;

	switch (type)
	{
		default:
		case TITLE_SOURCE_FILE:	fmt = _("%s source file"); break;
		case TITLE_FILE:		fmt = _("%s file"); break;
	}
	g_assert(!ft->title);
	g_assert(ft->name);
	ft->title = g_strdup_printf(fmt, ft->name);
}


/* Note: remember to update HACKING if this function is renamed. */
static void init_builtin_filetypes(void)
{
	GeanyFiletype *ft;

#define NONE	/* these macros are only to ease navigation */
	ft = filetypes[GEANY_FILETYPES_NONE];
	ft->name = g_strdup(_("None"));
	ft->title = g_strdup(_("None"));
	ft->group = GEANY_FILETYPE_GROUP_NONE;

#define C
	ft = filetypes[GEANY_FILETYPES_C];
	ft->lang = 0;
	ft->name = g_strdup("C");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-csrc");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define CPP
	ft = filetypes[GEANY_FILETYPES_CPP];
	ft->lang = 1;
	ft->name = g_strdup("C++");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-c++src");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define CS
	ft = filetypes[GEANY_FILETYPES_CS];
	ft->lang = 25;
	ft->name = g_strdup("C#");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-csharp");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define VALA
	ft = filetypes[GEANY_FILETYPES_VALA];
	ft->lang = 33;
	ft->name = g_strdup("Vala");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-vala");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define D
	ft = filetypes[GEANY_FILETYPES_D];
	ft->lang = 17;
	ft->name = g_strdup("D");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-dsrc");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define JAVA
	ft = filetypes[GEANY_FILETYPES_JAVA];
	ft->lang = 2;
	ft->name = g_strdup("Java");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-java");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define PAS /* to avoid warnings when building under Windows, the symbol PASCAL is there defined */
	ft = filetypes[GEANY_FILETYPES_PASCAL];
	ft->lang = 4;
	ft->name = g_strdup("Pascal");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-pascal");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define ASM
	ft = filetypes[GEANY_FILETYPES_ASM];
	ft->lang = 9;
	ft->name = g_strdup("ASM");
	ft->title = g_strdup_printf(_("%s source file"), "Assembler");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define BASIC
	ft = filetypes[GEANY_FILETYPES_BASIC];
	ft->lang = 26;
	ft->name = g_strdup("FreeBasic");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define FORTRAN
	ft = filetypes[GEANY_FILETYPES_FORTRAN];
	ft->lang = 18;
	ft->name = g_strdup("Fortran");
	ft->title = g_strdup_printf(_("%s source file"), "Fortran (F90)");
	ft->mime_type = g_strdup("text/x-fortran");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define F77
	ft = filetypes[GEANY_FILETYPES_F77];
	ft->lang = 30;
	ft->name = g_strdup("F77");
	ft->title = g_strdup_printf(_("%s source file"), "Fortran (F77)");
	ft->mime_type = g_strdup("text/x-fortran");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define GLSL
	ft = filetypes[GEANY_FILETYPES_GLSL];
	ft->lang = 31;
	ft->name = g_strdup("GLSL");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define CAML
	ft = filetypes[GEANY_FILETYPES_CAML];
	ft->name = g_strdup("CAML");
	ft->title = g_strdup_printf(_("%s source file"), "(O)Caml");
	ft->mime_type = g_strdup("text/x-ocaml");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define PERL
	ft = filetypes[GEANY_FILETYPES_PERL];
	ft->lang = 5;
	ft->name = g_strdup("Perl");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("application/x-perl");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define PHP
	ft = filetypes[GEANY_FILETYPES_PHP];
	ft->lang = 6;
	ft->name = g_strdup("PHP");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("application/x-php");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define JAVASCRIPT
	ft = filetypes[GEANY_FILETYPES_JS];
	ft->lang = 23;
	ft->name = g_strdup("Javascript");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("application/javascript");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define PYTHON
	ft = filetypes[GEANY_FILETYPES_PYTHON];
	ft->lang = 7;
	ft->name = g_strdup("Python");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-python");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define RUBY
	ft = filetypes[GEANY_FILETYPES_RUBY];
	ft->lang = 14;
	ft->name = g_strdup("Ruby");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("application/x-ruby");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define TCL
	ft = filetypes[GEANY_FILETYPES_TCL];
	ft->lang = 15;
	ft->name = g_strdup("Tcl");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-tcl");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define LUA
	ft = filetypes[GEANY_FILETYPES_LUA];
	ft->lang = 22;
	ft->name = g_strdup("Lua");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-lua");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define FERITE
	ft = filetypes[GEANY_FILETYPES_FERITE];
	ft->lang = 19;
	ft->name = g_strdup("Ferite");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define HASKELL
	ft = filetypes[GEANY_FILETYPES_HASKELL];
	ft->lang = 24;
	ft->name = g_strdup("Haskell");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-haskell");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define MARKDOWN
	ft = filetypes[GEANY_FILETYPES_MARKDOWN];
	ft->lang = 36;
	ft->name = g_strdup("Markdown");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define TXT2TAGS
	ft = filetypes[GEANY_FILETYPES_TXT2TAGS];
	ft->lang = 37;
	ft->name = g_strdup("Txt2tags");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-txt2tags");
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define ABC
	ft = filetypes[GEANY_FILETYPES_ABC];
	ft->lang = 38;
	ft->name = g_strdup("Abc");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define SH
	ft = filetypes[GEANY_FILETYPES_SH];
	ft->lang = 16;
	ft->name = g_strdup("Sh");
	ft->title = g_strdup(_("Shell script file"));
	ft->mime_type = g_strdup("application/x-shellscript");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define MAKE
	ft = filetypes[GEANY_FILETYPES_MAKE];
	ft->lang = 3;
	ft->name = g_strdup("Make");
	ft->title = g_strdup(_("Makefile"));
	ft->mime_type = g_strdup("text/x-makefile");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define XML
	ft = filetypes[GEANY_FILETYPES_XML];
	ft->name = g_strdup("XML");
	ft->title = g_strdup(_("XML document"));
	ft->mime_type = g_strdup("application/xml");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;

#define DOCBOOK
	ft = filetypes[GEANY_FILETYPES_DOCBOOK];
	ft->lang = 12;
	ft->name = g_strdup("Docbook");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("application/docbook+xml");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;

#define HTML
	ft = filetypes[GEANY_FILETYPES_HTML];
	ft->lang = 29;
	ft->name = g_strdup("HTML");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/html");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;

#define CSS
	ft = filetypes[GEANY_FILETYPES_CSS];
	ft->lang = 13;
	ft->name = g_strdup("CSS");
	ft->title = g_strdup(_("Cascading StyleSheet"));
	ft->mime_type = g_strdup("text/css");
	ft->group = GEANY_FILETYPE_GROUP_MARKUP;	/* not really markup but fit quite well to HTML */

#define SQL
	ft = filetypes[GEANY_FILETYPES_SQL];
	ft->lang = 11;
	ft->name = g_strdup("SQL");
	ft->title = g_strdup(_("SQL Dump file"));
	ft->mime_type = g_strdup("text/x-sql");
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define LATEX
	ft = filetypes[GEANY_FILETYPES_LATEX];
	ft->lang = 8;
	ft->name = g_strdup("LaTeX");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-tex");
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define VHDL
	ft = filetypes[GEANY_FILETYPES_VHDL];
	ft->lang = 21;
	ft->name = g_strdup("VHDL");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-vhdl");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define VERILOG
	ft = filetypes[GEANY_FILETYPES_VERILOG];
	ft->lang = 39;
	ft->name = g_strdup("Verilog");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define DIFF
	ft = filetypes[GEANY_FILETYPES_DIFF];
	ft->lang = 20;
	ft->name = g_strdup("Diff");
	filetype_make_title(ft, TITLE_FILE);
	ft->mime_type = g_strdup("text/x-patch");
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define LISP
	ft = filetypes[GEANY_FILETYPES_LISP];
	ft->name = g_strdup("Lisp");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define ERLANG
	ft = filetypes[GEANY_FILETYPES_ERLANG];
	ft->name = g_strdup("Erlang");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-erlang");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define CONF
	ft = filetypes[GEANY_FILETYPES_CONF];
	ft->lang = 10;
	ft->name = g_strdup("Conf");
	ft->title = g_strdup(_("Config file"));
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define PO
	ft = filetypes[GEANY_FILETYPES_PO];
	ft->name = g_strdup("Po");
	ft->title = g_strdup(_("Gettext translation file"));
	ft->mime_type = g_strdup("text/x-gettext-translation");
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define HAXE
	ft = filetypes[GEANY_FILETYPES_HAXE];
	ft->lang = 27;
	ft->name = g_strdup("Haxe");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define ACTIONSCRIPT
	ft = filetypes[GEANY_FILETYPES_AS];
	ft->lang = 34;
	ft->name = g_strdup("ActionScript");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("application/ecmascript");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define R
	ft = filetypes[GEANY_FILETYPES_R];
	ft->lang = 40;
	ft->name = g_strdup("R");
	ft->title = g_strdup_printf(_("%s script file"), "R");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define REST
	ft = filetypes[GEANY_FILETYPES_REST];
	ft->lang = 28;
	ft->name = g_strdup("reStructuredText");
	filetype_make_title(ft, TITLE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define MATLAB
	ft = filetypes[GEANY_FILETYPES_MATLAB];
	ft->lang = 32;
	ft->name = g_strdup("Matlab/Octave");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-matlab");
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;

#define YAML
	ft = filetypes[GEANY_FILETYPES_YAML];
	ft->name = g_strdup("YAML");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define CMAKE
	ft = filetypes[GEANY_FILETYPES_CMAKE];
	ft->name = g_strdup("CMake");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-cmake");
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define NSIS
	ft = filetypes[GEANY_FILETYPES_NSIS];
	ft->lang = 35;
	ft->name = g_strdup("NSIS");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_MISC;

#define ADA
	ft = filetypes[GEANY_FILETYPES_ADA];
	ft->name = g_strdup("Ada");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->mime_type = g_strdup("text/x-adasrc");
	ft->group = GEANY_FILETYPE_GROUP_COMPILED;

#define FORTH
	ft = filetypes[GEANY_FILETYPES_FORTH];
	ft->name = g_strdup("Forth");
	filetype_make_title(ft, TITLE_SOURCE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_SCRIPT;
}


/* initialize fields. */
static GeanyFiletype *filetype_new(void)
{
	GeanyFiletype *ft = g_new0(GeanyFiletype, 1);

	ft->lang = -2;	/* assume no tagmanager parser */
	/* pattern must not be null */
	ft->pattern = g_new0(gchar*, 1);
	ft->project_list_entry = -1; /* no entry */

	ft->priv = g_new0(GeanyFiletypePrivate, 1);
	return ft;
}


static gint cmp_filetype(gconstpointer pft1, gconstpointer pft2)
{
	const GeanyFiletype *ft1 = pft1, *ft2 = pft2;

	if (G_UNLIKELY(ft1->id == GEANY_FILETYPES_NONE))
		return -1;
	if (G_UNLIKELY(ft2->id == GEANY_FILETYPES_NONE))
		return 1;

	return utils_str_casecmp(ft1->title, ft2->title);
}


/* Add a filetype pointer to the lists of available filetypes,
 * and set the filetype::id field. */
static void filetype_add(GeanyFiletype *ft)
{
	g_return_if_fail(ft);
	g_return_if_fail(ft->name);

	ft->id = filetypes_array->len;	/* len will be the index for filetype_array */
	g_ptr_array_add(filetypes_array, ft);
	g_hash_table_insert(filetypes_hash, ft->name, ft);

	/* list will be sorted later */
	filetypes_by_title = g_slist_prepend(filetypes_by_title, ft);

	if (!ft->mime_type)
		ft->mime_type = g_strdup("text/plain");

	ft->icon = ui_get_mime_icon(ft->mime_type, GTK_ICON_SIZE_MENU);
}


static void add_custom_filetype(const gchar *filename)
{
	gchar *fn = utils_strdupa(strstr(filename, ".") + 1);
	gchar *dot = g_strrstr(fn, ".conf");
	GeanyFiletype *ft;

	g_return_if_fail(dot);

	*dot = 0x0;

	if (g_hash_table_lookup(filetypes_hash, fn))
		return;

	ft = filetype_new();
	ft->name = g_strdup(fn);
	filetype_make_title(ft, TITLE_FILE);
	ft->group = GEANY_FILETYPE_GROUP_CUSTOM;
	ft->priv->custom = TRUE;
	filetype_add(ft);
	geany_debug("Added filetype %s (%d).", ft->name, ft->id);
}


static void init_custom_filetypes(const gchar *path)
{
	GDir *dir;
	const gchar *filename;

	g_return_if_fail(path);

	dir = g_dir_open(path, 0, NULL);
	if (dir == NULL)
		return;

	foreach_dir(filename, dir)
	{
		const gchar prefix[] = "filetypes.";

		if (g_str_has_prefix(filename, prefix) &&
			g_str_has_suffix(filename + strlen(prefix), ".conf"))
		{
			add_custom_filetype(filename);
		}
	}
	g_dir_close(dir);
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
	init_custom_filetypes(app->datadir);
	init_custom_filetypes(utils_build_path(app->configdir, GEANY_FILEDEFS_SUBDIR, NULL));

	/* sort last instead of on insertion to prevent exponential time */
	filetypes_by_title = g_slist_sort(filetypes_by_title, cmp_filetype);
}


static void on_document_save(G_GNUC_UNUSED GObject *object, GeanyDocument *doc)
{
	g_return_if_fail(NZV(doc->real_path));

	if (utils_str_equal(doc->real_path,
		utils_build_path(app->configdir, "filetype_extensions.conf", NULL)))
		filetypes_read_extensions();
	else if (utils_str_equal(doc->real_path,
		utils_build_path(app->configdir, GEANY_FILEDEFS_SUBDIR, "filetypes.common", NULL)))
	{
		guint i;

		/* Note: we don't reload other filetypes, even though the named styles may have changed.
		 * The user can do this manually with 'Tools->Reload Configuration' */
		filetypes_load_config(GEANY_FILETYPES_NONE, TRUE);

		foreach_document(i)
			document_reload_config(documents[i]);
	}
}


static void setup_config_file_menus(void)
{
	ui_add_config_file_menu_item(
		utils_build_path(app->configdir, "filetype_extensions.conf", NULL), NULL, NULL);
	ui_add_config_file_menu_item(
		utils_build_path(app->configdir, GEANY_FILEDEFS_SUBDIR, "filetypes.common", NULL), NULL, NULL);

	g_signal_connect(geany_object, "document-save", G_CALLBACK(on_document_save), NULL);
}


static GtkWidget *group_menus[GEANY_FILETYPE_GROUP_COUNT] = {NULL};

static void create_sub_menu(GtkWidget *parent, gsize group_id, const gchar *title)
{
	GtkWidget *menu, *item;

	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_mnemonic((title));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
	gtk_container_add(GTK_CONTAINER(parent), item);
	gtk_widget_show(item);
	group_menus[group_id] = menu;
}


static void create_set_filetype_menu(void)
{
	GSList *node;
	GtkWidget *filetype_menu = ui_lookup_widget(main_widgets.window, "set_filetype1_menu");

	create_sub_menu(filetype_menu, GEANY_FILETYPE_GROUP_COMPILED, _("_Programming Languages"));
	create_sub_menu(filetype_menu, GEANY_FILETYPE_GROUP_SCRIPT, _("_Scripting Languages"));
	create_sub_menu(filetype_menu, GEANY_FILETYPE_GROUP_MARKUP, _("_Markup Languages"));
	create_sub_menu(filetype_menu, GEANY_FILETYPE_GROUP_MISC, _("M_iscellaneous Languages"));
	create_sub_menu(filetype_menu, GEANY_FILETYPE_GROUP_CUSTOM, _("_Custom Filetypes"));

	/* Append all filetypes to the filetype menu */
	foreach_slist(node, filetypes_by_title)
	{
		GeanyFiletype *ft = node->data;

		if (ft->group != GEANY_FILETYPE_GROUP_NONE)
			create_radio_menu_item(group_menus[ft->group], ft);
		else
			create_radio_menu_item(filetype_menu, ft);
	}
}


void filetypes_init()
{
	filetypes_init_types();

	create_set_filetype_menu();
	setup_config_file_menus();
}


/* Find a filetype that predicate returns TRUE for, otherwise return NULL. */
GeanyFiletype *filetypes_find(GCompareFunc predicate, gpointer user_data)
{
	guint i;

	for (i = 0; i < filetypes_array->len; i++)
	{
		GeanyFiletype *ft = filetypes[i];

		if (predicate(ft, user_data))
			return ft;
	}
	return NULL;
}


static gboolean match_basename(gconstpointer pft, gconstpointer user_data)
{
	const GeanyFiletype *ft = pft;
	const gchar *base_filename = user_data;
	gint j;
	gboolean ret = FALSE;

	if (G_UNLIKELY(ft->id == GEANY_FILETYPES_NONE))
		return FALSE;

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

	ft = filetypes_find(match_basename, base_filename);
	if (ft == NULL)
		ft = filetypes[GEANY_FILETYPES_NONE];

	g_free(base_filename);
	return ft;
}


/* This detects the filetype of the file pointed by 'utf8_filename' and a list of filetype id's,
 * terminated by -1.
 * The detected filetype of the file is checked against every id in the passed list and if
 * there is a match, TRUE is returned. */
static gboolean shebang_find_and_match_filetype(const gchar *utf8_filename, gint first, ...)
{
	GeanyFiletype *ft = NULL;
	gint test;
	gboolean result = FALSE;
	va_list args;

	ft = filetypes_detect_from_extension(utf8_filename);
	if (ft == NULL || ft->id >= filetypes_array->len)
		return FALSE;

	va_start(args, first);
	test = first;
	while (1)
	{
		if (test == -1)
			break;

		if (ft->id == (guint) test)
		{
			result = TRUE;
			break;
		}
		test = va_arg(args, gint);
	}
	va_end(args);

	return result;
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
			basename_interpreter += 4;
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
	/* detect HTML files */
	if (strncmp(line, "<!DOCTYPE html", 14) == 0 || strncmp(line, "<html", 5) == 0)
	{
		/* PHP, Perl and Python files might also start with <html, so detect them based on filename
		 * extension and use the detected filetype, else assume HTML */
		if (! shebang_find_and_match_filetype(utf8_filename,
				GEANY_FILETYPES_PERL, GEANY_FILETYPES_PHP, GEANY_FILETYPES_PYTHON, -1))
		{
			ft = filetypes[GEANY_FILETYPES_HTML];
		}
	}
	/* detect XML files */
	else if (utf8_filename && strncmp(line, "<?xml", 5) == 0)
	{
		/* HTML and DocBook files might also start with <?xml, so detect them based on filename
		 * extension and use the detected filetype, else assume XML */
		if (! shebang_find_and_match_filetype(utf8_filename,
				GEANY_FILETYPES_HTML, GEANY_FILETYPES_DOCBOOK,
				/* Perl, Python and PHP only to be safe */
				GEANY_FILETYPES_PERL, GEANY_FILETYPES_PHP, GEANY_FILETYPES_PYTHON, -1))
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
/**
 *  Detects filetype based on a shebang line in the file or the filename extension.
 *
 *  @param utf8_filename The filename in UTF-8 encoding.
 *
 *  @return The detected filetype for @a utf8_filename or @c filetypes[GEANY_FILETYPES_NONE]
 *          if it could not be detected.
 **/
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
on_filetype_change                     (GtkCheckMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GeanyDocument *doc = document_get_current();
	if (ignore_callback || doc == NULL || ! gtk_check_menu_item_get_active(menuitem))
		return;

	document_set_filetype(doc, (GeanyFiletype*)user_data);
}


static void create_radio_menu_item(GtkWidget *menu, GeanyFiletype *ftype)
{
	static GSList *group = NULL;
	GtkWidget *tmp;

	tmp = gtk_radio_menu_item_new_with_label(group, ftype->title);
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


static void set_error_regex(GeanyFiletype *ft, gchar *string)
{
	setptr(ft->error_regex_string, string);

	if (ft->priv->error_regex_compiled)
		regfree(&ft->priv->error_regex);

	ft->priv->error_regex_compiled = FALSE;
	/* regex will be compiled when needed */
}


static void filetype_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	GeanyFiletype *ft = data;

	g_return_if_fail(ft != NULL);

	g_free(ft->name);
	g_free(ft->title);
	g_free(ft->extension);
	g_free(ft->mime_type);
	g_free(ft->comment_open);
	g_free(ft->comment_close);
	g_free(ft->context_action_cmd);
	g_free(ft->filecmds);
	g_free(ft->ftdefcmds);
	g_free(ft->execcmds);
	set_error_regex(ft, NULL);
	if (ft->icon)
		g_object_unref(ft->icon);

	g_strfreev(ft->pattern);
	g_free(ft->priv);
	g_free(ft);
}


/* frees the array and all related pointers */
void filetypes_free_types(void)
{
	g_return_if_fail(filetypes_array != NULL);
	g_return_if_fail(filetypes_hash != NULL);

	g_ptr_array_foreach(filetypes_array, filetype_free, NULL);
	g_ptr_array_free(filetypes_array, TRUE);
	g_hash_table_destroy(filetypes_hash);
}


static void load_settings(gint ft_id, GKeyFile *config, GKeyFile *configh)
{
	GeanyFiletype *ft = filetypes[ft_id];
	gchar *result;
	GError *error = NULL;
	gboolean tmp;

	/* default extension */
	result = g_key_file_get_string(configh, "settings", "extension", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "extension", NULL);
	if (G_LIKELY(result != NULL))
	{
		setptr(filetypes[ft_id]->extension, result);
	}

	/* read comment notes */
	result = g_key_file_get_string(configh, "settings", "comment_open", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "comment_open", NULL);
	if (G_LIKELY(result != NULL))
	{
		g_free(filetypes[ft_id]->comment_open);
		filetypes[ft_id]->comment_open = result;
	}

	result = g_key_file_get_string(configh, "settings", "comment_close", NULL);
	if (result == NULL) result = g_key_file_get_string(config, "settings", "comment_close", NULL);
	if (G_LIKELY(result != NULL))
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
	if (G_LIKELY(result != NULL))
	{
		filetypes[ft_id]->context_action_cmd = result;
	}

	result = utils_get_setting_string(configh, "settings", "tag_parser", NULL);
	if (!result)
		result = utils_get_setting_string(config, "settings", "tag_parser", NULL);
	if (result)
	{
		ft->lang = tm_source_file_get_named_lang(result);
		if (ft->lang < 0)
			geany_debug("Cannot find tag parser '%s' for custom filetype '%s'.", result, ft->name);
		g_free(result);
	}

	result = utils_get_setting_string(configh, "settings", "lexer_filetype", NULL);
	if (!result)
		result = utils_get_setting_string(config, "settings", "lexer_filetype", NULL);
	if (result)
	{
		ft->lexer_filetype = filetypes_lookup_by_name(result);
		if (!ft->lexer_filetype)
			geany_debug("Cannot find lexer filetype '%s' for custom filetype '%s'.", result, ft->name);
		g_free(result);
	}

	ft->priv->symbol_list_sort_mode = utils_get_setting(integer, configh, config, "settings",
		"symbol_list_sort_mode", SYMBOLS_SORT_BY_NAME);

	/* read build settings */
	build_load_menu(config, GEANY_BCS_FT, (gpointer)ft);
	build_load_menu(configh, GEANY_BCS_HOME_FT, (gpointer)ft);
}


/* simple wrapper function to print file errors in DEBUG mode */
static void load_system_keyfile(GKeyFile *key_file, const gchar *file, GKeyFileFlags flags,
		GeanyFiletype *ft)
{
	GError *error = NULL;
	gboolean done = g_key_file_load_from_file(key_file, file, flags, &error);

	if (error != NULL)
	{
		if (!done && !ft->priv->custom)
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
	GeanyFiletype *ft;

	g_return_if_fail(ft_id >= 0 && ft_id < (gint) filetypes_array->len);

	ft = filetypes[ft_id];
	pft = ft->priv;

	/* when reloading, proceed only if the settings were already loaded */
	if (reload && G_UNLIKELY(! pft->keyfile_loaded))
		return;

	/* when not reloading, load the settings only once */
	if (! reload && G_LIKELY(pft->keyfile_loaded))
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

		load_system_keyfile(config, f0, G_KEY_FILE_KEEP_COMMENTS, ft);
		g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);

		g_free(ext);
		g_free(f);
		g_free(f0);
	}

	load_settings(ft_id, config, config_home);
	highlighting_init_styles(ft_id, config, config_home);

	g_key_file_free(config);
	g_key_file_free(config_home);
}


gchar *filetypes_get_conf_extension(gint filetype_idx)
{
	gchar *result, *ptr;
	GeanyFiletype *ft = filetypes[filetype_idx];

	if (ft->priv->custom)
		return g_strconcat(ft->name, ".conf", NULL);

	/* Handle any special extensions different from lowercase filetype->name */
	switch (filetype_idx)
	{
		case GEANY_FILETYPES_CPP: result = g_strdup("cpp"); break;
		case GEANY_FILETYPES_CS: result = g_strdup("cs"); break;
		case GEANY_FILETYPES_MAKE: result = g_strdup("makefile"); break;
		default:
			result = g_ascii_strdown(ft->name, -1);
			/* truncate at slash (e.g. for Matlab/Octave) */
			ptr = strstr(result, "/");
			if (ptr)
				*ptr = 0x0;
			break;
	}
	return result;
}


void filetypes_save_commands(void)
{
	gchar *conf_prefix = g_strconcat(app->configdir,
		G_DIR_SEPARATOR_S GEANY_FILEDEFS_SUBDIR G_DIR_SEPARATOR_S "filetypes.", NULL);
	guint i;

	for (i = 1; i < filetypes_array->len; i++)
	{
		GKeyFile *config_home;
		gchar *fname, *ext, *data;

		if (filetypes[i]->home_save_needed)
		{
			ext = filetypes_get_conf_extension(i);
			fname = g_strconcat(conf_prefix, ext, NULL);
			g_free(ext);
			config_home = g_key_file_new();
			g_key_file_load_from_file(config_home, fname, G_KEY_FILE_KEEP_COMMENTS, NULL);
			build_save_menu(config_home, (gpointer)(filetypes[i]), GEANY_BCS_HOME_FT);
			data = g_key_file_to_data(config_home, NULL, NULL);
			utils_write_file(fname, data);
			g_free(data);
			g_key_file_free(config_home);
			g_free(fname);
		}
	}
	g_free(conf_prefix);
}


/* create one file filter which has each file pattern of each filetype */
GtkFileFilter *filetypes_create_file_filter_all_source(void)
{
	GtkFileFilter *new_filter;
	guint i, j;

	new_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(new_filter, _("All Source"));

	for (i = 0; i < filetypes_array->len; i++)
	{
		if (G_UNLIKELY(i == GEANY_FILETYPES_NONE))
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
	const gchar *title;

	g_return_val_if_fail(ft != NULL, NULL);

	new_filter = gtk_file_filter_new();
	title = ft->id == GEANY_FILETYPES_NONE ? _("All files") : ft->title;
	gtk_file_filter_set_name(new_filter, title);

	for (i = 0; ft->pattern[i]; i++)
	{
		gtk_file_filter_add_pattern(new_filter, ft->pattern[i]);
	}

	return new_filter;
}


/* Indicates whether there is a tag parser for the filetype or not.
 * Only works for custom filetypes if the filetype settings have been loaded. */
gboolean filetype_has_tags(GeanyFiletype *ft)
{
	g_return_val_if_fail(ft != NULL, FALSE);

	return ft->lang >= 0;
}


/** Finds a filetype pointer from its @a name field.
 * @param name Filetype name.
 * @return The filetype found, or @c NULL.
 *
 * @since 0.15
 **/
GeanyFiletype *filetypes_lookup_by_name(const gchar *name)
{
	GeanyFiletype *ft;

	g_return_val_if_fail(NZV(name), NULL);

	ft = g_hash_table_lookup(filetypes_hash, name);
	if (G_UNLIKELY(ft == NULL))
		geany_debug("Could not find filetype '%s'.", name);
	return ft;
}


static gchar *get_regex_match_string(const gchar *message, regmatch_t *pmatch, gint match_idx)
{
	return g_strndup(&message[pmatch[match_idx].rm_so],
		pmatch[match_idx].rm_eo - pmatch[match_idx].rm_so);
}


static void compile_regex(GeanyFiletype *ft, regex_t *regex, gchar *regstr)
{
	gint retval = regcomp(regex, regstr, REG_EXTENDED);

	ft->priv->error_regex_compiled = (retval == 0);	/* prevent recompilation */

	if (G_UNLIKELY(retval != 0))
	{
		gchar buf[256];
		regerror(retval, regex, buf, sizeof buf);
		ui_set_statusbar(TRUE, _("Bad regex for filetype %s: %s"),
			ft->name, buf);
	}
	/* regex will be freed in set_error_regex(). */
}


gboolean filetypes_parse_error_message(GeanyFiletype *ft, const gchar *message,
		gchar **filename, gint *line)
{
	gchar *regstr;
	gchar **tmp;
	GeanyDocument *doc;
	regex_t *regex;
	regmatch_t pmatch[3];

	if (ft == NULL)
	{
		doc = document_get_current();
		if (doc != NULL)
			ft = doc->file_type;
	}
	tmp = build_get_regex(build_info.grp, ft, NULL);
	if (tmp == NULL)
		return FALSE;
	regstr = *tmp;
	regex = &ft->priv->error_regex;

	*filename = NULL;
	*line = -1;

	if (!NZV(regstr))
		return FALSE;

	if (!ft->priv->error_regex_compiled || regstr != ft->priv->last_string)
	{
		compile_regex(ft, regex, regstr);
		ft->priv->last_string = regstr;
	}
	if (!ft->priv->error_regex_compiled)	/* regex error */
		return FALSE;

	if (regexec(regex, message, G_N_ELEMENTS(pmatch), pmatch, 0) != 0)
		return FALSE;

	if (pmatch[0].rm_so != -1 && pmatch[1].rm_so != -1 && pmatch[2].rm_so != -1)
	{
		gchar *first, *second, *end;
		glong l;

		first = get_regex_match_string(message, pmatch, 1);
		second = get_regex_match_string(message, pmatch, 2);
		l = strtol(first, &end, 10);
		if (*end == '\0')	/* first is purely decimals */
		{
			*line = l;
			g_free(first);
			*filename = second;
		}
		else
		{
			l = strtol(second, &end, 10);
			if (*end == '\0')
			{
				*line = l;
				g_free(second);
				*filename = first;
			}
			else
			{
				g_free(first);
				g_free(second);
			}
		}
	}
	return *filename != NULL;
}


#ifdef G_OS_WIN32
static void convert_filetype_extensions_to_lower_case(gchar **patterns, gsize len)
{
	guint i;
	for (i = 0; i < len; i++)
	{
		setptr(patterns[i], g_ascii_strdown(patterns[i], -1));
	}
}
#endif


void filetypes_read_extensions(void)
{
	guint i;
	gsize len = 0;
	gchar *sysconfigfile = g_strconcat(app->datadir, G_DIR_SEPARATOR_S,
		"filetype_extensions.conf", NULL);
	gchar *userconfigfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S,
		"filetype_extensions.conf", NULL);
	GKeyFile *sysconfig = g_key_file_new();
	GKeyFile *userconfig = g_key_file_new();

	g_key_file_load_from_file(sysconfig, sysconfigfile, G_KEY_FILE_NONE, NULL);
	g_key_file_load_from_file(userconfig, userconfigfile, G_KEY_FILE_NONE, NULL);

	/* read the keys */
	for (i = 0; i < filetypes_array->len; i++)
	{
		gboolean userset =
			g_key_file_has_key(userconfig, "Extensions", filetypes[i]->name, NULL);
		gchar **list = g_key_file_get_string_list(
			(userset) ? userconfig : sysconfig, "Extensions", filetypes[i]->name, &len, NULL);

		g_strfreev(filetypes[i]->pattern);
		/* Note: we allow 'Foo=' to remove all patterns */
		if (!list)
			list = g_new0(gchar*, 1);
		filetypes[i]->pattern = list;

#ifdef G_OS_WIN32
		convert_filetype_extensions_to_lower_case(filetypes[i]->pattern, len);
#endif
	}

	g_free(sysconfigfile);
	g_free(userconfigfile);
	g_key_file_free(sysconfig);
	g_key_file_free(userconfig);

	/* Redetect filetype of any documents with none set */
	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];
		if (doc->file_type->id != GEANY_FILETYPES_NONE)
			continue;
		document_set_filetype(doc, filetypes_detect_from_document(doc));
	}
}


/** Accessor function for @ref GeanyData::filetypes_array items.
 * Example: @code ft = filetypes_index(GEANY_FILETYPES_C); @endcode
 * @param idx @c filetypes_array index.
 * @return The filetype, or @c NULL if @a idx is out of range.
 *
 *  @since 0.16
 */
GeanyFiletype *filetypes_index(gint idx)
{
	return (idx >= 0 && idx < (gint) filetypes_array->len) ? filetypes[idx] : NULL;
}


void filetypes_reload(void)
{
	guint i;
	GeanyDocument *current_doc;

	/* save possibly changed commands before re-reading them */
	filetypes_save_commands();

	/* reload filetype configs */
	for (i = 0; i < filetypes_array->len; i++)
	{
		/* filetypes_load_config() will skip not loaded filetypes */
		filetypes_load_config(i, TRUE);
	}

	current_doc = document_get_current();
	if (!current_doc)
		return;

	/* update document styling */
	foreach_document(i)
	{
		if (current_doc != documents[i])
			document_reload_config(documents[i]);
	}
	/* process the current document at last */
	document_reload_config(current_doc);
}


