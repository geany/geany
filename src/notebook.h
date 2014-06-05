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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GEANY_NOTEBOOK_H
#define GEANY_NOTEBOOK_H 1

#include "document.h"
#include "geanypage.h"

#include <glib.h>
#include "Scintilla.h"
#include "ScintillaWidget.h"

G_BEGIN_DECLS

/* Returns an array of notebooks that are used by the UI */
GPtrArray* notebook_init(void);

void notebook_free(void);

/* Returns page number of notebook page, or -1 on error */
gint notebook_new_tab(GeanyPage *page, GtkNotebook *notebook);

/* Always use this instead of gtk_notebook_remove_page(). */
void notebook_remove_tab(GeanyPage *page);

void notebook_destroy_tab(GeanyPage *page);

void notebook_show_tab(GeanyPage *page);

/* Switch notebook to the last used tab. Can be called repeatedly to get to the
 * previous tabs. */
void notebook_switch_tablastused(void);

/* Returns TRUE when MRU tab switch is in progress (i.e. not at the final 
 * document yet). */
gboolean notebook_switch_in_progress(void);

/*
 * Returns the active notebook across all notebooks.
 * This is the notebook that contains the current document */
GtkNotebook *notebook_get_current_notebook(void);

/*
 * Get the order of two notebooks. Can be used as compare func for sorting functions.
 *
 * Returns 0 if both pointer are the same, -1 if notebook1 sorts before notebook2 and 1 if notebook2
 * sorts before notebook. */
gint notebook_order_compare(GtkNotebook *notebook1, GtkNotebook *notebook2);

/*
 * Returns the number of open tabs in all notebooks */
guint notebook_get_num_tabs(void);

/*
 * Overrides the paned position upon the next re-layout of the editor notebooks
 * (which happens when new tabs are added to an empty notebook or the last tab of a
 * notebook is closed). This is intended to restore the paned position from stored
 * configuration.
 */
void notebook_restore_paned_position(gint position);

/*
 * Moves the tab containing doc to the specified notebook */
gint notebook_move_tab(GeanyPage *page, GtkNotebook *new_notebook);

#define foreach_notebook(notebook)                                                 \
	for (gint __idx = 0; __idx < main_widgets.notebooks->len; ++__idx)             \
		if (((notebook) = g_ptr_array_index(main_widgets.notebooks, (__idx))) || 1)

#define notebook_get_primary() (g_ptr_array_index(main_widgets.notebooks, 0))

G_END_DECLS

#endif /* GEANY_NOTEBOOK_H */
