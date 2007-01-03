/*
 *      templates.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

#ifndef GEANY_TEMPLATES_H
#define GEANY_TEMPLATES_H 1

#include "filetypes.h"


void templates_init(void);

gchar *templates_get_template_fileheader(gint idx);

gchar *templates_get_template_new_file(filetype *ft);

gchar *templates_get_template_changelog(void);

gchar *templates_get_template_generic(gint template);

gchar *templates_get_template_function(gint template, const gchar *func_name);

gchar *templates_get_template_gpl(gint filetype_idx);

void templates_free_templates(void);


enum
{
	GEANY_TEMPLATE_GPL_PASCAL = 0,
	GEANY_TEMPLATE_GPL_ROUTE,
	GEANY_TEMPLATE_GPL,
	GEANY_TEMPLATE_FILEHEADER_PASCAL,
	GEANY_TEMPLATE_FILEHEADER_ROUTE,
	GEANY_TEMPLATE_FILEHEADER,
	GEANY_TEMPLATE_CHANGELOG,
	GEANY_TEMPLATE_FUNCTION,
	GEANY_TEMPLATE_FUNCTION_PASCAL,
	GEANY_TEMPLATE_FUNCTION_ROUTE,
	GEANY_TEMPLATE_MULTILINE,
	GEANY_TEMPLATE_MULTILINE_PASCAL,
	GEANY_TEMPLATE_MULTILINE_ROUTE,

	GEANY_TEMPLATE_FILETYPE_NONE,
	GEANY_TEMPLATE_FILETYPE_C,
	GEANY_TEMPLATE_FILETYPE_CPP,
	GEANY_TEMPLATE_FILETYPE_JAVA,
	GEANY_TEMPLATE_FILETYPE_PHP,
	GEANY_TEMPLATE_FILETYPE_PASCAL,
	GEANY_TEMPLATE_FILETYPE_RUBY,
	GEANY_TEMPLATE_FILETYPE_D,
	GEANY_TEMPLATE_FILETYPE_HTML,

	GEANY_MAX_TEMPLATES
};


#endif
