/*
 *      editor.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#ifndef GEANY_EDITOR_H
#define GEANY_EDITOR_H 1

#include "tm_tag.h" /* for TMTag */

#include "gtkcompat.h" /* Needed by ScintillaWidget.h */
#include "Scintilla.h" /* Needed by ScintillaWidget.h */
#include "ScintillaWidget.h" /* for ScintillaObject */

#include <glib.h>


G_BEGIN_DECLS

/* Forward-declared to avoid including document.h since it includes this header */
struct GeanyDocument;

/** Default character set to define which characters should be treated as part of a word. */
#define GEANY_WORDCHARS					"_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define GEANY_MAX_WORD_LENGTH			192

/** Whether to use tabs, spaces or both to indent. */
typedef enum
{
	GEANY_INDENT_TYPE_SPACES,	/**< Spaces. */
	GEANY_INDENT_TYPE_TABS,		/**< Tabs. */
	GEANY_INDENT_TYPE_BOTH		/**< Both. */
}
GeanyIndentType;

/** @gironly
 * Auto indentation modes */
typedef enum
{
	GEANY_AUTOINDENT_NONE = 0,
	GEANY_AUTOINDENT_BASIC,
	GEANY_AUTOINDENT_CURRENTCHARS,
	GEANY_AUTOINDENT_MATCHBRACES
}
GeanyAutoIndent;

/** Geany indicator types, can be used with Editor indicator functions to highlight
 *  text in the document. */
typedef enum
{
	/** Indicator to highlight errors in the document text. This is a red squiggly underline. */
	GEANY_INDICATOR_ERROR = 0,
	/** Indicator used to highlight search results in the document. This is a
	 *  rounded box around the text. */
	/* start container indicator outside of lexer indicators (0..7), see Scintilla docs */
	GEANY_INDICATOR_SEARCH = 8,
	GEANY_INDICATOR_SNIPPET = 9
}
GeanyIndicator;

/** Indentation prefs that might be different according to project or filetype.
 * Use @c editor_get_indent_prefs() to lookup the prefs for a particular document.
 *
 * @since 0.15
 **/
typedef struct GeanyIndentPrefs
{
	gint			width;				/**< Indent width. */
	GeanyIndentType	type;				/**< Whether to use tabs, spaces or both to indent. */
	/** Width of a tab, but only when using GEANY_INDENT_TYPE_BOTH.
	 * To get the display tab width, use sci_get_tab_width(). */
	gint			hard_tab_width;
	GeanyAutoIndent	auto_indent_mode;
	gboolean		detect_type;
	gboolean		detect_width;
}
GeanyIndentPrefs;

/** Default prefs when creating a new editor window.
 * Some of these can be overridden per document or per project. */
/* @warning Use @c editor_get_prefs() instead to include project overrides. */
typedef struct GeanyEditorPrefs
{
	GeanyIndentPrefs *indentation;	/* Default indentation prefs. Use editor_get_indent_prefs(). */
	gboolean	show_white_space;
	gboolean	show_indent_guide;
	gboolean	show_line_endings;
	/* 0 - line, 1 - background, 2 - disabled. */
	gint		long_line_type;
	gint		long_line_column;
	gchar		*long_line_color;
	gboolean	show_markers_margin;		/* view menu */
	gboolean	show_linenumber_margin;		/* view menu */
	gboolean	show_scrollbars;			/* hidden pref */
	gboolean	scroll_stop_at_last_line;
	gboolean	line_wrapping;
	gboolean	use_indicators;
	gboolean	folding;
	gboolean	unfold_all_children;
	gboolean	disable_dnd;
	gboolean	use_tab_to_indent;	/* makes tab key indent instead of insert a tab char */
	gboolean	smart_home_key;
	gboolean	newline_strip;
	gboolean	auto_complete_symbols;
	gboolean	auto_close_xml_tags;
	gboolean	complete_snippets;
	gint		symbolcompletion_min_chars;
	gint		symbolcompletion_max_height;
	gboolean	brace_match_ltgt;	/* whether to highlight < and > chars (hidden pref) */
	gboolean	use_gtk_word_boundaries;	/* hidden pref */
	gboolean	complete_snippets_whilst_editing;	/* hidden pref */
	gint		line_break_column;
	gboolean	auto_continue_multiline;
	gchar		*comment_toggle_mark;
	guint		autocompletion_max_entries;
	guint		autoclose_chars;
	gboolean	autocomplete_doc_words;
	gboolean	completion_drops_rest_of_word;
	gchar		*color_scheme;
	gint 		show_virtual_space;
	gboolean	long_line_enabled;
	gint		autocompletion_update_freq;
	gint		scroll_lines_around_cursor;
	gint		ime_interaction; /* input method editor's candidate window behaviour */
}
GeanyEditorPrefs;


#define GEANY_TYPE_EDITOR (editor_get_type())
GType editor_get_type (void);

/** Editor-owned fields for each document. */
typedef struct GeanyEditor
{
	struct GeanyDocument	*document;		/**< The document associated with the editor. */
	ScintillaObject	*sci;			/**< The Scintilla editor @c GtkWidget. */
	gboolean		 line_wrapping;	/**< @c TRUE if line wrapping is enabled. */
	gboolean		 auto_indent;	/**< @c TRUE if auto-indentation is enabled. */
	/** Percentage to scroll view by on paint, if positive. */
	gfloat			 scroll_percent;
	GeanyIndentType	 indent_type;	/* Use editor_get_indent_prefs() instead. */
	gboolean		 line_breaking;	/**< Whether to split long lines as you type. */
	gint			 indent_width;
}
GeanyEditor;


const GeanyIndentPrefs *editor_get_indent_prefs(GeanyEditor *editor);

ScintillaObject *editor_create_widget(GeanyEditor *editor);

void editor_indicator_set_on_range(GeanyEditor *editor, gint indic, gint start, gint end);

void editor_indicator_set_on_line(GeanyEditor *editor, gint indic, gint line);

void editor_indicator_clear(GeanyEditor *editor, gint indic);

void editor_set_indent_type(GeanyEditor *editor, GeanyIndentType type);

void editor_set_indent_width(GeanyEditor *editor, gint width);

gchar *editor_get_word_at_pos(GeanyEditor *editor, gint pos, const gchar *wordchars);

const gchar *editor_get_eol_char_name(GeanyEditor *editor);

gint editor_get_eol_char_len(GeanyEditor *editor);

const gchar *editor_get_eol_char(GeanyEditor *editor);

void editor_insert_text_block(GeanyEditor *editor, const gchar *text,
	 						  gint insert_pos, gint cursor_index,
	 						  gint newline_indent_size, gboolean replace_newlines);

gint editor_get_eol_char_mode(GeanyEditor *editor);

gboolean editor_goto_pos(GeanyEditor *editor, gint pos, gboolean mark);

const gchar *editor_find_snippet(GeanyEditor *editor, const gchar *snippet_name);

void editor_insert_snippet(GeanyEditor *editor, gint pos, const gchar *snippet);


#ifdef GEANY_PRIVATE

extern GeanyEditorPrefs editor_prefs;

typedef enum
{
	GEANY_VIRTUAL_SPACE_DISABLED = 0,
	GEANY_VIRTUAL_SPACE_SELECTION = 1,
	GEANY_VIRTUAL_SPACE_ALWAYS = 3
}
GeanyVirtualSpace;

/* Auto-close brackets/quotes */
enum {
	GEANY_AC_PARENTHESIS	= 1,
	GEANY_AC_CBRACKET		= 2,
	GEANY_AC_SBRACKET		= 4,
	GEANY_AC_SQUOTE			= 8,
	GEANY_AC_DQUOTE			= 16
};

typedef struct
{
	gchar	*current_word;	/* holds word under the mouse or keyboard cursor */
	gint	click_pos;		/* text position where the mouse was clicked */
} EditorInfo;

extern EditorInfo editor_info;


void editor_init(void);

GeanyEditor *editor_create(struct GeanyDocument *doc);

void editor_destroy(GeanyEditor *editor);

void editor_sci_notify_cb(GtkWidget *widget, gint scn, gpointer scnt, gpointer data);

gboolean editor_start_auto_complete(GeanyEditor *editor, gint pos, gboolean force);

gboolean editor_complete_word_part(GeanyEditor *editor);

gboolean editor_goto_next_snippet_cursor(GeanyEditor *editor);

gboolean editor_complete_snippet(GeanyEditor *editor, gint pos);

gboolean editor_show_calltip(GeanyEditor *editor, gint pos);

void editor_do_comment_toggle(GeanyEditor *editor);

gint editor_do_comment(GeanyEditor *editor, gint line, gboolean allow_empty_lines, gboolean toggle,
		gboolean single_comment);

gint editor_do_uncomment(GeanyEditor *editor, gint line, gboolean toggle);

void editor_insert_multiline_comment(GeanyEditor *editor);

void editor_insert_alternative_whitespace(GeanyEditor *editor);

void editor_indent(GeanyEditor *editor, gboolean increase);

void editor_smart_line_indentation(GeanyEditor *editor, gint pos);

void editor_indentation_by_one_space(GeanyEditor *editor, gint pos, gboolean decrease);

gboolean editor_line_in_view(GeanyEditor *editor, gint line);

void editor_scroll_to_line(GeanyEditor *editor, gint line, gfloat percent_of_view);

void editor_display_current_line(GeanyEditor *editor, gfloat percent_of_view);

void editor_finalize(void);

void editor_snippets_init(void);

void editor_snippets_free(void);

const GeanyEditorPrefs *editor_get_prefs(GeanyEditor *editor);


/* General editing functions */

void editor_find_current_word(GeanyEditor *editor, gint pos, gchar *word, gsize wordlen,
	const gchar *wc);

void editor_find_current_word_sciwc(GeanyEditor *editor, gint pos, gchar *word, gsize wordlen);

gchar *editor_get_default_selection(GeanyEditor *editor, gboolean use_current_word, const gchar *wordchars);


void editor_select_word(GeanyEditor *editor);

void editor_select_lines(GeanyEditor *editor, gboolean extra_line);

void editor_select_paragraph(GeanyEditor *editor);

void editor_select_indent_block(GeanyEditor *editor);


void editor_set_font(GeanyEditor *editor, const gchar *font);

void editor_indicator_clear_errors(GeanyEditor *editor);

void editor_fold_all(GeanyEditor *editor);

void editor_unfold_all(GeanyEditor *editor);

void editor_replace_tabs(GeanyEditor *editor, gboolean ignore_selection);

void editor_replace_spaces(GeanyEditor *editor, gboolean ignore_selection);

void editor_strip_line_trailing_spaces(GeanyEditor *editor, gint line);

void editor_strip_trailing_spaces(GeanyEditor *editor, gboolean ignore_selection);

void editor_ensure_final_newline(GeanyEditor *editor);

void editor_insert_color(GeanyEditor *editor, const gchar *colour);

void editor_set_indent(GeanyEditor *editor, GeanyIndentType type, gint width);

void editor_set_line_wrapping(GeanyEditor *editor, gboolean wrap);

gboolean editor_goto_line(GeanyEditor *editor, gint line_no, gint offset);

void editor_set_indentation_guides(GeanyEditor *editor);

void editor_apply_update_prefs(GeanyEditor *editor);

gchar *editor_get_calltip_text(GeanyEditor *editor, const TMTag *tag);

void editor_toggle_fold(GeanyEditor *editor, gint line, gint modifiers);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_EDITOR_H */
