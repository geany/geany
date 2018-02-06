/*
 *      pluginutils.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2009-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

/** @file pluginutils.h
 * Plugin utility functions.
 * These functions all take the @ref geany_plugin symbol as their first argument. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_PLUGINS

#include "pluginutils.h"

#include "app.h"
#include "geanyobject.h"
#include "keybindings.h"
#include "keybindingsprivate.h"
#include "plugindata.h"
#include "pluginprivate.h"
#include "plugins.h"
#include "support.h"
#include "toolbar.h"
#include "ui_utils.h"
#include "utils.h"


typedef struct
{
	gpointer data;
	GDestroyNotify free_func;
}
PluginDocDataProxy;


/** Returns the runtime API version Geany was compiled with.
 *
 * Unlike @ref GEANY_API_VERSION this version is the value of that
 * define at the time when Geany itself was compiled. This allows to
 * establish soft dependencies which are resolved at runtime depending
 * on Geany's API version.
 *
 * @return Geany's API version
 * @since 1.30 (API 231)
 **/
GEANY_API_SYMBOL
gint geany_api_version(void)
{
	return GEANY_API_VERSION;
}

/** Inserts a toolbar item before the Quit button, or after the previous plugin toolbar item.
 * A separator is added on the first call to this function, and will be shown when @a item is
 * shown; hidden when @a item is hidden.
 * @note You should still destroy @a item yourself, usually in @ref plugin_cleanup().
 * @param plugin Must be @ref geany_plugin.
 * @param item The item to add. */
GEANY_API_SYMBOL
void plugin_add_toolbar_item(GeanyPlugin *plugin, GtkToolItem *item)
{
	GtkToolbar *toolbar = GTK_TOOLBAR(main_widgets.toolbar);
	gint pos;
	GeanyAutoSeparator *autosep;

	g_return_if_fail(plugin);
	autosep = &plugin->priv->toolbar_separator;

	if (!autosep->widget)
	{
		GtkToolItem *sep;

		pos = toolbar_get_insert_position();

		sep = gtk_separator_tool_item_new();
		gtk_toolbar_insert(toolbar, sep, pos);
		autosep->widget = GTK_WIDGET(sep);

		toolbar_item_ref(sep);
	}
	else
	{
		pos = gtk_toolbar_get_item_index(toolbar, GTK_TOOL_ITEM(autosep->widget));
		g_return_if_fail(pos >= 0);
	}

	gtk_toolbar_insert(toolbar, item, pos + autosep->item_count + 1);
	toolbar_item_ref(item);

	/* hide the separator widget if there are no toolbar items showing for the plugin */
	ui_auto_separator_add_ref(autosep, GTK_WIDGET(item));
}


/** Ensures that a plugin's module (*.so) will never be unloaded.
 *  This is necessary if you register new GTypes in your plugin, e.g. when using own classes
 *  using the GObject system.
 *
 * @param plugin Must be @ref geany_plugin.
 *
 *  @since 0.16
 */
GEANY_API_SYMBOL
void plugin_module_make_resident(GeanyPlugin *plugin)
{
	g_return_if_fail(plugin);
	plugin_make_resident(plugin->priv);
}


/** @girskip
 * Connects a signal which will be disconnected on unloading the plugin, to prevent a possible segfault.
 * @param plugin Must be @ref geany_plugin.
 * @param object @nullable Object to connect to, or @c NULL when using @link pluginsignals.c Geany signals @endlink.
 * @param signal_name The name of the signal. For a list of available
 * signals, please see the @link pluginsignals.c Signal documentation @endlink.
 * @param after Set to @c TRUE to call your handler after the main signal handlers have been called
 * (if supported by @a signal_name).
 * @param callback The function to call when the signal is emitted.
 * @param user_data The user data passed to the signal handler.
 * @see plugin_callbacks.
 *
 * @warning Before version 1.25 (API < 218),
 *          this should only be used on objects that outlive the plugin, never on
 *          objects that will get destroyed before the plugin is unloaded.  For objects
 *          created and destroyed by the plugin, you can simply use @c g_signal_connect(),
 *          since all handlers are disconnected when the object is destroyed anyway.
 *          For objects that may or may not outlive the plugin (like @link GeanyEditor.sci
 *          a document's @c ScintillaObject @endlink, which is destroyed when the document
 *          is closed), you currently have to manually handle both situations, when the
 *          plugin is unloaded before the object is destroyed (and then, you have to
 *          disconnect the signal on @c plugin_cleanup()), and when the object is destroyed
 *          during the plugin's lifetime (in which case you cannot and should not disconnect
 *          manually in @c plugin_cleanup() since it already has been disconnected and the
 *          object has been destroyed), and disconnect yourself or not as appropriate.
 * @note Since version 1.25 (API >= 218), the object lifetime is watched and so the above
 *       restriction does not apply.  However, for objects destroyed by the plugin,
 *       @c g_signal_connect() is safe and has lower overhead.
 **/
GEANY_API_SYMBOL
void plugin_signal_connect(GeanyPlugin *plugin,
		GObject *object, const gchar *signal_name, gboolean after,
		GCallback callback, gpointer user_data)
{
	gulong id;
	SignalConnection sc;

	g_return_if_fail(plugin != NULL);
	g_return_if_fail(object == NULL || G_IS_OBJECT(object));

	if (!object)
		object = geany_object;

	id = after ?
		g_signal_connect_after(object, signal_name, callback, user_data) :
		g_signal_connect(object, signal_name, callback, user_data);

	if (!plugin->priv->signal_ids)
		plugin->priv->signal_ids = g_array_new(FALSE, FALSE, sizeof(SignalConnection));

	sc.object = object;
	sc.handler_id = id;
	g_array_append_val(plugin->priv->signal_ids, sc);

	/* watch the object lifetime to nuke our pointers to it */
	plugin_watch_object(plugin->priv, object);
}


typedef struct PluginSourceData
{
	Plugin		*plugin;
	GList		list_link;	/* element of plugin->sources cointaining this GSource */
	GSourceFunc	function;
	gpointer	user_data;
} PluginSourceData;


/* prepend psd->list_link to psd->plugin->sources */
static void psd_register(PluginSourceData *psd, GSource *source)
{
	psd->list_link.data = source;
	psd->list_link.prev = NULL;
	psd->list_link.next = psd->plugin->sources;
	if (psd->list_link.next)
		psd->list_link.next->prev = &psd->list_link;
	psd->plugin->sources = &psd->list_link;
}


/* removes psd->list_link from psd->plugin->sources */
static void psd_unregister(PluginSourceData *psd)
{
	if (psd->list_link.next)
		psd->list_link.next->prev = psd->list_link.prev;
	if (psd->list_link.prev)
		psd->list_link.prev->next = psd->list_link.next;
	else /* we were the first of the list, update the plugin->sources pointer */
		psd->plugin->sources = psd->list_link.next;
}


static void on_plugin_source_destroy(gpointer data)
{
	PluginSourceData *psd = data;

	psd_unregister(psd);
	g_slice_free1(sizeof *psd, psd);
}


static gboolean on_plugin_source_callback(gpointer data)
{
	PluginSourceData *psd = data;

	return psd->function(psd->user_data);
}


/* adds the given source to the default GMainContext and to the list of sources to remove at plugin
 * unloading time */
static guint plugin_source_add(GeanyPlugin *plugin, GSource *source, GSourceFunc func, gpointer data)
{
	guint id;
	PluginSourceData *psd = g_slice_alloc(sizeof *psd);

	psd->plugin = plugin->priv;
	psd->function = func;
	psd->user_data = data;

	g_source_set_callback(source, on_plugin_source_callback, psd, on_plugin_source_destroy);
	psd_register(psd, source);
	id = g_source_attach(source, NULL);
	g_source_unref(source);

	return id;
}


/** @girskip
 * Adds a GLib main loop timeout callback that will be removed when unloading the plugin,
 * preventing it to run after the plugin has been unloaded (which may lead to a segfault).
 *
 * @param plugin Must be @ref geany_plugin.
 * @param interval The time between calls to the function, in milliseconds.
 * @param function The function to call after the given timeout.
 * @param data The user data passed to the function.
 * @return the ID of the event source (you generally won't need it, or better use g_timeout_add()
 *   directly if you want to manage this event source manually).
 *
 * @see g_timeout_add()
 * @since 0.21, plugin API 205.
 */
GEANY_API_SYMBOL
guint plugin_timeout_add(GeanyPlugin *plugin, guint interval, GSourceFunc function, gpointer data)
{
	return plugin_source_add(plugin, g_timeout_source_new(interval), function, data);
}


/** @girskip
 * Adds a GLib main loop timeout callback that will be removed when unloading the plugin,
 * preventing it to run after the plugin has been unloaded (which may lead to a segfault).
 *
 * @param plugin Must be @ref geany_plugin.
 * @param interval The time between calls to the function, in seconds.
 * @param function The function to call after the given timeout.
 * @param data The user data passed to the function.
 * @return the ID of the event source (you generally won't need it, or better use
 *   g_timeout_add_seconds() directly if you want to manage this event source manually).
 *
 * @see g_timeout_add_seconds()
 * @since 0.21, plugin API 205.
 */
GEANY_API_SYMBOL
guint plugin_timeout_add_seconds(GeanyPlugin *plugin, guint interval, GSourceFunc function,
		gpointer data)
{
	return plugin_source_add(plugin, g_timeout_source_new_seconds(interval), function, data);
}


/** @girskip
 * Adds a GLib main loop IDLE callback that will be removed when unloading the plugin, preventing
 * it to run after the plugin has been unloaded (which may lead to a segfault).
 *
 * @param plugin Must be @ref geany_plugin.
 * @param function The function to call in IDLE time.
 * @param data The user data passed to the function.
 * @return the ID of the event source (you generally won't need it, or better use g_idle_add()
 *   directly if you want to manage this event source manually).
 *
 * @see g_idle_add()
 * @since 0.21, plugin API 205.
 */
GEANY_API_SYMBOL
guint plugin_idle_add(GeanyPlugin *plugin, GSourceFunc function, gpointer data)
{
	return plugin_source_add(plugin, g_idle_source_new(), function, data);
}


/** @girskip
 * Sets up or resizes a keybinding group for the plugin.
 * You should then call keybindings_set_item() for each keybinding in the group.
 * @param plugin Must be @ref geany_plugin.
 * @param section_name Name of the section used for this group in the keybindings configuration file, i.e. @c "html_chars".
 * @param count Number of keybindings for the group.
 * @param callback @nullable Group callback, or @c NULL if you only want individual keybinding callbacks.
 * @return The plugin's keybinding group.
 * @since 0.19.
 **/
GEANY_API_SYMBOL
GeanyKeyGroup *plugin_set_key_group(GeanyPlugin *plugin,
		const gchar *section_name, gsize count, GeanyKeyGroupCallback callback)
{
	Plugin *priv = plugin->priv;

	priv->key_group = keybindings_set_group(priv->key_group, section_name,
		priv->info.name, count, callback);
	return priv->key_group;
}

/** Sets up or resizes a keybinding group for the plugin
 *
 * You should then call keybindings_set_item() or keybindings_set_item_full() for each
 * keybinding in the group.
 * @param plugin Must be @ref geany_plugin.
 * @param section_name Name of the section used for this group in the keybindings configuration file, i.e. @c "html_chars".
 * @param count Number of keybindings for the group.
 * @param cb @nullable New-style group callback, or @c NULL if you only want individual keybinding callbacks.
 * @param pdata Plugin specific data, passed to the group callback @a cb.
 * @param destroy_notify Function that is invoked to free the plugin data when not needed anymore.
 * @return @transfer{none} The plugin's keybinding group.
 *
 * @since 1.26 (API 226)
 * @see See keybindings_set_item
 * @see See keybindings_set_item_full
 **/
GEANY_API_SYMBOL
GeanyKeyGroup *plugin_set_key_group_full(GeanyPlugin *plugin,
		const gchar *section_name, gsize count,
		GeanyKeyGroupFunc cb, gpointer pdata, GDestroyNotify destroy_notify)
{
	GeanyKeyGroup *group;

	group = plugin_set_key_group(plugin, section_name, count, NULL);
	group->cb_func = cb;
	group->cb_data = pdata;
	group->cb_data_destroy = destroy_notify;

	return group;
}


static void on_pref_btn_clicked(gpointer btn, Plugin *p)
{
	p->configure_single(main_widgets.window);
}


static GtkWidget *create_pref_page(Plugin *p, GtkWidget *dialog)
{
	GtkWidget *page = NULL;	/* some plugins don't have prefs */

	if (p->cbs.configure)
	{
		page = p->cbs.configure(&p->public, GTK_DIALOG(dialog), p->cb_data);
		if (! GTK_IS_WIDGET(page))
		{
			geany_debug("Invalid widget returned from plugin_configure() in plugin \"%s\"!",
				p->info.name);
			return NULL;
		}
		else
		{
			GtkWidget *align = gtk_alignment_new(0.5, 0.5, 1, 1);

			gtk_alignment_set_padding(GTK_ALIGNMENT(align), 6, 6, 6, 6);
			gtk_container_add(GTK_CONTAINER(align), page);
			page = gtk_vbox_new(FALSE, 0);
			gtk_box_pack_start(GTK_BOX(page), align, TRUE, TRUE, 0);
		}
	}
	else if (p->configure_single)
	{
		GtkWidget *align = gtk_alignment_new(0.5, 0.5, 0, 0);
		GtkWidget *btn;

		gtk_alignment_set_padding(GTK_ALIGNMENT(align), 6, 6, 6, 6);

		btn = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);
		g_signal_connect(btn, "clicked", G_CALLBACK(on_pref_btn_clicked), p);
		gtk_container_add(GTK_CONTAINER(align), btn);
		page = align;
	}
	return page;
}


/* multiple plugin configure dialog
 * current_plugin can be NULL */
static void configure_plugins(Plugin *current_plugin)
{
	GtkWidget *dialog, *vbox, *nb;
	GList *node;
	gint cur_page = -1;

	dialog = gtk_dialog_new_with_buttons(_("Configure Plugins"),
		GTK_WINDOW(main_widgets.window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	gtk_widget_set_name(dialog, "GeanyDialog");

	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	nb = gtk_notebook_new();
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(nb), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), nb, TRUE, TRUE, 0);

	foreach_list(node, active_plugin_list)
	{
		Plugin *p = node->data;
		GtkWidget *page = create_pref_page(p, dialog);

		if (page)
		{
			GtkWidget *label = gtk_label_new(p->info.name);
			gint n = gtk_notebook_append_page(GTK_NOTEBOOK(nb), page, label);

			if (p == current_plugin)
				cur_page = n;
		}
	}
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(nb)))
	{
		gtk_widget_show_all(vbox);
		if (cur_page >= 0)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), cur_page);

		/* run the dialog */
		while (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_APPLY);
	}
	else
		utils_beep();

	gtk_widget_destroy(dialog);
}


/** Shows the plugin's configure dialog.
 * The plugin must implement one of the plugin_configure() or plugin_configure_single() symbols.
 * @param plugin Must be @ref geany_plugin.
 * @since 0.19. */
/* if NULL, show all plugins */
GEANY_API_SYMBOL
void plugin_show_configure(GeanyPlugin *plugin)
{
	Plugin *p;

	if (!plugin)
	{
		configure_plugins(NULL);
		return;
	}
	p = plugin->priv;

	if (p->cbs.configure)
		configure_plugins(p);
	else
	{
		g_return_if_fail(p->configure_single);
		p->configure_single(main_widgets.window);
	}
}


struct BuilderConnectData
{
	gpointer user_data;
	GeanyPlugin *plugin;
};


static void connect_plugin_signals(GtkBuilder *builder, GObject *object,
	const gchar *signal_name, const gchar *handler_name,
	GObject *connect_object, GConnectFlags flags, gpointer user_data)
{
	gpointer symbol = NULL;
	struct BuilderConnectData *data = user_data;

	symbol = plugin_get_module_symbol(data->plugin->priv, handler_name);

	plugin_signal_connect(data->plugin, object, signal_name, FALSE,
		G_CALLBACK(symbol) /*ub?*/, data->user_data);
}


/**
 * Allows auto-connecting Glade/GtkBuilder signals in plugins.
 *
 * When a plugin uses GtkBuilder to load some UI from file/string,
 * the gtk_builder_connect_signals() function is unable to automatically
 * connect to the plugin's signal handlers. A plugin could itself use
 * the gtk_builder_connect_signals_full() function to automatically
 * connect to the signal handler functions by loading it's GModule
 * and retrieving pointers to the handler functions, but rather than
 * each plugin having to do that, this function handles it automatically.
 *
 * @code
 * ...
 * GeanyPlugin *geany_plugin;
 *
 * G_MODULE_EXPORT void
 * myplugin_button_clicked(GtkButton *button, gpointer user_data)
 * {
 *   g_print("Button pressed\n");
 * }
 *
 * void plugin_init(GeanyData *data)
 * {
 *   GtkBuilder *builder = gtk_builder_new();
 *   gtk_builder_add_from_file(builder, "gui.glade", NULL);
 *   plugin_builder_connect_signals(geany_plugin, builder, NULL);
 *   ...
 * }
 * @endcode
 *
 * @note It's important that you prefix your callback handlers with
 * a plugin-specific prefix to avoid clashing with other plugins since
 * the function symbols will be exported process-wide.
 *
 * @param plugin Must be @ref geany_plugin.
 * @param builder The GtkBuilder to connect signals with.
 * @param user_data User data to pass to the connected signal handlers.
 *
 * @since 1.24, plugin API 217.
 */
GEANY_API_SYMBOL
void plugin_builder_connect_signals(GeanyPlugin *plugin,
	GtkBuilder *builder, gpointer user_data)
{
	struct BuilderConnectData data = { NULL };

	g_return_if_fail(plugin != NULL && plugin->priv != NULL);
	g_return_if_fail(GTK_IS_BUILDER(builder));

	data.user_data = user_data;
	data.plugin = plugin;

	gtk_builder_connect_signals_full(builder, connect_plugin_signals, &data);
}


/** Get the additional data that corresponds to the plugin.
 *
 * @param plugin The plugin provided by Geany
 * @return The data corresponding to the plugin or @c NULL if none set.
 *
 * @since 1.32 (API 234)
 *
 * @see geany_plugin_set_data()
 */
gpointer geany_plugin_get_data(const GeanyPlugin *plugin)
{
	g_return_val_if_fail (plugin != NULL, NULL);
	g_return_val_if_fail (PLUGIN_LOADED_OK (plugin->priv), NULL);

	return plugin->priv->cb_data;
}


/** Add additional data that corresponds to the plugin.
 *
 * @p pdata is the pointer going to be passed to the individual plugin callbacks
 * of GeanyPlugin::funcs. When the  plugin is cleaned up, @p free_func is invoked for the data,
 * which connects the data to the time the plugin is enabled.
 *
 * One intended use case is to set GObjects as data and have them destroyed automatically
 * by passing g_object_unref() as @a free_func, so that member functions can be used
 * for the @ref GeanyPluginFuncs (via wrappers) but you can set completely custom data.
 *
 * Be aware that this can only be called once and only by plugins registered via
 * @ref geany_plugin_register(). So-called legacy plugins cannot use this function.
 *
 * @note This function must not be called if the plugin was registered with
 * geany_plugin_register_full().
 *
 * @param plugin The plugin provided by Geany
 * @param pdata The plugin's data to associate, must not be @c NULL
 * @param free_func The destroy notify
 *
 * @since 1.26 (API 225)
 */
GEANY_API_SYMBOL
void geany_plugin_set_data(GeanyPlugin *plugin, gpointer pdata, GDestroyNotify free_func)
{
	Plugin *p = plugin->priv;

	g_return_if_fail(PLUGIN_LOADED_OK(p));
	/* Do not allow calling this only to set a notify. */
	g_return_if_fail(pdata != NULL);
	/* The rationale to allow only setting the data once is the following:
	 * In the future we want to support proxy plugins (which bind non-C plugins to
	 * Geany's plugin api). These proxy plugins might need to own the data pointer
	 * on behalf of the proxied plugin. However, if not, then the plugin should be
	 * free to use it. This way we can make sure the plugin doesn't accidentally
	 * trash its proxy.
	 *
	 * Better a more limited API now that can be opened up later than a potentially
	 * wrong one that can only be replaced by another one. */
	if (p->cb_data != NULL || p->cb_data_destroy != NULL)
	{
		if (PLUGIN_HAS_LOAD_DATA(p))
			g_warning("Invalid call to %s(), geany_plugin_register_full() was used. Ignored!\n", G_STRFUNC);
		else
			g_warning("Double call to %s(), ignored!", G_STRFUNC);
		return;
	}

	p->cb_data = pdata;
	p->cb_data_destroy = free_func;
}


static void plugin_doc_data_proxy_free(gpointer pdata)
{
	PluginDocDataProxy *prox = pdata;
	if (prox != NULL)
	{
		if (prox->free_func)
			prox->free_func(prox->data);
		g_slice_free(PluginDocDataProxy, prox);
	}
}


/**
 * Retrieve plugin-specific data attached to a document.
 *
 * @param plugin The plugin who attached the data.
 * @param doc The document which the data was attached to.
 * @param key The key name of the attached data.
 *
 * @return The attached data pointer or `NULL` if the key is not found
 * for the given plugin.
 *
 * @since 1.29 (Plugin API 228)
 * @see plugin_set_document_data plugin_set_document_data_full
 */
GEANY_API_SYMBOL
gpointer plugin_get_document_data(struct GeanyPlugin *plugin,
	struct GeanyDocument *doc, const gchar *key)
{
	gchar *real_key;
	PluginDocDataProxy *data;

	g_return_val_if_fail(plugin != NULL, NULL);
	g_return_val_if_fail(doc != NULL, NULL);
	g_return_val_if_fail(key != NULL && *key != '\0', NULL);

	real_key = g_strdup_printf("geany/plugins/%s/%s", plugin->info->name, key);
	data = document_get_data(doc, real_key);
	g_free(real_key);

	return (data != NULL) ? data->data : NULL;
}


/**
 * Attach plugin-specific data to a document.
 *
 * @param plugin The plugin attaching data to the document.
 * @param doc The document to attach the data to.
 * @param key The key name for the data.
 * @param data The pointer to attach to the document.
 *
 * @since 1.29 (Plugin API 228)
 * @see plugin_get_document_data plugin_set_document_data_full
 */
GEANY_API_SYMBOL
void plugin_set_document_data(struct GeanyPlugin *plugin, struct GeanyDocument *doc,
	const gchar *key, gpointer data)
{
	plugin_set_document_data_full(plugin, doc, key, data, NULL);
}


/**
 * Attach plugin-specific data and a free function to a document.
 *
 * This is useful for plugins who want to keep some additional data with
 * the document and even have it auto-released appropriately (see below).
 *
 * This is a simple example showing how a plugin might use this to
 * attach a string to each document and print it when the document is
 * saved:
 *
 * @code
 * void on_document_open(GObject *unused, GeanyDocument *doc, GeanyPlugin *plugin)
 * {
 *     plugin_set_document_data_full(plugin, doc, "my-data",
 *         g_strdup("some-data"), g_free);
 * }
 *
 * void on_document_save(GObject *unused, GeanyDocument *doc, GeanyPlugin *plugin)
 * {
 *     const gchar *some_data = plugin_get_document_data(plugin, doc, "my-data");
 *     g_print("my-data: %s", some_data);
 * }
 *
 * gboolean plugin_init(GeanyPlugin *plugin, gpointer unused)
 * {
 *     plugin_signal_connect(plugin, NULL, "document-open", TRUE,
 *         G_CALLBACK(on_document_open), plugin);
 *     plugin_signal_connect(plugin, NULL, "document-new", TRUE,
 *         G_CALLBACK(on_document_open), plugin);
 *     plugin_signal_connect(plugin, NULL, "document-save", TRUE,
 *         G_CALLBACK(on_document_save), plugin);
 *     return TRUE;
 * }
 *
 * void geany_load_module(GeanyPlugin *plugin)
 * {
 *   // ...
 *   plugin->funcs->init = plugin_init;
 *   // ...
 * }
 * @endcode
 *
 * The @a free_func can be used to tie the lifetime of the data to that
 * of the @a doc and/or the @a plugin. The @a free_func will be called
 * in any of the following cases:
 *
 *   - When a document is closed.
 *   - When the plugin is unloaded.
 *   - When the document data is set again using the same key.
 *
 * @param plugin The plugin attaching data to the document.
 * @param doc The document to attach the data to.
 * @param key The key name for the data.
 * @param data The pointer to attach to the document.
 * @param free_func The function to call with data when removed.
 *
 * @since 1.29 (Plugin API 228)
 * @see plugin_get_document_data plugin_set_document_data
 */
GEANY_API_SYMBOL
void plugin_set_document_data_full(struct GeanyPlugin *plugin,
	struct GeanyDocument *doc, const gchar *key, gpointer data,
	GDestroyNotify free_func)
{
	PluginDocDataProxy *prox;

	g_return_if_fail(plugin != NULL);
	g_return_if_fail(doc != NULL);
	g_return_if_fail(key != NULL);

	prox = g_slice_new(PluginDocDataProxy);
	if (prox != NULL)
	{
		gchar *real_key = g_strdup_printf("geany/plugins/%s/%s", plugin->info->name, key);
		prox->data = data;
		prox->free_func = free_func;
		document_set_data_full(doc, real_key, prox, plugin_doc_data_proxy_free);
		g_free(real_key);
	}
}


#endif
