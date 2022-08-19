/*
 *      plugins.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 The Geany contributors
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

/* Code to manage, load and unload plugins. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_PLUGINS

#include "plugins.h"

#include "app.h"
#include "dialogs.h"
#include "documentprivate.h"
#include "encodings.h"
#include "geanyobject.h"
#include "geanywraplabel.h"
#include "highlighting.h"
#include "keybindingsprivate.h"
#include "keyfile.h"
#include "main.h"
#include "msgwindow.h"
#include "navqueue.h"
#include "plugindata.h"
#include "pluginprivate.h"
#include "pluginutils.h"
#include "prefs.h"
#include "sciwrappers.h"
#include "stash.h"
#include "support.h"
#include "symbols.h"
#include "templates.h"
#include "toolbar.h"
#include "ui_utils.h"
#include "utils.h"
#include "win32.h"

#include <gtk/gtk.h>
#include <string.h>


typedef struct
{
	gchar *prefix;
	GeanyDocument *document;
}
ForEachDocData;


GList *active_plugin_list = NULL; /* list of only actually loaded plugins, always valid */


static gboolean want_plugins = FALSE;

/* list of all available, loadable plugins, only valid as long as the plugin manager dialog is
 * opened, afterwards it will be destroyed */
static GList *plugin_list = NULL;
static gchar **active_plugins_pref = NULL; 	/* list of plugin filenames to load at startup */
static GList *failed_plugins_list = NULL;	/* plugins the user wants active but can't be used */

static GtkWidget *menu_separator = NULL;

static gchar *get_plugin_path(void);
static void pm_show_dialog(GtkMenuItem *menuitem, gpointer user_data);

typedef struct {
	gchar		extension[8];
	Plugin		*plugin; /* &builtin_so_proxy_plugin for native plugins */
} PluginProxy;


static gpointer plugin_load_gmodule(GeanyPlugin *proxy, GeanyPlugin *plugin, const gchar *filename, gpointer pdata);
static void plugin_unload_gmodule(GeanyPlugin *proxy, GeanyPlugin *plugin, gpointer load_data, gpointer pdata);

static Plugin builtin_so_proxy_plugin = {
	.proxy_cbs = {
		.load = plugin_load_gmodule,
		.unload = plugin_unload_gmodule,
	},
	/* rest of Plugin can be NULL/0 */
};

static PluginProxy builtin_so_proxy = {
	.extension = G_MODULE_SUFFIX,
	.plugin = &builtin_so_proxy_plugin,
};

static GQueue active_proxies = G_QUEUE_INIT;

static void plugin_free(Plugin *plugin);

static GeanyData geany_data;

static void
geany_data_init(void)
{
	GeanyData gd = {
		app,
		&main_widgets,
		documents_array,
		filetypes_array,
		&prefs,
		&interface_prefs,
		&toolbar_prefs,
		&editor_prefs,
		&file_prefs,
		&search_prefs,
		&tool_prefs,
		&template_prefs,
		NULL, /* Remove field on next ABI break (abi-todo) */
		filetypes_by_title,
		geany_object,
	};

	geany_data = gd;
}


/* In order to have nested proxies work the count of dependent plugins must propagate up.
 * This prevents that any plugin in the tree is unloaded while a leaf plugin is active. */
static void proxied_count_inc(Plugin *proxy)
{
	do
	{
		proxy->proxied_count += 1;
		proxy = proxy->proxy;
	} while (proxy != NULL);
}


static void proxied_count_dec(Plugin *proxy)
{
	g_warn_if_fail(proxy->proxied_count > 0);

	do
	{
		proxy->proxied_count -= 1;
		proxy = proxy->proxy;
	} while (proxy != NULL);
}


/* Prevent the same plugin filename being loaded more than once.
 * Note: g_module_name always returns the .so name, even when Plugin::filename is a .la file. */
static gboolean
plugin_loaded(Plugin *plugin)
{
	gchar *basename_module, *basename_loaded;
	GList *item;

	basename_module = g_path_get_basename(plugin->filename);
	for (item = plugin_list; item != NULL; item = g_list_next(item))
	{
		basename_loaded = g_path_get_basename(((Plugin*)item->data)->filename);

		if (utils_str_equal(basename_module, basename_loaded))
		{
			g_free(basename_loaded);
			g_free(basename_module);
			return TRUE;
		}
		g_free(basename_loaded);
	}
	/* Look also through the list of active plugins. This prevents problems when we have the same
	 * plugin in libdir/geany/ AND in configdir/plugins/ and the one in libdir/geany/ is loaded
	 * as active plugin. The plugin manager list would only take the one in configdir/geany/ and
	 * the plugin manager would list both plugins. Additionally, unloading the active plugin
	 * would cause a crash. */
	for (item = active_plugin_list; item != NULL; item = g_list_next(item))
	{
		basename_loaded = g_path_get_basename(((Plugin*)item->data)->filename);

		if (utils_str_equal(basename_module, basename_loaded))
		{
			g_free(basename_loaded);
			g_free(basename_module);
			return TRUE;
		}
		g_free(basename_loaded);
	}
	g_free(basename_module);
	return FALSE;
}


static Plugin *find_active_plugin_by_name(const gchar *filename)
{
	GList *item;

	g_return_val_if_fail(filename, FALSE);

	for (item = active_plugin_list; item != NULL; item = g_list_next(item))
	{
		if (utils_str_equal(filename, ((Plugin*)item->data)->filename))
			return item->data;
	}

	return NULL;
}


/* Mimics plugin_version_check() of legacy plugins for use with plugin_check_version() below */
#define PLUGIN_VERSION_CODE(api, abi) ((abi) != GEANY_ABI_VERSION ? -1 : (api))

static gboolean
plugin_check_version(Plugin *plugin, int plugin_version_code)
{
	gboolean ret = TRUE;
	if (plugin_version_code < 0)
	{
		gchar *name = g_path_get_basename(plugin->filename);
		msgwin_status_add(_("The plugin \"%s\" is not binary compatible with this "
			"release of Geany - please recompile it."), name);
		geany_debug("Plugin \"%s\" is not binary compatible with this "
			"release of Geany - recompile it.", name);
		ret = FALSE;
		g_free(name);
	}
	else if (plugin_version_code > GEANY_API_VERSION)
	{
		gchar *name = g_path_get_basename(plugin->filename);
		geany_debug("Plugin \"%s\" requires a newer version of Geany (API >= v%d).",
			name, plugin_version_code);
		ret = FALSE;
		g_free(name);
	}

	return ret;
}


static void add_callbacks(Plugin *plugin, PluginCallback *callbacks)
{
	PluginCallback *cb;
	guint i, len = 0;

	while (TRUE)
	{
		cb = &callbacks[len];
		if (!cb->signal_name || !cb->callback)
			break;
		len++;
	}
	if (len == 0)
		return;

	for (i = 0; i < len; i++)
	{
		cb = &callbacks[i];

		/* Pass the callback data as default user_data if none was set by the plugin itself */
		plugin_signal_connect(&plugin->public, NULL, cb->signal_name, cb->after,
			cb->callback, cb->user_data ? cb->user_data : plugin->cb_data);
	}
}


static gint cmp_plugin_names(gconstpointer a, gconstpointer b)
{
	const Plugin *pa = a;
	const Plugin *pb = b;

	return strcmp(pa->info.name, pb->info.name);
}


/** Register a plugin to Geany.
 *
 * The plugin will show up in the plugin manager. The user can interact with
 * it based on the functions it provides and installed GUI elements.
 *
 * You must initialize the info and funcs fields of @ref GeanyPlugin
 * appropriately prior to calling this, otherwise registration will fail. For
 * info at least a valid name must be set (possibly localized). For funcs,
 * at least init() and cleanup() functions must be implemented and set.
 *
 * The return value must be checked. It may be FALSE if the plugin failed to register which can
 * mainly happen for two reasons (future Geany versions may add new failure conditions):
 *  - Not all mandatory fields of GeanyPlugin have been set.
 *  - The ABI or API versions reported by the plugin are incompatible with the running Geany.
 *
 * Do not call this directly. Use GEANY_PLUGIN_REGISTER() instead which automatically
 * handles @a api_version and @a abi_version.
 *
 * @param plugin The plugin provided by Geany
 * @param api_version The API version the plugin is compiled against (pass GEANY_API_VERSION)
 * @param min_api_version The minimum API version required by the plugin
 * @param abi_version The exact ABI version the plugin is compiled against (pass GEANY_ABI_VERSION)
 *
 * @return TRUE if the plugin was successfully registered. Otherwise FALSE.
 *
 * @since 1.26 (API 225)
 * @see GEANY_PLUGIN_REGISTER()
 **/
GEANY_API_SYMBOL
gboolean geany_plugin_register(GeanyPlugin *plugin, gint api_version, gint min_api_version,
                               gint abi_version)
{
	Plugin *p;
	GeanyPluginFuncs *cbs = plugin->funcs;

	g_return_val_if_fail(plugin != NULL, FALSE);

	p = plugin->priv;
	/* already registered successfully */
	g_return_val_if_fail(!PLUGIN_LOADED_OK(p), FALSE);

	/* Prevent registering incompatible plugins. */
	if (! plugin_check_version(p, PLUGIN_VERSION_CODE(api_version, abi_version)))
		return FALSE;

	/* Only init and cleanup callbacks are truly mandatory. */
	if (! cbs->init || ! cbs->cleanup)
	{
		gchar *name = g_path_get_basename(p->filename);
		geany_debug("Plugin '%s' has no %s function - ignoring plugin!", name,
		            cbs->init ? "cleanup" : "init");
		g_free(name);
	}
	else
	{
		/* Yes, name is checked again later on, however we want return FALSE here
		 * to signal the error back to the plugin (but we don't print the message twice) */
		if (! EMPTY(p->info.name))
			p->flags = LOADED_OK;
	}

	/* If it ever becomes necessary we can save the api version in Plugin
	 * and apply compat code on a per-plugin basis, because we learn about
	 * the requested API version here. For now it's not necessary. */

	return PLUGIN_LOADED_OK(p);
}


/** Register a plugin to Geany, with plugin-defined data.
 *
 * This is a variant of geany_plugin_register() that also allows to set the plugin-defined data.
 * Refer to that function for more details on registering in general.
 *
 * @p pdata is the pointer going to be passed to the individual plugin callbacks
 * of GeanyPlugin::funcs. When the plugin module is unloaded, @p free_func is invoked on
 * @p pdata, which connects the data to the plugin's module life time.
 *
 * You cannot use geany_plugin_set_data() after registering with this function. Use
 * geany_plugin_register() if you need to.
 *
 * Do not call this directly. Use GEANY_PLUGIN_REGISTER_FULL() instead which automatically
 * handles @p api_version and @p abi_version.
 *
 * @param plugin The plugin provided by Geany.
 * @param api_version The API version the plugin is compiled against (pass GEANY_API_VERSION).
 * @param min_api_version The minimum API version required by the plugin.
 * @param abi_version The exact ABI version the plugin is compiled against (pass GEANY_ABI_VERSION).
 * @param pdata Pointer to the plugin-defined data. Must not be @c NULL.
 * @param free_func Function used to deallocate @a pdata, may be @c NULL.
 *
 * @return TRUE if the plugin was successfully registered. Otherwise FALSE.
 *
 * @since 1.26 (API 225)
 * @see GEANY_PLUGIN_REGISTER_FULL()
 * @see geany_plugin_register()
 **/
GEANY_API_SYMBOL
gboolean geany_plugin_register_full(GeanyPlugin *plugin, gint api_version, gint min_api_version,
									gint abi_version, gpointer pdata, GDestroyNotify free_func)
{
	if (geany_plugin_register(plugin, api_version, min_api_version, abi_version))
	{
		geany_plugin_set_data(plugin, pdata, free_func);
		/* We use LOAD_DATA to indicate that pdata cb_data was set during loading/registration
		 * as opposed to during GeanyPluginFuncs::init(). In the latter case we call free_func
		 * after GeanyPluginFuncs::cleanup() */
		plugin->priv->flags |= LOAD_DATA;
		return TRUE;
	}
	return FALSE;
}

struct LegacyRealFuncs
{
	void       (*init) (GeanyData *data);
	GtkWidget* (*configure) (GtkDialog *dialog);
	void       (*help) (void);
	void       (*cleanup) (void);
};

/* Wrappers to support legacy plugins are below */
static gboolean legacy_init(GeanyPlugin *plugin, gpointer pdata)
{
	struct LegacyRealFuncs *h = pdata;
	h->init(plugin->geany_data);
	return TRUE;
}

static void legacy_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	struct LegacyRealFuncs *h = pdata;
	/* Can be NULL because it's optional for legacy plugins */
	if (h->cleanup)
		h->cleanup();
}

static void legacy_help(GeanyPlugin *plugin, gpointer pdata)
{
	struct LegacyRealFuncs *h = pdata;
	h->help();
}

static GtkWidget *legacy_configure(GeanyPlugin *plugin, GtkDialog *parent, gpointer pdata)
{
	struct LegacyRealFuncs *h = pdata;
	return h->configure(parent);
}

static void free_legacy_cbs(gpointer data)
{
	g_slice_free(struct LegacyRealFuncs, data);
}

/* This function is the equivalent of geany_plugin_register() for legacy-style
 * plugins which we continue to load for the time being. */
static void register_legacy_plugin(Plugin *plugin, GModule *module)
{
	gint (*p_version_check) (gint abi_version);
	void (*p_set_info) (PluginInfo *info);
	void (*p_init) (GeanyData *geany_data);
	GeanyData **p_geany_data;
	struct LegacyRealFuncs *h;

#define CHECK_FUNC(__x)                                                                   \
	if (! g_module_symbol(module, "plugin_" #__x, (void *) (&p_##__x)))                   \
	{                                                                                     \
		geany_debug("Plugin \"%s\" has no plugin_" #__x "() function - ignoring plugin!", \
				g_module_name(module));                                                   \
		return;                                                                           \
	}
	CHECK_FUNC(version_check);
	CHECK_FUNC(set_info);
	CHECK_FUNC(init);
#undef CHECK_FUNC

	/* We must verify the version first. If the plugin has become incompatible any
	 * further actions should be considered invalid and therefore skipped. */
	if (! plugin_check_version(plugin, p_version_check(GEANY_ABI_VERSION)))
		return;

	h = g_slice_new(struct LegacyRealFuncs);

	/* Since the version check passed we can proceed with setting basic fields and
	 * calling its set_info() (which might want to call Geany functions already). */
	g_module_symbol(module, "geany_data", (void *) &p_geany_data);
	if (p_geany_data)
		*p_geany_data = &geany_data;
	/* Read plugin name, etc. name is mandatory but that's enforced in the common code. */
	p_set_info(&plugin->info);

	/* If all went well we can set the remaining callbacks and let it go for good. */
	h->init = p_init;
	g_module_symbol(module, "plugin_configure", (void *) &h->configure);
	g_module_symbol(module, "plugin_configure_single", (void *) &plugin->configure_single);
	g_module_symbol(module, "plugin_help", (void *) &h->help);
	g_module_symbol(module, "plugin_cleanup", (void *) &h->cleanup);
	/* pointer to callbacks struct can be stored directly, no wrapper necessary */
	g_module_symbol(module, "plugin_callbacks", (void *) &plugin->cbs.callbacks);
	if (app->debug_mode)
	{
		if (h->configure && plugin->configure_single)
			g_warning("Plugin '%s' implements plugin_configure_single() unnecessarily - "
				"only plugin_configure() will be used!",
				plugin->info.name);
		if (h->cleanup == NULL)
			g_warning("Plugin '%s' has no plugin_cleanup() function - there may be memory leaks!",
				plugin->info.name);
	}

	plugin->cbs.init = legacy_init;
	plugin->cbs.cleanup = legacy_cleanup;
	plugin->cbs.configure = h->configure ? legacy_configure : NULL;
	plugin->cbs.help = h->help ? legacy_help : NULL;

	plugin->flags = LOADED_OK | IS_LEGACY;
	geany_plugin_set_data(&plugin->public, h, free_legacy_cbs);
}


static gboolean
plugin_load(Plugin *plugin)
{
	gboolean init_ok = TRUE;

	/* Start the plugin. Legacy plugins require additional cruft. */
	if (PLUGIN_IS_LEGACY(plugin) && plugin->proxy == &builtin_so_proxy_plugin)
	{
		GeanyPlugin **p_geany_plugin;
		PluginInfo **p_info;
		GModule *module = plugin->proxy_data;
		/* set these symbols before plugin_init() is called
		 * we don't set geany_data since it is set directly by plugin_new() */
		g_module_symbol(module, "geany_plugin", (void *) &p_geany_plugin);
		if (p_geany_plugin)
			*p_geany_plugin = &plugin->public;
		g_module_symbol(module, "plugin_info", (void *) &p_info);
		if (p_info)
			*p_info = &plugin->info;

		/* Legacy plugin_init() cannot fail. */
		plugin->cbs.init(&plugin->public, plugin->cb_data);
	}
	else
	{
		init_ok = plugin->cbs.init(&plugin->public, plugin->cb_data);
	}

	if (! init_ok)
		return FALSE;

	/* new-style plugins set their callbacks in geany_load_module() */
	if (plugin->cbs.callbacks)
		add_callbacks(plugin, plugin->cbs.callbacks);

	/* remember which plugins are active.
	 * keep list sorted so tools menu items and plugin preference tabs are
	 * sorted by plugin name */
	active_plugin_list = g_list_insert_sorted(active_plugin_list, plugin, cmp_plugin_names);
	proxied_count_inc(plugin->proxy);

	geany_debug("Loaded:   %s (%s)", plugin->filename, plugin->info.name);
	return TRUE;
}


static gpointer plugin_load_gmodule(GeanyPlugin *proxy, GeanyPlugin *subplugin, const gchar *fname, gpointer pdata)
{
	GModule *module;
	void (*p_geany_load_module)(GeanyPlugin *);

	g_return_val_if_fail(g_module_supported(), NULL);
	/* Don't use G_MODULE_BIND_LAZY otherwise we can get unresolved symbols at runtime,
	 * causing a segfault. Without that flag the module will safely fail to load.
	 * G_MODULE_BIND_LOCAL also helps find undefined symbols e.g. app when it would
	 * otherwise not be detected due to the shadowing of Geany's app variable.
	 * Also without G_MODULE_BIND_LOCAL calling public functions e.g. the old info()
	 * function from a plugin will be shadowed. */
	module = g_module_open(fname, G_MODULE_BIND_LOCAL);
	if (!module)
	{
		geany_debug("Can't load plugin: %s", g_module_error());
		return NULL;
	}

	/*geany_debug("Initializing plugin '%s'", plugin->info.name);*/
	g_module_symbol(module, "geany_load_module", (void *) &p_geany_load_module);
	if (p_geany_load_module)
	{
		/* set this here already so plugins can call i.e. plugin_module_make_resident()
		 * right from their geany_load_module() */
		subplugin->priv->proxy_data = module;

		/* This is a new style plugin. It should fill in plugin->info and then call
		 * geany_plugin_register() in its geany_load_module() to successfully load.
		 * The ABI and API checks are performed by geany_plugin_register() (i.e. by us).
		 * We check the LOADED_OK flag separately to protect us against buggy plugins
		 * who ignore the result of geany_plugin_register() and register anyway */
		p_geany_load_module(subplugin);
	}
	else
	{
		/* This is the legacy / deprecated code path. It does roughly the same as
		 * geany_load_module() and geany_plugin_register() together for the new ones */
		register_legacy_plugin(subplugin->priv, module);
	}
	/* We actually check the LOADED_OK flag later */
	return module;
}


static void plugin_unload_gmodule(GeanyPlugin *proxy, GeanyPlugin *subplugin, gpointer load_data, gpointer pdata)
{
	GModule *module = (GModule *) load_data;

	g_return_if_fail(module != NULL);

	if (! g_module_close(module))
		g_warning("%s: %s", subplugin->priv->filename, g_module_error());
}


/* Load and optionally init a plugin.
 * load_plugin decides whether the plugin's plugin_init() function should be called or not. If it is
 * called, the plugin will be started, if not the plugin will be read only (for the list of
 * available plugins in the plugin manager).
 * When add_to_list is set, the plugin will be added to the plugin manager's plugin_list. */
static Plugin*
plugin_new(Plugin *proxy, const gchar *fname, gboolean load_plugin, gboolean add_to_list)
{
	Plugin *plugin;

	g_return_val_if_fail(fname, NULL);
	g_return_val_if_fail(proxy, NULL);

	/* find the plugin in the list of already loaded, active plugins and use it, otherwise
	 * load the module */
	plugin = find_active_plugin_by_name(fname);
	if (plugin != NULL)
	{
		geany_debug("Plugin \"%s\" already loaded.", fname);
		if (add_to_list)
		{
			/* do not add to the list twice */
			if (g_list_find(plugin_list, plugin) != NULL)
				return NULL;

			plugin_list = g_list_prepend(plugin_list, plugin);
		}
		return plugin;
	}

	plugin = g_new0(Plugin, 1);
	plugin->filename = g_strdup(fname);
	plugin->proxy = proxy;
	plugin->public.geany_data = &geany_data;
	plugin->public.priv = plugin;
	/* Fields of plugin->info/funcs must to be initialized by the plugin */
	plugin->public.info = &plugin->info;
	plugin->public.funcs = &plugin->cbs;
	plugin->public.proxy_funcs = &plugin->proxy_cbs;

	if (plugin_loaded(plugin))
	{
		geany_debug("Plugin \"%s\" already loaded.", fname);
		goto err;
	}

	/* Load plugin, this should read its name etc. It must also call
	 * geany_plugin_register() for the following PLUGIN_LOADED_OK condition */
	plugin->proxy_data = proxy->proxy_cbs.load(&proxy->public, &plugin->public, fname, proxy->cb_data);

	if (! PLUGIN_LOADED_OK(plugin))
	{
		geany_debug("Failed to load \"%s\" - ignoring plugin!", fname);
		goto err;
	}

	/* The proxy assumes success, therefore we have to call unload from here
	 * on in case of errors */
	if (EMPTY(plugin->info.name))
	{
		geany_debug("No plugin name set for \"%s\" - ignoring plugin!", fname);
		goto err_unload;
	}

	/* cb_data_destroy() frees plugin->cb_data. If that pointer also passed to unload() afterwards
	 * then that would become a use-after-free. Disallow this combination. If a proxy
	 * needs the same pointer it must not use a destroy func but free manually in its unload(). */
	if (plugin->proxy_data == proxy->cb_data && plugin->cb_data_destroy)
	{
		geany_debug("Proxy of plugin \"%s\" specified invalid data - ignoring plugin!", fname);
		plugin->proxy_data = NULL;
		goto err_unload;
	}

	if (load_plugin && !plugin_load(plugin))
	{
		/* Handle failing init same as failing to load for now. In future we
		 * could present a informational UI or something */
		geany_debug("Plugin failed to initialize \"%s\" - ignoring plugin!", fname);
		goto err_unload;
	}

	if (add_to_list)
		plugin_list = g_list_prepend(plugin_list, plugin);

	return plugin;

err_unload:
	if (plugin->cb_data_destroy)
		plugin->cb_data_destroy(plugin->cb_data);
	proxy->proxy_cbs.unload(&proxy->public, &plugin->public, plugin->proxy_data, proxy->cb_data);
err:
	g_free(plugin->filename);
	g_free(plugin);
	return NULL;
}


static void on_object_weak_notify(gpointer data, GObject *old_ptr)
{
	Plugin *plugin = data;
	guint i = 0;

	g_return_if_fail(plugin && plugin->signal_ids);

	for (i = 0; i < plugin->signal_ids->len; i++)
	{
		SignalConnection *sc = &g_array_index(plugin->signal_ids, SignalConnection, i);

		if (sc->object == old_ptr)
		{
			g_array_remove_index_fast(plugin->signal_ids, i);
			/* we can break the loop right after finding the first match,
			 * because we will get one notification per connected signal */
			break;
		}
	}
}


/* add an object to watch for destruction, and release pointers to it when destroyed.
 * this should only be used by plugin_signal_connect() to add a watch on
 * the object lifetime and nuke out references to it in plugin->signal_ids */
void plugin_watch_object(Plugin *plugin, gpointer object)
{
	g_object_weak_ref(object, on_object_weak_notify, plugin);
}


static void remove_callbacks(Plugin *plugin)
{
	GArray *signal_ids = plugin->signal_ids;
	SignalConnection *sc;

	if (signal_ids == NULL)
		return;

	foreach_array(SignalConnection, sc, signal_ids)
	{
		g_signal_handler_disconnect(sc->object, sc->handler_id);
		g_object_weak_unref(sc->object, on_object_weak_notify, plugin);
	}

	g_array_free(signal_ids, TRUE);
}


static void remove_sources(Plugin *plugin)
{
	GList *item;

	item = plugin->sources;
	while (item != NULL)
	{
		GList *next = item->next; /* cache the next pointer because current item will be freed */

		g_source_destroy(item->data);
		item = next;
	}
	/* don't free the list here, it is allocated inside each source's data */
}


/* Make the GModule backing plugin resident (if it's GModule-backed at all) */
void plugin_make_resident(Plugin *plugin)
{
	if (plugin->proxy == &builtin_so_proxy_plugin)
	{
		g_return_if_fail(plugin->proxy_data != NULL);
		g_module_make_resident(plugin->proxy_data);
	}
	else
		g_warning("Skipping g_module_make_resident() for non-native plugin");
}


/* Retrieve the address of a symbol sym located in plugin, if it's GModule-backed */
gpointer plugin_get_module_symbol(Plugin *plugin, const gchar *sym)
{
	gpointer symbol;

	if (plugin->proxy == &builtin_so_proxy_plugin)
	{
		g_return_val_if_fail(plugin->proxy_data != NULL, NULL);
		if (g_module_symbol(plugin->proxy_data, sym, &symbol))
			return symbol;
		else
			g_warning("Failed to locate signal handler for '%s': %s",
				sym, g_module_error());
	}
	else /* TODO: Could possibly support this via a new proxy hook */
		g_warning("Failed to locate signal handler for '%s': Not supported for non-native plugins",
			sym);
	return NULL;
}


static gboolean is_active_plugin(Plugin *plugin)
{
	return (g_list_find(active_plugin_list, plugin) != NULL);
}


static void remove_each_doc_data(GQuark key_id, gpointer data, gpointer user_data)
{
	const ForEachDocData *doc_data = user_data;
	const gchar *key = g_quark_to_string(key_id);
	if (g_str_has_prefix(key, doc_data->prefix))
		g_datalist_remove_data(&doc_data->document->priv->data, key);
}


static void remove_doc_data(Plugin *plugin)
{
	ForEachDocData data;

	data.prefix = g_strdup_printf("geany/plugins/%s/", plugin->public.info->name);

	for (guint i = 0; i < documents_array->len; i++)
	{
		GeanyDocument *doc = documents_array->pdata[i];
		if (DOC_VALID(doc))
		{
			data.document = doc;
			g_datalist_foreach(&doc->priv->data, remove_each_doc_data, &data);
		}
	}

	g_free(data.prefix);
}


/* Clean up anything used by an active plugin  */
static void
plugin_cleanup(Plugin *plugin)
{
	GtkWidget *widget;

	/* With geany_register_plugin cleanup is mandatory */
	plugin->cbs.cleanup(&plugin->public, plugin->cb_data);

	remove_doc_data(plugin);
	remove_callbacks(plugin);
	remove_sources(plugin);

	if (plugin->key_group)
		keybindings_free_group(plugin->key_group);

	widget = plugin->toolbar_separator.widget;
	if (widget)
		gtk_widget_destroy(widget);

	if (!PLUGIN_HAS_LOAD_DATA(plugin) && plugin->cb_data_destroy)
	{
		/* If the plugin has used geany_plugin_set_data(), destroy the data here. But don't
		 * if it was already set through geany_plugin_register_full() because we couldn't call
		 * its init() anymore (not without completely reloading it anyway). */
		plugin->cb_data_destroy(plugin->cb_data);
		plugin->cb_data = NULL;
		plugin->cb_data_destroy = NULL;
	}

	proxied_count_dec(plugin->proxy);
	geany_debug("Unloaded: %s", plugin->filename);
}


/* Remove all plugins that proxy is a proxy for from plugin_list (and free) */
static void free_subplugins(Plugin *proxy)
{
	GList *item;

	item = plugin_list;
	while (item)
	{
		GList *next = g_list_next(item);
		if (proxy == ((Plugin *) item->data)->proxy)
		{
			/* plugin_free modifies plugin_list */
			plugin_free((Plugin *) item->data);
		}
		item = next;
	}
}


/* Returns true if the removal was successful (=> never for non-proxies) */
static gboolean unregister_proxy(Plugin *proxy)
{
	gboolean is_proxy = FALSE;
	GList *node;

	/* Remove the proxy from the proxy list first. It might appear more than once (once
	 * for each extension), but if it doesn't appear at all it's not actually a proxy */
	foreach_list_safe(node, active_proxies.head)
	{
		PluginProxy *p = node->data;
		if (p->plugin == proxy)
		{
			is_proxy = TRUE;
			g_queue_delete_link(&active_proxies, node);
		}
	}
	return is_proxy;
}


/* Cleanup a plugin and free all resources allocated on behalf of it.
 *
 * If the plugin is a proxy then this also takes special care to unload all
 * subplugin loaded through it (make sure none of them is active!) */
static void
plugin_free(Plugin *plugin)
{
	Plugin *proxy;

	g_return_if_fail(plugin);
	g_return_if_fail(plugin->proxy);
	g_return_if_fail(plugin->proxied_count == 0);

	proxy = plugin->proxy;
	/* If this a proxy remove all depending subplugins. We can assume none of them is *activated*
	 * (but potentially loaded). Note that free_subplugins() might call us through recursion */
	if (is_active_plugin(plugin))
	{
		if (unregister_proxy(plugin))
			free_subplugins(plugin);
		plugin_cleanup(plugin);
	}

	active_plugin_list = g_list_remove(active_plugin_list, plugin);
	plugin_list = g_list_remove(plugin_list, plugin);

	/* cb_data_destroy might be plugin code and must be called before unloading the module. */
	if (plugin->cb_data_destroy)
		plugin->cb_data_destroy(plugin->cb_data);
	proxy->proxy_cbs.unload(&proxy->public, &plugin->public, plugin->proxy_data, proxy->cb_data);

	g_free(plugin->filename);
	g_free(plugin);
}


static gchar *get_custom_plugin_path(const gchar *plugin_path_config,
									 const gchar *plugin_path_system)
{
	gchar *plugin_path_custom;

	if (EMPTY(prefs.custom_plugin_path))
		return NULL;

	plugin_path_custom = utils_get_locale_from_utf8(prefs.custom_plugin_path);
	utils_tidy_path(plugin_path_custom);

	/* check whether the custom plugin path is one of the system or user plugin paths
	 * and abort if so */
	if (utils_str_equal(plugin_path_custom, plugin_path_config) ||
		utils_str_equal(plugin_path_custom, plugin_path_system))
	{
		g_free(plugin_path_custom);
		return NULL;
	}
	return plugin_path_custom;
}


/* all 3 paths Geany looks for plugins in can change (even system path on Windows)
 * so we need to check active plugins are in the right place before loading */
static gboolean check_plugin_path(const gchar *fname)
{
	gchar *plugin_path_config;
	gchar *plugin_path_system;
	gchar *plugin_path_custom;
	gboolean ret = FALSE;

	plugin_path_config = g_build_filename(app->configdir, "plugins", NULL);
	if (g_str_has_prefix(fname, plugin_path_config))
		ret = TRUE;

	plugin_path_system = get_plugin_path();
	if (g_str_has_prefix(fname, plugin_path_system))
		ret = TRUE;

	plugin_path_custom = get_custom_plugin_path(plugin_path_config, plugin_path_system);
	if (plugin_path_custom)
	{
		if (g_str_has_prefix(fname, plugin_path_custom))
			ret = TRUE;

		g_free(plugin_path_custom);
	}
	g_free(plugin_path_config);
	g_free(plugin_path_system);
	return ret;
}


/* Returns NULL if this ain't a plugin,
 * otherwise it returns the appropriate PluginProxy instance to load it */
static PluginProxy* is_plugin(const gchar *file)
{
	GList *node;
	const gchar *ext;

	/* extract file extension to avoid g_str_has_suffix() in the loop */
	ext = (const gchar *)strrchr(file, '.');
	if (ext == NULL)
		return FALSE;
	/* ensure the dot is really part of the filename */
	else if (strchr(ext, G_DIR_SEPARATOR) != NULL)
		return FALSE;

	ext += 1;
	/* O(n*m), (m being extensions per proxy) doesn't scale very well in theory
	 * but not a problem in practice yet */
	foreach_list(node, active_proxies.head)
	{
		PluginProxy *proxy = node->data;
		if (utils_str_casecmp(ext, proxy->extension) == 0)
		{
			Plugin *p = proxy->plugin;
			gint ret = GEANY_PROXY_MATCH;

			if (p->proxy_cbs.probe)
				ret = p->proxy_cbs.probe(&p->public, file, p->cb_data);
			switch (ret)
			{
				case GEANY_PROXY_MATCH:
					return proxy;
				case GEANY_PROXY_RELATED:
					return NULL;
				case GEANY_PROXY_IGNORE:
					continue;
				default:
					g_warning("Ignoring bogus return value '%d' from "
						"proxy plugin '%s' probe() function!", ret,
						proxy->plugin->info.name);
					continue;
			}
		}
	}
	return NULL;
}


/* load active plugins at startup */
static void
load_active_plugins(void)
{
	guint i, len, proxies;

	if (active_plugins_pref == NULL || (len = g_strv_length(active_plugins_pref)) == 0)
		return;

	/* If proxys are loaded we have to restart to load plugins that sort before their proxy */
	do
	{
		proxies = active_proxies.length;
		g_list_free_full(failed_plugins_list, (GDestroyNotify) g_free);
		failed_plugins_list = NULL;
		for (i = 0; i < len; i++)
		{
			gchar *fname = active_plugins_pref[i];

#ifdef G_OS_WIN32
			/* ensure we have canonical paths */
			gchar *p = fname;
			while ((p = strchr(p, '/')) != NULL)
				*p = G_DIR_SEPARATOR;
#endif

			if (!EMPTY(fname) && g_file_test(fname, G_FILE_TEST_EXISTS))
			{
				PluginProxy *proxy = NULL;
				if (check_plugin_path(fname))
					proxy = is_plugin(fname);
				if (proxy == NULL || plugin_new(proxy->plugin, fname, TRUE, FALSE) == NULL)
					failed_plugins_list = g_list_prepend(failed_plugins_list, g_strdup(fname));
			}
		}
	} while (proxies != active_proxies.length);
}


static void
load_plugins_from_path(const gchar *path)
{
	GSList *list, *item;
	gint count = 0;

	list = utils_get_file_list(path, NULL, NULL);

	for (item = list; item != NULL; item = g_slist_next(item))
	{
		gchar *fname = g_build_filename(path, item->data, NULL);
		PluginProxy *proxy = is_plugin(fname);

		if (proxy != NULL && plugin_new(proxy->plugin, fname, FALSE, TRUE))
			count++;

		g_free(fname);
	}

	g_slist_foreach(list, (GFunc) g_free, NULL);
	g_slist_free(list);

	if (count)
		geany_debug("Added %d plugin(s) in '%s'.", count, path);
}


static gchar *get_plugin_path(void)
{
	return g_strdup(utils_resource_dir(RESOURCE_DIR_PLUGIN));
}


/* See load_all_plugins(), this simply sorts items with lower hierarchy level first
 * (where hierarchy level == number of intermediate proxies before the builtin so loader) */
static gint cmp_plugin_by_proxy(gconstpointer a, gconstpointer b)
{
	const Plugin *pa = a;
	const Plugin *pb = b;

	while (TRUE)
	{
		if (pa->proxy == pb->proxy)
			return 0;
		else if (pa->proxy == &builtin_so_proxy_plugin)
			return -1;
		else if (pb->proxy == &builtin_so_proxy_plugin)
			return 1;

		pa = pa->proxy;
		pb = pb->proxy;
	}
}


/* Load (but don't initialize) all plugins for the Plugin Manager dialog */
static void load_all_plugins(void)
{
	gchar *plugin_path_config;
	gchar *plugin_path_system;
	gchar *plugin_path_custom;

	plugin_path_config = g_build_filename(app->configdir, "plugins", NULL);
	plugin_path_system = get_plugin_path();

	/* first load plugins in ~/.config/geany/plugins/ */
	load_plugins_from_path(plugin_path_config);

	/* load plugins from a custom path */
	plugin_path_custom = get_custom_plugin_path(plugin_path_config, plugin_path_system);
	if (plugin_path_custom)
	{
		load_plugins_from_path(plugin_path_custom);
		g_free(plugin_path_custom);
	}

	/* finally load plugins from $prefix/lib/geany */
	load_plugins_from_path(plugin_path_system);

	/* It is important to sort any plugins that are proxied after their proxy because
	 * pm_populate() needs the proxy to be loaded and active (if selected by user) in order
	 * to properly set the value for the PLUGIN_COLUMN_CAN_UNCHECK column. The order between
	 * sub-plugins does not matter, only between sub-plugins and their proxy, thus
	 * sorting by hierarchy level is perfectly sufficient */
	plugin_list = g_list_sort(plugin_list, cmp_plugin_by_proxy);

	g_free(plugin_path_config);
	g_free(plugin_path_system);
}


static void on_tools_menu_show(GtkWidget *menu_item, G_GNUC_UNUSED gpointer user_data)
{
	GList *item, *list = gtk_container_get_children(GTK_CONTAINER(menu_item));
	guint i = 0;
	gboolean have_plugin_menu_items = FALSE;

	for (item = list; item != NULL; item = g_list_next(item))
	{
		if (item->data == menu_separator)
		{
			if (i < g_list_length(list) - 1)
			{
				have_plugin_menu_items = TRUE;
				break;
			}
		}
		i++;
	}
	g_list_free(list);

	ui_widget_show_hide(menu_separator, have_plugin_menu_items);
}


/* Calling this starts up plugin support */
void plugins_load_active(void)
{
	GtkWidget *widget;

	want_plugins = TRUE;

	geany_data_init();

	widget = gtk_separator_menu_item_new();
	gtk_widget_show(widget);
	gtk_container_add(GTK_CONTAINER(main_widgets.tools_menu), widget);

	widget = gtk_menu_item_new_with_mnemonic(_("_Plugin Manager"));
	gtk_container_add(GTK_CONTAINER(main_widgets.tools_menu), widget);
	gtk_widget_show(widget);
	g_signal_connect(widget, "activate", G_CALLBACK(pm_show_dialog), NULL);

	menu_separator = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(main_widgets.tools_menu), menu_separator);
	g_signal_connect(main_widgets.tools_menu, "show", G_CALLBACK(on_tools_menu_show), NULL);

	load_active_plugins();
}


/* Update the global active plugins list so it's up-to-date when configuration
 * is saved. Called in response to GeanyObject's "save-settings" signal. */
static void update_active_plugins_pref(void)
{
	gint i = 0;
	GList *list;
	gsize count;

	/* if plugins are disabled, don't clear list of active plugins */
	if (!want_plugins)
		return;

	count = g_list_length(active_plugin_list) + g_list_length(failed_plugins_list);

	g_strfreev(active_plugins_pref);

	if (count == 0)
	{
		active_plugins_pref = NULL;
		return;
	}

	active_plugins_pref = g_new0(gchar*, count + 1);

	for (list = g_list_first(active_plugin_list); list != NULL; list = list->next)
	{
		Plugin *plugin = list->data;

		active_plugins_pref[i] = g_strdup(plugin->filename);
		i++;
	}
	for (list = g_list_first(failed_plugins_list); list != NULL; list = list->next)
	{
		const gchar *fname = list->data;

		active_plugins_pref[i] = g_strdup(fname);
		i++;
	}
	active_plugins_pref[i] = NULL;
}


/* called even if plugin support is disabled */
void plugins_init(void)
{
	StashGroup *group;
	gchar *path;

	path = get_plugin_path();
	geany_debug("System plugin path: %s", path);
	g_free(path);

	group = stash_group_new("plugins");
	configuration_add_session_group(group, TRUE);

	stash_group_add_toggle_button(group, &prefs.load_plugins,
		"load_plugins", TRUE, "check_plugins");
	stash_group_add_entry(group, &prefs.custom_plugin_path,
		"custom_plugin_path", "", "extra_plugin_path_entry");

	g_signal_connect(geany_object, "save-settings", G_CALLBACK(update_active_plugins_pref), NULL);
	stash_group_add_string_vector(group, &active_plugins_pref, "active_plugins", NULL);

	g_queue_push_head(&active_proxies, &builtin_so_proxy);
}


/* Same as plugin_free(), except it does nothing for proxies-in-use, to be called on
 * finalize in a loop */
static void plugin_free_leaf(Plugin *p)
{
	if (p->proxied_count == 0)
		plugin_free(p);
}


/* called even if plugin support is disabled */
void plugins_finalize(void)
{
	if (failed_plugins_list != NULL)
	{
		g_list_foreach(failed_plugins_list, (GFunc) g_free,	NULL);
		g_list_free(failed_plugins_list);
	}
	/* Have to loop because proxys cannot be unloaded until after all their
	 * plugins are unloaded as well (the second loop should should catch all the remaining ones) */
	while (active_plugin_list != NULL)
		g_list_foreach(active_plugin_list, (GFunc) plugin_free_leaf, NULL);

	g_strfreev(active_plugins_pref);
}


/* Check whether there are any plugins loaded which provide a configure symbol */
gboolean plugins_have_preferences(void)
{
	GList *item;

	if (active_plugin_list == NULL)
		return FALSE;

	foreach_list(item, active_plugin_list)
	{
		Plugin *plugin = item->data;
		if (plugin->configure_single != NULL || plugin->cbs.configure != NULL)
			return TRUE;
	}

	return FALSE;
}


/* Plugin Manager */

enum
{
	PLUGIN_COLUMN_CHECK = 0,
	PLUGIN_COLUMN_CAN_UNCHECK,
	PLUGIN_COLUMN_PLUGIN,
	PLUGIN_N_COLUMNS,
	PM_BUTTON_KEYBINDINGS,
	PM_BUTTON_CONFIGURE,
	PM_BUTTON_HELP
};

typedef struct
{
	GtkWidget *dialog;
	GtkWidget *tree;
	GtkTreeStore *store;
	GtkWidget *filter_entry;
	GtkWidget *configure_button;
	GtkWidget *keybindings_button;
	GtkWidget *help_button;
	GtkWidget *popup_menu;
	GtkWidget *popup_configure_menu_item;
	GtkWidget *popup_keybindings_menu_item;
	GtkWidget *popup_help_menu_item;
}
PluginManagerWidgets;

static PluginManagerWidgets pm_widgets;


static void pm_update_buttons(Plugin *p)
{
	gboolean has_configure = FALSE;
	gboolean has_help = FALSE;
	gboolean has_keybindings = FALSE;

	if (p != NULL && is_active_plugin(p))
	{
		has_configure = p->cbs.configure || p->configure_single;
		has_help = p->cbs.help != NULL;
		has_keybindings = p->key_group && p->key_group->plugin_key_count;
	}

	gtk_widget_set_sensitive(pm_widgets.configure_button, has_configure);
	gtk_widget_set_sensitive(pm_widgets.help_button, has_help);
	gtk_widget_set_sensitive(pm_widgets.keybindings_button, has_keybindings);

	gtk_widget_set_sensitive(pm_widgets.popup_configure_menu_item, has_configure);
	gtk_widget_set_sensitive(pm_widgets.popup_help_menu_item, has_help);
	gtk_widget_set_sensitive(pm_widgets.popup_keybindings_menu_item, has_keybindings);
}


static void pm_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	Plugin *p;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, PLUGIN_COLUMN_PLUGIN, &p, -1);

		if (p != NULL)
			pm_update_buttons(p);
	}
}


static gboolean find_iter_for_plugin(Plugin *p, GtkTreeModel *model, GtkTreeIter *iter)
{
	Plugin *pp;
	gboolean valid;

	for (valid = gtk_tree_model_get_iter_first(model, iter);
	     valid;
	     valid = gtk_tree_model_iter_next(model, iter))
	{
		gtk_tree_model_get(model, iter, PLUGIN_COLUMN_PLUGIN, &pp, -1);
		if (p == pp)
			return TRUE;
	}

	return FALSE;
}


static void pm_populate(GtkTreeStore *store);


static void pm_plugin_toggled(GtkCellRendererToggle *cell, gchar *pth, gpointer data)
{
	gboolean old_state, state;
	gchar *file_name;
	GtkTreeIter iter;
	GtkTreeIter store_iter;
	GtkTreePath *path = gtk_tree_path_new_from_string(pth);
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(pm_widgets.tree));
	Plugin *p;
	Plugin *proxy;
	guint prev_num_proxies;

	gtk_tree_model_get_iter(model, &iter, path);

	gtk_tree_model_get(model, &iter,
		PLUGIN_COLUMN_CHECK, &old_state,
		PLUGIN_COLUMN_PLUGIN, &p, -1);

	/* no plugins item */
	if (p == NULL)
	{
		gtk_tree_path_free(path);
		return;
	}

	gtk_tree_model_filter_convert_iter_to_child_iter(
		GTK_TREE_MODEL_FILTER(model), &store_iter, &iter);

	state = ! old_state; /* toggle the state */

	/* save the filename and proxy of the plugin */
	file_name = g_strdup(p->filename);
	proxy = p->proxy;
	prev_num_proxies = active_proxies.length;

	/* unload plugin module */
	if (!state)
		/* save shortcuts (only need this group, but it doesn't take long) */
		keybindings_write_to_file();

	/* plugin_new() below may cause a tree view refresh with invalid p - set to NULL */
	gtk_tree_store_set(pm_widgets.store, &store_iter,
		PLUGIN_COLUMN_PLUGIN, NULL, -1);
	plugin_free(p);

	/* reload plugin module and initialize it if item is checked */
	p = plugin_new(proxy, file_name, state, TRUE);
	if (!p)
	{
		/* plugin file may no longer be on disk, or is now incompatible */
		gtk_tree_store_remove(pm_widgets.store, &store_iter);
	}
	else
	{
		if (state)
			keybindings_load_keyfile();		/* load shortcuts */

		/* update model */
		gtk_tree_store_set(pm_widgets.store, &store_iter,
			PLUGIN_COLUMN_CHECK, state,
			PLUGIN_COLUMN_PLUGIN, p, -1);

		/* set again the sensitiveness of the configure and help buttons */
		pm_update_buttons(p);

		/* Depending on the state disable the checkbox for the proxy of this plugin, and
		 * only re-enable if the proxy is not used by any other plugin */
		if (p->proxy != &builtin_so_proxy_plugin)
		{
			GtkTreeIter parent;
			gboolean can_uncheck;
			GtkTreePath *store_path = gtk_tree_model_filter_convert_path_to_child_path(
			                                GTK_TREE_MODEL_FILTER(model), path);

			g_warn_if_fail(store_path != NULL);
			if (gtk_tree_path_up(store_path))
			{
				gtk_tree_model_get_iter(GTK_TREE_MODEL(pm_widgets.store), &parent, store_path);

				if (state)
					can_uncheck = FALSE;
				else
					can_uncheck = p->proxy->proxied_count == 0;

				gtk_tree_store_set(pm_widgets.store, &parent,
					PLUGIN_COLUMN_CAN_UNCHECK, can_uncheck, -1);
			}
			gtk_tree_path_free(store_path);
		}
	}
	/* We need to find out if a proxy was added or removed because that affects the plugin list
	 * presented by the plugin manager */
	if (prev_num_proxies != active_proxies.length)
	{
		/* Rescan the plugin list as we now support more. Gives some "already loaded" warnings
		 * they are unproblematic */
		if (prev_num_proxies < active_proxies.length)
			load_all_plugins();

		pm_populate(pm_widgets.store);
		gtk_tree_view_expand_row(GTK_TREE_VIEW(pm_widgets.tree), path, FALSE);
	}

	gtk_tree_path_free(path);
	g_free(file_name);
}

static void pm_populate(GtkTreeStore *store)
{
	GtkTreeIter iter;
	GList *list;

	gtk_tree_store_clear(store);
	list = g_list_first(plugin_list);
	if (list == NULL)
	{
		gtk_tree_store_append(store, &iter, NULL);
		gtk_tree_store_set(store, &iter, PLUGIN_COLUMN_CHECK, FALSE,
				PLUGIN_COLUMN_PLUGIN, NULL, -1);
	}
	else
	{
		for (; list != NULL; list = list->next)
		{
			Plugin *p = list->data;
			GtkTreeIter parent;

			if (p->proxy != &builtin_so_proxy_plugin
			        && find_iter_for_plugin(p->proxy, GTK_TREE_MODEL(pm_widgets.store), &parent))
				gtk_tree_store_append(store, &iter, &parent);
			else
				gtk_tree_store_append(store, &iter, NULL);

			gtk_tree_store_set(store, &iter,
				PLUGIN_COLUMN_CHECK, is_active_plugin(p),
				PLUGIN_COLUMN_PLUGIN, p,
				PLUGIN_COLUMN_CAN_UNCHECK, (p->proxied_count == 0),
				-1);
		}
	}
}

static gboolean pm_treeview_query_tooltip(GtkWidget *widget, gint x, gint y,
		gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	Plugin *p = NULL;
	gboolean can_uncheck = TRUE;

	if (! gtk_tree_view_get_tooltip_context(GTK_TREE_VIEW(widget), &x, &y, keyboard_mode,
			&model, &path, &iter))
		return FALSE;

	gtk_tree_model_get(model, &iter, PLUGIN_COLUMN_PLUGIN, &p, PLUGIN_COLUMN_CAN_UNCHECK, &can_uncheck, -1);
	if (p != NULL)
	{
		gchar *prefix, *suffix, *details, *markup;
		const gchar *uchk;

		uchk = can_uncheck ?
		       "" : _("\n<i>Other plugins depend on this. Disable them first to allow deactivation.</i>\n");
		/* Four allocations is less than ideal but meh */
		details = g_strdup_printf(_("Version:\t%s\nAuthor(s):\t%s\nFilename:\t%s"),
			p->info.version, p->info.author, p->filename);
		prefix = g_markup_printf_escaped("<b>%s</b>\n%s\n", p->info.name, p->info.description);
		suffix = g_markup_printf_escaped("<small><i>\n%s</i></small>", details);
		markup = g_strconcat(prefix, uchk, suffix, NULL);

		gtk_tooltip_set_markup(tooltip, markup);
		gtk_tree_view_set_tooltip_row(GTK_TREE_VIEW(widget), tooltip, path);

		g_free(details);
		g_free(suffix);
		g_free(prefix);
		g_free(markup);
	}
	gtk_tree_path_free(path);

	return p != NULL;
}


static void pm_treeview_text_cell_data_func(GtkTreeViewColumn *column, GtkCellRenderer *cell,
		GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	Plugin *p;

	gtk_tree_model_get(model, iter, PLUGIN_COLUMN_PLUGIN, &p, -1);

	if (p == NULL)
		g_object_set(cell, "text", _("No plugins available."), NULL);
	else
	{
		gchar *markup = g_markup_printf_escaped("<b>%s</b>\n%s", p->info.name, p->info.description);

		g_object_set(cell, "markup", markup, NULL);
		g_free(markup);
	}
}


static gboolean pm_treeview_button_press_cb(GtkWidget *widget, GdkEventButton *event,
		G_GNUC_UNUSED gpointer user_data)
{
	if (event->button == 3)
	{
		ui_menu_popup(GTK_MENU(pm_widgets.popup_menu), NULL, NULL,
					  event->button, event->time);
	}
	return FALSE;
}


static gint pm_tree_sort_func(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
		gpointer user_data)
{
	Plugin *pa, *pb;

	gtk_tree_model_get(model, a, PLUGIN_COLUMN_PLUGIN, &pa, -1);
	gtk_tree_model_get(model, b, PLUGIN_COLUMN_PLUGIN, &pb, -1);

	if (pa && pb)
		return strcmp(pa->info.name, pb->info.name);
	else
		return pa - pb;
}


static gboolean pm_tree_search(const gchar *key, const gchar *haystack)
{
	gchar *normalized_string = NULL;
	gchar *normalized_key = NULL;
	gchar *case_normalized_string = NULL;
	gchar *case_normalized_key = NULL;
	gboolean matched = TRUE;

	normalized_string = g_utf8_normalize(haystack, -1, G_NORMALIZE_ALL);
	normalized_key = g_utf8_normalize(key, -1, G_NORMALIZE_ALL);

	if (normalized_string != NULL && normalized_key != NULL)
	{
		GString *stripped_key;
		gchar **subkey, **subkeys;

		case_normalized_string = g_utf8_casefold(normalized_string, -1);
		case_normalized_key = g_utf8_casefold(normalized_key, -1);
		stripped_key = g_string_new(case_normalized_key);
		do {} while (utils_string_replace_all(stripped_key, "  ", " "));
		subkeys = g_strsplit(stripped_key->str, " ", -1);
		g_string_free(stripped_key, TRUE);
		foreach_strv(subkey, subkeys)
		{
			if (strstr(case_normalized_string, *subkey) == NULL)
			{
				matched = FALSE;
				break;
			}
		}
		g_strfreev(subkeys);
	}

	g_free(normalized_key);
	g_free(normalized_string);
	g_free(case_normalized_key);
	g_free(case_normalized_string);

	return matched;
}


static gboolean pm_tree_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	Plugin *plugin;
	gboolean matched;
	const gchar *key;
	gchar *haystack, *filename;

	gtk_tree_model_get(model, iter, PLUGIN_COLUMN_PLUGIN, &plugin, -1);

	if (!plugin)
		return TRUE;
	key = gtk_entry_get_text(GTK_ENTRY(pm_widgets.filter_entry));

	filename = g_path_get_basename(plugin->filename);
	haystack = g_strjoin(" ", plugin->info.name, plugin->info.description,
					plugin->info.author, filename, NULL);
	matched = pm_tree_search(key, haystack);
	g_free(haystack);
	g_free(filename);

	return matched;
}


static void on_pm_tree_filter_entry_changed_cb(GtkEntry *entry, gpointer user_data)
{
	GtkTreeModel *filter_model = gtk_tree_view_get_model(GTK_TREE_VIEW(pm_widgets.tree));
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter_model));
}


static void on_pm_tree_filter_entry_icon_release_cb(GtkEntry *entry, GtkEntryIconPosition icon_pos,
		GdkEvent *event, gpointer user_data)
{
	if (event->button.button == 1 && icon_pos == GTK_ENTRY_ICON_PRIMARY)
		on_pm_tree_filter_entry_changed_cb(entry, user_data);
}


static void pm_prepare_treeview(GtkWidget *tree, GtkTreeStore *store)
{
	GtkCellRenderer *text_renderer, *checkbox_renderer;
	GtkTreeViewColumn *column;
	GtkTreeModel *filter_model;
	GtkTreeSelection *sel;

	g_signal_connect(tree, "query-tooltip", G_CALLBACK(pm_treeview_query_tooltip), NULL);
	gtk_widget_set_has_tooltip(tree, TRUE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), FALSE);

	checkbox_renderer = gtk_cell_renderer_toggle_new();
	column = gtk_tree_view_column_new_with_attributes(
		_("Active"), checkbox_renderer,
		"active", PLUGIN_COLUMN_CHECK, "activatable", PLUGIN_COLUMN_CAN_UNCHECK, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
	g_signal_connect(checkbox_renderer, "toggled", G_CALLBACK(pm_plugin_toggled), NULL);

	text_renderer = gtk_cell_renderer_text_new();
	g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new_with_attributes(_("Plugin"), text_renderer, NULL);
	gtk_tree_view_column_set_cell_data_func(column, text_renderer,
		pm_treeview_text_cell_data_func, NULL, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), FALSE);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), PLUGIN_COLUMN_PLUGIN,
		pm_tree_sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(
		GTK_TREE_SORTABLE(store), PLUGIN_COLUMN_PLUGIN, GTK_SORT_ASCENDING);

	/* selection handling */
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
	g_signal_connect(sel, "changed", G_CALLBACK(pm_selection_changed), NULL);

	g_signal_connect(tree, "button-press-event", G_CALLBACK(pm_treeview_button_press_cb), NULL);

	/* filter */
	filter_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL);
	gtk_tree_model_filter_set_visible_func(
		GTK_TREE_MODEL_FILTER(filter_model), pm_tree_filter_func, NULL, NULL);

	/* set model to tree view */
	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), filter_model);
	g_object_unref(filter_model);

	pm_populate(store);
}


static void pm_on_plugin_button_clicked(G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	Plugin *p;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pm_widgets.tree));
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, PLUGIN_COLUMN_PLUGIN, &p, -1);

		if (p != NULL)
		{
			if (GPOINTER_TO_INT(user_data) == PM_BUTTON_CONFIGURE)
				plugin_show_configure(&p->public);
			else if (GPOINTER_TO_INT(user_data) == PM_BUTTON_HELP)
			{
				g_return_if_fail(p->cbs.help != NULL);
				p->cbs.help(&p->public, p->cb_data);
			}
			else if (GPOINTER_TO_INT(user_data) == PM_BUTTON_KEYBINDINGS && p->key_group && p->key_group->plugin_key_count > 0)
				keybindings_dialog_show_prefs_scroll(p->info.name);
		}
	}
}


static void
free_non_active_plugin(gpointer data, gpointer user_data)
{
	Plugin *plugin = data;

	/* don't do anything when closing the plugin manager and it is an active plugin */
	if (is_active_plugin(plugin))
		return;

	plugin_free(plugin);
}


/* Callback when plugin manager dialog closes, responses GTK_RESPONSE_CLOSE and
 * GTK_RESPONSE_DELETE_EVENT are treated the same. */
static void pm_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	switch (response)
	{
		case GTK_RESPONSE_CLOSE:
		case GTK_RESPONSE_DELETE_EVENT:
			if (plugin_list != NULL)
			{
				/* remove all non-active plugins from the list */
				g_list_foreach(plugin_list, free_non_active_plugin, NULL);
				g_list_free(plugin_list);
				plugin_list = NULL;
			}
			gtk_widget_destroy(GTK_WIDGET(dialog));
			pm_widgets.dialog = NULL;

			configuration_save();
			break;
		case PM_BUTTON_CONFIGURE:
		case PM_BUTTON_HELP:
		case PM_BUTTON_KEYBINDINGS:
			/* forward event to the generic handler */
			pm_on_plugin_button_clicked(NULL, GINT_TO_POINTER(response));
			break;
	}
}


static void pm_show_dialog(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *vbox, *vbox2, *swin, *label, *menu_item, *filter_entry;

	if (pm_widgets.dialog != NULL)
	{
		gtk_window_present(GTK_WINDOW(pm_widgets.dialog));
		return;
	}

	/* before showing the dialog, we need to create the list of available plugins */
	load_all_plugins();

	pm_widgets.dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(pm_widgets.dialog), _("Plugins"));
	gtk_window_set_transient_for(GTK_WINDOW(pm_widgets.dialog), GTK_WINDOW(main_widgets.window));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(pm_widgets.dialog), TRUE);

	vbox = ui_dialog_vbox_new(GTK_DIALOG(pm_widgets.dialog));
	gtk_widget_set_name(pm_widgets.dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	gtk_window_set_default_size(GTK_WINDOW(pm_widgets.dialog), 500, 450);

	pm_widgets.help_button = gtk_dialog_add_button(
		GTK_DIALOG(pm_widgets.dialog), GTK_STOCK_HELP, PM_BUTTON_HELP);
	pm_widgets.configure_button = gtk_dialog_add_button(
		GTK_DIALOG(pm_widgets.dialog), GTK_STOCK_PREFERENCES, PM_BUTTON_CONFIGURE);
	pm_widgets.keybindings_button = gtk_dialog_add_button(
		GTK_DIALOG(pm_widgets.dialog), _("Keybindings"), PM_BUTTON_KEYBINDINGS);
	gtk_dialog_add_button(GTK_DIALOG(pm_widgets.dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_set_default_response(GTK_DIALOG(pm_widgets.dialog), GTK_RESPONSE_CLOSE);

	/* filter */
	pm_widgets.filter_entry = filter_entry = gtk_entry_new();
	gtk_entry_set_icon_from_stock(GTK_ENTRY(filter_entry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_FIND);
	ui_entry_add_clear_icon(GTK_ENTRY(filter_entry));
	g_signal_connect(filter_entry, "changed", G_CALLBACK(on_pm_tree_filter_entry_changed_cb), NULL);
	g_signal_connect(filter_entry, "icon-release",
		G_CALLBACK(on_pm_tree_filter_entry_icon_release_cb), NULL);

	/* prepare treeview */
	pm_widgets.tree = gtk_tree_view_new();
	pm_widgets.store = gtk_tree_store_new(
		PLUGIN_N_COLUMNS, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_POINTER);
	pm_prepare_treeview(pm_widgets.tree, pm_widgets.store);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(pm_widgets.tree));
	g_object_unref(pm_widgets.store);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(swin), pm_widgets.tree);

	label = geany_wrap_label_new(_("Choose which plugins should be loaded at startup:"));

	/* plugin popup menu */
	pm_widgets.popup_menu = gtk_menu_new();

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	gtk_container_add(GTK_CONTAINER(pm_widgets.popup_menu), menu_item);
	g_signal_connect(menu_item, "activate",
			G_CALLBACK(pm_on_plugin_button_clicked), GINT_TO_POINTER(PM_BUTTON_CONFIGURE));
	pm_widgets.popup_configure_menu_item = menu_item;

	menu_item = gtk_image_menu_item_new_with_mnemonic(_("Keybindings"));
	gtk_container_add(GTK_CONTAINER(pm_widgets.popup_menu), menu_item);
	g_signal_connect(menu_item, "activate",
			G_CALLBACK(pm_on_plugin_button_clicked), GINT_TO_POINTER(PM_BUTTON_KEYBINDINGS));
	pm_widgets.popup_keybindings_menu_item = menu_item;

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP, NULL);
	gtk_container_add(GTK_CONTAINER(pm_widgets.popup_menu), menu_item);
	g_signal_connect(menu_item, "activate",
			G_CALLBACK(pm_on_plugin_button_clicked), GINT_TO_POINTER(PM_BUTTON_HELP));
	pm_widgets.popup_help_menu_item = menu_item;

	/* put it together */
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), filter_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), swin, TRUE, TRUE, 0);

	g_signal_connect(pm_widgets.dialog, "response", G_CALLBACK(pm_dialog_response), NULL);

	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show_all(pm_widgets.dialog);
	gtk_widget_show_all(pm_widgets.popup_menu);

	/* set initial plugin buttons state, pass NULL as no plugin is selected by default */
	pm_update_buttons(NULL);
	gtk_widget_grab_focus(pm_widgets.filter_entry);
}


/** Register the plugin as a proxy for other plugins
 *
 * Proxy plugins register a list of file extensions and a set of callbacks that are called
 * appropriately. A plugin can be a proxy for multiple types of sub-plugins by handling
 * separate file extensions, however they must share the same set of hooks, because this
 * function can only be called at most once per plugin.
 *
 * Each callback receives the plugin-defined data as parameter (see geany_plugin_register()). The
 * callbacks must be set prior to calling this, by assigning to @a plugin->proxy_funcs.
 * GeanyProxyFuncs::load and GeanyProxyFuncs::unload must be implemented.
 *
 * Nested proxies are unsupported at this point (TODO).
 *
 * @note It is entirely up to the proxy to provide access to Geany's plugin API. Native code
 * can naturally call Geany's API directly, for interpreted languages the proxy has to
 * implement some kind of bindings that the plugin can use.
 *
 * @see proxy for detailed documentation and an example.
 *
 * @param plugin The pointer to the plugin's GeanyPlugin instance
 * @param extensions A @c NULL-terminated string array of file extensions, excluding the dot.
 * @return @c TRUE if the proxy was successfully registered, otherwise @c FALSE
 *
 * @since 1.26 (API 226)
 */
GEANY_API_SYMBOL
gboolean geany_plugin_register_proxy(GeanyPlugin *plugin, const gchar **extensions)
{
	Plugin *p;
	const gchar **ext;
	PluginProxy *proxy;
	GList *node;

	g_return_val_if_fail(plugin != NULL, FALSE);
	g_return_val_if_fail(extensions != NULL, FALSE);
	g_return_val_if_fail(*extensions != NULL, FALSE);
	g_return_val_if_fail(plugin->proxy_funcs->load != NULL, FALSE);
	g_return_val_if_fail(plugin->proxy_funcs->unload != NULL, FALSE);

	p = plugin->priv;
	/* Check if this was called already. We want to reserve for the use case of calling
	 * this again to set new supported extensions (for example, based on proxy configuration). */
	foreach_list(node, active_proxies.head)
	{
		proxy = node->data;
		g_return_val_if_fail(p != proxy->plugin, FALSE);
	}

	foreach_strv(ext, extensions)
	{
		if (**ext == '.')
		{
			g_warning(_("Proxy plugin '%s' extension '%s' starts with a dot. "
				"Please fix your proxy plugin."), p->info.name, *ext);
		}

		proxy = g_new(PluginProxy, 1);
		g_strlcpy(proxy->extension, *ext, sizeof(proxy->extension));
		proxy->plugin = p;
		/* prepend, so that plugins automatically override core providers for a given extension */
		g_queue_push_head(&active_proxies, proxy);
	}

	return TRUE;
}

#endif
