/*
 *      templates.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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

typedef struct _ClassInfo	ClassInfo;

void templates_init(void);

gchar *templates_get_template_fileheader(gint idx);

gchar *templates_get_template_new_file(filetype *ft);

gchar *templates_get_template_changelog(void);

gchar *templates_get_template_generic(gint template);

gchar *templates_get_template_function(gint filetype_idx, const gchar *func_name);

gchar *templates_get_template_licence(gint filetype_idx, gint licence_type);

gchar *templates_get_template_class_header(ClassInfo *class_info);

gchar *templates_get_template_class_source(ClassInfo *class_info);

void templates_free_templates(void);

enum
{
	GEANY_TEMPLATE_GPL = 0,
	GEANY_TEMPLATE_BSD,
	GEANY_TEMPLATE_FILEHEADER,
	GEANY_TEMPLATE_CHANGELOG,
	GEANY_TEMPLATE_FUNCTION,
	GEANY_MAX_TEMPLATES
};

enum
{
	GEANY_CLASS_TYPE_CPP,
	GEANY_CLASS_TYPE_GTK
};

struct _ClassInfo
{
	gint type;
	gchar *class_name;
	gchar *class_name_up;
	gchar *class_name_low;
	gchar *base_name;
	gchar *base_gtype;
	gchar *header;
	gchar *header_guard;
	gchar *base_include;
	gchar *base_decl;
	gchar *constructor_decl;
	gchar *destructor_decl;
	gchar *source;
	gchar *constructor_impl;
	gchar *destructor_impl;
	gchar *gtk_destructor_registration;
};

#endif
