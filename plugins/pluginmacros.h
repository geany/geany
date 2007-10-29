/*
 *      pluginmacros.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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

/* Useful macros to avoid typing geany_data so often. */

#ifndef PLUGINMACROS_H
#define PLUGINMACROS_H

#define app			geany_data->app
#define doc_array	geany_data->doc_array
#define prefs		geany_data->prefs
#define project		app->project

#define dialogs		geany_data->dialogs
#define documents	geany_data->document	// avoids conflict with document typedef
#define encodings	geany_data->encoding	// avoids conflict with document::encoding
#define keybindings	geany_data->keybindings
#define msgwindow	geany_data->msgwindow
#define scintilla	geany_data->sci
#define support		geany_data->support
#define templates	geany_data->templates
#define tagmanager	geany_data->tm			// avoids conflict with "struct tm *t"
#define ui			geany_data->ui
#define utils		geany_data->utils

#endif
