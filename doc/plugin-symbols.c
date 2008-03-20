/*
 *      plugin-symbols.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * @file plugin-symbols.c
 * Symbols declared from within plugins.
 *
 * Geany looks for these symbols (arrays, pointers and functions) when initializing
 * plugins. Some of them are optional, i.e. they can be omitted; others are required
 * and must be defined. Some symbols should only be declared using specific macros in
 * @link plugindata.h @endlink.
 */

/** Use the VERSION_CHECK() macro instead. Required by Geany. */
gint version_check(gint);

/** Use the PLUGIN_INFO() macro to define it. Required by Geany. */
PluginInfo* info();

/** Geany owned fields and functions. */
GeanyData* geany_data;

/** Plugin owned fields, including flags. */
PluginFields* plugin_fields;

/** An array for connecting GeanyObject events, which should be terminated with
 * @c {NULL, NULL, FALSE, NULL}. See @link signals Signal documentation @endlink. */
GeanyCallback geany_callbacks[];

/** Most plugins should use the PLUGIN_KEY_GROUP() macro to define it. However,
 * its fields are not read until after init() is called for the plugin, so it
 * is possible to setup a variable number of keybindings, e.g. based on the
 * plugin's configuration file settings.
 * - The @c name field must not be empty or match Geany's default group name.
 * - The @c label field is set by Geany after init() is called to the name of the
 * plugin.
 * @note This is a single element array for implementation reasons,
 * but you can treat it like a pointer. */
KeyBindingGroup plugin_key_group[1];


/** Called when the plugin should show a configure dialog to let the user set some basic
 * plugin configuration. Optionally, can be omitted when not needed.
 * @param parent The Plugin Manager dialog widget. */
void configure(GtkWidget *parent);

/** Called after loading the plugin.
 * @param data The same as #geany_data. */
void init(GeanyData *data);

/** Called before unloading the plugin. Required for normal plugins - it should undo
 * everything done in init() - e.g. destroy menu items, free memory. */
void cleanup(); */

