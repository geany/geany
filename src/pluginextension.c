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


GEANY_API_SYMBOL
gboolean plugin_extension_autocomplete_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(autocomplete_provided, doc, ext);
}


void plugin_extension_autocomplete_perform(GeanyDocument *doc, gboolean force)
{
	CALL_PERFORM(autocomplete_provided, doc, autocomplete_perform, (doc, force, entry->data), /* void */);
}


GEANY_API_SYMBOL
gboolean plugin_extension_calltips_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(calltips_provided, doc, ext);
}


void plugin_extension_calltips_show(GeanyDocument *doc, gboolean force)
{
	CALL_PERFORM(calltips_provided, doc, calltips_show, (doc, force, entry->data), /* void */);
}


GEANY_API_SYMBOL
gboolean plugin_extension_goto_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(goto_provided, doc, ext);
}


gboolean plugin_extension_goto_perform(GeanyDocument *doc, gint pos, gboolean definition)
{
	CALL_PERFORM(goto_provided, doc, goto_perform, (doc, pos, definition, entry->data), FALSE);
}


GEANY_API_SYMBOL
gboolean plugin_extension_symbol_highlight_provided(GeanyDocument *doc, PluginExtension *ext)
{
	CALL_PROVIDED(symbol_highlight_provided, doc, ext);
}
