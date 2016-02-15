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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * @file search.h
 * Search (prefs).
 **/


#ifndef GEANY_SEARCH_H
#define GEANY_SEARCH_H 1

#include <glib.h>


G_BEGIN_DECLS

typedef enum GeanyFindFlags
{
	GEANY_FIND_MATCHCASE = 1 << 0,
	GEANY_FIND_WHOLEWORD = 1 << 1,
	GEANY_FIND_WORDSTART = 1 << 2,
	GEANY_FIND_REGEXP    = 1 << 3,
	GEANY_FIND_MULTILINE = 1 << 4
}
GeanyFindFlags;

/** @gironly
 * Find selection options */
typedef enum
{
	GEANY_FIND_SEL_CURRENT_WORD,
	GEANY_FIND_SEL_X,
	GEANY_FIND_SEL_AGAIN
}
GeanyFindSelOptions;

/** Search preferences */
typedef struct GeanySearchPrefs
{
	gboolean	always_wrap;			/* don't ask whether to wrap search */
	gboolean	use_current_word;		/**< Use current word for default search text */
	gboolean	use_current_file_dir;	/* find in files directory to use on showing dialog */
	gboolean	hide_find_dialog;		/* hide the find dialog on next or previous */
	gboolean	replace_and_find_by_default;	/* enter in replace window performs Replace & Find instead of Replace */
	GeanyFindSelOptions find_selection_type;
}
GeanySearchPrefs;

typedef struct GeanyMatchInfo
{
	GeanyFindFlags flags;
	/* range */
	gint start, end;
	/* only valid if (flags & GEANY_FIND_REGEX) */
	gchar *match_text; /* text actually matched */
	struct
	{
		gint start, end;
	}
	matches[10]; /* sub-patterns */
}
GeanyMatchInfo;

void search_show_find_in_files_dialog(const gchar *dir);


#ifdef GEANY_PRIVATE

struct GeanyDocument; /* document.h includes this header */
struct _ScintillaObject;
struct Sci_TextToFind;


/* the flags given in the search dialog for "find next", also used by the search bar */
typedef struct GeanySearchData
{
	gchar			*text;
	GeanyFindFlags	flags;
	gboolean		backwards;
	/* set to TRUE when text was set by a search bar callback to keep track of
	 * search bar background colour */
	gboolean		search_bar;
	/* text as it was entered by user */
	gchar			*original_text;
}
GeanySearchData;

extern GeanySearchData search_data;

extern GeanySearchPrefs search_prefs;


void search_init(void);

void search_finalize(void);

void search_show_find_dialog(void);

void search_show_replace_dialog(void);

void search_show_find_in_files_dialog_full(const gchar *text, const gchar *dir);

void geany_match_info_free(GeanyMatchInfo *info);

gint search_find_prev(struct _ScintillaObject *sci, const gchar *str, GeanyFindFlags flags, GeanyMatchInfo **match_);

gint search_find_next(struct _ScintillaObject *sci, const gchar *str, GeanyFindFlags flags, GeanyMatchInfo **match_);

gint search_find_text(struct _ScintillaObject *sci, GeanyFindFlags flags, struct Sci_TextToFind *ttf, GeanyMatchInfo **match_);

void search_find_again(gboolean change_direction);

void search_find_usage(const gchar *search_text, const gchar *original_search_text, GeanyFindFlags flags, gboolean in_session);

void search_find_selection(struct GeanyDocument *doc, gboolean search_backwards);

gint search_mark_all(struct GeanyDocument *doc, const gchar *search_text, GeanyFindFlags flags);

gint search_replace_match(struct _ScintillaObject *sci, const GeanyMatchInfo *match, const gchar *replace_text);

guint search_replace_range(struct _ScintillaObject *sci, struct Sci_TextToFind *ttf,
		GeanyFindFlags flags, const gchar *replace_text);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_SEARCH_H */
