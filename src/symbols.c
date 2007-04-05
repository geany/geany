/*
 *      symbols.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2007 Enrico Tröger <enrico.troeger@uvena.de>
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
 * Tagmanager related convenience functions.
 * Tagmanager parses tags in the current documents, known as the workspace, plus global tags,
 * which are lists of tags for each filetype. Global tags are loaded when a document with a
 * matching filetype is first loaded.
 */

#include "geany.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "symbols.h"
#include "utils.h"
#include "filetypes.h"
#include "encodings.h"
#include "document.h"
#include "support.h"


const guint TM_GLOBAL_TYPE_MASK =
	tm_tag_class_t | tm_tag_enum_t | tm_tag_interface_t |
	tm_tag_struct_t | tm_tag_typedef_t | tm_tag_union_t;


static gchar **html_entities = NULL;


typedef struct
{
	gboolean	tags_loaded;
	const gchar	*tag_file;
} TagFileInfo;

enum	// Geany tag files
{
	GTF_C,
	GTF_PASCAL,
	GTF_PHP,
	GTF_HTML_ENTITIES,
	GTF_LATEX,
	GTF_MAX
};

static TagFileInfo tag_file_info[GTF_MAX] =
{
	{FALSE, "global.tags"},
	{FALSE, "pascal.tags"},
	{FALSE, "php.tags"},
	{FALSE, "html_entities.tags"},
	{FALSE, "latex.tags"}
};


static void html_tags_loaded();


// Ensure that the global tags file for the file_type_idx filetype is loaded.
void symbols_global_tags_loaded(gint file_type_idx)
{
	TagFileInfo *tfi;
	gint tag_type;

	if (app->ignore_global_tags) return;

	switch (file_type_idx)
	{
		case GEANY_FILETYPES_PHP:
		case GEANY_FILETYPES_HTML:
			html_tags_loaded();
	}
	switch (file_type_idx)
	{
		case GEANY_FILETYPES_CPP:
			symbols_global_tags_loaded(GEANY_FILETYPES_C);	// load C global tags
			// no C++ tagfile yet
			return;
		case GEANY_FILETYPES_C:		tag_type = GTF_C; break;
		case GEANY_FILETYPES_PASCAL:tag_type = GTF_PASCAL; break;
		case GEANY_FILETYPES_PHP:	tag_type = GTF_PHP; break;
		case GEANY_FILETYPES_LATEX:	tag_type = GTF_LATEX; break;
		default:
			return;
	}
	tfi = &tag_file_info[tag_type];

	if (! tfi->tags_loaded)
	{
		gchar *fname = g_strconcat(app->datadir, G_DIR_SEPARATOR_S, tfi->tag_file, NULL);
		gint tm_lang;

		tm_lang = filetypes[file_type_idx]->lang;
		tm_workspace_load_global_tags(fname, tm_lang);
		tfi->tags_loaded = TRUE;
		g_free(fname);
	}
}


// HTML tagfile is just a list of entities for autocompletion (e.g. '&amp;')
static void html_tags_loaded()
{
	TagFileInfo *tfi;

	if (app->ignore_global_tags) return;

	tfi = &tag_file_info[GTF_HTML_ENTITIES];
	if (! tfi->tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S, tfi->tag_file, NULL);

		html_entities = utils_read_file_in_array(file);
		tfi->tags_loaded = TRUE;
		g_free(file);
	}
}


GString *symbols_find_tags_as_string(GPtrArray *tags_array, guint tag_types)
{
	guint j;
	GString *s = NULL;
	GPtrArray *typedefs;

	g_return_val_if_fail(tags_array != NULL, NULL);

	typedefs = tm_tags_extract(tags_array, tag_types);

	if ((typedefs) && (typedefs->len > 0))
	{
		s = g_string_sized_new(typedefs->len * 10);
		for (j = 0; j < typedefs->len; ++j)
		{
			if (!(TM_TAG(typedefs->pdata[j])->atts.entry.scope))
			{
				if (TM_TAG(typedefs->pdata[j])->name)
				{
					if (j != 0)
						g_string_append_c(s, ' ');
					g_string_append(s, TM_TAG(typedefs->pdata[j])->name);
				}
			}
		}
	}
	g_ptr_array_free(typedefs, TRUE);
	return s;
}


const gchar *symbols_get_context_separator(filetype_id ft_id)
{
	gchar *cosep;

	switch (ft_id)
	{
		case GEANY_FILETYPES_C:	// for C++ .h headers or C structs
		case GEANY_FILETYPES_CPP:
		{
			static gchar cc[] = "::";

			cosep = cc;
		}
		break;

		default:
		{
			static gchar def[] = ".";

			cosep = def;
		}
	}
	return cosep;	// return ptr to static string
}


GString *symbols_get_macro_list()
{
	guint j, i;
	const GPtrArray *tags;
	GPtrArray *ftags;
	GString *words;

	if (app->tm_workspace->work_objects == NULL)
		return NULL;

	ftags = g_ptr_array_sized_new(50);
	words = g_string_sized_new(200);

	for (j = 0; j < app->tm_workspace->work_objects->len; j++)
	{
		tags = tm_tags_extract(TM_WORK_OBJECT(app->tm_workspace->work_objects->pdata[j])->tags_array,
			tm_tag_enum_t | tm_tag_variable_t | tm_tag_macro_t | tm_tag_macro_with_arg_t);
		if (NULL != tags)
		{
			for (i = 0; ((i < tags->len) && (i < GEANY_MAX_AUTOCOMPLETE_WORDS)); ++i)
			{
				g_ptr_array_add(ftags, (gpointer) tags->pdata[i]);
			}
		}
	}
	tm_tags_sort(ftags, NULL, FALSE);
	for (j = 0; j < ftags->len; j++)
	{
		if (j > 0) g_string_append_c(words, ' ');
		g_string_append(words, TM_TAG(ftags->pdata[j])->name);
	}
	g_ptr_array_free(ftags, TRUE);
	return words;
}


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


TMTag *symbols_find_in_workspace(const gchar *tag_name, gint type)
{
	guint j;
	const GPtrArray *tags;
	TMTag *tmtag;

	if (app->tm_workspace->work_objects != NULL)
	{
		for (j = 0; j < app->tm_workspace->work_objects->len; j++)
		{
			tags = tm_tags_extract(
				TM_WORK_OBJECT(app->tm_workspace->work_objects->pdata[j])->tags_array,
				type);
			if (tags == NULL) continue;

			tmtag = symbols_find_tm_tag(tags, tag_name);
			if (tmtag != NULL)
				return tmtag;
		}
	}
	return NULL;	// not found
}


const gchar **symbols_get_html_entities()
{
	if (html_entities == NULL)
		html_tags_loaded(); // if not yet created, force creation of the array but shouldn't occur

	return (const gchar **) html_entities;
}


void symbols_finalize()
{
	g_strfreev(html_entities);
}


// small struct to track tag name and type together
typedef struct GeanySymbol
{
	gchar	*str;
	gint	 type;
	gint	 line;
} GeanySymbol;


/* wrapper function to let strcmp work with GeanySymbol struct */
static gint compare_symbol(const GeanySymbol *a, const GeanySymbol *b)
{
	if (a == NULL || b == NULL) return 0;

	return strcmp(a->str, b->str);
}


static const GList *
get_tag_list(gint idx, guint tag_types)
{
	static GList *tag_names = NULL;

	if (idx >= 0 && doc_list[idx].is_valid && doc_list[idx].tm_file &&
		doc_list[idx].tm_file->tags_array)
	{
		TMTag *tag;
		guint i;
		GeanySymbol *symbol;
		gboolean doc_is_utf8 = FALSE;
		gchar *utf8_name;
		const gchar *cosep =
			symbols_get_context_separator(FILETYPE_ID(doc_list[idx].file_type));

		if (tag_names)
		{
			GList *tmp;
			for (tmp = tag_names; tmp; tmp = g_list_next(tmp))
			{
				g_free(((GeanySymbol*)tmp->data)->str);
				g_free(tmp->data);
			}
			g_list_free(tag_names);
			tag_names = NULL;
		}

		// do this comparison only once
		if (utils_str_equal(doc_list[idx].encoding, "UTF-8")) doc_is_utf8 = TRUE;

		for (i = 0; i < (doc_list[idx].tm_file)->tags_array->len; ++i)
		{
			tag = TM_TAG((doc_list[idx].tm_file)->tags_array->pdata[i]);
			if (tag == NULL)
				return NULL;

			if (tag->type & tag_types)
			{
				if (! doc_is_utf8) utf8_name = encodings_convert_to_utf8_from_charset(tag->name,
															-1, doc_list[idx].encoding, TRUE);
				else utf8_name = tag->name;
				if ((tag->atts.entry.scope != NULL) && isalpha(tag->atts.entry.scope[0]))
				{
					symbol = g_new0(GeanySymbol, 1);
					symbol->str = g_strdup_printf("%s%s%s [%ld]", tag->atts.entry.scope, cosep,
																utf8_name, tag->atts.entry.line);
					symbol->type = tag->type;
					symbol->line = tag->atts.entry.line;
					tag_names = g_list_prepend(tag_names, symbol);
				}
				else
				{
					symbol = g_new0(GeanySymbol, 1);
					symbol->str = g_strdup_printf("%s [%ld]", utf8_name, tag->atts.entry.line);
					symbol->type = tag->type;
					symbol->line = tag->atts.entry.line;
					tag_names = g_list_prepend(tag_names, symbol);
				}
				if (! doc_is_utf8) g_free(utf8_name);
			}
		}
		tag_names = g_list_sort(tag_names, (GCompareFunc) compare_symbol);
		return tag_names;
	}
	else
		return NULL;
}


struct TreeviewSymbols
{
	GtkTreeIter		 tag_function;
	GtkTreeIter		 tag_class;
	GtkTreeIter		 tag_macro;
	GtkTreeIter		 tag_member;
	GtkTreeIter		 tag_variable;
	GtkTreeIter		 tag_namespace;
	GtkTreeIter		 tag_struct;
	GtkTreeIter		 tag_other;
} tv_iters;


static void init_tag_iters()
{
	// init all GtkTreeIters with -1 to make them invalid to avoid crashes when switching between
	// filetypes(e.g. config file to Python crashes Geany without this)
	tv_iters.tag_function.stamp = -1;
	tv_iters.tag_class.stamp = -1;
	tv_iters.tag_member.stamp = -1;
	tv_iters.tag_macro.stamp = -1;
	tv_iters.tag_variable.stamp = -1;
	tv_iters.tag_namespace.stamp = -1;
	tv_iters.tag_struct.stamp = -1;
	tv_iters.tag_other.stamp = -1;
}


static void init_tag_list(gint idx)
{
	filetype_id ft_id = doc_list[idx].file_type->id;

	init_tag_iters();

	switch (ft_id)
	{
		case GEANY_FILETYPES_DIFF:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Files"), -1);
			break;
		}
		case GEANY_FILETYPES_DOCBOOK:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Chapter"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Section"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("Sect1"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Sect2"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Sect3"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Appendix"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
			//gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			//gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, _("Other"), -1);
			break;
		}
		case GEANY_FILETYPES_LATEX:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Command"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Environment"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("Section"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Subsection"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Subsubsection"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Label"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, _("Chapter"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
			break;
		}
		case GEANY_FILETYPES_PERL:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Package"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Function"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("My"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Local"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Our"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Label"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, _("Begin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
*/
			break;
		}
		case GEANY_FILETYPES_RUBY:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Classes"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("Singletons"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Mixins"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Methods"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Members"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Variables"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, _("Begin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
*/
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Classes"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("Methods"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Functions"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Mixin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Variables"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Members"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, _("Begin"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
*/
			break;
		}
		case GEANY_FILETYPES_VHDL:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Functions"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Constants"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("Members"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Macros"), -1);
*/			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Variables"), -1);
/*			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, _("Namespaces"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Signals"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
*/
			break;
		}
		case GEANY_FILETYPES_JAVA:
		{
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, _("Package"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Interfaces"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Classes"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Methods"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("Members"), -1);
			//gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
			//gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Macros"), -1);
			//gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			//gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Variables"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
			break;
		}
		case GEANY_FILETYPES_D:
		default:
		{
			gchar *namespace_name;

			switch (ft_id)
			{
				case GEANY_FILETYPES_D:
				namespace_name = _("Module");	// one file can only belong to one module
				break;

				default:
				namespace_name = _("Namespaces");
			}
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_namespace), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_namespace), 0, namespace_name, -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_class), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_class), 0, _("Classes"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_function), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_function), 0, _("Functions"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_member), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_member), 0, _("Members"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_struct), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_struct), 0, _("Structs / Typedefs"), -1);
			// TODO: maybe D Mixins could be parsed instead of macros
			if (ft_id != GEANY_FILETYPES_D)
			{
				gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_macro), NULL);
				gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_macro), 0, _("Macros"), -1);
			}
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_variable), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_variable), 0, _("Variables"), -1);
			gtk_tree_store_append(doc_list[idx].tag_store, &(tv_iters.tag_other), NULL);
			gtk_tree_store_set(doc_list[idx].tag_store, &(tv_iters.tag_other), 0, _("Other"), -1);
		}
	}
}


gboolean symbols_recreate_tag_list(gint idx)
{
	GList *tmp;
	const GList *tags;
	GtkTreeIter iter;
	GtkTreeModel *model;

	tags = get_tag_list(idx, tm_tag_max_t);
	if (doc_list[idx].tm_file == NULL || tags == NULL) return FALSE;

	gtk_tree_store_clear(doc_list[idx].tag_store);
	// unref the store to speed up the filling(from TreeView Tutorial)
	model = gtk_tree_view_get_model(GTK_TREE_VIEW(doc_list[idx].tag_tree));
	g_object_ref(model); // Make sure the model stays with us after the tree view unrefs it
	gtk_tree_view_set_model(GTK_TREE_VIEW(doc_list[idx].tag_tree), NULL); // Detach model from view

	init_tag_list(idx);
	for (tmp = (GList*)tags; tmp; tmp = g_list_next(tmp))
	{
		const GeanySymbol *symbol = (GeanySymbol*)tmp->data;

		switch (symbol->type)
		{
			case tm_tag_prototype_t:
			case tm_tag_method_t:
			case tm_tag_function_t:
			{
				if (tv_iters.tag_function.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_function));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
				break;
			}
			case tm_tag_macro_t:
			case tm_tag_macro_with_arg_t:
			{
				if (tv_iters.tag_macro.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_macro));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
				break;
			}
			case tm_tag_class_t:
			{
				if (tv_iters.tag_class.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_class));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
				break;
			}
			case tm_tag_member_t:
			case tm_tag_field_t:
			{
				if (tv_iters.tag_member.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_member));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
				break;
			}
			case tm_tag_typedef_t:
			case tm_tag_enum_t:
			case tm_tag_union_t:
			case tm_tag_struct_t:
			case tm_tag_interface_t:
			{
				if (tv_iters.tag_struct.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_struct));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
				break;
			}
			case tm_tag_variable_t:
			{
				if (tv_iters.tag_variable.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_variable));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
				break;
			}
			case tm_tag_namespace_t:
			case tm_tag_package_t:
			{
				if (tv_iters.tag_namespace.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_namespace));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
				break;
			}
			default:
			{
				if (tv_iters.tag_other.stamp == -1) break;
				gtk_tree_store_append(doc_list[idx].tag_store, &iter, &(tv_iters.tag_other));
				gtk_tree_store_set(doc_list[idx].tag_store, &iter, 0, symbol->str, -1);
			}
		}
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(doc_list[idx].tag_tree), model); // Re-attach model to view
	g_object_unref(model);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(doc_list[idx].tag_tree));
	return TRUE;
}


/* Adapted from anjuta-2.0.2/global-tags/tm_global_tags.c, thanks.
 * Needs full path and \" quoting characters around filenames.
 * Example:
 * geany -g tagsfile \"/home/nmt/svn/geany/src/d*.h\" */
int symbols_generate_global_tags(int argc, char **argv)
{
	/* -E pre-process, -dD output user macros, -p prof info (?),
	 * -undef remove builtin macros (seems to be needed with FC5 gcc 4.1.1 */
	const char pre_process[] = "gcc -E -dD -p -undef";

	if (argc > 2)
	{
		/* Create global taglist */
		int status;
		char *command;
		command = g_strdup_printf("%s %s", pre_process,
								  NVL(getenv("CFLAGS"), ""));
		//printf(">%s<\n", command);
		status = tm_workspace_create_global_tags(command,
												 (const char **) (argv + 2),
												 argc - 2, argv[1]);
		g_free(command);
		if (!status)
			return 1;
	}
	else
	{
		fprintf(stderr, "Usage: %s -g <Tag File> <File list>\n\n", argv[0]);
		fprintf(stderr, "Example:\n"
			"CFLAGS=`pkg-config gtk+-2.0 --cflags` %s -g gtk2.c.tags"
			" /usr/include/gtk-2.0/gtk/gtk.h\n", argv[0]);
		return 1;
	}
	return 0;
}


