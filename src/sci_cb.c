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

#include "SciLexer.h"
#include "geany.h"

#include "sci_cb.h"
#include "document.h"
#include "sciwrappers.h"
#include "ui_utils.h"
#include "utils.h"
#include "main.h"

static gchar current_word[GEANY_MAX_WORD_LENGTH];	// holds word under the mouse or keyboard cursor

EditorInfo editor_info = {current_word, -1};

static struct
{
	gchar *text;
	gboolean set;
} calltip = {NULL, FALSE};

static gchar indent[100];


static void on_new_line_added(ScintillaObject *sci, gint idx);


// calls the edit popup menu in the editor
gboolean
on_editor_button_press_event           (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{
	gint idx = GPOINTER_TO_INT(user_data);
	editor_info.click_pos = sci_get_position_from_xy(doc_list[idx].sci, event->x, event->y, FALSE);

#ifndef G_OS_WIN32
	if (event->button == 1)
	{
		return utils_check_disk_status(idx);
	}
#endif

	if (event->button == 3)
	{
		sci_cb_find_current_word(doc_list[idx].sci, editor_info.click_pos,
			current_word, sizeof current_word);

		ui_update_popup_goto_items((current_word[0] != '\0') ? TRUE : FALSE);
		ui_update_popup_copy_items(idx);
		ui_update_insert_include_item(idx, 0);
		gtk_menu_popup(GTK_MENU(app->popup_menu), NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}
	return FALSE;
}


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
			ui_update_popup_reundo_items(idx);

			// brace highlighting
			sci_cb_highlight_braces(sci, pos);

			ui_update_statusbar(idx, pos);

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
		case 2023:
		{
			geany_debug("Undo notification");
			break;
		}
/*		case SCN_KEY:
		{
			//geany_debug("key notification triggered with %c", nt->ch);
			break;
		}
		case SCN_MODIFIED:
		{
			if (nt->modificationType  & SC_MOD_INSERTTEXT ||
				nt->modificationType & SC_MOD_DELETETEXT)
			{
				document_undo_add(idx, UNDO_SCINTILLA, NULL);
			}
			break;
		}
*/		case SCN_CHARADDED:
		{
			gint pos = sci_get_current_position(sci);

			switch (nt->ch)
			{
				case '\r':
				{	// simple indentation (only for CR format)
					if (sci_get_eol_mode(sci) == SC_EOL_CR)
						on_new_line_added(sci, idx);
					break;
				}
				case '\n':
				{	// simple indentation (for CR/LF and LF format)
					on_new_line_added(sci, idx);
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
					sci_cb_show_calltip(idx, pos);
					break;
				}
				case ')':
				{	// hide calltips
					if (SSM(sci, SCI_CALLTIPACTIVE, 0, 0))
					{
						SSM(sci, SCI_CALLTIPCANCEL, 0, 0);
					}
					g_free(calltip.text);
					calltip.text = NULL;
					calltip.set = FALSE;
					break;
				}
				case ' ':
				{	// if and for autocompletion
					if (app->pref_editor_auto_complete_constructs) sci_cb_auto_forif(idx, pos);
					break;
				}
				case '[':
				case '{':
				{	// Tex auto-closing
					sci_cb_auto_close_bracket(sci, pos, nt->ch);	// Tex auto-closing
					sci_cb_show_calltip(idx, pos);
					break;
				}
				case '}':
				{	// closing bracket handling
					if (doc_list[idx].use_auto_indention) sci_cb_close_block(idx, pos - 1);
					break;
				}
				default: sci_cb_start_auto_complete(idx, pos, FALSE);
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
		case SCN_AUTOCSELECTION:
		{
			// now that autocomplete is finishing, reshow calltips if they were showing
			if (calltip.set)
			{
				gint pos = sci_get_current_position(sci);
				SSM(sci, SCI_CALLTIPSHOW, pos, (sptr_t) calltip.text);
				// now autocompletion has been cancelled, so do it manually
				sci_set_selection_start(sci, nt->lParam);
				sci_set_selection_end(sci, pos);
				sci_replace_sel(sci, "");	// clear root of word
				SSM(sci, SCI_INSERTTEXT, nt->lParam, (sptr_t) nt->text);
				sci_goto_pos(sci, nt->lParam + strlen(nt->text), FALSE);
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
					document_open_file(-1, filename, 0, FALSE, NULL, NULL);
					g_free(filename);
				}

				g_strfreev(list);
			}
			break;
		}
	}
}


static void on_new_line_added(ScintillaObject *sci, gint idx)
{
	gint pos = sci_get_current_position(sci);

	// simple indentation
	if (doc_list[idx].use_auto_indention)
	{
		sci_cb_get_indent(sci, pos, FALSE);
		sci_add_text(sci, indent);
	}
	// " * " auto completion in multiline C/C++ comments
	sci_cb_auto_multiline(sci, pos);
	if (app->pref_editor_auto_complete_constructs) sci_cb_auto_latex(idx, pos);
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


/* Finds a corresponding matching brace to the given pos
 * (this is taken from Scintilla Editor.cxx,
 * fit to work with sci_cb_close_block) */
static gint brace_match(ScintillaObject *sci, gint pos)
{
	gchar chBrace = sci_get_char_at(sci, pos);
	gchar chSeek = utils_brace_opposite(chBrace);
	gint direction, styBrace, depth = 1;

	if (chSeek == '\0') return -1;
	styBrace = sci_get_style_at(sci, pos);
	direction = -1;

	if (chBrace == '(' || chBrace == '[' || chBrace == '{' || chBrace == '<')
		direction = 1;

	pos = pos + direction;
	while ((pos >= 0) && (pos < sci_get_length(sci)))
	{
		gchar chAtPos = sci_get_char_at(sci, pos - 1);
		gint styAtPos = sci_get_style_at(sci, pos);

		if ((pos > sci_get_end_styled(sci)) || (styAtPos == styBrace))
		{
			if (chAtPos == chBrace)
				depth++;
			if (chAtPos == chSeek)
				depth--;
			if (depth == 0)
				return pos;
		}
		pos = pos + direction;
	}
	return - 1;
}


void sci_cb_close_block(gint idx, gint pos)
{
	gint x = 0, cnt = 0;
	gint start_brace, line, line_start, line_len, eol_char_len, lexer;
	gchar *text, *line_buf;
	ScintillaObject *sci;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;

	sci = doc_list[idx].sci;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	if (lexer != SCLEX_CPP && lexer != SCLEX_HTML && lexer != SCLEX_PASCAL && lexer != SCLEX_BASH)
			return;

	start_brace = brace_match(sci, pos);
	line = sci_get_line_from_position(sci, pos);
	line_start = sci_get_position_from_line(sci, line);
	line_len = sci_get_line_length(sci, line);
	// set eol_char_len to 0 if on last line, because there is no EOL char
	eol_char_len = (line == (SSM(sci, SCI_GETLINECOUNT, 0, 0) - 1)) ? 0 :
								utils_get_eol_char_len(document_find_by_sci(sci));

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


void sci_cb_find_current_word(ScintillaObject *sci, gint pos, gchar *word, size_t wordlen)
{
	gint line = sci_get_line_from_position(sci, pos);
	gint line_start = sci_get_position_from_line(sci, line);
	gint startword = pos - line_start;
	gint endword = pos - line_start;
	gchar *chunk = g_malloc(sci_get_line_length(sci, line) + 1);

	word[0] = '\0';
	sci_get_line(sci, line, chunk);
	chunk[sci_get_line_length(sci, line)] = '\0';

	while (startword > 0 && strchr(GEANY_WORDCHARS, chunk[startword - 1]))
		startword--;
	while (chunk[endword] && strchr(GEANY_WORDCHARS, chunk[endword]))
		endword++;
	if(startword == endword)
		return;

	chunk[endword] = '\0';

	g_strlcpy(word, chunk + startword, wordlen); //ensure null terminated
	g_free(chunk);
}


gboolean sci_cb_show_calltip(gint idx, gint pos)
{
	gint orig_pos = pos; // the position for the calltip
	gint lexer;
	gint style;
	gchar word[GEANY_MAX_WORD_LENGTH];
	TMTag *tag;
	const GPtrArray *tags;
	ScintillaObject *sci;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return FALSE;
	sci = doc_list[idx].sci;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);

	word[0] = '\0';
	if (pos == -1)
	{
		gchar c;
		// position of '(' is unknown, so go backwards to find it
		pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
		orig_pos = pos;
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

	sci_cb_find_current_word(sci, pos - 1, word, sizeof word);
	if (word[0] == '\0') return FALSE;

	tags = tm_workspace_find(word, tm_tag_max_t, NULL, FALSE, doc_list[idx].file_type->lang);
	if (tags->len == 1 && TM_TAG(tags->pdata[0])->atts.entry.arglist)
	{
		tag = TM_TAG(tags->pdata[0]);
		g_free(calltip.text);	// free the old calltip
		calltip.set = TRUE;
		if (tag->atts.entry.var_type)
			calltip.text = g_strconcat(tag->atts.entry.var_type, " ", tag->name,
										 " ", tag->atts.entry.arglist, NULL);
		else
			calltip.text = g_strconcat(tag->name, " ", tag->atts.entry.arglist, NULL);

		utils_wrap_string(calltip.text, -1);
		SSM(sci, SCI_CALLTIPSHOW, orig_pos, (sptr_t) calltip.text);
	}

	return TRUE;
}


static void show_autocomplete(ScintillaObject *sci, gint rootlen, const gchar *words)
{
	// store whether a calltip is showing, so we can reshow it after autocompletion
	calltip.set = SSM(sci, SCI_CALLTIPACTIVE, 0, 0);
	SSM(sci, SCI_AUTOCSHOW, rootlen, (sptr_t) words);
}


static gboolean
autocomplete_html(ScintillaObject *sci, const gchar *root, gsize rootlen)
{	// HTML entities auto completion
	guint i, j = 0;
	GString *words;

	if (*root != '&') return FALSE;
	if (html_entities == NULL) return FALSE;

	words = g_string_sized_new(500);
	for (i = 0; ; i++)
	{
		if (html_entities[i] == NULL) break;
		else if (html_entities[i][0] == '#') continue;

		if (! strncmp(html_entities[i], root, rootlen))
		{
			if (j++ > 0) g_string_append_c(words, ' ');
			g_string_append(words, html_entities[i]);
		}
	}
	if (words->len > 0) show_autocomplete(sci, rootlen, words->str);
	g_string_free(words, TRUE);
	return TRUE;
}


static gboolean
autocomplete_tags(gint idx, gchar *root, gsize rootlen)
{	// PHP, LaTeX, C and C++ tag autocompletion
	TMTagAttrType attrs[] = { tm_tag_attr_name_t, 0 };
	const GPtrArray *tags;
	ScintillaObject *sci;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL)
		return FALSE;
	sci = doc_list[idx].sci;

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
		show_autocomplete(sci, rootlen, words->str);
		//geany_debug("string size: %d", words->len);
		g_string_free(words, TRUE);
	}
	return TRUE;
}


gboolean sci_cb_start_auto_complete(gint idx, gint pos, gboolean force)
{
	gint line, line_start, line_len, line_pos, current, rootlen, startword, lexer, style;
	gchar *linebuf, *root;
	ScintillaObject *sci;
	gboolean ret = FALSE;

	if (idx < 0 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return FALSE;
	sci = doc_list[idx].sci;

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

	if (lexer == SCLEX_HTML)
		ret = autocomplete_html(sci, root, rootlen);
	else
	{
		// force is set when called by keyboard shortcut, otherwise start at the 4th char
		if (force || rootlen >= 4)
			ret = autocomplete_tags(idx, root, rootlen);
	}

	g_free(linebuf);
	return ret;
}


void sci_cb_auto_latex(gint idx, gint pos)
{
	ScintillaObject *sci;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;
	sci = doc_list[idx].sci;

	if (sci_get_char_at(sci, pos - 2) == '}')
	{
		gchar *eol, *buf, *construct;
		gchar env[50];
		gint line = sci_get_line_from_position(sci, pos - 2);
		gint line_len = sci_get_line_length(sci, line);
		gint i, start;

		// get the line
		buf = g_malloc0(line_len + 1);
		sci_get_line(sci, line, buf);

		// get to the first non-blank char (some kind of ltrim())
		start = 0;
		//while (isspace(buf[i++])) start++;
		while (isspace(buf[start])) start++;

		// check for begin
		if (strncmp(buf + start, "\\begin", 6) == 0)
		{
			gchar full_cmd[15];
			guint j = 0;

			// take also "\begingroup" (or whatever there can be) and append "\endgroup" and so on.
			i = start + 6;
			while (buf[i] != '{' && j < (sizeof(full_cmd) - 1))
			{	// copy all between "\begin" and "{" to full_cmd
				full_cmd[j] = buf[i];
				i++;
				j++;
			}
			full_cmd[j] = '\0';

			// go through the line and get the environment
			for (i = start + j; i < line_len; i++)
			{
				if (buf[i] == '{')
				{
					j = 0;
					i++;
					while (buf[i] != '}' && j < (sizeof(env) - 1))
					{	// this could be done in a shorter way, but so it remains readable ;-)
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

			construct = g_strdup_printf("%s\\end%s{%s}", eol, full_cmd, env);

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
}


void sci_cb_auto_forif(gint idx, gint pos)
{
	static gchar buf[16];
	gchar *eol;
	gchar *construct;
	gint lexer, style;
	gint i;
	ScintillaObject *sci;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;
	sci = doc_list[idx].sci;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	style = SSM(sci, SCI_GETSTYLEAT, pos - 2, 0);

	// only for C, C++, Java, Perl and PHP
	if (doc_list[idx].file_type->id != GEANY_FILETYPES_PHP &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_C &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_D &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_CPP &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_PERL &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_JAVA &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_FERITE)
		return;

	// return, if we are in a comment, or when SCLEX_HTML but not in PHP
	if (lexer == SCLEX_CPP && (
		style == SCE_C_COMMENT ||
	    style == SCE_C_COMMENTLINE ||
	    style == SCE_C_COMMENTDOC ||
	    style == SCE_C_COMMENTLINEDOC ||
		style == SCE_C_STRING ||
		style == SCE_C_CHARACTER ||
		style == SCE_C_PREPROCESSOR)) return;
	//if (lexer == SCLEX_HTML && ! (style >= 118 && style <= 127)) return;

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_PHP && (
		style == SCE_HPHP_SIMPLESTRING ||
		style == SCE_HPHP_HSTRING ||
		style == SCE_HPHP_COMMENTLINE ||
		style == SCE_HPHP_COMMENT)) return;

	// get the indention
	if (doc_list[idx].use_auto_indention) sci_cb_get_indent(sci, pos, TRUE);
	eol = g_strconcat(utils_get_eol_char(idx), indent, NULL);
	sci_get_text_range(sci, pos - 16, pos - 1, buf);
	// check the first 8 characters of buf for whitespace, but only in this line
	i = 14;
	while (isalpha(buf[i])) i--;	// find pos before keyword
	while (i >= 0 && buf[i] != '\n' && buf[i] != '\r')	// we want to keep in this line('\n' check)
	{
		if (! isspace(buf[i])) goto free_and_return;
		i--;
	}

	// "pattern", buf + x, y -> x + y = 15, because buf is (pos - 16)...(pos - 1) = 15
	if (! strncmp("if", buf + 13, 2))
	{
		if (! isspace(*(buf + 12))) goto free_and_return;

		construct = g_strdup_printf("()%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}
	else if (! strncmp("else", buf + 11, 4))
	{
		if (! isspace(*(buf + 10))) goto free_and_return;

		construct = g_strdup_printf("%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 4 + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("for", buf + 12, 3))
	{
		gchar *var;
		gint contruct_len;

		if (! isspace(*(buf + 11))) goto free_and_return;

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
	else if (! strncmp("while", buf + 10, 5))
	{
		if (! isspace(*(buf + 9))) goto free_and_return;

		construct = g_strdup_printf("()%s{%s\t%s}%s", eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}
	else if (! strncmp("do", buf + 13, 2))
	{
		if (! isspace(*(buf + 12))) goto free_and_return;

		construct = g_strdup_printf("%s{%s\t%s}%swhile ();%s", eol, eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 4 + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("try", buf + 12, 3))
	{
		if (! isspace(*(buf + 11))) goto free_and_return;

		construct = g_strdup_printf("%s{%s\t%s}%scatch ()%s{%s\t%s}%s",
							eol, eol, eol, eol, eol, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 4 + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("switch", buf + 9, 6))
	{
		if (! isspace(*(buf + 8))) goto free_and_return;

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
	gint indent_pos;

	if (SSM(sci, SCI_GETLEXER, 0, 0) != SCLEX_HTML) return;

	sci_cb_get_indent(sci, pos, TRUE);
	indent_pos = sci_get_line_indent_position(sci, sci_get_line_from_position(sci, pos));
	if ((pos - 7) != indent_pos) // 7 == strlen("<table>")
	{
		gint i, x;
		x = strlen(indent);
		// find the start of the <table tag
		i = 1;
		while (sci_get_char_at(sci, pos - i) != '<') i++;
		// add all non whitespace before the tag to the indent string
		while ((pos - i) != indent_pos)
		{
			indent[x++] = ' ';
			i++;
		}
		indent[x] = '\0';
	}

	table = g_strconcat("\n", indent, "    <tr>\n", indent, "        <td>\n", indent, "        </td>\n",
						indent, "    </tr>\n", indent, NULL);
	sci_insert_text(sci, pos, table);
	g_free(table);
}


static void real_comment_multiline(gint idx, gint line_start, gint last_line)
{
	gchar *eol, *str_begin, *str_end;
	gint line_len;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;

	eol = utils_get_eol_char(idx);
	str_begin = g_strdup_printf("%s%s", doc_list[idx].file_type->comment_open, eol);
	str_end = g_strdup_printf("%s%s", doc_list[idx].file_type->comment_close, eol);

	// insert the comment strings
	sci_insert_text(doc_list[idx].sci, line_start, str_begin);
	line_len = sci_get_position_from_line(doc_list[idx].sci, last_line + 2);
	sci_insert_text(doc_list[idx].sci, line_len, str_end);

	g_free(str_begin);
	g_free(str_end);
}


static void real_uncomment_multiline(gint idx)
{
	// find the beginning of the multi line comment
	gint pos, line, len, x;
	gchar *linebuf;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;

	// remove comment open chars
	pos = document_find_text(idx, doc_list[idx].file_type->comment_open, 0, TRUE);
	SSM(doc_list[idx].sci, SCI_DELETEBACK, 0, 0);

	// check whether the line is empty and can be deleted
	line = sci_get_line_from_position(doc_list[idx].sci, pos);
	len = sci_get_line_length(doc_list[idx].sci, line);
	linebuf = g_malloc(len + 1);
	sci_get_line(doc_list[idx].sci, line, linebuf);
	linebuf[len] = '\0';
	x = 0;
	while (linebuf[x] != '\0' && isspace(linebuf[x])) x++;
	if (x == len) SSM(doc_list[idx].sci, SCI_LINEDELETE, 0, 0);
	g_free(linebuf);

	// remove comment close chars
	pos = document_find_text(idx, doc_list[idx].file_type->comment_close, 0, FALSE);
	SSM(doc_list[idx].sci, SCI_DELETEBACK, 0, 0);

	// check whether the line is empty and can be deleted
	line = sci_get_line_from_position(doc_list[idx].sci, pos);
	len = sci_get_line_length(doc_list[idx].sci, line);
	linebuf = g_malloc(len + 1);
	sci_get_line(doc_list[idx].sci, line, linebuf);
	linebuf[len] = '\0';
	x = 0;
	while (linebuf[x] != '\0' && isspace(linebuf[x])) x++;
	if (x == len) SSM(doc_list[idx].sci, SCI_LINEDELETE, 0, 0);
	g_free(linebuf);
}


void sci_cb_do_uncomment(gint idx, gint line)
{
	gint first_line, last_line;
	gint x, i, line_start, line_len;
	gint sel_start, sel_end;
	gsize co_len;
	gchar sel[256], *co, *cc;
	gboolean break_loop = FALSE, single_line = FALSE;
	filetype *ft;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;

	if (line < 0)
	{	// use selection or current line
		sel_start = sci_get_selection_start(doc_list[idx].sci);
		sel_end = sci_get_selection_end(doc_list[idx].sci);

		first_line = sci_get_line_from_position(doc_list[idx].sci, sel_start);
		// Find the last line with chars selected (not EOL char)
		last_line = sci_get_line_from_position(doc_list[idx].sci, sel_end - 1);
		last_line = MAX(first_line, last_line);
	}
	else
	{
		first_line = last_line = line;
		sel_start = sel_end = sci_get_position_from_line(doc_list[idx].sci, line);
	}

	ft = doc_list[idx].file_type;

	// detection of HTML vs PHP code, if non-PHP set filetype to XML
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

	co_len = strlen(co);
	if (co_len == 0) return;

	SSM(doc_list[idx].sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		//geany_debug("line: %d line_start: %d len: %d (%d)", i, line_start, MIN(255, (line_len - 1)), line_len);
		sci_get_text_range(doc_list[idx].sci, line_start, MIN((line_start + 255), (line_start + line_len - 1)), sel);
		sel[MIN(255, (line_len - 1))] = '\0';

		while (isspace(sel[x])) x++;

		// to skip blank lines
		if (x < line_len && sel[x] != '\0')
		{
			// use single line comment
			if (cc == NULL || strlen(cc) == 0)
			{
				guint j;

				single_line = TRUE;

				switch (co_len)
				{
					case 1: if (sel[x] != co[0]) continue; break;
					case 2: if (sel[x] != co[0] || sel[x+1] != co[1]) continue; break;
					case 3: if (sel[x] != co[0] || sel[x+1] != co[1] || sel[x+2] != co[2])
								continue; break;
					default: continue;
				}

				SSM(doc_list[idx].sci, SCI_GOTOPOS, line_start + x + co_len, 0);
				for (j = 0; j < co_len; j++) SSM(doc_list[idx].sci, SCI_DELETEBACK, 0, 0);
			}
			// use multi line comment
			else
			{
				gint style_comment;
				gint lexer = SSM(doc_list[idx].sci, SCI_GETLEXER, 0, 0);

				// process only lines which are already comments
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
				if (sci_get_style_at(doc_list[idx].sci, line_start + x) == style_comment)
				{
					real_uncomment_multiline(idx);
				}

				// break because we are already on the last line
				break_loop = TRUE;
				break;
			}
		}
	}
	SSM(doc_list[idx].sci, SCI_ENDUNDOACTION, 0, 0);

	// restore selection if there is one
	if (sel_start < sel_end)
	{
		if (single_line)
		{
			sci_set_selection_start(doc_list[idx].sci, sel_start - co_len);
			sci_set_selection_end(doc_list[idx].sci, sel_end - ((i - first_line) * co_len));
		}
		else
		{
			gint eol_len = (sci_get_eol_mode(doc_list[idx].sci) == SC_EOL_CRLF) ? 2 : 1;
			sci_set_selection_start(doc_list[idx].sci, sel_start - co_len - eol_len);
			sci_set_selection_end(doc_list[idx].sci, sel_end - co_len - eol_len);
		}
	}
}


void sci_cb_do_comment_toggle(gint idx)
{
	gint first_line, last_line;
	gint x, i, line_start, line_len;
	gint sel_start, sel_end, co_len;
	gint count_commented = 0, count_uncommented = 0;
	gchar sel[256], *co, *cc;
	gboolean break_loop = FALSE, single_line = FALSE;
	gboolean first_line_was_comment = FALSE;
	filetype *ft;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;

	sel_start = sci_get_selection_start(doc_list[idx].sci);
	sel_end = sci_get_selection_end(doc_list[idx].sci);

	ft = doc_list[idx].file_type;

	first_line = sci_get_line_from_position(doc_list[idx].sci,
		sci_get_selection_start(doc_list[idx].sci));
	// Find the last line with chars selected (not EOL char)
	last_line = sci_get_line_from_position(doc_list[idx].sci,
		sci_get_selection_end(doc_list[idx].sci) - 1);
	last_line = MAX(first_line, last_line);

	// detection of HTML vs PHP code, if non-PHP set filetype to XML
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

	co_len = strlen(co);
	if (co_len == 0) return;

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		//geany_debug("line: %d line_start: %d len: %d (%d)", i, line_start, MIN(255, (line_len - 1)), line_len);
		sci_get_text_range(doc_list[idx].sci, line_start, MIN((line_start + 255), (line_start + line_len - 1)), sel);
		sel[MIN(255, (line_len - 1))] = '\0';

		while (isspace(sel[x])) x++;

		// to skip blank lines
		if (x < line_len && sel[x] != '\0')
		{
			// use single line comment
			if (cc == NULL || strlen(cc) == 0)
			{
				gboolean do_continue = FALSE;
				single_line = TRUE;

				switch (co_len)
				{
					case 1:
						if (sel[x] == co[0])
						{
							do_continue = TRUE;
							sci_cb_do_uncomment(idx, i);
							count_uncommented++;
						}
						break;
					case 2:
						if (sel[x] == co[0] && sel[x+1] == co[1])
						{
							do_continue = TRUE;
							sci_cb_do_uncomment(idx, i);
							count_uncommented++;
						}
						break;
					case 3:
						if (sel[x] == co[0] && sel[x+1] == co[1] && sel[x+2] == co[2])
						{
							do_continue = TRUE;
							sci_cb_do_uncomment(idx, i);
							count_uncommented++;
						}
						break;
					default: return;
				}
				if (do_continue && i == first_line) first_line_was_comment = TRUE;
				if (do_continue) continue;

				// we are still here, so the above lines were not already comments, so comment it
				sci_cb_do_comment(idx, i);
				count_commented++;
			}
			// use multi line comment
			else
			{
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
				if (sci_get_style_at(doc_list[idx].sci, line_start + x) == style_comment)
				{
					real_uncomment_multiline(idx);
					count_uncommented++;
				}
				else 
				{
					real_comment_multiline(idx, line_start, last_line);
					count_commented++;
				}
				
				// break because we are already on the last line
				break_loop = TRUE;
				break;
			}
		}
	}

	// restore selection if there is one
	if (sel_start < sel_end)
	{
		if (single_line)
		{
			gint a = (first_line_was_comment) ? - co_len : co_len;
			gint line_start;

			// don't modify sel_start when the selection starts within indentation
			line_start = sci_get_position_from_line(doc_list[idx].sci,
										sci_get_line_from_position(doc_list[idx].sci, sel_start));
			sci_cb_get_indent(doc_list[idx].sci, sel_start, TRUE);
			if ((sel_start - line_start) <= (gint) strlen(indent))
				a = 0;

			sci_set_selection_start(doc_list[idx].sci, sel_start + a);
			sci_set_selection_end(doc_list[idx].sci, sel_end +
								(count_commented * co_len) - (count_uncommented * co_len));
		}
		else
		{
			gint eol_len = (sci_get_eol_mode(doc_list[idx].sci) == SC_EOL_CRLF) ? 2 : 1;
			if (count_uncommented > 0)
			{
				sci_set_selection_start(doc_list[idx].sci, sel_start - co_len - eol_len);
				sci_set_selection_end(doc_list[idx].sci, sel_end - co_len - eol_len);
			}
			else 
			{
				sci_set_selection_start(doc_list[idx].sci, sel_start + co_len + eol_len);
				sci_set_selection_end(doc_list[idx].sci, sel_end + co_len + eol_len);
			}
		}
	}
	else if (count_uncommented > 0)
	{
		gint eol_len = (sci_get_eol_mode(doc_list[idx].sci) == SC_EOL_CRLF) ? 2 : 1;
		sci_set_current_position(doc_list[idx].sci, sel_start - co_len - eol_len);
	}
}


void sci_cb_do_comment(gint idx, gint line)
{
	gint first_line, last_line;
	gint x, i, line_start, line_len;
	gint sel_start, sel_end, co_len;
	gchar sel[256], *co, *cc;
	gboolean break_loop = FALSE, single_line = FALSE;
	filetype *ft;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;

	if (line < 0)
	{	// use selection or current line
		sel_start = sci_get_selection_start(doc_list[idx].sci);
		sel_end = sci_get_selection_end(doc_list[idx].sci);

		first_line = sci_get_line_from_position(doc_list[idx].sci, sel_start);
		// Find the last line with chars selected (not EOL char)
		last_line = sci_get_line_from_position(doc_list[idx].sci, sel_end - 1);
		last_line = MAX(first_line, last_line);
	}
	else
	{
		first_line = last_line = line;
		sel_start = sel_end = sci_get_position_from_line(doc_list[idx].sci, line);
	}

	ft = doc_list[idx].file_type;

	// detection of HTML vs PHP code, if non-PHP set filetype to XML
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

	co_len = strlen(co);
	if (co_len == 0) return;

	SSM(doc_list[idx].sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		//geany_debug("line: %d line_start: %d len: %d (%d)", i, line_start, MIN(256, (line_len - 1)), line_len);
		sci_get_text_range(doc_list[idx].sci, line_start, MIN((line_start + 256), (line_start + line_len - 1)), sel);
		sel[MIN(256, (line_len - 1))] = '\0';

		while (isspace(sel[x])) x++;

		// to skip blank lines
		if (x < line_len && sel[x] != '\0')
		{
			// use single line comment
			if (cc == NULL || strlen(cc) == 0)
			{
				single_line = TRUE;

				if (ft->comment_use_indent)
					sci_insert_text(doc_list[idx].sci, line_start + x, co);
				else
					sci_insert_text(doc_list[idx].sci, line_start, co);
			}
			// use multi line comment
			else
			{
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

				real_comment_multiline(idx, line_start, last_line);

				// break because we are already on the last line
				break_loop = TRUE;
				break;
			}
		}
	}
	SSM(doc_list[idx].sci, SCI_ENDUNDOACTION, 0, 0);

	// restore selection if there is one
	if (sel_start < sel_end)
	{
		if (single_line)
		{
			sci_set_selection_start(doc_list[idx].sci, sel_start + co_len);
			sci_set_selection_end(doc_list[idx].sci, sel_end + ((i - first_line) * co_len));
		}
		else
		{
			gint eol_len = (sci_get_eol_mode(doc_list[idx].sci) == SC_EOL_CRLF) ? 2 : 1;
			sci_set_selection_start(doc_list[idx].sci, sel_start + co_len + eol_len);
			sci_set_selection_end(doc_list[idx].sci, sel_end + co_len + eol_len);
		}
	}
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


