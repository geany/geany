/*
 *      plugindata.h - this file is part of Geany, a fast and lightweight IDE
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

#ifndef PLUGIN_H
#define PLUGIN_H

/* The API version should be incremented whenever any plugin data types below are
 * modified. */
static const gint api_version = 3;

/* The ABI version should be incremented whenever existing fields in the plugin
 * data types below have to be changed or reordered. It should stay the same if fields
 * are only appended, as this doesn't affect existing fields. */
static const gint abi_version = 2;

/* This performs runtime checks that try to ensure:
 * 1. Geany ABI data types are compatible with this plugin.
 * 2. Geany sources provide the required API for this plugin. */
/* TODO: if possible, the API version should be checked at compile time, not runtime. */
#define VERSION_CHECK(api_required) \
	gint version_check(gint abi_ver) \
	{ \
		if (abi_ver != abi_version) \
			return -1; \
		if (api_version < (api_required)) \
			return (api_required); \
		else return 0; \
	}


typedef struct PluginInfo
{
	gchar	*name;			// name of plugin
	gchar	*description;	// description of plugin
}
PluginInfo;

/* Sets the plugin name and a brief description of what it is. */
#define PLUGIN_INFO(p_name, p_description) \
	PluginInfo *info() \
	{ \
		static PluginInfo p_info; \
		 \
		p_info.name = (p_name); \
		p_info.description = (p_description); \
		return &p_info; \
	}


typedef struct DocumentFuncs	DocumentFuncs;
typedef struct ScintillaFuncs	ScintillaFuncs;
typedef struct TemplateFuncs	TemplateFuncs;
typedef struct UtilsFuncs		UtilsFuncs;

/* These are fields and functions owned by Geany.
 * Fields will be appended when needed by plugin authors.
 * Note: Remember to increment api_version (and abi_version if necessary) when
 * making changes. */
typedef struct PluginData
{
	MyApp	*app;	// Geany application data fields

	GtkWidget	*tools_menu;	// Almost all plugins should add menu items to the Tools menu only

	GArray		*doc_array;	// array of document pointers

	DocumentFuncs	*document;
	ScintillaFuncs	*sci;
	TemplateFuncs	*templates;
	UtilsFuncs		*utils;
}
PluginData;


struct filetype;

struct DocumentFuncs
{
	gint (*new_file) (const gchar *filename, struct filetype *ft);
};

struct _ScintillaObject;

struct ScintillaFuncs
{
	void (*set_text) (struct _ScintillaObject *sci, const gchar *text);
};

struct TemplateFuncs
{
	gchar* (*get_template_fileheader) (gint filetype_idx, const gchar *fname);
};

struct UtilsFuncs
{
	gboolean (*str_equal) (const gchar *a, const gchar *b);
	gchar* (*str_replace) (gchar *haystack, const gchar *needle, const gchar *replacement);
};

#endif
