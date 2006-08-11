/*
 *      search.h
 *
 *      Copyright 2006 Nick Treleaven <nick.treleaven@btinternet.com>
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


typedef enum
{
	FIF_CASE_SENSITIVE 	= 1 << 0,
	FIF_USE_EREGEXP		= 1 << 1,
	FIF_INVERT_MATCH	= 1 << 2
} fif_options;

// the flags given in the search dialog for "find next"
typedef struct
{
	gchar		*text;
	gint		flags;
	gboolean	backwards;
} GeanySearchData;

extern GeanySearchData search_data;


void search_init();

void search_finalise();

void search_show_find_dialog();

void search_show_replace_dialog();

void search_show_find_in_files_dialog();

gboolean search_find_in_files(const gchar *search_text, const gchar *dir, fif_options opts);

