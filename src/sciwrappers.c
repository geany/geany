/*
 *      sciwrappers.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/** @file sciwrappers.h
 * Wrapper functions for the Scintilla editor widget @c SCI_* messages.
 * You should also check the http://scintilla.org documentation, as it is more detailed.
 *
 * To get Scintilla notifications, use the
 * @link pluginsignals.c @c "editor-notify" signal @endlink.
 *
 * @note These functions were originally from the cssed project
 * (http://cssed.sf.net, thanks).
 * @see scintilla_send_message().
 */

#include <string.h>

#include "geany.h"

#include "sciwrappers.h"
#include "utils.h"

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)


/* line numbers visibility */
void sci_set_line_numbers(ScintillaObject *sci, gboolean set, gint extra_width)
{
	if (set)
	{
		gchar tmp_str[15];
		gint len = SSM(sci, SCI_GETLINECOUNT, 0, 0);
		gint width;

		g_snprintf(tmp_str, 15, "_%d", len);
		width = sci_text_width(sci, STYLE_LINENUMBER, tmp_str);
		if (extra_width)
		{
			g_snprintf(tmp_str, 15, "%d", extra_width);
			width += sci_text_width(sci, STYLE_LINENUMBER, tmp_str);
		}
		SSM(sci, SCI_SETMARGINWIDTHN, 0, width);
		SSM(sci, SCI_SETMARGINSENSITIVEN, 0, FALSE); /* use default behaviour */
	}
	else
	{
		SSM(sci, SCI_SETMARGINWIDTHN, 0, 0);
	}
}


void sci_set_mark_long_lines(ScintillaObject *sci, gint type, gint column, const gchar *colour)
{
	if (column == 0)
		type = 2;
	switch (type)
	{
		case 0:
		{
			SSM(sci, SCI_SETEDGEMODE, EDGE_LINE, 0);
			break;
		}
		case 1:
		{
			SSM(sci, SCI_SETEDGEMODE, EDGE_BACKGROUND, 0);
			break;
		}
		case 2:
		{
			SSM(sci, SCI_SETEDGEMODE, EDGE_NONE, 0);
			return;
		}
	}
	SSM(sci, SCI_SETEDGECOLUMN, column, 0);
	SSM(sci, SCI_SETEDGECOLOUR, utils_strtod(colour, NULL, TRUE), 0);
}


/* symbol margin visibility */
void sci_set_symbol_margin(ScintillaObject *sci, gboolean set)
{
	if (set)
	{
		SSM(sci, SCI_SETMARGINWIDTHN, 1, 16);
		SSM(sci, SCI_SETMARGINSENSITIVEN, 1, TRUE);
	}
	else
	{
		SSM(sci, SCI_SETMARGINWIDTHN, 1, 0);
		SSM(sci, SCI_SETMARGINSENSITIVEN, 1, FALSE);
	}
}


/* folding margin visibility */
void sci_set_folding_margin_visible(ScintillaObject *sci, gboolean set)
{
	if (set)
	{
		SSM(sci, SCI_SETMARGINWIDTHN, 2, 12);
		SSM(sci, SCI_SETMARGINSENSITIVEN, 2, TRUE);
	}
	else
	{
		SSM(sci, SCI_SETMARGINSENSITIVEN, 2, FALSE);
		SSM(sci, SCI_SETMARGINWIDTHN, 2, 0);
	}
}


/* end of lines */
void sci_set_visible_eols(ScintillaObject *sci, gboolean set)
{
	SSM(sci, SCI_SETVIEWEOL, set, 0);
}


void sci_set_visible_white_spaces(ScintillaObject *sci, gboolean set)
{
	if (set)
		SSM(sci, SCI_SETVIEWWS, SCWS_VISIBLEALWAYS, 0);
	else
		SSM(sci, SCI_SETVIEWWS, SCWS_INVISIBLE, 0);
}


void sci_set_lines_wrapped(ScintillaObject *sci, gboolean set)
{
	if (set)
		SSM(sci, SCI_SETWRAPMODE, SC_WRAP_WORD, 0);
	else
		SSM(sci, SCI_SETWRAPMODE, SC_WRAP_NONE, 0);
}


gint sci_get_eol_mode(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETEOLMODE, 0, 0);
}


void sci_set_eol_mode(ScintillaObject *sci, gint eolmode)
{
	SSM(sci, SCI_SETEOLMODE, eolmode, 0);
}


void sci_convert_eols(ScintillaObject *sci, gint eolmode)
{
	SSM(sci, SCI_CONVERTEOLS, eolmode, 0);
}


void sci_add_text(ScintillaObject *sci, const gchar *text)
{
	if (text != NULL)
	{ /* if null text is passed scintilla will segfault */
		SSM(sci, SCI_ADDTEXT, strlen(text), (sptr_t) text);
	}
}


/** Sets all text.
 * @param sci Scintilla widget.
 * @param text Text. */
void sci_set_text(ScintillaObject *sci, const gchar *text)
{
	if( text != NULL ){ /* if null text is passed to scintilla will segfault */
		SSM(sci, SCI_SETTEXT, 0, (sptr_t) text);
	}
}


gboolean sci_can_undo(ScintillaObject *sci)
{
	return SSM(sci, SCI_CANUNDO, 0, 0);
}


gboolean sci_can_redo(ScintillaObject *sci)
{
	return SSM(sci, SCI_CANREDO, 0, 0);
}


void sci_undo(ScintillaObject *sci)
{
	if (sci_can_undo(sci))
		SSM(sci, SCI_UNDO, 0, 0);
}


void sci_redo(ScintillaObject *sci)
{
	if (sci_can_redo(sci))
		SSM(sci, SCI_REDO, 0, 0);
}


/** Begins grouping a set of edits together as one Undo action.
 * You must call sci_end_undo_action() after making your edits.
 * @param sci Scintilla @c GtkWidget. */
void sci_start_undo_action(ScintillaObject *sci)
{
	SSM(sci, SCI_BEGINUNDOACTION, 0, 0);
}


/** Ends grouping a set of edits together as one Undo action.
 * @param sci Scintilla @c GtkWidget.
 * @see sci_start_undo_action(). */
void sci_end_undo_action(ScintillaObject *sci)
{
	SSM(sci, SCI_ENDUNDOACTION, 0, 0);
}


void sci_set_undo_collection(ScintillaObject *sci, gboolean set)
{
	SSM(sci, SCI_SETUNDOCOLLECTION, set, 0);
}


void sci_empty_undo_buffer(ScintillaObject *sci)
{
	SSM(sci, SCI_EMPTYUNDOBUFFER, 0, 0);
}


gboolean sci_is_modified(ScintillaObject *sci)
{
	return (SSM(sci, SCI_GETMODIFY, 0, 0) != 0);
}


void sci_zoom_in(ScintillaObject *sci)
{
	SSM(sci, SCI_ZOOMIN, 0, 0);
}


void sci_zoom_out(ScintillaObject *sci)
{
	SSM(sci, SCI_ZOOMOUT, 0, 0);
}


void sci_zoom_off(ScintillaObject *sci)
{
	SSM(sci, SCI_SETZOOM, 0, 0);
}


gint sci_get_zoom(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETZOOM, 0, 0);
}


/** Sets a line marker.
 * @param sci Scintilla widget.
 * @param line_number Line number.
 * @param marker Marker number. */
void sci_set_marker_at_line(ScintillaObject *sci, gint line_number, gint marker)
{
	SSM(sci, SCI_MARKERADD, line_number, marker);
}


/** Deletes a line marker.
 * @param sci Scintilla widget.
 * @param line_number Line number.
 * @param marker Marker number. */
void sci_delete_marker_at_line(ScintillaObject *sci, gint line_number, gint marker)
{
	SSM(sci, SCI_MARKERDELETE, line_number, marker);
}


/** Checks if a line has a marker set.
 * @param sci Scintilla widget.
 * @param line Line number.
 * @param marker Marker number.
 * @return Whether it's set. */
gboolean sci_is_marker_set_at_line(ScintillaObject *sci, gint line, gint marker)
{
	gint state;

	state = SSM(sci, SCI_MARKERGET, line, 0);
	return (state & (1 << marker));
}


void sci_toggle_marker_at_line(ScintillaObject *sci, gint line, gint marker)
{
	gboolean set = sci_is_marker_set_at_line(sci, line, marker);

	if (!set)
		sci_set_marker_at_line(sci, line, marker);
	else
		sci_delete_marker_at_line(sci, line, marker);
}


/* Returns the line number of the next marker that matches marker_mask, or -1.
 * marker_mask is a bitor of 1 << marker_index. (See MarkerHandleSet::MarkValue()).
 * Note: If there is a marker on the line, it returns the same line. */
gint sci_marker_next(ScintillaObject *sci, gint line, gint marker_mask, gboolean wrap)
{
	gint marker_line;

	marker_line = SSM(sci, SCI_MARKERNEXT, line, marker_mask);
	if (wrap && marker_line == -1)
		marker_line = SSM(sci, SCI_MARKERNEXT, 0, marker_mask);
	return marker_line;
}


/* Returns the line number of the previous marker that matches marker_mask, or -1.
 * marker_mask is a bitor of 1 << marker_index. (See MarkerHandleSet::MarkValue()).
 * Note: If there is a marker on the line, it returns the same line. */
gint sci_marker_previous(ScintillaObject *sci, gint line, gint marker_mask, gboolean wrap)
{
	gint marker_line;

	marker_line = SSM(sci, SCI_MARKERPREVIOUS, line, marker_mask);
	if (wrap && marker_line == -1)
	{
		gint len = sci_get_length(sci);
		gint last_line = sci_get_line_from_position(sci, len - 1);

		marker_line = SSM(sci, SCI_MARKERPREVIOUS, last_line, marker_mask);
	}
	return marker_line;
}


/** Gets the line number from @a position.
 * @param sci Scintilla widget.
 * @param position Position.
 * @return The line. */
gint sci_get_line_from_position(ScintillaObject *sci, gint position)
{
	return SSM(sci, SCI_LINEFROMPOSITION, position, 0);
}


/** Gets the column number relative to the start of the line that @a position is on.
 * @param sci Scintilla widget.
 * @param position Position.
 * @return The column. */
gint sci_get_col_from_position(ScintillaObject *sci, gint position)
{
	return SSM(sci, SCI_GETCOLUMN, position, 0);
}


/** Gets the position for the start of @a line.
 * @param sci Scintilla widget.
 * @param line Line.
 * @return Position. */
gint sci_get_position_from_line(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_POSITIONFROMLINE, line, 0);
}


/** Gets the cursor position.
 * @param sci Scintilla widget.
 * @return Position. */
gint sci_get_current_position(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETCURRENTPOS, 0, 0);
}


/** Sets the cursor position.
 * @param sci Scintilla widget.
 * @param position Position.
 * @param scroll_to_caret Whether to scroll the cursor in view. */
void sci_set_current_position(ScintillaObject *sci, gint position, gboolean scroll_to_caret)
{
	if (scroll_to_caret)
		SSM(sci, SCI_GOTOPOS, position, 0);
	else
	{
		SSM(sci, SCI_SETCURRENTPOS, position, 0);
		SSM(sci, SCI_SETANCHOR, position, 0); /* to avoid creation of a selection */
	}
	SSM(sci, SCI_CHOOSECARETX, 0, 0);
}


/* Set the cursor line without scrolling the view.
 * Use sci_goto_line() to also scroll. */
void sci_set_current_line(ScintillaObject *sci, gint line)
{
	gint pos = sci_get_position_from_line(sci, line);
	sci_set_current_position(sci, pos, FALSE);
}


/** Gets the total number of lines.
 * @param sci Scintilla widget.
 * @return The line count. */
gint sci_get_line_count(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETLINECOUNT, 0, 0);
}


/** Sets the selection start position.
 * @param sci Scintilla widget.
 * @param position Position. */
void sci_set_selection_start(ScintillaObject *sci, gint position)
{
	SSM(sci, SCI_SETSELECTIONSTART, position, 0);
}


/** Sets the selection end position.
 * @param sci Scintilla widget.
 * @param position Position. */
void sci_set_selection_end(ScintillaObject *sci, gint position)
{
	SSM(sci, SCI_SETSELECTIONEND, position, 0);
}


void sci_set_selection(ScintillaObject *sci, gint anchorPos, gint currentPos)
{
	SSM(sci, SCI_SETSEL, anchorPos, currentPos);
}


/** Gets the position at the end of a line
 * @param sci Scintilla widget.
 * @param line Line.
 * @return The position at the end of the line. */
gint sci_get_line_end_position(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETLINEENDPOSITION, line, 0);
}


void sci_cut(ScintillaObject *sci)
{
	SSM(sci, SCI_CUT, 0, 0);
}


void sci_copy(ScintillaObject *sci)
{
	SSM(sci, SCI_COPY, 0, 0);
}


void sci_paste(ScintillaObject *sci)
{
	SSM(sci, SCI_PASTE, 0, 0);
}


void sci_clear(ScintillaObject *sci)
{
	SSM(sci, SCI_CLEAR, 0, 0);
}


/** Gets the selection start position.
 * @param sci Scintilla widget.
 * @return Position. */
gint sci_get_selection_start(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETSELECTIONSTART, 0, 0);
}


/** Gets the selection end position.
 * @param sci Scintilla widget.
 * @return Position. */
gint sci_get_selection_end(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETSELECTIONEND, 0, 0);
}


/** Replaces selection.
 * @param sci Scintilla widget.
 * @param text Text. */
void sci_replace_sel(ScintillaObject *sci, const gchar *text)
{
	SSM(sci, SCI_REPLACESEL, 0, (sptr_t) text);
}


/** Gets the length of all text.
 * @param sci Scintilla widget.
 * @return Length. */
gint sci_get_length(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETLENGTH, 0, 0);
}


gint sci_get_lexer(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETLEXER, 0, 0);
}


/** Gets line length.
 * @param sci Scintilla widget.
 * @param line Line number.
 * @return Length. */
gint sci_get_line_length(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_LINELENGTH, line, 0);
}


/* safe way to read Scintilla string into new memory.
 * works with any string buffer messages that follow the Windows message convention. */
gchar *sci_get_string(ScintillaObject *sci, gint msg, gulong wParam)
{
	gint size = SSM(sci, msg, wParam, 0) + 1;
	gchar *str = g_malloc(size);

	SSM(sci, msg, wParam, (sptr_t)str);
	str[size - 1] = '\0';	/* ensure termination, needed for SCI_GETLINE */
	return str;
}


/** Gets line contents.
 * @param sci Scintilla widget.
 * @param line_num Line number.
 * @return A @c NULL-terminated copy of the line text. */
gchar *sci_get_line(ScintillaObject *sci, gint line_num)
{
	return sci_get_string(sci, SCI_GETLINE, line_num);
}


/** Gets all text.
 * @deprecated sci_get_text is deprecated and should not be used in newly-written code.
 * Use sci_get_contents() instead.
 *
 * @param sci Scintilla widget.
 * @param len Length of @a text buffer, usually sci_get_length() + 1.
 * @param text Text buffer; must be allocated @a len + 1 bytes for null-termination. */
void sci_get_text(ScintillaObject *sci, gint len, gchar *text)
{
	SSM(sci, SCI_GETTEXT, len, (sptr_t) text);
}


/** Gets all text inside a given text length.
 * @param sci Scintilla widget.
 * @param len Length of the text to retrieve from the start of the document,
 *            usually sci_get_length() + 1.
 * @return A copy of the text. Should be freed when no longer needed.
 *
 * @since 0.17
 */
gchar *sci_get_contents(ScintillaObject *sci, gint len)
{
	gchar *text = g_malloc(len);
	SSM(sci, SCI_GETTEXT, len, (sptr_t) text);
	return text;
}


/** Gets selected text.
 * @deprecated sci_get_selected_text is deprecated and should not be used in newly-written code.
 * Use sci_get_selection_contents() instead.
 *
 * @param sci Scintilla widget.
 * @param text Text buffer; must be allocated sci_get_selected_text_length() + 1 bytes
 * for null-termination. */
void sci_get_selected_text(ScintillaObject *sci, gchar *text)
{
	SSM(sci, SCI_GETSELTEXT, 0, (sptr_t) text);
}


/** Gets selected text.
 * @param sci Scintilla widget.
 *
 * @return The selected text. Should be freed when no longer needed.
 *
 * @since 0.17
 */
gchar *sci_get_selection_contents(ScintillaObject *sci)
{
	return sci_get_string(sci, SCI_GETSELTEXT, 0);
}


/** Gets selected text length.
 * @param sci Scintilla widget.
 * @return Length. */
gint sci_get_selected_text_length(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETSELTEXT, 0, 0);
}


gint sci_get_position_from_xy(ScintillaObject *sci, gint x, gint y, gboolean nearby)
{
	/* for nearby return -1 if there is no character near to the x,y point. */
	return SSM(sci, (nearby) ? SCI_POSITIONFROMPOINTCLOSE : SCI_POSITIONFROMPOINT, x, y);
}


/** Checks if a line is visible (folding may have hidden it).
 * @param sci Scintilla widget.
 * @param line Line number.
 * @return Whether @a line will be drawn on the screen. */
gboolean sci_get_line_is_visible(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETLINEVISIBLE, line, 0);
}


/** Makes @a line visible (folding may have hidden it).
 * @param sci Scintilla widget.
 * @param line Line number. */
void sci_ensure_line_is_visible(ScintillaObject *sci, gint line)
{
	SSM(sci, SCI_ENSUREVISIBLE, line, 0);
}


gint sci_get_fold_level(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETFOLDLEVEL, line, 0);
}


/* Get the line number of the fold point before start_line, or -1 if there isn't one */
gint sci_get_fold_parent(ScintillaObject *sci, gint start_line)
{
	return SSM(sci, SCI_GETFOLDPARENT, start_line, 0);
}


void sci_toggle_fold(ScintillaObject *sci, gint line)
{
	SSM(sci, SCI_TOGGLEFOLD, line, 1);
}


gboolean sci_get_fold_expanded(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETFOLDEXPANDED, line, 0);
}


void sci_colourise(ScintillaObject *sci, gint start, gint end)
{
	SSM(sci, SCI_COLOURISE, start, end);
}


void sci_clear_all(ScintillaObject *sci)
{
	SSM(sci, SCI_CLEARALL, 0, 0);
}


gint sci_get_end_styled(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETENDSTYLED, 0, 0);
}


void sci_set_tab_width(ScintillaObject *sci, gint width)
{
	SSM(sci, SCI_SETTABWIDTH, width, 0);
}


/** Gets display tab width (this is not indent width, see GeanyIndentPrefs).
 * @param sci Scintilla widget.
 * @return Width.
 *
 * @since 0.15
 **/
gint sci_get_tab_width(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETTABWIDTH, 0, 0);
}


/** Gets a character.
 * @param sci Scintilla widget.
 * @param pos Position.
 * @return Char. */
gchar sci_get_char_at(ScintillaObject *sci, gint pos)
{
	return (gchar) SSM(sci, SCI_GETCHARAT, pos, 0);
}


void sci_set_savepoint(ScintillaObject *sci)
{
	SSM(sci, SCI_SETSAVEPOINT, 0, 0);
}


void sci_set_indentation_guides(ScintillaObject *sci, gint mode)
{
	SSM(sci, SCI_SETINDENTATIONGUIDES, mode, 0);
}


void sci_use_popup(ScintillaObject *sci, gboolean enable)
{
	SSM(sci, SCI_USEPOPUP, enable, 0);
}


/** Checks if there's a selection.
 * @param sci Scintilla widget.
 * @return Whether a selection is present.
 *
 * @since 0.15
 **/
gboolean sci_has_selection(ScintillaObject *sci)
{
	if (SSM(sci, SCI_GETSELECTIONEND, 0, 0) - SSM(sci, SCI_GETSELECTIONSTART, 0, 0))
		return TRUE;
	else
		return FALSE;
}


void sci_goto_pos(ScintillaObject *sci, gint pos, gboolean unfold)
{
	if (unfold) SSM(sci, SCI_ENSUREVISIBLE, SSM(sci, SCI_LINEFROMPOSITION, pos, 0), 0);
	SSM(sci, SCI_GOTOPOS, pos, 0);
}


void sci_set_search_anchor(ScintillaObject *sci)
{
	SSM(sci, SCI_SEARCHANCHOR, 0, 0);
}


/* removes a selection if pos < 0 */
void sci_set_anchor(ScintillaObject *sci, gint pos)
{
	if (pos < 0)
		pos = sci_get_current_position(sci);

	SSM(sci, SCI_SETANCHOR, pos, 0);
}


/** Scrolls the cursor in view.
 * @param sci Scintilla widget. */
void sci_scroll_caret(ScintillaObject *sci)
{
	SSM(sci, SCI_SCROLLCARET, 0, 0);
}


void sci_scroll_lines(ScintillaObject *sci, gint lines)
{
	SSM(sci, SCI_LINESCROLL, 0, lines);
}


void sci_scroll_columns(ScintillaObject *sci, gint columns)
{
	SSM(sci, SCI_LINESCROLL, columns, 0);
}


gint sci_search_next(ScintillaObject *sci, gint flags, const gchar *text)
{
	return SSM(sci, SCI_SEARCHNEXT, flags, (sptr_t) text);
}


gint sci_search_prev(ScintillaObject *sci, gint flags, const gchar *text)
{
	return SSM(sci, SCI_SEARCHPREV, flags, (sptr_t) text);
}


/** Finds text in the document.
 * The @a ttf argument should be a pointer to a Sci_TextToFind structure which contains
 * the text to find and the range in which the text should be searched.
 *
 * Please refer to the Scintilla documentation for a more detailed description.
 *
 * @param sci Scintilla widget.
 * @param flags Bitmask of Scintilla search flags (@c SCFIND_*, see Scintilla documentation).
 * @param ttf Pointer to a TextToFind structure which contains the text to find and the range.
 * @return The position of the start of the found text if it succeeds, otherwise @c -1.
 *         The @c chrgText.cpMin and @c chrgText.cpMax members of @c TextToFind are filled in
 *         with the start and end positions of the found text.
 */
gint sci_find_text(ScintillaObject *sci, gint flags, struct Sci_TextToFind *ttf)
{
	return SSM(sci, SCI_FINDTEXT, flags, (long) ttf);
}


/** Sets the font for a particular style.
 * @param sci Scintilla widget.
 * @param style The style.
 * @param font The font name.
 * @param size The font size. */
void sci_set_font(ScintillaObject *sci, gint style, const gchar *font, gint size)
{
	SSM(sci, SCI_STYLESETFONT, style, (sptr_t) font);
	SSM(sci, SCI_STYLESETSIZE, style, size);
}


/** Jumps to the specified line in the document.
 * If @a unfold is set and @a line is hidden by a fold, it is unfolded
 * first to ensure it is visible.
 * @param sci Scintilla widget.
 * @param line Line.
 * @param unfold Whether to unfold first.
 */
void sci_goto_line(ScintillaObject *sci, gint line, gboolean unfold)
{
	if (unfold) SSM(sci, SCI_ENSUREVISIBLE, line, 0);
	SSM(sci, SCI_GOTOLINE, line, 0);
}


void sci_marker_delete_all(ScintillaObject *sci, gint marker)
{
	SSM(sci, SCI_MARKERDELETEALL, marker, 0);
}


/** Gets style ID at @a position.
 * @param sci Scintilla widget.
 * @param position Position.
 * @return Style ID. */
gint sci_get_style_at(ScintillaObject *sci, gint position)
{
	return SSM(sci, SCI_GETSTYLEAT, position, 0);
}


void sci_set_codepage(ScintillaObject *sci, gint cp)
{
	g_return_if_fail(cp == 0 || cp == SC_CP_UTF8);
	SSM(sci, SCI_SETCODEPAGE, cp, 0);
}


void sci_assign_cmdkey(ScintillaObject *sci, gint key, gint command)
{
	SSM(sci, SCI_ASSIGNCMDKEY, key, command);
}


void sci_clear_cmdkey(ScintillaObject *sci, gint key)
{
	SSM(sci, SCI_CLEARCMDKEY, key, 0);
}


/** Gets text between @a start and @a end.
 * @deprecated sci_get_text_range is deprecated and should not be used in newly-written code.
 * Use sci_get_contents_range() instead.
 *
 * @param sci Scintilla widget.
 * @param start Start.
 * @param end End.
 * @param text Text will be zero terminated and must be allocated (end - start + 1) bytes. */
void sci_get_text_range(ScintillaObject *sci, gint start, gint end, gchar *text)
{
	struct Sci_TextRange tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;
	SSM(sci, SCI_GETTEXTRANGE, 0, (long) &tr);
}


/** Gets text between @a start and @a end.
 *
 * @param sci Scintilla widget.
 * @param start Start.
 * @param end End.
 * @return The text inside the given range. Should be freed when no longer needed.
 *
 * @since 0.17
 */
gchar *sci_get_contents_range(ScintillaObject *sci, gint start, gint end)
{
	gchar *text = g_malloc((end - start) + 1);
	sci_get_text_range(sci, start, end, text);
	return text;
}


void sci_line_duplicate(ScintillaObject *sci)
{
	SSM(sci, SCI_LINEDUPLICATE, 0, 0);
}


void sci_selection_duplicate(ScintillaObject *sci)
{
	SSM(sci, SCI_SELECTIONDUPLICATE, 0, 0);
}


/** Inserts text.
 * @param sci Scintilla widget.
 * @param pos Position, or -1 for current.
 * @param text Text. */
void sci_insert_text(ScintillaObject *sci, gint pos, const gchar *text)
{
	SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) text);
}


void sci_target_from_selection(ScintillaObject *sci)
{
	SSM(sci, SCI_TARGETFROMSELECTION, 0, 0);
}


void sci_set_target_start(ScintillaObject *sci, gint start)
{
	SSM(sci, SCI_SETTARGETSTART, start, 0);
}


void sci_set_target_end(ScintillaObject *sci, gint end)
{
	SSM(sci, SCI_SETTARGETEND, end, 0);
}


gint sci_replace_target(ScintillaObject *sci, const gchar *text, gboolean regex)
{
	return SSM(sci, (regex) ? SCI_REPLACETARGETRE : SCI_REPLACETARGET, (uptr_t) -1, (sptr_t) text);
}


void sci_set_keywords(ScintillaObject *sci, gint k, const gchar *text)
{
	SSM(sci, SCI_SETKEYWORDS, k, (sptr_t) text);
}


void sci_set_readonly(ScintillaObject *sci, gboolean readonly)
{
	SSM(sci, SCI_SETREADONLY, readonly, 0);
}


/** Sends Scintilla commands without any parameters.
 * @param sci The Scintilla @c GtkWidget.
 * @param cmd @c SCI_COMMAND.
 * @see http://scintilla.org for the documentation.
 *
 *  @since 0.16
 */
void sci_send_command(ScintillaObject *sci, gint cmd)
{
	SSM(sci, cmd, 0, 0);
}


/** Gets current line number.
 * @param sci Scintilla widget.
 * @return Line number. */
gint sci_get_current_line(ScintillaObject *sci)
{
	return SSM(sci, SCI_LINEFROMPOSITION, SSM(sci, SCI_GETCURRENTPOS, 0, 0), 0);
}


/* Get number of lines partially or fully selected.
 * Returns 1 if there is a partial selection on the same line.
 * Returns 2 if a whole line is selected including the line break char(s). */
gint sci_get_lines_selected(ScintillaObject *sci)
{
	gint start = SSM(sci, SCI_GETSELECTIONSTART, 0, 0);
	gint end = SSM(sci, SCI_GETSELECTIONEND, 0, 0);

	if (start == end)
		return 0; /* no selection */

	return SSM(sci, SCI_LINEFROMPOSITION, end, 0) - SSM(sci, SCI_LINEFROMPOSITION, start, 0) + 1;
}


gint sci_get_first_visible_line(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
}


/**
 *  Sets the current indicator. This is necessary to define an indicator for a range of text or
 *  clearing indicators for a range of text.
 *
 *  @param sci Scintilla widget.
 *  @param indic The indicator number to set.
 *
 *  @see sci_indicator_clear
 *
 *  @since 0.16
 */
void sci_indicator_set(ScintillaObject *sci, gint indic)
{
	SSM(sci, SCI_SETINDICATORCURRENT, indic, 0);
}


void sci_indicator_fill(ScintillaObject *sci, gint pos, gint len)
{
	SSM(sci, SCI_INDICATORFILLRANGE, pos, len);
}


/**
 *  Clears the currently set indicator from a range of text.
 *  Starting at @a pos, @a len characters long.
 *  In order to make this function properly, you need to set the current indicator before with
 *  @ref sci_indicator_set().
 *
 *  @param sci Scintilla widget.
 *  @param pos Starting position.
 *  @param len Length.
 *
 *  @since 0.16
 */
void sci_indicator_clear(ScintillaObject *sci, gint pos, gint len)
{
	SSM(sci, SCI_INDICATORCLEARRANGE, pos, len);
}


void sci_select_all(ScintillaObject *sci)
{
	SSM(sci, SCI_SELECTALL, 0, 0);
}


gint sci_get_line_indent_position(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETLINEINDENTPOSITION, line, 0);
}


void sci_set_autoc_max_height(ScintillaObject *sci, gint val)
{
	SSM(sci, SCI_AUTOCSETMAXHEIGHT, val, 0);
}


/** Finds a matching brace at @a pos.
 * @param sci Scintilla widget.
 * @param pos Position.
 * @return Matching brace position.
 *
 * @since 0.15
 **/
gint sci_find_matching_brace(ScintillaObject *sci, gint pos)
{
	return SSM(sci, SCI_BRACEMATCH, pos, 0);
}


gint sci_get_overtype(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETOVERTYPE, 0, 0);
}


void sci_set_tab_indents(ScintillaObject *sci, gboolean set)
{
	SSM(sci, SCI_SETTABINDENTS, set, 0);
}


void sci_set_use_tabs(ScintillaObject *sci, gboolean set)
{
	SSM(sci, SCI_SETUSETABS, set, 0);
}


gint sci_get_pos_at_line_sel_start(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETLINESELSTARTPOSITION, line, 0);
}


gint sci_get_pos_at_line_sel_end(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETLINESELENDPOSITION, line, 0);
}


/** Gets selection mode.
 * @param sci Scintilla widget.
 * @return Selection mode. */
gint sci_get_selection_mode(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETSELECTIONMODE, 0, 0);
}


/** Sets selection mode.
 * @param sci Scintilla widget.
 * @param mode Mode. */
void sci_set_selection_mode(ScintillaObject *sci, gint mode)
{
	SSM(sci, SCI_SETSELECTIONMODE, mode, 0);
}


void sci_set_scrollbar_mode(ScintillaObject *sci, gboolean visible)
{
	SSM(sci, SCI_SETHSCROLLBAR, visible, 0);
	SSM(sci, SCI_SETVSCROLLBAR, visible, 0);
}


/** Sets the indentation of a line.
 * @param sci Scintilla widget.
 * @param line Line to indent.
 * @param indent Indentation width.
 *
 * @since 0.19
 */
void sci_set_line_indentation(ScintillaObject *sci, gint line, gint indent)
{
	SSM(sci, SCI_SETLINEINDENTATION, line, indent);
}


/** Gets the indentation width of a line.
 * @param sci Scintilla widget.
 * @param line Line to get the indentation from.
 * @return Indentation width.
 *
 * @since 0.19
 */
gint sci_get_line_indentation(ScintillaObject *sci, gint line)
{
	return SSM(sci, SCI_GETLINEINDENTATION, line, 0);
}


void sci_set_caret_policy_x(ScintillaObject *sci, gint policy, gint slop)
{
	SSM(sci, SCI_SETXCARETPOLICY, policy, slop);
}


void sci_set_caret_policy_y(ScintillaObject *sci, gint policy, gint slop)
{
	SSM(sci, SCI_SETYCARETPOLICY, policy, slop);
}


void sci_set_scroll_stop_at_last_line(ScintillaObject *sci, gboolean set)
{
	SSM(sci, SCI_SETENDATLASTLINE, set, 0);
}


void sci_cancel(ScintillaObject *sci)
{
	SSM(sci, SCI_CANCEL, 0, 0);
}


gint sci_get_target_end(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETTARGETEND, 0, 0);
}


gint sci_get_position_after(ScintillaObject *sci, gint start)
{
	return SSM(sci, SCI_POSITIONAFTER, start, 0);
}


void sci_lines_split(ScintillaObject *sci, gint pixelWidth)
{
	SSM(sci, SCI_LINESSPLIT, pixelWidth, 0);
}


void sci_lines_join(ScintillaObject *sci)
{
	SSM(sci, SCI_LINESJOIN, 0, 0);
}


gint sci_text_width(ScintillaObject *sci, gint styleNumber, const gchar *text)
{
	return SSM(sci, SCI_TEXTWIDTH, styleNumber, (sptr_t) text);
}
