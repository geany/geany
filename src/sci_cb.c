/*
 *      sci_cb.c - this file is part of Geany, a fast and lightweight IDE
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

/*
 * Callbacks for the Scintilla widget (ScintillaObject).
 * Most important is the sci-notify callback, handled in on_editor_notification().
 * This includes auto-indentation, comments, auto-completion, calltips, etc.
 * Also some general Scintilla-related functions.
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
#include "symbols.h"


static gchar current_word[GEANY_MAX_WORD_LENGTH];	// holds word under the mouse or keyboard cursor

EditorInfo editor_info = {current_word, -1};

static struct
{
	gchar *text;
	gboolean set;
} calltip = {NULL, FALSE};

static gchar indent[100];


static void on_new_line_added(ScintillaObject *sci, gint idx);
static gboolean handle_xml(ScintillaObject *sci, gchar ch, gint idx);
static void get_indent(ScintillaObject *sci, gint pos, gboolean use_this_line);
static void auto_multiline(ScintillaObject *sci, gint pos);
static gboolean is_comment(gint lexer, gint style);
static void scroll_to_line(ScintillaObject *sci, gint line, gfloat percent_of_view);
static void auto_close_bracket(ScintillaObject *sci, gint pos, gchar c);


// calls the edit popup menu in the editor
gboolean
on_editor_button_press_event           (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{
	gint idx = GPOINTER_TO_INT(user_data);
	editor_info.click_pos = sci_get_position_from_xy(doc_list[idx].sci, event->x, event->y, FALSE);

	if (event->button == 1)
	{
		if (GDK_BUTTON_PRESS==event->type && app->pref_editor_disable_dnd)
		{
			gint ss = sci_get_selection_start(doc_list[idx].sci);
			sci_set_selection_end(doc_list[idx].sci, ss);
		}
		return utils_check_disk_status(idx, FALSE);
	}

	if (event->button == 3)
	{
		sci_cb_find_current_word(doc_list[idx].sci, editor_info.click_pos,
			current_word, sizeof current_word, NULL);

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
				gboolean set = sci_is_marker_set_at_line(sci, line, 1);

				//sci_marker_delete_all(doc_list[idx].sci, 1);
				sci_set_marker_at_line(sci, line, ! set, 1);	// toggle the marker
			}
			// left click on the folding margin to toggle folding state of current line
			else if (nt->margin == 2 && app->pref_editor_folding)
			{
				gint line = SSM(sci, SCI_LINEFROMPOSITION, nt->position, 0);

				SSM(sci, SCI_TOGGLEFOLD, line, 0);
				if (app->pref_editor_unfold_all_children &&
					SSM(sci, SCI_GETLINEVISIBLE, line + 1, 0))
				{	// unfold all children of the current fold point
					gint last_line = SSM(sci, SCI_GETLASTCHILD, line, -1);
					gint i;

					for (i = line; i < last_line; i++)
					{
						if (! SSM(sci, SCI_GETLINEVISIBLE, i, 0))
						{
							SSM(sci, SCI_TOGGLEFOLD, SSM(sci, SCI_GETFOLDPARENT, i, 0), 0);
						}
					}
				}
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

			/* Visible lines are only laid out accurately once [SCN_UPDATEUI] is sent,
			 * so we need to only call sci_scroll_to_line here, because the document
			 * may have line wrapping and folding enabled.
			 * http://scintilla.sourceforge.net/ScintillaDoc.html#LineWrapping */
			if (doc_list[idx].scroll_percent > 0.0F)
			{
				scroll_to_line(sci, -1, doc_list[idx].scroll_percent);
				doc_list[idx].scroll_percent = -1.0F;	// disable further scrolling
			}
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
 		case SCN_MODIFIED:
		{
			if (nt->modificationType & SC_STARTACTION && ! app->ignore_callback)
			{
				// get notified about undo changes
				document_undo_add(idx, UNDO_SCINTILLA, NULL);
			}
			break;
		}
		case SCN_CHARADDED:
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
					handle_xml(sci, nt->ch, idx);
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
					if (app->pref_editor_auto_complete_constructs)
						sci_cb_auto_forif(idx, pos);
					break;
				}
				case '[':
				case '{':
				{	// Tex auto-closing
					if (sci_get_lexer(sci) == SCLEX_LATEX)
					{
						auto_close_bracket(sci, pos, nt->ch);	// Tex auto-closing
						sci_cb_show_calltip(idx, pos);
					}
					break;
				}
				case '}':
				{	// closing bracket handling
					if (doc_list[idx].use_auto_indention &&
						app->pref_editor_indention_mode == INDENT_ADVANCED)
						sci_cb_close_block(idx, pos - 1);
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
				while (start > 0 && sci_get_char_at(sci, --start) != '&') ;

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
#ifdef GEANY_DEBUG
		case SCN_STYLENEEDED:
		{
			geany_debug("style");
			break;
		}
#endif
		case SCN_URIDROPPED:
		{
			if (nt->text != NULL)
			{
				document_open_file_list(nt->text, -1);
			}
			break;
		}
	}
}


static void on_new_line_added(ScintillaObject *sci, gint idx)
{
	gint pos = sci_get_current_position(sci);

	if (doc_list[idx].file_type == NULL) return;

	// simple indentation
	if (doc_list[idx].use_auto_indention)
	{
		get_indent(sci, pos, FALSE);
		sci_add_text(sci, indent);

		if (app->pref_editor_indention_mode == INDENT_ADVANCED)
		{
			// add extra indentation for Python after colon
			if (doc_list[idx].file_type->id == GEANY_FILETYPES_PYTHON &&
				sci_get_char_at(sci, pos - 2) == ':' &&
				sci_get_style_at(sci, pos - 2) == SCE_P_OPERATOR)
			{
				// creates and inserts one tabulator sign or whitespace of the amount of the tab width
				gchar *text = utils_get_whitespace(app->pref_editor_tab_width);
				sci_add_text(sci, text);
				g_free(text);
			}
		}
	}

	if (app->pref_editor_auto_complete_constructs)
	{
		// " * " auto completion in multiline C/C++/D/Java comments
		auto_multiline(sci, pos);

		sci_cb_auto_latex(idx, pos);
	}
}


static void get_indent(ScintillaObject *sci, gint pos, gboolean use_this_line)
{
	// very simple indentation algorithm
	guint i, len, j = 0;
	gint prev_line;
	gchar *linebuf;

	prev_line = sci_get_line_from_position(sci, pos);

	if (! use_this_line) prev_line--;
	len = sci_get_line_length(sci, prev_line);
	linebuf = sci_get_line(sci, prev_line);

	for (i = 0; i < len && j <= (sizeof(indent) - 1); i++)
	{
		if (linebuf[i] == ' ' || linebuf[i] == '\t')
			indent[j++] = linebuf[i];
		// "&& ! use_this_line" to auto-indent only if it is a real new line
		// and ignore the case of sci_cb_close_block
		else if (linebuf[i] == '{' && ! use_this_line &&
				 app->pref_editor_indention_mode == INDENT_ADVANCED)
		{
			if (app->pref_editor_use_tabs)
			{
				indent[j++] = '\t';
			}
			else
			{	// insert as many spaces as a tabulator would take
				gint k;
				for (k = 0; k < app->pref_editor_tab_width; k++)
					indent[j++] = ' ';
			}

			break;
		}
		else
		{
			gint k = len - 1;

			if (use_this_line)
				break;	// break immediately in the case of sci_cb_close_block

			while (k > 0 && isspace(linebuf[k])) k--;

			// if last non-whitespace character is a { increase indention by a tab
			// e.g. for (...) {
			if (app->pref_editor_indention_mode == INDENT_ADVANCED && linebuf[k] == '{')
			{
				if (app->pref_editor_use_tabs)
				{
					indent[j++] = '\t';
				}
				else
				{	// insert as many spaces as a tabulator would take
					gint n;
					for (n = 0; n < app->pref_editor_tab_width; n++)
						indent[j++] = ' ';
				}
			}
			break;
		}
	}
	indent[j] = '\0';
	g_free(linebuf);
}


static void auto_close_bracket(ScintillaObject *sci, gint pos, gchar c)
{
	if (! app->pref_editor_auto_complete_constructs || SSM(sci, SCI_GETLEXER, 0, 0) != SCLEX_LATEX)
		return;

	if (c == '[')
	{
		sci_add_text(sci, "]");
	}
	else if (c == '{')
	{
		sci_add_text(sci, "}");
	}
	sci_set_current_position(sci, pos, TRUE);
}


/* Finds a corresponding matching brace to the given pos
 * (this is taken from Scintilla Editor.cxx,
 * fit to work with sci_cb_close_block) */
static gint brace_match(ScintillaObject *sci, gint pos)
{
	gchar chBrace = sci_get_char_at(sci, pos);
	gchar chSeek = utils_brace_opposite(chBrace);
	gchar chAtPos;
	gint direction = -1;
	gint styBrace;
	gint depth = 1;
	gint styAtPos;

	styBrace = sci_get_style_at(sci, pos);

	if (utils_is_opening_brace(chBrace))
		direction = 1;

	pos = pos + direction;
	while ((pos >= 0) && (pos < sci_get_length(sci)))
	{
		chAtPos = sci_get_char_at(sci, pos - 1);
		styAtPos = sci_get_style_at(sci, pos);

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
	return -1;
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
	if (lexer != SCLEX_CPP && lexer != SCLEX_HTML && lexer != SCLEX_PASCAL &&
		lexer != SCLEX_D && lexer != SCLEX_BASH)
		return;

	start_brace = brace_match(sci, pos);
	line = sci_get_line_from_position(sci, pos);
	line_start = sci_get_position_from_line(sci, line);
	line_len = sci_get_line_length(sci, line);
	// set eol_char_len to 0 if on last line, because there is no EOL char
	eol_char_len = (line == (SSM(sci, SCI_GETLINECOUNT, 0, 0) - 1)) ? 0 :
								utils_get_eol_char_len(document_find_by_sci(sci));

	// check that the line is empty, to not kill text in the line
	line_buf = sci_get_line(sci, line);
	line_buf[line_len - eol_char_len] = '\0';
	while (x < (line_len - eol_char_len))
	{
		if (isspace(line_buf[x])) cnt++;
		x++;
	}
	g_free(line_buf);

	if ((line_len - eol_char_len - 1) != cnt) return;

	if (start_brace >= 0)
	{
		get_indent(sci, start_brace, TRUE);
		text = g_strconcat(indent, "}", NULL);
		sci_set_anchor(sci, line_start);
		SSM(sci, SCI_REPLACESEL, 0, (sptr_t) text);
		g_free(text);
	}
}


/* Reads the word at given cursor position and writes it into the given buffer. The buffer will be
 * NULL terminated in any case, even when the word is truncated because wordlen is too small.
 * position can be -1, then the current position is used.
 * wc are the wordchars to use, if NULL, GEANY_WORDCHARS will be used */
void sci_cb_find_current_word(ScintillaObject *sci, gint pos, gchar *word, size_t wordlen,
							  const gchar *wc)
{
	gint line, line_start, startword, endword;
	gchar *chunk;

	if (pos == -1)
		pos = sci_get_current_position(sci);

	line = sci_get_line_from_position(sci, pos);
	line_start = sci_get_position_from_line(sci, line);
	startword = pos - line_start;
	endword = pos - line_start;

	word[0] = '\0';
	chunk = sci_get_line(sci, line);

	if (wc == NULL)
		wc = GEANY_WORDCHARS;

	while (startword > 0 && strchr(wc, chunk[startword - 1]))
		startword--;
	while (chunk[endword] && strchr(wc, chunk[endword]))
		endword++;
	if(startword == endword)
		return;

	chunk[endword] = '\0';

	g_strlcpy(word, chunk + startword, wordlen); //ensure null terminated
	g_free(chunk);
}


static gint find_previous_brace(ScintillaObject *sci, gint pos)
{
	gchar c;
	gint orig_pos = pos;

	c = SSM(sci, SCI_GETCHARAT, pos, 0);
	while (pos >= 0 && pos > orig_pos - 300)
	{
		c = SSM(sci, SCI_GETCHARAT, pos, 0);
		pos--;
		if (utils_is_opening_brace(c)) return pos;
	}
	return -1;
}


static gint find_start_bracket(ScintillaObject *sci, gint pos)
{
	gchar c;
	gint brackets = 0;
	gint orig_pos = pos;

	c = SSM(sci, SCI_GETCHARAT, pos, 0);
	while (pos > 0 && pos > orig_pos - 300)
	{
		c = SSM(sci, SCI_GETCHARAT, pos, 0);
		if (c == ')') brackets++;
		else if (c == '(') brackets--;
		pos--;
		if (brackets < 0) return pos;	// found start bracket
	}
	return -1;
}


static gchar *tag_to_calltip(const TMTag *tag, filetype_id ft_id)
{
	GString *str;
	gchar *result;

	if (! tag->atts.entry.arglist) return NULL;

	str = g_string_new(NULL);

	if (tag->atts.entry.var_type)
	{
		guint i;

		g_string_append(str, tag->atts.entry.var_type);
		for (i = 0; i < tag->atts.entry.pointerOrder; i++)
		{
			g_string_append_c(str, '*');
		}
		g_string_append_c(str, ' ');
	}
	if (tag->atts.entry.scope)
	{
		const gchar *cosep = symbols_get_context_separator(ft_id);

		g_string_append(str, tag->atts.entry.scope);
		g_string_append(str, cosep);
	}
	g_string_append(str, tag->name);
	g_string_append_c(str, ' ');
	g_string_append(str, tag->atts.entry.arglist);

	result = str->str;
	g_string_free(str, FALSE);
	return result;
}


static gchar *find_calltip(const gchar *word, filetype *ft)
{
	TMTag *tag;
	const GPtrArray *tags;

	g_return_val_if_fail(ft && word && *word, NULL);

	tags = tm_workspace_find(word, tm_tag_max_t, NULL, FALSE, ft->lang);

	if (tags->len == 0)
		return NULL;

	tag = TM_TAG(tags->pdata[0]);
	if (tag->atts.entry.arglist)
	{
		return tag_to_calltip(tag, FILETYPE_ID(ft));
	}
	else if (tag->type == tm_tag_class_t && FILETYPE_ID(ft) == GEANY_FILETYPES_D)
	{
		TMTagAttrType attrs[] = { tm_tag_attr_name_t, 0 };

		// user typed e.g. 'new Classname(' so lookup D constructor Classname::this()
		tags = tm_workspace_find_scoped("this", tag->name, tm_tag_function_t | tm_tag_prototype_t,
			attrs, FALSE, ft->lang, TRUE);
		if (tags->len != 0)
		{
			tag = TM_TAG(tags->pdata[0]);
			if (tag->atts.entry.arglist)
				return tag_to_calltip(tag, FILETYPE_ID(ft));
		}
	}
	return NULL;
}


gboolean sci_cb_show_calltip(gint idx, gint pos)
{
	gint orig_pos = pos; // the position for the calltip
	gint lexer;
	gint style;
	gchar word[GEANY_MAX_WORD_LENGTH];
	gchar *str;
	ScintillaObject *sci;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return FALSE;
	sci = doc_list[idx].sci;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);

	if (pos == -1)
	{
		// position of '(' is unknown, so go backwards from current position to find it
		pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
		pos--;
		orig_pos = pos;
		pos = (lexer == SCLEX_LATEX) ? find_previous_brace(sci, pos) :
			find_start_bracket(sci, pos);
		if (pos == -1) return FALSE;
	}

	style = SSM(sci, SCI_GETSTYLEAT, pos, 0);
	if (is_comment(lexer, style))
		return FALSE;
	// never show a calltip in a PHP file outside of the <? ?> tags
	if (lexer == SCLEX_HTML && ! (style >= SCE_HPHP_DEFAULT && style <= SCE_HPHP_OPERATOR))
		return FALSE;

	word[0] = '\0';
	sci_cb_find_current_word(sci, pos - 1, word, sizeof word, NULL);
	if (word[0] == '\0') return FALSE;

	str = find_calltip(word, doc_list[idx].file_type);
	if (str)
	{
		g_free(calltip.text);	// free the old calltip
		calltip.text = str;
		calltip.set = TRUE;
		utils_wrap_string(calltip.text, -1);
		SSM(sci, SCI_CALLTIPSHOW, orig_pos, (sptr_t) calltip.text);
		return TRUE;
	}
	return FALSE;
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
	gchar **entities = symbols_get_html_entities();

	if (*root != '&' || entities == NULL) return FALSE;

	words = g_string_sized_new(500);
	for (i = 0; ; i++)
	{
		if (entities[i] == NULL) break;
		else if (entities[i][0] == '#') continue;

		if (! strncmp(entities[i], root, rootlen))
		{
			if (j++ > 0) g_string_append_c(words, ' ');
			g_string_append(words, entities[i]);
		}
	}
	if (words->len > 0) show_autocomplete(sci, rootlen, words->str);
	g_string_free(words, TRUE);
	return TRUE;
}


static gboolean
autocomplete_tags(gint idx, gchar *root, gsize rootlen)
{	// PHP, LaTeX, C, C++, D and Java tag autocompletion
	TMTagAttrType attrs[] = { tm_tag_attr_name_t, 0 };
	const GPtrArray *tags;
	ScintillaObject *sci;

	if (! DOC_IDX_VALID(idx) || doc_list[idx].file_type == NULL)
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
	gchar *wordchars;
	filetype *ft;

	if ((! app->pref_editor_auto_complete_symbols && ! force) ||
		! DOC_IDX_VALID(idx) || doc_list[idx].file_type == NULL)
		return FALSE;

	sci = doc_list[idx].sci;
	ft = doc_list[idx].file_type;

	line = sci_get_line_from_position(sci, pos);
	line_start = sci_get_position_from_line(sci, line);
	line_len = sci_get_line_length(sci, line);
	line_pos = pos - line_start - 1;
	current = pos - line_start;
	startword = current;
	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	style = SSM(sci, SCI_GETSTYLEAT, pos, 0);

	 // don't autocomplete in comments and strings
	 if (is_comment(lexer, style))
		return FALSE;

	linebuf = sci_get_line(sci, line);

	if (ft->id == GEANY_FILETYPES_LATEX)
		wordchars = GEANY_WORDCHARS"\\"; // add \ to word chars if we are in a LaTeX file
	else if (ft->id == GEANY_FILETYPES_HTML || ft->id == GEANY_FILETYPES_PHP)
		wordchars = GEANY_WORDCHARS"&"; // add & to word chars if we are in a PHP or HTML file
	else
		wordchars = GEANY_WORDCHARS;

	// find the start of the current word
	while ((startword > 0) && (strchr(wordchars, linebuf[startword - 1])))
		startword--;
	linebuf[current] = '\0';
	root = linebuf + startword;
	rootlen = current - startword;

	// entity autocompletion always in a HTML file, in a PHP file only when we are outside of <? ?>
	if (ft->id == GEANY_FILETYPES_HTML ||
		(ft->id == GEANY_FILETYPES_PHP && (style < SCE_HPHP_DEFAULT || style > SCE_HPHP_OPERATOR)))
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
		buf = sci_get_line(sci, line);

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
			while (i < line_len && buf[i] != '{' && j < (sizeof(full_cmd) - 1))
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
			if (doc_list[idx].use_auto_indention) get_indent(sci, pos, TRUE);
			eol = g_strconcat(utils_get_eol_char(idx), indent, NULL);

			construct = g_strdup_printf("%s\\end%s{%s}", eol, full_cmd, env);

			SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
			sci_goto_pos(sci, pos + 1, TRUE);
			g_free(construct);
			g_free(eol);
		}
		// later there could be some else ifs for other keywords

		g_free(buf);
	}
}


static gboolean at_eol(ScintillaObject *sci, gint pos)
{
	gint line = sci_get_line_from_position(sci, pos);

	return (pos == sci_get_line_end_position(sci, line));
}


void sci_cb_auto_forif(gint idx, gint pos)
{
	static gchar buf[16];
	gchar *eol;
	gchar *space;
	gchar *construct;
	gint lexer, style;
	gint i;
	gint space_len;
	ScintillaObject *sci;

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_type == NULL) return;

	// only for C, C++, D, Ferite, Java, JavaScript, Perl and PHP
	if (doc_list[idx].file_type->id != GEANY_FILETYPES_PHP &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_C &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_D &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_CPP &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_PERL &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_JAVA &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_JS &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_FERITE)
		return;

	sci = doc_list[idx].sci;
	// return if we are editing an existing line (chars on right of cursor)
	if (! at_eol(sci, pos))
		return;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	style = SSM(sci, SCI_GETSTYLEAT, pos - 2, 0);
	// return, if we are in a comment
	if (is_comment(lexer, style))
		return;
	// never auto complete in a PHP file outside of the <? ?> tags
	if (lexer == SCLEX_HTML && ! (style >= SCE_HPHP_DEFAULT && style <= SCE_HPHP_OPERATOR))
		return;

	// get the indentation
	if (doc_list[idx].use_auto_indention) get_indent(sci, pos, TRUE);
	eol = g_strconcat(utils_get_eol_char(idx), indent, NULL);
	sci_get_text_range(sci, pos - 16, pos - 1, buf);
	// check the first 8 characters of buf for whitespace, but only in this line
	i = 14;
	while (i >= 0 && isalpha(buf[i])) i--;	// find pos before keyword
	while (i >= 0 && buf[i] != '\n' && buf[i] != '\r') // we want to stay in this line('\n' check)
	{
		if (! isspace(buf[i]))
		{
			g_free(eol);
			return;
		}
		i--;
	}

	// get the whitespace for additional indentation
	space = utils_get_whitespace(app->pref_editor_tab_width);
	space_len = strlen(space);

	// "pattern", buf + x, y -> x + y = 15, because buf is (pos - 16)...(pos - 1) = 15
	if (! strncmp("if", buf + 13, 2))
	{
		if (! isspace(*(buf + 12)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("()%s{%s%s%s}%s", eol, eol, space, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}
	else if (! strncmp("else", buf + 11, 4))
	{
		if (! isspace(*(buf + 10)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("%s{%s%s%s}%s", eol, eol, space, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 3 + space_len + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("for", buf + 12, 3))
	{
		gchar *var;
		gint contruct_len;

		if (! isspace(*(buf + 11)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

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
		construct = g_strdup_printf("(%s%s = 0; %s < ; %s++)%s{%s%s%s}%s",
						(doc_list[idx].file_type->id == GEANY_FILETYPES_CPP) ? "int " : "",
						var, var, var, eol, eol, space, eol, eol);

		// add 4 characters because of "int " in C++ mode
		contruct_len += (doc_list[idx].file_type->id == GEANY_FILETYPES_CPP) ? 4 : 0;

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + contruct_len, TRUE);
		g_free(var);
		g_free(construct);
	}
	else if (! strncmp("while", buf + 10, 5))
	{
		if (! isspace(*(buf + 9)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("()%s{%s%s%s}%s", eol, eol, space, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}
	else if (! strncmp("do", buf + 13, 2))
	{
		if (! isspace(*(buf + 12)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("%s{%s%s%s}%swhile ();%s", eol, eol, space, eol, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 3 + space_len + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("try", buf + 12, 3))
	{
		if (! isspace(*(buf + 11)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("%s{%s%s%s}%scatch ()%s{%s%s%s}%s",
							eol, eol, space, eol, eol, eol, eol, space, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 3 + space_len + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (! strncmp("switch", buf + 9, 6))
	{
		if (! isspace(*(buf + 8)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("()%s{%s%scase : break;%s%sdefault: %s}%s",
										eol, eol, space, eol, space, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 1, TRUE);
		g_free(construct);
	}
	else if (doc_list[idx].file_type->id == GEANY_FILETYPES_FERITE && ! strncmp("iferr", buf + 10, 5))
	{
		if (doc_list[idx].file_type->id != GEANY_FILETYPES_FERITE ||
			! isspace(*(buf + 9)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("%s{%s%s%s}%sfix%s{%s%s%s}%s",
										eol, eol, space, eol, eol, eol, eol, space, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 3 + space_len + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}
	else if (doc_list[idx].file_type->id == GEANY_FILETYPES_FERITE && ! strncmp("monitor", buf + 8, 7))
	{
		if (! isspace(*(buf + 7)))
		{
			utils_free_pointers(eol, space, NULL);
			return;
		}

		construct = g_strdup_printf("%s{%s%s%s}%shandle%s{%s%s%s}%s",
										eol, eol, space, eol, eol, eol, eol, space, eol, eol);

		SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
		sci_goto_pos(sci, pos + 3 + space_len + (2 * strlen(indent)), TRUE);
		g_free(construct);
	}

	utils_free_pointers(eol, space, NULL);
}


void sci_cb_show_macro_list(ScintillaObject *sci)
{
	GString *words;

	if (sci == NULL) return;

	words = symbols_get_macro_list();
	SSM(sci, SCI_USERLISTSHOW, 1, (sptr_t) words->str);
	g_string_free(words, TRUE);
}


/**
 * (stolen from anjuta and heavily modified)
 * This routine will auto complete XML or HTML tags that are still open by closing them
 * @param ch The character we are dealing with, currently only works with the '>' character
 * @return True if handled, false otherwise
 */
static gboolean handle_xml(ScintillaObject *sci, gchar ch, gint idx)
{
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint pos, min;
	gchar *str_found, sel[512];

	// If the user has turned us off, quit now.
	// This may make sense only in certain languages
	if (! app->pref_editor_auto_close_xml_tags || (lexer != SCLEX_HTML && lexer != SCLEX_XML))
		return FALSE;

	pos = sci_get_current_position(sci);

	// return if we are in PHP but not in a string or outside of <? ?> tags
	if (doc_list[idx].file_type->id == GEANY_FILETYPES_PHP)
	{
		gint style = sci_get_style_at(sci, pos);
		if (style != SCE_HPHP_SIMPLESTRING && style != SCE_HPHP_HSTRING &&
			style <= SCE_HPHP_OPERATOR && style >= SCE_HPHP_DEFAULT)
			return FALSE;
	}

	// if ch is /, check for </, else quit
	if (ch == '/' && sci_get_char_at(sci, pos - 2) != '<')
		return FALSE;

	// Grab the last 512 characters or so
	min = pos - (sizeof(sel) - 1);
	if (min < 0) min = 0;

	if (pos - min < 3)
		return FALSE; // Smallest tag is 3 characters e.g. <p>

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
	if (utils_str_equal(str_found, "br")
	 || utils_str_equal(str_found, "img")
	 || utils_str_equal(str_found, "base")
	 || utils_str_equal(str_found, "basefont")	// < or not <
	 || utils_str_equal(str_found, "frame")
	 || utils_str_equal(str_found, "input")
	 || utils_str_equal(str_found, "link")
	 || utils_str_equal(str_found, "area")
	 || utils_str_equal(str_found, "meta"))
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
			if (utils_str_equal(str_found, "table")) sci_cb_auto_table(sci, pos);
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

	get_indent(sci, pos, TRUE);
	indent_pos = sci_get_line_indent_position(sci, sci_get_line_from_position(sci, pos));
	if ((pos - 7) != indent_pos) // 7 == strlen("<table>")
	{
		gint i, x;
		x = strlen(indent);
		// find the start of the <table tag
		i = 1;
		while (i <= pos && sci_get_char_at(sci, pos - i) != '<') i++;
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
	pos = document_find_text(idx, doc_list[idx].file_type->comment_open, 0, TRUE, FALSE);
	SSM(doc_list[idx].sci, SCI_DELETEBACK, 0, 0);

	// check whether the line is empty and can be deleted
	line = sci_get_line_from_position(doc_list[idx].sci, pos);
	len = sci_get_line_length(doc_list[idx].sci, line);
	linebuf = sci_get_line(doc_list[idx].sci, line);
	x = 0;
	while (linebuf[x] != '\0' && isspace(linebuf[x])) x++;
	if (x == len) SSM(doc_list[idx].sci, SCI_LINEDELETE, 0, 0);
	g_free(linebuf);

	// remove comment close chars
	pos = document_find_text(idx, doc_list[idx].file_type->comment_close, 0, FALSE, FALSE);
	SSM(doc_list[idx].sci, SCI_DELETEBACK, 0, 0);

	// check whether the line is empty and can be deleted
	line = sci_get_line_from_position(doc_list[idx].sci, pos);
	len = sci_get_line_length(doc_list[idx].sci, line);
	linebuf = sci_get_line(doc_list[idx].sci, line);
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
		gint buf_len;

		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		buf_len = MIN((gint)sizeof(sel) - 1, line_len - 1);
		if (buf_len <= 0)
			continue;
		sci_get_text_range(doc_list[idx].sci, line_start, line_start + buf_len, sel);
		sel[buf_len] = '\0';

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
					case SCLEX_D: style_comment = SCE_D_COMMENT; break;
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

	SSM(doc_list[idx].sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		gint buf_len;

		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		buf_len = MIN((gint)sizeof(sel) - 1, line_len - 1);
		if (buf_len <= 0)
			continue;
		sci_get_text_range(doc_list[idx].sci, line_start, line_start + buf_len, sel);
		sel[buf_len] = '\0';

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
				sci_cb_do_comment(idx, i, FALSE);
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
					case SCLEX_D: style_comment = SCE_D_COMMENT; break;
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

	SSM(doc_list[idx].sci, SCI_ENDUNDOACTION, 0, 0);

	// restore selection if there is one
	if (sel_start < sel_end)
	{
		if (single_line)
		{
			gint a = (first_line_was_comment) ? - co_len : co_len;

			// don't modify sel_start when the selection starts within indentation
			get_indent(doc_list[idx].sci, sel_start, TRUE);
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
		sci_set_current_position(doc_list[idx].sci, sel_start - co_len - eol_len, TRUE);
	}
}


void sci_cb_do_comment(gint idx, gint line, gboolean allow_empty_lines)
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
		gint buf_len;

		line_start = sci_get_position_from_line(doc_list[idx].sci, i);
		line_len = sci_get_line_length(doc_list[idx].sci, i);
		x = 0;

		buf_len = MIN((gint)sizeof(sel) - 1, line_len - 1);
		if (buf_len <= 0)
			continue;
		sci_get_text_range(doc_list[idx].sci, line_start, line_start + buf_len, sel);
		sel[buf_len] = '\0';

		while (isspace(sel[x])) x++;

		// to skip blank lines
		if (allow_empty_lines || (x < line_len && sel[x] != '\0'))
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
					case SCLEX_D: style_comment = SCE_D_COMMENT; break;
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
	gint brace_pos = cur_pos - 1;
	gint end_pos;

	if (! utils_isbrace(sci_get_char_at(sci, brace_pos)))
	{
		brace_pos++;
		if (! utils_isbrace(sci_get_char_at(sci, brace_pos)))
		{
			SSM(sci, SCI_BRACEBADLIGHT, -1, 0);
			return;
		}
	}
	end_pos = SSM(sci, SCI_BRACEMATCH, brace_pos, 0);

	if (end_pos >= 0)
		SSM(sci, SCI_BRACEHIGHLIGHT, brace_pos, end_pos);
	else
		SSM(sci, SCI_BRACEBADLIGHT, brace_pos, 0);
}


static gboolean is_doc_comment_char(gchar c, gint lexer)
{
	if (c == '*' && (lexer = SCLEX_HTML || lexer == SCLEX_CPP))
		return TRUE;
	else if ((c == '*' || c == '+') && lexer == SCLEX_D)
		return TRUE;
	else
		return FALSE;
}


static void auto_multiline(ScintillaObject *sci, gint pos)
{
	gint style = SSM(sci, SCI_GETSTYLEAT, pos - 2, 0);
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint i;

	if ((lexer == SCLEX_CPP && (style == SCE_C_COMMENT || style == SCE_C_COMMENTDOC)) ||
		(lexer == SCLEX_HTML && style == SCE_HPHP_COMMENT) ||
		(lexer == SCLEX_D && (style == SCE_D_COMMENT ||
							  style == SCE_D_COMMENTDOC ||
							  style == SCE_D_COMMENTNESTED)))
	{
		gchar *previous_line = sci_get_line(sci, sci_get_line_from_position(sci, pos - 2));
		gchar *continuation = "*"; // the type of comment, '*' (C/C++/Java), '+' and the others (D)
		gchar *whitespace = ""; // to hold whitespace if needed
		gchar *result;
		gint len = strlen(previous_line);

		// find and stop at end of multi line comment
		i = len - 1;
		while (i >= 0 && isspace(previous_line[i])) i--;
		if (i >= 1 && is_doc_comment_char(previous_line[i - 1], lexer) && previous_line[i] == '/')
		{
			gint cur_line = sci_get_current_line(sci, -1);
			gint indent_pos = sci_get_line_indent_position(sci, cur_line);
			gint indent_len = sci_get_col_from_position(sci, indent_pos);

			/* if there is one too many spaces, delete the last space,
			 * to return to the indent used before the multiline comment was started. */
			if (indent_len % app->pref_editor_tab_width == 1)
				SSM(sci, SCI_DELETEBACKNOTLINE, 0, 0);	// remove whitespace indent
			g_free(previous_line);
			return;
		}
		// check whether we are on the second line of multi line comment
		i = 0;
		while (i < len && isspace(previous_line[i])) i++; // get to start of the line

		if (i + 1 < len &&
			previous_line[i] == '/' && is_doc_comment_char(previous_line[i + 1], lexer))
		{ // we are on the second line of a multi line comment, so we have to insert white space
			whitespace = " ";
		}

		if (style == SCE_D_COMMENTNESTED) continuation = "+"; // for nested comments in D

		result = g_strconcat(whitespace, continuation, " ", NULL);
		sci_add_text(sci, result);
		g_free(result);

		g_free(previous_line);
	}
}


/* Checks whether the given style is a comment or string for the given lexer.
 * It doesn't handle LEX_HTML, this should be done by the caller.
 * Returns true if the style is a comment, FALSE otherwise.
 */
static gboolean is_comment(gint lexer, gint style)
{
	gboolean result = FALSE;

	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_PASCAL:
		{
			if (style == SCE_C_COMMENT ||
				style == SCE_C_COMMENTLINE ||
				style == SCE_C_COMMENTDOC ||
				style == SCE_C_COMMENTLINEDOC ||
				style == SCE_C_CHARACTER ||
				style == SCE_C_PREPROCESSOR ||
				style == SCE_C_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_D:
		{
			if (style == SCE_D_COMMENT ||
				style == SCE_D_COMMENTLINE ||
				style == SCE_D_COMMENTDOC ||
				style == SCE_D_COMMENTLINEDOC ||
				style == SCE_D_COMMENTNESTED ||
				style == SCE_D_CHARACTER ||
				style == SCE_D_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_PYTHON:
		{
			if (style == SCE_P_COMMENTLINE ||
				style == SCE_P_COMMENTBLOCK ||
				style == SCE_P_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_F77:
		{
			if (style == SCE_F_COMMENT ||
				style == SCE_F_STRING1 ||
				style == SCE_F_STRING2)
				result = TRUE;
			break;
		}
		case SCLEX_PERL:
		{
			if (style == SCE_PL_COMMENTLINE ||
				style == SCE_PL_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_PROPERTIES:
		{
			if (style == SCE_PROPS_COMMENT)
				result = TRUE;
			break;
		}
		case SCLEX_LATEX:
		{
			if (style == SCE_L_COMMENT)
				result = TRUE;
			break;
		}
		case SCLEX_MAKEFILE:
		{
			if (style == SCE_MAKE_COMMENT)
				result = TRUE;
			break;
		}
		case SCLEX_RUBY:
		{
			if (style == SCE_RB_COMMENTLINE ||
				style == SCE_RB_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_BASH:
		{
			if (style == SCE_SH_COMMENTLINE ||
				style == SCE_SH_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_SQL:
		{
			if (style == SCE_SQL_COMMENT ||
				style == SCE_SQL_COMMENTLINE ||
				style == SCE_SQL_COMMENTDOC ||
				style == SCE_SQL_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_TCL:
		{
			if (style == SCE_TCL_COMMENT ||
				style == SCE_TCL_COMMENTLINE ||
				style == SCE_TCL_IN_QUOTE)
				result = TRUE;
			break;
		}
		case SCLEX_LUA:
		{
			if (style == SCE_LUA_COMMENT ||
				style == SCE_LUA_COMMENTLINE ||
				style == SCE_LUA_COMMENTDOC ||
				style == SCE_LUA_LITERALSTRING ||
				style == SCE_LUA_CHARACTER ||
				style == SCE_LUA_STRING)
				result = TRUE;
			break;
		}
		case SCLEX_HTML:
		{
			if (style == SCE_HPHP_SIMPLESTRING ||
				style == SCE_HPHP_HSTRING ||
				style == SCE_HPHP_COMMENTLINE ||
				style == SCE_HPHP_COMMENT ||
				style == SCE_H_DOUBLESTRING ||
				style == SCE_H_SINGLESTRING ||
				style == SCE_H_CDATA ||
				style == SCE_H_COMMENT ||
				style == SCE_H_SGML_DOUBLESTRING ||
				style == SCE_H_SGML_SIMPLESTRING ||
				style == SCE_H_SGML_COMMENT)
				result = TRUE;
			break;
		}
	}

	return result;
}


#if 0
gboolean sci_cb_lexer_is_c_like(gint lexer)
{
	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_D:
		return TRUE;

		default:
		return FALSE;
	}
}
#endif


// Returns: -1 if lexer doesn't support type keywords
gint sci_cb_lexer_get_type_keyword_idx(gint lexer)
{
	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_D:
		return 3;

		default:
		return -1;
	}
}


// inserts a three-line comment at one line above current cursor position
void sci_cb_insert_multiline_comment(gint idx)
{
	gchar *text;
	gint text_len;
	gint line;
	gint pos;
	gboolean have_multiline_comment = FALSE;

	if (doc_list[idx].file_type->comment_close != NULL &&
		strlen(doc_list[idx].file_type->comment_close) > 0)
		have_multiline_comment = TRUE;

	// insert three lines one line above of the current position
	line = sci_get_line_from_position(doc_list[idx].sci, editor_info.click_pos);
	pos = sci_get_position_from_line(doc_list[idx].sci, line);

	// use the indentation on the current line but only when comment indention is used
	// and we don't have multi line comment characters
	if (doc_list[idx].use_auto_indention && ! have_multiline_comment &&
		doc_list[idx].file_type->comment_use_indent)
	{
		get_indent(doc_list[idx].sci, editor_info.click_pos, TRUE);
		text = g_strdup_printf("%s\n%s\n%s\n", indent, indent, indent);
		text_len = strlen(text);
	}
	else
	{
		text = g_strdup("\n\n\n");
		text_len = 3;
	}
	sci_insert_text(doc_list[idx].sci, pos, text);
	g_free(text);


	// select the inserted lines for commenting
	sci_set_selection_start(doc_list[idx].sci, pos);
	sci_set_selection_end(doc_list[idx].sci, pos + text_len);

	sci_cb_do_comment(idx, -1, TRUE);

	// set the current position to the start of the first inserted line
	pos += strlen(doc_list[idx].file_type->comment_open);

	// on multi line comment jump to the next line, otherwise add the length of added indentation
	if (have_multiline_comment)
		pos += 1;
	else
		pos += strlen(indent);

	sci_set_current_position(doc_list[idx].sci, pos, TRUE);
	// reset the selection
	sci_set_anchor(doc_list[idx].sci, pos);
}


/* Scroll the view to make line appear at percent_of_view.
 * line can be -1 to use the current position. */
static void scroll_to_line(ScintillaObject *sci, gint line, gfloat percent_of_view)
{
	gint vis1, los, delta;
	GtkWidget *wid = GTK_WIDGET(sci);

	if (! wid->window || ! gdk_window_is_viewable(wid->window))
		return;	// prevent gdk_window_scroll warning

	if (line == -1)
		line = sci_get_current_line(sci, -1);

	// sci 'visible line' != doc line number because of folding and line wrapping
	/* calling SCI_VISIBLEFROMDOCLINE for line is more accurate than calling
	 * SCI_DOCLINEFROMVISIBLE for vis1. */
	line = SSM(sci, SCI_VISIBLEFROMDOCLINE, line, 0);
	vis1 = SSM(sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	los = SSM(sci, SCI_LINESONSCREEN, 0, 0);
	delta = (line - vis1) - los * percent_of_view;
	sci_scroll_lines(sci, delta);
	//sci_scroll_caret(sci); // ensure visible (maybe not needed now)
}


void sci_cb_select_word(ScintillaObject *sci)
{
	gint pos;
	gint start;
	gint end;

	g_return_if_fail(sci != NULL);

	pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
	start = SSM(sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	end = SSM(sci, SCI_WORDENDPOSITION, pos, TRUE);

	if (start == end) // caret in whitespaces sequence
	{
		// look forward but reverse the selection direction,
		// so the caret end up stay as near as the original position.
		end = SSM(sci, SCI_WORDENDPOSITION, pos, FALSE);
		start = SSM(sci, SCI_WORDENDPOSITION, end, TRUE);
		if (start == end)
			return;
	}

	SSM(sci, SCI_SETSEL, start, end);
}
