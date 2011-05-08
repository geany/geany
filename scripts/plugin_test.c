/*
 *      plugin_test.c
 *
 *      Copyright 2010-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2010-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 */


/* This code tries to load the passed Geany plugin
 * (passed as path, e.g. pluginname.so) and looks up some symbols.
 * Example use case for this tool is to test whether a plugin is
 * loadable and defines all required symbols.
 *
 * This file is not built during the normal build process, instead
 * compile it on your with the following command in the root of the Geany source tree:
 *
 * cc -o plugin_test scripts/plugin_test.c `pkg-config --cflags --libs glib-2.0 gmodule-2.0`
 */

#include <stdio.h>
#include <glib.h>
#include <gmodule.h>


static gboolean tp_check_version(GModule *module)
{
	/* TODO implement me */
	return TRUE;
}


static void tp_close_module(GModule *module)
{
	if (! g_module_close(module))
		g_warning("%s: %s", g_module_name(module), g_module_error());
}


/* Emulate loading a plugin and looking up some symbols,
 * similar to what Geany would do on loading the plugin.
 */
static gboolean test_plugin(const gchar *filename)
{
	GModule *module;
	void (*plugin_set_info)(void*);
	void (*init) (void *data);
	void (*cleanup) (void);

	g_return_val_if_fail(filename, FALSE);
	g_return_val_if_fail(g_module_supported(), FALSE);

	module = g_module_open(filename, G_MODULE_BIND_LOCAL);
	if (! module)
	{
		g_warning("Can't load plugin: %s", g_module_error());
		return FALSE;
	}

	if (! tp_check_version(module))
	{
		tp_close_module(module);
		return FALSE;
	}

	g_module_symbol(module, "plugin_set_info", (void *) &plugin_set_info);
	if (plugin_set_info == NULL)
	{
		g_warning("No plugin_set_info() defined for \"%s\" - consider fixing the plugin!", filename);
		tp_close_module(module);
		return FALSE;
	}

	g_module_symbol(module, "plugin_init", (void *) &init);
	if (init == NULL)
	{
		g_warning("Plugin '%s' has no plugin_init() function - consider fixing the plugin!", filename);
		tp_close_module(module);
		return FALSE;
	}

	g_module_symbol(module, "plugin_cleanup", (void *) &cleanup);
	if (cleanup == NULL)
	{
		g_warning("Plugin '%s' has no plugin_cleanup() function - there may be memory leaks!", filename);
	}

	tp_close_module(module);

	return TRUE;
}


gint main(gint argc, gchar **argv)
{
	gint i;
	gint result = 0;

	g_set_prgname("plugin_test");
	/* we could perform better argument processing here as well as more error checking but
	 * it's probably not worth at all */
	for (i = 1; i < argc; i++)
	{
		if (! test_plugin(argv[i]))
			result = 1;
	}

	return result;
}

