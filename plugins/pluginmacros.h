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

/** @file pluginmacros.h
 * Useful macros to avoid typing @c geany_data-> or @c geany_functions-> so often.
 *
 * @section function_macros Function Macros
 * These macros are named the same as the first word in the core function name,
 * but with a 'p_' prefix to prevent conflicts with other tag names.
 *
 * Example for @c document_open_file(): @c p_document->open_file(); */


#ifndef PLUGINMACROS_H
#define PLUGINMACROS_H

/* common items */
#define geany			geany_data	/**< Simple macro for @c geany_data that reduces typing. */
#define documents_array	geany_data->documents_array		/**< Allows use of @c documents[] macro */
#define filetypes_array	geany_data->filetypes_array		/**< Allows use of @c filetypes[] macro */


/* function group macros */
#define p_editor		geany_functions->p_editor		/**< See editor.h */
#define p_document		geany_functions->p_document		/**< See document.h */
#define p_dialogs		geany_functions->p_dialogs		/**< See dialogs.h */
#define p_encodings		geany_functions->p_encodings	/**< See encodings.h */
#define p_filetypes		geany_functions->p_filetypes	/**< See filetypes.h */
#define p_highlighting	geany_functions->p_highlighting	/**< See highlighting.h */
#define p_keybindings	geany_functions->p_keybindings	/**< See keybindings.h */
#define p_main			geany_functions->p_main			/**< See main.h */
#define p_msgwindow		geany_functions->p_msgwindow	/**< See msgwindow.h */
#define p_navqueue		geany_functions->p_navqueue		/**< See navqueue.h */
#define p_sci			geany_functions->p_sci			/**< See sciwrappers.h */
#define p_search		geany_functions->p_search		/**< See search.h */
#define p_support		geany_functions->p_support		/**< See support.h */
#define p_templates		geany_functions->p_templates	/**< See templates.h */
#define p_tm			geany_functions->p_tm			/**< See the TagManager headers. */
#define p_ui			geany_functions->p_ui			/**< See ui_utils.h */
#define p_utils			geany_functions->p_utils		/**< See utils.h */

#endif
