/*
 *      pluginprivate.h - this file is part of Geany, a fast and lightweight IDE
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */


#ifndef GEANY_PLUGINPRIVATE_H
#define GEANY_PLUGINPRIVATE_H

#include "plugindata.h"
#include "ui_utils.h"	/* GeanyAutoSeparator */
#include "keybindings.h"	/* GeanyKeyGroup */


typedef struct SignalConnection
{
	GObject	*object;
	gulong	handler_id;
}
SignalConnection;


typedef struct GeanyPluginPrivate
{
	GModule 		*module;
	gchar			*filename;				/* plugin filename (/path/libname.so) */
	PluginInfo		info;				/* plugin name, description, etc */
	GeanyPlugin		public;				/* fields the plugin can read */

	void		(*init) (GeanyData *data);			/* Called when the plugin is enabled */
	GtkWidget*	(*configure) (GtkDialog *dialog);	/* plugins configure dialog, optional */
	void		(*configure_single) (GtkWidget *parent); /* plugin configure dialog, optional */
	void		(*help) (void);						/* Called when the plugin should show some help, optional */
	void		(*cleanup) (void);					/* Called when the plugin is disabled or when Geany exits */

	/* extra stuff */
	PluginFields	fields;
	GeanyKeyGroup	*key_group;
	GeanyAutoSeparator	toolbar_separator;
	GArray			*signal_ids;			/* SignalConnection's to disconnect when unloading */
	GList			*sources;				/* GSources to destroy when unloading */
}
GeanyPluginPrivate;

typedef GeanyPluginPrivate Plugin;	/* shorter alias */


#endif /* GEANY_PLUGINPRIVATE_H */
