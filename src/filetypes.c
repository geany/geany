/*
 *      filetypes.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 The Geany contributors
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "filetypes.h"

#include "app.h"
#include "callbacks.h"
#include "document.h"
#include "filetypesprivate.h"
#include "geany.h"
#include "geanyobject.h"
#include "highlighting.h"
#include "projectprivate.h"
#include "sciwrappers.h"
#include "support.h"
#include "symbols.h"
#include "tm_parser.h"
#include "utils.h"
#include "ui_utils.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <glib/gstdio.h>

#define GEANY_FILETYPE_SEARCH_LINES 2 /* lines of file to search for filetype */

GPtrArray *filetypes_array = NULL;
static GHashTable *filetypes_hash = NULL;	/* Hash of filetype pointers based on name keys */
GSList *filetypes_by_title = NULL;


static void create_radio_menu_item(GtkWidget *menu, GeanyFiletype *ftype);

static gchar *filetypes_get_conf_extension(const GeanyFiletype *ft);
static void read_filetype_config(void);
static void create_set_filetype_menu(gboolean config);
static gchar *filetypes_get_filename(GeanyFiletype *ft, gboolean user);


static GeanyFiletypeGroupID get_group(const gchar *name)
{
	gchar ch;

	if (EMPTY(name) || !isalpha(name[0]))
		return GEANY_FILETYPE_GROUP_OTHER;

	ch = name[0];
	if (ch >= 'a')
		ch -= 'a' - 'A';

	if (ch >= 'A' && ch <= 'B')
		return GEANY_FILETYPE_GROUP_A_B;
	else if (ch == 'C')
		return GEANY_FILETYPE_GROUP_C;
	else if (ch >= 'D' && ch <= 'F')
		return GEANY_FILETYPE_GROUP_D_E_F;
	else if (ch >= 'G' && ch <= 'I')
		return GEANY_FILETYPE_GROUP_G_H_I;
	else if (ch >= 'J' && ch <= 'L')
		return GEANY_FILETYPE_GROUP_J_K_L;
	else if (ch >= 'M' && ch <= 'O')
		return GEANY_FILETYPE_GROUP_M_N_O;
	else if (ch >= 'P' && ch <= 'Q')
		return GEANY_FILETYPE_GROUP_P_Q;
	else if (ch >= 'R' && ch <= 'S')
		return GEANY_FILETYPE_GROUP_R_S;
	else if (ch >= 'T' && ch <= 'W')
		return GEANY_FILETYPE_GROUP_T_U_V_W;
	else if (ch >= 'X' && ch <= 'Z')
		return GEANY_FILETYPE_GROUP_X_Y_Z;

	return GEANY_FILETYPE_GROUP_OTHER;
}


static GeanyFiletypeGroupID get_filetype_group(const gchar *title, const gchar *name)
{
	if (g_strcmp0(name, "None") == 0)
		return GEANY_FILETYPE_GROUP_NONE;

	return get_group(title);
}


static void ft_init(GeanyFiletypeID ft_id, TMParserType lang, const char *name,
	const char *title_name)
{
	GeanyFiletype *ft = filetypes[ft_id];
	if (G_UNLIKELY(!ft)) return;
	ft->lang = lang;
	ft->name = g_strdup(name);
	ft->title = g_strdup((title_name != NULL) ? title_name : ft->name);
	ft->group = get_filetype_group(ft->title, ft->name);
}

#define FT_INIT(ft_id, parser_id, name, title_name) \
	ft_init(GEANY_FILETYPES_##ft_id, TM_PARSER_##parser_id, name, title_name)


static void init_builtin_filetypes(void)
{
	FT_INIT( NONE,         NONE,         "None",             _("None")                 );
	FT_INIT( C,            C,            "C",                NULL                      );
	FT_INIT( CPP,          CPP,          "C++",              NULL                      );
	FT_INIT( OBJECTIVEC,   OBJC,         "Objective-C",      NULL                      );
	FT_INIT( CS,           CSHARP,       "C#",               NULL                      );
	FT_INIT( VALA,         VALA,         "Vala",             NULL                      );
	FT_INIT( D,            D,            "D",                NULL                      );
	FT_INIT( JAVA,         JAVA,         "Java",             NULL                      );
	FT_INIT( PASCAL,       PASCAL,       "Pascal",           NULL                      );
	FT_INIT( ASM,          ASM,          "ASM",              "Assembler"               );
	FT_INIT( BASIC,        FREEBASIC,    "FreeBasic",        NULL                      );
	FT_INIT( FORTRAN,      FORTRAN,      "Fortran",          "Fortran (F90)"           );
	FT_INIT( F77,          FORTRAN,      "F77",              "Fortran (F77)"           );
	FT_INIT( GLSL,         C,            "GLSL",             NULL                      );
	FT_INIT( CAML,         OCAML,        "CAML",             "Caml/OCaml"              );
	FT_INIT( PERL,         PERL,         "Perl",             NULL                      );
	FT_INIT( PHP,          PHP,          "PHP",              NULL                      );
	FT_INIT( JS,           JAVASCRIPT,   "Javascript",       NULL                      );
	FT_INIT( PYTHON,       PYTHON,       "Python",           NULL                      );
	FT_INIT( RUBY,         RUBY,         "Ruby",             NULL                      );
	FT_INIT( TCL,          TCLOO,        "Tcl",              NULL                      );
	FT_INIT( LUA,          LUA,          "Lua",              NULL                      );
	FT_INIT( GDSCRIPT,     GDSCRIPT,     "GDScript",         NULL                      );
	FT_INIT( HASKELL,      HASKELL,      "Haskell",          NULL                      );
	FT_INIT( MARKDOWN,     MARKDOWN,     "Markdown",         NULL                      );
	FT_INIT( TXT2TAGS,     TXT2TAGS,     "Txt2tags",         NULL                      );
	FT_INIT( ABC,          ABC,          "Abc",              NULL                      );
	FT_INIT( SH,           SH,           "Sh",               "Shell"                   );
	FT_INIT( MAKE,         MAKEFILE,     "Make",             "Makefile"                );
	FT_INIT( XML,          NONE,         "XML",              NULL                      );
	FT_INIT( DOCBOOK,      DOCBOOK,      "Docbook",          NULL                      );
	FT_INIT( HTML,         HTML,         "HTML",             NULL                      );
	FT_INIT( CSS,          CSS,          "CSS",              NULL                      );
	FT_INIT( SQL,          SQL,          "SQL",              NULL                      );
	FT_INIT( COBOL,        COBOL,        "COBOL",            NULL                      );
	FT_INIT( LATEX,        LATEX,        "LaTeX",            NULL                      );
	FT_INIT( BIBTEX,       BIBTEX,       "BibTeX",           NULL                      );
	FT_INIT( VHDL,         VHDL,         "VHDL",             NULL                      );
	FT_INIT( VERILOG,      VERILOG,      "Verilog",          NULL                      );
	FT_INIT( DIFF,         DIFF,         "Diff",             NULL                      );
	FT_INIT( LISP,         LISP,         "Lisp",             NULL                      );
	FT_INIT( ERLANG,       ERLANG,       "Erlang",           NULL                      );
	FT_INIT( CONF,         CONF,         "Conf",             "Conf/Ini"                );
	FT_INIT( PO,           NONE,         "Po",               "Po (Gettext)"            );
	FT_INIT( HAXE,         HAXE,         "Haxe",             NULL                      );
	FT_INIT( AS,           ACTIONSCRIPT, "ActionScript",     NULL                      );
	FT_INIT( R,            R,            "R",                NULL                      );
	FT_INIT( REST,         REST,         "reStructuredText", NULL                      );
	FT_INIT( MATLAB,       MATLAB,       "Matlab/Octave",    NULL                      );
	FT_INIT( YAML,         NONE,         "YAML",             NULL                      );
	FT_INIT( CMAKE,        NONE,         "CMake",            NULL                      );
	FT_INIT( NSIS,         NSIS,         "NSIS",             NULL                      );
	FT_INIT( ADA,          ADA,          "Ada",              NULL                      );
	FT_INIT( FORTH,        FORTH,        "Forth",            NULL                      );
	FT_INIT( ASCIIDOC,     ASCIIDOC,     "Asciidoc",         NULL                      );
	FT_INIT( ABAQUS,       ABAQUS,       "Abaqus",           NULL                      );
	FT_INIT( BATCH,        BATCH,        "Batch",            NULL                      );
	FT_INIT( POWERSHELL,   POWERSHELL,   "PowerShell",       NULL                      );
	FT_INIT( RUST,         RUST,         "Rust",             NULL                      );
	FT_INIT( COFFEESCRIPT, NONE,         "CoffeeScript",     NULL                      );
	FT_INIT( GO,           GO,           "Go",               NULL                      );
	FT_INIT( ZEPHIR,       ZEPHIR,       "Zephir",           NULL                      );
	FT_INIT( SMALLTALK,    NONE,         "Smalltalk",        NULL                      );
	FT_INIT( JULIA,        JULIA,        "Julia",            NULL                      );
	FT_INIT( AU3,          AUTOIT,       "AutoIt",           NULL                      );
	FT_INIT( RAKU,         RAKU,         "Raku",             NULL                      );
	FT_INIT( CIL,          NONE,         "CIL",              NULL                      );
	FT_INIT( PROLOG,       NONE,         "Prolog",           NULL                      );
	FT_INIT( NIM,          NONE,         "Nim",              NULL                      );
	FT_INIT( ZIG,          NONE,         "Zig",              NULL                      );
	FT_INIT( DART,         NONE,         "Dart",             NULL                      );
	FT_INIT( NIX,          NONE,         "Nix",              NULL                      );
	FT_INIT( TOML,         CONF,         "TOML",             NULL                      );
	FT_INIT( SINEX,        SINEX,        "SINEX",            NULL                      );
}


static GeanyFiletype *filetype_new(void)
{
	GeanyFiletype *ft = g_new0(GeanyFiletype, 1);
	if (!ft) return NULL;

	ft->group = GEANY_FILETYPE_GROUP_NONE;
	ft->lang = TM_PARSER_NONE;
	ft->pattern = g_new0(gchar*, 1);
	ft->indent_width = -1;
	ft->indent_type = -1;

	ft->priv = g_new0(GeanyFiletypePrivate, 1);
	if (ft->priv) {
		ft->priv->project_list_entry = -1;
	}

	return ft;
}


static gint cmp_filetype(gconstpointer pft1, gconstpointer pft2, gpointer data)
{
	gboolean by_name = GPOINTER_TO_INT(data);
	const GeanyFiletype *ft1 = pft1, *ft2 = pft2;

	if (G_UNLIKELY(!ft1 || !ft2)) return 0;
	if (G_UNLIKELY(ft1->id == GEANY_FILETYPES_NONE))
		return -1;
	if (G_UNLIKELY(ft2->id == GEANY_FILETYPES_NONE))
		return 1;

	return by_name ?
		utils_str_casecmp(ft1->name, ft2->name) :
		utils_str_casecmp(ft1->title, ft2->title);
}


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


static void filetype_add(GeanyFiletype *ft)
{
	g_return_if_fail(ft);
	g_return_if_fail(ft->name);

	ft->id = filetypes_array->len;
	g_ptr_array_add(filetypes_array, ft);
	g_hash_table_insert(filetypes_hash, ft->name, ft);

	filetypes_by_title = g_slist_prepend(filetypes_by_title, ft);
}


static void add_custom_filetype(const gchar *filename)
{
	if (EMPTY(filename)) return;
	const gchar *dot_ptr = strstr(filename, ".");
	if (!dot_ptr) return;

	gchar *fn = utils_strdupa(dot_ptr + 1);
	gchar *dot = g_strrstr(fn, ".conf");
	GeanyFiletype *ft;

	if (!dot) return;
	*dot = 0x0;

	if (g_hash_table_lookup(filetypes_hash, fn))
		return;

	ft = filetype_new();
	if (!ft) return;
	ft->name = g_strdup(fn);
	ft->title = g_strdup(ft->name);
	ft->group = get_filetype_group(ft->title, ft->name);
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

		if (filename && g_str_has_prefix(filename, prefix) &&
			g_str_has_suffix(filename + strlen(prefix), ".conf"))
		{
			add_custom_filetype(filename);
		}
	}
	g_dir_close(dir);
}


void filetypes_init_types(void)
{
	GeanyFiletypeID ft_id;
	gchar *f;

	g_return_if_fail(filetypes_array == NULL);
	g_return_if_fail(filetypes_hash == NULL);

	filetypes_array = g_ptr_array_sized_new(GEANY_MAX_BUILT_IN_FILETYPES);
	filetypes_hash = g_hash_table_new(g_str_hash, g_str_equal);

	for (ft_id = 0; ft_id < GEANY_MAX_BUILT_IN_FILETYPES; ft_id++)
	{
		filetypes[ft_id] = filetype_new();
	}
	init_builtin_filetypes();

	for (ft_id = 0; ft_id < GEANY_MAX_BUILT_IN_FILETYPES; ft_id++)
	{
		if (filetypes[ft_id]) {
			filetype_add(filetypes[ft_id]);
		}
	}
	f = g_build_filename(app->datadir, GEANY_FILEDEFS_SUBDIR, NULL);
	if (f) {
		init_custom_filetypes(f);
		g_free(f);
	}
	f = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, NULL);
	if (f) {
		init_custom_filetypes(f);
		g_free(f);
	}

	filetypes_by_title = g_slist_sort_with_data(filetypes_by_title,
		cmp_filetype, GINT_TO_POINTER(FALSE));

	read_filetype_config();
}


static void on_document_save(G_GNUC_UNUSED GObject *object, GeanyDocument *doc)
{
	gchar *f, *basename;

	if (!doc || EMPTY(doc->real_path)) return;

	f = g_build_filename(app->configdir, "filetype_extensions.conf", NULL);
	if (f && utils_str_equal(doc->real_path, f))
		filetypes_reload_extensions();
	g_free(f);

	basename = g_path_get_basename(doc->real_path);
	if (basename && g_str_has_prefix(basename, "filetypes."))
	{
		guint i;

		for (i = 0; i < filetypes_array->len; i++)
		{
			GeanyFiletype *ft = g_ptr_array_index(filetypes_array, i);
			if (!ft) continue;

			f = filetypes_get_filename(ft, TRUE);
			if (f && utils_str_equal(doc->real_path, f))
			{
				guint j;
				filetypes_load_config(i, TRUE);

				foreach_document(j)
				{
					if (documents[j])
						document_reload_config(documents[j]);
				}

				g_free(f);
				break;
			}
			g_free(f);
		}
	}
	g_free(basename);
}


static void setup_config_file_menus(void)
{
	gchar *f;

	f = g_build_filename(app->configdir, "filetype_extensions.conf", NULL);
	if (f) {
		ui_add_config_file_menu_item(f, NULL, NULL);
		g_free(f);
	}
	f = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, "filetypes.common", NULL);
	if (f) {
		ui_add_config_file_menu_item(f, NULL, NULL);
		g_free(f);
	}

	create_set_filetype_menu(TRUE);

	g_signal_connect(geany_object, "document-save", G_CALLBACK(on_document_save), NULL);
}


static GtkWidget *create_sub_menu(GtkWidget *parent, const gchar *title)
{
	GtkWidget *menu, *item;

	if (!parent || EMPTY(title)) return NULL;

	menu = gtk_menu_new();
	item = gtk_menu_item_new_with_mnemonic(title);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
	gtk_container_add(GTK_CONTAINER(parent), item);
	gtk_widget_show(item);

	return menu;
}


static void create_set_filetype_menu(gboolean config)
{
	GtkWidget *group_menus[GEANY_FILETYPE_GROUP_COUNT] = {NULL};
	GSList *node;
	GtkWidget *menu;

	menu = config ? ui_widgets.config_files_filetype_menu :
		ui_lookup_widget(main_widgets.window, "set_filetype1_menu");

	if (!menu) return;

	group_menus[GEANY_FILETYPE_GROUP_A_B] =     create_sub_menu(menu, "A-B");
	group_menus[GEANY_FILETYPE_GROUP_C] =       create_sub_menu(menu, "C");
	group_menus[GEANY_FILETYPE_GROUP_D_E_F] =   create_sub_menu(menu, "D-E-F");
	group_menus[GEANY_FILETYPE_GROUP_G_H_I] =   create_sub_menu(menu, "G-H-I");
	group_menus[GEANY_FILETYPE_GROUP_J_K_L] =   create_sub_menu(menu, "J-K-L");
	group_menus[GEANY_FILETYPE_GROUP_M_N_O] =   create_sub_menu(menu, "M-N-O");
	group_menus[GEANY_FILETYPE_GROUP_P_Q] =     create_sub_menu(menu, "P-Q");
	group_menus[GEANY_FILETYPE_GROUP_R_S] =     create_sub_menu(menu, "R-S");
	group_menus[GEANY_FILETYPE_GROUP_T_U_V_W] = create_sub_menu(menu, "T-U-V-W");
	group_menus[GEANY_FILETYPE_GROUP_X_Y_Z] =   create_sub_menu(menu, "X-Y-Z");

	foreach_slist(node, filetypes_by_title)
	{
		GeanyFiletype *ft = node ? node->data : NULL;
		if (ft && ft->group == GEANY_FILETYPE_GROUP_OTHER)
		{
			group_menus[GEANY_FILETYPE_GROUP_OTHER] = create_sub_menu(menu, _("Other"));
			break;
		}
	}

	foreach_slist(node, filetypes_by_title)
	{
		GeanyFiletype *ft = node ? node->data : NULL;
		if (!ft) continue;

		GtkWidget *parent = (ft->group != GEANY_FILETYPE_GROUP_NONE) ? group_menus[ft->group] : menu;

		if (config && ft->id == GEANY_FILETYPES_NONE)
			continue;

		if (config)
		{
			gchar *filename = filetypes_get_filename(ft, TRUE);
			if (filename) {
				ui_add_config_file_menu_item(filename, NULL, GTK_CONTAINER(parent));
				g_free(filename);
			}
		}
		else
		{
			create_radio_menu_item(parent, ft);
		}
	}
}


void filetypes_init(void)
{
	filetypes_init_types();
	create_set_filetype_menu(FALSE);
	setup_config_file_menus();
}


static guint match_basename(const GeanyFiletype *ft, const gchar *base_filename)
{
	if (G_UNLIKELY(!ft || ft->id == GEANY_FILETYPES_NONE || !ft->pattern || EMPTY(base_filename)))
		return 0;

	for (guint j = 0; ft->pattern[j] != NULL; j++)
	{
		gchar *pat = ft->pattern[j];
		if (pat && g_pattern_match_simple(pat, base_filename))
		{
			return strlen(pat);
		}
	}
	return 0;
}


static GeanyFiletype *detect_filetype_conf_file(const gchar *utf8_filename)
{
	if (EMPTY(utf8_filename)) return NULL;

	gchar *lfn = NULL;
	gchar *path;
	gboolean found = FALSE;

#ifdef G_OS_WIN32
	lfn = g_utf8_strdown(utf8_filename, -1);
#else
	lfn = g_strdup(utf8_filename);
#endif
	if (!lfn) return NULL;
	SETPTR(lfn, utils_get_locale_from_utf8(lfn));

	path = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, "filetypes.", NULL);
	if (path && lfn && g_str_has_prefix(lfn, path))
		found = TRUE;
	g_free(path);

	path = g_build_filename(app->datadir, GEANY_FILEDEFS_SUBDIR, "filetypes.", NULL);
	if (path && lfn && g_str_has_prefix(lfn, path))
		found = TRUE;

	g_free(path);
	g_free(lfn);
	return found ? filetypes[GEANY_FILETYPES_CONF] : NULL;
}


GeanyFiletype *filetypes_detect_from_extension(const gchar *utf8_filename)
{
	GeanyFiletype *ft = NULL;
	
	if (EMPTY(utf8_filename))
		return filetypes[GEANY_FILETYPES_SH]; // Safe fallback

	ft = detect_filetype_conf_file(utf8_filename);
	if (ft)
		return ft;

	gchar *base_filename = g_path_get_basename(utf8_filename);
	if (!base_filename)
		return filetypes[GEANY_FILETYPES_SH];

#ifdef G_OS_WIN32
	SETPTR(base_filename, g_utf8_strdown(base_filename, -1));
#endif

	guint plen = 0;
	for (guint i = 0; i < filetypes_array->len; i++)
	{
		GeanyFiletype *candidate = g_ptr_array_index(filetypes_array, i);
		if (!candidate) continue;

		guint mlen = match_basename(candidate, base_filename);
		
		if (mlen > plen)
		{
			plen = mlen;
			ft = candidate;
		}
		else if (mlen == plen && ft && !ft->priv->user_extensions &&
			candidate->priv->user_extensions)
		{
			ft = candidate;
		}
	}
	
	g_free(base_filename);

	// Enforce Shell (sh) as the robust fallback instead of Plain Text (None)
	if (ft == NULL)
		ft = filetypes[GEANY_FILETYPES_SH];

	return ft;
}


static gboolean shebang_find_and_match_filetype(const gchar *utf8_filename, gint first, ...)
{
	GeanyFiletype *ft = NULL;
	gint test;
	gboolean result = FALSE;
	va_list args;

	if (EMPTY(utf8_filename)) return FALSE;

	ft = filetypes_detect_from_extension(utf8_filename);
	if (ft == NULL || ft->id >= filetypes_array->len)
		return FALSE;

	va_start(args, first);
	test = first;
	while (test != -1)
	{
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

	if (EMPTY(line)) return NULL;

	if (strlen(line) > 2 && line[0] == '#' && line[1] == '!')
	{
		static const struct {
			const gchar *name;
			GeanyFiletypeID filetype;
		} interpreter_map[] = {
			{ "sh",		GEANY_FILETYPES_SH },
			{ "bash",	GEANY_FILETYPES_SH },
			{ "dash",	GEANY_FILETYPES_SH },
			{ "zsh",	GEANY_FILETYPES_SH },
			{ "fish",	GEANY_FILETYPES_SH },
			{ "ksh",	GEANY_FILETYPES_SH },
			{ "mksh",	GEANY_FILETYPES_SH },
			{ "csh",	GEANY_FILETYPES_SH },
			{ "tcsh",	GEANY_FILETYPES_SH },
			{ "ash",	GEANY_FILETYPES_SH },
			{ "raku",	GEANY_FILETYPES_RAKU },
			{ "perl6",	GEANY_FILETYPES_RAKU },
			{ "perl",	GEANY_FILETYPES_PERL },
			{ "python",	GEANY_FILETYPES_PYTHON },
			{ "php",	GEANY_FILETYPES_PHP },
			{ "ruby",	GEANY_FILETYPES_RUBY },
			{ "tcl",	GEANY_FILETYPES_TCL },
			{ "make",	GEANY_FILETYPES_MAKE },
			{ "dmd",	GEANY_FILETYPES_D },
			{ "wish",	GEANY_FILETYPES_TCL },
			{ "node",	GEANY_FILETYPES_JS },
			{ "rust",	GEANY_FILETYPES_RUST },
			{ "dart",	GEANY_FILETYPES_DART }
		};
		gchar *tmp = g_path_get_basename(line + 2);
		if (!tmp) return NULL;

		gchar *basename_interpreter = tmp;
		guint i;

		if (g_str_has_prefix(tmp, "env "))
		{
			basename_interpreter += 4;
		}
		else if (g_str_has_prefix(tmp, "busybox "))
		{
			basename_interpreter += 8;
		}

		for (i = 0; !ft && i < G_N_ELEMENTS(interpreter_map); i++)
		{
			if (g_str_has_prefix(basename_interpreter, interpreter_map[i].name))
				ft = filetypes[interpreter_map[i].filetype];
		}
		g_free(tmp);
	}
	else if (g_str_has_prefix(line, "<!DOCTYPE html") || g_str_has_prefix(line, "<html"))
	{
		if (EMPTY(utf8_filename) || !shebang_find_and_match_filetype(utf8_filename,
				GEANY_FILETYPES_PERL, GEANY_FILETYPES_PHP, GEANY_FILETYPES_PYTHON, -1))
		{
			ft = filetypes[GEANY_FILETYPES_HTML];
		}
	}
	else if (g_str_has_prefix(line, "<?xml"))
	{
		if (EMPTY(utf8_filename) || !shebang_find_and_match_filetype(utf8_filename,
				GEANY_FILETYPES_HTML, GEANY_FILETYPES_DOCBOOK,
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


static GeanyFiletype *filetypes_detect_from_file_internal(const gchar *utf8_filename, gchar **lines)
{
	GeanyFiletype	*ft = NULL;
	gint			 i;
	GRegex			*ft_regex;
	GMatchInfo		*match = NULL;
	GError			*regex_error = NULL;

	if (lines && lines[0])
	{
		ft = find_shebang(utf8_filename, lines[0]);
		if (ft != NULL)
			return ft;
	}

	ft_regex = g_regex_new(file_prefs.extract_filetype_regex,
			G_REGEX_RAW | G_REGEX_MULTILINE, 0, &regex_error);
	if (ft_regex != NULL && lines != NULL)
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
			if (match) {
				g_match_info_free(match);
				match = NULL;
			}
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

	if (EMPTY(utf8_filename))
		return filetypes[GEANY_FILETYPES_SH]; // Fallback to SH instead of None

	return filetypes_detect_from_extension(utf8_filename);
}


GeanyFiletype *filetypes_detect_from_document(GeanyDocument *doc)
{
	GeanyFiletype 	*ft;
	gchar 			*lines[GEANY_FILETYPE_SEARCH_LINES + 1];
	gint			 i;

	g_return_val_if_fail(doc == NULL || doc->is_valid, filetypes[GEANY_FILETYPES_SH]);

	if (doc == NULL || !doc->editor || !doc->editor->sci)
		return filetypes[GEANY_FILETYPES_SH];

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
	return ft ? ft : filetypes[GEANY_FILETYPES_SH];
}


#ifdef HAVE_PLUGINS
GEANY_API_SYMBOL
GeanyFiletype *filetypes_detect_from_file(const gchar *utf8_filename)
{
	gchar line[1024];
	gchar *lines[2];
	FILE  *f;
	
	if (EMPTY(utf8_filename))
		return filetypes[GEANY_FILETYPES_SH];

	gchar *locale_name = utils_get_locale_from_utf8(utf8_filename);
	if (!locale_name)
		return filetypes_detect_from_extension(utf8_filename);

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
	g_return_if_fail(ignore_callback); // standard macro wrapper check
	
	if (ft == NULL || !ft->priv || !ft->priv->menu_item)
		ft = filetypes[GEANY_FILETYPES_SH];

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ft->priv->menu_item), TRUE);
}


static void on_filetype_change(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	if (ignore_callback || doc == NULL || !gtk_check_menu_item_get_active(menuitem))
		return;

	document_set_filetype(doc, (GeanyFiletype*)user_data);
}


static void create_radio_menu_item(GtkWidget *menu, GeanyFiletype *ftype)
{
	static GSList *group = NULL;
	GtkWidget *tmp;

	if (!menu || !ftype || EMPTY(ftype->title)) return;

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

	if (!ft) return;

	g_free(ft->name);
	g_free(ft->title);
	g_free(ft->extension);
	g_free(ft->mime_type);
	g_free(ft->comment_open);
	g_free(ft->comment_close);
	g_free(ft->comment_single);
	g_free(ft->context_action_cmd);
	g_free(ft->error_regex_string);
	
	if (ft->icon)
		g_object_unref(ft->icon);
	g_strfreev(ft->pattern);

	if (ft->priv) {
		g_free(ft->priv->filecmds);
		g_free(ft->priv->ftdefcmds);
		g_free(ft->priv->execcmds);
		if (ft->priv->error_regex)
			g_regex_unref(ft->priv->error_regex);
		g_slist_free_full(ft->priv->tag_files, g_free);
		g_free(ft->priv);
	}
	g_free(ft);
}


void filetypes_free_types(void)
{
	if (!filetypes_array || !filetypes_hash) return;

	g_ptr_array_foreach(filetypes_array, filetype_free, NULL);
	g_ptr_array_free(filetypes_array, TRUE);
	g_hash_table_destroy(filetypes_hash);
	
	filetypes_array = NULL;
	filetypes_hash = NULL;
}


static void load_indent_settings(GeanyFiletype *ft, GKeyFile *config, GKeyFile *configh)
{
	if (!ft) return;
	ft->indent_width = utils_get_setting(integer, configh, config, "indentation", "width", -1);
	ft->indent_type = utils_get_setting(integer, configh, config, "indentation", "type", -1);
	
	switch (ft->indent_type)
	{
		case GEANY_INDENT_TYPE_TABS:
		case GEANY_INDENT_TYPE_SPACES:
		case GEANY_INDENT_TYPE_BOTH:
		case -1:
			break;

		default:
			g_warning("Invalid indent type %d in file type %s", ft->indent_type, ft->name ? ft->name : "unknown");
			ft->indent_type = -1;
			break;
	}
}


static void load_settings(guint ft_id, GKeyFile *config, GKeyFile *configh)
{
	if (ft_id >= filetypes_array->len) return;
	GeanyFiletype *ft = filetypes[ft_id];
	if (!ft) return;
	
	gchar *result;

	result = utils_get_setting(string, configh, config, "settings", "extension", NULL);
	if (result != NULL)
	{
		SETPTR(ft->extension, result);
	}

	result = utils_get_setting(string, configh, config, "settings", "mime_type", "text/plain");
	SETPTR(ft->mime_type, result);

	result = utils_get_setting(string, configh, config, "settings", "comment_open", NULL);
	if (result != NULL)
	{
		SETPTR(ft->comment_open, result);
	}

	result = utils_get_setting(string, configh, config, "settings", "comment_close", NULL);
	if (result != NULL)
	{
		SETPTR(ft->comment_close, result);
	}

	result = utils_get_setting(string, configh, config, "settings", "comment_single", NULL);
	if (result != NULL)
	{
		SETPTR(ft->comment_single, result);
	}
	else if (EMPTY(ft->comment_close))
	{
		SETPTR(ft->comment_single, ft->comment_open);
		ft->comment_open = NULL;
	}

	ft->comment_use_indent = utils_get_setting(boolean, configh, config,
			"settings", "comment_use_indent", FALSE);

	result = utils_get_setting(string, configh, config, "settings", "context_action_cmd", NULL);
	if (result != NULL)
	{
		SETPTR(ft->context_action_cmd, result);
	}

	result = utils_get_setting(string, configh, config, "settings", "tag_parser", NULL);
	if (result != NULL)
	{
		ft->lang = tm_source_file_get_named_lang(result);
		if (ft->lang == TM_PARSER_NONE)
			geany_debug("Cannot find tags parser '%s' for custom filetype '%s'.", result, ft->name ? ft->name : "");
		g_free(result);
	}

	result = utils_get_setting(string, configh, config, "settings", "lexer_filetype", NULL);
	if (result != NULL)
	{
		ft->lexer_filetype = filetypes_lookup_by_name(result);
		if (!ft->lexer_filetype)
			geany_debug("Cannot find lexer filetype '%s' for custom filetype '%s'.", result, ft->name ? ft->name : "");
		g_free(result);
	}

	if (ft->priv) {
		ft->priv->symbol_list_sort_mode = utils_get_setting(integer, configh, config, "settings",
			"symbol_list_sort_mode", SYMBOLS_SORT_USE_PREVIOUS);
		ft->priv->xml_indent_tags = utils_get_setting(boolean, configh, config, "settings",
			"xml_indent_tags", FALSE);
	}

	load_indent_settings(ft, config, configh);

	build_load_menu(config, GEANY_BCS_FT, (gpointer)ft);
	build_load_menu(configh, GEANY_BCS_HOME_FT, (gpointer)ft);
}


static void copy_keys(GKeyFile *dest, const gchar *dest_group, GKeyFile *src, const gchar *src_group)
{
	if (!dest || !src || EMPTY(dest_group) || EMPTY(src_group)) return;

	gchar **keys = g_key_file_get_keys(src, src_group, NULL, NULL);
	if (!keys) return;

	gchar **ptr;
	foreach_strv(ptr, keys)
	{
		gchar *key = *ptr;
		if (!key) continue;
		gchar *value = g_key_file_get_value(src, src_group, key, NULL);
		if (value) {
			g_key_file_set_value(dest, dest_group, key, value);
			g_free(value);
		}
	}
	g_strfreev(keys);
}


static gchar *filetypes_get_filename(GeanyFiletype *ft, gboolean user)
{
	if (!ft) return NULL;
	gchar *ext = filetypes_get_conf_extension(ft);
	if (!ext) return NULL;

	gchar *base_name = g_strconcat("filetypes.", ext, NULL);
	g_free(ext);
	if (!base_name) return NULL;

	gchar *file_name = user ? 
		g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, base_name, NULL) :
		g_build_filename(app->datadir, GEANY_FILEDEFS_SUBDIR, base_name, NULL);

	g_free(base_name);
	return file_name;
}


static void add_group_keys(GKeyFile *kf, const gchar *group, GeanyFiletype *ft)
{
	if (!kf || EMPTY(group) || !ft) return;

	gchar *files[2];
	gboolean loaded = FALSE;
	guint i;

	files[0] = filetypes_get_filename(ft, FALSE);
	files[1] = filetypes_get_filename(ft, TRUE);

	for (i = 0; i < G_N_ELEMENTS(files); i++)
	{
		if (!files[i]) continue;
		GKeyFile *src = g_key_file_new();

		if (g_key_file_load_from_file(src, files[i], G_KEY_FILE_NONE, NULL))
		{
			copy_keys(kf, group, src, group);
			loaded = TRUE;
		}
		g_key_file_free(src);
	}

	if (!loaded)
		geany_debug("Could not read config file configuration for [%s]!", group);

	g_free(files[0]);
	g_free(files[1]);
}


static void copy_ft_groups(GKeyFile *kf)
{
	if (!kf) return;
	gchar **groups = g_key_file_get_groups(kf, NULL);
	if (!groups) return;

	gchar **ptr;
	foreach_strv(ptr, groups)
	{
		gchar *group = *ptr;
		if (!group) continue;

		gchar *name = strchr(group, '=');
		if (!name || !name[1]) 
			continue;

		gchar *old_group = g_strdup(group);
		if (!old_group) continue;

		*name = 0;
		name++;

		GeanyFiletype *ft = filetypes_lookup_by_name(name);
		if (ft)
		{
			add_group_keys(kf, group, ft);
			copy_keys(kf, group, kf, old_group);
		}
		g_free(old_group);
	}
	g_strfreev(groups);
}


static void load_system_keyfile(GKeyFile *key_file, const gchar *file, GKeyFileFlags flags, GeanyFiletype *ft)
{
	if (!key_file || EMPTY(file) || !ft) return;
	GError *error = NULL;
	gboolean done = g_key_file_load_from_file(key_file, file, flags, &error);

	if (error != NULL)
	{
		if (!done && ft->priv && !ft->priv->custom)
			geany_debug("Failed to open %s (%s)", file, error->message);

		g_error_free(error);
	}
}


void filetypes_load_config(guint ft_id, gboolean reload)
{
	GKeyFile *config, *config_home;
	GeanyFiletypePrivate *pft;
	GeanyFiletype *ft;

	if (ft_id >= filetypes_array->len) return;

	ft = filetypes[ft_id];
	if (!ft || !ft->priv) return;
	pft = ft->priv;

	if (G_UNLIKELY(reload && !pft->keyfile_loaded))
		return;

	if (G_LIKELY(!reload && pft->keyfile_loaded))
		return;
	pft->keyfile_loaded = TRUE;

	config = g_key_file_new();
	config_home = g_key_file_new();
	{
		gchar *f = filetypes_get_filename(ft, FALSE);
		if (f) {
			load_system_keyfile(config, f, G_KEY_FILE_KEEP_COMMENTS, ft);
			g_free(f);
		}

		f = filetypes_get_filename(ft, TRUE);
		if (f) {
			g_key_file_load_from_file(config_home, f, G_KEY_FILE_KEEP_COMMENTS, NULL);
			g_free(f);
		}
	}
	
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
	if (!ft) return NULL;

	if (ft->priv && ft->priv->custom)
		return g_strconcat(ft->name, ".conf", NULL);

	switch (ft->id)
	{
		case GEANY_FILETYPES_CPP: return g_strdup("cpp");
		case GEANY_FILETYPES_CS: return g_strdup("cs");
		case GEANY_FILETYPES_MAKE: return g_strdup("makefile");
		case GEANY_FILETYPES_NONE: return g_strdup("common");
		case GEANY_FILETYPES_MATLAB: return g_strdup("matlab");
		case GEANY_FILETYPES_OBJECTIVEC: return g_strdup("objectivec");
		default:
			return ft->name ? g_ascii_strdown(ft->name, -1) : g_strdup("common");
	}
}


void filetypes_save_commands(GeanyFiletype *ft)
{
	if (!ft) return;
	GKeyFile *config_home;
	gchar *fname, *data;

	fname = filetypes_get_filename(ft, TRUE);
	if (!fname) return;

	config_home = g_key_file_new();
	g_key_file_load_from_file(config_home, fname, G_KEY_FILE_KEEP_COMMENTS, NULL);
	build_save_menu(config_home, ft, GEANY_BCS_HOME_FT);
	data = g_key_file_to_data(config_home, NULL, NULL);
	if (data) {
		utils_write_file(fname, data);
		g_free(data);
	}
	g_key_file_free(config_home);
	g_free(fname);
}


GtkFileFilter *filetypes_create_file_filter_all_source(void)
{
	GtkFileFilter *new_filter;
	guint i, j;
	guint n_patterns = 0;

	new_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(new_filter, _("All Source"));

	for (i = 0; filetypes_array && i < filetypes_array->len; i++)
	{
		GeanyFiletype *ft = g_ptr_array_index(filetypes_array, i);
		if (!ft || G_UNLIKELY(i == GEANY_FILETYPES_NONE) || !ft->pattern)
			continue;

		for (j = 0; ft->pattern[j]; j++)
		{
			gtk_file_filter_add_pattern(new_filter, ft->pattern[j]);
		}
		n_patterns += j;
	}

	if (n_patterns == 0)
	{
		g_object_unref(new_filter);
		new_filter = NULL;
	}

	return new_filter;
}


GtkFileFilter *filetypes_create_file_filter(const GeanyFiletype *ft)
{
	GtkFileFilter *new_filter;
	gint i;
	const gchar *title;

	g_return_val_if_fail(ft != NULL, NULL);

	if (!ft->pattern || !ft->pattern[0])
	{
		return NULL;
	}

	new_filter = gtk_file_filter_new();
	title = ft->id == GEANY_FILETYPES_NONE ? _("All files") : ft->title;
	gtk_file_filter_set_name(new_filter, title ? title : _("Files"));

	for (i = 0; ft->pattern[i]; i++)
	{
		gtk_file_filter_add_pattern(new_filter, ft->pattern[i]);
	}

	return new_filter;
}


gboolean filetype_has_tags(GeanyFiletype *ft)
{
	g_return_val_if_fail(ft != NULL, FALSE);
	return ft->lang != TM_PARSER_NONE;
}


GEANY_API_SYMBOL
GeanyFiletype *filetypes_lookup_by_name(const gchar *name)
{
	if (EMPTY(name) || !filetypes_hash) return NULL;
	return g_hash_table_lookup(filetypes_hash, name);
}


static void compile_regex(GeanyFiletype *ft, gchar *regstr)
{
	if (!ft || !ft->priv || EMPTY(regstr)) return;
	GError *error = NULL;
	GRegex *regex = g_regex_new(regstr, 0, 0, &error);

	if (!regex)
	{
		ui_set_statusbar(TRUE, _("Bad regex for filetype %s: %s"),
			filetypes_get_display_name(ft), error->message ? error->message : "unknown");
		g_error_free(error);
	}
	if (ft->priv->error_regex)
		g_regex_unref(ft->priv->error_regex);
	ft->priv->error_regex = regex;
}


gboolean filetypes_parse_error_message(GeanyFiletype *ft, const gchar *message, gchar **filename, gint *line)
{
	if (EMPTY(message) || !filename || !line) return FALSE;

	gchar *regstr;
	gchar **tmp;
	GeanyDocument *doc;
	GMatchInfo *minfo = NULL;
	gint i, n_match_groups;
	gchar *first = NULL, *second = NULL;

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
		if (minfo) g_match_info_free(minfo);
		return FALSE;
	}

	n_match_groups = g_match_info_get_match_count(minfo);
	
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
		if (*end == '\0')
		{
			*line = (gint)l;
			g_free(first);
			*filename = second;
		}
		else
		{
			l = strtol(second, &end, 10);
			if (*end == '\0')
			{
				*line = (gint)l;
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
	{
		g_free(first);
	}

	if (minfo) g_match_info_free(minfo);
	return *filename != NULL;
}


#ifdef G_OS_WIN32
static void convert_filetype_extensions_to_lower_case(gchar **patterns, gsize len)
{
	if (!patterns) return;
	guint i;
	for (i = 0; i < len; i++)
	{
		if (patterns[i])
			SETPTR(patterns[i], g_ascii_strdown(patterns[i], -1));
	}
}
#endif


static void read_extensions(GKeyFile *sysconfig, GKeyFile *userconfig)
{
	if (!filetypes_array) return;
	guint i;
	gsize len = 0;

	for (i = 0; i < filetypes_array->len; i++)
	{
		GeanyFiletype *ft = g_ptr_array_index(filetypes_array, i);
		if (!ft || !ft->priv || !ft->name) continue;

		gboolean userset = userconfig ?
			g_key_file_has_key(userconfig, "Extensions", ft->name, NULL) : FALSE;
			
		GKeyFile *target_config = (userset && userconfig) ? userconfig : sysconfig;
		gchar **list = target_config ?
			g_key_file_get_string_list(target_config, "Extensions", ft->name, &len, NULL) : NULL;

		ft->priv->user_extensions = userset;
		g_strfreev(ft->pattern);
		
		if (!list)
			list = g_new0(gchar*, 1);
		ft->pattern = list;

#ifdef G_OS_WIN32
		convert_filetype_extensions_to_lower_case(ft->pattern, len);
#endif
	}
}


static void read_filetype_config(void)
{
	gchar *sysconfigfile = g_build_filename(app->datadir, "filetype_extensions.conf", NULL);
	gchar *userconfigfile = g_build_filename(app->configdir, "filetype_extensions.conf", NULL);
	GKeyFile *sysconfig = g_key_file_new();
	GKeyFile *userconfig = g_key_file_new();

	if (sysconfigfile) {
		g_key_file_load_from_file(sysconfig, sysconfigfile, G_KEY_FILE_NONE, NULL);
		g_free(sysconfigfile);
	}
	if (userconfigfile) {
		g_key_file_load_from_file(userconfig, userconfigfile, G_KEY_FILE_NONE, NULL);
		g_free(userconfigfile);
	}

	read_extensions(sysconfig, userconfig);

	g_key_file_free(sysconfig);
	g_key_file_free(userconfig);
}


void filetypes_reload_extensions(void)
{
	guint i;
	read_filetype_config();

	foreach_document(i)
	{
		GeanyDocument *doc = documents[i];
		if (!doc || !doc->file_type) continue;
		
		if (doc->file_type->id != GEANY_FILETYPES_NONE && doc->file_type->id != GEANY_FILETYPES_SH)
			continue;
			
		document_set_filetype(doc, filetypes_detect_from_document(doc));
	}
}


GEANY_API_SYMBOL
GeanyFiletype *filetypes_index(gint idx)
{
	if (!filetypes_array) return NULL;
	return (idx >= 0 && idx < (gint) filetypes_array->len) ? g_ptr_array_index(filetypes_array, idx) : NULL;
}


void filetypes_reload(void)
{
	guint i;
	GeanyDocument *current_doc;

	if (filetypes_array) {
		for (i = 0; i < filetypes_array->len; i++)
		{
			filetypes_load_config(i, TRUE);
		}
	}

	current_doc = document_get_current();
	if (!current_doc)
		return;

	foreach_document(i)
	{
		if (documents[i] && current_doc != documents[i])
			document_reload_config(documents[i]);
	}
	
	document_reload_config(current_doc);
}


GEANY_API_SYMBOL
const gchar *filetypes_get_display_name(GeanyFiletype *ft)
{
	if (!ft) return _("None");
	return ft->id == GEANY_FILETYPES_NONE ? _("None") : (ft->name ? ft->name : _("Unknown"));
}


gboolean filetype_get_comment_open_close(const GeanyFiletype *ft, gboolean single_first, const gchar **co, const gchar **cc)
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

static void *copy_(void *src) { return src; }
static void free_(G_GNUC_UNUSED void *doc) { }

GEANY_API_SYMBOL
GType filetype_get_type(void);

G_DEFINE_BOXED_TYPE(GeanyFiletype, filetype, copy_, free_);
