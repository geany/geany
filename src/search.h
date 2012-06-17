/*
 *      search.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 * @file search.h
 * Search (prefs).
 **/


#ifndef GEANY_SEARCH_H
#define GEANY_SEARCH_H 1

G_BEGIN_DECLS

/* the flags given in the search dialog for "find next", also used by the search bar */
typedef struct GeanySearchData
{
	gchar		*text;
	gint		flags;
	gboolean	backwards;
	/* set to TRUE when text was set by a search bar callback to keep track of
	 * search bar background colour */
	gboolean	search_bar;
	/* text as it was entered by user */
	gchar		*original_text;
}
GeanySearchData;

extern GeanySearchData search_data;


enum GeanyFindSelOptions
{
	GEANY_FIND_SEL_CURRENT_WORD,
	GEANY_FIND_SEL_X,
	GEANY_FIND_SEL_AGAIN
};

/** Search preferences */
typedef struct GeanySearchPrefs
{
	gboolean	always_wrap;			/* don't ask whether to wrap search */
	gboolean	use_current_word;		/**< Use current word for default search text */
	gboolean	use_current_file_dir;	/* find in files directory to use on showing dialog */
	gboolean	hide_find_dialog;		/* hide the find dialog on next or previous */
	enum GeanyFindSelOptions find_selection_type;
}
GeanySearchPrefs;

extern GeanySearchPrefs search_prefs;


void search_init(void);

void search_finalize(void);

void search_show_find_dialog(void);

void search_show_replace_dialog(void);

void search_show_find_in_files_dialog(const gchar *dir);


struct _ScintillaObject;
struct Sci_TextToFind;

gint search_find_next(struct _ScintillaObject *sci, const gchar *str, gint flags);

gint search_find_text(struct _ScintillaObject *sci, gint flags, struct Sci_TextToFind *ttf);

void search_find_again(gboolean change_direction);

void search_find_usage(const gchar *search_text, const gchar *original_search_text, gint flags, gboolean in_session);

void search_find_selection(GeanyDocument *doc, gboolean search_backwards);

gint search_mark_all(GeanyDocument *doc, const gchar *search_text, gint flags);

gint search_replace_target(struct _ScintillaObject *sci, const gchar *replace_text,
	gboolean regex);

guint search_replace_range(struct _ScintillaObject *sci, struct Sci_TextToFind *ttf,
		gint flags, const gchar *replace_text);

G_END_DECLS

#endif
