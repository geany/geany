/*
 *      search.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2007 Enrico Tröger <enrico.troeger@uvena.de>
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


#ifndef GEANY_SEARCH_H
#define GEANY_SEARCH_H 1

// the flags given in the search dialog for "find next"
typedef struct
{
	gchar		*text;
	gint		flags;
	gboolean	backwards;
} GeanySearchData;

extern GeanySearchData search_data;


typedef struct
{
	gchar		*fif_extra_options;
} SearchPrefs;

extern SearchPrefs search_prefs;


void search_init();

void search_finalize();

void search_show_find_dialog();

void search_show_replace_dialog();

void search_show_find_in_files_dialog();

void search_find_usage(const gchar *search_text, gint flags, gboolean in_session);

void search_find_selection(gint idx, gboolean search_backwards);

#endif
