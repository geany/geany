/*
 *      sciwrappers.c - this file is part of Geany, a fast and lightweight IDE
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
 * $Id$
 */

#include <string.h>

#include "geany.h"

#include "sciwrappers.h"
#include "utils.h"

#define SSM(s, m, w, l) scintilla_send_message(s, m, w, l)


// stolen from cssed (http://cssed.sf.net), thanks


/* line numbers visibility */
void sci_set_line_numbers(ScintillaObject * sci, gboolean set, gint extra_width)
{
	if (set)
	{
		gchar tmp_str[15];
		gint len = SSM(sci, SCI_GETLINECOUNT, 0, 0);
		gint width;
		g_snprintf(tmp_str, 15, "_%d%d", len, extra_width);
		width = SSM(sci, SCI_TEXTWIDTH, STYLE_LINENUMBER, (sptr_t) tmp_str);
		SSM (sci, SCI_SETMARGINWIDTHN, 0, width);
		SSM (sci, SCI_SETMARGINSENSITIVEN, 0, FALSE); // use default behaviour
	}
	else
	{
		SSM (sci, SCI_SETMARGINWIDTHN, 0, 0 );
	}
}

void sci_set_mark_long_lines(ScintillaObject * sci, gint type, gint column, const gchar *colour)
{
	if (column == 0) type = 2;
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
	SSM(sci, SCI_SETEDGECOLUMN, column - 1, 0);
	SSM(sci, SCI_SETEDGECOLOUR, utils_strtod(colour, NULL, TRUE), 0);
}


gboolean sci_get_line_numbers(ScintillaObject * sci)
{
	gint margin_width;

	margin_width = SSM(sci, SCI_GETMARGINWIDTHN, 0, 0);
	if( margin_width > 0 ) return TRUE;
	else return FALSE;
}

/* symbol margin visibility */
void sci_set_symbol_margin(ScintillaObject * sci, gboolean set )
{
	if (set)
	{
		SSM(sci, SCI_SETMARGINWIDTHN, 1, 16);
		SSM(sci, SCI_SETMARGINSENSITIVEN, 1, TRUE);
	}
	else
	{
		SSM (sci, SCI_SETMARGINWIDTHN, 1, 0);
		SSM(sci, SCI_SETMARGINSENSITIVEN, 1, FALSE);
	}
}

gboolean sci_get_symbol_margin(ScintillaObject *sci)
{
	if (SSM(sci, SCI_GETMARGINWIDTHN, 1, 0) > 0 ) return TRUE;
	else return FALSE;
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

gboolean sci_get_folding_margin_visible(ScintillaObject * sci)
{
	gint margin_width;

	margin_width = SSM(sci, SCI_GETMARGINWIDTHN, 2, 0);
	if( margin_width > 0 ) return TRUE;
	else return FALSE;
}

/* end of lines */
void sci_set_visible_eols(ScintillaObject* sci, gboolean set )
{
	SSM(sci,SCI_SETVIEWEOL,set,0);
}

gboolean sci_get_visible_eols(ScintillaObject* sci)
{
	return SSM( sci, SCI_GETVIEWEOL,0,0);
}

void sci_set_visible_white_spaces(ScintillaObject* sci, gboolean set )
{
	if(set){
		SSM(sci,SCI_SETVIEWWS,SCWS_VISIBLEALWAYS,0);
	}else{
		SSM(sci,SCI_SETVIEWWS,SCWS_INVISIBLE,0);
	}
}

gboolean sci_get_visible_white_spaces(ScintillaObject* sci)
{
	return SSM( sci, SCI_GETVIEWWS,0,0);
}

void sci_set_lines_wrapped(ScintillaObject* sci, gboolean set )
{
	if( set ){
		SSM(sci,SCI_SETWRAPMODE,SC_WRAP_WORD,0);
		SSM(sci, SCI_SETWRAPVISUALFLAGS, SC_WRAPVISUALFLAG_END, 0);
	}else{
		SSM(sci,SCI_SETWRAPMODE,SC_WRAP_NONE,0);
	}
}

gboolean sci_get_lines_wrapped(ScintillaObject* sci)
{
	return SSM( sci, SCI_GETWRAPMODE,0,0);
}

gint sci_get_eol_mode( ScintillaObject* sci)
{
	return SSM( sci, SCI_GETEOLMODE, 0, 0);
}

void sci_set_eol_mode( ScintillaObject* sci, gint eolmode)
{
	SSM( sci, SCI_SETEOLMODE, eolmode, 0);
}

void sci_convert_eols( ScintillaObject* sci, gint eolmode)
{
	SSM( sci, SCI_CONVERTEOLS, eolmode,0);
}

void sci_add_text(ScintillaObject* sci, const gchar* text)
{
	if( text != NULL ){// if null text is passed to scintilla will segfault
		SSM( sci, SCI_ADDTEXT, strlen(text), (sptr_t) text);
	}
}

void sci_set_text(ScintillaObject* sci, const gchar* text)
{
	if( text != NULL ){// if null text is passed to scintilla will segfault
		SSM( sci, SCI_SETTEXT, 0, (sptr_t) text);
	}
}


void sci_add_text_buffer(ScintillaObject* sci, const gchar* text, gint len)
{
	if( text != NULL ){// if null text is passed to scintilla will segfault
		SSM(sci, SCI_CLEARALL, 0, 0);
		SSM(sci, SCI_ADDTEXT, len, (sptr_t) text);
	}
}


gboolean sci_can_undo( ScintillaObject* sci )
{
	return SSM( sci, SCI_CANUNDO, 0, 0);
}


gboolean sci_can_redo( ScintillaObject* sci )
{
	return SSM( sci, SCI_CANREDO, 0, 0);
}


void sci_undo( ScintillaObject* sci )
{
	if( sci_can_undo(sci) ){
		SSM( sci, SCI_UNDO, 0, 0);
	}else{ // change it to a document function

	}
}


void sci_redo( ScintillaObject* sci )
{
	if( sci_can_redo( sci ) ){
		SSM( sci, SCI_REDO,0,0);
	}else{// change it to a document function

	}
}


void sci_start_undo_action( ScintillaObject* sci )
{
	SSM( sci,SCI_BEGINUNDOACTION,0,0 );
}


void sci_end_undo_action( ScintillaObject* sci )
{
	SSM( sci, SCI_ENDUNDOACTION,0,0);
}


void sci_set_undo_collection( ScintillaObject* sci, gboolean set )
{
	SSM( sci, SCI_SETUNDOCOLLECTION,set,0);
}


gboolean sci_get_undo_collection( ScintillaObject* sci )
{
	return SSM( sci, SCI_GETUNDOCOLLECTION,0,0);
}


void sci_empty_undo_buffer( ScintillaObject* sci )
{
	SSM( sci, SCI_EMPTYUNDOBUFFER,0,0);
}


gboolean sci_is_modified(ScintillaObject *sci)
{
	return (SSM(sci, SCI_GETMODIFY, 0, 0) != 0);
}


void sci_zoom_in( ScintillaObject* sci )
{
	SSM( sci, SCI_ZOOMIN,0,0);
}


void sci_zoom_out( ScintillaObject* sci )
{
	SSM( sci, SCI_ZOOMOUT,0,0);
}


void sci_zoom_off( ScintillaObject* sci )
{
	SSM( sci, SCI_SETZOOM,0,0);
}


gint sci_get_zoom( ScintillaObject* sci )
{
	return SSM( sci, SCI_GETZOOM,0,0);
}


void sci_set_marker_at_line( ScintillaObject* sci, gint line_number, gboolean set, gint marker )
{
	if ( set )
	{
		SSM( sci, SCI_MARKERADD, line_number, marker);
	}
	else
	{
		SSM( sci, SCI_MARKERDELETE, line_number, marker);
	}
}


gboolean sci_is_marker_set_at_line(ScintillaObject* sci, gint line, gint marker)
{
	gint state;

	state = SSM( sci, SCI_MARKERGET, line, marker );
	return(!(state & (1 << marker)));
}


gboolean sci_marker_next(ScintillaObject* sci, gint line, gint marker_mask)
{
	gint marker_line;

	marker_line = SSM(sci, SCI_MARKERNEXT, line, marker_mask);

	if( marker_line != -1 ){
		SSM(sci,SCI_GOTOLINE,marker_line,0);
		return TRUE;
	}else{
		return FALSE;
	}
}


gboolean sci_marker_prev(ScintillaObject* sci, gint line, gint marker_mask)
{
	gint marker_line;

	marker_line = SSM(sci, SCI_MARKERPREVIOUS, line, marker_mask);

	if( marker_line != -1 ){
		SSM(sci,SCI_GOTOLINE,marker_line,0);
		return TRUE;
	}else{
		return FALSE;
	}
}


gint sci_get_line_from_position(ScintillaObject* sci, gint position )
{
	return SSM(sci, SCI_LINEFROMPOSITION, position, 0);
}


gint sci_get_col_from_position(ScintillaObject* sci, gint position )
{
	return SSM(sci, SCI_GETCOLUMN, position, 0);
}


gint sci_get_position_from_line(ScintillaObject* sci, gint line )
{
	return SSM(sci, SCI_POSITIONFROMLINE, line, 0);
}


gint sci_get_current_position(ScintillaObject* sci )
{
	return SSM(sci, SCI_GETCURRENTPOS, 0, 0);
}


void sci_set_current_position(ScintillaObject* sci, gint position )
{
	SSM(sci, SCI_GOTOPOS, position, 0);
}


void sci_set_current_line(ScintillaObject* sci, gint line )
{
	SSM(sci, SCI_GOTOLINE, line, 0);
}


gint sci_get_line_count( ScintillaObject* sci )
{
	return SSM(sci, SCI_GETLINECOUNT, 0, 0);
}


void sci_set_selection_start(ScintillaObject* sci, gint position)
{
	SSM(sci, SCI_SETSELECTIONSTART, position, 0);
}


void sci_set_selection_end(ScintillaObject* sci, gint position)
{
	SSM(sci, SCI_SETSELECTIONEND, position, 0);
}


gint sci_get_line_end_from_position(ScintillaObject* sci, gint position)
{
	return SSM(sci, SCI_GETLINEENDPOSITION, position, 0);
}


void sci_cut(ScintillaObject* sci)
{
	SSM(sci, SCI_CUT, 0, 0);
}


void sci_copy(ScintillaObject* sci)
{
	SSM(sci, SCI_COPY, 0, 0);
}


void sci_paste(ScintillaObject* sci)
{
	SSM(sci, SCI_PASTE, 0, 0);
}


void sci_clear(ScintillaObject* sci)
{
	SSM(sci, SCI_CLEAR, 0, 0);
}


gint sci_get_selection_start(ScintillaObject* sci)
{
	return SSM(sci, SCI_GETSELECTIONSTART,0,0);
}


gint sci_get_selection_end(ScintillaObject* sci)
{
	return SSM(sci, SCI_GETSELECTIONEND,0,0);
}


void sci_replace_sel(ScintillaObject* sci, gchar* text)
{
	SSM(sci, SCI_REPLACESEL,0, (sptr_t) text);
}


gint sci_get_length(ScintillaObject* sci)
{
	return SSM(sci,SCI_GETLENGTH,0,0);
}


gint sci_get_lexer(ScintillaObject* sci)
{
	return SSM(sci,SCI_GETLEXER,0,0);
}


gint sci_get_line_length(ScintillaObject* sci,gint line)
{
	return SSM(sci,SCI_LINELENGTH, line,0);
}


// Returns: a NULL-terminated copy of the line text
gchar *sci_get_line(ScintillaObject* sci, gint line_num)
{
	gint len = sci_get_line_length(sci, line_num);
	gchar *linebuf = g_malloc(len + 1);

	SSM(sci, SCI_GETLINE, line_num, (sptr_t) linebuf);
	linebuf[len] = '\0';
	return linebuf;
}


// the last char will be null terminated
void sci_get_text(ScintillaObject* sci, gint len, gchar* text)
{
	SSM( sci, SCI_GETTEXT, len, (sptr_t) text );
}


void sci_get_selected_text(ScintillaObject* sci, gchar* text)
{
	SSM( sci, SCI_GETSELTEXT, 0, (sptr_t) text);
}


gint sci_get_selected_text_length(ScintillaObject* sci)
{
	return SSM( sci, SCI_GETSELTEXT, 0, 0);
}


gint sci_get_position_from_xy(ScintillaObject* sci, gint x, gint y, gboolean nearby)
{
	// for nearby return -1 if there is no character near to the x,y point.
	return SSM(sci, (nearby) ? SCI_POSITIONFROMPOINTCLOSE : SCI_POSITIONFROMPOINT, x, y);
}


void sci_get_xy_from_position(ScintillaObject* sci,gint pos, gint* x, gint* y)
{
	*x = SSM(sci, SCI_POINTXFROMPOSITION,0, (int) pos);
 	*y = SSM(sci, SCI_POINTYFROMPOSITION,0, (int) pos);
}


/* folding */
gboolean sci_get_line_is_visible(ScintillaObject* sci, gint line)
{
	return SSM(sci,SCI_GETLINEVISIBLE, line,0);
}


void sci_ensure_line_is_visible(ScintillaObject* sci, gint line)
{
	SSM(sci,SCI_ENSUREVISIBLE,line,0);
}


gint sci_get_fold_level(ScintillaObject* sci, gint line)
{
	return SSM(sci,SCI_GETFOLDLEVEL, line,0);
}


/* Get the next line after start_line with fold level <= level */
gint sci_get_last_child(ScintillaObject* sci, gint start_line, gint level)
{
	return SSM( sci, SCI_GETLASTCHILD, start_line, level);
}


/* Get the line number of the fold point before start_line, or -1 if there isn't one */
gint sci_get_fold_parent(ScintillaObject* sci, gint start_line)
{
	return SSM( sci, SCI_GETFOLDPARENT, start_line, 0);
}


void sci_toggle_fold(ScintillaObject* sci, gint line)
{
	SSM( sci, SCI_TOGGLEFOLD, line, 1);
}


gboolean sci_get_fold_expanded(ScintillaObject* sci, gint line)
{
	return SSM( sci, SCI_GETFOLDEXPANDED, line, 0);
}


void sci_colourise( ScintillaObject* sci, gint start, gint end)
{
	SSM( sci, SCI_COLOURISE, start, end);
}


void sci_set_lexer(ScintillaObject * sci, gint lexer)
{
	SSM( sci, SCI_SETLEXER, lexer, 0);
}


void sci_clear_all(ScintillaObject * sci)
{
	SSM( sci, SCI_CLEARALL, 0, 0);
}


gint sci_get_end_styled(ScintillaObject * sci)
{
	return SSM(sci,SCI_GETENDSTYLED, 0,0);
}


gint sci_get_line_end_styled(ScintillaObject * sci, gint end_styled)
{
	return SSM(sci,SCI_LINEFROMPOSITION, end_styled,0);
}


void sci_set_tab_width(ScintillaObject * sci, gint width)
{
	SSM(sci, SCI_SETTABWIDTH, width, 0);
}


gint sci_get_tab_width(ScintillaObject * sci)
{
	return SSM(sci, SCI_GETTABWIDTH, 0, 0);
}


gchar sci_get_char_at(ScintillaObject *sci, gint pos)
{
	return (gchar) SSM(sci, SCI_GETCHARAT, pos, 0);
}


void sci_set_savepoint(ScintillaObject *sci)
{
	SSM(sci, SCI_SETSAVEPOINT, 0, 0);
}


void sci_set_indentionguides(ScintillaObject *sci, gboolean enable)
{
	SSM(sci, SCI_SETINDENTATIONGUIDES, enable, 0);
}


void sci_use_popup(ScintillaObject *sci, gboolean enable)
{
	SSM(sci, SCI_USEPOPUP, enable, 0);
}


// you can also call this has_selection
gboolean sci_can_copy(ScintillaObject *sci)
{
	if (SSM(sci, SCI_GETSELECTIONEND,0,0) - SSM(sci, SCI_GETSELECTIONSTART,0,0))
		return TRUE;
	else
		return FALSE;
}


void sci_goto_pos(ScintillaObject *sci, gint pos, gboolean ensure_visibility)
{
	if (ensure_visibility) SSM(sci,SCI_ENSUREVISIBLE,SSM(sci, SCI_LINEFROMPOSITION, pos, 0),0);
	SSM(sci, SCI_GOTOPOS, pos, 0);
}


void sci_set_search_anchor(ScintillaObject *sci)
{
	SSM(sci, SCI_SEARCHANCHOR, 0, 0);
}


void sci_set_anchor(ScintillaObject *sci, gint pos)
{
	SSM(sci, SCI_SETANCHOR, pos, 0);
}


void sci_scroll_caret(ScintillaObject *sci)
{
	SSM(sci, SCI_SCROLLCARET, 0, 0);
}


void sci_scroll_lines(ScintillaObject *sci, gint lines)
{
	SSM(sci, SCI_LINESCROLL, 0, lines);
}


/* Scroll the view to make line appear at percent_of_view.
 * line can be -1 to use the current position. */
void sci_scroll_to_line(ScintillaObject *sci, gint line, gfloat percent_of_view)
{
	gint vis1, los, delta;

	if (line == -1)
		line = sci_get_current_line(sci, -1);

	// sci 'visible line' != file line number
	vis1 = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	vis1 = SSM(sci, SCI_DOCLINEFROMVISIBLE, vis1, 0);
	los = SSM(sci, SCI_LINESONSCREEN, 0, 0);
	delta = (line - vis1) - los * percent_of_view;
	sci_scroll_lines(sci, delta);
	sci_scroll_caret(sci); //ensure visible, in case of excessive folding/wrapping
}


gint sci_search_next(ScintillaObject *sci, gint flags, const gchar *text)
{
	return SSM(sci, SCI_SEARCHNEXT, flags, (sptr_t) text );
}


gint sci_search_prev(ScintillaObject *sci, gint flags, const gchar *text)
{
	return SSM(sci, SCI_SEARCHPREV, flags, (sptr_t) text );
}


gint sci_find_text(ScintillaObject *sci, gint flags, struct TextToFind *ttf)
{
	return SSM(sci, SCI_FINDTEXT, flags, (long) ttf );
}


void sci_set_font(ScintillaObject *sci, gint style, const gchar* font, gint size)
{
	SSM(sci, SCI_STYLESETFONT, style, (sptr_t) font);
	SSM(sci, SCI_STYLESETSIZE, style, size);
}


void sci_goto_line(ScintillaObject *sci, gint line, gboolean ensure_visibility)
{
	if (ensure_visibility) SSM(sci,SCI_ENSUREVISIBLE,line,0);
	SSM(sci, SCI_GOTOLINE, line, 0);
}


void sci_marker_delete_all(ScintillaObject *sci, gint marker)
{
	SSM(sci, SCI_MARKERDELETEALL, marker, 0);
}


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
	SSM (sci, SCI_CLEARCMDKEY, key, 0);
}


void sci_get_text_range(ScintillaObject *sci, gint start, gint end, gchar *text)
{
	struct TextRange tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = text;
	SSM(sci, SCI_GETTEXTRANGE, 0, (long) &tr);
}


void sci_line_duplicate(ScintillaObject *sci)
{
	SSM(sci, SCI_LINEDUPLICATE, 0, 0);
}


void sci_selection_duplicate(ScintillaObject *sci)
{
	SSM(sci, SCI_SELECTIONDUPLICATE, 0, 0);
}


void sci_insert_text(ScintillaObject *sci, gint pos, const gchar *text)
{
	SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) text);
}


void sci_grap_focus(ScintillaObject *sci)
{
	SSM(sci, SCI_GRABFOCUS, 0, 0);
}


void sci_set_cursor(ScintillaObject *sci, gint cursor)
{
	SSM(sci, SCI_SETCURSOR, cursor, 0);
}


void sci_target_from_selection(ScintillaObject *sci)
{
	SSM(sci, SCI_TARGETFROMSELECTION, 0, 0);
}


 void sci_target_start(ScintillaObject *sci, gint start)
{
	SSM(sci, SCI_SETTARGETSTART, start, 0);
}


void sci_target_end(ScintillaObject *sci, gint end)
{
	SSM(sci, SCI_SETTARGETEND, end, 0);
}


gint sci_target_replace(ScintillaObject *sci, const gchar *text, gboolean regex)
{
	return SSM(sci, (regex) ? SCI_REPLACETARGETRE : SCI_REPLACETARGET, -1, (sptr_t) text);
}


void sci_set_keywords(ScintillaObject *sci, gint k, gchar *text)
{
	SSM(sci, SCI_SETKEYWORDS, k, (sptr_t) text);
}

void sci_set_readonly(ScintillaObject *sci, gboolean readonly)
{
	SSM(sci, SCI_SETREADONLY, readonly, 0);
}

gboolean sci_get_readonly(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETREADONLY, 0, 0);
}

// a simple convenience function to not have SSM() in the outside of this file
 void sci_cmd(ScintillaObject * sci, gint cmd)
{
	SSM(sci, cmd, 0, 0);
}


gint sci_get_current_line(ScintillaObject *sci, gint pos)
{
	if (pos >= 0)
	{
		return SSM(sci, SCI_LINEFROMPOSITION, pos, 0);
	}
	else
	{
		return SSM(sci, SCI_LINEFROMPOSITION, SSM(sci, SCI_GETCURRENTPOS, 0, 0), 0);
	}
}

/* Get number of lines partially or fully selected.
 * Returns 1 if there is a partial selection on the same line.
 * Returns 2 if a whole line is selected including the line break char(s). */
gint sci_get_lines_selected(ScintillaObject *sci)
{
	gint start = SSM(sci, SCI_GETSELECTIONSTART, 0, 0);
	gint end = SSM(sci, SCI_GETSELECTIONEND, 0, 0);

	if (start == end)
		return 0; // no selection

	return SSM(sci, SCI_LINEFROMPOSITION, end, 0) - SSM(sci, SCI_LINEFROMPOSITION, start, 0) + 1;
}

gint sci_get_first_visible_line(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
}


void sci_set_styling(ScintillaObject *sci, gint len, gint style)
{
	if (len < 0 || style < 0) return;

	SSM(sci, SCI_SETSTYLING, len, style);
}

void sci_start_styling(ScintillaObject *sci, gint pos, gint mask)
{
	SSM(sci, SCI_STARTSTYLING, pos, mask);
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

gint sci_find_bracematch(ScintillaObject *sci, gint pos)
{
	return SSM(sci, SCI_BRACEMATCH, pos, 0);
}

gint sci_get_overtype(ScintillaObject *sci)
{
	return SSM(sci, SCI_GETOVERTYPE, 0, 0);
}

