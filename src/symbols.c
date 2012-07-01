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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "SciLexer.h"
#include "geany.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "prefix.h"
#include "symbols.h"
#include "utils.h"
#include "filetypes.h"
#include "encodings.h"
#include "document.h"
#include "documentprivate.h"
#include "support.h"
#include "msgwindow.h"
#include "sidebar.h"
#include "main.h"
#include "navqueue.h"
#include "ui_utils.h"
#include "editor.h"
#include "sciwrappers.h"
#include "filetypesprivate.h"


const guint TM_GLOBAL_TYPE_MASK =
	tm_tag_class_t | tm_tag_enum_t | tm_tag_interface_t |
	tm_tag_struct_t | tm_tag_typedef_t | tm_tag_union_t | tm_tag_namespace_t;


static gchar **html_entities = NULL;

typedef struct
{
	gboolean	tags_loaded;
	const gchar	*tag_file;
} TagFileInfo;

/* Check before adding any more tags files, usually they should be downloaded separately. */
enum	/* Geany tag files */
{
	GTF_C,
	GTF_PASCAL,
	GTF_PHP,
	GTF_HTML_ENTITIES,
	GTF_LATEX,
	GTF_PYTHON,
	GTF_MAX
};

static TagFileInfo tag_file_info[GTF_MAX] =
{
	{FALSE, "c99.tags"},
	{FALSE, "pascal.tags"},
	{FALSE, "php.tags"},
	{FALSE, "html_entities.tags"},
	{FALSE, "latex.tags"},
	{FALSE, "python.tags"}
};

static GPtrArray *top_level_iter_names = NULL;

static struct
{
	GtkWidget *expand_all;
	GtkWidget *collapse_all;
	GtkWidget *sort_by_name;
	GtkWidget *sort_by_appearance;
}
symbol_menu = {NULL, NULL, NULL, NULL};


static void html_tags_loaded(void);
static void load_user_tags(filetype_id ft_id);

/* get the tags_ignore list, exported by tagmanager's options.c */
extern gchar **c_tags_ignore;

/* ignore certain tokens when parsing C-like syntax.
 * Also works for reloading. */
static void load_c_ignore_tags(void)
{
	gchar *path = g_strconcat(app->configdir, G_DIR_SEPARATOR_S "ignore.tags", NULL);
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
		geany_debug("Loaded %s (%s), %u tag(s).", tags_file, ft->name,
			(guint) (get_tag_count() - old_tag_count));
	}
	return result;
}


/* Ensure that the global tags file(s) for the file_type_idx filetype is loaded.
 * This provides autocompletion, calltips, etc. */
void symbols_global_tags_loaded(guint file_type_idx)
{
	TagFileInfo *tfi;
	gint tag_type;

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
		case GEANY_FILETYPES_PHP:
		case GEANY_FILETYPES_HTML:
			html_tags_loaded();
	}
	switch (file_type_idx)
	{
		case GEANY_FILETYPES_CPP:
			symbols_global_tags_loaded(GEANY_FILETYPES_C);	/* load C global tags */
			/* no C++ tagfile yet */
			return;
		case GEANY_FILETYPES_C:		tag_type = GTF_C; break;
		case GEANY_FILETYPES_PASCAL:tag_type = GTF_PASCAL; break;
		case GEANY_FILETYPES_PHP:	tag_type = GTF_PHP; break;
		case GEANY_FILETYPES_LATEX:	tag_type = GTF_LATEX; break;
		case GEANY_FILETYPES_PYTHON:tag_type = GTF_PYTHON; break;
		default:
			return;
	}
	tfi = &tag_file_info[tag_type];

	if (! tfi->tags_loaded)
	{
		gchar *fname = g_strconcat(app->datadir, G_DIR_SEPARATOR_S, tfi->tag_file, NULL);

		symbols_load_global_tags(fname, filetypes[file_type_idx]);
		tfi->tags_loaded = TRUE;
		g_free(fname);
	}
}


/* HTML tagfile is just a list of entities for autocompletion (e.g. '&amp;') */
static void html_tags_loaded(void)
{
	TagFileInfo *tfi;

	if (cl_options.ignore_global_tags)
		return;

	tfi = &tag_file_info[GTF_HTML_ENTITIES];
	if (! tfi->tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S, tfi->tag_file, NULL);

		html_entities = utils_read_file_in_array(file);
		tfi->tags_loaded = TRUE;
		g_free(file);
	}
}


GString *symbols_find_tags_as_string(GPtrArray *tags_array, guint tag_types, gint lang)
{
	guint j;
	TMTag *tag;
	GString *s = NULL;
	GPtrArray *typedefs;
	gint tag_lang;

	g_return_val_if_fail(tags_array != NULL, NULL);

	typedefs = tm_tags_extract(tags_array, tag_types);

	if ((typedefs) && (typedefs->len > 0))
	{
		s = g_string_sized_new(typedefs->len * 10);
		for (j = 0; j < typedefs->len; ++j)
		{
			tag = TM_TAG(typedefs->pdata[j]);
			/* tag->atts.file.lang contains (for some reason) the line of the tag if
			 * tag->atts.entry.file is not NULL */
			tag_lang = (tag->atts.entry.file) ? tag->atts.entry.file->lang : tag->atts.file.lang;

			/* the check for tag_lang == lang is necessary to avoid wrong type colouring of
			 * e.g. PHP classes in C++ files
			 * lang = -2 disables the check */
			if (tag->name && (tag_lang == lang || lang == -2))
			{
				if (j != 0)
					g_string_append_c(s, ' ');
				g_string_append(s, tag->name);
			}
		}
	}
	if (typedefs)
		g_ptr_array_free(typedefs, TRUE);
	return s;
}


/** Gets the context separator used by the tag manager for a particular file
 * type.
 * @param ft_id File type identifier.
 * @return The context separator string.
 *
 * @since 0.19
 */
const gchar *symbols_get_context_separator(gint ft_id)
{
	switch (ft_id)
	{
		case GEANY_FILETYPES_C:	/* for C++ .h headers or C structs */
		case GEANY_FILETYPES_CPP:
		case GEANY_FILETYPES_GLSL:	/* for structs */
		/*case GEANY_FILETYPES_RUBY:*/ /* not sure what to use atm*/
			return "::";

		/* avoid confusion with other possible separators in group/section name */
		case GEANY_FILETYPES_CONF:
		case GEANY_FILETYPES_REST:
			return ":::";

		default:
			return ".";
	}
}


GString *symbols_get_macro_list(gint lang)
{
	guint j, i;
	GPtrArray *ftags;
	GString *words;
	gint tag_lang;
	TMTag *tag;

	if (app->tm_workspace->work_objects == NULL)
		return NULL;

	ftags = g_ptr_array_sized_new(50);
	words = g_string_sized_new(200);

	for (j = 0; j < app->tm_workspace->work_objects->len; j++)
	{
		GPtrArray *tags;

		tags = tm_tags_extract(TM_WORK_OBJECT(app->tm_workspace->work_objects->pdata[j])->tags_array,
			tm_tag_enum_t | tm_tag_variable_t | tm_tag_macro_t | tm_tag_macro_with_arg_t);
		if (NULL != tags)
		{
			for (i = 0; ((i < tags->len) && (i < editor_prefs.autocompletion_max_entries)); ++i)
			{
				tag = TM_TAG(tags->pdata[i]);
				tag_lang = (tag->atts.entry.file) ?
					tag->atts.entry.file->lang : tag->atts.file.lang;

				if (tag_lang == lang)
					g_ptr_array_add(ftags, (gpointer) tags->pdata[i]);
			}
			g_ptr_array_free(tags, TRUE);
		}
	}

	if (ftags->len == 0)
	{
		g_ptr_array_free(ftags, TRUE);
		g_string_free(words, TRUE);
		return NULL;
	}

	tm_tags_sort(ftags, NULL, FALSE);
	for (j = 0; j < ftags->len; j++)
	{
		if (j > 0)
			g_string_append_c(words, '\n');
		g_string_append(words, TM_TAG(ftags->pdata[j])->name);
	}
	g_ptr_array_free(ftags, TRUE);
	return words;
}


/* Note: if tags is sorted, we can use bsearch or tm_tags_find() to speed this up. */
static TMTag *
symbols_find_tm_tag(const GPtrArray *tags, const gchar *tag_name)
{
	guint i;
	g_return_val_if_fail(tags != NULL, NULL);

	for (i = 0; i < tags->len; ++i)
	{
		if (utils_str_equal(TM_TAG(tags->pdata[i])->name, tag_name))
			return TM_TAG(tags->pdata[i]);
	}
	return NULL;
}


static TMTag *find_work_object_tag(const TMWorkObject *workobj,
		const gchar *tag_name, guint type)
{
	GPtrArray *tags;
	TMTag *tmtag;

	if (G_LIKELY(workobj != NULL))
	{
		tags = tm_tags_extract(workobj->tags_array, type);
		if (tags != NULL)
		{
			tmtag = symbols_find_tm_tag(tags, tag_name);

			g_ptr_array_free(tags, TRUE);

			if (tmtag != NULL)
				return tmtag;
		}
	}
	return NULL;	/* not found */
}


static TMTag *find_workspace_tag(const gchar *tag_name, guint type)
{
	guint j;
	const GPtrArray *work_objects = NULL;

	if (app->tm_workspace != NULL)
		work_objects = app->tm_workspace->work_objects;

	if (work_objects != NULL)
	{
		for (j = 0; j < work_objects->len; j++)
		{
			TMWorkObject *workobj = TM_WORK_OBJECT(work_objects->pdata[j]);
			TMTag *tmtag;

			tmtag = find_work_object_tag(workobj, tag_name, type);
			if (tmtag != NULL)
				return tmtag;
		}
	}
	return NULL;	/* not found */
}


const gchar **symbols_get_html_entities(void)
{
	if (html_entities == NULL)
		html_tags_loaded(); /* if not yet created, force creation of the array but shouldn't occur */

	return (const gchar **) html_entities;
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
		return tag_a->atts.entry.line - tag_b->atts.entry.line;
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

	ret = tag_a->atts.entry.line - tag_b->atts.entry.line;
	if (ret == 0)
	{
		if (tag_a->atts.entry.scope == NULL)
			return -(tag_a->atts.entry.scope != tag_b->atts.entry.scope);
		if (tag_b->atts.entry.scope == NULL)
			return tag_a->atts.entry.scope != tag_b->atts.entry.scope;
		else
			return strcmp(tag_a->atts.entry.scope, tag_b->atts.entry.scope);
	}
	return ret;
}


static GList *get_tag_list(GeanyDocument *doc, guint tag_types)
{
	GList *tag_names = NULL;
	TMTag *tag;
	guint i;

	g_return_val_if_fail(doc, NULL);

	if (! doc->tm_file || ! doc->tm_file->tags_array)
		return NULL;

	for (i = 0; i < doc->tm_file->tags_array->len; ++i)
	{
		tag = TM_TAG(doc->tm_file->tags_array->pdata[i]);
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
	tv_iters.tag_namespace.stamp = -1;
	tv_iters.tag_struct.stamp = -1;
	tv_iters.tag_interface.stamp = -1;
	tv_iters.tag_type.stamp = -1;
	tv_iters.tag_other.stamp = -1;
}


static GdkPixbuf *get_tag_icon(const gchar *icon_name)
{
	static GtkIconTheme *icon_theme = NULL;
	static gint x, y;

	if (G_UNLIKELY(icon_theme == NULL))
	{
#ifndef G_OS_WIN32
		gchar *path = g_strconcat(GEANY_DATADIR, "/icons", NULL);
#endif
		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &x, &y);
		icon_theme = gtk_icon_theme_get_default();
#ifdef G_OS_WIN32
		gtk_icon_theme_append_search_path(icon_theme, "share\\icons");
#else
		gtk_icon_theme_append_search_path(icon_theme, path);
		g_free(path);
#endif
	}
	return gtk_icon_theme_load_icon(icon_theme, icon_name, x, 0, NULL);
}


/* finds the next iter at any level
 * @param iter in/out, the current iter, will be changed to the next one
 * @param down whether to try the child iter
 * @return TRUE if there @p iter was set, or FALSE if there is no next iter */
static gboolean next_iter(GtkTreeModel *model, GtkTreeIter *iter, gboolean down)
{
	GtkTreeIter guess;
	GtkTreeIter copy = *iter;

	/* go down if the item has children */
	if (down && gtk_tree_model_iter_children(model, &guess, iter))
		*iter = guess;
	/* or to the next item at the same level */
	else if (gtk_tree_model_iter_next(model, &copy))
		*iter = copy;
	/* or to the next item at a parent level */
	else if (gtk_tree_model_iter_parent(model, &guess, iter))
	{
		copy = guess;
		while(TRUE)
		{
			if (gtk_tree_model_iter_next(model, &copy))
			{
				*iter = copy;
				return TRUE;
			}
			else if (gtk_tree_model_iter_parent(model, &copy, &guess))
				guess = copy;
			else
				return FALSE;
		}
	}
	else
		return FALSE;

	return TRUE;
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
		gchar *icon_name = va_arg(args, gchar *);
		GdkPixbuf *icon = NULL;

		if (icon_name)
		{
			icon = get_tag_icon(icon_name);
		}

		g_assert(title != NULL);
		g_ptr_array_add(top_level_iter_names, title);

		if (!find_toplevel_iter(tree_store, iter, title))
			gtk_tree_store_append(tree_store, iter, NULL);

		if (G_IS_OBJECT(icon))
		{
			gtk_tree_store_set(tree_store, iter, SYMBOLS_COLUMN_ICON, icon, -1);
			g_object_unref(icon);
		}
		gtk_tree_store_set(tree_store, iter, SYMBOLS_COLUMN_NAME, title, -1);
	}
	va_end(args);
}


static void add_top_level_items(GeanyDocument *doc)
{
	filetype_id ft_id = doc->file_type->id;
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
				&(tv_iters.tag_function), _("Files"), NULL, NULL);
			break;
		}
		case GEANY_FILETYPES_DOCBOOK:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Chapter"), NULL,
				&(tv_iters.tag_class), _("Section"), NULL,
				&(tv_iters.tag_member), _("Sect1"), NULL,
				&(tv_iters.tag_macro), _("Sect2"), NULL,
				&(tv_iters.tag_variable), _("Sect3"), NULL,
				&(tv_iters.tag_struct), _("Appendix"), NULL,
				&(tv_iters.tag_other), _("Other"), NULL,
				NULL);
			break;
		}
		case GEANY_FILETYPES_HASKELL:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_namespace, _("Module"), NULL,
				&tv_iters.tag_type, _("Types"), NULL,
				&tv_iters.tag_macro, _("Type constructors"), NULL,
				&tv_iters.tag_function, _("Functions"), "classviewer-method",
				NULL);
			break;
		case GEANY_FILETYPES_COBOL:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_class, _("Program"), "classviewer-class",
				&tv_iters.tag_function, _("File"), "classviewer-method",
				&tv_iters.tag_namespace, _("Sections"), "classviewer-namespace",
				&tv_iters.tag_macro, _("Paragraph"), "classviewer-other",
				&tv_iters.tag_struct, _("Group"), "classviewer-struct",
				&tv_iters.tag_variable, _("Data"), "classviewer-var",
				NULL);
			break;
		case GEANY_FILETYPES_CONF:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_namespace, _("Sections"), "classviewer-other",
				&tv_iters.tag_macro, _("Keys"), "classviewer-var",
				NULL);
			break;
		case GEANY_FILETYPES_NSIS:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_namespace, _("Sections"), "classviewer-other",
				&tv_iters.tag_function, _("Functions"), "classviewer-method",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				NULL);
			break;
		case GEANY_FILETYPES_LATEX:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Command"), NULL,
				&(tv_iters.tag_class), _("Environment"), NULL,
				&(tv_iters.tag_member), _("Section"), NULL,
				&(tv_iters.tag_macro), _("Subsection"), NULL,
				&(tv_iters.tag_variable), _("Subsubsection"), NULL,
				&(tv_iters.tag_struct), _("Label"), NULL,
				&(tv_iters.tag_namespace), _("Chapter"), NULL,
				&(tv_iters.tag_other), _("Other"), NULL,
				NULL);
			break;
		}
		case GEANY_FILETYPES_MATLAB:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_struct), _("Structures"), "classviewer-struct",
				NULL);
			break;
		}
		case GEANY_FILETYPES_R:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_struct), _("Other"), NULL,
				NULL);
			break;
		}
		case GEANY_FILETYPES_PERL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), "classviewer-namespace",
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_macro), _("Labels"), NULL,
				&(tv_iters.tag_type), _("Constants"), NULL,
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_PHP:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_interface), _("Interfaces"), "classviewer-struct",
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_macro), _("Constants"), "classviewer-macro",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				NULL);
			break;
		}
		case GEANY_FILETYPES_HTML:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), NULL,
				&(tv_iters.tag_member), _("Anchors"), NULL,
				&(tv_iters.tag_namespace), _("H1 Headings"), NULL,
				&(tv_iters.tag_class), _("H2 Headings"), NULL,
				&(tv_iters.tag_variable), _("H3 Headings"), NULL,
				NULL);
			break;
		}
		case GEANY_FILETYPES_CSS:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_variable), _("ID Selectors"), "classviewer-var",
				&(tv_iters.tag_struct), _("Type Selectors"), "classviewer-struct", NULL);
			break;
		}
		case GEANY_FILETYPES_REST:
		case GEANY_FILETYPES_TXT2TAGS:
		case GEANY_FILETYPES_ABC:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Chapter"), NULL,
				&(tv_iters.tag_member), _("Section"), NULL,
				&(tv_iters.tag_macro), _("Subsection"), NULL,
				&(tv_iters.tag_variable), _("Subsubsection"), NULL,
				NULL);
			break;
		}
		case GEANY_FILETYPES_RUBY:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Modules"), NULL,
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_member), _("Singletons"), "classviewer-struct",
				&(tv_iters.tag_function), _("Methods"), "classviewer-method",
				NULL);
			break;
		}
		case GEANY_FILETYPES_TCL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Namespaces"), "classviewer-namespace",
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_member), _("Methods"), "classviewer-method",
				&(tv_iters.tag_function), _("Procedures"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_member), _("Methods"), "classviewer-macro",
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				&(tv_iters.tag_namespace), _("Imports"), "classviewer-namespace",
				NULL);
			break;
		}
		case GEANY_FILETYPES_VHDL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), "classviewer-namespace",
				&(tv_iters.tag_class), _("Entities"), "classviewer-class",
				&(tv_iters.tag_struct), _("Architectures"), "classviewer-struct",
				&(tv_iters.tag_type), _("Types"), "classviewer-other",
				&(tv_iters.tag_function), _("Functions / Procedures"), "classviewer-method",
				&(tv_iters.tag_variable), _("Variables / Signals"), "classviewer-var",
				&(tv_iters.tag_member), _("Processes / Components"), "classviewer-member",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_VERILOG:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_type), _("Events"), "classviewer-macro",
				&(tv_iters.tag_class), _("Modules"), "classviewer-class",
				&(tv_iters.tag_function), _("Functions / Tasks"), "classviewer-method",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_JAVA:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), "classviewer-namespace",
				&(tv_iters.tag_interface), _("Interfaces"), "classviewer-struct",
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_function), _("Methods"), "classviewer-method",
				&(tv_iters.tag_member), _("Members"), "classviewer-member",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_AS:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Package"), "classviewer-namespace",
				&(tv_iters.tag_interface), _("Interfaces"), "classviewer-struct",
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_member), _("Properties"), "classviewer-member",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				&(tv_iters.tag_macro), _("Constants"), "classviewer-macro",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_HAXE:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_interface), _("Interfaces"), "classviewer-struct",
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_function), _("Methods"), "classviewer-method",
				&(tv_iters.tag_type), _("Types"), "classviewer-macro",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_BASIC:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				&(tv_iters.tag_macro), _("Constants"), "classviewer-macro",
				&(tv_iters.tag_struct), _("Types"), "classviewer-namespace",
				&(tv_iters.tag_namespace), _("Labels"), "classviewer-member",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_F77:
		case GEANY_FILETYPES_FORTRAN:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Module"), "classviewer-class",
				&(tv_iters.tag_interface), _("Interfaces"), "classviewer-struct",
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_member), _("Subroutines"), "classviewer-method",
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				&(tv_iters.tag_type), _("Types"), "classviewer-namespace",
				&(tv_iters.tag_macro), _("Blocks"), "classviewer-member",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_ASM:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_namespace), _("Labels"), "classviewer-namespace",
				&(tv_iters.tag_function), _("Macros"), "classviewer-method",
				&(tv_iters.tag_macro), _("Defines"), "classviewer-macro",
				&(tv_iters.tag_struct), _("Types"), "classviewer-struct",
				NULL);
			break;
		}
		case GEANY_FILETYPES_MAKE:
			tag_list_add_groups(tag_store,
				&tv_iters.tag_function, _("Targets"), "classviewer-method",
				&tv_iters.tag_macro, _("Macros"), "classviewer-macro",
				NULL);
			break;
		case GEANY_FILETYPES_SQL:
		{
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_namespace), _("Procedures"), "classviewer-namespace",
				&(tv_iters.tag_struct), _("Indexes"), "classviewer-struct",
				&(tv_iters.tag_class), _("Tables"), "classviewer-class",
				&(tv_iters.tag_macro), _("Triggers"), "classviewer-macro",
				&(tv_iters.tag_member), _("Views"), "classviewer-var",
				&(tv_iters.tag_other), _("Other"), "classviewer-other",
				NULL);
			break;
		}
		case GEANY_FILETYPES_D:
		default:
		{
			if (ft_id == GEANY_FILETYPES_D)
				tag_list_add_groups(tag_store,
					&(tv_iters.tag_namespace), _("Module"), NULL, NULL);
			else
				tag_list_add_groups(tag_store,
					&(tv_iters.tag_namespace), _("Namespaces"), "classviewer-namespace", NULL);

			tag_list_add_groups(tag_store,
				&(tv_iters.tag_class), _("Classes"), "classviewer-class",
				&(tv_iters.tag_interface), _("Interfaces"), "classviewer-struct",
				&(tv_iters.tag_function), _("Functions"), "classviewer-method",
				&(tv_iters.tag_member), _("Members"), "classviewer-member",
				&(tv_iters.tag_struct), _("Structs"), "classviewer-struct",
				&(tv_iters.tag_type), _("Typedefs / Enums"), "classviewer-struct",
				NULL);

			if (ft_id != GEANY_FILETYPES_D)
			{
				tag_list_add_groups(tag_store,
					&(tv_iters.tag_macro), _("Macros"), "classviewer-macro", NULL);
			}
			tag_list_add_groups(tag_store,
				&(tv_iters.tag_variable), _("Variables"), "classviewer-var",
				&(tv_iters.tag_other), _("Other"), "classviewer-other", NULL);
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
	const gchar *scope = tag->atts.entry.scope;
	static GString *buffer = NULL;	/* buffer will be small so we can keep it for reuse */
	gboolean doc_is_utf8 = FALSE;

	/* encodings_convert_to_utf8_from_charset() fails with charset "None", so skip conversion
	 * for None at this point completely */
	if (utils_str_equal(doc->encoding, "UTF-8") ||
		utils_str_equal(doc->encoding, "None"))
		doc_is_utf8 = TRUE;

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

	g_string_append_printf(buffer, " [%lu]", tag->atts.entry.line);

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

	if (utf8_name != NULL)
		SETPTR(utf8_name, g_markup_escape_text(utf8_name, -1));

	return utf8_name;
}


/* find the last word in "foo::bar::blah", e.g. "blah" */
static const gchar *get_parent_name(const TMTag *tag, filetype_id ft_id)
{
	const gchar *scope = tag->atts.entry.scope;
	const gchar *separator = symbols_get_context_separator(ft_id);
	const gchar *str, *ptr;

	if (!scope)
		return NULL;

	str = scope;

	while (1)
	{
		ptr = strstr(str, separator);
		if (ptr)
		{
			str = ptr + strlen(separator);
		}
		else
			break;
	}

	return NZV(str) ? str : NULL;
}


static GtkTreeIter *get_tag_type_iter(TMTagType tag_type, filetype_id ft_id)
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
		return get_tag_icon("classviewer-var");
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
			utils_str_equal(t1->atts.entry.scope, t2->atts.entry.scope) &&
			/* include arglist in match to support e.g. C++ overloading */
			utils_str_equal(t1->atts.entry.arglist, t2->atts.entry.arglist));
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
	if (tag->atts.entry.scope)
	{
		for (p = tag->atts.entry.scope; *p != '\0'; p++)
			h = (h << 5) + h + *p;
	}
	/* for e.g. C++ overloading */
	if (tag->atts.entry.arglist)
	{
		for (p = tag->atts.entry.arglist; *p != '\0'; p++)
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
		cont = next_iter(GTK_TREE_MODEL(store), iter, FALSE);
	}

	return cont;
}


/* adds a new element in the parent table if it's key is known.
 * duplicates are kept */
static void update_parents_table(GHashTable *table, const TMTag *tag, const gchar *parent_name,
		const GtkTreeIter *iter)
{
	GList **list;
	if (g_hash_table_lookup_extended(table, tag->name, NULL, (gpointer *) &list) &&
		! utils_str_equal(parent_name, tag->name) /* prevent Foo::Foo from making parent = child */)
	{
		if (! list)
		{
			list = g_slice_alloc(sizeof *list);
			*list = NULL;
			g_hash_table_insert(table, tag->name, list);
		}
		*list = g_list_prepend(*list, g_slice_dup(GtkTreeIter, iter));
	}
}


static void free_iter_slice_list(gpointer data)
{
	GList **list = data;

	if (list)
	{
		GList *node;
		foreach_list(node, *list)
			g_slice_free(GtkTreeIter, node->data);
		g_list_free(*list);
		g_slice_free1(sizeof *list, list);
	}
}


/* inserts a @data in @table on key @tag.
 * previous data is not overwritten if the key is duplicated, but rather the
 * two values are kept in a list
 *
 * table is: GHashTable<TMTag, GList<GList<TMTag>>> */
static void tags_table_insert(GHashTable *table, TMTag *tag, GList *data)
{
	GList *list = g_hash_table_lookup(table, tag);
	list = g_list_prepend(list, data);
	g_hash_table_insert(table, tag, list);
}


/* looks up the entry in @table that better matches @tag.
 * if there are more than one candidate, the one that has closest line position to @tag is chosen */
static GList *tags_table_lookup(GHashTable *table, TMTag *tag)
{
	GList *data = NULL;
	GList *node = g_hash_table_lookup(table, tag);
	if (node)
	{
		glong delta;
		data = node->data;

#define TAG_DELTA(a, b) ABS((glong) TM_TAG(a)->atts.entry.line - (glong) TM_TAG(b)->atts.entry.line)

		delta = TAG_DELTA(((GList *) node->data)->data, tag);
		for (node = node->next; node; node = node->next)
		{
			glong d = TAG_DELTA(((GList *) node->data)->data, tag);

			if (d < delta)
			{
				data = node->data;
				delta = d;
			}
		}

#undef TAG_DELTA

	}
	return data;
}


/* removes the element at @tag from @table.
 * @tag must be the exact pointer used at insertion time */
static void tags_table_remove(GHashTable *table, TMTag *tag)
{
	GList *list = g_hash_table_lookup(table, tag);
	if (list)
	{
		GList *node;
		foreach_list(node, list)
		{
			if (((GList *) node->data)->data == tag)
				break;
		}
		list = g_list_delete_link(list, node);
		if (list)
			g_hash_table_insert(table, tag, list);
		else
			g_hash_table_remove(table, tag);
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
	/* parent table holds "tag-name":GtkTreeIter */
	parents_table = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, free_iter_slice_list);
	/* tags table is another representation of the @tags list, TMTag:GList<TMTag> */
	tags_table = g_hash_table_new_full(tag_hash, tag_equal, NULL, NULL);
	foreach_list(item, *tags)
	{
		TMTag *tag = item->data;
		const gchar *name;

		tags_table_insert(tags_table, tag, item);

		name = get_parent_name(tag, doc->file_type->id);
		if (name)
			g_hash_table_insert(parents_table, (gpointer) name, NULL);
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
			cont = next_iter(model, &iter, TRUE);
		else
		{
			GList *found_item;

			found_item = tags_table_lookup(tags_table, tag);
			if (! found_item) /* tag doesn't exist, remove it */
				cont = tree_store_remove_row(store, &iter);
			else /* tag still exist, update it */
			{
				const gchar *name;
				const gchar *parent_name;
				TMTag *found = found_item->data;

				parent_name = get_parent_name(found, doc->file_type->id);
				/* if parent is unknown, ignore it */
				if (parent_name && ! g_hash_table_lookup(parents_table, parent_name))
					parent_name = NULL;

				/* only update fields that (can) have changed (name that holds line
				 * number, and the tag itself) */
				name = get_symbol_name(doc, found, parent_name != NULL);
				gtk_tree_store_set(store, &iter,
						SYMBOLS_COLUMN_NAME, name,
						SYMBOLS_COLUMN_TAG, found,
						-1);

				update_parents_table(parents_table, found, parent_name, &iter);

				/* remove the updated tag from the table and list */
				tags_table_remove(tags_table, found);
				*tags = g_list_delete_link(*tags, found_item);

				cont = next_iter(model, &iter, TRUE);
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

		parent = get_tag_type_iter(tag->type, doc->file_type->id);
		if (G_UNLIKELY(! parent))
			geany_debug("Missing symbol-tree parent iter for type %d!", tag->type);
		else
		{
			gboolean expand;
			const gchar *name;
			const gchar *parent_name;
			gchar *tooltip;
			GdkPixbuf *icon = get_child_icon(store, parent);

			parent_name = get_parent_name(tag, doc->file_type->id);
			if (parent_name)
			{
				GList **candidates;
				GtkTreeIter *parent_search = NULL;

				/* walk parent candidates to find the better one.
				 * if there are more than one, take the one that has the closest line number
				 * after the tag we're searching the parent for */
				candidates = g_hash_table_lookup(parents_table, parent_name);
				if (candidates)
				{
					GList *node;
					glong delta = G_MAXLONG;
					foreach_list(node, *candidates)
					{
						TMTag *parent_tag;
						glong  d;

						gtk_tree_model_get(GTK_TREE_MODEL(store), node->data,
								SYMBOLS_COLUMN_TAG, &parent_tag, -1);

						d = tag->atts.entry.line - parent_tag->atts.entry.line;
						if (! parent_search || (d >= 0 && d < delta))
						{
							delta = d;
							parent_search = node->data;
						}
					}
				}

				if (parent_search)
					parent = parent_search;
				else
					parent_name = NULL;
			}

			/* only expand to the iter if the parent was empty, otherwise we let the
			 * folding as it was before (already expanded, or closed by the user) */
			expand = ! gtk_tree_model_iter_has_child(model, parent);

			/* insert the new element */
			gtk_tree_store_append(store, &iter, parent);
			name = get_symbol_name(doc, tag, parent_name != NULL);
			tooltip = get_symbol_tooltip(doc, tag);
			gtk_tree_store_set(store, &iter,
					SYMBOLS_COLUMN_NAME, name,
					SYMBOLS_COLUMN_TOOLTIP, tooltip,
					SYMBOLS_COLUMN_ICON, icon,
					SYMBOLS_COLUMN_TAG, tag,
					-1);
			g_free(tooltip);
			if (G_LIKELY(icon))
				g_object_unref(icon);

			update_parents_table(parents_table, tag, parent_name, &iter);

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
	return NZV(tag->atts.entry.scope) &&
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
							utils_str_equal(tag_a->atts.entry.scope, tag_b->atts.entry.scope)))
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

	g_return_val_if_fail(doc != NULL, FALSE);

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
			command = g_strdup_printf("%s %s", pre_process, NVL(getenv("CFLAGS"), ""));
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
			g_printerr(_("Failed to create tags file, perhaps because no tags "
				"were found.\n"));
			return 1;
		}
	}
	else
	{
		g_printerr(_("Usage: %s -g <Tag File> <File list>\n\n"), argv[0]);
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

	dialog = gtk_file_chooser_dialog_new(_("Load Tags"), GTK_WINDOW(main_widgets.window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_OK,
		NULL);
	gtk_widget_set_name(dialog, "GeanyDialog");
	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("Geany tag files (*.*.tags)"));
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


static void detect_tag_files(const GSList *file_list)
{
	const GSList *node;

	for (node = file_list; node != NULL; node = g_slist_next(node))
	{
		gchar *fname = node->data;
		gchar *utf8_fname = utils_get_utf8_from_locale(fname);
		GeanyFiletype *ft = detect_global_tags_filetype(utf8_fname);

		g_free(utf8_fname);

		if (FILETYPE_ID(ft) != GEANY_FILETYPES_NONE)
			ft->priv->tag_files = g_slist_prepend(ft->priv->tag_files, fname);
		else
			geany_debug("Unknown filetype for file '%s'.", fname);
	}
}


static void init_user_tags(void)
{
	GSList *file_list = NULL, *list = NULL;
	gchar *dir;

	dir = g_build_filename(app->configdir, "tags", NULL);
	/* create the user tags dir for next time if it doesn't exist */
	if (! g_file_test(dir, G_FILE_TEST_IS_DIR))
	{
		utils_mkdir(dir, FALSE);
	}
	file_list = utils_get_file_list_full(dir, TRUE, FALSE, NULL);

	SETPTR(dir, g_build_filename(app->datadir, "tags", NULL));
	list = utils_get_file_list_full(dir, TRUE, FALSE, NULL);
	g_free(dir);

	file_list = g_slist_concat(file_list, list);
	detect_tag_files(file_list);
	/* don't need to delete list contents because they are stored in ft->priv->tag_files */
	g_slist_free(file_list);
}


static void load_user_tags(filetype_id ft_id)
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


static gboolean goto_tag(const gchar *name, gboolean definition)
{
	const gint forward_types = tm_tag_prototype_t | tm_tag_externvar_t;
	guint type;
	TMTag *tmtag = NULL;
	GeanyDocument *old_doc = document_get_current();

	/* goto tag definition: all except prototypes / forward declarations / externs */
	type = (definition) ? tm_tag_max_t - forward_types : forward_types;

	/* first look in the current document */
	if (old_doc != NULL && old_doc->tm_file)
		tmtag = find_work_object_tag(old_doc->tm_file, name, type);

	/* if not found, look in the workspace */
	if (tmtag == NULL)
		tmtag = find_workspace_tag(name, type);

	if (tmtag != NULL)
	{
		GeanyDocument *new_doc = document_find_by_real_path(
			tmtag->atts.entry.file->work_object.file_name);

		if (new_doc)
		{
			/* If we are already on the tag line, swap definition/declaration */
			if (new_doc == old_doc &&
				tmtag->atts.entry.line == (guint)sci_get_current_line(old_doc->editor->sci) + 1)
			{
				if (goto_tag(name, !definition))
					return TRUE;
			}
		}
		else
		{
			/* not found in opened document, should open */
			new_doc = document_open_file(tmtag->atts.entry.file->work_object.file_name, FALSE, NULL, NULL);
		}

		if (navqueue_goto_line(old_doc, new_doc, tmtag->atts.entry.line))
			return TRUE;
	}
	return FALSE;
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


/* Should be used only with symbols_get_current_function. */
static gboolean current_function_changed(GeanyDocument *doc, gint cur_line, gint fold_level)
{
	static gint old_line = -2;
	static GeanyDocument *old_doc = NULL;
	static gint old_fold_num = -1;
	const gint fold_num = fold_level & SC_FOLDLEVELNUMBERMASK;
	gboolean ret;

	/* check if the cached line and file index have changed since last time: */
	if (doc == NULL || doc != old_doc)
		ret = TRUE;
	else
	if (cur_line == old_line)
		ret = FALSE;
	else
	{
		/* if the line has only changed by 1 */
		if (abs(cur_line - old_line) == 1)
		{
			const gint fn_fold =
				get_function_fold_number(doc);
			/* It's the same function if the fold number hasn't changed, or both the new
			 * and old fold numbers are above the function fold number. */
			gboolean same =
				fold_num == old_fold_num ||
				(old_fold_num > fn_fold && fold_num > fn_fold);

			ret = ! same;
		}
		else ret = TRUE;
	}

	/* record current line and file index for next time */
	old_line = cur_line;
	old_doc = doc;
	old_fold_num = fold_num;
	return ret;
}


/* Parse the function name up to 2 lines before tag_line.
 * C++ like syntax should be parsed by parse_cpp_function_at_line, otherwise the return
 * type or argument names can be confused with the function name. */
static gchar *parse_function_at_line(ScintillaObject *sci, gint tag_line)
{
	gint start, end, max_pos;
	gchar *cur_tag;
	gint fn_style;

	switch (sci_get_lexer(sci))
	{
		case SCLEX_RUBY:	fn_style = SCE_RB_DEFNAME; break;
		case SCLEX_PYTHON:	fn_style = SCE_P_DEFNAME; break;
		default: fn_style = SCE_C_IDENTIFIER;	/* several lexers use SCE_C_IDENTIFIER */
	}
	start = sci_get_position_from_line(sci, tag_line - 2);
	max_pos = sci_get_position_from_line(sci, tag_line + 1);
	while (sci_get_style_at(sci, start) != fn_style
		&& start < max_pos) start++;

	end = start;
	while (sci_get_style_at(sci, end) == fn_style
		&& end < max_pos) end++;

	if (start == end)
		return NULL;
	cur_tag = g_malloc(end - start + 1);
	sci_get_text_range(sci, start, end, cur_tag);
	return cur_tag;
}


/* Parse the function name */
static gchar *parse_cpp_function_at_line(ScintillaObject *sci, gint tag_line)
{
	gint start, end, first_pos, max_pos;
	gint tmp;
	gchar c;
	gchar *cur_tag;

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
	c = 0;
	/* Use tmp to find SCE_C_IDENTIFIER or SCE_C_GLOBALCLASS chars */
	while (start >= 0 && ((tmp = sci_get_style_at(sci, start)) == SCE_C_IDENTIFIER
		 ||  tmp == SCE_C_GLOBALCLASS
		 || (c = sci_get_char_at(sci, start)) == '~'
		 ||  c == ':'))
		start--;
	if (start != 0 && start < end) start++;	/* correct for last non-matching char */

	if (start == end) return NULL;
	cur_tag = g_malloc(end - start + 2);
	sci_get_text_range(sci, start, end + 1, cur_tag);
	return cur_tag;
}


/* Sets *tagname to point at the current function or tag name.
 * If doc is NULL, reset the cached current tag data to ensure it will be reparsed on the next
 * call to this function.
 * Returns: line number of the current tag, or -1 if unknown. */
gint symbols_get_current_function(GeanyDocument *doc, const gchar **tagname)
{
	static gint tag_line = -1;
	static gchar *cur_tag = NULL;
	gint line;
	gint fold_level;
	TMWorkObject *tm_file;

	if (doc == NULL)	/* reset current function */
	{
		current_function_changed(NULL, -1, -1);
		g_free(cur_tag);
		cur_tag = g_strdup(_("unknown"));
		if (tagname != NULL)
			*tagname = cur_tag;
		tag_line = -1;
		return tag_line;
	}

	line = sci_get_current_line(doc->editor->sci);
	fold_level = sci_get_fold_level(doc->editor->sci, line);
	/* check if the cached line and file index have changed since last time: */
	if (! current_function_changed(doc, line, fold_level))
	{
		/* we can assume same current function as before */
		*tagname = cur_tag;
		return tag_line;
	}
	g_free(cur_tag); /* free the old tag, it will be replaced. */

	/* if line is at base fold level, we're not in a function */
	if ((fold_level & SC_FOLDLEVELNUMBERMASK) == SC_FOLDLEVELBASE)
	{
		cur_tag = g_strdup(_("unknown"));
		*tagname = cur_tag;
		tag_line = -1;
		return tag_line;
	}
	tm_file = doc->tm_file;

	/* if the document has no changes, get the previous function name from TM */
	if (! doc->changed && tm_file != NULL && tm_file->tags_array != NULL)
	{
		const TMTag *tag = (const TMTag*) tm_get_current_function(tm_file->tags_array, line);

		if (tag != NULL)
		{
			gchar *tmp;
			tmp = tag->atts.entry.scope;
			cur_tag = tmp ? g_strconcat(tmp, "::", tag->name, NULL) : g_strdup(tag->name);
			*tagname = cur_tag;
			tag_line = tag->atts.entry.line;
			return tag_line;
		}
	}

	/* parse the current function name here because TM line numbers may have changed,
	 * and it would take too long to reparse the whole file. */
	if (doc->file_type != NULL && doc->file_type->id != GEANY_FILETYPES_NONE)
	{
		const gint fn_fold = get_function_fold_number(doc);

		tag_line = line;
		do	/* find the top level fold point */
		{
			tag_line = sci_get_fold_parent(doc->editor->sci, tag_line);
			fold_level = sci_get_fold_level(doc->editor->sci, tag_line);
		} while (tag_line >= 0 &&
			(fold_level & SC_FOLDLEVELNUMBERMASK) != fn_fold);

		if (tag_line >= 0)
		{
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

	cur_tag = g_strdup(_("unknown"));
	*tagname = cur_tag;
	tag_line = -1;
	return tag_line;
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

	g_signal_connect(menu, "show", G_CALLBACK(on_symbol_tree_menu_show), NULL);

	sidebar_add_common_menu_items(GTK_MENU(menu));
}


static void on_document_save(G_GNUC_UNUSED GObject *object, GeanyDocument *doc)
{
	gchar *f = g_build_filename(app->configdir, "ignore.tags", NULL);

	g_return_if_fail(NZV(doc->real_path));

	if (utils_str_equal(doc->real_path, f))
		load_c_ignore_tags();

	g_free(f);
}


void symbols_init(void)
{
	gchar *f;

	create_taglist_popup_menu();

	f = g_build_filename(app->configdir, "ignore.tags", NULL);
	ui_add_config_file_menu_item(f, NULL, NULL);
	g_free(f);

	g_signal_connect(geany_object, "document-save", G_CALLBACK(on_document_save), NULL);
}


void symbols_finalize(void)
{
	g_strfreev(html_entities);
	g_strfreev(c_tags_ignore);
}
