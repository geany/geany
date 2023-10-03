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


gboolean func_return_false(GeanyDocument *doc)
{
	return FALSE;
}


void func_args_doc(GeanyDocument *doc)
{
}


void func_args_doc_bool(GeanyDocument *doc, gboolean dummy)
{
}


static Lsp dummy_lsp = {
	.autocomplete_available = func_return_false,
	.autocomplete_perform = func_args_doc,

	.calltips_available = func_return_false,
	.calltips_show = func_args_doc,

	.goto_available = func_return_false,
	.goto_perform = func_args_doc_bool,
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
