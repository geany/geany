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

static gchar indent[100];


// callback func called by all editors when a signal arises

void on_editor_notification(GtkWidget *editor, gint scn, gpointer lscn, gpointer user_data)
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
			utils_beep();
			break;
		}
		case SCN_MARGINCLICK:
		{
			// left click to marker margin marks the line
			if (nt->margin == 1)
			{
				gint line = sci_get_line_from_position(sci, nt->position);
				//sci_marker_delete_all(doc_list[idx].sci, 1);
				sci_set_marker_at_line(sci, line, sci_is_marker_set_at_line(sci, line, 1), 1);
			}
			else if (nt->margin == 2 && app->pref_editor_folding)
			{
				SSM(sci, SCI_TOGGLEFOLD, SSM(sci, SCI_LINEFROMPOSITION, nt->position, 0), 0);
			}
			break;
		}
		case SCN_UPDATEUI:
		{
			gint pos = sci_get_current_position(sci);

			// undo / redo menu update
			utils_update_popup_reundo_items(idx);

			// brace highlighting
			sci_cb_highlight_braces(sci, pos);

			utils_update_statusbar(idx, pos);

#if 0
			/// experimental code for inverting selections
			{
			gint i;
			for (i = SSM(sci, SCI_GETSELECTIONSTART, 0, 0); i < SSM(sci, SCI_GETSELECTIONEND, 0, 0); i++)
			{
				// need to get colour from getstyleat(), but how?
				SSM(sci, SCI_STYLESETFORE, STYLE_DEFAULT, 0);
				SSM(sci, SCI_STYLESETBACK, STYLE_DEFAULT, 0);
			}

			sci_get_style_at(sci, pos);
			}
#endif
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
					if (doc_list[idx].use_auto_indention && (sci_get_eol_mode(sci) == SC_EOL_CR))
					{
						sci_cb_get_indent(sci, pos, FALSE);
						sci_add_text(sci, indent);
					}
					// " * " auto completion in multiline C/C++ comments
					if (sci_get_eol_mode(sci) == SC_EOL_CR)
					{
						sci_cb_auto_multiline(sci, pos);
						if (app->pref_editor_auto_complete_constructs) sci_cb_auto_latex(sci, pos, idx);
					}
					break;
				}
				case '\n':
				{	// simple indentation (for CR/LF and LF format)
					if (doc_list[idx].use_auto_indention)
					{
						sci_cb_get_indent(sci, pos, FALSE);
						sci_add_text(sci, indent);
					}
					// " * " auto completion in multiline C/C++ comments
					sci_cb_auto_multiline(sci, pos);
					if (app->pref_editor_auto_complete_constructs) sci_cb_auto_latex(sci, pos, idx);
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
					if (app->pref_editor_auto_complete_constructs) sci_cb_auto_forif(sci, pos, idx);
					break;
				}
				case '[':
				case '{':
				{	// Tex auto-closing
					sci_cb_auto_close_bracket(sci, pos, nt->ch);	// Tex auto-closing
					sci_cb_show_calltip(sci, pos);
					break;
				}
				case '}':
				{	// closing bracket handling
					if (doc_list[idx].use_auto_indention) sci_cb_close_block(sci, pos - 1);
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
/*		case SCN_STYLENEEDED:
		{
			geany_debug("style");
			break;
		}
*/		case SCN_URIDROPPED:
		{
			if (nt->text != NULL && strlen(nt->text) > 0)
			{
				gint i;
				gchar *filename;
				gchar **list;

				switch (utils_get_line_endings((gchar*) nt->text, strlen(nt->text)))
				{
					case SC_EOL_CR: list = g_strsplit(nt->text, "\r", 0); break;
					case SC_EOL_CRLF: list = g_strsplit(nt->text, "\r\n", 0); break;
					case SC_EOL_LF: list = g_strsplit(nt->text, "\n", 0); break;
					default: list = g_strsplit(nt->text, "\n", 0);
				}

				for (i = 0; ; i++)
				{
					if (list[i] == NULL) break;
					filename = g_filename_from_uri(list[i], NULL, NULL);
					if (filename == NULL) continue;
					document_open_file(-1, filename, 0, FALSE, NULL);
					g_free(filename);
				}

				g_strfreev(list);
			}
			break;
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
		if (j == sizeof(indent) - 1) break;
		else if (linebuf[i] == ' ' || linebuf[i] == '\t') indent[j++] = linebuf[i];
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
	gchar *text, *line_buf;

	if (lexer != SCLEX_CPP && lexer != SCLEX_HTML && lexer != SCLEX_PASCAL) return;

	// check that the line is empty, to not kill text in the line
	line_buf = g_malloc(line_len + 1);
	sci_get_line(sci, line, line_buf);
	line_buf[line_len - eol_char_len] = '\0';
	while (x < (line_len - eol_char_len))
	{
		if (isspace(line_buf[x])) cnt++;
		x++;
	}
	g_free(line_buf);

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
	gint lexer;
	gint style;
	gchar word[GEANY_MAX_WORD_LENGTH];
	gint idx;
	const GPtrArray *tags;

	if (sci == NULL) return FALSE;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	idx = document_find_by_sci(sci);
	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return FALSE;

	word[0] = '\0';
	if (pos == -1)
	{
		gchar c;
		// position of '(' is unknown, so go backwards to find it
		pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
		// I'm not sure if utils_is_opening_brace() is a good idea, but it is the simplest way,
		// but we need something more intelligent than only check for '(' because e.g. LaTeX
		// uses {, [ or (
		c = SSM(sci, SCI_GETCHARAT, pos, 0);
		while (pos > 0 && ! utils_is_opening_brace(c) && c != ';')
		{
			c = SSM(sci, SCI_GETCHARAT, pos, 0);
			pos--;
		}
	}

	style = SSM(sci, SCI_GETSTYLEAT, pos, 0);
	if (lexer == SCLEX_CPP && (style == SCE_C_COMMENT ||
			style == SCE_C_COMMENTLINE || style == SCE_C_COMMENTDOC)) return FALSE;

	utils_find_current_word(sci, pos - 1, word, sizeof word);
	if (word[0] == '\0') return FALSE;

	tags = tm_workspace_find(word, tm_tag_max_t, NULL, FALSE, doc_list[idx].file_type->lang);
	if (tags->len == 1 && TM_TAG(tags->pdata[0])->atts.entry.arglist)
	{
		SSM(sci, SCI_CALLTIPSHOW, pos, (sptr_t) TM_TAG(tags->pdata[0])->atts.entry.arglist);
	}

	return TRUE;
}


gboolean sci_cb_start_auto_complete(ScintillaObject *sci, gint pos)
{
	gint line, line_start, line_len, line_pos, current, rootlen, startword, lexer, style;
	gchar *linebuf, *root;
	const GPtrArray *tags;

	if (sci == NULL) return FALSE;

	line = sci_get_line_from_position(sci, pos);
	line_start = sci_get_position_from_line(sci, line);
	line_len = sci_get_line_length(sci, line);
	line_pos = pos - line_start - 1;
	current = pos - line_start;
	startword = current;
	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	style = SSM(sci, SCI_GETSTYLEAT, pos, 0);

	//if (lexer != SCLEX_CPP && lexer != SCLEX_HTML && lexer != SCLEX_PASCAL) return FALSE;
	if (lexer == SCLEX_HTML && style == SCE_H_DEFAULT) return FALSE;
	if (lexer == SCLEX_CPP && (style == SCE_C_COMMENT ||
			style == SCE_C_COMMENTLINE || style == SCE_C_COMMENTDOC)) return FALSE;

	linebuf = g_malloc(line_len + 1);
	sci_get_line(sci, line, linebuf);

	// find the start of the current word
	while ((startword > 0) && (strchr(GEANY_WORDCHARS, linebuf[startword - 1])))
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
	{	// PHP, LaTeX, C and C++ tag autocompletion
		gint i = 0;
		gint idx = document_find_by_sci(sci);
		TMTagAttrType attrs[] = { tm_tag_attr_name_t, 0 };

		if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL)
		{	// go home if typed less than 4 chars
			g_free(linebuf);
			return FALSE;
		}

		while ((line_pos - i >= 0) && ! g_ascii_isspace(linebuf[line_pos - i])) i++;
		if (i < 4)
		{	// go home if typed less than 4 chars
			g_free(linebuf);
			return FALSE;
		}

		tags = tm_workspace_find(root, tm_tag_max_t, attrs, TRUE, doc_list[idx].file_type->lang);
		if (NULL != tags && tags->len > 0)
		{
			GString *words = g_string_sized_new(150);
			guint j;

			for (j = 0; ((j < tags->len) && (j < GEANY_MAX_AUTOCOMPLETE_WORDS)); ++j)
			{
				if (j > 0) g_string_append_c(words, ' ');
				g_string_append(words, ((TMTag *) tags->pdata[j])->name);
			}
			SSM(sci, SCI_AUTOCSHOW, rootlen, (sptr_t) words->str);
			//geany_debug("string size: %d", words->len);
			g_string_free(words, TRUE);
		}
	}
	g_free(linebuf);
	return TRUE;
}


void sci_cb_auto_latex(ScintillaObject *sci, gint pos, gint idx)
{
	// currently disabled
#if 0
	if (sci_get_char_at(sci, pos - 2) == '}')
	{
		gchar *eol, *buf, *construct;
		gchar env[30]; /// FIXME are 30 chars enough?
		gint line = sci_get_line_from_position(sci, pos - 2);
		gint line_len = sci_get_line_length(sci, line);
		gint i, start;

		// get the line
		buf = g_malloc0(line_len + 1);
		sci_get_line(sci, line, buf);

		// get to the first non-blank char (some kind of ltrim())
		i = start = 0;
		while (isspace(buf[i++])) start++;

		// check for begin
		if (strncmp(buf + start, "\\begin", 6) == 0)
		{
			// goto through the line and get the environment, begin at first non-blank char (= start)
			for (i = start; i < line_len; i++)
			{
				if (buf[i] == '{')
				{
					gint j = 0;
					i++;
					while (buf[i] != '}')
					{	// this could be done in a shorter way, but so it resists readable ;-)
						env[j] = buf[i];
						j++;
						i++;
					}
					env[j] = '\0';
					break;
				}
			}

			// get the indention
			if (doc_list[idx].use_auto_indention) sci_cb_get_indent(sci, pos, TRUE);
			eol = g_strconcat(utils_get_eol_char(idx), indent, NULL);

			construct = g_strdup_printf("%s\\end{%s}", eol, env);

			SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
			sci_goto_pos(sci, pos + 1, TRUE);
			g_free(construct);
			g_free(buf);
			g_free(eol);
		}
		else
		{	// later there could be some else ifs for other keywords
			return;
		}
	}
#endif
}


void sci_cb_auto_forif(ScintillaObject *sci, gint pos, gint idx)
{
	static gchar buf[8];
	gchar *eol;
	gchar *construct;
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

	// get the indention
	if (doc_list[idx].use_auto_indention) sci_cb_get_indent(sci, pos, TRUE);
	eol = g_strconcat(utils_get_eol_char(idx), indent, NULL);
	sci_get_text_range(sci, pos - 8, pos - 1, buf);
	// "pattern", buf + x, y -> x + y = 7, because buf is (pos - 8)...(pos - 1) = 7
	if (! strncmp("if", buf + 5, 2))
	{
		if (! isspace(*(buf + 4))) goto free_and_return;

		construct = g_strdup_printf("()%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}
	else if (! strncmp("else", buf + 3, 4))
	{
		if (! isspace(*(buf + 2))) goto free_and_return;

		construct = g_strdup_printf("%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 4 + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("for", buf + 4, 3))
	{
		gchar *var;
		gint contruct_len;

		if (! isspace(*(buf + 3))) goto free_and_return;

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
		construct = g_strdup_printf("(%s%s = 0; %s < ; %s++)%s{%s\t%s}%s",
						(doc_list[idx].file_type->id == GEANY_FILETYPES_CPP) ? "int " : "",
						var, var, var, eol, eol, eol, eol);

		// add 4 characters because of "int " in C++ mode
		contruct_len += (doc_list[idx].file_type->id == GEANY_FILETYPES_CPP) ? 4 : 0;

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + contruct_len, TRUE);
		g_free(var);
		g_free(construct);
	}
	else if (! strncmp("while", buf + 2, 5))
	{
		if (! isspace(*buf + 1)) goto free_and_return;

		construct = g_strdup_printf("()%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}
	else if (! strncmp("do", buf + 5, 2))
	{
		if (! isspace(*(buf + 4))) goto free_and_return;

		construct = g_strdup_printf("%s{%s\t%s}%swhile ();%s", eol, eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 4 + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("try", buf + 4, 3))
	{
		if (! isspace(*(buf + 3))) goto free_and_return;

		construct = g_strdup_printf("%s{%s\t%s}%scatch ()%s{%s\t%s}%s",
							eol, eol, eol, eol, eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 4 + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("switch", buf + 1, 6))
	{
		if (! isspace(*buf)) goto free_and_return;

		construct = g_strdup_printf("()%s{%s\tcase : break;%s\tdefault: %s}%s", eol, eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}

	free_and_return:
	g_free(eol);
}


void sci_cb_show_macro_list(ScintillaObject *sci)
{
	guint j, i;
	const GPtrArray *tags;
	GPtrArray *ftags;
	GString *words;

	if (sci == NULL) return;

	ftags = g_ptr_array_sized_new(50);
	words = g_string_sized_new(200);

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
	if (! app->pref_editor_auto_close_xml_tags || (lexer != SCLEX_HTML && lexer != SCLEX_XML))
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


void sci_cb_do_comment(gint idx)
{
	gint first_line;
	gint last_line;
	gint x, i, line_start, line_len;
	gchar sel[64], *co, *cc;
	gboolean break_loop = FALSE;
	filetype *ft;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	ft = doc_list[idx].file_type;

	first_line = sci_get_line_from_position(doc_list[idx].sci,
		sci_get_selection_start(doc_list[idx].sci));
	// Find the last line with chars selected (not EOL char)
	last_line = sci_get_line_from_position(doc_list[idx].sci,
		sci_get_selection_end(doc_list[idx].sci) - 1);
	last_line = MAX(first_line, last_line);

	// hack for detection of HTML vs PHP code, if non-PHP set filetype to XML
	line_start = sci_get_position_from_line(doc_list[idx].sci, first_line);
	if (ft->id == GEANY_FILETYPES_PHP)
	{
		if (sci_get_style_at(doc_list[idx].sci, line_start) < 118 ||
			sci_get_style_at(doc_list[idx].sci, line_start) > 127)
			ft = filetypes[GEANY_FILETYPES_XML];
	}

	co = ft->comment_open;
	cc = ft->comment_close;
	if (co == NULL) return;

	SSM(doc_list[idx].sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		//geany_debug("line: %d line_start: %d len: %d (%d)", i, line_start, MIN(63, (line_len - 1)), line_len);
		sci_get_text_range(doc_list[idx].sci, line_start, MIN((line_start + 63), (line_start + line_len - 1)), sel);
		sel[MIN(63, (line_len - 1))] = '\0';

		while (isspace(sel[x])) x++;

		// to skip blank lines
		if (x < line_len && sel[x] != '\0')
		{
			// use single line comment
			if (cc == NULL || strlen(cc) == 0)
			{
				gboolean do_continue = FALSE;
				switch (strlen(co))
				{
					case 1: if (sel[x] == co[0]) do_continue = TRUE; break;
					case 2: if (sel[x] == co[0] && sel[x+1] == co[1]) do_continue = TRUE; break;
					case 3: if (sel[x] == co[0] && sel[x+1] == co[1] && sel[x+2] == co[2])
								do_continue = TRUE;	break;
					default: return;
				}
				if (do_continue) continue;

				if (ft->comment_use_indent)
					sci_insert_text(doc_list[idx].sci, line_start + x, co);
				else
					sci_insert_text(doc_list[idx].sci, line_start, co);
			}
			// use multi line comment
			else
			{
				gchar *eol = utils_get_eol_char(idx);
				gchar *str_begin = g_strdup_printf("%s%s", co, eol);
				gchar *str_end = g_strdup_printf("%s%s", cc, eol);
				gint style_comment;
				gint lexer = SSM(doc_list[idx].sci, SCI_GETLEXER, 0, 0);

				// skip lines which are already comments
				switch (lexer)
				{	// I will list only those lexers which support multi line comments
					case SCLEX_XML:
					case SCLEX_HTML:
					{
						if (sci_get_style_at(doc_list[idx].sci, line_start) >= 118 &&
							sci_get_style_at(doc_list[idx].sci, line_start) <= 127)
							style_comment = SCE_HPHP_COMMENT;
						else style_comment = SCE_H_COMMENT;
						break;
					}
					case SCLEX_CSS: style_comment = SCE_CSS_COMMENT; break;
					case SCLEX_SQL: style_comment = SCE_SQL_COMMENT; break;
					case SCLEX_CAML: style_comment = SCE_CAML_COMMENT; break;
					case SCLEX_CPP:
					case SCLEX_PASCAL:
					default: style_comment = SCE_C_COMMENT;
				}
				if (sci_get_style_at(doc_list[idx].sci, line_start + x) == style_comment) continue;

				// insert the comment strings
				sci_insert_text(doc_list[idx].sci, line_start, str_begin);
				line_len = sci_get_position_from_line(doc_list[idx].sci, last_line + 2);
				sci_insert_text(doc_list[idx].sci, line_len, str_end);

				g_free(str_begin);
				g_free(str_end);

				// break because we are already on the last line
				break_loop = TRUE;
				break;
			}
		}
	}
	SSM(doc_list[idx].sci, SCI_ENDUNDOACTION, 0, 0);
}


void sci_cb_highlight_braces(ScintillaObject *sci, gint cur_pos)
{
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


