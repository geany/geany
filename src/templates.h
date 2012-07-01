/*
 *      templates.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 */

/**
 * @file templates.h
 * Templates (prefs).
 **/


#ifndef GEANY_TEMPLATES_H
#define GEANY_TEMPLATES_H 1

G_BEGIN_DECLS

#define GEANY_TEMPLATES_INDENT 3

enum
{
	GEANY_TEMPLATE_GPL = 0,
	GEANY_TEMPLATE_BSD,
	GEANY_TEMPLATE_FILEHEADER,
	GEANY_TEMPLATE_CHANGELOG,
	GEANY_TEMPLATE_FUNCTION,
	GEANY_MAX_TEMPLATES
};


/** Template preferences. */
typedef struct GeanyTemplatePrefs
{
	gchar			*developer;	/**< Name */
	gchar			*company;	/**< Company */
	gchar			*mail;		/**< Email */
	gchar			*initials;	/**< Initials */
	gchar			*version;	/**< Initial version */
	gchar			*year_format;
	gchar			*date_format;
	gchar			*datetime_format;
}
GeanyTemplatePrefs;

extern GeanyTemplatePrefs template_prefs;


struct filetype;

void templates_init(void);

gchar *templates_get_template_fileheader(gint filetype_idx, const gchar *fname);

gchar *templates_get_template_changelog(GeanyDocument *doc);

gchar *templates_get_template_function(GeanyDocument *doc, const gchar *func_name);

gchar *templates_get_template_licence(GeanyDocument *doc, gint licence_type);

void templates_replace_common(GString *tmpl, const gchar *fname,
	GeanyFiletype *ft, const gchar *func_name);

void templates_replace_valist(GString *text,
	const gchar *first_wildcard, ...) G_GNUC_NULL_TERMINATED;

void templates_free_templates(void);

G_END_DECLS

#endif
