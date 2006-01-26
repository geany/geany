/*
 *      sci_cb.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#include <ctype.h>
#include <string.h>

#include "geany.h"

#include "sci_cb.h"
#include "document.h"
#include "sciwrappers.h"
#include "utils.h"

static gint link_start, link_end, style;
static gchar indent[100];


// callback func called by all editors when a signal arises

void on_editor_notification(GtkWidget* editor, gint scn, gpointer lscn, gpointer user_data)
{
	struct SCNotification *nt;
	ScintillaObject *sci;
	gint idx;

	idx = GPOINTER_TO_INT(user_data);
	sci = doc_list[idx].sci;

	nt = lscn;
	switch (nt->nmhdr.code)
	{
		case SCN_SAVEPOINTLEFT:
		{
			doc_list[idx].changed = TRUE;
			document_set_text_changed(idx);
			//app->has_dirty_editors = TRUE;
			break;
		}
		case SCN_SAVEPOINTREACHED:
		{
			doc_list[idx].changed = FALSE;
			document_set_text_changed(idx);
			break;
		}
		case SCN_MODIFYATTEMPTRO:
		{
			if (app->beep_on_errors) gdk_beep();
			break;
		}
		case SCN_MARGINCLICK:
		{
			// left click to line number margin without CTRL selects the line
			if (nt->margin == 0 && ! (nt->modifiers & SCMOD_CTRL))
			{	// selection complete line by clicking the linenumber
				gint line = sci_get_line_from_position(sci, nt->position);
				gint len = sci_get_line_length(sci, line);
				sci_set_selection_start(sci, nt->position);
				sci_set_selection_end(sci, (nt->position + len));
			}
			// left click to line number margin with CTRL (or marker margin) marks the line
			if (nt->margin == 1 || (nt->margin == 0 && (nt->modifiers & SCMOD_CTRL)))
			{
				gint line = sci_get_line_from_position(sci, nt->position);
				//sci_marker_delete_all(doc_list[idx].sci, 1);
				sci_set_marker_at_line(sci, line, sci_is_marker_set_at_line(sci, line, 1), 1);
			}
			break;
		}
		case SCN_UPDATEUI:
		{
			// undo / redo menu update
			utils_update_popup_reundo_items(idx);

			// brace highlighting
			sci_cb_highlight_braces(sci);

			utils_update_statusbar(idx);

			break;
		}
/*		case SCN_KEY:
		{
			//geany_debug("key notification triggered with %c", nt->ch);
			break;
		}
		case SCN_MODIFIED:
		{
			//if (nt->modificationType == SC_MOD_INSERTTEXT)
			//	geany_debug("modi: %s", nt->text);

			break;
		}
*/		case SCN_CHARADDED:
		{
			gint pos = sci_get_current_position(sci);

			switch (nt->ch)
			{
				case '\r':
				{	// simple indentation (only for CR format)
					if (app->use_auto_indention && (sci_get_eol_mode(sci) == SC_EOL_CR))
					{
						sci_cb_get_indent(sci, pos, FALSE);
						sci_add_text(sci, indent);
					}
					// " * " auto completion in multiline C/C++ comments
					if (sci_get_eol_mode(sci) == SC_EOL_CR)
					{
						sci_cb_auto_multiline(sci, pos);
					}
					break;
				}
				case '\n':
				{	// simple indentation (for CR/LF and LF format)
					if (app->use_auto_indention)
					{
						sci_cb_get_indent(sci, pos, FALSE);
						sci_add_text(sci, indent);
					}
					// " * " auto completion in multiline C/C++ comments
					sci_cb_auto_multiline(sci, pos);
					break;
				}
				case '>':
				case '/':
				{	// close xml-tags
					sci_cb_handle_xml(sci, nt->ch);
					break;
				}
				case '(':
				{	// show calltips
					sci_cb_show_calltip(sci, pos);
					break;
				}
				case ')':
				{	// hide calltips
					if (SSM(sci, SCI_CALLTIPACTIVE, 0, 0))
					{
						SSM(sci, SCI_CALLTIPCANCEL, 0, 0);
					}
					break;
				}
				case ' ':
				{	// if and for autocompletion
					if (app->auto_complete_constructs) sci_cb_auto_forif(sci, idx);
					break;
				}
				case '[':
				case '{':
				{	// Tex auto-closing
					sci_cb_auto_close_bracket(sci, pos, nt->ch);	// Tex auto-closing
					break;
				}
				case '}':
				{	// closing bracket handling
					if (app->use_auto_indention) sci_cb_close_block(sci, pos - 1);
					break;
				}
				default: sci_cb_start_auto_complete(sci, pos);
			}
			break;
		}
		case SCN_USERLISTSELECTION:
		{
			if (nt->listType == 1)
			{
				gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
				SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) nt->text);
			}
			else if (nt->listType == 2)
			{
				gint start, pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
				start = pos;
				while (sci_get_char_at(sci, --start) != '&') ;

				geany_debug("%d-%d", start, pos);
				SSM(sci, SCI_INSERTTEXT, pos - 1, (sptr_t) nt->text);
			}
			break;
		}
		case SCN_DWELLSTART:
		{
			//sci_cb_handle_uri(sci, nt->position);
			break;
		}
		case SCN_DWELLEND:
		{
/*			gint end_of_styling = SSM(sci, SCI_GETENDSTYLED, 0, 0);
			SSM(sci, SCI_STARTSTYLING, link_start, 31);
			SSM(sci, SCI_SETSTYLING, (link_end - link_start) + 1, style);
			SSM(sci, SCI_STARTSTYLING, end_of_styling, 31);
*/			break;
		}
		case SCN_STYLENEEDED:
		{
			geany_debug("style");
			break;
		}
		case SCN_URIDROPPED:
		{
			geany_debug(nt->text);
		}
	}
}


void sci_cb_get_indent(ScintillaObject *sci, gint pos, gboolean use_this_line)
{
	// very simple indentation algorithm
	gint i, prev_line, len, j = 0;
	gchar *linebuf;

	prev_line = sci_get_line_from_position(sci, pos);

	if (! use_this_line) prev_line--;
	len = sci_get_line_length(sci, prev_line);
	linebuf = g_malloc(len + 1);
	sci_get_line(sci, prev_line, linebuf);
	linebuf[len] = '\0';

	for (i = 0; i < len; i++)
	{
		if (linebuf[i] == ' ' || linebuf[i] == '\t') indent[j++] = linebuf[i];
		// "&& ! use_this_line" to auto-indent only if it is a real new line
		// and ignore the case of sci_cb_close_block
		else if (linebuf[i] == '{' && ! use_this_line)
		{
			indent[j++] = '\t';
			break;
		}
		else
		{
			gint k = len - 1;
			if (use_this_line) break;	// break immediately in the case of sci_cb_close_block
			while (isspace(linebuf[k])) k--;
			// if last non-whitespace character is a { increase indention by a tab
			// e.g. for (...) {
			if (linebuf[k] == '{') indent[j++] = '\t';
			break;
		}
	}
	indent[j] = '\0';
	g_free(linebuf);
}


void sci_cb_auto_close_bracket(ScintillaObject *sci, gint pos, gchar c)
{
	if (SSM(sci, SCI_GETLEXER, 0, 0) != SCLEX_LATEX) return;

	if (c == '[')
	{
		sci_add_text(sci, "]");
	}
	else if (c == '{')
	{
		sci_add_text(sci, "}");
	}
	sci_set_current_position(sci, pos);
}


void sci_cb_close_block(ScintillaObject *sci, gint pos)
{
	gint x = 0, cnt = 0;
	gint start_brace = utils_brace_match(sci, pos);
	gint line = sci_get_line_from_position(sci, pos);
	gint line_start = sci_get_position_from_line(sci, line);
	gint line_len = sci_get_line_length(sci, line);
	// set eol_char_len to 0 if on last line, because there is no EOL char
	gint eol_char_len = (line == (SSM(sci, SCI_GETLINECOUNT, 0, 0) - 1)) ? 0 :
								utils_get_eol_char_len(document_find_by_sci(sci));
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gchar *text, line_buf[255];

	if (lexer != SCLEX_CPP && lexer != SCLEX_HTML && lexer != SCLEX_PASCAL) return;

	// check that the line is empty, to not kill text in the line
	sci_get_line(sci, line, line_buf);
	line_buf[line_len - eol_char_len] = '\0';
	while (x < (line_len - eol_char_len))
	{
		if (isspace(line_buf[x])) cnt++;
		x++;
	}
	//geany_debug("line_len: %d eol: %d cnt: %d", line_len, eol_char_len, cnt);
	if ((line_len - eol_char_len - 1) != cnt) return;

	if (start_brace >= 0) sci_cb_get_indent(sci, start_brace, TRUE);
/*	geany_debug("pos: %d, start: %d char: %c start_line: %d", pos, start_brace,
					sci_get_char_at(sci, pos), sci_get_line_from_position(sci, start_brace));
*/

	text = g_strconcat(indent, "}", NULL);
	sci_set_anchor(sci, line_start);
	SSM(sci, SCI_REPLACESEL, 0, (sptr_t) text);
	g_free(text);
}


gboolean sci_cb_show_calltip(ScintillaObject *sci, gint pos)
{
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint style;
	gchar word[128];
	const GPtrArray *tags;

	word[0] = '\0';
	if (pos == -1)
	{	// position of '(' is unknown, so go backwards to find it
		pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
		while (SSM(sci, SCI_GETCHARAT, pos, 0) != '(') pos--;
	}

	style = SSM(sci, SCI_GETSTYLEAT, pos, 0);
	if (lexer != SCLEX_CPP) return FALSE;
	if (lexer == SCLEX_CPP && (style == SCE_C_COMMENT ||
			style == SCE_C_COMMENTLINE || style == SCE_C_COMMENTDOC)) return FALSE;

	utils_find_current_word(sci, pos - 1, word);
	if (word[0] == '\0') return FALSE;

	tags = tm_workspace_find(word, tm_tag_max_t, NULL, FALSE);
	if (tags->len == 1 && TM_TAG(tags->pdata[0])->atts.entry.arglist)
	{
		SSM(sci, SCI_CALLTIPSHOW, pos, (sptr_t) TM_TAG(tags->pdata[0])->atts.entry.arglist);
	}

	return TRUE;
}


gboolean sci_cb_start_auto_complete(ScintillaObject *sci, gint pos)
{
	gint line = sci_get_line_from_position(sci, pos);
	gint line_start = sci_get_position_from_line(sci, line);
	gint line_len = sci_get_line_length(sci, line);
	gint line_pos = pos - line_start - 1;
	gint i = 0;
	gint current = pos - line_start;
	gint rootlen;
	gint startword = current, lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint style = SSM(sci, SCI_GETSTYLEAT, pos, 0);
	gchar linebuf[line_len + 1];
	gchar *root;
	const GPtrArray *tags;

	sci_get_line(sci, line, linebuf);

	if (lexer != SCLEX_CPP && lexer != SCLEX_HTML && lexer != SCLEX_PASCAL) return FALSE;
	if (lexer == SCLEX_HTML && style == SCE_H_DEFAULT) return FALSE;
	if (lexer == SCLEX_CPP && (style == SCE_C_COMMENT ||
			style == SCE_C_COMMENTLINE || style == SCE_C_COMMENTDOC)) return FALSE;

	while ((startword > 0) && (strchr(GEANY_WORDCHARS, linebuf[startword - 1]) || strchr(GEANY_WORDCHARS, linebuf[startword - 1])))
		startword--;
	linebuf[current] = '\0';
	root = linebuf + startword;
	rootlen = current - startword;

	if (*root == '&' && lexer == SCLEX_HTML)
	{	// HTML entities auto completion
		gchar **ents = g_strsplit(sci_cb_html_entities, " ", 0);
		guint i, j = 0, ents_len = g_strv_length(ents);
		GString *words = g_string_sized_new(60);

		for (i = 0; i < ents_len; i++)
		{
			if (! strncmp(ents[i], root, rootlen))
			{
				if (j++ > 0) g_string_append_c(words, ' ');
				g_string_append(words, ents[i]);
			}
		}
		SSM(sci, SCI_AUTOCSHOW, rootlen, (sptr_t) words->str);
		g_string_free(words, TRUE);
		g_strfreev(ents);
	}
	else
	{	// C and C++ tag autocompletion
		while (! g_ascii_isspace(linebuf[line_pos - i])) i++;
		if (i < 4) return FALSE;	// go home if typed less than 4 chars

		tags = tm_workspace_find(root, tm_tag_max_t, NULL, TRUE);
		if (NULL != tags)
		{
			GString *words = g_string_sized_new(150);
			TMTag *tag;
			guint j;
			for (j = 0; ((j < tags->len) && (j < GEANY_MAX_AUTOCOMPLETE_WORDS)); ++j)
			{
				tag = (TMTag *) tags->pdata[j];
				if (j > 0) g_string_append_c(words, ' ');
				g_string_append(words, tag->name);
			}
			SSM(sci, SCI_AUTOCSHOW, rootlen, (sptr_t) words->str);
			g_string_free(words, TRUE);
		}
	}
	return TRUE;
}


void sci_cb_auto_forif(ScintillaObject *sci, gint idx)
{
	static gchar buf[7];
	gchar *eol;
	gchar *construct;
	gint pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint style = SSM(sci, SCI_GETSTYLEAT, pos - 2, 0);
	//gint line = sci_get_line_from_position(sci, pos);
	//gint start_style = SSM(sci, SCI_GETSTYLEAT, sci_get_position_from_line(sci, line), 0);

	// only for C, C++, Java, Perl and PHP
	if (lexer != SCLEX_CPP &&
		lexer != SCLEX_HTML &&
		lexer != SCLEX_PERL)
		return;

	// return, if we are in a comment, or when SCLEX_HTML but not in PHP
	if (lexer == SCLEX_CPP && (
		style == SCE_C_COMMENT ||
	    style == SCE_C_COMMENTLINE ||
	    style == SCE_C_COMMENTDOC ||
	    style == SCE_C_COMMENTLINEDOC ||
		style == SCE_C_STRING ||
		style == SCE_C_PREPROCESSOR)) return;
	if (lexer == SCLEX_HTML && ! (style >= 118 && style <= 127)) return;

	if (lexer == SCLEX_HTML && (
		style == SCE_HPHP_SIMPLESTRING ||
		style == SCE_HPHP_HSTRING ||
		style == SCE_HPHP_COMMENTLINE ||
		style == SCE_HPHP_COMMENT)) return;

	// gets the indention
	if (app->use_auto_indention) sci_cb_get_indent(sci, pos, TRUE);
	eol = g_strconcat(utils_get_eol_char(idx), indent, NULL);
	sci_get_text_range(sci, pos - 7, pos - 1, buf);
	if (! strncmp("if", buf + 4, 2))
	{
		construct = g_strdup_printf("()%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		SSM(sci, SCI_GOTOPOS, pos + 1, 0);
		g_free(construct);
	}
	else if (! strncmp("else", buf + 2, 4))
	{
		construct = g_strdup_printf("%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		SSM(sci, SCI_GOTOPOS, pos + 4 + (2 * strlen(indent)), 0);
		g_free(construct);
	}
	else if (! strncmp("for", buf + 3, 3))
	{
		gchar *var;
		gint contruct_len;
		if (doc_list[idx].file_type->id == GEANY_FILETYPES_PHP)
		{
			var = g_strdup("$i");
			contruct_len = 14;
		}
		else
		{
			var = g_strdup("i");
			contruct_len = 12;
		}
		construct = g_strdup_printf("(%s = 0; %s < ; %s++)%s{%s\t%s}%s", var, var, var, eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		SSM(sci, SCI_GOTOPOS, pos + contruct_len, 0);
		g_free(var);
		g_free(construct);
	}
	else if (! strncmp("while", buf + 1, 5))
	{
		construct = g_strdup_printf("()%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		SSM(sci, SCI_GOTOPOS, pos + 1, 0);
		g_free(construct);
	}
	else if (! strncmp("switch", buf, 5))
	{
		construct = g_strdup_printf("()%s{%s\tcase : break;%s\tdefault: %s}%s", eol, eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		SSM(sci, SCI_GOTOPOS, pos + 1, 0);
		g_free(construct);
	}
	g_free(eol);
}


void sci_cb_show_macro_list(ScintillaObject *sci)
{
	guint j, i;
	const GPtrArray *tags;
	GPtrArray *ftags = g_ptr_array_sized_new(50);
	GString *words = g_string_sized_new(200);

	for (j = 0; j < app->tm_workspace->work_objects->len; j++)
	{
		tags = tm_tags_extract(TM_WORK_OBJECT(app->tm_workspace->work_objects->pdata[j])->tags_array,
			tm_tag_enum_t | tm_tag_variable_t | tm_tag_macro_t | tm_tag_macro_with_arg_t);
		if (NULL != tags)
		{
			for (i = 0; ((i < tags->len) && (i < GEANY_MAX_AUTOCOMPLETE_WORDS)); ++i)
			{
				g_ptr_array_add(ftags, (gpointer) tags->pdata[i]);
			}
		}
	}
	tm_tags_sort(ftags, NULL, FALSE);
	for (j = 0; j < ftags->len; j++)
	{
		if (j > 0) g_string_append_c(words, ' ');
		g_string_append(words, TM_TAG(ftags->pdata[j])->name);
	}
	SSM(sci, SCI_USERLISTSHOW, 1, (sptr_t) words->str);
	g_ptr_array_free(ftags, TRUE);
	g_string_free(words, TRUE);
}


/**
 * (stolen from anjuta and heavily modified)
 * This routine will auto complete XML or HTML tags that are still open by closing them
 * @parm ch The character we are dealing with, currently only works with the '>' character
 * @return True if handled, false otherwise
 */

gboolean sci_cb_handle_xml(ScintillaObject *sci, gchar ch)
{
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint pos, min;
	gchar *str_found, sel[512];

	// If the user has turned us off, quit now.
	// This may make sense only in certain languages
	if (! app->auto_close_xml_tags || (lexer != SCLEX_HTML && lexer != SCLEX_XML))
		return FALSE;


	// if ch is /, check for </, else quit
	pos = sci_get_current_position(sci);
	if (ch == '/' && sci_get_char_at(sci, pos - 2) != '<')
		return FALSE;

	// Grab the last 512 characters or so
	min = pos - (sizeof(sel) - 1);
	if (min < 0) min = 0;

	if (pos - min < 3)
		return FALSE; // Smallest tag is 3 characters ex. <p>

	sci_get_text_range(sci, min, pos, sel);
	sel[sizeof(sel) - 1] = '\0';

	if (ch == '>' && sel[pos - min - 2] == '/')
		// User typed something like "<br/>"
		return FALSE;

	if (ch == '/')
		str_found = utils_find_open_xml_tag(sel, pos - min, TRUE);
	else
		str_found = utils_find_open_xml_tag(sel, pos - min, FALSE);

	// when found string is something like br, img or another short tag, quit
	if (utils_strcmp(str_found, "br")
	 || utils_strcmp(str_found, "img")
	 || utils_strcmp(str_found, "base")
	 || utils_strcmp(str_found, "basefont")	// < or not <
	 || utils_strcmp(str_found, "frame")
	 || utils_strcmp(str_found, "input")
	 || utils_strcmp(str_found, "link")
	 || utils_strcmp(str_found, "area")
	 || utils_strcmp(str_found, "meta"))
	{
		return FALSE;
	}

	if (strlen(str_found) > 0)
	{
		gchar *to_insert;
		if (ch == '/')
			to_insert = g_strconcat(str_found, ">", NULL);
		else
			to_insert = g_strconcat("</", str_found, ">", NULL);
		sci_start_undo_action(sci);
		sci_replace_sel(sci, to_insert);
		if (ch == '>')
		{
			SSM(sci, SCI_SETSEL, pos, pos);
			if (utils_strcmp(str_found, "table")) sci_cb_auto_table(sci, pos);
		}
		sci_end_undo_action(sci);
		g_free(to_insert);
		g_free(str_found);
		return TRUE;
	}


	g_free(str_found);
	return FALSE;
}



void sci_cb_auto_table(ScintillaObject *sci, gint pos)
{
	gchar *table;

	if (SSM(sci, SCI_GETLEXER, 0, 0) != SCLEX_HTML) return;

	sci_cb_get_indent(sci, pos, TRUE);
	table = g_strconcat("\n", indent, indent, "<tr>\n", indent, indent, indent, "<td>\n",
								indent, indent, indent, "</td>\n", indent, indent, "</tr>\n",
								indent, NULL);
	sci_insert_text(sci, pos, table);
	g_free(table);
}


/**
 * This routine will find an uri like http://www.example.com or file:///tmp/test.c
 * @parm pos The position where to start looking for an URI
 * @return the position where the URI starts, or -1 if no URI was found
 */
gint sci_cb_handle_uri(ScintillaObject *sci, gint pos)
{
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint min, sel_start, sel_end, end_of_text, end_of_styling;
	gint line = sci_get_line_from_position(sci, pos);
	gint line_start = sci_get_position_from_line(sci, line);
	gint line_end = line_start + sci_get_line_length(sci, line) - 1;
	gchar sel[300], *begin, *cur, *uri;

	// This may make sense only in certain languages
	if ((lexer != SCLEX_HTML && lexer != SCLEX_XML))
		return -1;

	if (pos < 0) return -1;

	if (SSM(sci, SCI_GETSTYLEAT, pos, 0) == SCE_H_VALUE || SSM(sci, SCI_GETSTYLEAT, pos, 0) == SCE_H_CDATA)
		return -1;

	// Grab some characters around pos
	min = pos - 149;
	if (min < 0) min = 0;
	end_of_text = pos + 149;
	if (end_of_text > sci_get_length(sci)) end_of_text = sci_get_length(sci);

	sci_get_text_range(sci, min, end_of_text, sel);
	sel[sizeof(sel) - 1] = '\0';

	begin = &sel[0];
	cur = &sel[149];

	sel_start = pos;
	while (cur > begin)
	{
		if (*cur == '>') break;
		else if (*cur == ' ') break;
		else if (*cur == '"') break;
		else if (*cur == '=') break;
		--cur;
		--sel_start;
	}
	// stay in the current line
	if (sel_start < line_start) sel_start = line_start - 1;
	sel_end = sel_start++;

	cur++;
	while(cur > begin)
	{
		if (*cur == '\0') break;
		else if (*cur == ' ') break;
		else if (*cur == '"') break;
		else if (*cur == '<') break;
		else if (*cur == '>') break;
		sel_end++;
		cur++;
	}
	// stay in the current line
	if (sel_end > line_end) sel_end = line_end - 1;

	// check wether uri contains ://, otherwise give up
	uri = g_malloc0(sel_end - sel_start + 1);
	sci_get_text_range(sci, sel_start, sel_end, uri);
	if (strstr(uri, "://") == NULL)
	{
		g_free(uri);
		return -1;
	}
	g_free(uri);

	end_of_styling = SSM(sci, SCI_GETENDSTYLED, 0, 0);
	SSM(sci, SCI_STARTSTYLING, sel_start, 31);
	SSM(sci, SCI_SETSTYLING, (sel_end - sel_start) + 1, SCE_H_QUESTION);
	SSM(sci, SCI_STARTSTYLING, end_of_styling, 31);

	link_start = sel_start;
	link_end = sel_end + 1;
	style = SSM(sci, SCI_GETSTYLEAT, pos, 0);

	//geany_debug("pos: %d start: %d end: %d length: %d", pos, sel_start, sel_end, sel_end - sel_start);

	return -1;
}


void sci_cb_do_comment(gint idx)
{
	gint first_line = sci_get_line_from_position(doc_list[idx].sci, sci_get_selection_start(doc_list[idx].sci));
	gint last_line = sci_get_line_from_position(doc_list[idx].sci, sci_get_selection_end(doc_list[idx].sci));
	gint x, i, line_start, line_len, lexer;
	gchar sel[64];

	lexer = SSM(doc_list[idx].sci, SCI_GETLEXER, 0, 0);
	SSM(doc_list[idx].sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; i <= last_line; i++)
	{
		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		//g_message("line: %d line_start: %d len: %d (%d)", i, line_start, MIN(63, (line_len - 1)), line_len);
		sci_get_text_range(doc_list[idx].sci, line_start, MIN((line_start + 63), (line_start + line_len - 1)), sel);
		sel[MIN(63, (line_len - 1))] = '\0';

		while (isspace(sel[x])) x++;

		// to skip blank lines
		if (x < line_len && sel[x] != '\0')
		{
			// use // comments for C, C++ & Java
			if (lexer == SCLEX_CPP)
			{
				// skip lines which are already comments
				if (sel[x] == '/' && (sel[x+1] == '/' || sel[x+1] == '*')) continue;
				if (sci_get_style_at(doc_list[idx].sci, line_start + x) == SCE_C_COMMENT) continue;

				sci_insert_text(doc_list[idx].sci, line_start + x, "//");
			}
			// use /* ... */ for CSS
			else if (lexer == SCLEX_CSS)
			{
				gint end_pos;
				// skip lines which are already comments
				if (sci_get_style_at(doc_list[idx].sci, line_start + x) == SCE_CSS_COMMENT) continue;

				sci_insert_text(doc_list[idx].sci, line_start, "/* ");
				// cares about the inluding EOL chars from SCI_LINELENGTH()
				end_pos = line_start + line_len - strlen(utils_get_eol_char(idx)) + 3;
				//geany_debug("%d = %d + %d - %d + %d", end_pos, line_start, line_len, strlen(utils_get_eol_char(idx)), 3);
				sci_insert_text(doc_list[idx].sci, end_pos, " */");
			}
			// use # for Python, Perl, Shell & Makefiles
			else if (lexer == SCLEX_PYTHON
				  || lexer == SCLEX_PERL
				  || lexer == SCLEX_MAKEFILE
				  || lexer == SCLEX_PROPERTIES
				  || lexer == SCLEX_BASH)
			{
				// skip lines which are already comments
				if (sel[x] == '#') continue;
				sci_insert_text(doc_list[idx].sci, line_start + x, "#");
			}
			// use ; for Assembler, and anything else?
			else if (lexer == SCLEX_ASM)
			{
				// skip lines which are already comments
				if (sel[x] == ';') continue;
				sci_insert_text(doc_list[idx].sci, line_start + x, ";");
			}
			// use % for LaTex
			else if (lexer == SCLEX_LATEX)
			{
				// skip lines which are already comments
				if (sel[x] == '%') continue;
				sci_insert_text(doc_list[idx].sci, line_start + x, "%");
			}
			// use { ... } for Pascal
			else if (lexer == SCLEX_PASCAL)
			{
				// skip lines which are already comments
				gchar *eol = utils_get_eol_char(idx);
				gchar *str_begin = g_strdup_printf("{%s", eol);
				gchar *str_end = g_strdup_printf("}%s", eol);

				if (sci_get_style_at(doc_list[idx].sci, line_start + x) == SCE_C_COMMENT) continue;
				sci_insert_text(doc_list[idx].sci, line_start, str_begin);
				line_len = sci_get_position_from_line(doc_list[idx].sci, last_line + 2);
				sci_insert_text(doc_list[idx].sci, line_len, str_end);
				g_free(str_begin);
				g_free(str_end);
				break;
			}
			// use <!-- ... --> for [HT|X]ML, // for PHP
			else if (lexer == SCLEX_HTML || lexer == SCLEX_XML)
			{
				// skip lines which are already comments
				if (sci_get_style_at(doc_list[idx].sci, line_start) >= 118 &&
					sci_get_style_at(doc_list[idx].sci, line_start) <= 127)
				{	// PHP mode
					if (sel[x] == '/' && (sel[x+1] == '/' || sel[x+1] == '*')) continue;
					sci_insert_text(doc_list[idx].sci, line_start + x, "//");
				}
				else
				{	// HTML mode
					gchar *eol = utils_get_eol_char(idx);
					gchar *str_begin = g_strdup_printf("<!--%s", eol);
					gchar *str_end = g_strdup_printf("-->%s", eol);

					// skip lines which are already comments
					if (sci_get_style_at(doc_list[idx].sci, line_start + x) == SCE_H_COMMENT) continue;

					sci_insert_text(doc_list[idx].sci, line_start, str_begin);
					line_len = sci_get_position_from_line(doc_list[idx].sci, last_line + 2);
					sci_insert_text(doc_list[idx].sci, line_len , str_end);
					g_free(str_begin);
					g_free(str_end);
					// break because we are already on the last line
					break;
				}
			}
		}
	}
	SSM(doc_list[idx].sci, SCI_ENDUNDOACTION, 0, 0);
}


void sci_cb_highlight_braces(ScintillaObject *sci)
{
	gint cur_pos = sci_get_current_position(sci);
#if 0
	// is it useful (or performant) to check for lexer and style, only to prevent brace highlighting in comments
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint style = SSM(sci, SCI_GETSTYLEAT, cur_pos - 2, 0);

	if (lexer == SCLEX_CPP && (
		style == SCE_C_COMMENT ||
	    style == SCE_C_COMMENTLINE ||
	    style == SCE_C_COMMENTDOC ||
	    style == SCE_C_COMMENTLINEDOC ||
		style == SCE_C_STRING ||
		style == SCE_C_PREPROCESSOR)) return;
	if (lexer == SCLEX_HTML && (style == SCE_HPHP_COMMENT ||
		style == SCE_H_COMMENT ||
		style == SCE_H_SGML_COMMENT ||
		style == SCE_H_XCCOMMENT)) return;
	if (lexer == SCLEX_PASCAL && (
		style == SCE_C_COMMENT ||
	    style == SCE_C_COMMENTLINE ||
	    style == SCE_C_COMMENTDOC ||
	    style == SCE_C_COMMENTLINEDOC)) return;
#endif
	if (utils_isbrace(sci_get_char_at(sci, cur_pos)) || utils_isbrace(sci_get_char_at(sci, cur_pos - 1)))
	{
		gint end_pos = SSM(sci, SCI_BRACEMATCH, cur_pos, 0);
		gint end_pos_prev = SSM(sci, SCI_BRACEMATCH, (cur_pos - 1), 0);
		if (end_pos >= 0)
			SSM(sci, SCI_BRACEHIGHLIGHT, cur_pos, end_pos);
		else if (end_pos_prev >= 0)
			SSM(sci, SCI_BRACEHIGHLIGHT, (cur_pos - 1), end_pos_prev);
		else
		{
			if (utils_isbrace(sci_get_char_at(sci, cur_pos)))
				SSM(sci, SCI_BRACEBADLIGHT, cur_pos, 0);
			if (utils_isbrace(sci_get_char_at(sci, cur_pos - 1)))
				SSM(sci, SCI_BRACEBADLIGHT, cur_pos - 1, 0);
		}
	}
	else
	{
		SSM(sci, SCI_BRACEBADLIGHT, -1, 0);
	}
}


void sci_cb_auto_multiline(ScintillaObject *sci, gint pos)
{
	gint style = SSM(sci, SCI_GETSTYLEAT, pos - 2, 0);
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint i = pos;

	if (((lexer == SCLEX_CPP && style == SCE_C_COMMENT) ||
		(lexer == SCLEX_HTML && style == SCE_HPHP_COMMENT)))
	{
		while (isspace(sci_get_char_at(sci, i))) i--;
		if (sci_get_char_at(sci, i) == '/' && sci_get_char_at(sci, i - 1) == '*') return;

		if (strlen(indent) == 0)
		{	// if strlen(indent) is 0, there is no indentation, but should
			sci_add_text(sci, " * ");
		}
		else
		{
			sci_add_text(sci, "* ");
		}
	}
}


