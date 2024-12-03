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

#include "editor.h"
#include "main.h"


typedef struct
{
	PluginExtension *extension;
	gpointer data;
	gint priority;
} PluginExtensionEntry;

static GList *all_extensions = NULL;


/* sort higher priorities first */
static gint sort_extension_entries(gconstpointer a, gconstpointer b)
{
	const PluginExtensionEntry *entry_a = a;
	const PluginExtensionEntry *entry_b = b;

	return entry_b->priority - entry_a->priority;
}


/**
 * Registers the provided extension in Geany. There can be multiple extensions
 * registered in Geany - these are sorted by the priority
 * parameter. When executing functions assigned to the @c PluginExtension
 * members ending with @c _perform, Geany goes
 * through the registered extensions and executes the @c _perform() function of
 * the first extension for which the function assigned to the corresponding
 * @c _provided member returns @c TRUE.
 * 
 * This function is typically called in the plugin's @c init() function.
 * 
 * Plugins wishing to re-register themselves, e.g. with a different priority,
 * should first unregister themselves using @c plugin_extension_unregister()
 * and call @c plugin_extension_register() afterwards.
 * 
 * @param extension A pointer to the @c PluginExtension structure to register.
 * This pointer and the @c PluginExtension it points to have to remain valid
 * until the extension is unregistered. All fields of the @c PluginExtension
 * structure have to be fully initialized either to the @c NULL pointer or to a
 * proper implementation. Usually, this structure is statically allocated which
 * automatically zero-initializes uninitialized members as appropriate.
 * @param ext_name Human-readable name of the extension that can appear in
 * the user interface. The string should be reasonably unique so extensions can
 * be distinguished from each other.
 * @param priority Extension priority. The recommended values are:
 *  - if the extension is the only thing the plugin provides, and if it targets
 *    a single filetype, use 400 <= priority < 500.
 *  - if the extension is the only thing the plugin provides, but it targets
 *    several filetypes, use  300 <= priority < 400.
 *  - if the plugin provides other features than the extension, but it only
 *    targets a single filetype, use  200 <= priority < 300.
 *  - if the plugin provides other features than the extension, and targets
 *    several filetypes, use 100 <= priority < 200.
 *  - a priority of 0 or less is reserved, and using it has unspecified behavior.
 * @param data User data passed to the functions from the @c PluginExtension
 * struct.
 * @warning Plugins are responsible for calling @c plugin_extension_unregister()
 * when they no longer provide the extension and when the plugin is unloaded.
 *
 * @see @c plugin_extension_unregister().
 *
 * @since 2.1
 **/
GEANY_API_SYMBOL
void plugin_extension_register(PluginExtension *extension, const gchar *ext_name,
	gint priority, gpointer data)
{
	PluginExtensionEntry *entry;

	g_return_if_fail(ext_name != NULL);

	entry = g_malloc(sizeof *entry);
	entry->extension = extension;
	entry->data = data;
	entry->priority = priority;

	all_extensions = g_list_insert_sorted(all_extensions, entry, sort_extension_entries);
}


/**
 * Plugins are responsible for calling this function when they no longer
 * provide the extension, at the latest in the plugin's @c cleanup() function.
 * 
 * @param extension The @c PluginExtension structure pointer to unregister, as
 * previously registered with @c plugin_extension_register().
 *
 * @see @c plugin_extension_register().
 *
 * @since 2.1
 **/
GEANY_API_SYMBOL
void plugin_extension_unregister(PluginExtension *extension)
{
	for (GList *node = all_extensions; node; node = node->next)
	{
		PluginExtensionEntry *entry = node->data;

		if (entry->extension == extension)
		{
			g_free(entry);
			all_extensions = g_list_delete_link(all_extensions, node);
			break;
		}
	}
}


/*
 * @brief Checks whether a feature is provided
 * @param f The virtual function name
 * @param doc The document to check the feature on
 * @param ext A @c PluginExtension, or @c NULL
 * @returns @c TRUE if the feature is provided, @c FALSE otherwise.  If @p ext
 *          is @c NULL, it check whether any extension provides the feature;
 *          if it is an extension, it check whether it's this extension that
 *          provides the feature (taking into account possible overrides).
 */
#define CALL_PROVIDED(f, doc, ext)												\
	G_STMT_START {																\
		if (main_status.quitting || main_status.closing_all ||					\
			main_status.opening_session_files)									\
			return FALSE;														\
		for (GList *node = all_extensions; node; node = node->next)				\
		{																		\
			PluginExtensionEntry *entry = node->data;							\
																				\
			if (entry->extension->f && entry->extension->f(doc, entry->data))	\
				return (ext) ? entry->extension == (ext) : TRUE;				\
																				\
			if ((ext) && entry->extension == (ext))								\
				return FALSE;													\
		}																		\
		return FALSE;															\
	} G_STMT_END

/*
 * @brief Calls the extension implementation for f_provided/f_perform
 * @param f_provided The name of the virtual function checking if the feature is provided
 * @param doc The document to check the feature on
 * @param f_perform The name of the virtual function implementing the feature
 * @param args Arguments for @p f_perform. This should include @c entry->data as the last argument
 * @param defret Return value if the feature is not implemented
 * @returns The return value of @p f_perform or @p defret
 */
#define CALL_PERFORM(f_provided, doc, f_perform, args, defret)					\
	G_STMT_START {																\
		if (main_status.quitting || main_status.closing_all ||					\
			main_status.opening_session_files)									\
			return defret;														\
		for (GList *node = all_extensions; node; node = node->next)				\
		{																		\
			PluginExtensionEntry *entry = node->data;							\
																				\
			if (entry->extension->f_provided &&									\
				entry->extension->f_provided(doc, entry->data))					\
			{																	\
				if (entry->extension->f_perform)								\
					return entry->extension->f_perform args;					\
				break;															\
			}																	\
		}																		\
		return defret;															\
	} G_STMT_END


/*
 * Geany itself can also pass NULL as the extension parameter to determine
 * whether there is any extension implementing autocompletion. Plugins should
 * not use this and rely on Geany's extension list and the priorities so
 * this feature is not documented in the plugin API.
 */
/**
 * Plugins can call this function to check whether, based on the extensions
 * registered and the provided extension priorities, the extension passed in
 * the @c ext parameter is used for autocompletion.
 * 
 * Plugins will typically call this function with their own @c PluginExtension
 * to check, if they get (or got) executed for autocompletion of the
 * provided document. This is useful for various auxiliary functions such as
 * cleanups after the function assigned to @c autocomplete_perform is completed
 * so plugins know they executed this function and do not have to store this
 * information by some other means.
 * 
 * @param doc Document for which the check is performed.
 * @param ext The extension for which the check is performed.
 * @return Returns @c TRUE if autocompletion provided by the passed extension
 * is used, @c FALSE otherwise.
 *
 * @since 2.1
 **/
GEANY_API_SYMBOL
gboolean plugin_extension_autocomplete_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(autocomplete_provided, doc, ext);
}


void plugin_extension_autocomplete_perform(GeanyDocument *doc, gboolean force)
{
	CALL_PERFORM(autocomplete_provided, doc, autocomplete_perform, (doc, force, entry->data), /* void */);
}


/**
 * Checks whether the provided extension is used for showing calltips.
 * 
 * @see @c plugin_extension_autocomplete_provided()
 *
 * @since 2.1
 **/
GEANY_API_SYMBOL
gboolean plugin_extension_calltips_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(calltips_provided, doc, ext);
}


void plugin_extension_calltips_show(GeanyDocument *doc, gboolean force)
{
	CALL_PERFORM(calltips_provided, doc, calltips_show, (doc, force, entry->data), /* void */);
}


/**
 * Checks whether the provided extension is used for going to symbol
 * definition/declaration.
 * 
 * @see @c plugin_extension_autocomplete_provided()
 *
 * @since 2.1
 **/
GEANY_API_SYMBOL
gboolean plugin_extension_goto_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(goto_provided, doc, ext);
}


gboolean plugin_extension_goto_perform(GeanyDocument *doc, gint pos, gboolean definition)
{
	CALL_PERFORM(goto_provided, doc, goto_perform, (doc, pos, definition, entry->data), FALSE);
}


/**
 * Checks whether the provided extension is used for highlighting symbols in
 * the document.
 * 
 * @see @c plugin_extension_autocomplete_provided()
 *
 * @since 2.1
 **/
GEANY_API_SYMBOL
gboolean plugin_extension_symbol_highlight_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(symbol_highlight_provided, doc, ext);
}
