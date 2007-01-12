/*
 *      symbols.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
 *      Copyright 2006 Nick Treleaven <nick.treleaven@btinternet.com>
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

#include "geany.h"

#include <ctype.h>

#include "symbols.h"
#include "utils.h"
#include "filetypes.h"
#include "encodings.h"
#include "document.h"


const guint TM_GLOBAL_TYPE_MASK =
	tm_tag_class_t | tm_tag_enum_t | tm_tag_interface_t |
	tm_tag_struct_t | tm_tag_typedef_t | tm_tag_union_t;


enum	// Geany tag files
{
	GTF_C,
	GTF_PASCAL,
	GTF_PHP,
	GTF_HTML_ENTITIES,
	GTF_LATEX,
	GTF_MAX
};

static gchar **html_entities = NULL;

typedef struct
{
	gboolean	tags_loaded;
	const gchar	*tag_file;
} TagFileInfo;

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
	gint gtf, lt;

	if (app->ignore_global_tags) return;

	switch (file_type_idx)
	{
		case GEANY_FILETYPES_HTML:
			html_tags_loaded();
			return;
		case GEANY_FILETYPES_C:		gtf = GTF_C; break;
		case GEANY_FILETYPES_CPP:	gtf = GTF_C; break;
		case GEANY_FILETYPES_PASCAL:gtf = GTF_PASCAL; break;
		case GEANY_FILETYPES_PHP:	gtf = GTF_PHP; break;
		case GEANY_FILETYPES_LATEX:	gtf = GTF_LATEX; break;
		default:
			return;
	}
	lt = filetypes[file_type_idx]->lang;
	tfi = &tag_file_info[gtf];

	if (! tfi->tags_loaded)
	{
		gchar *file = g_strconcat(app->datadir, G_DIR_SEPARATOR_S, tfi->tag_file, NULL);
		tm_workspace_load_global_tags(file, lt);
		tfi->tags_loaded = TRUE;
		g_free(file);
	}
}


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


const GList *symbols_get_tag_list(gint idx, guint tag_types)
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
		tag_names = g_list_sort(tag_names, (GCompareFunc) utils_compare_symbol);
		return tag_names;
	}
	else
		return NULL;
}


GString *symbols_get_macro_list()
{
	guint j, i;
	const GPtrArray *tags;
	GPtrArray *ftags;
	GString *words;

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


gchar **symbols_get_html_entities()
{
	if (html_entities == NULL)
		html_tags_loaded(); // if not yet created, force creation of the array but shouldn't occur

	return html_entities;
}


void symbols_finalize()
{
	g_strfreev(html_entities);
}
