/*
 *      printing.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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


#ifndef GEANY_PRINTING_H
#define GEANY_PRINTING_H 1


/* General printing preferences. */
typedef struct PrintingPrefs
{
	gboolean use_gtk_printing;
	gboolean print_line_numbers;
	gboolean print_page_numbers;
	gboolean print_page_header;
	gboolean page_header_basename;
	gchar *page_header_datefmt;
	gchar *external_print_cmd;
} PrintingPrefs;

extern PrintingPrefs printing_prefs;


void printing_page_setup_gtk(void);

void printing_print_doc(GeanyDocument *doc);

#endif
