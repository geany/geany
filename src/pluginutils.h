/*
 *      pluginutils.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2009-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#ifndef PLUGINUTILS_H
#define PLUGINUTILS_H

#ifdef HAVE_PLUGINS

#include "plugindata.h"		/* GeanyPlugin */

void plugin_add_toolbar_item(GeanyPlugin *plugin, GtkToolItem *item);

void plugin_module_make_resident(GeanyPlugin *plugin);

void plugin_signal_connect(GeanyPlugin *plugin,
		GObject *object, gchar *signal_name, gboolean after,
		GCallback callback, gpointer user_data);

#endif /* HAVE_PLUGINS */
#endif /* PLUGINUTILS_H */
