/*
 *      pluginsymbols.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */

/* Note: this file is for Doxygen only. */

/**
 * @file pluginsymbols.c
 * Symbols declared from within plugins.
 *
 * Geany looks for these symbols (arrays, pointers and functions) when initializing
 * plugins. Some of them are optional, i.e. they can be omitted; others are required
 * and must be defined. Some symbols should only be declared using specific macros in
 * @link plugindata.h @endlink.
 */

/** Use the PLUGIN_VERSION_CHECK() macro instead. Required by Geany. */
gint plugin_version_check(gint);

/** Use the PLUGIN_SET_INFO() macro to define it. Required by Geany.
 * This function is called before the plugin is initialized, so Geany
 * can read the plugin's name.
 * @param info The data struct which should be initialized by this function. */
void plugin_set_info(PluginInfo *info);

/** @deprecated Use @ref geany_plugin->info instead.
 * Basic information about a plugin, which is set in plugin_set_info(). */
const PluginInfo *plugin_info;

/** Basic information for the plugin and identification. */
const GeanyPlugin *geany_plugin;

/** Geany owned data pointers.
 * Example: @c assert(geany_data->app->configdir != NULL); */
const GeanyData *geany_data;

/** Geany owned function pointers, split into groups.
 * Example: @code #include "geanyfunctions.h"
 * ...
 * document_new_file(NULL, NULL, NULL); @endcode
 * This is equivalent of @c geany_functions->p_document->new_file(NULL, NULL, NULL); */
const GeanyFunctions *geany_functions;

/** @deprecated Use @ref ui_add_document_sensitive() instead.
 * Plugin owned fields, including flags. */
PluginFields *plugin_fields;

/** An array for connecting GeanyObject events, which should be terminated with
 * @c {NULL, NULL, FALSE, NULL}. See @link signals Signal documentation @endlink.
 * @see plugin_signal_connect(). */
PluginCallback plugin_callbacks[];

/** Most plugins should use the PLUGIN_KEY_GROUP() macro to define it. However,
 * its fields are not read until after plugin_init() is called for the plugin, so it
 * is possible to setup a variable number of keybindings, e.g. based on the
 * plugin's configuration file settings.
 * - The @c name field must not be empty or match Geany's default group name.
 * - The @c label field is set by Geany after plugin_init() is called to the name of the
 * plugin.
 * @note This is a single element array for implementation reasons,
 * but you can treat it like a pointer. */
KeyBindingGroup plugin_key_group[1];


/** Called before showing the plugin preferences dialog to let the user set some basic
 * plugin configuration options. Can be omitted when not needed.
 * @param dialog The plugin preferences dialog widget - this should only be used to
 * connect the @c "response" signal. If settings should be read from the dialog, the
 * reponse will be either @c GTK_RESPONSE_OK or @c GTK_RESPONSE_APPLY.
 * @return A container widget holding preference widgets. */
GtkWidget* plugin_configure(GtkDialog *dialog);

/** Called after loading the plugin.
 * @param data The same as #geany_data. */
void plugin_init(GeanyData *data);

/** Called before unloading the plugin. Required for normal plugins - it should undo
 * everything done in plugin_init() - e.g. destroy menu items, free memory. */
void plugin_cleanup();

/** Called whenever the plugin should show its documentation (if any). This may open a dialog,
 * a browser with a website or a local installed HTML help file(see utils_start_browser())
 * or something else.
 * Can be omitted when not needed. */
void plugin_help();

