/*
 *      symbol.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2024 The Geany contributors
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "editor.h"
#include "encodings.h"
#include "symbol.h"
#include "tm_tag.h"
#include "utils.h"

#include <gtk/gtk.h>


typedef struct GeanySymbol
{
	gchar *name;
	gchar *detail;
	gchar *scope;
	gchar *file;
	TMParserType lang;
	gulong line;
	gulong pos;
	glong kind;
	guint icon;

	TMTag *tag;

	gint refcount; /* the reference count of the symbol */
} GeanySymbol;


GeanySymbol *geany_symbol_new(gchar *name, gchar *detail, gchar *scope, gchar *file,
	TMParserType lang, glong kind, gulong line, gulong pos, guint icon)
{
	GeanySymbol *sym = g_slice_new0(GeanySymbol);
	sym->refcount = 1;

	sym->name = g_strdup(name);
	sym->detail = g_strdup(detail);
	sym->scope = g_strdup(scope);
	sym->file = g_strdup(file);
	sym->lang = lang;
	sym->kind = kind;
	sym->line = line;
	sym->pos = pos;
	sym->icon = icon;

	return sym;
}


GeanySymbol *geany_symbol_new_from_tag(TMTag *tag)
{
	GeanySymbol *sym = g_slice_new0(GeanySymbol);
	sym->refcount = 1;
	sym->tag = tm_tag_ref(tag);
	return sym;
}


static void symbol_destroy(GeanySymbol *sym)
{
	tm_tag_unref(sym->tag);

	g_free(sym->name);
	g_free(sym->detail);
	g_free(sym->scope);
	g_free(sym->file);
}


GEANY_API_SYMBOL
GType geany_symbol_get_type(void)
{
	static GType gtype = 0;
	if (G_UNLIKELY (gtype == 0))
	{
		gtype = g_boxed_type_register_static("GeanySymbol", (GBoxedCopyFunc)geany_symbol_ref,
											 (GBoxedFreeFunc)geany_symbol_unref);
	}
	return gtype;
}


void geany_symbol_unref(GeanySymbol *sym)
{
	if (sym && g_atomic_int_dec_and_test(&sym->refcount))
	{
		symbol_destroy(sym);
		g_slice_free(GeanySymbol, sym);
	}
}


GeanySymbol *geany_symbol_ref(GeanySymbol *sym)
{
	g_atomic_int_inc(&sym->refcount);
	return sym;
}


static gchar *get_symbol_name(const gchar *encoding, const TMTag *tag, gboolean include_scope,
	gboolean include_line)
{
	gchar *utf8_name;
	const gchar *scope = tag->scope;
	GString *buffer = NULL;
	gboolean doc_is_utf8 = FALSE;

	/* encodings_convert_to_utf8_from_charset() fails with charset "None", so skip conversion
	 * for None at this point completely */
	if (utils_str_equal(encoding, "UTF-8") ||
		utils_str_equal(encoding, "None"))
		doc_is_utf8 = TRUE;
	else /* normally the tags will always be in UTF-8 since we parse from our buffer, but a
		  * plugin might have called tm_source_file_update(), so check to be sure */
		doc_is_utf8 = g_utf8_validate(tag->name, -1, NULL);

	if (! doc_is_utf8)
		utf8_name = encodings_convert_to_utf8_from_charset(tag->name,
			-1, encoding, TRUE);
	else
		utf8_name = tag->name;

	if (utf8_name == NULL)
		return NULL;

	buffer = g_string_new(NULL);

	/* check first char of scope is a wordchar */
	if (include_scope && scope &&
		strpbrk(scope, GEANY_WORDCHARS) == scope)
	{
		const gchar *sep = tm_parser_scope_separator_printable(tag->lang);

		g_string_append(buffer, scope);
		g_string_append(buffer, sep);
	}
	g_string_append(buffer, utf8_name);

	if (! doc_is_utf8)
		g_free(utf8_name);

	if (include_line)
		g_string_append_printf(buffer, " [%lu]", tag->line);

	return g_string_free(buffer, FALSE);
}


// Returns NULL if the tag is not a variable or callable
static gchar *get_symbol_tooltip(const gchar *encoding, const TMTag *tag, gboolean include_scope)
{
	gchar *utf8_name = tm_parser_format_function(tag->lang, tag->name,
		tag->arglist, tag->var_type, tag->scope);

	if (!utf8_name && tag->var_type &&
		tag->type & (tm_tag_field_t | tm_tag_member_t | tm_tag_variable_t | tm_tag_externvar_t))
	{
		gchar *scope = include_scope ? tag->scope : NULL;
		utf8_name = tm_parser_format_variable(tag->lang, tag->name, tag->var_type, scope);
	}

	/* encodings_convert_to_utf8_from_charset() fails with charset "None", so skip conversion
	 * for None at this point completely */
	if (utf8_name != NULL &&
		! utils_str_equal(encoding, "UTF-8") &&
		! utils_str_equal(encoding, "None"))
	{
		SETPTR(utf8_name,
			encodings_convert_to_utf8_from_charset(utf8_name, -1, encoding, TRUE));
	}

	return utf8_name;
}


gulong geany_symbol_get_line(const GeanySymbol *sym)
{
	if (sym->tag)
		return sym->tag->line;
	return sym->line;
}


const gchar *geany_symbol_get_scope(const GeanySymbol *sym)
{
	if (sym->tag)
		return sym->tag->scope;
	return sym->scope;
}


const gchar *geany_symbol_get_name(const GeanySymbol *sym)
{
	if (sym->tag)
		return sym->tag->name;
	return sym->name;
}


const gchar *geany_symbol_get_detail(const GeanySymbol *sym)
{
	if (sym->tag)
		return sym->tag->arglist;
	return sym->detail;
}


glong geany_symbol_get_kind(const GeanySymbol *sym)
{
	if (sym->tag)
		return sym->tag->type;
	return sym->kind;
}


gchar *geany_symbol_get_name_with_scope(const GeanySymbol *sym)
{
	gchar *name = NULL;

	if (sym->tag)
	{
		if (EMPTY(sym->tag->scope))
		{
			/* simple case, just use the tag name */
			name = g_strdup(sym->tag->name);
		}
		else if (! tm_parser_has_full_scope(sym->tag->lang))
		{
			/* if the parser doesn't use fully qualified scope, use the name alone but
			 * prevent Foo::Foo from making parent = child */
			if (utils_str_equal(sym->tag->scope, sym->tag->name))
				name = NULL;
			else
				name = g_strdup(sym->tag->name);
		}
		else
		{
			/* build the fully qualified scope as get_parent_name() would return it for a child tag */
			name = g_strconcat(sym->tag->scope, tm_parser_scope_separator(sym->tag->lang), sym->tag->name, NULL);
		}
	}
	else
	{
		if (EMPTY(sym->scope))
			name = g_strdup(sym->name);
		else
			name = g_strconcat(sym->scope, tm_parser_scope_separator(sym->lang), sym->name, NULL);
	}

	return name;
}


gchar *geany_symbol_get_symtree_name(const GeanySymbol *sym, const gchar *encoding, gboolean include_scope)
{
	if (sym->tag)
		return get_symbol_name(encoding, sym->tag, include_scope, TRUE);

	return g_strdup(sym->name);
}


gchar *geany_symbol_get_symtree_tooltip(const GeanySymbol *sym, const gchar *encoding)
{
	if (sym->tag)
		return get_symbol_tooltip(encoding, sym->tag, FALSE);

	return g_strdup(sym->detail);
}


gchar *geany_symbol_get_goto_name(const GeanySymbol *sym, const gchar *encoding)
{
	if (sym->tag)
		return get_symbol_name(encoding, sym->tag, TRUE, FALSE);

	/* unused now */
	return g_strdup("");
}


gchar *geany_symbol_get_goto_tooltip(const GeanySymbol *sym, const gchar *encoding)
{
	if (sym->tag)
		return get_symbol_tooltip(encoding, sym->tag, TRUE);

	/* unused now */
	return g_strdup("");
}


gboolean geany_symbol_equal(const GeanySymbol *a, const GeanySymbol *b)
{
	if (a->tag && b->tag)
		return tm_tags_equal(a->tag, b->tag);
	else if (!a->tag && !b->tag)
	{
		//TODO
		return FALSE;
	}

	/* mixing GeanySymbols based on TMTags and external symbols not supported */
	g_warn_if_reached();
	return FALSE;
}


gint geany_symbol_get_symtree_group_index(const GeanySymbol *sym)
{
	if (sym->tag)
	{
		/* TODO: tm_parser_get_sidebar_group() goes through groups one by one.
		 * If this happens to be slow for tree construction, create a lookup
		 * table for them. */
		return tm_parser_get_sidebar_group(sym->tag->lang, sym->tag->type);
	}

	return 0;  /* 'Symbols' group always at index 0 */
}
