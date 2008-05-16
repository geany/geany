/*
 *      templates.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

enum
{
	GEANY_TEMPLATE_GPL = 0,
	GEANY_TEMPLATE_BSD,
	GEANY_TEMPLATE_FILEHEADER,
	GEANY_TEMPLATE_CHANGELOG,
	GEANY_TEMPLATE_FUNCTION,
	GEANY_MAX_TEMPLATES
};


typedef struct GeanyTemplatePrefs
{
	gchar			*developer;
	gchar			*company;
	gchar			*mail;
	gchar			*initials;
	gchar			*version;
}
GeanyTemplatePrefs;

extern GeanyTemplatePrefs template_prefs;


struct filetype;

void templates_init(void);

gchar *templates_get_template_fileheader(gint filetype_idx, const gchar *fname);

gchar *templates_get_template_new_file(struct filetype *ft);

gchar *templates_get_template_changelog(void);

gchar *templates_get_template_generic(gint template);

gchar *templates_get_template_function(gint filetype_idx, const gchar *func_name);

gchar *templates_get_template_licence(gint filetype_idx, gint licence_type);

void templates_free_templates(void);

#endif
