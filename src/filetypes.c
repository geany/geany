/*
 *      filetypes.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file filetypes.h
 * Filetype detection, file extensions and filetype menu items.
 */

/* Note: we use filetype_id for some function arguments, but GeanyFiletype is better; we should
 * only use GeanyFiletype for API functions. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "filetypes.h"

#include "app.h"
#include "callbacks.h" /* FIXME: for ignore_callback */
#include "document.h"
#include "filetypesprivate.h"
#include "geany.h"
#include "geanyobject.h"
#include "highlighting.h"
#include "pluginexport.h"
#include "projectprivate.h"
#include "sciwrappers.h"
#include "support.h"
#include "symbols.h"
#include "tm_parser.h"
#include "utils.h"
#include "ui_utils.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>

#define GEANY_FILETYPE_SEARCH_LINES 2 /* lines of file to search for filetype */

GPtrArray *filetypes_array = NULL;	/* Dynamic array of filetype pointers */

static GHashTable *filetypes_hash = NULL;	/* Hash of filetype pointers based on name keys */

/** List of filetype pointers sorted by name, but with @c filetypes_index(GEANY_FILETYPES_NONE)
 * first, as this is usually treated specially.
 * The list does not change (after filetypes have been initialized), so you can use
 * @code g_slist_nth_data(filetypes_by_title, n) @endcode and expect the same result at different times.
 * @see filetypes_get_sorted_by_name(). */
GSList *filetypes_by_title = NULL;

static GtkWidget *group_menus[GEANY_FILETYPE_GROUP_COUNT] = {NULL};


static void create_radio_menu_item(GtkWidget *menu, GeanyFiletype *ftype);

static gchar *filetypes_get_conf_extension(const GeanyFiletype *ft);
static void read_filetype_config(void);


enum TitleType
{
	TITLE_NONE,
	TITLE_SOURCE_FILE,
	TITLE_FILE,
	TITLE_SCRIPT,
	TITLE_DOCUMENT
};

/* Save adding many translation strings if the filetype name doesn't need translating */
static gchar *filetype_make_title(const char *name, enum TitleType type)
{
	g_return_val_if_fail(name != NULL, NULL);

	switch (type)
	{
		case TITLE_SOURCE_FILE:	return g_strdup_printf(_("%s source file"), name);
		case TITLE_FILE:		return g_strdup_printf(_("%s file"), name);
		case TITLE_SCRIPT:		return g_strdup_printf(_("%s script"), name);
		case TITLE_DOCUMENT:	return g_strdup_printf(_("%s document"), name);
		case TITLE_NONE: /* fall through */
		default:				return g_strdup(name);
	}
}


/* name argument (ie filetype name) must not be translated as it is used for
 * filetype lookup. Use filetypes_get_display_name() instead.*/
static void ft_init(filetype_id ft_id, int lang, const char *name,
	const char *title_name, enum TitleType title_type,
	GeanyFiletypeGroupID group_id)
{
	GeanyFiletype *ft = filetypes[ft_id];
	ft->lang = lang;
	ft->name = g_strdup(name);
	ft->title = filetype_make_title((title_name != NULL) ? title_name : ft->name, title_type);
	ft->group = group_id;
}

/* Evil macro to save typing and make init_builtin_filetypes() more readable */
#define FT_INIT(ft_id, parser_id, name, title_name, title_type, group_id) \
	ft_init(GEANY_FILETYPES_##ft_id, TM_PARSER_##parser_id, name, title_name, \
		TITLE_##title_type, GEANY_FILETYPE_GROUP_##group_id)


/* Note: remember to update HACKING if this function is renamed. */
static void init_builtin_filetypes(void)
{
	/* Column legend:
	 *   [0] = Filetype constant (GEANY_FILETYPES_*)
	 *   [1] = CTags parser (TM_PARSER_*)
	 *   [2] = Non-translated filetype name (*not* label for display)
	 *   [3] = Translatable human filetype title prefix or NULL to use [2]
	 *   [4] = Title type (TITLE_*) constant (ex. TITLE_SOURCE_FILE is 'source file' suffix)
	 *   [5] = The filetype group constant (GEANY_FILETYPE_GROUP_*)
	 * --------------------------------------------------------------------------------------------------------------------------
	 *       [0]         [1]           [2]                 [3]                        [4]          [5]      */
	FT_INIT( NONE,       NONE,         "None",             _("None"),                 NONE,        NONE     );
	FT_INIT( C,          C,            "C",                NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( CPP,        CPP,          "C++",              NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( OBJECTIVEC, OBJC,         "Objective-C",      NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( CS,         CSHARP,       "C#",               NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( VALA,       VALA,         "Vala",             NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( D,          D,            "D",                NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( JAVA,       JAVA,         "Java",             NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( PASCAL,     PASCAL,       "Pascal",           NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( ASM,        ASM,          "ASM",              "Assembler",               SOURCE_FILE, COMPILED );
	FT_INIT( BASIC,      FREEBASIC,    "FreeBasic",        NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( FORTRAN,    FORTRAN,      "Fortran",          "Fortran (F90)",           SOURCE_FILE, COMPILED );
	FT_INIT( F77,        F77,          "F77",              "Fortran (F77)",           SOURCE_FILE, COMPILED );
	FT_INIT( GLSL,       GLSL,         "GLSL",             NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( CAML,       NONE,         "CAML",             "(O)Caml",                 SOURCE_FILE, COMPILED );
	FT_INIT( PERL,       PERL,         "Perl",             NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( PHP,        PHP,          "PHP",              NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( JS,         JAVASCRIPT,   "Javascript",       NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( PYTHON,     PYTHON,       "Python",           NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( RUBY,       RUBY,         "Ruby",             NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( TCL,        TCL,          "Tcl",              NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( LUA,        LUA,          "Lua",              NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( FERITE,     FERITE,       "Ferite",           NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( HASKELL,    HASKELL,      "Haskell",          NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( MARKDOWN,   MARKDOWN,     "Markdown",         NULL,                      SOURCE_FILE, MARKUP   );
	FT_INIT( TXT2TAGS,   TXT2TAGS,     "Txt2tags",         NULL,                      SOURCE_FILE, MARKUP   );
	FT_INIT( ABC,        ABC,          "Abc",              NULL,                      FILE,        MISC     );
	FT_INIT( SH,         SH,           "Sh",               _("Shell"),                SCRIPT,      SCRIPT   );
	FT_INIT( MAKE,       MAKEFILE,     "Make",             _("Makefile"),             NONE,        SCRIPT   );
	FT_INIT( XML,        NONE,         "XML",              NULL,                      DOCUMENT,    MARKUP   );
	FT_INIT( DOCBOOK,    DOCBOOK,      "Docbook",          NULL,                      DOCUMENT,    MARKUP   );
	FT_INIT( HTML,       HTML,         "HTML",             NULL,                      DOCUMENT,    MARKUP   );
	FT_INIT( CSS,        CSS,          "CSS",              _("Cascading Stylesheet"), NONE,        MARKUP   ); /* not really markup but fit quite well to HTML */
	FT_INIT( SQL,        SQL,          "SQL",              NULL,                      FILE,        MISC     );
	FT_INIT( COBOL,      COBOL,        "COBOL",            NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( LATEX,      LATEX,        "LaTeX",            NULL,                      SOURCE_FILE, MARKUP   );
	FT_INIT( VHDL,       VHDL,         "VHDL",             NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( VERILOG,    VERILOG,      "Verilog",          NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( DIFF,       DIFF,         "Diff",             NULL,                      FILE,        MISC     );
	FT_INIT( LISP,       NONE,         "Lisp",             NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( ERLANG,     NONE,         "Erlang",           NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( CONF,       CONF,         "Conf",             _("Config"),               FILE,        MISC     );
	FT_INIT( PO,         NONE,         "Po",               _("Gettext translation"),  FILE,        MISC     );
	FT_INIT( HAXE,       HAXE,         "Haxe",             NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( AS,         ACTIONSCRIPT, "ActionScript",     NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( R,          R,            "R",                NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( REST,       REST,         "reStructuredText", NULL,                      SOURCE_FILE, MARKUP   );
	FT_INIT( MATLAB,     MATLAB,       "Matlab/Octave",    NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( YAML,       NONE,         "YAML",             NULL,                      FILE,        MISC     );
	FT_INIT( CMAKE,      NONE,         "CMake",            NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( NSIS,       NSIS,         "NSIS",             NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( ADA,        NONE,         "Ada",              NULL,                      SOURCE_FILE, COMPILED );
	FT_INIT( FORTH,      NONE,         "Forth",            NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( ASCIIDOC,   ASCIIDOC,     "Asciidoc",         NULL,                      SOURCE_FILE, MARKUP   );
	FT_INIT( ABAQUS,     ABAQUS,       "Abaqus",           NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( BATCH,      NONE,         "Batch",            NULL,                      SCRIPT,      SCRIPT   );
	FT_INIT( POWERSHELL, NONE,         "PowerShell",       NULL,                      SOURCE_FILE, SCRIPT   );
	FT_INIT( RUST,       RUST,         "Rust",             NULL,                      SOURCE_FILE, COMPILED );
}


/* initialize fields. */
static GeanyFiletype *filetype_new(void)
{
	GeanyFiletype *ft = g_new0(GeanyFiletype, 1);

	ft->group = GEANY_FILETYPE_GROUP_NONE;
	ft->lang = -2;	/* assume no tagmanager parser */
	/* pattern must not be null */
	ft->pattern = g_new0(gchar*, 1);
	ft->indent_width = -1;
	ft->indent_type = -1;

	ft->priv = g_new0(GeanyFiletypePrivate, 1);
	ft->priv->project_list_entry = -1; /* no entry */

	return ft;
}


static gint cmp_filetype(gconstpointer pft1, gconstpointer pft2, gpointer data)
{
	gboolean by_name = GPOINTER_TO_INT(data);
	const GeanyFiletype *ft1 = pft1, *ft2 = pft2;

	if (G_UNLIKELY(ft1->id == GEANY_FILETYPES_NONE))
		return -1;
	if (G_UNLIKELY(ft2->id == GEANY_FILETYPES_NONE))
		return 1;

	return by_name ?
		utils_str_casecmp(ft1->name, ft2->name) :
		utils_str_casecmp(ft1->title, ft2->title);
}


/** Gets a list of filetype pointers sorted by name.
 * The list does not change on subsequent calls.
 * @return The list - do not free.
 * @see filetypes_by_title. */
GEANY_API_SYMBOL
const GSList *filetypes_get_sorted_by_name(void)
{
	static GSList *list = NULL;

	g_return_val_if_fail(filetypes_by_title, NULL);

	if (!list)
	{
		list = g_slist_copy(filetypes_by_title);
		list = g_slist_sort_with_data(list, cmp_filetype, GINT_TO_POINTER(TRUE));
	}
	return list;
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
	ft->title = filetype_make_title(ft->name, TITLE_FILE);
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


/* Create the filetypes array and fill it with the known filetypes.
 * Warning: GTK isn't necessarily initialized yet. */
void filetypes_init_types(void)
{
	filetype_id ft_id;
	gchar *f;

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
	f = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, NULL);
	init_custom_filetypes(f);
	g_free(f);

	/* sort last instead of on insertion to prevent exponential time */
	filetypes_by_title = g_slist_sort_with_data(filetypes_by_title,
		cmp_filetype, GINT_TO_POINTER(FALSE));

	read_filetype_config();
}


static void on_document_save(G_GNUC_UNUSED GObject *object, GeanyDocument *doc)
{
	gchar *f;

	g_return_if_fail(!EMPTY(doc->real_path));

	f = g_build_filename(app->configdir, "filetype_extensions.conf", NULL);
	if (utils_str_equal(doc->real_path, f))
		filetypes_reload_extensions();

	g_free(f);
	f = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, "filetypes.common", NULL);
	if (utils_str_equal(doc->real_path, f))
	{
		guint i;

		/* Note: we don't reload other filetypes, even though the named styles may have changed.
		 * The user can do this manually with 'Tools->Reload Configuration' */
		filetypes_load_config(GEANY_FILETYPES_NONE, TRUE);

		foreach_document(i)
			document_reload_config(documents[i]);
	}
	g_free(f);
}


static void setup_config_file_menus(void)
{
	gchar *f;

	f = g_build_filename(app->configdir, "filetype_extensions.conf", NULL);
	ui_add_config_file_menu_item(f, NULL, NULL);
	SETPTR(f, g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, "filetypes.common", NULL));
	ui_add_config_file_menu_item(f, NULL, NULL);
	g_free(f);

	g_signal_connect(geany_object, "document-save", G_CALLBACK(on_document_save), NULL);
}


static void create_sub_menu(GtkWidget *parent, GeanyFiletypeGroupID group_id, const gchar *title)
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
	create_sub_menu(filetype_menu, GEANY_FILETYPE_GROUP_MISC, _("M_iscellaneous"));

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


void filetypes_init(void)
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


static GeanyFiletype *check_builtin_filenames(const gchar *utf8_filename)
{
	gchar *lfn = NULL;
	gchar *path;
	gboolean found = FALSE;

#ifdef G_OS_WIN32
	/* use lower case basename */
	lfn = g_utf8_strdown(utf8_filename, -1);
#else
	lfn = g_strdup(utf8_filename);
#endif
	SETPTR(lfn, utils_get_locale_from_utf8(lfn));

	path = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, "filetypes.", NULL);
	if (g_str_has_prefix(lfn, path))
		found = TRUE;

	SETPTR(path, g_build_filename(app->datadir, "filetypes.", NULL));
	if (g_str_has_prefix(lfn, path))
		found = TRUE;

	g_free(path);
	g_free(lfn);
	return found ? filetypes[GEANY_FILETYPES_CONF] : NULL;
}


/* Detect filetype only based on the filename extension.
 * utf8_filename can include the full path. */
GeanyFiletype *filetypes_detect_from_extension(const gchar *utf8_filename)
{
	gchar *base_filename;
	GeanyFiletype *ft;

	ft = check_builtin_filenames(utf8_filename);
	if (ft)
		return ft;

	/* to match against the basename of the file (because of Makefile*) */
	base_filename = g_path_get_basename(utf8_filename);
#ifdef G_OS_WIN32
	/* use lower case basename */
	SETPTR(base_filename, g_utf8_strdown(base_filename, -1));
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
		static const struct {
			const gchar *name;
			filetype_id filetype;
		} intepreter_map[] = {
			{ "sh",		GEANY_FILETYPES_SH },
			{ "bash",	GEANY_FILETYPES_SH },
			{ "dash",	GEANY_FILETYPES_SH },
			{ "perl",	GEANY_FILETYPES_PERL },
			{ "python",	GEANY_FILETYPES_PYTHON },
			{ "php",	GEANY_FILETYPES_PHP },
			{ "ruby",	GEANY_FILETYPES_RUBY },
			{ "tcl",	GEANY_FILETYPES_TCL },
			{ "make",	GEANY_FILETYPES_MAKE },
			{ "zsh",	GEANY_FILETYPES_SH },
			{ "ksh",	GEANY_FILETYPES_SH },
			{ "mksh",	GEANY_FILETYPES_SH },
			{ "csh",	GEANY_FILETYPES_SH },
			{ "tcsh",	GEANY_FILETYPES_SH },
			{ "ash",	GEANY_FILETYPES_SH },
			{ "dmd",	GEANY_FILETYPES_D },
			{ "wish",	GEANY_FILETYPES_TCL },
			{ "node",	GEANY_FILETYPES_JS },
			{ "rust",	GEANY_FILETYPES_RUST }
		};
		gchar *tmp = g_path_get_basename(line + 2);
		gchar *basename_interpreter = tmp;
		guint i;

		if (g_str_has_prefix(tmp, "env "))
		{	/* skip "env" and read the following interpreter */
			basename_interpreter += 4;
		}

		for (i = 0; ! ft && i < G_N_ELEMENTS(intepreter_map); i++)
		{
			if (g_str_has_prefix(basename_interpreter, intepreter_map[i].name))
				ft = filetypes[intepreter_map[i].filetype];
		}
		g_free(tmp);
	}
	/* detect HTML files */
	if (g_str_has_prefix(line, "<!DOCTYPE html") || g_str_has_prefix(line, "<html"))
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
	else if (utf8_filename && g_str_has_prefix(line, "<?xml"))
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
	else if (g_str_has_prefix(line, "<?php"))
	{
		ft = filetypes[GEANY_FILETYPES_PHP];
	}
	return ft;
}


/* Detect the filetype checking for a shebang, then filename extension.
 * @lines: an strv of the lines to scan (must containing at least one line) */
static GeanyFiletype *filetypes_detect_from_file_internal(const gchar *utf8_filename,
														  gchar **lines)
{
	GeanyFiletype	*ft;
	gint			 i;
	GRegex			*ft_regex;
	GMatchInfo		*match;
	GError			*regex_error = NULL;

	/* try to find a shebang and if found use it prior to the filename extension
	 * also checks for <?xml */
	ft = find_shebang(utf8_filename, lines[0]);
	if (ft != NULL)
		return ft;

	/* try to extract the filetype using a regex capture */
	ft_regex = g_regex_new(file_prefs.extract_filetype_regex,
			G_REGEX_RAW | G_REGEX_MULTILINE, 0, &regex_error);
	if (ft_regex != NULL)
	{
		for (i = 0; ft == NULL && lines[i] != NULL; i++)
		{
			if (g_regex_match(ft_regex, lines[i], 0, &match))
			{
				gchar *capture = g_match_info_fetch(match, 1);
				if (capture != NULL)
				{
					ft = filetypes_lookup_by_name(capture);
					g_free(capture);
				}
			}
			g_match_info_free(match);
		}
		g_regex_unref(ft_regex);
	}
	else if (regex_error != NULL)
	{
		geany_debug("Filetype extract regex ignored: %s", regex_error->message);
		g_error_free(regex_error);
	}
	if (ft != NULL)
		return ft;

	if (utf8_filename == NULL)
		return filetypes[GEANY_FILETYPES_NONE];

	return filetypes_detect_from_extension(utf8_filename);
}


/* Detect the filetype for the document, checking for a shebang, then filename extension. */
GeanyFiletype *filetypes_detect_from_document(GeanyDocument *doc)
{
	GeanyFiletype 	*ft;
	gchar 			*lines[GEANY_FILETYPE_SEARCH_LINES + 1];
	gint			 i;

	g_return_val_if_fail(doc == NULL || doc->is_valid, filetypes[GEANY_FILETYPES_NONE]);

	if (doc == NULL)
		return filetypes[GEANY_FILETYPES_NONE];

	for (i = 0; i < GEANY_FILETYPE_SEARCH_LINES; ++i)
	{
		lines[i] = sci_get_line(doc->editor->sci, i);
	}
	lines[i] = NULL;
	ft = filetypes_detect_from_file_internal(doc->file_name, lines);
	for (i = 0; i < GEANY_FILETYPE_SEARCH_LINES; ++i)
	{
		g_free(lines[i]);
	}
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
GEANY_API_SYMBOL
GeanyFiletype *filetypes_detect_from_file(const gchar *utf8_filename)
{
	gchar line[1024];
	gchar *lines[2];
	FILE  *f;
	gchar *locale_name = utils_get_locale_from_utf8(utf8_filename);

	f = g_fopen(locale_name, "r");
	g_free(locale_name);
	if (f != NULL)
	{
		if (fgets(line, sizeof(line), f) != NULL)
		{
			fclose(f);
			lines[0] = line;
			lines[1] = NULL;
			return filetypes_detect_from_file_internal(utf8_filename, lines);
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
on_filetype_change(GtkCheckMenuItem *menuitem, gpointer user_data)
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
	g_free(ft->comment_single);
	g_free(ft->context_action_cmd);
	g_free(ft->priv->filecmds);
	g_free(ft->priv->ftdefcmds);
	g_free(ft->priv->execcmds);
	g_free(ft->error_regex_string);
	if (ft->icon)
		g_object_unref(ft->icon);
	g_strfreev(ft->pattern);

	if (ft->priv->error_regex)
		g_regex_unref(ft->priv->error_regex);
	g_slist_foreach(ft->priv->tag_files, (GFunc) g_free, NULL);
	g_slist_free(ft->priv->tag_files);

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


static void load_indent_settings(GeanyFiletype *ft, GKeyFile *config, GKeyFile *configh)
{
	ft->indent_width = utils_get_setting(integer, configh, config, "indentation", "width", -1);
	ft->indent_type = utils_get_setting(integer, configh, config, "indentation", "type", -1);
	/* check whether the indent type is OK */
	switch (ft->indent_type)
	{
		case GEANY_INDENT_TYPE_TABS:
		case GEANY_INDENT_TYPE_SPACES:
		case GEANY_INDENT_TYPE_BOTH:
		case -1:
			break;

		default:
			g_warning("Invalid indent type %d in file type %s", ft->indent_type, ft->name);
			ft->indent_type = -1;
			break;
	}
}


static void load_settings(guint ft_id, GKeyFile *config, GKeyFile *configh)
{
	GeanyFiletype *ft = filetypes[ft_id];
	gchar *result;

	/* default extension */
	result = utils_get_setting(string, configh, config, "settings", "extension", NULL);
	if (result != NULL)
	{
		SETPTR(filetypes[ft_id]->extension, result);
	}

	/* MIME type */
	result = utils_get_setting(string, configh, config, "settings", "mime_type", "text/plain");
	SETPTR(filetypes[ft_id]->mime_type, result);

	/* read comment notes */
	result = utils_get_setting(string, configh, config, "settings", "comment_open", NULL);
	if (result != NULL)
	{
		SETPTR(filetypes[ft_id]->comment_open, result);
	}

	result = utils_get_setting(string, configh, config, "settings", "comment_close", NULL);
	if (result != NULL)
	{
		SETPTR(filetypes[ft_id]->comment_close, result);
	}

	result = utils_get_setting(string, configh, config, "settings", "comment_single", NULL);
	if (result != NULL)
	{
		SETPTR(filetypes[ft_id]->comment_single, result);
	}
	/* import correctly filetypes that use old-style single comments */
	else if (EMPTY(filetypes[ft_id]->comment_close))
	{
		SETPTR(filetypes[ft_id]->comment_single, filetypes[ft_id]->comment_open);
		filetypes[ft_id]->comment_open = NULL;
	}

	filetypes[ft_id]->comment_use_indent = utils_get_setting(boolean, configh, config,
			"settings", "comment_use_indent", FALSE);

	/* read context action */
	result = utils_get_setting(string, configh, config, "settings", "context_action_cmd", NULL);
	if (result != NULL)
	{
		SETPTR(filetypes[ft_id]->context_action_cmd, result);
	}

	result = utils_get_setting(string, configh, config, "settings", "tag_parser", NULL);
	if (result != NULL)
	{
		ft->lang = tm_source_file_get_named_lang(result);
		if (ft->lang < 0)
			geany_debug("Cannot find tag parser '%s' for custom filetype '%s'.", result, ft->name);
		g_free(result);
	}

	result = utils_get_setting(string, configh, config, "settings", "lexer_filetype", NULL);
	if (result != NULL)
	{
		ft->lexer_filetype = filetypes_lookup_by_name(result);
		if (!ft->lexer_filetype)
			geany_debug("Cannot find lexer filetype '%s' for custom filetype '%s'.", result, ft->name);
		g_free(result);
	}

	ft->priv->symbol_list_sort_mode = utils_get_setting(integer, configh, config, "settings",
		"symbol_list_sort_mode", SYMBOLS_SORT_BY_NAME);
	ft->priv->xml_indent_tags = utils_get_setting(boolean, configh, config, "settings",
		"xml_indent_tags", FALSE);

	/* read indent settings */
	load_indent_settings(ft, config, configh);

	/* read build settings */
	build_load_menu(config, GEANY_BCS_FT, (gpointer)ft);
	build_load_menu(configh, GEANY_BCS_HOME_FT, (gpointer)ft);
}


static void copy_keys(GKeyFile *dest, const gchar *dest_group,
		GKeyFile *src, const gchar *src_group)
{
	gchar **keys = g_key_file_get_keys(src, src_group, NULL, NULL);
	gchar **ptr;

	foreach_strv(ptr, keys)
	{
		gchar *key = *ptr;
		gchar *value = g_key_file_get_value(src, src_group, key, NULL);

		g_key_file_set_value(dest, dest_group, key, value);
		g_free(value);
	}
	g_strfreev(keys);
}


static gchar *filetypes_get_filename(GeanyFiletype *ft, gboolean user)
{
	gchar *ext = filetypes_get_conf_extension(ft);
	gchar *base_name = g_strconcat("filetypes.", ext, NULL);
	gchar *file_name;

	if (user)
		file_name = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, base_name, NULL);
	else
		file_name = g_build_filename(app->datadir, base_name, NULL);

	g_free(ext);
	g_free(base_name);

	return file_name;
}


static void add_group_keys(GKeyFile *kf, const gchar *group, GeanyFiletype *ft)
{
	gchar *files[2];
	gboolean loaded = FALSE;
	guint i;

	files[0] = filetypes_get_filename(ft, FALSE);
	files[1] = filetypes_get_filename(ft, TRUE);

	for (i = 0; i < G_N_ELEMENTS(files); i++)
	{
		GKeyFile *src = g_key_file_new();

		if (g_key_file_load_from_file(src, files[i], G_KEY_FILE_NONE, NULL))
		{
			copy_keys(kf, group, src, group);
			loaded = TRUE;
		}
		g_key_file_free(src);
	}

	if (!loaded)
		geany_debug("Could not read config file %s for [%s=%s]!", files[0], group, ft->name);

	g_free(files[0]);
	g_free(files[1]);
}


static void copy_ft_groups(GKeyFile *kf)
{
	gchar **groups = g_key_file_get_groups(kf, NULL);
	gchar **ptr;

	foreach_strv(ptr, groups)
	{
		gchar *group = *ptr;
		gchar *old_group;
		gchar *name = strchr(*ptr, '=');
		GeanyFiletype *ft;

		if (!name || !name[1]) /* no name or no parent name */
			continue;

		old_group = g_strdup(group);

		/* terminate group at '=' */
		*name = 0;
		name++;

		ft = filetypes_lookup_by_name(name);
		if (ft)
		{
			add_group_keys(kf, group, ft);
			/* move old group keys (foo=bar) to proper group name (foo) */
			copy_keys(kf, group, kf, old_group);
		}
		g_free(old_group);
	}
	g_strfreev(groups);
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
void filetypes_load_config(guint ft_id, gboolean reload)
{
	GKeyFile *config, *config_home;
	GeanyFiletypePrivate *pft;
	GeanyFiletype *ft;

	g_return_if_fail(ft_id < filetypes_array->len);

	ft = filetypes[ft_id];
	pft = ft->priv;

	/* when reloading, proceed only if the settings were already loaded */
	if (G_UNLIKELY(reload && ! pft->keyfile_loaded))
		return;

	/* when not reloading, load the settings only once */
	if (G_LIKELY(! reload && pft->keyfile_loaded))
		return;
	pft->keyfile_loaded = TRUE;

	config = g_key_file_new();
	config_home = g_key_file_new();
	{
		/* highlighting uses GEANY_FILETYPES_NONE for common settings */
		gchar *f;

		f = filetypes_get_filename(ft, FALSE);
		load_system_keyfile(config, f, G_KEY_FILE_KEEP_COMMENTS, ft);

		SETPTR(f, filetypes_get_filename(ft, TRUE));
		g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);
		g_free(f);
	}
	/* Copy keys for any groups with [group=C] from system keyfile */
	copy_ft_groups(config);
	copy_ft_groups(config_home);

	load_settings(ft_id, config, config_home);
	highlighting_init_styles(ft_id, config, config_home);

	if (ft->icon)
		g_object_unref(ft->icon);
	ft->icon = ui_get_mime_icon(ft->mime_type);

	g_key_file_free(config);
	g_key_file_free(config_home);
}


static gchar *filetypes_get_conf_extension(const GeanyFiletype *ft)
{
	gchar *result;

	if (ft->priv->custom)
		return g_strconcat(ft->name, ".conf", NULL);

	/* Handle any special extensions different from lowercase filetype->name */
	switch (ft->id)
	{
		case GEANY_FILETYPES_CPP: result = g_strdup("cpp"); break;
		case GEANY_FILETYPES_CS: result = g_strdup("cs"); break;
		case GEANY_FILETYPES_MAKE: result = g_strdup("makefile"); break;
		case GEANY_FILETYPES_NONE: result = g_strdup("common"); break;
		/* name is Matlab/Octave */
		case GEANY_FILETYPES_MATLAB: result = g_strdup("matlab"); break;
		/* name is Objective-C, and we don't want the hyphen */
		case GEANY_FILETYPES_OBJECTIVEC: result = g_strdup("objectivec"); break;
		default:
			result = g_ascii_strdown(ft->name, -1);
			break;
	}
	return result;
}


void filetypes_save_commands(GeanyFiletype *ft)
{
	GKeyFile *config_home;
	gchar *fname, *data;

	fname = filetypes_get_filename(ft, TRUE);
	config_home = g_key_file_new();
	g_key_file_load_from_file(config_home, fname, G_KEY_FILE_KEEP_COMMENTS, NULL);
	build_save_menu(config_home, ft, GEANY_BCS_HOME_FT);
	data = g_key_file_to_data(config_home, NULL, NULL);
	utils_write_file(fname, data);
	g_free(data);
	g_key_file_free(config_home);
	g_free(fname);
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
GEANY_API_SYMBOL
GeanyFiletype *filetypes_lookup_by_name(const gchar *name)
{
	GeanyFiletype *ft;

	g_return_val_if_fail(!EMPTY(name), NULL);

	ft = g_hash_table_lookup(filetypes_hash, name);
	if (G_UNLIKELY(ft == NULL))
		geany_debug("Could not find filetype '%s'.", name);
	return ft;
}


static void compile_regex(GeanyFiletype *ft, gchar *regstr)
{
	GError *error = NULL;
	GRegex *regex = g_regex_new(regstr, 0, 0, &error);

	if (!regex)
	{
		ui_set_statusbar(TRUE, _("Bad regex for filetype %s: %s"),
			filetypes_get_display_name(ft), error->message);
		g_error_free(error);
	}
	if (ft->priv->error_regex)
		g_regex_unref(ft->priv->error_regex);
	ft->priv->error_regex = regex;
}


gboolean filetypes_parse_error_message(GeanyFiletype *ft, const gchar *message,
		gchar **filename, gint *line)
{
	gchar *regstr;
	gchar **tmp;
	GeanyDocument *doc;
	GMatchInfo *minfo;
	gint i, n_match_groups;
	gchar *first, *second;

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

	*filename = NULL;
	*line = -1;

	if (G_UNLIKELY(EMPTY(regstr)))
		return FALSE;

	if (!ft->priv->error_regex || regstr != ft->priv->last_error_pattern)
	{
		compile_regex(ft, regstr);
		ft->priv->last_error_pattern = regstr;
	}
	if (!ft->priv->error_regex)
		return FALSE;

	if (!g_regex_match(ft->priv->error_regex, message, 0, &minfo))
	{
		g_match_info_free(minfo);
		return FALSE;
	}

	n_match_groups = g_match_info_get_match_count(minfo);
	first = second = NULL;

	for (i = 1; i < n_match_groups; i++)
	{
		gint start_pos;

		g_match_info_fetch_pos(minfo, i, &start_pos, NULL);
		if (start_pos != -1)
		{
			if (first == NULL)
				first = g_match_info_fetch(minfo, i);
			else
			{
				second = g_match_info_fetch(minfo, i);
				break;
			}
		}
	}

	if (second)
	{
		gchar *end;
		glong l;

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
	else
		g_free(first);

	g_match_info_free(minfo);
	return *filename != NULL;
}


#ifdef G_OS_WIN32
static void convert_filetype_extensions_to_lower_case(gchar **patterns, gsize len)
{
	guint i;
	for (i = 0; i < len; i++)
	{
		SETPTR(patterns[i], g_ascii_strdown(patterns[i], -1));
	}
}
#endif


static void read_extensions(GKeyFile *sysconfig, GKeyFile *userconfig)
{
	guint i;
	gsize len = 0;

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
}


static void read_group(GKeyFile *config, const gchar *group_name, GeanyFiletypeGroupID group_id)
{
	gchar **names = g_key_file_get_string_list(config, "Groups", group_name, NULL, NULL);
	gchar **name;

	foreach_strv(name, names)
	{
		GeanyFiletype *ft = filetypes_lookup_by_name(*name);

		if (ft)
		{
			ft->group = group_id;
			if (ft->priv->custom &&
				(group_id == GEANY_FILETYPE_GROUP_COMPILED || group_id == GEANY_FILETYPE_GROUP_SCRIPT))
			{
				SETPTR(ft->title, filetype_make_title(ft->name, TITLE_SOURCE_FILE));
			}
		}
		else
			geany_debug("Filetype '%s' not found for group '%s'!", *name, group_name);
	}
	g_strfreev(names);
}


static void read_groups(GKeyFile *config)
{
	read_group(config, "Programming", GEANY_FILETYPE_GROUP_COMPILED);
	read_group(config, "Script", GEANY_FILETYPE_GROUP_SCRIPT);
	read_group(config, "Markup", GEANY_FILETYPE_GROUP_MARKUP);
	read_group(config, "Misc", GEANY_FILETYPE_GROUP_MISC);
	read_group(config, "None", GEANY_FILETYPE_GROUP_NONE);
}


static void read_filetype_config(void)
{
	gchar *sysconfigfile = g_build_filename(app->datadir, "filetype_extensions.conf", NULL);
	gchar *userconfigfile = g_build_filename(app->configdir, "filetype_extensions.conf", NULL);
	GKeyFile *sysconfig = g_key_file_new();
	GKeyFile *userconfig = g_key_file_new();

	g_key_file_load_from_file(sysconfig, sysconfigfile, G_KEY_FILE_NONE, NULL);
	g_key_file_load_from_file(userconfig, userconfigfile, G_KEY_FILE_NONE, NULL);

	read_extensions(sysconfig, userconfig);
	read_groups(sysconfig);
	read_groups(userconfig);

	g_free(sysconfigfile);
	g_free(userconfigfile);
	g_key_file_free(sysconfig);
	g_key_file_free(userconfig);
}


void filetypes_reload_extensions(void)
{
	guint i;

	read_filetype_config();

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
GEANY_API_SYMBOL
GeanyFiletype *filetypes_index(gint idx)
{
	return (idx >= 0 && idx < (gint) filetypes_array->len) ? filetypes[idx] : NULL;
}


void filetypes_reload(void)
{
	guint i;
	GeanyDocument *current_doc;

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


/** Gets @c ft->name or a translation for filetype None.
 * @param ft .
 * @return .
 * @since Geany 0.20 */
GEANY_API_SYMBOL
const gchar *filetypes_get_display_name(GeanyFiletype *ft)
{
	return ft->id == GEANY_FILETYPES_NONE ? _("None") : ft->name;
}


/* gets comment_open/comment_close/comment_single strings from the filetype
 * @param single_first: whether single comment is preferred if both available
 * returns true if at least comment_open is set, false otherwise */
gboolean filetype_get_comment_open_close(const GeanyFiletype *ft, gboolean single_first,
		const gchar **co, const gchar **cc)
{
	g_return_val_if_fail(ft != NULL, FALSE);
	g_return_val_if_fail(co != NULL, FALSE);
	g_return_val_if_fail(cc != NULL, FALSE);

	if (single_first)
	{
		*co = ft->comment_single;
		if (!EMPTY(*co))
			*cc = NULL;
		else
		{
			*co = ft->comment_open;
			*cc = ft->comment_close;
		}
	}
	else
	{
		*co = ft->comment_open;
		if (!EMPTY(*co))
			*cc = ft->comment_close;
		else
		{
			*co = ft->comment_single;
			*cc = NULL;
		}
	}

	return !EMPTY(*co);
}
