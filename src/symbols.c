/*
 *      symbols.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2011-2012 Colomban Wendling <ban(at)herbesfolles(dot)org>
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
 * @file symbols.h
 * Tag-related functions.
 **/

/*
 * Symbol Tree and TagManager-related convenience functions.
 * TagManager parses tags for each document, and also adds them to the workspace (session).
 * Global tags are lists of tags for each filetype, loaded when a document with a
 * matching filetype is first loaded.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "symbols.h"

#include "app.h"
#include "callbacks.h" /* FIXME: for ignore_callback */
#include "documentprivate.h"
#include "editor.h"
#include "encodings.h"
#include "filetypesprivate.h"
#include "geanyobject.h"
#include "highlighting.h"
#include "main.h"
#include "navqueue.h"
#include "sciwrappers.h"
#include "sidebar.h"
#include "support.h"
#include "tm_parser.h"
#include "tm_tag.h"
#include "ui_utils.h"
#include "utils.h"

#include "SciLexer.h"

#include "gtkcompat.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>


typedef struct
{
	gint found_line; /* return: the nearest line found */
	gint line;       /* input: the line to look for */
	gboolean lower   /* input: search only for lines with lower number than @line */;
} TreeSearchData;


static GPtrArray *top_level_iter_names = NULL;

enum
{
	ICON_CLASS,
	ICON_MACRO,
	ICON_MEMBER,
	ICON_METHOD,
	ICON_NAMESPACE,
	ICON_OTHER,
	ICON_STRUCT,
	ICON_VAR,
	ICON_NONE,
	N_ICONS = ICON_NONE
};

static struct
{
	const gchar *icon_name;
	GdkPixbuf *pixbuf;
}
symbols_icons[N_ICONS] = {
	[ICON_CLASS]		= { "classviewer-class", NULL },
	[ICON_MACRO]		= { "classviewer-macro", NULL },
	[ICON_MEMBER]		= { "classviewer-member", NULL },
	[ICON_METHOD]		= { "classviewer-method", NULL },
	[ICON_NAMESPACE]	= { "classviewer-namespace", NULL },
	[ICON_OTHER]		= { "classviewer-other", NULL },
	[ICON_STRUCT]		= { "classviewer-struct", NULL },
	[ICON_VAR]			= { "classviewer-var", NULL },
};

static struct
{
	GtkWidget *expand_all;
	GtkWidget *collapse_all;
	GtkWidget *sort_by_name;
	GtkWidget *sort_by_appearance;
	GtkWidget *find_usage;
	GtkWidget *find_doc_usage;
	GtkWidget *find_in_files;
}
symbol_menu;

static void load_user_tags(GeanyFiletypeID ft_id);

/* get the tags_ignore list, exported by tagmanager's options.c */
extern gchar **c_tags_ignore;

/* ignore certain tokens when parsing C-like syntax.
 * Also works for reloading. */
static void load_c_ignore_tags(void)
{
	gchar *path = g_build_filename(app->configdir, "ignore.tags", NULL);
	gchar *content;

	if (g_file_get_contents(path, &content, NULL, NULL))
	{
		/* historically we ignore the glib _DECLS for tag generation */
		SETPTR(content, g_strconcat("G_BEGIN_DECLS G_END_DECLS\n", content, NULL));

		g_strfreev(c_tags_ignore);
		c_tags_ignore = g_strsplit_set(content, " \n\r", -1);
		g_free(content);
	}
	g_free(path);
}


void symbols_reload_config_files(void)
{
	load_c_ignore_tags();
}


static gsize get_tag_count(void)
{
	GPtrArray *tags = tm_get_workspace()->global_tags;
	gsize count = tags ? tags->len : 0;

	return count;
}


/* wrapper for tm_workspace_load_global_tags().
 * note that the tag count only counts new global tags added - if a tag has the same name,
 * currently it replaces the existing tag, so loading a file twice will say 0 tags the 2nd time. */
static gboolean symbols_load_global_tags(const gchar *tags_file, GeanyFiletype *ft)
{
	gboolean result;
	gsize old_tag_count = get_tag_count();

	result = tm_workspace_load_global_tags(tags_file, ft->lang);
	if (result)
	{
		geany_debug("Loaded %s (%s), %u symbol(s).", tags_file, ft->name,
			(guint) (get_tag_count() - old_tag_count));
	}
	return result;
}


/* Ensure that the global tags file(s) for the file_type_idx filetype is loaded.
 * This provides autocompletion, calltips, etc. */
void symbols_global_tags_loaded(guint file_type_idx)
{
	/* load ignore list for C/C++ parser */
	if ((file_type_idx == GEANY_FILETYPES_C || file_type_idx == GEANY_FILETYPES_CPP) &&
		c_tags_ignore == NULL)
	{
		load_c_ignore_tags();
	}

	if (cl_options.ignore_global_tags || app->tm_workspace == NULL)
		return;

	/* load config in case of custom filetypes */
	filetypes_load_config(file_type_idx, FALSE);

	load_user_tags(file_type_idx);

	switch (file_type_idx)
	{
		case GEANY_FILETYPES_CPP:
			symbols_global_tags_loaded(GEANY_FILETYPES_C);	/* load C global tags */
			break;
		case GEANY_FILETYPES_PHP:
			symbols_global_tags_loaded(GEANY_FILETYPES_HTML);	/* load HTML global tags */
			break;
	}
}


GString *symbols_find_typenames_as_string(TMParserType lang, gboolean global)
{
	guint j;
	TMTag *tag;
	GString *s = NULL;
	GPtrArray *typedefs;
	TMParserType tag_lang;

	if (global)
		typedefs = app->tm_workspace->global_typename_array;
	else
		typedefs = app->tm_workspace->typename_array;

	if ((typedefs) && (typedefs->len > 0))
	{
		const gchar *last_name = "";

		s = g_string_sized_new(typedefs->len * 10);
		for (j = 0; j < typedefs->len; ++j)
		{
			tag = TM_TAG(typedefs->pdata[j]);
			tag_lang = tag->lang;

			if (tag->name && tm_parser_langs_compatible(lang, tag_lang) &&
				strcmp(tag->name, last_name) != 0)
			{
				if (j != 0)
					g_string_append_c(s, ' ');
				g_string_append(s, tag->name);
				last_name = tag->name;
			}
		}
	}
	return s;
}


/** Gets the context separator used by the tag manager for a particular file
 * type.
 * @param ft_id File type identifier.
 * @return The context separator string.
 *
 * Returns non-printing sequence "\x03" ie ETX (end of text) for filetypes
 * without a context separator.
 *
 * @since 0.19
 */
GEANY_API_SYMBOL
const gchar *symbols_get_context_separator(gint ft_id)
{
	return tm_parser_context_separator(filetypes[ft_id]->lang);
}


/* sort by name, then line */
static gint compare_symbol(const TMTag *tag_a, const TMTag *tag_b)
{
	gint ret;

	if (tag_a == NULL || tag_b == NULL)
		return 0;

	if (tag_a->name == NULL)
		return -(tag_a->name != tag_b->name);

	if (tag_b->name == NULL)
		return tag_a->name != tag_b->name;

	ret = strcmp(tag_a->name, tag_b->name);
	if (ret == 0)
	{
		return tag_a->line - tag_b->line;
	}
	return ret;
}


/* sort by line, then scope */
static gint compare_symbol_lines(gconstpointer a, gconstpointer b)
{
	const TMTag *tag_a = TM_TAG(a);
	const TMTag *tag_b = TM_TAG(b);
	gint ret;

	if (a == NULL || b == NULL)
		return 0;

	ret = tag_a->line - tag_b->line;
	if (ret == 0)
	{
		if (tag_a->scope == NULL)
			return -(tag_a->scope != tag_b->scope);
		if (tag_b->scope == NULL)
			return tag_a->scope != tag_b->scope;
		else
			return strcmp(tag_a->scope, tag_b->scope);
	}
	return ret;
}


static GList *get_tag_list(GeanyDocument *doc, TMTagType tag_types)
{
	GList *tag_names = NULL;
	guint i;

	g_return_val_if_fail(doc, NULL);

	if (! doc->tm_file || ! doc->tm_file->tags_array)
		return NULL;

	for (i = 0; i < doc->tm_file->tags_array->len; ++i)
	{
		TMTag *tag = TM_TAG(doc->tm_file->tags_array->pdata[i]);

		if (G_UNLIKELY(tag == NULL))
			return NULL;

		if (tag->type & tag_types)
		{
			tag_names = g_list_prepend(tag_names, tag);
		}
	}
	tag_names = g_list_sort(tag_names, compare_symbol_lines);
	return tag_names;
}


/* amount of types in the symbol list (currently max. 8 are used) */
#define MAX_SYMBOL_TYPES	(sizeof(tv_iters) / sizeof(GtkTreeIter))

struct TreeviewSymbols
{
	GtkTreeIter		 tag_function;
	GtkTreeIter		 tag_class;
	GtkTreeIter		 tag_macro;
	GtkTreeIter		 tag_member;
	GtkTreeIter		 tag_variable;
	GtkTreeIter		 tag_externvar;
	GtkTreeIter		 tag_namespace;
	GtkTreeIter		 tag_struct;
	GtkTreeIter		 tag_interface;
	GtkTreeIter		 tag_type;
	GtkTreeIter		 tag_other;
} tv_iters;


static void init_tag_iters(void)
{
	/* init all GtkTreeIters with -1 to make them invalid to avoid crashes when switching between
	 * filetypes(e.g. config file to Python crashes Geany without this) */
	tv_iters.tag_function.stamp = -1;
	tv_iters.tag_class.stamp = -1;
	tv_iters.tag_member.stamp = -1;
	tv_iters.tag_macro.stamp = -1;
	tv_iters.tag_variable.stamp = -1;
	tv_iters.tag_externvar.stamp = -1;
	tv_iters.tag_namespace.stamp = -1;
	tv_iters.tag_struct.stamp = -1;
	tv_iters.tag_interface.stamp = -1;
	tv_iters.tag_type.stamp = -1;
	tv_iters.tag_other.stamp = -1;
}


static GdkPixbuf *get_tag_icon(const gchar *icon_name)
{
	static GtkIconTheme *icon_theme = NULL;
	static gint x = -1;

	if (G_UNLIKELY(x < 0))
	{
		gint dummy;
		icon_theme = gtk_icon_theme_get_default();
		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &x, &dummy);
	}
	return gtk_icon_theme_load_icon(icon_theme, icon_name, x, 0, NULL);
}


static gboolean find_toplevel_iter(GtkTreeStore *store, GtkTreeIter *iter, const gchar *title)
{
	GtkTreeModel *model = GTK_TREE_MODEL(store);

	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;
	do
	{
		gchar *candidate;

		gtk_tree_model_get(model, iter, SYMBOLS_COLUMN_NAME, &candidate, -1);
		/* FIXME: what if 2 different items have the same name?
		 * this should never happen, but might be caused by a typo in a translation */
		if (utils_str_equal(candidate, title))
		{
			g_free(candidate);
			return TRUE;
		}
		else
			g_free(candidate);
	}
	while (gtk_tree_model_iter_next(model, iter));

	return FALSE;
}


/* Adds symbol list groups in (iter*, title) pairs.
 * The list must be ended with NULL. */
static void G_GNUC_NULL_TERMINATED
tag_list_add_groups(GtkTreeStore *tree_store, ...)
{
	va_list args;
	GtkTreeIter *iter;

	g_return_if_fail(top_level_iter_names);

	va_start(args, tree_store);
	for (; iter = va_arg(args, GtkTreeIter*), iter != NULL;)
	{
		gchar *title = va_arg(args, gchar*);
		guint icon_id = va_arg(args, guint);
		GdkPixbuf *icon = NULL;

		if (icon_id < N_ICONS)
			icon = symbols_icons[icon_id].pixbuf;

		g_assert(title != NULL);
		g_ptr_array_add(top_level_iter_names, title);

		if (!find_toplevel_iter(tree_store, iter, title))
			gtk_tree_store_append(tree_store, iter, NULL);

		if (icon)
			gtk_tree_store_set(tree_store, iter, SYMBOLS_COLUMN_ICON, icon, -1);
		gtk_tree_store_set(tree_store, iter, SYMBOLS_COLUMN_NAME, title, -1);
	}
	va_end(args);
}


static void add_top_level_items(GeanyDocument *doc)
{
	GeanyFiletypeID ft_id = doc->file_type->id;
	GtkTreeStore *tag_store = doc->priv->tag_store;

	if (top_level_iter_names == NULL)
		top_level_iter_names = g_ptr_array_new();
	else
		g_ptr_array_set_size(top_level_iter_names, 0);

	init_tag_iters();

	switch (ft_id)
	{
		case GEANY_FILETYPES_DIFF:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Files"), ICON_NONE, NULL);
			break;
		}
		case GEANY_FILETYPES_DOCBOOK:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Chapter"), ICON_NONE,
				&(tv_iters.tag_class), _("Section"), ICON_NONE,
				&(tv_iters.tag_member), _("Sect1"), ICON_NONE,
				&(tv_iters.tag_macro), _("Sect2"), ICON_NONE,
				&(tv_iters.tag_variable), _("Sect3"), ICON_NONE,
				&(tv_iters.tag_struct), _("Appendix"), ICON_NONE,
				&(tv_iters.tag_other), _("Other"), ICON_NONE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_HASKELL:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_namespace, _("Module"), ICON_NONE,
				&tv_iters.tag_type, _("Types"), ICON_NONE,
				&tv_iters.tag_macro, _("Type constructors"), ICON_NONE,
				&tv_iters.tag_function, _("Functions"), ICON_METHOD,
				NULL);
			break;
		case GEANY_FILETYPES_COBOL:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_class, _("Program"), ICON_CLASS,
				&tv_iters.tag_function, _("File"), ICON_METHOD,
				&tv_iters.tag_namespace, _("Sections"), ICON_NAMESPACE,
				&tv_iters.tag_macro, _("Paragraph"), ICON_OTHER,
				&tv_iters.tag_struct, _("Group"), ICON_STRUCT,
				&tv_iters.tag_variable, _("Data"), ICON_VAR,
				NULL);
			break;
		case GEANY_FILETYPES_CONF:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_namespace, _("Sections"), ICON_OTHER,
				&tv_iters.tag_macro, _("Keys"), ICON_VAR,
				NULL);
			break;
		case GEANY_FILETYPES_NSIS:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_namespace, _("Sections"), ICON_OTHER,
				&tv_iters.tag_function, _("Functions"), ICON_METHOD,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				NULL);
			break;
		case GEANY_FILETYPES_LATEX:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Command"), ICON_NONE,
				&(tv_iters.tag_class), _("Environment"), ICON_NONE,
				&(tv_iters.tag_member), _("Section"), ICON_NONE,
				&(tv_iters.tag_macro), _("Subsection"), ICON_NONE,
				&(tv_iters.tag_variable), _("Subsubsection"), ICON_NONE,
				&(tv_iters.tag_struct), _("Label"), ICON_NONE,
				&(tv_iters.tag_namespace), _("Chapter"), ICON_NONE,
				&(tv_iters.tag_other), _("Other"), ICON_NONE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_MATLAB:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_struct), _("Structures"), ICON_STRUCT,
				NULL);
			break;
		}
		case GEANY_FILETYPES_ABAQUS:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_class), _("Parts"), ICON_NONE,
				&(tv_iters.tag_member), _("Assembly"), ICON_NONE,
				&(tv_iters.tag_namespace), _("Steps"), ICON_NONE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_R:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_other), _("Other"), ICON_NONE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_RUST:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Modules"), ICON_NAMESPACE,
				&(tv_iters.tag_struct), _("Structures"), ICON_STRUCT,
				&(tv_iters.tag_interface), _("Traits"), ICON_CLASS,
				&(tv_iters.tag_class), _("Implementations"), ICON_CLASS,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_type), _("Typedefs / Enums"), ICON_STRUCT,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_macro), _("Macros"), ICON_MACRO,
				&(tv_iters.tag_member), _("Methods"), ICON_MEMBER,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_GO:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), ICON_NAMESPACE,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_interface), _("Interfaces"), ICON_STRUCT,
				&(tv_iters.tag_struct), _("Structs"), ICON_STRUCT,
				&(tv_iters.tag_type), _("Types"), ICON_STRUCT,
				&(tv_iters.tag_macro), _("Constants"), ICON_MACRO,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_member), _("Members"), ICON_MEMBER,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_PERL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), ICON_NAMESPACE,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_macro), _("Labels"), ICON_NONE,
				&(tv_iters.tag_type), _("Constants"), ICON_NONE,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_PHP:
		case GEANY_FILETYPES_ZEPHIR:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Namespaces"), ICON_NAMESPACE,
				&(tv_iters.tag_interface), _("Interfaces"), ICON_STRUCT,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_macro), _("Constants"), ICON_MACRO,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_struct), _("Traits"), ICON_STRUCT,
				NULL);
			break;
		}
		case GEANY_FILETYPES_HTML:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), ICON_NONE,
				&(tv_iters.tag_member), _("Anchors"), ICON_NONE,
				&(tv_iters.tag_namespace), _("H1 Headings"), ICON_NONE,
				&(tv_iters.tag_class), _("H2 Headings"), ICON_NONE,
				&(tv_iters.tag_variable), _("H3 Headings"), ICON_NONE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_CSS:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_variable), _("ID Selectors"), ICON_VAR,
				&(tv_iters.tag_struct), _("Type Selectors"), ICON_STRUCT, NULL);
			break;
		}
		case GEANY_FILETYPES_REST:
		case GEANY_FILETYPES_TXT2TAGS:
		case GEANY_FILETYPES_ABC:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Chapter"), ICON_NONE,
				&(tv_iters.tag_member), _("Section"), ICON_NONE,
				&(tv_iters.tag_macro), _("Subsection"), ICON_NONE,
				&(tv_iters.tag_variable), _("Subsubsection"), ICON_NONE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_ASCIIDOC:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Document"), ICON_NONE,
				&(tv_iters.tag_member), _("Section Level 1"), ICON_NONE,
				&(tv_iters.tag_macro), _("Section Level 2"), ICON_NONE,
				&(tv_iters.tag_variable), _("Section Level 3"), ICON_NONE,
				&(tv_iters.tag_struct), _("Section Level 4"), ICON_NONE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_RUBY:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Modules"), ICON_NAMESPACE,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_member), _("Singletons"), ICON_STRUCT,
				&(tv_iters.tag_function), _("Methods"), ICON_METHOD,
				NULL);
			break;
		}
		case GEANY_FILETYPES_TCL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Namespaces"), ICON_NAMESPACE,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_member), _("Methods"), ICON_METHOD,
				&(tv_iters.tag_function), _("Procedures"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_member), _("Methods"), ICON_MACRO,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_externvar), _("Imports"), ICON_NAMESPACE,
				NULL);
			break;
		}
		case GEANY_FILETYPES_VHDL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), ICON_NAMESPACE,
				&(tv_iters.tag_class), _("Entities"), ICON_CLASS,
				&(tv_iters.tag_struct), _("Architectures"), ICON_STRUCT,
				&(tv_iters.tag_type), _("Types"), ICON_OTHER,
				&(tv_iters.tag_function), _("Functions / Procedures"), ICON_METHOD,
				&(tv_iters.tag_variable), _("Variables / Signals"), ICON_VAR,
				&(tv_iters.tag_member), _("Processes / Blocks / Components"), ICON_MEMBER,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_VERILOG:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_type), _("Events"), ICON_MACRO,
				&(tv_iters.tag_class), _("Modules"), ICON_CLASS,
				&(tv_iters.tag_function), _("Functions / Tasks"), ICON_METHOD,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_JAVA:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), ICON_NAMESPACE,
				&(tv_iters.tag_interface), _("Interfaces"), ICON_STRUCT,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_function), _("Methods"), ICON_METHOD,
				&(tv_iters.tag_member), _("Members"), ICON_MEMBER,
				&(tv_iters.tag_type), _("Enums"), ICON_STRUCT,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_AS:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), ICON_NAMESPACE,
				&(tv_iters.tag_interface), _("Interfaces"), ICON_STRUCT,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_member), _("Properties"), ICON_MEMBER,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_macro), _("Constants"), ICON_MACRO,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_HAXE:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_interface), _("Interfaces"), ICON_STRUCT,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_function), _("Methods"), ICON_METHOD,
				&(tv_iters.tag_type), _("Types"), ICON_MACRO,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_BASIC:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_macro), _("Constants"), ICON_MACRO,
				&(tv_iters.tag_struct), _("Types"), ICON_NAMESPACE,
				&(tv_iters.tag_namespace), _("Labels"), ICON_MEMBER,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_F77:
		case GEANY_FILETYPES_FORTRAN:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Module"), ICON_CLASS,
				&(tv_iters.tag_struct), _("Programs"), ICON_CLASS,
				&(tv_iters.tag_interface), _("Interfaces"), ICON_STRUCT,
				&(tv_iters.tag_function), _("Functions / Subroutines"), ICON_METHOD,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_class), _("Types"), ICON_CLASS,
				&(tv_iters.tag_member), _("Components"), ICON_MEMBER,
				&(tv_iters.tag_macro), _("Blocks"), ICON_MEMBER,
				&(tv_iters.tag_type), _("Enums"), ICON_STRUCT,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				NULL);
			break;
		}
		case GEANY_FILETYPES_ASM:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Labels"), ICON_NAMESPACE,
				&(tv_iters.tag_function), _("Macros"), ICON_METHOD,
				&(tv_iters.tag_macro), _("Defines"), ICON_MACRO,
				&(tv_iters.tag_struct), _("Types"), ICON_STRUCT,
				NULL);
			break;
		}
		case GEANY_FILETYPES_MAKE:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_function, _("Targets"), ICON_METHOD,
				&tv_iters.tag_macro, _("Macros"), ICON_MACRO,
				NULL);
			break;
		case GEANY_FILETYPES_SQL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_namespace), _("Procedures"), ICON_NAMESPACE,
				&(tv_iters.tag_struct), _("Indexes"), ICON_STRUCT,
				&(tv_iters.tag_class), _("Tables"), ICON_CLASS,
				&(tv_iters.tag_macro), _("Triggers"), ICON_MACRO,
				&(tv_iters.tag_member), _("Views"), ICON_VAR,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				NULL);
			break;
		}
		case GEANY_FILETYPES_D:
		default:
		{
			if (ft_id == GEANY_FILETYPES_D)
				tag_list_add_groups(tag_store,
					&(tv_iters.tag_namespace), _("Module"), ICON_NONE, NULL);
			else
				tag_list_add_groups(tag_store,
					&(tv_iters.tag_namespace), _("Namespaces"), ICON_NAMESPACE, NULL);

			tag_list_add_groups(tag_store,
				&(tv_iters.tag_class), _("Classes"), ICON_CLASS,
				&(tv_iters.tag_interface), _("Interfaces"), ICON_STRUCT,
				&(tv_iters.tag_function), _("Functions"), ICON_METHOD,
				&(tv_iters.tag_member), _("Members"), ICON_MEMBER,
				&(tv_iters.tag_struct), _("Structs"), ICON_STRUCT,
				&(tv_iters.tag_type), _("Typedefs / Enums"), ICON_STRUCT,
				NULL);

			if (ft_id != GEANY_FILETYPES_D)
			{
				tag_list_add_groups(tag_store,
					&(tv_iters.tag_macro), _("Macros"), ICON_MACRO, NULL);
			}
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_variable), _("Variables"), ICON_VAR,
				&(tv_iters.tag_externvar), _("Extern Variables"), ICON_VAR,
				&(tv_iters.tag_other), _("Other"), ICON_OTHER, NULL);
		}
	}
}


/* removes toplevel items that have no children */
static void hide_empty_rows(GtkTreeStore *store)
{
	GtkTreeIter iter;
	gboolean cont = TRUE;

	if (! gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
		return; /* stop when first iter is invalid, i.e. no elements */

	while (cont)
	{
		if (! gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter))
			cont = gtk_tree_store_remove(store, &iter);
		else
			cont = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
	}
}


static const gchar *get_symbol_name(GeanyDocument *doc, const TMTag *tag, gboolean found_parent)
{
	gchar *utf8_name;
	const gchar *scope = tag->scope;
	static GString *buffer = NULL;	/* buffer will be small so we can keep it for reuse */
	gboolean doc_is_utf8 = FALSE;

	/* encodings_convert_to_utf8_from_charset() fails with charset "None", so skip conversion
	 * for None at this point completely */
	if (utils_str_equal(doc->encoding, "UTF-8") ||
		utils_str_equal(doc->encoding, "None"))
		doc_is_utf8 = TRUE;
	else /* normally the tags will always be in UTF-8 since we parse from our buffer, but a
		  * plugin might have called tm_source_file_update(), so check to be sure */
		doc_is_utf8 = g_utf8_validate(tag->name, -1, NULL);

	if (! doc_is_utf8)
		utf8_name = encodings_convert_to_utf8_from_charset(tag->name,
			-1, doc->encoding, TRUE);
	else
		utf8_name = tag->name;

	if (utf8_name == NULL)
		return NULL;

	if (! buffer)
		buffer = g_string_new(NULL);
	else
		g_string_truncate(buffer, 0);

	/* check first char of scope is a wordchar */
	if (!found_parent && scope &&
		strpbrk(scope, GEANY_WORDCHARS) == scope)
	{
		const gchar *sep = symbols_get_context_separator(doc->file_type->id);

		g_string_append(buffer, scope);
		g_string_append(buffer, sep);
	}
	g_string_append(buffer, utf8_name);

	if (! doc_is_utf8)
		g_free(utf8_name);

	g_string_append_printf(buffer, " [%lu]", tag->line);

	return buffer->str;
}


static gchar *get_symbol_tooltip(GeanyDocument *doc, const TMTag *tag)
{
	gchar *utf8_name = editor_get_calltip_text(doc->editor, tag);

	/* encodings_convert_to_utf8_from_charset() fails with charset "None", so skip conversion
	 * for None at this point completely */
	if (utf8_name != NULL &&
		! utils_str_equal(doc->encoding, "UTF-8") &&
		! utils_str_equal(doc->encoding, "None"))
	{
		SETPTR(utf8_name,
			encodings_convert_to_utf8_from_charset(utf8_name, -1, doc->encoding, TRUE));
	}

	return utf8_name;
}


static const gchar *get_parent_name(const TMTag *tag)
{
	return !EMPTY(tag->scope) ? tag->scope : NULL;
}


static GtkTreeIter *get_tag_type_iter(TMTagType tag_type)
{
	GtkTreeIter *iter = NULL;

	switch (tag_type)
	{
		case tm_tag_prototype_t:
		case tm_tag_method_t:
		case tm_tag_function_t:
		{
			iter = &tv_iters.tag_function;
			break;
		}
		case tm_tag_externvar_t:
		{
			iter = &tv_iters.tag_externvar;
			break;
		}
		case tm_tag_macro_t:
		case tm_tag_macro_with_arg_t:
		{
			iter = &tv_iters.tag_macro;
			break;
		}
		case tm_tag_class_t:
		{
			iter = &tv_iters.tag_class;
			break;
		}
		case tm_tag_member_t:
		case tm_tag_field_t:
		{
			iter = &tv_iters.tag_member;
			break;
		}
		case tm_tag_typedef_t:
		case tm_tag_enum_t:
		{
			iter = &tv_iters.tag_type;
			break;
		}
		case tm_tag_union_t:
		case tm_tag_struct_t:
		{
			iter = &tv_iters.tag_struct;
			break;
		}
		case tm_tag_interface_t:
			iter = &tv_iters.tag_interface;
			break;
		case tm_tag_variable_t:
		{
			iter = &tv_iters.tag_variable;
			break;
		}
		case tm_tag_namespace_t:
		case tm_tag_package_t:
		{
			iter = &tv_iters.tag_namespace;
			break;
		}
		default:
		{
			iter = &tv_iters.tag_other;
		}
	}
	if (G_LIKELY(iter->stamp != -1))
		return iter;
	else
		return NULL;
}


static GdkPixbuf *get_child_icon(GtkTreeStore *tree_store, GtkTreeIter *parent)
{
	GdkPixbuf *icon = NULL;

	if (parent == &tv_iters.tag_other)
	{
		return g_object_ref(symbols_icons[ICON_VAR].pixbuf);
	}
	/* copy parent icon */
	gtk_tree_model_get(GTK_TREE_MODEL(tree_store), parent,
		SYMBOLS_COLUMN_ICON, &icon, -1);
	return icon;
}


static gboolean tag_equal(gconstpointer v1, gconstpointer v2)
{
	const TMTag *t1 = v1;
	const TMTag *t2 = v2;

	return (t1->type == t2->type && strcmp(t1->name, t2->name) == 0 &&
			utils_str_equal(t1->scope, t2->scope) &&
			/* include arglist in match to support e.g. C++ overloading */
			utils_str_equal(t1->arglist, t2->arglist));
}


/* inspired from g_str_hash() */
static guint tag_hash(gconstpointer v)
{
	const TMTag *tag = v;
	const gchar *p;
	guint32 h = 5381;

	h = (h << 5) + h + tag->type;
	for (p = tag->name; *p != '\0'; p++)
		h = (h << 5) + h + *p;
	if (tag->scope)
	{
		for (p = tag->scope; *p != '\0'; p++)
			h = (h << 5) + h + *p;
	}
	/* for e.g. C++ overloading */
	if (tag->arglist)
	{
		for (p = tag->arglist; *p != '\0'; p++)
			h = (h << 5) + h + *p;
	}

	return h;
}


/* like gtk_tree_view_expand_to_path() but with an iter */
static void tree_view_expand_to_iter(GtkTreeView *view, GtkTreeIter *iter)
{
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	GtkTreePath *path = gtk_tree_model_get_path(model, iter);

	gtk_tree_view_expand_to_path(view, path);
	gtk_tree_path_free(path);
}


/* like gtk_tree_store_remove() but finds the next iter at any level */
static gboolean tree_store_remove_row(GtkTreeStore *store, GtkTreeIter *iter)
{
	GtkTreeIter parent;
	gboolean has_parent;
	gboolean cont;

	has_parent = gtk_tree_model_iter_parent(GTK_TREE_MODEL(store), &parent, iter);
	cont = gtk_tree_store_remove(store, iter);
	/* if there is no next at this level but there is a parent iter, continue from it */
	if (! cont && has_parent)
	{
		*iter = parent;
		cont = ui_tree_model_iter_any_next(GTK_TREE_MODEL(store), iter, FALSE);
	}

	return cont;
}


static gint tree_search_func(gconstpointer key, gpointer user_data)
{
	TreeSearchData *data = user_data;
	gint parent_line = GPOINTER_TO_INT(key);
	gboolean new_nearest;

	if (data->found_line == -1)
		data->found_line = parent_line; /* initial value */

	new_nearest = ABS(data->line - parent_line) < ABS(data->line - data->found_line);

	if (parent_line > data->line)
	{
		if (new_nearest && !data->lower)
			data->found_line = parent_line;
		return -1;
	}

	if (new_nearest)
		data->found_line = parent_line;

	if (parent_line < data->line)
		return 1;

	return 0;
}


static gint tree_cmp(gconstpointer a, gconstpointer b, gpointer user_data)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}


static void parents_table_tree_value_free(gpointer data)
{
	g_slice_free(GtkTreeIter, data);
}


/* adds a new element in the parent table if its key is known. */
static void update_parents_table(GHashTable *table, const TMTag *tag, const GtkTreeIter *iter)
{
	const gchar *name;
	gchar *name_free = NULL;
	GTree *tree;

	if (EMPTY(tag->scope))
	{
		/* simple case, just use the tag name */
		name = tag->name;
	}
	else if (! tm_parser_has_full_context(tag->lang))
	{
		/* if the parser doesn't use fully qualified scope, use the name alone but
		 * prevent Foo::Foo from making parent = child */
		if (utils_str_equal(tag->scope, tag->name))
			name = NULL;
		else
			name = tag->name;
	}
	else
	{
		/* build the fully qualified scope as get_parent_name() would return it for a child tag */
		name_free = g_strconcat(tag->scope, tm_parser_context_separator(tag->lang), tag->name, NULL);
		name = name_free;
	}

	if (name && g_hash_table_lookup_extended(table, name, NULL, (gpointer *) &tree))
	{
		if (!tree)
		{
			tree = g_tree_new_full(tree_cmp, NULL, NULL, parents_table_tree_value_free);
			g_hash_table_insert(table, name_free ? name_free : g_strdup(name), tree);
			name_free = NULL;
		}

		g_tree_insert(tree, GINT_TO_POINTER(tag->line), g_slice_dup(GtkTreeIter, iter));
	}

	g_free(name_free);
}


static GtkTreeIter *parents_table_lookup(GHashTable *table, const gchar *name, guint line)
{
	GtkTreeIter *parent_search = NULL;
	GTree *tree;

	tree = g_hash_table_lookup(table, name);
	if (tree)
	{
		TreeSearchData user_data = {-1, line, TRUE};

		/* search parent candidates for the one with the nearest
		 * line number which is lower than the tag's line number */
		g_tree_search(tree, (GCompareFunc)tree_search_func, &user_data);
		parent_search = g_tree_lookup(tree, GINT_TO_POINTER(user_data.found_line));
	}

	return parent_search;
}


static void parents_table_value_free(gpointer data)
{
	GTree *tree = data;
	if (tree)
		g_tree_destroy(tree);
}


/* inserts a @data in @table on key @tag.
 * previous data is not overwritten if the key is duplicated, but rather the
 * two values are kept in a list
 *
 * table is: GHashTable<TMTag, GTree<line_num, GList<GList<TMTag>>>> */
static void tags_table_insert(GHashTable *table, TMTag *tag, GList *data)
{
	GTree *tree = g_hash_table_lookup(table, tag);
	if (!tree)
	{
		tree = g_tree_new_full(tree_cmp, NULL, NULL, NULL);
		g_hash_table_insert(table, tag, tree);
	}
	GList *list = g_tree_lookup(tree, GINT_TO_POINTER(tag->line));
	list = g_list_prepend(list, data);
	g_tree_insert(tree, GINT_TO_POINTER(tag->line), list);
}


/* looks up the entry in @table that best matches @tag.
 * if there is more than one candidate, the one that has closest line position to @tag is chosen */
static GList *tags_table_lookup(GHashTable *table, TMTag *tag)
{
	TreeSearchData user_data = {-1, tag->line, FALSE};
	GTree *tree = g_hash_table_lookup(table, tag);

	if (tree)
	{
		GList *list;

		g_tree_search(tree, (GCompareFunc)tree_search_func, &user_data);
		list = g_tree_lookup(tree, GINT_TO_POINTER(user_data.found_line));
		/* return the first value in the list - we don't care which of the
		 * tags with identical names defined on the same line we get */
		if (list)
			return list->data;
	}
	return NULL;
}


/* removes the element at @tag from @table.
 * @tag must be the exact pointer used at insertion time */
static void tags_table_remove(GHashTable *table, TMTag *tag)
{
	GTree *tree = g_hash_table_lookup(table, tag);
	if (tree)
	{
		GList *list = g_tree_lookup(tree, GINT_TO_POINTER(tag->line));
		if (list)
		{
			GList *node;
			/* should always be the first element as we returned the first one in
			 * tags_table_lookup() */
			foreach_list(node, list)
			{
				if (((GList *) node->data)->data == tag)
					break;
			}
			list = g_list_delete_link(list, node);
			if (!list)
				g_tree_remove(tree, GINT_TO_POINTER(tag->line));
			else
				g_tree_insert(tree, GINT_TO_POINTER(tag->line), list);
		}
	}
}


static gboolean tags_table_tree_value_free(gpointer key, gpointer value, gpointer data)
{
	GList *list = value;
	g_list_free(list);
	return FALSE;
}


static void tags_table_value_free(gpointer data)
{
	GTree *tree = data;
	if (tree)
	{
		/* free any leftover elements.  note that we can't register a value_free_func when
		 * creating the tree because we only want to free it when destroying the tree,
		 * not when inserting a duplicate (we handle this manually) */
		g_tree_foreach(tree, tags_table_tree_value_free, NULL);
		g_tree_destroy(tree);
	}
}


/*
 * Updates the tag tree for a document with the tags in *list.
 * @param doc a document
 * @param tags a pointer to a GList* holding the tags to add/update.  This
 *             list may be updated, removing updated elements.
 *
 * The update is done in two passes:
 * 1) walking the current tree, update tags that still exist and remove the
 *    obsolescent ones;
 * 2) walking the remaining (non updated) tags, adds them in the list.
 *
 * For better performances, we use 2 hash tables:
 * - one containing all the tags for lookup in the first pass (actually stores a
 *   reference in the tags list for removing it efficiently), avoiding list search
 *   on each tag;
 * - the other holding "tag-name":row references for tags having children, used to
 *   lookup for a parent in both passes, avoiding tree traversal.
 */
static void update_tree_tags(GeanyDocument *doc, GList **tags)
{
	GtkTreeStore *store = doc->priv->tag_store;
	GtkTreeModel *model = GTK_TREE_MODEL(store);
	GHashTable *parents_table;
	GHashTable *tags_table;
	GtkTreeIter iter;
	gboolean cont;
	GList *item;

	/* Build hash tables holding tags and parents */
	/* parent table is GHashTable<tag_name, GTree<line_num, GtkTreeIter>>
	 * where tag_name might be a fully qualified name (with scope) if the language
	 * parser reports scope properly (see tm_parser_has_full_context()). */
	parents_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, parents_table_value_free);
	/* tags table is another representation of the @tags list,
	 * GHashTable<TMTag, GTree<line_num, GList<GList<TMTag>>>> */
	tags_table = g_hash_table_new_full(tag_hash, tag_equal, NULL, tags_table_value_free);
	foreach_list(item, *tags)
	{
		TMTag *tag = item->data;
		const gchar *parent_name;

		tags_table_insert(tags_table, tag, item);

		parent_name = get_parent_name(tag);
		if (parent_name)
			g_hash_table_insert(parents_table, g_strdup(parent_name), NULL);
	}

	/* First pass, update existing rows or delete them.
	 * It is OK to delete them since we walk top down so we would remove
	 * parents before checking for their children, thus never implicitly
	 * deleting an updated child */
	cont = gtk_tree_model_get_iter_first(model, &iter);
	while (cont)
	{
		TMTag *tag;

		gtk_tree_model_get(model, &iter, SYMBOLS_COLUMN_TAG, &tag, -1);
		if (! tag) /* most probably a toplevel, skip it */
			cont = ui_tree_model_iter_any_next(model, &iter, TRUE);
		else
		{
			GList *found_item;

			found_item = tags_table_lookup(tags_table, tag);
			if (! found_item) /* tag doesn't exist, remove it */
				cont = tree_store_remove_row(store, &iter);
			else /* tag still exist, update it */
			{
				const gchar *parent_name;
				TMTag *found = found_item->data;

				parent_name = get_parent_name(found);
				/* if parent is unknown, ignore it */
				if (parent_name && ! g_hash_table_lookup(parents_table, parent_name))
					parent_name = NULL;

				if (!tm_tags_equal(tag, found))
				{
					const gchar *name;
					gchar *tooltip;

					/* only update fields that (can) have changed (name that holds line
					 * number, tooltip, and the tag itself) */
					name = get_symbol_name(doc, found, parent_name != NULL);
					tooltip = get_symbol_tooltip(doc, found);
					gtk_tree_store_set(store, &iter,
							SYMBOLS_COLUMN_NAME, name,
							SYMBOLS_COLUMN_TOOLTIP, tooltip,
							SYMBOLS_COLUMN_TAG, found,
							-1);
					g_free(tooltip);
				}

				update_parents_table(parents_table, found, &iter);

				/* remove the updated tag from the table and list */
				tags_table_remove(tags_table, found);
				*tags = g_list_delete_link(*tags, found_item);

				cont = ui_tree_model_iter_any_next(model, &iter, TRUE);
			}

			tm_tag_unref(tag);
		}
	}

	/* Second pass, now we have a tree cleaned up from invalid rows,
	 * we simply add new ones */
	foreach_list (item, *tags)
	{
		TMTag *tag = item->data;
		GtkTreeIter *parent;

		parent = get_tag_type_iter(tag->type);
		if (G_UNLIKELY(! parent))
			geany_debug("Missing symbol-tree parent iter for type %d!", tag->type);
		else
		{
			gboolean expand;
			const gchar *name;
			const gchar *parent_name;
			gchar *tooltip;
			GdkPixbuf *icon = get_child_icon(store, parent);

			parent_name = get_parent_name(tag);
			if (parent_name)
			{
				GtkTreeIter *parent_search = parents_table_lookup(parents_table, parent_name, tag->line);

				if (parent_search)
					parent = parent_search;
				else
					parent_name = NULL;
			}

			/* only expand to the iter if the parent was empty, otherwise we let the
			 * folding as it was before (already expanded, or closed by the user) */
			expand = ! gtk_tree_model_iter_has_child(model, parent);

			/* insert the new element */
			name = get_symbol_name(doc, tag, parent_name != NULL);
			tooltip = get_symbol_tooltip(doc, tag);
			gtk_tree_store_insert_with_values(store, &iter, parent, 0,
					SYMBOLS_COLUMN_NAME, name,
					SYMBOLS_COLUMN_TOOLTIP, tooltip,
					SYMBOLS_COLUMN_ICON, icon,
					SYMBOLS_COLUMN_TAG, tag,
					-1);
			g_free(tooltip);
			if (G_LIKELY(icon))
				g_object_unref(icon);

			update_parents_table(parents_table, tag, &iter);

			if (expand)
				tree_view_expand_to_iter(GTK_TREE_VIEW(doc->priv->tag_tree), &iter);
		}
	}

	g_hash_table_destroy(parents_table);
	g_hash_table_destroy(tags_table);
}


/* we don't want to sort 1st-level nodes, but we can't return 0 because the tree sort
 * is not stable, so the order is already lost. */
static gint compare_top_level_names(const gchar *a, const gchar *b)
{
	guint i;
	const gchar *name;

	/* This should never happen as it would mean that two or more top
	 * level items have the same name but it can happen by typos in the translations. */
	if (utils_str_equal(a, b))
		return 1;

	foreach_ptr_array(name, i, top_level_iter_names)
	{
		if (utils_str_equal(name, a))
			return -1;
		if (utils_str_equal(name, b))
			return 1;
	}
	g_warning("Couldn't find top level node '%s' or '%s'!", a, b);
	return 0;
}


static gboolean tag_has_missing_parent(const TMTag *tag, GtkTreeStore *store,
		GtkTreeIter *iter)
{
	/* if the tag has a parent tag, it should be at depth >= 2 */
	return !EMPTY(tag->scope) &&
		gtk_tree_store_iter_depth(store, iter) == 1;
}


static gint tree_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
		gpointer user_data)
{
	gboolean sort_by_name = GPOINTER_TO_INT(user_data);
	TMTag *tag_a, *tag_b;
	gint cmp;

	gtk_tree_model_get(model, a, SYMBOLS_COLUMN_TAG, &tag_a, -1);
	gtk_tree_model_get(model, b, SYMBOLS_COLUMN_TAG, &tag_b, -1);

	/* Check if the iters can be sorted based on tag name and line, not tree item name.
	 * Sort by tree name if the scope was prepended, e.g. 'ScopeNameWithNoTag::TagName'. */
	if (tag_a && !tag_has_missing_parent(tag_a, GTK_TREE_STORE(model), a) &&
		tag_b && !tag_has_missing_parent(tag_b, GTK_TREE_STORE(model), b))
	{
		cmp = sort_by_name ? compare_symbol(tag_a, tag_b) :
			compare_symbol_lines(tag_a, tag_b);
	}
	else
	{
		gchar *astr, *bstr;

		gtk_tree_model_get(model, a, SYMBOLS_COLUMN_NAME, &astr, -1);
		gtk_tree_model_get(model, b, SYMBOLS_COLUMN_NAME, &bstr, -1);

		/* if a is toplevel, b must be also */
		if (gtk_tree_store_iter_depth(GTK_TREE_STORE(model), a) == 0)
		{
			cmp = compare_top_level_names(astr, bstr);
		}
		else
		{
			/* this is what g_strcmp0() does */
			if (! astr)
				cmp = -(astr != bstr);
			else if (! bstr)
				cmp = astr != bstr;
			else
			{
				cmp = strcmp(astr, bstr);

				/* sort duplicate 'ScopeName::OverloadedTagName' items by line as well */
				if (tag_a && tag_b)
					if (!sort_by_name ||
						(utils_str_equal(tag_a->name, tag_b->name) &&
							utils_str_equal(tag_a->scope, tag_b->scope)))
						cmp = compare_symbol_lines(tag_a, tag_b);
			}
		}
		g_free(astr);
		g_free(bstr);
	}
	tm_tag_unref(tag_a);
	tm_tag_unref(tag_b);

	return cmp;
}


static void sort_tree(GtkTreeStore *store, gboolean sort_by_name)
{
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), SYMBOLS_COLUMN_NAME, tree_sort_func,
		GINT_TO_POINTER(sort_by_name), NULL);

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), SYMBOLS_COLUMN_NAME, GTK_SORT_ASCENDING);
}


gboolean symbols_recreate_tag_list(GeanyDocument *doc, gint sort_mode)
{
	GList *tags;

	g_return_val_if_fail(DOC_VALID(doc), FALSE);

	tags = get_tag_list(doc, tm_tag_max_t);
	if (tags == NULL)
		return FALSE;

	/* FIXME: Not sure why we detached the model here? */

	/* disable sorting during update because the code doesn't support correctly
	 * models that are currently being built */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(doc->priv->tag_store), GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, 0);

	/* add grandparent type iters */
	add_top_level_items(doc);

	update_tree_tags(doc, &tags);
	g_list_free(tags);

	hide_empty_rows(doc->priv->tag_store);

	if (sort_mode == SYMBOLS_SORT_USE_PREVIOUS)
		sort_mode = doc->priv->symbol_list_sort_mode;

	sort_tree(doc->priv->tag_store, sort_mode == SYMBOLS_SORT_BY_NAME);
	doc->priv->symbol_list_sort_mode = sort_mode;

	return TRUE;
}


/* Detects a global tags filetype from the *.lang.* language extension.
 * Returns NULL if there was no matching TM language. */
static GeanyFiletype *detect_global_tags_filetype(const gchar *utf8_filename)
{
	gchar *tags_ext;
	gchar *shortname = utils_strdupa(utf8_filename);
	GeanyFiletype *ft = NULL;

	tags_ext = g_strrstr(shortname, ".tags");
	if (tags_ext)
	{
		*tags_ext = '\0';	/* remove .tags extension */
		ft = filetypes_detect_from_extension(shortname);
		if (ft->id != GEANY_FILETYPES_NONE)
			return ft;
	}
	return NULL;
}


/* Adapted from anjuta-2.0.2/global-tags/tm_global_tags.c, thanks.
 * Needs full paths for filenames, except for C/C++ tag files, when CFLAGS includes
 * the relevant path.
 * Example:
 * CFLAGS=-I/home/user/libname-1.x geany -g libname.d.tags libname.h */
int symbols_generate_global_tags(int argc, char **argv, gboolean want_preprocess)
{
	/* -E pre-process, -dD output user macros, -p prof info (?) */
	const char pre_process[] = "gcc -E -dD -p -I.";

	if (argc > 2)
	{
		/* Create global taglist */
		int status;
		char *command;
		const char *tags_file = argv[1];
		char *utf8_fname;
		GeanyFiletype *ft;

		utf8_fname = utils_get_utf8_from_locale(tags_file);
		ft = detect_global_tags_filetype(utf8_fname);
		g_free(utf8_fname);

		if (ft == NULL)
		{
			g_printerr(_("Unknown filetype extension for \"%s\".\n"), tags_file);
			return 1;
		}
		/* load config in case of custom filetypes */
		filetypes_load_config(ft->id, FALSE);

		/* load ignore list for C/C++ parser */
		if (ft->id == GEANY_FILETYPES_C || ft->id == GEANY_FILETYPES_CPP)
			load_c_ignore_tags();

		if (want_preprocess && (ft->id == GEANY_FILETYPES_C || ft->id == GEANY_FILETYPES_CPP))
		{
			const gchar *cflags = getenv("CFLAGS");
			command = g_strdup_printf("%s %s", pre_process, FALLBACK(cflags, ""));
		}
		else
			command = NULL;	/* don't preprocess */

		geany_debug("Generating %s tags file.", ft->name);
		tm_get_workspace();
		status = tm_workspace_create_global_tags(command, (const char **) (argv + 2),
												 argc - 2, tags_file, ft->lang);
		g_free(command);
		symbols_finalize(); /* free c_tags_ignore data */
		if (! status)
		{
			g_printerr(_("Failed to create tags file, perhaps because no symbols "
				"were found.\n"));
			return 1;
		}
	}
	else
	{
		g_printerr(_("Usage: %s -g <Tags File> <File list>\n\n"), argv[0]);
		g_printerr(_("Example:\n"
			"CFLAGS=`pkg-config gtk+-2.0 --cflags` %s -g gtk2.c.tags"
			" /usr/include/gtk-2.0/gtk/gtk.h\n"), argv[0]);
		return 1;
	}
	return 0;
}


void symbols_show_load_tags_dialog(void)
{
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new(_("Load Tags File"), GTK_WINDOW(main_widgets.window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_OK,
		NULL);
	gtk_widget_set_name(dialog, "GeanyDialog");
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Geany tags file (*.*.tags)"));
	gtk_file_filter_add_pattern(filter, "*.*.tags");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		GSList *flist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
		GSList *item;

		for (item = flist; item != NULL; item = g_slist_next(item))
		{
			gchar *fname = item->data;
			gchar *utf8_fname;
			GeanyFiletype *ft;

			utf8_fname = utils_get_utf8_from_locale(fname);
			ft = detect_global_tags_filetype(utf8_fname);

			if (ft != NULL && symbols_load_global_tags(fname, ft))
				/* For translators: the first wildcard is the filetype, the second the filename */
				ui_set_statusbar(TRUE, _("Loaded %s tags file '%s'."),
					filetypes_get_display_name(ft), utf8_fname);
			else
				ui_set_statusbar(TRUE, _("Could not load tags file '%s'."), utf8_fname);

			g_free(utf8_fname);
			g_free(fname);
		}
		g_slist_free(flist);
	}
	gtk_widget_destroy(dialog);
}


static void init_user_tags(void)
{
	GSList *file_list = NULL, *list = NULL;
	const GSList *node;
	gchar *dir;

	dir = g_build_filename(app->configdir, GEANY_TAGS_SUBDIR, NULL);
	/* create the user tags dir for next time if it doesn't exist */
	if (! g_file_test(dir, G_FILE_TEST_IS_DIR))
		utils_mkdir(dir, FALSE);
	file_list = utils_get_file_list_full(dir, TRUE, FALSE, NULL);

	SETPTR(dir, g_build_filename(app->datadir, GEANY_TAGS_SUBDIR, NULL));
	list = utils_get_file_list_full(dir, TRUE, FALSE, NULL);
	g_free(dir);

	file_list = g_slist_concat(file_list, list);

	/* populate the filetype-specific tag files lists */
	for (node = file_list; node != NULL; node = node->next)
	{
		gchar *fname = node->data;
		gchar *utf8_fname = utils_get_utf8_from_locale(fname);
		GeanyFiletype *ft = detect_global_tags_filetype(utf8_fname);

		g_free(utf8_fname);

		if (FILETYPE_ID(ft) != GEANY_FILETYPES_NONE)
			ft->priv->tag_files = g_slist_prepend(ft->priv->tag_files, fname);
		else
		{
			geany_debug("Unknown filetype for file '%s'.", fname);
			g_free(fname);
		}
	}

	/* don't need to delete list contents because they are now stored in
	 * ft->priv->tag_files */
	g_slist_free(file_list);
}


static void load_user_tags(GeanyFiletypeID ft_id)
{
	static guchar *tags_loaded = NULL;
	static gboolean init_tags = FALSE;
	const GSList *node;
	GeanyFiletype *ft = filetypes[ft_id];

	g_return_if_fail(ft_id > 0);

	if (!tags_loaded)
		tags_loaded = g_new0(guchar, filetypes_array->len);
	if (tags_loaded[ft_id])
		return;
	tags_loaded[ft_id] = TRUE;	/* prevent reloading */

	if (!init_tags)
	{
		init_user_tags();
		init_tags = TRUE;
	}

	for (node = ft->priv->tag_files; node != NULL; node = g_slist_next(node))
	{
		const gchar *fname = node->data;

		symbols_load_global_tags(fname, ft);
	}
}


static void on_goto_popup_item_activate(GtkMenuItem *item, TMTag *tag)
{
	GeanyDocument *new_doc, *old_doc;

	g_return_if_fail(tag);

	old_doc = document_get_current();
	new_doc = document_open_file(tag->file->file_name, FALSE, NULL, NULL);

	if (new_doc)
		navqueue_goto_line(old_doc, new_doc, tag->line);
}


/* FIXME: use the same icons as in the symbols tree defined in add_top_level_items() */
static guint get_tag_class(const TMTag *tag)
{
	switch (tag->type)
	{
		case tm_tag_prototype_t:
		case tm_tag_method_t:
		case tm_tag_function_t:
			return ICON_METHOD;
		case tm_tag_variable_t:
		case tm_tag_externvar_t:
			return ICON_VAR;
		case tm_tag_macro_t:
		case tm_tag_macro_with_arg_t:
			return ICON_MACRO;
		case tm_tag_class_t:
			return ICON_CLASS;
		case tm_tag_member_t:
		case tm_tag_field_t:
			return ICON_MEMBER;
		case tm_tag_typedef_t:
		case tm_tag_enum_t:
		case tm_tag_union_t:
		case tm_tag_struct_t:
			return ICON_STRUCT;
		case tm_tag_namespace_t:
		case tm_tag_package_t:
			return ICON_NAMESPACE;
		default:
			break;
	}
	return ICON_STRUCT;
}


/* positions a popup at the caret from the ScintillaObject in @p data */
static void goto_popup_position_func(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	gint line_height;
	GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(menu));
	gint monitor_num;
	GdkRectangle monitor;
	GtkRequisition req;
	GdkEventButton *event_button = g_object_get_data(G_OBJECT(menu), "geany-button-event");

	if (event_button)
	{
		/* if we got a mouse click, popup at that position */
		*x = (gint) event_button->x_root;
		*y = (gint) event_button->y_root;
		line_height = 0; /* we don't want to offset below the line or anything */
	}
	else /* keyboard positioning */
	{
		ScintillaObject *sci = data;
		GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(sci));
		gint pos = sci_get_current_position(sci);
		gint line = sci_get_line_from_position(sci, pos);
		gint pos_x = SSM(sci, SCI_POINTXFROMPOSITION, 0, pos);
		gint pos_y = SSM(sci, SCI_POINTYFROMPOSITION, 0, pos);

		line_height = SSM(sci, SCI_TEXTHEIGHT, line, 0);

		gdk_window_get_origin(window, x, y);
		*x += pos_x;
		*y += pos_y;
	}

	monitor_num = gdk_screen_get_monitor_at_point(screen, *x, *y);

#if GTK_CHECK_VERSION(3, 0, 0)
	gtk_widget_get_preferred_size(GTK_WIDGET(menu), NULL, &req);
#else
	gtk_widget_size_request(GTK_WIDGET(menu), &req);
#endif

#if GTK_CHECK_VERSION(3, 4, 0)
	gdk_screen_get_monitor_workarea(screen, monitor_num, &monitor);
#else
	gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor);
#endif

	/* put on one size of the X position, but within the monitor */
	if (gtk_widget_get_direction(GTK_WIDGET(menu)) == GTK_TEXT_DIR_RTL)
	{
		if (*x - req.width - 1 >= monitor.x)
			*x -= req.width + 1;
		else if (*x + req.width > monitor.x + monitor.width)
			*x = monitor.x;
		else
			*x += 1;
	}
	else
	{
		if (*x + req.width + 1 <= monitor.x + monitor.width)
			*x = MAX(monitor.x, *x + 1);
		else if (*x - req.width - 1 >= monitor.x)
			*x -= req.width + 1;
		else
			*x = monitor.x + MAX(0, monitor.width - req.width);
	}

	/* try to put, in order:
	 * 1. below the Y position, under the line
	 * 2. above the Y position
	 * 3. within the monitor */
	if (*y + line_height + req.height <= monitor.y + monitor.height)
		*y = MAX(monitor.y, *y + line_height);
	else if (*y - req.height >= monitor.y)
		*y = *y - req.height;
	else
		*y = monitor.y + MAX(0, monitor.height - req.height);

	*push_in = FALSE;
}


static void show_goto_popup(GeanyDocument *doc, GPtrArray *tags, gboolean have_best)
{
	GtkWidget *first = NULL;
	GtkWidget *menu;
	GdkEvent *event;
	GdkEventButton *button_event = NULL;
	TMTag *tmtag;
	guint i;

	menu = gtk_menu_new();

	foreach_ptr_array(tmtag, i, tags)
	{
		GtkWidget *item;
		GtkWidget *label;
		GtkWidget *image;
		gchar *fname = g_path_get_basename(tmtag->file->file_name);
		gchar *text;

		if (! first && have_best)
			/* For translators: it's the filename and line number of a symbol in the goto-symbol popup menu */
			text = g_markup_printf_escaped(_("<b>%s: %lu</b>"), fname, tmtag->line);
		else
			/* For translators: it's the filename and line number of a symbol in the goto-symbol popup menu */
			text = g_markup_printf_escaped(_("%s: %lu"), fname, tmtag->line);

		image = gtk_image_new_from_pixbuf(symbols_icons[get_tag_class(tmtag)].pixbuf);
		label = g_object_new(GTK_TYPE_LABEL, "label", text, "use-markup", TRUE, "xalign", 0.0, NULL);
		item = g_object_new(GTK_TYPE_IMAGE_MENU_ITEM, "image", image, "child", label, NULL);
		g_signal_connect_data(item, "activate", G_CALLBACK(on_goto_popup_item_activate),
		                      tm_tag_ref(tmtag), (GClosureNotify) tm_tag_unref, 0);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		if (! first)
			first = item;

		g_free(text);
		g_free(fname);
	}

	gtk_widget_show_all(menu);

	if (first) /* always select the first item for better keyboard navigation */
		g_signal_connect(menu, "realize", G_CALLBACK(gtk_menu_shell_select_item), first);

	event = gtk_get_current_event();
	if (event && event->type == GDK_BUTTON_PRESS)
		button_event = (GdkEventButton *) event;
	else
		gdk_event_free(event);

	g_object_set_data_full(G_OBJECT(menu), "geany-button-event", button_event,
	                       button_event ? (GDestroyNotify) gdk_event_free : NULL);
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, goto_popup_position_func, doc->editor->sci,
				   button_event ? button_event->button : 0, gtk_get_current_event_time ());
}


static gint compare_tags_by_name_line(gconstpointer ptr1, gconstpointer ptr2)
{
	gint res;
	TMTag *t1 = *((TMTag **) ptr1);
	TMTag *t2 = *((TMTag **) ptr2);

	res = g_strcmp0(t1->file->short_name, t2->file->short_name);
	if (res != 0)
		return res;
	return t1->line - t2->line;
}


static TMTag *find_best_goto_tag(GeanyDocument *doc, GPtrArray *tags)
{
	TMTag *tag;
	guint i;

	/* first check if we have a tag in the current file */
	foreach_ptr_array(tag, i, tags)
	{
		if (g_strcmp0(doc->real_path, tag->file->file_name) == 0)
			return tag;
	}

	/* next check if we have a tag for some of the open documents */
	foreach_ptr_array(tag, i, tags)
	{
		guint j;

		foreach_document(j)
		{
			if (g_strcmp0(documents[j]->real_path, tag->file->file_name) == 0)
				return tag;
		}
	}

	/* next check if we have a tag for a file inside the current document's directory */
	foreach_ptr_array(tag, i, tags)
	{
		gchar *dir = g_path_get_dirname(doc->real_path);

		if (g_str_has_prefix(tag->file->file_name, dir))
		{
			g_free(dir);
			return tag;
		}
		g_free(dir);
	}

	return NULL;
}


static GPtrArray *filter_tags(GPtrArray *tags, TMTag *current_tag, gboolean definition)
{
	const TMTagType forward_types = tm_tag_prototype_t | tm_tag_externvar_t;
	TMTag *tmtag, *last_tag = NULL;
	GPtrArray *filtered_tags = g_ptr_array_new();
	guint i;

	foreach_ptr_array(tmtag, i, tags)
	{
		if ((definition && !(tmtag->type & forward_types)) ||
			(!definition && (tmtag->type & forward_types)))
		{
			/* If there are typedefs of e.g. a struct such as
			 * "typedef struct Foo {} Foo;", filter out the typedef unless
			 * cursor is at the struct name. */
			if (last_tag != NULL && last_tag->file == tmtag->file &&
				last_tag->type != tm_tag_typedef_t && tmtag->type == tm_tag_typedef_t)
			{
				if (last_tag == current_tag)
					g_ptr_array_add(filtered_tags, tmtag);
			}
			else if (tmtag != current_tag)
				g_ptr_array_add(filtered_tags, tmtag);

			last_tag = tmtag;
		}
	}

	return filtered_tags;
}


static gboolean goto_tag(const gchar *name, gboolean definition)
{
	const TMTagType forward_types = tm_tag_prototype_t | tm_tag_externvar_t;
	TMTag *tmtag, *current_tag = NULL;
	GeanyDocument *old_doc = document_get_current();
	gboolean found = FALSE;
	const GPtrArray *all_tags;
	GPtrArray *tags, *filtered_tags;
	guint i;
	guint current_line = sci_get_current_line(old_doc->editor->sci) + 1;

	all_tags = tm_workspace_find(name, NULL, tm_tag_max_t, NULL, old_doc->file_type->lang);

	/* get rid of global tags and find tag at current line */
	tags = g_ptr_array_new();
	foreach_ptr_array(tmtag, i, all_tags)
	{
		if (tmtag->file)
		{
			g_ptr_array_add(tags, tmtag);
			if (tmtag->file == old_doc->tm_file && tmtag->line == current_line)
				current_tag = tmtag;
		}
	}

	if (current_tag)
		/* swap definition/declaration search */
		definition = current_tag->type & forward_types;

	filtered_tags = filter_tags(tags, current_tag, definition);
	if (filtered_tags->len == 0)
	{
		/* if we didn't find anything, try again with the opposite type */
		g_ptr_array_free(filtered_tags, TRUE);
		filtered_tags = filter_tags(tags, current_tag, !definition);
	}
	g_ptr_array_free(tags, TRUE);
	tags = filtered_tags;

	if (tags->len == 1)
	{
		GeanyDocument *new_doc;

		tmtag = tags->pdata[0];
		new_doc = document_find_by_real_path(tmtag->file->file_name);

		if (!new_doc)
			/* not found in opened document, should open */
			new_doc = document_open_file(tmtag->file->file_name, FALSE, NULL, NULL);

		navqueue_goto_line(old_doc, new_doc, tmtag->line);
	}
	else if (tags->len > 1)
	{
		GPtrArray *tag_list;
		TMTag *tag, *best_tag;

		g_ptr_array_sort(tags, compare_tags_by_name_line);
		best_tag = find_best_goto_tag(old_doc, tags);

		tag_list = g_ptr_array_new();
		if (best_tag)
			g_ptr_array_add(tag_list, best_tag);
		foreach_ptr_array(tag, i, tags)
		{
			if (tag != best_tag)
				g_ptr_array_add(tag_list, tag);
		}
		show_goto_popup(old_doc, tag_list, best_tag != NULL);

		g_ptr_array_free(tag_list, TRUE);
	}

	found = tags->len > 0;
	g_ptr_array_free(tags, TRUE);

	return found;
}


gboolean symbols_goto_tag(const gchar *name, gboolean definition)
{
	if (goto_tag(name, definition))
		return TRUE;

	/* if we are here, there was no match and we are beeping ;-) */
	utils_beep();

	if (!definition)
		ui_set_statusbar(FALSE, _("Forward declaration \"%s\" not found."), name);
	else
		ui_set_statusbar(FALSE, _("Definition of \"%s\" not found."), name);
	return FALSE;
}


/* This could perhaps be improved to check for #if, class etc. */
static gint get_function_fold_number(GeanyDocument *doc)
{
	/* for Java the functions are always one fold level above the class scope */
	if (doc->file_type->id == GEANY_FILETYPES_JAVA)
		return SC_FOLDLEVELBASE + 1;
	else
		return SC_FOLDLEVELBASE;
}


/* Should be used only with get_current_tag_cached.
 * tag_types caching might trigger recomputation too often but this isn't used differently often
 * enough to be an issue for now */
static gboolean current_tag_changed(GeanyDocument *doc, gint cur_line, gint fold_level, guint tag_types)
{
	static gint old_line = -2;
	static GeanyDocument *old_doc = NULL;
	static gint old_fold_num = -1;
	static guint old_tag_types = 0;
	const gint fold_num = fold_level & SC_FOLDLEVELNUMBERMASK;
	gboolean ret;

	/* check if the cached line and file index have changed since last time: */
	if (doc == NULL || doc != old_doc || old_tag_types != tag_types)
		ret = TRUE;
	else if (cur_line == old_line)
		ret = FALSE;
	else
	{
		/* if the line has only changed by 1 */
		if (abs(cur_line - old_line) == 1)
		{
			/* It's the same function if the fold number hasn't changed */
			ret = (fold_num != old_fold_num);
		}
		else ret = TRUE;
	}

	/* record current line and file index for next time */
	old_line = cur_line;
	old_doc = doc;
	old_fold_num = fold_num;
	old_tag_types = tag_types;
	return ret;
}


/* Parse the function name up to 2 lines before tag_line.
 * C++ like syntax should be parsed by parse_cpp_function_at_line, otherwise the return
 * type or argument names can be confused with the function name. */
static gchar *parse_function_at_line(ScintillaObject *sci, gint tag_line)
{
	gint start, end, max_pos;
	gint fn_style;

	switch (sci_get_lexer(sci))
	{
		case SCLEX_RUBY:	fn_style = SCE_RB_DEFNAME; break;
		case SCLEX_PYTHON:	fn_style = SCE_P_DEFNAME; break;
		default: fn_style = SCE_C_IDENTIFIER;	/* several lexers use SCE_C_IDENTIFIER */
	}
	start = sci_get_position_from_line(sci, tag_line - 2);
	max_pos = sci_get_position_from_line(sci, tag_line + 1);
	while (start < max_pos && sci_get_style_at(sci, start) != fn_style)
		start++;

	end = start;
	while (end < max_pos && sci_get_style_at(sci, end) == fn_style)
		end++;

	if (start == end)
		return NULL;
	return sci_get_contents_range(sci, start, end);
}


/* Parse the function name */
static gchar *parse_cpp_function_at_line(ScintillaObject *sci, gint tag_line)
{
	gint start, end, first_pos, max_pos;
	gint tmp;
	gchar c;

	first_pos = end = sci_get_position_from_line(sci, tag_line);
	max_pos = sci_get_position_from_line(sci, tag_line + 1);
	tmp = 0;
	/* goto the begin of function body */
	while (end < max_pos &&
		(tmp = sci_get_char_at(sci, end)) != '{' &&
		tmp != 0) end++;
	if (tmp == 0) end --;

	/* go back to the end of function identifier */
	while (end > 0 && end > first_pos - 500 &&
		(tmp = sci_get_char_at(sci, end)) != '(' &&
		tmp != 0) end--;
	end--;
	if (end < 0) end = 0;

	/* skip whitespaces between identifier and ( */
	while (end > 0 && isspace(sci_get_char_at(sci, end))) end--;

	start = end;
	/* Use tmp to find SCE_C_IDENTIFIER or SCE_C_GLOBALCLASS chars */
	while (start >= 0 && ((tmp = sci_get_style_at(sci, start)) == SCE_C_IDENTIFIER
		 ||  tmp == SCE_C_GLOBALCLASS
		 || (c = sci_get_char_at(sci, start)) == '~'
		 ||  c == ':'))
		start--;
	if (start != 0 && start < end) start++;	/* correct for last non-matching char */

	if (start == end) return NULL;
	return sci_get_contents_range(sci, start, end + 1);
}


/* gets the fold header after or on @line, but skipping folds created because of parentheses */
static gint get_fold_header_after(ScintillaObject *sci, gint line)
{
	const gint line_count = sci_get_line_count(sci);

	for (; line < line_count; line++)
	{
		if (sci_get_fold_level(sci, line) & SC_FOLDLEVELHEADERFLAG)
		{
			const gint last_child = SSM(sci, SCI_GETLASTCHILD, line, -1);
			const gint line_end = sci_get_line_end_position(sci, line);
			const gint lexer = sci_get_lexer(sci);
			gint parenthesis_match_line = -1;

			/* now find any unbalanced open parenthesis on the line and see where the matching
			 * brace would be, mimicking what folding on () does */
			for (gint pos = sci_get_position_from_line(sci, line); pos < line_end; pos++)
			{
				if (highlighting_is_code_style(lexer, sci_get_style_at(sci, pos)) &&
				    sci_get_char_at(sci, pos) == '(')
				{
					const gint matching = sci_find_matching_brace(sci, pos);

					if (matching >= 0)
					{
						parenthesis_match_line = sci_get_line_from_position(sci, matching);
						if (parenthesis_match_line != line)
							break;  /* match is on a different line, we found a possible fold */
						else
							pos = matching;  /* just skip the range and continue searching */
					}
				}
			}

			/* if the matching parenthesis matches the fold level, skip it and continue.
			 * it matches if it either spans the same lines, or spans one more but the next one is
			 * a fold header (in which case the last child of the fold is one less to let the
			 * header be at the parent level) */
			if ((parenthesis_match_line == last_child) ||
			    (parenthesis_match_line == last_child + 1 &&
			     sci_get_fold_level(sci, parenthesis_match_line) & SC_FOLDLEVELHEADERFLAG))
				line = last_child;
			else
				return line;
		}
	}

	return -1;
}


static gint get_current_tag_name(GeanyDocument *doc, gchar **tagname, TMTagType tag_types)
{
	gint line;
	gint parent;

	line = sci_get_current_line(doc->editor->sci);
	parent = sci_get_fold_parent(doc->editor->sci, line);
	/* if we're inside a fold level and we have up-to-date tags, get the function from TM */
	if (parent >= 0 && doc->tm_file != NULL && doc->tm_file->tags_array != NULL &&
		(! doc->changed || editor_prefs.autocompletion_update_freq > 0))
	{
		const TMTag *tag = tm_get_current_tag(doc->tm_file->tags_array, parent + 1, tag_types);

		if (tag)
		{
			gint tag_line = tag->line - 1;
			gint last_child = line + 1;

			/* if it may be a false positive because we're inside a fold level not inside anything
			 * we match, e.g. a #if in C or C++, we check we're inside the fold level that start
			 * right after the tag we got from TM.
			 * Additionally, we perform parentheses matching on the initial line not to get confused
			 * by folding on () in case the parameter list spans multiple lines */
			if (abs(tag_line - parent) > 1)
			{
				const gint tag_fold = get_fold_header_after(doc->editor->sci, tag_line);
				if (tag_fold >= 0)
					last_child = SSM(doc->editor->sci, SCI_GETLASTCHILD, tag_fold, -1);
			}

			if (line <= last_child)
			{
				if (tag->scope)
					*tagname = g_strconcat(tag->scope,
							symbols_get_context_separator(doc->file_type->id), tag->name, NULL);
				else
					*tagname = g_strdup(tag->name);

				return tag_line;
			}
		}
	}
	/* for the poor guy with a modified document and without real time tag parsing, we fallback
	 * to dirty and inaccurate hand-parsing */
	else if (parent >= 0 && doc->file_type != NULL && doc->file_type->id != GEANY_FILETYPES_NONE)
	{
		const gint fn_fold = get_function_fold_number(doc);
		gint tag_line = parent;
		gint fold_level = sci_get_fold_level(doc->editor->sci, tag_line);

		/* find the top level fold point */
		while (tag_line >= 0 && (fold_level & SC_FOLDLEVELNUMBERMASK) != fn_fold)
		{
			tag_line = sci_get_fold_parent(doc->editor->sci, tag_line);
			fold_level = sci_get_fold_level(doc->editor->sci, tag_line);
		}

		if (tag_line >= 0)
		{
			gchar *cur_tag;

			if (sci_get_lexer(doc->editor->sci) == SCLEX_CPP)
				cur_tag = parse_cpp_function_at_line(doc->editor->sci, tag_line);
			else
				cur_tag = parse_function_at_line(doc->editor->sci, tag_line);

			if (cur_tag != NULL)
			{
				*tagname = cur_tag;
				return tag_line;
			}
		}
	}

	*tagname = g_strdup(_("unknown"));
	return -1;
}


static gint get_current_tag_name_cached(GeanyDocument *doc, const gchar **tagname, TMTagType tag_types)
{
	static gint tag_line = -1;
	static gchar *cur_tag = NULL;

	g_return_val_if_fail(doc == NULL || doc->is_valid, -1);

	if (doc == NULL)	/* reset current function */
	{
		current_tag_changed(NULL, -1, -1, 0);
		g_free(cur_tag);
		cur_tag = g_strdup(_("unknown"));
		if (tagname != NULL)
			*tagname = cur_tag;
		tag_line = -1;
	}
	else
	{
		gint line = sci_get_current_line(doc->editor->sci);
		gint fold_level = sci_get_fold_level(doc->editor->sci, line);

		if (current_tag_changed(doc, line, fold_level, tag_types))
		{
			g_free(cur_tag);
			tag_line = get_current_tag_name(doc, &cur_tag, tag_types);
		}
		*tagname = cur_tag;
	}

	return tag_line;
}


/* Sets *tagname to point at the current function or tag name.
 * If doc is NULL, reset the cached current tag data to ensure it will be reparsed on the next
 * call to this function.
 * Returns: line number of the current tag, or -1 if unknown. */
gint symbols_get_current_function(GeanyDocument *doc, const gchar **tagname)
{
	return get_current_tag_name_cached(doc, tagname, tm_tag_function_t | tm_tag_method_t);
}


/* same as symbols_get_current_function() but finds class, namespaces and more */
gint symbols_get_current_scope(GeanyDocument *doc, const gchar **tagname)
{
	TMTagType tag_types = (tm_tag_function_t | tm_tag_method_t | tm_tag_class_t |
			tm_tag_struct_t | tm_tag_enum_t | tm_tag_union_t);

	/* Python parser reports imports as namespaces which confuses the scope detection */
	if (doc && doc->file_type->lang != filetypes[GEANY_FILETYPES_PYTHON]->lang)
		tag_types |= tm_tag_namespace_t;

	return get_current_tag_name_cached(doc, tagname, tag_types);
}


static void on_symbol_tree_sort_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
	gint sort_mode = GPOINTER_TO_INT(user_data);
	GeanyDocument *doc = document_get_current();

	if (ignore_callback)
		return;

	if (doc != NULL)
		doc->has_tags = symbols_recreate_tag_list(doc, sort_mode);
}


static void on_symbol_tree_menu_show(GtkWidget *widget,
		gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gboolean enable;

	enable = doc && doc->has_tags;
	gtk_widget_set_sensitive(symbol_menu.sort_by_name, enable);
	gtk_widget_set_sensitive(symbol_menu.sort_by_appearance, enable);
	gtk_widget_set_sensitive(symbol_menu.expand_all, enable);
	gtk_widget_set_sensitive(symbol_menu.collapse_all, enable);
	gtk_widget_set_sensitive(symbol_menu.find_usage, enable);
	gtk_widget_set_sensitive(symbol_menu.find_doc_usage, enable);

	if (! doc)
		return;

	ignore_callback = TRUE;

	if (doc->priv->symbol_list_sort_mode == SYMBOLS_SORT_BY_NAME)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(symbol_menu.sort_by_name), TRUE);
	else
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(symbol_menu.sort_by_appearance), TRUE);

	ignore_callback = FALSE;
}


static void on_expand_collapse(GtkWidget *widget, gpointer user_data)
{
	gboolean expand = GPOINTER_TO_INT(user_data);
	GeanyDocument *doc = document_get_current();

	if (! doc)
		return;

	g_return_if_fail(doc->priv->tag_tree);

	if (expand)
		gtk_tree_view_expand_all(GTK_TREE_VIEW(doc->priv->tag_tree));
	else
		gtk_tree_view_collapse_all(GTK_TREE_VIEW(doc->priv->tag_tree));
}


static void on_find_usage(GtkWidget *widget, G_GNUC_UNUSED gpointer unused)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GeanyDocument *doc;
	TMTag *tag = NULL;

	doc = document_get_current();
	if (!doc)
		return;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(doc->priv->tag_tree));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get(model, &iter, SYMBOLS_COLUMN_TAG, &tag, -1);
	if (tag)
	{
		if (widget == symbol_menu.find_in_files)
			search_show_find_in_files_dialog_full(tag->name, NULL);
		else
			search_find_usage(tag->name, tag->name, GEANY_FIND_WHOLEWORD | GEANY_FIND_MATCHCASE,
				widget == symbol_menu.find_usage);

		tm_tag_unref(tag);
	}
}


static void create_taglist_popup_menu(void)
{
	GtkWidget *item, *menu;

	tv.popup_taglist = menu = gtk_menu_new();

	symbol_menu.expand_all = item = ui_image_menu_item_new(GTK_STOCK_ADD, _("_Expand All"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_expand_collapse), GINT_TO_POINTER(TRUE));

	symbol_menu.collapse_all = item = ui_image_menu_item_new(GTK_STOCK_REMOVE, _("_Collapse All"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_expand_collapse), GINT_TO_POINTER(FALSE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	symbol_menu.sort_by_name = item = gtk_radio_menu_item_new_with_mnemonic(NULL,
		_("Sort by _Name"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_symbol_tree_sort_clicked),
			GINT_TO_POINTER(SYMBOLS_SORT_BY_NAME));

	symbol_menu.sort_by_appearance = item = gtk_radio_menu_item_new_with_mnemonic_from_widget(
		GTK_RADIO_MENU_ITEM(item), _("Sort by _Appearance"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_symbol_tree_sort_clicked),
			GINT_TO_POINTER(SYMBOLS_SORT_BY_APPEARANCE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	symbol_menu.find_usage = item = ui_image_menu_item_new(GTK_STOCK_FIND, _("Find _Usage"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_find_usage), symbol_menu.find_usage);

	symbol_menu.find_doc_usage = item = ui_image_menu_item_new(GTK_STOCK_FIND, _("Find _Document Usage"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_find_usage), symbol_menu.find_doc_usage);

	symbol_menu.find_in_files = item = ui_image_menu_item_new(GTK_STOCK_FIND, _("Find in F_iles..."));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_find_usage), NULL);

	g_signal_connect(menu, "show", G_CALLBACK(on_symbol_tree_menu_show), NULL);

	sidebar_add_common_menu_items(GTK_MENU(menu));
}


static void on_document_save(G_GNUC_UNUSED GObject *object, GeanyDocument *doc)
{
	gchar *f;

	g_return_if_fail(!EMPTY(doc->real_path));

	f = g_build_filename(app->configdir, "ignore.tags", NULL);
	if (utils_str_equal(doc->real_path, f))
		load_c_ignore_tags();

	g_free(f);
}


void symbols_init(void)
{
	gchar *f;
	guint i;

	create_taglist_popup_menu();

	f = g_build_filename(app->configdir, "ignore.tags", NULL);
	ui_add_config_file_menu_item(f, NULL, NULL);
	g_free(f);

	g_signal_connect(geany_object, "document-save", G_CALLBACK(on_document_save), NULL);

	for (i = 0; i < G_N_ELEMENTS(symbols_icons); i++)
		symbols_icons[i].pixbuf = get_tag_icon(symbols_icons[i].icon_name);
}


void symbols_finalize(void)
{
	guint i;

	g_strfreev(c_tags_ignore);

	for (i = 0; i < G_N_ELEMENTS(symbols_icons); i++)
	{
		if (symbols_icons[i].pixbuf)
			g_object_unref(symbols_icons[i].pixbuf);
	}
}
