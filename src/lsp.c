/*
 *      lsp.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2023 The Geany contributors
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

#include "lsp.h"


typedef enum {
	LspKindFile = 1,
	LspKindModule,
	LspKindNamespace,
	LspKindPackage,
	LspKindClass,
	LspKindMethod,
	LspKindProperty,
	LspKindField,
	LspKindConstructor,
	LspKindEnum,
	LspKindInterface,
	LspKindFunction,
	LspKindVariable,
	LspKindConstant,
	LspKindString,
	LspKindNumber,
	LspKindBoolean,
	LspKindArray,
	LspKindObject,
	LspKindKey,
	LspKindNull,
	LspKindEnumMember,
	LspKindStruct,
	LspKindEvent,
	LspKindOperator,
	LspKindTypeParameter,
	LSP_KIND_NUM = LspKindTypeParameter
} LspSymbolKind;  /* note: enums different than in LspCompletionItemKind */


static LspSymbolKind kind_icons[LSP_KIND_NUM] = {
	TM_ICON_NAMESPACE,  /* LspKindFile */
	TM_ICON_NAMESPACE,  /* LspKindModule */
	TM_ICON_NAMESPACE,  /* LspKindNamespace */
	TM_ICON_NAMESPACE,  /* LspKindPackage */
	TM_ICON_CLASS,      /* LspKindClass */
	TM_ICON_METHOD,     /* LspKindMethod */
	TM_ICON_MEMBER,     /* LspKindProperty */
	TM_ICON_MEMBER,     /* LspKindField */
	TM_ICON_METHOD,     /* LspKindConstructor */
	TM_ICON_STRUCT,     /* LspKindEnum */
	TM_ICON_CLASS,      /* LspKindInterface */
	TM_ICON_METHOD,     /* LspKindFunction */
	TM_ICON_VAR,        /* LspKindVariable */
	TM_ICON_MACRO,      /* LspKindConstant */
	TM_ICON_OTHER,      /* LspKindString */
	TM_ICON_OTHER,      /* LspKindNumber */
	TM_ICON_OTHER,      /* LspKindBoolean */
	TM_ICON_OTHER,      /* LspKindArray */
	TM_ICON_OTHER,      /* LspKindObject */
	TM_ICON_OTHER,      /* LspKindKey */
	TM_ICON_OTHER,      /* LspKindNull */
	TM_ICON_MEMBER,     /* LspKindEnumMember */
	TM_ICON_STRUCT,     /* LspKindStruct */
	TM_ICON_OTHER,      /* LspKindEvent */
	TM_ICON_METHOD,     /* LspKindOperator */
	TM_ICON_OTHER       /* LspKindTypeParameter */
};


gboolean func_return_false(GeanyDocument *doc)
{
	return FALSE;
}


GPtrArray *func_return_ptrarr(GeanyDocument *doc)
{
	return NULL;
}


void func_args_doc(GeanyDocument *doc)
{
}


void func_args_doc_bool(GeanyDocument *doc, gboolean dummy)
{
}


void func_args_doc_symcallback_ptr(GeanyDocument *doc, LspSymbolRequestCallback callback, gpointer user_data)
{
}


static Lsp dummy_lsp = {
	.autocomplete_available = func_return_false,
	.autocomplete_perform = func_args_doc,

	.calltips_available = func_return_false,
	.calltips_show = func_args_doc,

	.goto_available = func_return_false,
	.goto_perform = func_args_doc_bool,

	.doc_symbols_available = func_return_false,
	.doc_symbols_request = func_args_doc_symcallback_ptr,
	.doc_symbols_get_cached = func_return_ptrarr
};

static Lsp *current_lsp = &dummy_lsp;


GEANY_API_SYMBOL
void lsp_register(Lsp *lsp)
{
	/* possibly, in the future if there's a need for multiple LSP plugins,
	 * have a list of LSP clients and add/remove to/from the list */
	current_lsp = lsp;
}


GEANY_API_SYMBOL
void lsp_unregister(Lsp *lsp)
{
	current_lsp = &dummy_lsp;
}


guint lsp_get_symbols_icon_id(guint kind)
{
	if (kind >= LspKindFile && kind <= LSP_KIND_NUM)
		return kind_icons[kind - 1];
	return TM_ICON_STRUCT;
}


/* allow LSP plugins not to implement all the functions (might happen if we
 * add more in the future) and fall back to the dummy implementation */
#define CALL_IF_EXISTS(f) (current_lsp->f ? current_lsp->f : dummy_lsp.f)

gboolean lsp_autocomplete_available(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(autocomplete_available)(doc);
}


void lsp_autocomplete_perform(GeanyDocument *doc)
{
	CALL_IF_EXISTS(autocomplete_perform)(doc);
}


gboolean lsp_calltips_available(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(calltips_available)(doc);
}


void lsp_calltips_show(GeanyDocument *doc)
{
	CALL_IF_EXISTS(calltips_show)(doc);
}


gboolean lsp_goto_available(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(goto_available)(doc);
}


void lsp_goto_perform(GeanyDocument *doc, gboolean definition)
{
	CALL_IF_EXISTS(goto_perform)(doc, definition);
}


gboolean lsp_doc_symbols_available(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(doc_symbols_available)(doc);
}


void lsp_doc_symbols_request(GeanyDocument *doc, LspSymbolRequestCallback callback, gpointer user_data)
{
	CALL_IF_EXISTS(doc_symbols_request)(doc, callback, user_data);
}


GPtrArray *lsp_doc_symbols_get_cached(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(doc_symbols_get_cached)(doc);
}
