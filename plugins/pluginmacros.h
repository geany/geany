/*
 *      pluginmacros.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

#define app				geany_data->app
#define doc_array		geany_data->doc_array
#define prefs			geany_data->prefs
#define project			app->project


/* These macros are named the same as the first word in the core function name,
 * but with a 'p_' prefix to prevent conflicts with other tag names.
 * Example: document_open_file() -> p_document->open_file() */
#define p_filetypes		geany_data->filetype
#define p_navqueue		geany_data->navqueue

#ifdef GEANY_DISABLE_DEPRECATED

#define p_dialogs		geany_data->dialogs
#define p_document		geany_data->documents
#define p_encoding		geany_data->encoding
#define p_highlighting	geany_data->highlighting
#define p_keybindings	geany_data->keybindings
#define p_msgwindow		geany_data->msgwindow
#define p_sci			geany_data->sci
#define p_search		geany_data->search
#define p_support		geany_data->support
#define p_templates		geany_data->templates
#define p_tm			geany_data->tm
#define p_ui			geany_data->ui
#define p_utils			geany_data->utils

#else

#define p_dialogs		dialogs
#define p_document		documents
#define p_encoding		encodings
#define p_highlighting	highlighting
#define p_keybindings	keybindings
#define p_msgwindow		msgwindow
#define p_sci			scintilla
#define p_search		search
#define p_support		support
#define p_templates		templates
#define p_tm			tagmanager
#define p_ui			ui
#define p_utils			utils


/* Temporary source compatibility macros - do not use these in new code. */
#define dialogs			geany_data->dialogs
#define documents		geany_data->documents	/* avoids conflict with document typedef */
#define encodings		geany_data->encoding	/* avoids conflict with document::encoding */
#define highlighting	geany_data->highlighting
#define keybindings		geany_data->keybindings
#define msgwindow		geany_data->msgwindow
#define scintilla		geany_data->sci
#define search			geany_data->search
#define support			geany_data->support
#define templates		geany_data->templates
#define tagmanager		geany_data->tm			/* avoids conflict with "struct tm *t" */
#define ui				geany_data->ui
#define utils			geany_data->utils

#endif

#endif
