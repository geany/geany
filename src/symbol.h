/*
 *      symbol.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_SYMBOL_H
#define GEANY_SYMBOL_H 1

#include "tm_tag.h"

G_BEGIN_DECLS

struct GeanySymbol;
typedef struct GeanySymbol GeanySymbol;

/* The GType for a GeanySymbol */
#define GEANY_TYPE_SYMBOL (geany_symbol_get_type())

GType geany_symbol_get_type(void) G_GNUC_CONST;
void geany_symbol_unref(GeanySymbol *sym);
GeanySymbol *geany_symbol_ref(GeanySymbol *sym);

GeanySymbol *geany_symbol_new(gchar *name, gchar *detail, gchar *scope, gchar *file,
	TMParserType lang, glong kind, gulong line, gulong pos, guint icon);

GeanySymbol *geany_symbol_new_from_tag(TMTag *tag);

#ifdef GEANY_PRIVATE

gulong geany_symbol_get_line(const GeanySymbol *sym);
glong geany_symbol_get_kind(const GeanySymbol *sym);
const gchar *geany_symbol_get_scope(const GeanySymbol *sym);
const gchar *geany_symbol_get_name(const GeanySymbol *sym);
const gchar *geany_symbol_get_detail(const GeanySymbol *sym);

gchar *geany_symbol_get_name_with_scope(const GeanySymbol *sym);

gchar *geany_symbol_get_symtree_name(const GeanySymbol *sym, const gchar *encoding, gboolean include_scope);
gchar *geany_symbol_get_symtree_tooltip(const GeanySymbol *sym, const gchar *encoding);
gint geany_symbol_get_symtree_group_index(const GeanySymbol *sym);

gchar *geany_symbol_get_goto_name(const GeanySymbol *sym, const gchar *encoding);
gchar *geany_symbol_get_goto_tooltip(const GeanySymbol *sym, const gchar *encoding);

gboolean geany_symbol_equal(const GeanySymbol *a, const GeanySymbol *b);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_SYMBOL_H */
