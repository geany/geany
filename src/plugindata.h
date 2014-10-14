/*
 *      plugindata.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * @file plugindata.h
 * This file defines the plugin API, the interface between Geany and its plugins.
 * For detailed documentation of the plugin system please read the plugin
 * API documentation.
 **/
/* Note: Remember to increment GEANY_API_VERSION (and GEANY_ABI_VERSION if necessary)
 * when making changes (see 'Keeping the plugin ABI stable' in the HACKING file). */


#ifndef GEANY_PLUGIN_DATA_H
#define GEANY_PLUGIN_DATA_H 1

#include "build.h"  /* GeanyBuildGroup, GeanyBuildSource, GeanyBuildCmdEntries enums */
#include "document.h" /* GeanyDocument */
#include "editor.h"	/* GeanyEditor, GeanyIndentType */
#include "filetypes.h" /* GeanyFiletype */

#include "gtkcompat.h"

G_BEGIN_DECLS

/* Compatibility for sharing macros between API and core.
 * First include geany.h, then plugindata.h, then other API headers. */
#undef GEANY
#define GEANY(symbol_name) geany->symbol_name


/** The Application Programming Interface (API) version, incremented
 * whenever any plugin data types are modified or appended to.
 *
 * You can protect code that needs a higher API than e.g. 200 with:
 * @code #if GEANY_API_VERSION >= 200
 * 	some_newer_function();
 * #endif @endcode
 *
 * @warning You should not test for values below 200 as previously
 * @c GEANY_API_VERSION was defined as an enum value, not a macro.
 */
#define GEANY_API_VERSION 220

/* hack to have a different ABI when built with GTK3 because loading GTK2-linked plugins
 * with GTK3-linked Geany leads to crash */
#if GTK_CHECK_VERSION(3, 0, 0)
#	define GEANY_ABI_SHIFT 8
#else
#	define GEANY_ABI_SHIFT 0
#endif
/** The Application Binary Interface (ABI) version, incremented whenever
 * existing fields in the plugin data types have to be changed or reordered.
 * Changing this forces all plugins to be recompiled before Geany can load them. */
/* This should usually stay the same if fields are only appended, assuming only pointers to
 * structs and not structs themselves are declared by plugins. */
#define GEANY_ABI_VERSION (69 << GEANY_ABI_SHIFT)


/** Defines a function to check the plugin is safe to load.
 * This performs runtime checks that try to ensure:
 * - Geany ABI data types are compatible with this plugin.
 * - Geany sources provide the required API for this plugin.
 * @param api_required The minimum API number your plugin requires.
 * Look at the source for the value of @c GEANY_API_VERSION to use if you
 * want your plugin to require the current Geany version on your machine.
 * You should update this value when using any new API features. */
#define PLUGIN_VERSION_CHECK(api_required) \
	gint plugin_version_check(gint abi_ver) \
	{ \
		if (abi_ver != GEANY_ABI_VERSION) \
			return -1; \
		return (api_required); \
	}


/** Basic information about a plugin available to Geany without loading the plugin.
 * The fields are set in plugin_set_info(), usually with the PLUGIN_SET_INFO() macro. */
typedef struct PluginInfo
{
	/** The name of the plugin. */
	const gchar	*name;
	/** The description of the plugin. */
	const gchar	*description;
	/** The version of the plugin. */
	const gchar	*version;
	/** The author of the plugin. */
	const gchar	*author;
}
PluginInfo;


/** Basic information for the plugin and identification.
 * @see geany_plugin. */
typedef struct GeanyPlugin
{
	PluginInfo	*info;	/**< Fields set in plugin_set_info(). */

	struct GeanyPluginPrivate *priv;	/* private */
}
GeanyPlugin;


/** Sets the plugin name and some other basic information about a plugin.
 *
 * @note If you want some of the arguments to be translated, see @ref PLUGIN_SET_TRANSLATABLE_INFO()
 *
 * Example:
 * @code PLUGIN_SET_INFO("Cool Feature", "Adds cool feature support.", "0.1", "Joe Author") @endcode */
/* plugin_set_info() could be written manually for plugins if we want to add any
 * extra PluginInfo features (such as an icon), so we don't need to break API
 * compatibility. Alternatively just add a new macro, PLUGIN_SET_INFO_FULL(). -ntrel */
#define PLUGIN_SET_INFO(p_name, p_description, p_version, p_author) \
	void plugin_set_info(PluginInfo *info) \
	{ \
		info->name = (p_name); \
		info->description = (p_description); \
		info->version = (p_version); \
		info->author = (p_author); \
	}

/** Sets the plugin name and some other basic information about a plugin.
 * This macro is like @ref PLUGIN_SET_INFO() but allows the passed information to be translated
 * by setting up the translation mechanism with @ref main_locale_init().
 * You therefore don't need to call it manually in plugin_init().
 *
 * Example:
 * @code PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, _("Cool Feature"), _("Adds a cool feature."), "0.1", "John Doe") @endcode
 *
 * @since 0.19 */
#define PLUGIN_SET_TRANSLATABLE_INFO(localedir, package, p_name, p_description, p_version, p_author) \
	void plugin_set_info(PluginInfo *info) \
	{ \
		main_locale_init((localedir), (package)); \
		info->name = (p_name); \
		info->description = (p_description); \
		info->version = (p_version); \
		info->author = (p_author); \
	}


#ifndef GEANY_PRIVATE

/* Prototypes for building plugins with -Wmissing-prototypes */
gint plugin_version_check(gint abi_ver);
void plugin_set_info(PluginInfo *info);

#endif


/** @deprecated - use plugin_set_key_group() instead.
 * @see PLUGIN_KEY_GROUP() macro. */
typedef struct GeanyKeyGroupInfo
{
	const gchar *name;		/**< Group name used in the configuration file, such as @c "html_chars" */
	gsize count;			/**< The number of keybindings the group will hold */
}
GeanyKeyGroupInfo;

/** @deprecated - use plugin_set_key_group() instead.
 * Declare and initialise a keybinding group.
 * @code GeanyKeyGroup *plugin_key_group; @endcode
 * You must then set the @c plugin_key_group::keys[] entries for the group in plugin_init(),
 * normally using keybindings_set_item().
 * The @c plugin_key_group::label field is set by Geany after @c plugin_init()
 * is called, to the name of the plugin.
 * @param group_name A unique group name (without quotes) to be used in the
 * configuration file, such as @c html_chars.
 * @param key_count	The number of keybindings the group will hold. */
#define PLUGIN_KEY_GROUP(group_name, key_count) \
	/* We have to declare this as a single element array.
	 * Declaring as a pointer to a struct doesn't work with g_module_symbol(). */ \
	GeanyKeyGroupInfo plugin_key_group_info[1] = \
	{ \
		{G_STRINGIFY(group_name), key_count} \
	};\
	GeanyKeyGroup *plugin_key_group = NULL;


/** Callback array entry type used with the @ref plugin_callbacks symbol. */
typedef struct PluginCallback
{
	/** The name of signal, must be an existing signal. For a list of available signals,
	 *  please see the @link pluginsignals.c Signal documentation @endlink. */
	const gchar	*signal_name;
	/** A callback function which is called when the signal is emitted. */
	GCallback	callback;
	/** Set to TRUE to connect your handler with g_signal_connect_after(). */
	gboolean	after;
	/** The user data passed to the signal handler. */
	gpointer	user_data;
}
PluginCallback;


/** @deprecated Use @ref ui_add_document_sensitive() instead.
 * Flags to be set by plugins in PluginFields struct. */
typedef enum
{
	/** Whether a plugin's menu item should be disabled when there are no open documents */
	PLUGIN_IS_DOCUMENT_SENSITIVE	= 1 << 0
}
PluginFlags;

/** @deprecated Use @ref ui_add_document_sensitive() instead.
 * Fields set and owned by the plugin. */
typedef struct PluginFields
{
	/** Bitmask of @c PluginFlags. */
	PluginFlags	flags;
	/** Pointer to a plugin's menu item which will be automatically enabled/disabled when there
	 *  are no open documents and @c PLUGIN_IS_DOCUMENT_SENSITIVE is set.
	 *  This is required if using @c PLUGIN_IS_DOCUMENT_SENSITIVE, ignored otherwise */
	GtkWidget	*menu_item;
}
PluginFields;


/** This contains pointers to global variables owned by Geany for plugins to use.
 * Core variable pointers can be appended when needed by plugin authors, if appropriate. */
typedef struct GeanyData
{
	struct GeanyApp				*app;				/**< Geany application data fields */
	struct GeanyMainWidgets		*main_widgets;		/**< Important widgets in the main window */
	GPtrArray					*documents_array;	/**< See document.h#documents_array. */
	GPtrArray					*filetypes_array;	/**< Dynamic array of GeanyFiletype pointers */
	struct GeanyPrefs			*prefs;				/**< General settings */
	struct GeanyInterfacePrefs	*interface_prefs;	/**< Interface settings */
	struct GeanyToolbarPrefs	*toolbar_prefs;		/**< Toolbar settings */
	struct GeanyEditorPrefs		*editor_prefs;		/**< Editor settings */
	struct GeanyFilePrefs		*file_prefs;		/**< File-related settings */
	struct GeanySearchPrefs		*search_prefs;		/**< Search-related settings */
	struct GeanyToolPrefs		*tool_prefs;		/**< Tool settings */
	struct GeanyTemplatePrefs	*template_prefs;	/**< Template settings */
	struct GeanyBuildInfo		*build_info;		/**< Current build information */
	GSList						*filetypes_by_title; /**< See filetypes.h#filetypes_by_title. */
}
GeanyData;

#define geany			geany_data	/**< Simple macro for @c geany_data that reduces typing. */


/* Deprecated aliases */
#ifndef GEANY_DISABLE_DEPRECATED

#define document_reload_file document_reload_force

/** @deprecated - copy into your plugin code if needed.
 * @c NULL-safe way to get the index of @a doc_ptr in the documents array. */
#define DOC_IDX(doc_ptr) \
	(doc_ptr ? doc_ptr->index : -1)
#define DOC_IDX_VALID(doc_idx) \
	((doc_idx) >= 0 && (guint)(doc_idx) < documents_array->len && documents[doc_idx]->is_valid)

#define GEANY_WINDOW_MINIMAL_WIDTH		550
#define GEANY_WINDOW_MINIMAL_HEIGHT		GEANY_DEFAULT_DIALOG_HEIGHT

#endif	/* GEANY_DISABLE_DEPRECATED */

G_END_DECLS

#endif /* GEANY_PLUGIN_DATA_H */
