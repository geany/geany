/*
 *      pluginextension.c - this file is part of Geany, a fast and lightweight IDE
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

#include "pluginextension.h"


static gboolean func_return_false(GeanyDocument *doc)
{
	return FALSE;
}


static GPtrArray *func_return_ptrarr(GeanyDocument *doc)
{
	return NULL;
}


static void func_args_doc(GeanyDocument *doc)
{
}


static void func_args_doc_bool(GeanyDocument *doc, gboolean dummy1)
{
}


static void func_args_doc_int_bool(GeanyDocument *doc, gint dummy1, gboolean dummy2)
{
}


static PluginExtension dummy_extension = {
	.autocomplete_provided = func_return_false,
	.autocomplete_perform = func_args_doc,

	.calltips_provided = func_return_false,
	.calltips_show = func_args_doc_bool,

	.goto_provided = func_return_false,
	.goto_perform = func_args_doc_int_bool,

	.doc_symbols_provided = func_return_false,
	.doc_symbols_get = func_return_ptrarr,

	.symbol_highlight_provided = func_return_false
};

static PluginExtension *current_extension = &dummy_extension;


GEANY_API_SYMBOL
void plugin_extension_register(PluginExtension *extension)
{
	/* possibly, in the future if there's a need for multiple extensions,
	 * have a list of extensions and add/remove to/from the list */
	current_extension = extension;
}


GEANY_API_SYMBOL
void plugin_extension_unregister(PluginExtension *extension)
{
	current_extension = &dummy_extension;
}


/* allow plugins not to implement all the functions and fall back to the dummy
 * implementation */
#define CALL_IF_EXISTS(f) (current_extension->f ? current_extension->f : dummy_extension.f)

gboolean plugin_extension_autocomplete_provided(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(autocomplete_provided)(doc);
}


void plugin_extension_autocomplete_perform(GeanyDocument *doc)
{
	CALL_IF_EXISTS(autocomplete_perform)(doc);
}


gboolean plugin_extension_calltips_provided(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(calltips_provided)(doc);
}


void plugin_extension_calltips_show(GeanyDocument *doc, gboolean force)
{
	CALL_IF_EXISTS(calltips_show)(doc, force);
}


gboolean plugin_extension_goto_provided(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(goto_provided)(doc);
}


void plugin_extension_goto_perform(GeanyDocument *doc, gint pos, gboolean definition)
{
	CALL_IF_EXISTS(goto_perform)(doc, pos, definition);
}


gboolean plugin_extension_doc_symbols_provided(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(doc_symbols_provided)(doc);
}


GPtrArray *plugin_extension_doc_symbols_get(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(doc_symbols_get)(doc);
}


gboolean plugin_extension_symbol_highlight_provided(GeanyDocument *doc)
{
	return CALL_IF_EXISTS(symbol_highlight_provided)(doc);
}
