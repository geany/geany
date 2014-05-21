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
#include "plugindata.h"
#include "pluginprivate.h"
#include "plugins.h"
#include "support.h"
#include "toolbar.h"
#include "ui_utils.h"
#include "utils.h"


/** Inserts a toolbar item before the Quit button, or after the previous plugin toolbar item.
 * A separator is added on the first call to this function, and will be shown when @a item is
 * shown; hidden when @a item is hidden.
 * @note You should still destroy @a item yourself, usually in @ref plugin_cleanup().
 * @param plugin Must be @ref geany_plugin.
 * @param item The item to add. */
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
void plugin_module_make_resident(GeanyPlugin *plugin)
{
	g_return_if_fail(plugin);

	g_module_make_resident(plugin->priv->module);
}


/** Connects a signal which will be disconnected on unloading the plugin, to prevent a possible segfault.
 * @param plugin Must be @ref geany_plugin.
 * @param object Object to connect to, or @c NULL when using @link pluginsignals.c Geany signals @endlink.
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
 *       @c g_signal_connect() is safe and has lower overhead. */
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


/** Adds a GLib main loop timeout callback that will be removed when unloading the plugin,
 *  preventing it to run after the plugin has been unloaded (which may lead to a segfault).
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
guint plugin_timeout_add(GeanyPlugin *plugin, guint interval, GSourceFunc function, gpointer data)
{
	return plugin_source_add(plugin, g_timeout_source_new(interval), function, data);
}


/** Adds a GLib main loop timeout callback that will be removed when unloading the plugin,
 *  preventing it to run after the plugin has been unloaded (which may lead to a segfault).
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
guint plugin_timeout_add_seconds(GeanyPlugin *plugin, guint interval, GSourceFunc function,
		gpointer data)
{
	return plugin_source_add(plugin, g_timeout_source_new_seconds(interval), function, data);
}


/** Adds a GLib main loop IDLE callback that will be removed when unloading the plugin, preventing
 *  it to run after the plugin has been unloaded (which may lead to a segfault).
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
guint plugin_idle_add(GeanyPlugin *plugin, GSourceFunc function, gpointer data)
{
	return plugin_source_add(plugin, g_idle_source_new(), function, data);
}


/** Sets up or resizes a keybinding group for the plugin.
 * You should then call keybindings_set_item() for each keybinding in the group.
 * @param plugin Must be @ref geany_plugin.
 * @param section_name Name used in the configuration file, such as @c "html_chars".
 * @param count Number of keybindings for the group.
 * @param callback Group callback, or @c NULL if you only want individual keybinding callbacks.
 * @return The plugin's keybinding group.
 * @since 0.19. */
GeanyKeyGroup *plugin_set_key_group(GeanyPlugin *plugin,
		const gchar *section_name, gsize count, GeanyKeyGroupCallback callback)
{
	Plugin *priv = plugin->priv;

	priv->key_group = keybindings_set_group(priv->key_group, section_name,
		priv->info.name, count, callback);
	return priv->key_group;
}


static void on_pref_btn_clicked(gpointer btn, Plugin *p)
{
	p->configure_single(main_widgets.window);
}


static GtkWidget *create_pref_page(Plugin *p, GtkWidget *dialog)
{
	GtkWidget *page = NULL;	/* some plugins don't have prefs */

	if (p->configure)
	{
		page = p->configure(GTK_DIALOG(dialog));

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
void plugin_show_configure(GeanyPlugin *plugin)
{
	Plugin *p;

	if (!plugin)
	{
		configure_plugins(NULL);
		return;
	}
	p = plugin->priv;

	if (p->configure)
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

	if (!g_module_symbol(data->plugin->priv->module, handler_name, &symbol))
	{
		g_warning("Failed to locate signal handler for '%s': %s",
			signal_name, g_module_error());
		return;
	}

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
void plugin_builder_connect_signals(GeanyPlugin *plugin,
	GtkBuilder *builder, gpointer user_data)
{
	struct BuilderConnectData data = { NULL };

	g_return_if_fail(plugin != NULL && plugin->priv != NULL);
	g_return_if_fail(plugin->priv->module != NULL);
	g_return_if_fail(GTK_IS_BUILDER(builder));

	data.user_data = user_data;
	data.plugin = plugin;

	gtk_builder_connect_signals_full(builder, connect_plugin_signals, &data);
}


#endif
