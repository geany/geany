/*
 *      editor.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */

#ifndef GEANY_SCI_CB_H
#define GEANY_SCI_CB_H 1

#ifndef PLAT_GTK
#   define PLAT_GTK 1	// needed for ScintillaWidget.h
#endif

#include "Scintilla.h"
#include "ScintillaWidget.h"

// Note: Avoid using SSM in files not related to scintilla, use sciwrappers.h instead.
#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)


typedef enum
{
	INDENT_NONE = 0,
	INDENT_BASIC,
	INDENT_ADVANCED
} IndentMode;

/* These are the default prefs when creating a new editor window.
 * Some of these can be overridden per document. */
typedef struct
{
	gboolean	line_breaking;
	IndentMode	indent_mode;
	gboolean	use_indicators;
	gboolean	show_white_space;
	gboolean	show_indent_guide;
	gboolean	show_line_endings;
	gboolean	auto_complete_symbols;
	gboolean	auto_close_xml_tags;
	gboolean	auto_complete_constructs;
	gboolean	folding;
	gboolean	unfold_all_children;
	gboolean	show_scrollbars;
	gint		tab_width;
	gint		caret_blink_time;
	gboolean	use_tabs;
	gint		default_new_encoding;
	gint		default_open_encoding;
	gboolean	new_line;
	gboolean	replace_tabs;
	gboolean	trail_space;
	gboolean	disable_dnd;
	GHashTable	*auto_completions;
} EditorPrefs;

extern EditorPrefs editor_prefs;


typedef struct
{
	gchar *current_word;	// holds word under the mouse or keyboard cursor
	gint click_pos;			// text position where the mouse was clicked
} EditorInfo;

extern EditorInfo editor_info;


typedef struct
{
	gchar *type;	// represents in most cases the filetype(two exceptions: default and special)
	gchar *name;	// name of the key in config file, represents the entered text to complete
	gchar *value;
} AutoCompletion;



gboolean
on_editor_button_press_event           (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data);

// callback func called by all editors when a signal arises
void on_editor_notification(GtkWidget* editor, gint scn, gpointer lscn, gpointer user_data);

gboolean editor_start_auto_complete(gint idx, gint pos, gboolean force);

void editor_close_block(gint idx, gint pos);

gboolean editor_auto_forif(gint idx, gint pos);

void editor_auto_latex(gint idx, gint pos);

void editor_show_macro_list(ScintillaObject *sci);

/* Reads the word at given cursor position and writes it into the given buffer. The buffer will be
 * NULL terminated in any case, even when the word is truncated because wordlen is too small.
 * position can be -1, then the current position is used.
 * wc are the wordchars to use, if NULL, GEANY_WORDCHARS will be used */
void editor_find_current_word(ScintillaObject *sci, gint pos, gchar *word, size_t wordlen,
							  const gchar *wc);

gboolean editor_show_calltip(gint idx, gint pos);

void editor_do_comment_toggle(gint idx);

void editor_do_comment(gint idx, gint line, gboolean allow_empty_lines);

void editor_do_uncomment(gint idx, gint line);

void editor_highlight_braces(ScintillaObject *sci, gint cur_pos);

void editor_auto_table(ScintillaObject *sci, gint pos);

gboolean editor_lexer_is_c_like(gint lexer);

gint editor_lexer_get_type_keyword_idx(gint lexer);

void editor_insert_multiline_comment(gint idx);

void editor_select_word(ScintillaObject *sci);

void editor_insert_alternative_whitespace(ScintillaObject *sci);

void editor_finalize();

#endif
