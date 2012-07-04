/*
 *      notebook.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef GEANY_NOTEBOOK_H
#define GEANY_NOTEBOOK_H 1

void notebook_init(void);

void notebook_free(void);

/* Returns page number of notebook page, or -1 on error */
gint notebook_new_tab(GeanyDocument *doc);

/* Always use this instead of gtk_notebook_remove_page(). */
void notebook_remove_page(gint page_num);

/* Switch notebook to the last used tab. Can be called repeatedly to get to the
 * previous tabs. */
void notebook_switch_tablastused(void);

/* Returns TRUE when MRU tab switch is in progress (i.e. not at the final 
 * document yet). */
gboolean notebook_switch_in_progress(void);

#endif
