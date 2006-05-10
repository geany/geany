/*
 *      document.h - this file is part of Geany, a fast and lightweight IDE
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
 *  $Id$
 */


#ifndef GEANY_DOCUMENT_H
#define GEANY_DOCUMENT_H 1


/* returns the index of the notebook page which has the given filename */
gint document_find_by_filename(const gchar*);


/* returns the index of the notebook page which has sci */
gint document_find_by_sci(ScintillaObject*);


/* returns the index of the current notebook page in the document list */
gint document_get_cur_idx(void);


/* returns the index of the given notebook page in the document list */
gint document_get_n_idx(guint);


/* returns the next free place(i.e. index) in the document list
 * If there is for any reason no free place, -1 is returned */
gint document_get_new_idx(void);


/* changes the color of the tab text according to the status */
void document_change_tab_color(gint);


void document_set_text_changed(gint);


/* sets in all document structs the flag is_valid to FALSE and initializes some members to NULL,
 * to mark it uninitialized. The flag is_valid is set to TRUE in document_create_new_sci(). */
void document_init_doclist(void);


/* creates a new tab in the notebook and does all related stuff
 * finally it returns the index of the created document */
gint document_create_new_sci(const gchar*);


/* removes the given notebook tab and clears the related entry in the document
 * list */
gboolean document_remove(guint);


/* This creates a new document, by clearing the text widget and setting the
   current filename to NULL. */
void document_new_file(filetype *ft);


/* If idx is set to -1, it creates a new tab, opens the file from filename and
 * set the cursor to pos.
 * If idx is greater than -1, it reloads the file in the tab corresponding to
 * idx and set the cursor to position 0.
 */
void document_open_file(gint, const gchar*, gint, gboolean, filetype*);


/* This saves the file, which is in on-disk encoding (which may not
   be UTF-8). */
void document_save_file (gint);


void document_find_text(gint, const gchar*, gint, gboolean);

void document_replace_text(gint, const gchar*, const gchar*, gint, gboolean);

void document_replace_all(gint, const gchar*, const gchar*, gint);

void document_replace_sel(gint, const gchar*, const gchar*, gint, gboolean);

void document_find_next(gint, const gchar*, gint, gboolean, gboolean);


void document_set_font(ScintillaObject*, const gchar*, gint);


void document_update_tag_list(gint, gboolean);


void document_set_filetype(gint, filetype*);

gchar *document_get_eol_mode(gint);

gchar *document_prepare_template(filetype *ft);

void document_fold_all(gint idx);

void document_unfold_all(gint idx);

#endif
