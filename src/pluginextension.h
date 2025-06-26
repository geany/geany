/*
 *      pluginextension.h - this file is part of Geany, a fast and lightweight IDE
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

/**
 *  @file pluginextension.h
 *  This file defines an interface allowing plugins to take over some of the
 *  core functionality provided by Geany: autocompletion, calltips,
 *  symbol goto, and types highlighting inside document.
 **/

#ifndef GEANY_PLUGIN_EXTENSION_H
#define GEANY_PLUGIN_EXTENSION_H 1

#include "document.h"


G_BEGIN_DECLS

/**
 * Structure serving as an interface between plugins and Geany allowing
 * plugins to inform Geany about what features they provide and allowing
 * Geany to delegate its functionality to the plugins.
 * 
 * Depending on the functionality they provide, plugins should assign pointers
 * to the functions implementing this interface to the appropriate members of
 * the structure. Not all of the functions have to be implemented by the
 * plugin - member pointers of unimplemented functions can be left to contain
 * the @c NULL pointer.
 * 
 * Typically, functions from this interface come in pairs. Functions assigned to
 * the members ending with @c _provided inform Geany whether the plugin
 * implements the given feature for the passed document. Functions assigned
 * to the members ending with @c _perform are called by Geany at appropriate
 * moments to inform the plugin when to perform the given feature.
 * 
 * The extension is defined by the pointers in the PluginExtension structure and
 * is registered in Geany using the @c plugin_extension_register() function.
 * 
 * @warning The API provided by this file is subject to change and should not be
 * considered stable at this point. That said, it is highly probable that if
 * a change of this API is required in the future, it will not be of a major
 * nature and should not require major modifications of the affected plugins
 * (e.g. added/modified parameter of a function and similar changes).
 **/
typedef struct
{
	/**
	 * Pointer to function called by Geany to check whether the plugin
	 * implements autocompletion for the provided document.
	 *
	 * @param doc The document for which Geany is querying whether the plugin
	 * provides autocompletion. This allows plugins to restrict their
	 * autocompletion implementation to documents of specific filetypes or other
	 * characteristics.
	 * @param data User data passed during the @c plugin_extension_register()
	 * call.
	 *
	 * @return Plugins should return @c TRUE if they implement autocompletion
	 * for the provided document, @c FALSE otherwise.
	 *
	 * @since 2.1
	 **/
	gboolean (*autocomplete_provided)(GeanyDocument *doc, gpointer data);
	/**
	 * Pointer to function called by Geany to inform the plugin to perform
	 * autocompletion, e.g by showing an autocompletion popup window with
	 * suggestions.
	 *
	 * @param doc The document for which autocompletion is being performed.
	 * @param force @c TRUE when autocompletion was requested explicitly by the
	 * user, e.g. by pressing a keybinding requesting the autocompletion popup
	 * to appear. @c FALSE means autocompletion is being auto-performed as the
	 * user types, possibly allowing the plugin not to do anything if there are
	 * no meaningful suggestions.
	 * @param data User data passed during the @c plugin_extension_register()
	 * call.
	 *
	 * @since 2.1
	 **/
	void (*autocomplete_perform)(GeanyDocument *doc, gboolean force, gpointer data);

	/**
	 * Pointer to function called by Geany to check whether the plugin
	 * implements calltips containing function signatures for the provided
	 * document.
	 *
	 * @see @c autocomplete_provided() for more details.
	 *
	 * @since 2.1
	 **/
	gboolean (*calltips_provided)(GeanyDocument *doc, gpointer data);
	/**
	 * Pointer to function called by Geany to inform the plugin to show calltips.
	 *
	 * @see @c autocomplete_perform() for more details.
	 *
	 * @since 2.1
	 **/
	void (*calltips_show)(GeanyDocument *doc, gboolean force, gpointer data);

	/**
	 * Pointer to function called by Geany to check whether the plugin implements
	 * symbol definition/declaration goto functionality.
	 *
	 * @see @c autocomplete_provided() for more details.
	 *
	 * @since 2.1
	 **/
	gboolean (*goto_provided)(GeanyDocument *doc, gpointer data);
	/**
	 * Pointer to function called by Geany to inform the plugin to perform
	 * symbol goto.
	 *
	 * @param doc The document for which symbol goto is being performed.
	 * @param pos Scintilla position in the document at which the goto request
	 * was invoked (typically corresponds to the caret position or the position
	 * where the corresponding mouse event happened).
	 * @param definition If @c TRUE, this is the go to definition request, if
	 * @c FALSE, this is the goto declaration request.
	 * @param data User data passed during the @c plugin_extension_register()
	 * call.
	 *
	 * @return Plugins should return @c TRUE if the goto was performed, @c FALSE
	 * otherwise. Plugins that work asynchronously and do not know the result
	 * at the moment this function is called should return @c TRUE. However, such
	 * plugins can perform some synchronous pre-check such as whether there is
	 * an identifier at the caret position and if the necessary conditions are
	 * not satisfied and the plugins know that goto cannot be performed, they
	 * should return @c FALSE.
	 *
	 * @since 2.1
	 **/
	gboolean (*goto_perform)(GeanyDocument *doc, gint pos, gboolean definition, gpointer data);

	/**
	 * Pointer to function called by Geany to check whether the plugin implements
	 * additional symbol (e.g. type) highlighting in Scintilla.
	 *
	 * @see @c autocomplete_provided() for more details.
	 * @note There is no function in the @c PluginExtension structure informing
	 * plugins to perform symbol highlighting. Plugins
	 * implementing symbol highlighting should perform it at the appropriate
	 * moments based on Scintilla and Geany events such as when the document
	 * becomes visible or when the document is modified.
	 *
	 * @since 2.1
	 **/
	gboolean (*symbol_highlight_provided)(GeanyDocument *doc, gpointer data);

	/* Padding serving for possible future extensions of this API. */
	void (*_dummy[100])(void);
} PluginExtension;


void plugin_extension_register(PluginExtension *extension, const gchar *ext_name,
	gint priority, gpointer data);
void plugin_extension_unregister(PluginExtension *extension);

gboolean plugin_extension_autocomplete_provided(GeanyDocument *doc, PluginExtension *ext);
gboolean plugin_extension_calltips_provided(GeanyDocument *doc, PluginExtension *ext);
gboolean plugin_extension_goto_provided(GeanyDocument *doc, PluginExtension *ext);
gboolean plugin_extension_symbol_highlight_provided(GeanyDocument *doc, PluginExtension *ext);

#ifdef GEANY_PRIVATE

void plugin_extension_autocomplete_perform(GeanyDocument *doc, gboolean force);
void plugin_extension_calltips_show(GeanyDocument *doc, gboolean force);
gboolean plugin_extension_goto_perform(GeanyDocument *doc, gint pos, gboolean definition);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_PLUGIN_EXTENSION_H */
