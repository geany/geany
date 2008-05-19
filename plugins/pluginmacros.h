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

/* Useful macros to avoid typing geany_data-> or geany_functions-> so often. */

#ifndef PLUGINMACROS_H
#define PLUGINMACROS_H

/* common data structs */
#define app				geany_data->app
#define doc_array		geany_data->doc_array			/* Allows use of doc_list[] macro */
#define filetypes_array	geany_data->filetypes_array		/* Allows use of filetypes[] macro */
#define prefs			geany_data->prefs
#define project			app->project


/* These macros are named the same as the first word in the core function name,
 * but with a 'p_' prefix to prevent conflicts with other tag names.
 * Example: document_open_file() -> p_document->open_file() */
/* New macros should be added here */
#define p_filetypes		geany_functions->p_filetypes
#define p_navqueue		geany_functions->p_navqueue
#define p_editor		geany_functions->p_editor


#ifdef GEANY_DISABLE_DEPRECATED

#define p_dialogs		geany_functions->p_dialogs
#define p_document		geany_functions->p_document
#define p_encodings		geany_functions->p_encodings
#define p_highlighting	geany_functions->p_highlighting
#define p_keybindings	geany_functions->p_keybindings
#define p_msgwindow		geany_functions->p_msgwindow
#define p_sci			geany_functions->p_sci
#define p_search		geany_functions->p_search
#define p_support		geany_functions->p_support
#define p_templates		geany_functions->p_templates
#define p_tm			geany_functions->p_tm
#define p_ui			geany_functions->p_ui
#define p_utils			geany_functions->p_utils

#else

#define p_dialogs		dialogs
#define p_document		documents
#define p_encodings		encodings
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


/* Temporary source compatibility macros - do not use these in new code, they may get removed. */
#define dialogs			geany_functions->p_dialogs
#define documents		geany_functions->p_document
#define encodings		geany_functions->p_encodings
#define highlighting	geany_functions->p_highlighting
#define keybindings		geany_functions->p_keybindings
#define msgwindow		geany_functions->p_msgwindow
#define scintilla		geany_functions->p_sci
#define search			geany_functions->p_search
#define support			geany_functions->p_support
#define templates		geany_functions->p_templates
#define tagmanager		geany_functions->p_tm
#define ui				geany_functions->p_ui
#define utils			geany_functions->p_utils
#define p_encoding		p_encodings

#endif

#endif
