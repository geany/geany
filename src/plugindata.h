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

#include "geany.h"  /* for GEANY_DEPRECATED */
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
#define GEANY_API_VERSION 235

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
#define GEANY_ABI_VERSION (71 << GEANY_ABI_SHIFT)


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
	/** The user data passed to the signal handler. If set to NULL then the signal
	 * handler will receive the data set with geany_plugin_register_full() or
	 * geany_plugin_set_data() */
	gpointer	user_data;
}
PluginCallback;


/** This contains pointers to global variables owned by Geany for plugins to use.
 * Core variable pointers can be appended when needed by plugin authors, if appropriate. */
typedef struct GeanyData
{
	struct GeanyApp				*app;				/**< Geany application data fields */
	struct GeanyMainWidgets		*main_widgets;		/**< Important widgets in the main window */
	/** Dynamic array of GeanyDocument pointers.
	 * Once a pointer is added to this, it is never freed. This means the same document pointer
	 * can represent a different document later on, or it may have been closed and become invalid.
	 * For this reason, you should use document_find_by_id() instead of storing
	 * document pointers over time if there is a chance the user can close the
	 * document.
	 *
	 * @warning You must check @c GeanyDocument::is_valid when iterating over this array.
	 * This is done automatically if you use the foreach_document() macro.
	 *
	 * @note
	 * Never assume that the order of document pointers is the same as the order of notebook tabs.
	 * One reason is that notebook tabs can be reordered.
	 * Use @c document_get_from_page() to lookup a document from a notebook tab number.
	 *
	 * @see documents.
	 *
	 * @elementtype{GeanyDocument}
	 */
	GPtrArray					*documents_array;
	/** Dynamic array of filetype pointers
	 *
	 * List the list is dynamically expanded for custom filetypes filetypes so don't expect
	 * the list of known filetypes to be a constant.
	 *
	 * @elementtype{GeanyFiletype}
	 */
	GPtrArray					*filetypes_array;
	struct GeanyPrefs			*prefs;				/**< General settings */
	struct GeanyInterfacePrefs	*interface_prefs;	/**< Interface settings */
	struct GeanyToolbarPrefs	*toolbar_prefs;		/**< Toolbar settings */
	struct GeanyEditorPrefs		*editor_prefs;		/**< Editor settings */
	struct GeanyFilePrefs		*file_prefs;		/**< File-related settings */
	struct GeanySearchPrefs		*search_prefs;		/**< Search-related settings */
	struct GeanyToolPrefs		*tool_prefs;		/**< Tool settings */
	struct GeanyTemplatePrefs	*template_prefs;	/**< Template settings */
	gpointer					*_compat;			/* Remove field on next ABI break (abi-todo) */
	/** List of filetype pointers sorted by name, but with @c filetypes_index(GEANY_FILETYPES_NONE)
	 * first, as this is usually treated specially.
	 * The list does not change (after filetypes have been initialized), so you can use
	 * @code g_slist_nth_data(filetypes_by_title, n) @endcode and expect the same result at different times.
	 * @see filetypes_get_sorted_by_name().
	 *
	 * @elementtype{GeanyFiletype}
	 */
	GSList						*filetypes_by_title;
	/** @gironly
	 * Global object signalling events via signals
	 *
	 * This is mostly for language bindings. Otherwise prefer to use
	 * plugin_signal_connect() instead this which adds automatic cleanup. */
	GObject						*object;
}
GeanyData;

#define geany			geany_data	/**< Simple macro for @c geany_data that reduces typing. */

typedef struct GeanyPluginFuncs GeanyPluginFuncs;
typedef struct GeanyProxyFuncs GeanyProxyFuncs;

/** Basic information for the plugin and identification.
 * @see geany_plugin. */
typedef struct GeanyPlugin
{
	PluginInfo	*info;	/**< Fields set in plugin_set_info(). */
	GeanyData	*geany_data;	/**< Pointer to global GeanyData intance */
	GeanyPluginFuncs *funcs;	/**< Functions implemented by the plugin, set in geany_load_module() */
	GeanyProxyFuncs	*proxy_funcs; /**< Hooks implemented by the plugin if it wants to act as a proxy
									   Must be set prior to calling geany_plugin_register_proxy() */
	struct GeanyPluginPrivate *priv;	/* private */
}
GeanyPlugin;

#ifndef GEANY_PRIVATE

/* Prototypes for building plugins with -Wmissing-prototypes
 * Also allows the compiler to check if the signature of the plugin's
 * symbol properly matches what we expect. */
gint plugin_version_check(gint abi_ver);
void plugin_set_info(PluginInfo *info);

void plugin_init(GeanyData *data);
GtkWidget *plugin_configure(GtkDialog *dialog);
void plugin_configure_single(GtkWidget *parent);
void plugin_help(void);
void plugin_cleanup(void);

/** Called by Geany when a plugin library is loaded.
 *
 * This is the original entry point. Implement and export this function to be loadable at all.
 * Then fill in GeanyPlugin::info and GeanyPlugin::funcs of the passed @p plugin. Finally
 * GEANY_PLUGIN_REGISTER() and specify a minimum supported API version.
 *
 * For all glory details please read @ref howto.
 *
 * Because the plugin is not yet enabled by the user you may not call plugin API functions inside
 * this function, except for the API functions below which are required for proper registration.
 *
 * API functions which are allowed to be called within this function:
 *  - main_locale_init()
 *  - geany_plugin_register() (and GEANY_PLUGIN_REGISTER())
 *  - geany_plugin_register_full() (and GEANY_PLUGIN_REGISTER_FULL())
 *  - plugin_module_make_resident()
 *
 * @param plugin The unique plugin handle to your plugin. You must set some fields here.
 *
 * @since 1.26 (API 225)
 * @see @ref howto
 */
void geany_load_module(GeanyPlugin *plugin);

#endif

/** Callback functions that need to be implemented for every plugin.
 *
 * These callbacks should be registered by the plugin within Geany's call to
 * geany_load_module() by calling geany_plugin_register() with an instance of this type.
 *
 * Geany will then call the callbacks at appropriate times. Each gets passed the
 * plugin-defined data pointer as well as the corresponding GeanyPlugin instance
 * pointer.
 *
 * @since 1.26 (API 225)
 * @see @ref howto
 **/
struct GeanyPluginFuncs
{
	/** Array of plugin-provided signal handlers @see PluginCallback */
	PluginCallback *callbacks;
	/** Called to initialize the plugin, when the user activates it (must not be @c NULL) */
	gboolean    (*init)      (GeanyPlugin *plugin, gpointer pdata);
	/** plugins configure dialog, optional (can be @c NULL) */
	GtkWidget*  (*configure) (GeanyPlugin *plugin, GtkDialog *dialog, gpointer pdata);
	/** Called when the plugin should show some help, optional (can be @c NULL) */
	void        (*help)      (GeanyPlugin *plugin, gpointer pdata);
	/** Called when the plugin is disabled or when Geany exits (must not be @c NULL) */
	void        (*cleanup)   (GeanyPlugin *plugin, gpointer pdata);
};

gboolean geany_plugin_register(GeanyPlugin *plugin, gint api_version,
                               gint min_api_version, gint abi_version);
gboolean geany_plugin_register_full(GeanyPlugin *plugin, gint api_version,
                                    gint min_api_version, gint abi_version,
                                    gpointer data, GDestroyNotify free_func);
gpointer geany_plugin_get_data(const GeanyPlugin *plugin);
void geany_plugin_set_data(GeanyPlugin *plugin, gpointer data, GDestroyNotify free_func);

/** Convenience macro to register a plugin.
 *
 * It simply calls geany_plugin_register() with GEANY_API_VERSION and GEANY_ABI_VERSION.
 *
 * @since 1.26 (API 225)
 * @see @ref howto
 **/
#define GEANY_PLUGIN_REGISTER(plugin, min_api_version) \
	geany_plugin_register((plugin), GEANY_API_VERSION, \
	                      (min_api_version), GEANY_ABI_VERSION)

/** Convenience macro to register a plugin with data.
 *
 * It simply calls geany_plugin_register_full() with GEANY_API_VERSION and GEANY_ABI_VERSION.
 *
 * @since 1.26 (API 225)
 * @see @ref howto
 **/
#define GEANY_PLUGIN_REGISTER_FULL(plugin, min_api_version, pdata, free_func) \
	geany_plugin_register_full((plugin), GEANY_API_VERSION, \
	                           (min_api_version), GEANY_ABI_VERSION, (pdata), (free_func))

/** Return values for GeanyProxyHooks::probe()
 *
 * @see geany_plugin_register_proxy() for a full description of the proxy plugin mechanisms.
 */
typedef enum
{
	/** The proxy is not responsible at all, and Geany or other plugins are free
	 * to probe it.
	 *
	 * @since 1.29 (API 229)
	 **/
	GEANY_PROXY_IGNORE,
	/** The proxy is responsible for this file, and creates a plugin for it.
	 *
	 * @since 1.29 (API 229)
	 */
	GEANY_PROXY_MATCH,
	/** The proxy is does not directly load it, but it's still tied to the proxy.
	 *
	 * This is for plugins that come in multiple files where only one of these
	 * files is relevant for the plugin creation (for the PM dialog). The other
	 * files should be ignored by Geany and other proxies. Example: libpeas has
	 * a .plugin and a .so per plugin. Geany should not process the .so file
	 * if there is a corresponding .plugin.
	 *
	 * @since 1.29 (API 229)
	 */
	GEANY_PROXY_RELATED = GEANY_PROXY_MATCH | 0x100
}
GeanyProxyProbeResults;

/** @deprecated Use GEANY_PROXY_IGNORE instead.
 * @since 1.26 (API 226)
 */
#define PROXY_IGNORED GEANY_PROXY_IGNORE
/** @deprecated Use GEANY_PROXY_MATCH instead.
 * @since 1.26 (API 226)
 */
#define PROXY_MATCHED GEANY_PROXY_MATCH
/** @deprecated Use GEANY_PROXY_RELATED instead.
 * @since 1.26 (API 226)
 */
#define PROXY_NOLOAD 0x100

/** Hooks that need to be implemented by every proxy
 *
 * @see geany_plugin_register_proxy() for a full description of the proxy mechanism.
 *
 * @since 1.26 (API 226)
 **/
struct GeanyProxyFuncs
{
	/** Called to determine whether the proxy is truly responsible for the requested plugin.
	 * A NULL pointer assumes the probe() function would always return @ref GEANY_PROXY_MATCH */
	gint		(*probe)     (GeanyPlugin *proxy, const gchar *filename, gpointer pdata);
	/** Called after probe(), to perform the actual job of loading the plugin */
	gpointer	(*load)      (GeanyPlugin *proxy, GeanyPlugin *subplugin, const gchar *filename, gpointer pdata);
	/** Called when the user initiates unloading of a plugin, e.g. on Geany exit */
	void		(*unload)    (GeanyPlugin *proxy, GeanyPlugin *subplugin, gpointer load_data, gpointer pdata);
};

gint geany_plugin_register_proxy(GeanyPlugin *plugin, const gchar **extensions);

/* Deprecated aliases */
#ifndef GEANY_DISABLE_DEPRECATED

/* This remains so that older plugins that contain a `GeanyFunctions *geany_functions;`
 * variable in their plugin - as was previously required - will still compile
 * without changes.  */
typedef struct GeanyFunctionsUndefined GeanyFunctions GEANY_DEPRECATED;

/** @deprecated - use plugin_set_key_group() instead.
 * @see PLUGIN_KEY_GROUP() macro. */
typedef struct GeanyKeyGroupInfo
{
	const gchar *name;		/**< Group name used in the configuration file, such as @c "html_chars" */
	gsize count;			/**< The number of keybindings the group will hold */
}
GeanyKeyGroupInfo GEANY_DEPRECATED_FOR(plugin_set_key_group);

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
PluginFields GEANY_DEPRECATED_FOR(ui_add_document_sensitive);

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
