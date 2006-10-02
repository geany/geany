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
#include "symbols.h"
#include "utils.h"
#include "filetypes.h"
#include "sci_cb.h"	// html_entities


enum	// Geany tag files
{
	GTF_C,
	GTF_PASCAL,
	GTF_PHP,
	GTF_HTML_ENTITIES,
	GTF_LATEX,
	GTF_MAX
};

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

// langType used in TagManager (see tagmanager/parsers.h)
enum	// Geany lang type
{
	GLT_C = 0,
	GLT_CPP = 1,
	GLT_PASCAL = 4,
	GLT_PHP = 6,
	GLT_LATEX = 8
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
		case GEANY_FILETYPES_C:		gtf = GTF_C;		lt = GLT_C; break;
		case GEANY_FILETYPES_CPP:	gtf = GTF_C;		lt = GLT_CPP; break;
		case GEANY_FILETYPES_PASCAL:gtf = GTF_PASCAL;	lt = GLT_PASCAL; break;
		case GEANY_FILETYPES_PHP:	gtf = GTF_PHP;		lt = GLT_PHP; break;
		case GEANY_FILETYPES_LATEX:	gtf = GTF_LATEX;	lt = GLT_LATEX; break;
		default:
			return;
	}
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


GString *symbols_get_global_keywords()
{
	GString *s = NULL;

	if ((app->tm_workspace) && (app->tm_workspace->global_tags))
	{
		guint j;
		GPtrArray *g_typedefs = tm_tags_extract(app->tm_workspace->global_tags,
				tm_tag_typedef_t | tm_tag_struct_t | tm_tag_class_t);

		if ((g_typedefs) && (g_typedefs->len > 0))
		{
			s = g_string_sized_new(g_typedefs->len * 10);
			for (j = 0; j < g_typedefs->len; ++j)
			{
				if (!(TM_TAG(g_typedefs->pdata[j])->atts.entry.scope))
				{
					g_string_append(s, TM_TAG(g_typedefs->pdata[j])->name);
					g_string_append_c(s, ' ');
				}
			}
		}
		g_ptr_array_free(g_typedefs, TRUE);
	}
	return s;
}
