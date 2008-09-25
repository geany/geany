/*
 *      editor.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/**
 * @file editor.h
 * Editor-related functions for @ref GeanyEditor.
 * Geany uses the Scintilla editing widget, and this file is mostly built around
 * Scintilla's functionality.
 * @see sciwrappers.h.
 */
/* Callbacks for the Scintilla widget (ScintillaObject).
 * Most important is the sci-notify callback, handled in on_editor_notification().
 * This includes auto-indentation, comments, auto-completion, calltips, etc.
 * Also some general Scintilla-related functions.
 */


#include <ctype.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "SciLexer.h"
#include "geany.h"

#include "support.h"
#include "editor.h"
#include "document.h"
#include "documentprivate.h"
#include "filetypes.h"
#include "sciwrappers.h"
#include "ui_utils.h"
#include "utils.h"
#include "dialogs.h"
#include "symbols.h"
#include "callbacks.h"
#include "geanyobject.h"
#include "templates.h"
#include "keybindings.h"


/* holds word under the mouse or keyboard cursor */
static gchar current_word[GEANY_MAX_WORD_LENGTH];

/* Initialised in keyfile.c. */
GeanyEditorPrefs editor_prefs;

EditorInfo editor_info = {current_word, -1};

static struct
{
	gchar *text;
	gboolean set;
	gchar *last_word;
	guint tag_index;
	gint pos;
	ScintillaObject *sci;
} calltip = {NULL, FALSE, NULL, 0, 0, NULL};

static gchar indent[100];


static void on_new_line_added(GeanyEditor *editor);
static gboolean handle_xml(GeanyEditor *editor, gchar ch);
static void insert_indent_after_line(GeanyEditor *editor, gint line);
static void auto_multiline(GeanyEditor *editor, gint pos);
static gboolean is_code_style(gint lexer, gint style);
static void auto_close_bracket(ScintillaObject *sci, gint pos, gchar c);
static void auto_table(GeanyEditor *editor, gint pos);
static void close_block(GeanyEditor *editor, gint pos);


void editor_snippets_free()
{
	g_hash_table_destroy(editor_prefs.snippets);
}


void editor_snippets_init(void)
{
	gsize i, j, len = 0, len_keys = 0;
	gchar *sysconfigfile, *userconfigfile;
	gchar **groups_user, **groups_sys;
	gchar **keys_user, **keys_sys;
	gchar *value;
	GKeyFile *sysconfig = g_key_file_new();
	GKeyFile *userconfig = g_key_file_new();
	GHashTable *tmp;

	sysconfigfile = g_strconcat(app->datadir, G_DIR_SEPARATOR_S, "snippets.conf", NULL);
	userconfigfile = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "snippets.conf", NULL);

	/* check for old autocomplete.conf files (backwards compatibility) */
	if (! g_file_test(userconfigfile, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
		setptr(userconfigfile,
			g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "autocomplete.conf", NULL));

	/* load the actual config files */
	g_key_file_load_from_file(sysconfig, sysconfigfile, G_KEY_FILE_NONE, NULL);
	g_key_file_load_from_file(userconfig, userconfigfile, G_KEY_FILE_NONE, NULL);

	/* keys are strings, values are GHashTables, so use g_free and g_hash_table_destroy */
	editor_prefs.snippets =
		g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_hash_table_destroy);

	/* first read all globally defined auto completions */
	groups_sys = g_key_file_get_groups(sysconfig, &len);
	for (i = 0; i < len; i++)
	{
		keys_sys = g_key_file_get_keys(sysconfig, groups_sys[i], &len_keys, NULL);
		/* create new hash table for the read section (=> filetype) */
		tmp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		g_hash_table_insert(editor_prefs.snippets, g_strdup(groups_sys[i]), tmp);

		for (j = 0; j < len_keys; j++)
		{
			g_hash_table_insert(tmp, g_strdup(keys_sys[j]),
						utils_get_setting_string(sysconfig, groups_sys[i], keys_sys[j], ""));
		}
		g_strfreev(keys_sys);
	}

	/* now read defined completions in user's configuration directory and add / replace them */
	groups_user = g_key_file_get_groups(userconfig, &len);
	for (i = 0; i < len; i++)
	{
		keys_user = g_key_file_get_keys(userconfig, groups_user[i], &len_keys, NULL);

		tmp = g_hash_table_lookup(editor_prefs.snippets, groups_user[i]);
		if (tmp == NULL)
		{	/* new key found, create hash table */
			tmp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
			g_hash_table_insert(editor_prefs.snippets, g_strdup(groups_user[i]), tmp);
		}
		for (j = 0; j < len_keys; j++)
		{
			value = g_hash_table_lookup(tmp, keys_user[j]);
			if (value == NULL)
			{	/* value = NULL means the key doesn't yet exist, so insert */
				g_hash_table_insert(tmp, g_strdup(keys_user[j]),
						utils_get_setting_string(userconfig, groups_user[i], keys_user[j], ""));
			}
			else
			{	/* old key and value will be freed by destroy function (g_free) */
				g_hash_table_replace(tmp, g_strdup(keys_user[j]),
						utils_get_setting_string(userconfig, groups_user[i], keys_user[j], ""));
			}
		}
		g_strfreev(keys_user);
	}

	g_free(sysconfigfile);
	g_free(userconfigfile);
	g_strfreev(groups_sys);
	g_strfreev(groups_user);
	g_key_file_free(sysconfig);
	g_key_file_free(userconfig);
}


static gboolean
on_editor_button_press_event           (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{
	GeanyDocument *doc = user_data;
	ScintillaObject *sci = doc->editor->sci;

	editor_info.click_pos = sci_get_position_from_xy(doc->editor->sci, (gint)event->x, (gint)event->y, FALSE);
	if (event->button == 1)
	{
		if (event->type == GDK_BUTTON_PRESS && editor_prefs.disable_dnd)
		{
			gint ss = sci_get_selection_start(doc->editor->sci);
			sci_set_selection_end(doc->editor->sci, ss);
		}
		if (event->type == GDK_BUTTON_PRESS && event->state == GDK_CONTROL_MASK)
		{
			sci_set_current_position(sci, editor_info.click_pos, FALSE);
			keybindings_send_command(GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TAGDEFINITION);
			return TRUE;
		}
		return document_check_disk_status(doc, FALSE);
	}

	/* calls the edit popup menu in the editor */
	if (event->button == 3)
	{
		editor_find_current_word(doc->editor, editor_info.click_pos,
			current_word, sizeof current_word, NULL);

		ui_update_popup_goto_items((current_word[0] != '\0') ? TRUE : FALSE);
		ui_update_popup_copy_items(doc);
		ui_update_insert_include_item(doc, 0);

		if (geany_object)
		{
			g_signal_emit_by_name(geany_object, "update-editor-menu",
				current_word, editor_info.click_pos, doc);
		}
		gtk_menu_popup(GTK_MENU(main_widgets.editor_menu),
			NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}
	return FALSE;
}


static gboolean is_style_php(gint style)
{
	if ((style >= SCE_HPHP_DEFAULT && style <= SCE_HPHP_OPERATOR) ||
		style == SCE_HPHP_COMPLEX_VARIABLE)
	{
		return TRUE;
	}

	return FALSE;
}


typedef struct SCNotification SCNotification;

static void fold_symbol_click(ScintillaObject *sci, SCNotification *nt)
{
	gint line = SSM(sci, SCI_LINEFROMPOSITION, nt->position, 0);

	SSM(sci, SCI_TOGGLEFOLD, line, 0);
	/* extra toggling of child fold points
	 * use when editor_prefs.unfold_all_children is set and Shift is NOT pressed or when
	 * editor_prefs.unfold_all_children is NOT set but Shift is pressed */
	if ((editor_prefs.unfold_all_children && ! (nt->modifiers & SCMOD_SHIFT)) ||
		(! editor_prefs.unfold_all_children && (nt->modifiers & SCMOD_SHIFT)))
	{
		gint last_line = SSM(sci, SCI_GETLASTCHILD, line, -1);
		gint i;

		if (SSM(sci, SCI_GETLINEVISIBLE, line + 1, 0))
		{	/* unfold all children of the current fold point */
			for (i = line; i < last_line; i++)
			{
				if (! SSM(sci, SCI_GETLINEVISIBLE, i, 0))
				{
					SSM(sci, SCI_TOGGLEFOLD, SSM(sci, SCI_GETFOLDPARENT, i, 0), 0);
				}
			}
		}
		else
		{	/* fold all children of the current fold point */
			for (i = line; i < last_line; i++)
			{
				gint level = sci_get_fold_level(sci, i);
				if (level & SC_FOLDLEVELHEADERFLAG)
				{
					if (SSM(sci, SCI_GETFOLDEXPANDED, i, 0))
						SSM(sci, SCI_TOGGLEFOLD, i, 0);
				}
			}
		}
	}
}


static void on_margin_click(ScintillaObject *sci, SCNotification *nt)
{
	/* left click to marker margin marks the line */
	if (nt->margin == 1)
	{
		gint line = sci_get_line_from_position(sci, nt->position);
		gboolean set = sci_is_marker_set_at_line(sci, line, 1);

		/*sci_marker_delete_all(doc->editor->sci, 1);*/
		sci_set_marker_at_line(sci, line, ! set, 1);	/* toggle the marker */
	}
	/* left click on the folding margin to toggle folding state of current line */
	else if (nt->margin == 2 && editor_prefs.folding)
	{
		fold_symbol_click(sci, nt);
	}
}


static void on_update_ui(GeanyEditor *editor, G_GNUC_UNUSED SCNotification *nt)
{
	ScintillaObject *sci = editor->sci;
	gint pos = sci_get_current_position(sci);

	/* undo / redo menu update */
	ui_update_popup_reundo_items(editor->document);

	/* brace highlighting */
	editor_highlight_braces(editor, pos);

	ui_update_statusbar(editor->document, pos);

	/* Visible lines are only laid out accurately once [SCN_UPDATEUI] is sent,
	 * so we need to only call sci_scroll_to_line here, because the document
	 * may have line wrapping and folding enabled.
	 * http://scintilla.sourceforge.net/ScintillaDoc.html#LineWrapping */
	if (editor->scroll_percent > 0.0F)
	{
		editor_scroll_to_line(editor, -1, editor->scroll_percent);
		editor->scroll_percent = -1.0F;	/* disable further scrolling */
	}
#if 0
	/** experimental code for inverting selections */
	{
	gint i;
	for (i = SSM(sci, SCI_GETSELECTIONSTART, 0, 0); i < SSM(sci, SCI_GETSELECTIONEND, 0, 0); i++)
	{
		/* need to get colour from getstyleat(), but how? */
		SSM(sci, SCI_STYLESETFORE, STYLE_DEFAULT, 0);
		SSM(sci, SCI_STYLESETBACK, STYLE_DEFAULT, 0);
	}

	sci_get_style_at(sci, pos);
	}
#endif
}


static void check_line_breaking(GeanyEditor *editor, gint pos, gchar c)
{
	ScintillaObject *sci = editor->sci;
	gint line, lstart;

	if (!editor->line_breaking)
		return;

	if (c == GDK_space)
		pos--;	/* Look for previous space, not the new one */

	line = sci_get_current_line(sci);
	lstart = sci_get_position_from_line(sci, line);

	if (pos - lstart < editor_prefs.line_break_column)
		return;

	/* look for the last space before line_break_column */
	pos = MIN(pos, lstart + editor_prefs.line_break_column);

	while (pos > lstart)
	{
		c = sci_get_char_at(sci, --pos);
		if (c == GDK_space)
		{
			gint col, len, diff;
			const gchar *eol = editor_get_eol_char(editor);

			/* break the line after the space */
			sci_insert_text(sci, pos + 1, eol);
			line++;

			/* remember distance from end of line (we use column position in case
			 * the previous line gets altered, such as removing trailing spaces). */
			pos = sci_get_current_position(sci);
			len = sci_get_line_length(sci, line);
			col = sci_get_col_from_position(sci, pos);
			diff = len - col;

			/* set position as if user had pressed return */
			pos = sci_get_position_from_line(sci, line);
			sci_set_current_position(sci, pos, FALSE);
			/* add indentation, comment multilines, etc */
			on_new_line_added(editor);

			/* correct cursor position (might not be at line end) */
			pos = sci_get_position_from_line(sci, line);
			pos += sci_get_line_length(sci, line) - diff;
			sci_set_current_position(sci, pos, FALSE);
			return;
		}
	}
}


static void on_char_added(GeanyEditor *editor, SCNotification *nt)
{
	ScintillaObject *sci = editor->sci;
	gint pos = sci_get_current_position(sci);

	switch (nt->ch)
	{
		case '\r':
		{	/* simple indentation (only for CR format) */
			if (sci_get_eol_mode(sci) == SC_EOL_CR)
				on_new_line_added(editor);
			break;
		}
		case '\n':
		{	/* simple indentation (for CR/LF and LF format) */
			on_new_line_added(editor);
			break;
		}
		case '>':
		case '/':
		{	/* close xml-tags */
			handle_xml(editor, nt->ch);
			break;
		}
		case '(':
		{	/* show calltips */
			editor_show_calltip(editor, --pos);
			break;
		}
		case ')':
		{	/* hide calltips */
			if (SSM(sci, SCI_CALLTIPACTIVE, 0, 0))
			{
				SSM(sci, SCI_CALLTIPCANCEL, 0, 0);
			}
			g_free(calltip.text);
			calltip.text = NULL;
			calltip.pos = 0;
			calltip.sci = NULL;
			calltip.set = FALSE;
			break;
		}
		case '[':
		case '{':
		{	/* Tex auto-closing */
			if (sci_get_lexer(sci) == SCLEX_LATEX)
			{
				auto_close_bracket(sci, pos, nt->ch);	/* Tex auto-closing */
				editor_show_calltip(editor, --pos);
			}
			break;
		}
		case '}':
		{	/* closing bracket handling */
			if (editor->auto_indent)
				close_block(editor, pos - 1);
			break;
		}
		default:
			editor_start_auto_complete(editor, pos, FALSE);
	}
	check_line_breaking(editor, pos, nt->ch);
}


/* expand() and fold_changed() are copied from SciTE (thanks) to fix #1923350. */
static void expand(ScintillaObject *sci, gint *line, gboolean doExpand,
		gboolean force, gint visLevels, gint level)
{
	gint lineMaxSubord = SSM(sci, SCI_GETLASTCHILD, *line, level & SC_FOLDLEVELNUMBERMASK);
	gint levelLine = level;
	(*line)++;
	while (*line <= lineMaxSubord)
	{
		if (force)
		{
			if (visLevels > 0)
				SSM(sci, SCI_SHOWLINES, *line, *line);
			else
				SSM(sci, SCI_HIDELINES, *line, *line);
		}
		else
		{
			if (doExpand)
				SSM(sci, SCI_SHOWLINES, *line, *line);
		}
		if (levelLine == -1)
			levelLine = SSM(sci, SCI_GETFOLDLEVEL, *line, 0);
		if (levelLine & SC_FOLDLEVELHEADERFLAG)
		{
			if (force)
			{
				if (visLevels > 1)
					SSM(sci, SCI_SETFOLDEXPANDED, *line, 1);
				else
					SSM(sci, SCI_SETFOLDEXPANDED, *line, 0);
				expand(sci, line, doExpand, force, visLevels - 1, -1);
			}
			else
			{
				if (doExpand)
				{
					if (!SSM(sci, SCI_GETFOLDEXPANDED, *line, 0))
						SSM(sci, SCI_SETFOLDEXPANDED, *line, 1);
					expand(sci, line, TRUE, force, visLevels - 1, -1);
				}
				else
				{
					expand(sci, line, FALSE, force, visLevels - 1, -1);
				}
			}
		}
		else
		{
			(*line)++;
		}
	}
}


static void fold_changed(ScintillaObject *sci, gint line, gint levelNow, gint levelPrev)
{
	if (levelNow & SC_FOLDLEVELHEADERFLAG)
	{
		if (! (levelPrev & SC_FOLDLEVELHEADERFLAG))
		{
			/* Adding a fold point */
			SSM(sci, SCI_SETFOLDEXPANDED, line, 1);
			expand(sci, &line, TRUE, FALSE, 0, levelPrev);
		}
	}
	else if (levelPrev & SC_FOLDLEVELHEADERFLAG)
	{
		if (! SSM(sci, SCI_GETFOLDEXPANDED, line, 0))
		{	/* Removing the fold from one that has been contracted so should expand
			 * otherwise lines are left invisible with no way to make them visible */
			SSM(sci, SCI_SETFOLDEXPANDED, line, 1);
			expand(sci, &line, TRUE, FALSE, 0, levelPrev);
		}
	}
	if (! (levelNow & SC_FOLDLEVELWHITEFLAG) &&
			((levelPrev & SC_FOLDLEVELNUMBERMASK) > (levelNow & SC_FOLDLEVELNUMBERMASK)))
	{
		/* See if should still be hidden */
		gint parentLine = SSM(sci, SCI_GETFOLDPARENT, line, 0);
		if (parentLine < 0)
		{
			SSM(sci, SCI_SHOWLINES, line, line);
		}
		else if (SSM(sci, SCI_GETFOLDEXPANDED, parentLine, 0) &&
				SSM(sci, SCI_GETLINEVISIBLE, parentLine, 0))
		{
			SSM(sci, SCI_SHOWLINES, line, line);
		}
	}
}


static void ensure_range_visible(ScintillaObject *sci, gint posStart, gint posEnd,
		gboolean enforcePolicy)
{
	gint lineStart = SSM(sci, SCI_LINEFROMPOSITION, MIN(posStart, posEnd), 0);
	gint lineEnd = SSM(sci, SCI_LINEFROMPOSITION, MAX(posStart, posEnd), 0);
	gint line;

	for (line = lineStart; line <= lineEnd; line++)
	{
		SSM(sci, enforcePolicy ? SCI_ENSUREVISIBLEENFORCEPOLICY : SCI_ENSUREVISIBLE, line, 0);
	}
}


static gboolean reshow_calltip(gpointer data)
{
	SCNotification *nt = data;

	g_return_val_if_fail(calltip.sci != NULL, FALSE);

	SSM(calltip.sci, SCI_CALLTIPCANCEL, 0, 0);
	/* we use the position where the calltip was previously started as SCI_GETCURRENTPOS
	 * may be completely wrong in case the user cancelled the auto completion with the mouse */
	SSM(calltip.sci, SCI_CALLTIPSHOW, calltip.pos, (sptr_t) calltip.text);

	/* now autocompletion has been cancelled by SCI_CALLTIPSHOW, so do it manually */
	if (nt->nmhdr.code == SCN_AUTOCSELECTION)
	{
		gint pos = SSM(calltip.sci, SCI_GETCURRENTPOS, 0, 0);
		sci_set_selection_start(calltip.sci, nt->lParam);
		sci_set_selection_end(calltip.sci, pos);
		sci_replace_sel(calltip.sci, "");	/* clear root of word */
		SSM(calltip.sci, SCI_INSERTTEXT, nt->lParam, (sptr_t) nt->text);
		sci_goto_pos(calltip.sci, nt->lParam + strlen(nt->text), FALSE);
	}
	return FALSE;
}


/* callback func called by all editors when a signal arises */
void on_editor_notification(GtkWidget *widget, gint scn, gpointer lscn, gpointer user_data)
{
	SCNotification *nt;
	ScintillaObject *sci;
	GeanyDocument *doc = user_data;
	GeanyEditor *editor;

	editor = doc->editor;
	sci = editor->sci;

	nt = lscn;
	switch (nt->nmhdr.code)
	{
		case SCN_SAVEPOINTLEFT:
		{
			document_set_text_changed(doc, TRUE);
			break;
		}
		case SCN_SAVEPOINTREACHED:
		{
			document_set_text_changed(doc, FALSE);
			break;
		}
		case SCN_MODIFYATTEMPTRO:
		{
			utils_beep();
			break;
		}
		case SCN_MARGINCLICK:
			on_margin_click(sci, nt);
			break;

		case SCN_UPDATEUI:
			on_update_ui(editor, nt);
			break;

 		case SCN_MODIFIED:
		{
			if (nt->modificationType & SC_STARTACTION && ! ignore_callback)
			{
				/* get notified about undo changes */
				document_undo_add(doc, UNDO_SCINTILLA, NULL);
			}
			if (editor_prefs.folding && (nt->modificationType & SC_MOD_CHANGEFOLD) != 0)
			{
				/* handle special fold cases, e.g. #1923350 */
				fold_changed(sci, nt->line, nt->foldLevelNow, nt->foldLevelPrev);
			}
			break;
		}
		case SCN_CHARADDED:
			on_char_added(editor, nt);
			break;

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
		case SCN_AUTOCCANCELLED:
		case SCN_AUTOCSELECTION:
		{
			/* now that autocomplete is finishing or was cancelled, reshow calltips
			 * if they were showing */
			if (calltip.set)
			{
				/* delay the reshow of the calltip window to make sure it is actually displayed,
				 * without it might be not visible on SCN_AUTOCCANCEL */
				/* TODO g_idle_add() seems to be not enough, only with a timeout it works stable */
				g_timeout_add(50, reshow_calltip, nt);
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
		case SCN_NEEDSHOWN:
		{
			ensure_range_visible(sci, nt->position, nt->position + nt->length, FALSE);
			break;
		}
		case SCN_URIDROPPED:
		{
			if (nt->text != NULL)
			{
				document_open_file_list(nt->text, -1);
			}
			break;
		}
		case SCN_CALLTIPCLICK:
		{
			if (nt->position > 0)
			{
				switch (nt->position)
				{
					case 1:	/* up arrow */
						if (calltip.tag_index > 0)
							calltip.tag_index--;
						break;

					case 2: calltip.tag_index++; break;	/* down arrow */
				}
				editor_show_calltip(editor, -1);
			}
			break;
		}
	}
}


/* Note: this is the same as sci_get_tab_width(), but is still useful when you don't have
 * a scintilla pointer. */
static gint get_tab_width(const GeanyIndentPrefs *indent_prefs)
{
	if (indent_prefs->type == GEANY_INDENT_TYPE_BOTH)
		return indent_prefs->hard_tab_width;

	return indent_prefs->width;	/* tab width = indent width */
}


/* Returns a string containing width chars of whitespace, filled with simple space
 * characters or with the right number of tab characters, according to the indent prefs.
 * (Result is filled with tabs *and* spaces if width isn't a multiple of
 * the tab width). */
static gchar *
get_whitespace(const GeanyIndentPrefs *iprefs, gint width)
{
	g_return_val_if_fail(width >= 0, NULL);

	if (width == 0)
		return g_strdup("");

	if (iprefs->type == GEANY_INDENT_TYPE_SPACES)
	{
		return g_strnfill(width, ' ');
	}
	else
	{	/* first fill text with tabs and fill the rest with spaces */
		const gint tab_width = get_tab_width(iprefs);
		gint tabs = width / tab_width;
		gint spaces = width % tab_width;
		gint len = tabs + spaces;
		gchar *str;

		str = g_malloc(len + 1);

		memset(str, '\t', tabs);
		memset(str + tabs, ' ', spaces);
		str[len] = '\0';
		return str;
 	}
}


static const GeanyIndentPrefs *
get_default_indent_prefs(void)
{
	/* In future this might depend on the current project. */
	return editor_prefs.indentation;
}


/** Get the indentation prefs for the editor.
 * In future, the prefs might be different according to project or filetype.
 * @warning Always get a fresh result instead of keeping a pointer to it if the editor
 * settings may have changed, or if this function has been called for a different @a editor.
 * @param editor The editor, or @c NULL to get the default indent prefs.
 * @return The indent prefs. */
const GeanyIndentPrefs *
editor_get_indent_prefs(GeanyEditor *editor)
{
	static GeanyIndentPrefs iprefs;

	iprefs = *get_default_indent_prefs();

	if (!editor)
		return &iprefs;

	iprefs.type = editor->indent_type;

	if (!editor->auto_indent)
		iprefs.auto_indent_mode = GEANY_AUTOINDENT_NONE;

	return &iprefs;
}


static gchar *get_single_indent(GeanyEditor *editor)
{
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);

	return get_whitespace(iprefs, iprefs->width);
}


static void on_new_line_added(GeanyEditor *editor)
{
	ScintillaObject *sci = editor->sci;
	gint pos = sci_get_current_position(sci);
	gint line = sci_get_current_line(sci);

	/* simple indentation */
	if (editor->auto_indent)
	{
		insert_indent_after_line(editor, line - 1);
	}

	if (editor_prefs.auto_continue_multiline)
	{	/* " * " auto completion in multiline C/C++/D/Java comments */
		auto_multiline(editor, line);
	}

	if (editor_prefs.complete_snippets)
	{
		editor_auto_latex(editor, pos);
	}

	if (editor_prefs.newline_strip)
	{	/* strip the trailing spaces on the previous line */
		editor_strip_line_trailing_spaces(editor, line - 1);
	}
}


static gboolean lexer_has_braces(ScintillaObject *sci)
{
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);

	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_D:
		case SCLEX_HTML:	/* for PHP & JS */
		case SCLEX_PASCAL:	/* for multiline comments? */
		case SCLEX_BASH:
		case SCLEX_PERL:
		case SCLEX_TCL:
			return TRUE;
		default:
			return FALSE;
	}
}


/* Read indent chars for the line that pos is on into indent global variable.
 * Note: Use sci_get_line_indentation() and get_whitespace() instead in any new code.  */
static void read_indent(GeanyEditor *editor, gint pos)
{
	ScintillaObject *sci = editor->sci;
	guint i, len, j = 0;
	gint line;
	gchar *linebuf;
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);

	line = sci_get_line_from_position(sci, pos);

	len = sci_get_line_length(sci, line);
	linebuf = sci_get_line(sci, line);

	for (i = 0; i < len && j <= (sizeof(indent) - 1); i++)
	{
		if (linebuf[i] == ' ' || linebuf[i] == '\t')	/* simple indentation */
			indent[j++] = linebuf[i];
		else if (iprefs->auto_indent_mode <= GEANY_AUTOINDENT_BASIC)
			break;
	}
	indent[j] = '\0';
	g_free(linebuf);
}


static gint get_brace_indent(ScintillaObject *sci, gint line)
{
	guint i, len;
	gint ret = 0;
	gchar *linebuf;

	len = sci_get_line_length(sci, line);
	linebuf = sci_get_line(sci, line);

	for (i = 0; i < len; i++)
	{
		/* i == (len - 1) prevents wrong indentation after lines like
		 * "	{ return bless({}, shift); }" (Perl) */
		if (linebuf[i] == '{' && i == (len - 1))
		{
			ret++;
			break;
		}
		else
		{
			gint k = len - 1;

			while (k > 0 && isspace(linebuf[k])) k--;

			/* if last non-whitespace character is a { increase indentation by a tab
			 * e.g. for (...) { */
			if (linebuf[k] == '{')
			{
				ret++;
			}
			break;
		}
	}
	g_free(linebuf);
	return ret;
}


static gint get_python_indent(ScintillaObject *sci, gint line)
{
	gint last_char = sci_get_line_end_position(sci, line) - 1;

	/* add extra indentation for Python after colon */
	if (sci_get_char_at(sci, last_char) == ':' &&
		sci_get_style_at(sci, last_char) == SCE_P_OPERATOR)
	{
		return 1;
	}
	return 0;
}


static gint get_indent_size_after_line(GeanyEditor *editor, gint line)
{
	ScintillaObject *sci = editor->sci;
	gint size;
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);

	g_return_val_if_fail(line >= 0, 0);

	size = sci_get_line_indentation(sci, line);

	if (iprefs->auto_indent_mode > GEANY_AUTOINDENT_BASIC)
	{
		if (lexer_has_braces(sci))
			size += iprefs->width * get_brace_indent(sci, line);
		else
		if (FILETYPE_ID(editor->document->file_type) == GEANY_FILETYPES_PYTHON)
			size += iprefs->width * get_python_indent(sci, line);
	}
	return size;
}


static void insert_indent_after_line(GeanyEditor *editor, gint line)
{
	ScintillaObject *sci = editor->sci;
	gint size = get_indent_size_after_line(editor, line);
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);

	if (size > 0)
	{
		gchar *text;

		text = get_whitespace(iprefs, size);
		sci_add_text(sci, text);
		g_free(text);
	}
}


static void auto_close_bracket(ScintillaObject *sci, gint pos, gchar c)
{
	if (! editor_prefs.complete_snippets || SSM(sci, SCI_GETLEXER, 0, 0) != SCLEX_LATEX)
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
 * fit to work with close_block) */
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

	if (utils_is_opening_brace(chBrace, editor_prefs.brace_match_ltgt))
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


/* Called after typing '}'. */
static void close_block(GeanyEditor *editor, gint pos)
{
	GeanyDocument *doc;
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);
	gint x = 0, cnt = 0;
	gint line, line_len, eol_char_len;
	gchar *text, *line_buf;
	ScintillaObject *sci;
	gint line_indent, last_indent;

	if (iprefs->auto_indent_mode < GEANY_AUTOINDENT_CURRENTCHARS)
		return;
	if (editor == NULL || editor->document->file_type == NULL)
		return;

	sci = doc->editor->sci;
	doc = editor->document;

	if (! lexer_has_braces(sci))
		return;

	line = sci_get_line_from_position(sci, pos);
	line_len = sci_get_line_length(sci, line);
	/* set eol_char_len to 0 if on last line, because there is no EOL char */
	eol_char_len = (line == (SSM(sci, SCI_GETLINECOUNT, 0, 0) - 1)) ? 0 :
								editor_get_eol_char_len(editor);

	/* check that the line is empty, to not kill text in the line */
	line_buf = sci_get_line(sci, line);
	line_buf[line_len - eol_char_len] = '\0';
	while (x < (line_len - eol_char_len))
	{
		if (isspace(line_buf[x])) cnt++;
		x++;
	}
	g_free(line_buf);

	if ((line_len - eol_char_len - 1) != cnt)
		return;

	if (iprefs->auto_indent_mode == GEANY_AUTOINDENT_MATCHBRACES)
	{
		gint start_brace = brace_match(sci, pos);

		if (start_brace >= 0)
		{
			gint line_start;
			gint brace_line = sci_get_line_from_position(sci, start_brace);
			gint size = sci_get_line_indentation(sci, brace_line);
			gchar *ind = get_whitespace(iprefs, size);

			text = g_strconcat(ind, "}", NULL);
			line_start = sci_get_position_from_line(sci, line);
			sci_set_anchor(sci, line_start);
			SSM(sci, SCI_REPLACESEL, 0, (sptr_t) text);
			g_free(text);
			g_free(ind);
			return;
		}
		/* fall through - unmatched brace (possibly because of TCL, PHP lexer bugs) */
	}

	/* GEANY_AUTOINDENT_CURRENTCHARS */
	line_indent = sci_get_line_indentation(sci, line);
	last_indent = sci_get_line_indentation(sci, line - 1);

	if (line_indent < last_indent)
		return;
	line_indent -= iprefs->width;
	line_indent = MAX(0, line_indent);
	sci_set_line_indentation(sci, line, line_indent);
}


/* Reads the word at given cursor position and writes it into the given buffer. The buffer will be
 * NULL terminated in any case, even when the word is truncated because wordlen is too small.
 * position can be -1, then the current position is used.
 * wc are the wordchars to use, if NULL, GEANY_WORDCHARS will be used */
void editor_find_current_word(GeanyEditor *editor, gint pos, gchar *word, size_t wordlen,
							  const gchar *wc)
{
	gint line, line_start, startword, endword;
	gchar *chunk;
	ScintillaObject *sci;

	if (editor == NULL)
		return;
	sci = editor->sci;

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

	/* the checks for "c < 0" are to allow any Unicode character which should make the code
	 * a little bit more Unicode safe, anyway, this allows also any Unicode punctuation,
	 * TODO: improve this code */
	while (startword > 0 && (strchr(wc, chunk[startword - 1]) || chunk[startword - 1] < 0))
		startword--;
	while (chunk[endword] != 0 && (strchr(wc, chunk[endword]) || chunk[endword] < 0))
		endword++;
	if(startword == endword)
		return;

	chunk[endword] = '\0';

	g_strlcpy(word, chunk + startword, wordlen); /* ensure null terminated */
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
		if (utils_is_opening_brace(c, editor_prefs.brace_match_ltgt))
			return pos;
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
		if (brackets < 0) return pos;	/* found start bracket */
	}
	return -1;
}


static gboolean append_calltip(GString *str, const TMTag *tag, filetype_id ft_id)
{
	if (! tag->atts.entry.arglist) return FALSE;

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

	return TRUE;
}


static gchar *find_calltip(const gchar *word, GeanyFiletype *ft)
{
	const GPtrArray *tags;
	const gint arg_types = tm_tag_function_t | tm_tag_prototype_t |
		tm_tag_method_t | tm_tag_macro_with_arg_t;
	TMTagAttrType *attrs = NULL;
	TMTag *tag;
	GString *str = NULL;
	guint i;

	g_return_val_if_fail(ft && word && *word, NULL);

	tags = tm_workspace_find(word, arg_types | tm_tag_class_t, attrs, FALSE, ft->lang);
	if (tags->len == 0)
		return NULL;

	tag = TM_TAG(tags->pdata[0]);

	if (tag->type == tm_tag_class_t && FILETYPE_ID(ft) == GEANY_FILETYPES_D)
	{
		/* user typed e.g. 'new Classname(' so lookup D constructor Classname::this() */
		tags = tm_workspace_find_scoped("this", tag->name,
			arg_types, attrs, FALSE, ft->lang, TRUE);
		if (tags->len == 0)
			return NULL;
	}

	/* remove tags with no argument list */
	for (i = 0; i < tags->len; i++)
	{
		tag = TM_TAG(tags->pdata[i]);

		if (! tag->atts.entry.arglist)
			tags->pdata[i] = NULL;
	}
	tm_tags_prune((GPtrArray *) tags);
	if (tags->len == 0)
		return NULL;
	else
	{	/* remove duplicate calltips */
		TMTagAttrType sort_attr[] = {tm_tag_attr_name_t, tm_tag_attr_scope_t,
			tm_tag_attr_arglist_t, 0};

		tm_tags_sort((GPtrArray *) tags, sort_attr, TRUE);
	}

	/* if the current word has changed since last time, start with the first tag match */
	if (! utils_str_equal(word, calltip.last_word))
		calltip.tag_index = 0;
	/* cache the current word for next time */
	g_free(calltip.last_word);
	calltip.last_word = g_strdup(word);
	calltip.tag_index = MIN(calltip.tag_index, tags->len - 1);	/* ensure tag_index is in range */

	for (i = calltip.tag_index; i < tags->len; i++)
	{
		tag = TM_TAG(tags->pdata[i]);

		if (str == NULL)
		{
			str = g_string_new(NULL);
			if (calltip.tag_index > 0)
				g_string_prepend(str, "\001 ");	/* up arrow */
			append_calltip(str, tag, FILETYPE_ID(ft));
		}
		else /* add a down arrow */
		{
			if (calltip.tag_index > 0)	/* already have an up arrow */
				g_string_insert_c(str, 1, '\002');
			else
				g_string_prepend(str, "\002 ");
			break;
		}
	}
	if (str)
	{
		gchar *result = str->str;

		g_string_free(str, FALSE);
		return result;
	}
	return NULL;
}


/* use pos = -1 to search for the previous unmatched open bracket. */
gboolean editor_show_calltip(GeanyEditor *editor, gint pos)
{
	gint orig_pos = pos; /* the position for the calltip */
	gint lexer;
	gint style;
	gchar word[GEANY_MAX_WORD_LENGTH];
	gchar *str;
	ScintillaObject *sci;

	if (editor == NULL || editor->document->file_type == NULL)
		return FALSE;
	sci = editor->sci;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);

	if (pos == -1)
	{
		/* position of '(' is unknown, so go backwards from current position to find it */
		pos = SSM(sci, SCI_GETCURRENTPOS, 0, 0);
		pos--;
		orig_pos = pos;
		pos = (lexer == SCLEX_LATEX) ? find_previous_brace(sci, pos) :
			find_start_bracket(sci, pos);
		if (pos == -1) return FALSE;
	}

	/* the style 1 before the brace (which may be highlighted) */
	style = SSM(sci, SCI_GETSTYLEAT, pos - 1, 0);
	if (! is_code_style(lexer, style))
		return FALSE;

	word[0] = '\0';
	editor_find_current_word(editor, pos - 1, word, sizeof word, NULL);
	if (word[0] == '\0') return FALSE;

	str = find_calltip(word, editor->document->file_type);
	if (str)
	{
		g_free(calltip.text);	/* free the old calltip */
		calltip.text = str;
		calltip.pos = orig_pos;
		calltip.sci = sci;
		calltip.set = TRUE;
		utils_wrap_string(calltip.text, -1);
		SSM(sci, SCI_CALLTIPSHOW, orig_pos, (sptr_t) calltip.text);
		return TRUE;
	}
	return FALSE;
}


static void show_autocomplete(ScintillaObject *sci, gint rootlen, const gchar *words)
{
	/* store whether a calltip is showing, so we can reshow it after autocompletion */
	calltip.set = SSM(sci, SCI_CALLTIPACTIVE, 0, 0);
	SSM(sci, SCI_AUTOCSHOW, rootlen, (sptr_t) words);
}


static gboolean
autocomplete_html(ScintillaObject *sci, const gchar *root, gsize rootlen)
{	/* HTML entities auto completion */
	guint i, j = 0;
	GString *words;
	const gchar **entities = symbols_get_html_entities();

	if (*root != '&' || entities == NULL) return FALSE;

	words = g_string_sized_new(500);
	for (i = 0; ; i++)
	{
		if (entities[i] == NULL) break;
		else if (entities[i][0] == '#') continue;

		if (! strncmp(entities[i], root, rootlen))
		{
			if (j++ > 0) g_string_append_c(words, '\n');
			g_string_append(words, entities[i]);
		}
	}
	if (words->len > 0) show_autocomplete(sci, rootlen, words->str);
	g_string_free(words, TRUE);
	return TRUE;
}


/* Current document & global tags autocompletion */
static gboolean
autocomplete_tags(GeanyEditor *editor, const gchar *root, gsize rootlen)
{
	TMTagAttrType attrs[] = { tm_tag_attr_name_t, 0 };
	const GPtrArray *tags;
	ScintillaObject *sci;
	GeanyDocument *doc;

	if (editor == NULL || editor->document->file_type == NULL)
		return FALSE;
	sci = editor->sci;
	doc = editor->document;

	tags = tm_workspace_find(root, tm_tag_max_t, attrs, TRUE, doc->file_type->lang);
	if (NULL != tags && tags->len > 0)
	{
		GString *words = g_string_sized_new(150);
		guint j;

		for (j = 0; j < tags->len; ++j)
		{
			if (j > 0)
				g_string_append_c(words, '\n');

			if (j == editor_prefs.autocompletion_max_entries)
			{
				g_string_append(words, "...");
				break;
			}
			g_string_append(words, ((TMTag *) tags->pdata[j])->name);
		}
		show_autocomplete(sci, rootlen, words->str);
		g_string_free(words, TRUE);
	}
	return TRUE;
}


/* Check whether to use entity autocompletion:
 * - always in a HTML file except when inside embedded JavaScript, Python, ASP, ...
 * - in a PHP file only when we are outside of <? ?> */
static gboolean autocomplete_check_for_html(gint ft_id, gint style)
{
	/* use entity completion when style is not JavaScript, ASP, Python, PHP, ...
	 * (everything after SCE_HJ_START is for embedded scripting languages) */
	if (ft_id == GEANY_FILETYPES_HTML && style < SCE_HJ_START)
		return TRUE;

	if (ft_id == GEANY_FILETYPES_PHP)
	{
		/* use entity completion when style is outside of PHP styles */
		if (! is_style_php(style))
			return TRUE;
	}

	return FALSE;
}


gboolean editor_start_auto_complete(GeanyEditor *editor, gint pos, gboolean force)
{
	gint line, line_start, line_len, line_pos, current, rootlen, startword, lexer, style;
	gchar *linebuf, *root;
	ScintillaObject *sci;
	gboolean ret = FALSE;
	gchar *wordchars;
	GeanyFiletype *ft;

	if ((! editor_prefs.auto_complete_symbols && ! force) ||
		editor == NULL || editor->document->file_type == NULL)
		return FALSE;

	/* If we are at the beginning of the document, we skip autocompletion as we can't determine the
	 * necessary styling information */
	if (pos < 2)
		return FALSE;

	sci = editor->sci;
	ft = editor->document->file_type;

	line = sci_get_line_from_position(sci, pos);
	line_start = sci_get_position_from_line(sci, line);
	line_len = sci_get_line_length(sci, line);
	line_pos = pos - line_start - 1;
	current = pos - line_start;
	startword = current;
	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	style = SSM(sci, SCI_GETSTYLEAT, pos - 2, 0);

	/* don't autocomplete in comments and strings */
	if (!force && !is_code_style(lexer, style))
		return FALSE;

	linebuf = sci_get_line(sci, line);

	if (ft->id == GEANY_FILETYPES_LATEX)
		wordchars = GEANY_WORDCHARS"\\"; /* add \ to word chars if we are in a LaTeX file */
	else if (ft->id == GEANY_FILETYPES_HTML || ft->id == GEANY_FILETYPES_PHP)
		wordchars = GEANY_WORDCHARS"&"; /* add & to word chars if we are in a PHP or HTML file */
	else
		wordchars = GEANY_WORDCHARS;

	/* find the start of the current word */
	while ((startword > 0) && (strchr(wordchars, linebuf[startword - 1])))
		startword--;
	linebuf[current] = '\0';
	root = linebuf + startword;
	rootlen = current - startword;

	if (autocomplete_check_for_html(ft->id, style))
		ret = autocomplete_html(sci, root, rootlen);
	else
	{
		/* force is set when called by keyboard shortcut, otherwise start at the
		 * editor_prefs.symbolcompletion_min_chars'th char */
		if (force || rootlen >= editor_prefs.symbolcompletion_min_chars)
			ret = autocomplete_tags(editor, root, rootlen);
	}

	g_free(linebuf);
	return ret;
}


void editor_auto_latex(GeanyEditor *editor, gint pos)
{
	ScintillaObject *sci;

	if (editor == NULL || editor->document->file_type == NULL)
		return;
	sci = editor->sci;

	if (sci_get_char_at(sci, pos - 2) == '}')
	{
		gchar *eol, *buf, *construct;
		gchar env[50];
		gint line = sci_get_line_from_position(sci, pos - 2);
		gint line_len = sci_get_line_length(sci, line);
		gint i, start;

		/* get the line */
		buf = sci_get_line(sci, line);

		/* get to the first non-blank char (some kind of ltrim()) */
		start = 0;
		/*while (isspace(buf[i++])) start++;*/
		while (isspace(buf[start])) start++;

		/* check for begin */
		if (strncmp(buf + start, "\\begin", 6) == 0)
		{
			gchar full_cmd[15];
			guint j = 0;

			/* take also "\begingroup" (or whatever there can be) and
			 * append "\endgroup" and so on. */
			i = start + 6;
			while (i < line_len && buf[i] != '{' && j < (sizeof(full_cmd) - 1))
			{	/* copy all between "\begin" and "{" to full_cmd */
				full_cmd[j] = buf[i];
				i++;
				j++;
			}
			full_cmd[j] = '\0';

			/* go through the line and get the environment */
			for (i = start + j; i < line_len; i++)
			{
				if (buf[i] == '{')
				{
					j = 0;
					i++;
					while (buf[i] != '}' && j < (sizeof(env) - 1))
					{	/* this could be done in a shorter way, but so it remains readable ;-) */
						env[j] = buf[i];
						j++;
						i++;
					}
					env[j] = '\0';
					break;
				}
			}

			/* get the indentation */
			if (editor->auto_indent)
				read_indent(editor, pos);
			eol = g_strconcat(editor_get_eol_char(editor), indent, NULL);

			construct = g_strdup_printf("%s\\end%s{%s}", eol, full_cmd, env);

			SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) construct);
			sci_goto_pos(sci, pos + 1, TRUE);
			g_free(construct);
			g_free(eol);
		}
		/* later there could be some else ifs for other keywords */

		g_free(buf);
	}
}


static gchar *snippets_find_completion_by_name(const gchar *type, const gchar *name)
{
	gchar *result = NULL;
	GHashTable *tmp;

	g_return_val_if_fail(type != NULL && name != NULL, NULL);

	tmp = g_hash_table_lookup(editor_prefs.snippets, type);
	if (tmp != NULL)
	{
		result = g_hash_table_lookup(tmp, name);
	}
	/* whether nothing is set for the current filetype(tmp is NULL) or
	 * the particular completion for this filetype is not set (result is NULL) */
	if (tmp == NULL || result == NULL)
	{
		tmp = g_hash_table_lookup(editor_prefs.snippets, "Default");
		if (tmp != NULL)
		{
			result = g_hash_table_lookup(tmp, name);
		}
	}
	/* if result is still NULL here, no completion could be found */

	/* result is owned by the hash table and will be freed when the table will destroyed */
	return g_strdup(result);
}


/* This is very ugly but passing the pattern to ac_replace_specials() doesn't work because it is
 * modified when replacing a completion but the foreach function still passes the old pointer
 * to ac_replace_specials, so we use a global pointer outside of ac_replace_specials and
 * ac_complete_constructs. Any hints to improve this are welcome. */
static gchar *snippets_global_pattern = NULL;

void snippets_replace_specials(gpointer key, gpointer value, gpointer user_data)
{
	gchar *needle;

	if (key == NULL || value == NULL)
		return;

	needle = g_strconcat("%", (gchar*) key, "%", NULL);

	snippets_global_pattern = utils_str_replace(snippets_global_pattern, needle, (gchar*) value);
	g_free(needle);
}


static gchar *snippets_replace_wildcards(GeanyEditor *editor, gchar *text)
{
	gchar *year = utils_get_date_time(template_prefs.year_format, NULL);
	gchar *date = utils_get_date_time(template_prefs.date_format, NULL);
	gchar *datetime = utils_get_date_time(template_prefs.datetime_format, NULL);
	gchar *basename = g_path_get_basename(DOC_FILENAME(editor->document));

	text = templates_replace_all(text, year, date);
	text = utils_str_replace(text, "{datetime}", datetime);
	text = utils_str_replace(text, "{filename}", basename);

	g_free(year);
	g_free(date);
	g_free(datetime);
	g_free(basename);

	return text;
}


static gboolean snippets_complete_constructs(GeanyEditor *editor, gint pos, const gchar *word)
{
	gchar *str;
	gchar *pattern;
	gchar *lindent;
	gchar *whitespace;
	gint step, str_len;
	gint ft_id = FILETYPE_ID(editor->document->file_type);
	GHashTable *specials;
	ScintillaObject *sci = editor->sci;

	str = g_strdup(word);
	g_strstrip(str);

	pattern = snippets_find_completion_by_name(filetypes[ft_id]->name, str);
	if (pattern == NULL || pattern[0] == '\0')
	{
		utils_free_pointers(str, pattern, NULL); /* free pattern in case it is "" */
		return FALSE;
	}

	read_indent(editor, pos);
	lindent = g_strconcat(editor_get_eol_char(editor), indent, NULL);
	whitespace = get_single_indent(editor);

	/* remove the typed word, it will be added again by the used auto completion
	 * (not really necessary but this makes the auto completion more flexible,
	 *  e.g. with a completion like hi=hello, so typing "hi<TAB>" will result in "hello") */
	str_len = strlen(str);
	sci_set_selection_start(sci, pos - str_len);
	sci_set_selection_end(sci, pos);
	sci_replace_sel(sci, "");
	pos -= str_len; /* pos has changed while deleting */

	/* replace 'special' completions */
	specials = g_hash_table_lookup(editor_prefs.snippets, "Special");
	if (specials != NULL)
	{
		/* ugly hack using global_pattern */
		snippets_global_pattern = pattern;
		g_hash_table_foreach(specials, snippets_replace_specials, NULL);
		pattern = snippets_global_pattern;
	}

	/* replace line breaks and whitespaces */
	pattern = utils_str_replace(pattern, "\n", "%newline%"); /* to avoid endless replacing of \n */
	pattern = utils_str_replace(pattern, "%newline%", lindent);

	pattern = utils_str_replace(pattern, "\t", "%ws%"); /* to avoid endless replacing of \t */
	pattern = utils_str_replace(pattern, "%ws%", whitespace);

	/* replace any %template% wildcards */
	pattern = snippets_replace_wildcards(editor, pattern);

	/* find the %cursor% pos (has to be done after all other operations) */
	step = utils_strpos(pattern, "%cursor%");
	if (step != -1)
		pattern = utils_str_replace(pattern, "%cursor%", "");

	/* finally insert the text and set the cursor */
	SSM(sci, SCI_INSERTTEXT, pos, (sptr_t) pattern);
	if (step != -1)
		sci_goto_pos(sci, pos + step, TRUE);
	else
		sci_goto_pos(sci, pos + strlen(pattern), TRUE);

	utils_free_pointers(pattern, whitespace, lindent, str, NULL);
 	return TRUE;
}


static gboolean at_eol(ScintillaObject *sci, gint pos)
{
	gint line = sci_get_line_from_position(sci, pos);
	gchar c;

	/* skip any trailing spaces */
	while (TRUE)
	{
		c = sci_get_char_at(sci, pos);
		if (c == ' ' || c == '\t')
			pos++;
		else
			break;
	}

	return (pos == sci_get_line_end_position(sci, line));
}


gboolean editor_complete_snippet(GeanyEditor *editor, gint pos)
{
	gboolean result = FALSE;
	gint lexer, style;
	gchar *wc;
	ScintillaObject *sci;

	if (editor == NULL)
		return FALSE;

	sci = editor->sci;
	/* return if we are editing an existing line (chars on right of cursor) */
	if (! editor_prefs.complete_snippets_whilst_editing && ! at_eol(sci, pos))
		return FALSE;

	lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	style = SSM(sci, SCI_GETSTYLEAT, pos - 2, 0);

	wc = snippets_find_completion_by_name("Special", "wordchars");
	editor_find_current_word(editor, pos, current_word, sizeof current_word, wc);

	/* prevent completion of "for " */
	if (! isspace(sci_get_char_at(sci, pos - 1))) /* pos points to the line end char so use pos -1 */
	{
		sci_start_undo_action(sci);	/* needed because we insert a space separately from construct */
		result = snippets_complete_constructs(editor, pos, current_word);
		sci_end_undo_action(sci);
		if (result)
			SSM(sci, SCI_CANCEL, 0, 0);	/* cancel any autocompletion list, etc */
	}

	g_free(wc);
	return result;
}


void editor_show_macro_list(GeanyEditor *editor)
{
	GString *words;

	if (editor == NULL) return;

	words = symbols_get_macro_list();
	if (words == NULL) return;

	SSM(editor->sci, SCI_USERLISTSHOW, 1, (sptr_t) words->str);
	g_string_free(words, TRUE);
}


static void insert_closing_tag(GeanyEditor *editor, gint pos, gchar ch, const gchar *tag_name)
{
	ScintillaObject *sci = editor->sci;
	gchar *to_insert = NULL;

	if (ch == '/')
	{
		const gchar *gt = ">";
		/* if there is already a '>' behind the cursor, don't add it */
		if (sci_get_char_at(sci, pos) == '>')
			gt = "";

		to_insert = g_strconcat(tag_name, gt, NULL);
	}
	else
		to_insert = g_strconcat("</", tag_name, ">", NULL);

	sci_start_undo_action(sci);
	sci_replace_sel(sci, to_insert);
	if (ch == '>')
	{
		SSM(sci, SCI_SETSEL, pos, pos);
		if (utils_str_equal(tag_name, "table"))
			auto_table(editor, pos);
	}
	sci_end_undo_action(sci);
	g_free(to_insert);
}


/*
 * (stolen from anjuta and heavily modified)
 * This routine will auto complete XML or HTML tags that are still open by closing them
 * @param ch The character we are dealing with, currently only works with the '>' character
 * @return True if handled, false otherwise
 */
static gboolean handle_xml(GeanyEditor *editor, gchar ch)
{
	ScintillaObject *sci = editor->sci;
	gint lexer = SSM(sci, SCI_GETLEXER, 0, 0);
	gint pos, min;
	gchar *str_found, sel[512];
	gboolean result = FALSE;

	/* If the user has turned us off, quit now.
	 * This may make sense only in certain languages */
	if (! editor_prefs.auto_close_xml_tags || (lexer != SCLEX_HTML && lexer != SCLEX_XML))
		return FALSE;

	pos = sci_get_current_position(sci);

	/* return if we are in PHP but not in a string or outside of <? ?> tags */
	if (editor->document->file_type->id == GEANY_FILETYPES_PHP)
	{
		gint style = sci_get_style_at(sci, pos);
		if (is_style_php(style) && style != SCE_HPHP_SIMPLESTRING && style != SCE_HPHP_HSTRING)
			return FALSE;
	}

	/* if ch is /, check for </, else quit */
	if (ch == '/' && sci_get_char_at(sci, pos - 2) != '<')
		return FALSE;

	/* Grab the last 512 characters or so */
	min = pos - (sizeof(sel) - 1);
	if (min < 0) min = 0;

	if (pos - min < 3)
		return FALSE; /* Smallest tag is 3 characters e.g. <p> */

	sci_get_text_range(sci, min, pos, sel);
	sel[sizeof(sel) - 1] = '\0';

	if (ch == '>' && sel[pos - min - 2] == '/')
		/* User typed something like "<br/>" */
		return FALSE;

	str_found = utils_find_open_xml_tag(sel, pos - min, (ch == '/'));

	/* when found string is something like br, img or another short tag, quit */
	if (utils_str_equal(str_found, "br")
	 || utils_str_equal(str_found, "img")
	 || utils_str_equal(str_found, "base")
	 || utils_str_equal(str_found, "basefont")	/* < or not < */
	 || utils_str_equal(str_found, "frame")
	 || utils_str_equal(str_found, "input")
	 || utils_str_equal(str_found, "link")
	 || utils_str_equal(str_found, "area")
	 || utils_str_equal(str_found, "meta"))
	{
		/* ignore tag */
	}
	else if (NZV(str_found))
	{
		insert_closing_tag(editor, pos, ch, str_found);
		result = TRUE;
	}
	g_free(str_found);
	return result;
}


static gsize count_indent_size(GeanyEditor *editor, const gchar *base_indent)
{
	const gchar *ptr;
	gsize tab_size = sci_get_tab_width(editor->sci);
	gsize count = 0;

	g_return_val_if_fail(base_indent, 0);

	for (ptr = base_indent; *ptr != 0; ptr++)
	{
		switch (*ptr)
		{
			case ' ':
				count++;
				break;
			case '\t':
				count += tab_size;
				break;
		}
	}
	return count;
}


static void string_append_indent_width(GString *str, const GeanyIndentPrefs *iprefs,
		gsize width)
{
	gchar *ws = get_whitespace(iprefs, width);

	g_string_append(str, ws);
	g_free(ws);
}


static gchar *get_table_body(GeanyEditor *editor, const gchar *base_indent)
{
	gsize base_size = count_indent_size(editor, base_indent);
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);
	gsize indent_width = iprefs->width;
	GString *str = g_string_new("\n");

	if (! editor->auto_indent)
		indent_width = 0;

	string_append_indent_width(str, iprefs, base_size + indent_width);
	g_string_append(str, "<tr>\n");
	string_append_indent_width(str, iprefs, base_size + indent_width + indent_width);
	g_string_append(str, "<td>\n");
	string_append_indent_width(str, iprefs, base_size + indent_width + indent_width);
	g_string_append(str, "</td>\n");
	string_append_indent_width(str, iprefs, base_size + indent_width);
	g_string_append(str, "</tr>\n");
	string_append_indent_width(str, iprefs, base_size);

	return g_string_free(str, FALSE);
}


static void auto_table(GeanyEditor *editor, gint pos)
{
	ScintillaObject *sci = editor->sci;
	gchar *table;
	gint indent_pos;

	if (SSM(sci, SCI_GETLEXER, 0, 0) != SCLEX_HTML) return;

	read_indent(editor, pos);
	indent_pos = sci_get_line_indent_position(sci, sci_get_line_from_position(sci, pos));
	if ((pos - 7) != indent_pos) /* 7 == strlen("<table>") */
	{
		gint i;
		guint x;

		x = strlen(indent);
		/* find the start of the <table tag */
		i = 1;
		while (i <= pos && sci_get_char_at(sci, pos - i) != '<') i++;
		/* add all non whitespace before the tag to the indent string */
		while ((pos - i) != indent_pos && x < sizeof(indent) - 1)
		{
			indent[x++] = ' ';
			i++;
		}
		indent[x] = '\0';
	}

	/* get indent string for generated code */
	table = get_table_body(editor, indent);
	sci_insert_text(sci, pos, table);
	g_free(table);
}


static void real_comment_multiline(GeanyEditor *editor, gint line_start, gint last_line)
{
	const gchar *eol;
	gchar *str_begin, *str_end;
	gint line_len;

	if (editor == NULL || editor->document->file_type == NULL)
		return;

	eol = editor_get_eol_char(editor);
	str_begin = g_strdup_printf("%s%s", editor->document->file_type->comment_open, eol);
	str_end = g_strdup_printf("%s%s", editor->document->file_type->comment_close, eol);

	/* insert the comment strings */
	sci_insert_text(editor->sci, line_start, str_begin);
	line_len = sci_get_position_from_line(editor->sci, last_line + 2);
	sci_insert_text(editor->sci, line_len, str_end);

	g_free(str_begin);
	g_free(str_end);
}


static void real_uncomment_multiline(GeanyEditor *editor)
{
	/* find the beginning of the multi line comment */
	gint pos, line, len, x;
	gchar *linebuf;
	GeanyDocument *doc;

	if (editor == NULL || editor->document->file_type == NULL)
		return;
	doc = editor->document;

	/* remove comment open chars */
	pos = document_find_text(doc, doc->file_type->comment_open, 0, TRUE, FALSE, NULL);
	SSM(doc->editor->sci, SCI_DELETEBACK, 0, 0);

	/* check whether the line is empty and can be deleted */
	line = sci_get_line_from_position(editor->sci, pos);
	len = sci_get_line_length(editor->sci, line);
	linebuf = sci_get_line(editor->sci, line);
	x = 0;
	while (linebuf[x] != '\0' && isspace(linebuf[x])) x++;
	if (x == len) SSM(editor->sci, SCI_LINEDELETE, 0, 0);
	g_free(linebuf);

	/* remove comment close chars */
	pos = document_find_text(doc, doc->file_type->comment_close, 0, FALSE, FALSE, NULL);
	SSM(editor->sci, SCI_DELETEBACK, 0, 0);

	/* check whether the line is empty and can be deleted */
	line = sci_get_line_from_position(editor->sci, pos);
	len = sci_get_line_length(editor->sci, line);
	linebuf = sci_get_line(editor->sci, line);
	x = 0;
	while (linebuf[x] != '\0' && isspace(linebuf[x])) x++;
	if (x == len) SSM(editor->sci, SCI_LINEDELETE, 0, 0);
	g_free(linebuf);
}


/* set toggle to TRUE if the caller is the toggle function, FALSE otherwise
 * returns the amount of uncommented single comment lines, in case of multi line uncomment
 * it returns just 1 */
gint editor_do_uncomment(GeanyEditor *editor, gint line, gboolean toggle)
{
	gint first_line, last_line;
	gint x, i, line_start, line_len;
	gint sel_start, sel_end;
	gint count = 0;
	gsize co_len;
	gchar sel[256], *co, *cc;
	gboolean break_loop = FALSE, single_line = FALSE;
	GeanyFiletype *ft;

	if (editor == NULL || editor->document->file_type == NULL)
		return 0;

	if (line < 0)
	{	/* use selection or current line */
		sel_start = sci_get_selection_start(editor->sci);
		sel_end = sci_get_selection_end(editor->sci);

		first_line = sci_get_line_from_position(editor->sci, sel_start);
		/* Find the last line with chars selected (not EOL char) */
		last_line = sci_get_line_from_position(editor->sci,
			sel_end - editor_get_eol_char_len(editor));
		last_line = MAX(first_line, last_line);
	}
	else
	{
		first_line = last_line = line;
		sel_start = sel_end = sci_get_position_from_line(editor->sci, line);
	}

	ft = editor->document->file_type;

	/* detection of HTML vs PHP code, if non-PHP set filetype to XML */
	line_start = sci_get_position_from_line(editor->sci, first_line);
	if (ft->id == GEANY_FILETYPES_PHP)
	{
		if (! is_style_php(sci_get_style_at(editor->sci, line_start)))
			ft = filetypes[GEANY_FILETYPES_XML];
	}

	co = ft->comment_open;
	cc = ft->comment_close;
	if (co == NULL)
		return 0;

	co_len = strlen(co);
	if (co_len == 0)
		return 0;

	SSM(editor->sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		gint buf_len;

		line_start = sci_get_position_from_line(editor->sci, i);
		line_len = sci_get_line_length(editor->sci, i);
		x = 0;

		buf_len = MIN((gint)sizeof(sel) - 1, line_len - 1);
		if (buf_len <= 0)
			continue;
		sci_get_text_range(editor->sci, line_start, line_start + buf_len, sel);
		sel[buf_len] = '\0';

		while (isspace(sel[x])) x++;

		/* to skip blank lines */
		if (x < line_len && sel[x] != '\0')
		{
			/* use single line comment */
			if (cc == NULL || strlen(cc) == 0)
			{
				single_line = TRUE;

				if (toggle)
				{
					gsize tm_len = strlen(editor_prefs.comment_toggle_mark);
					if (strncmp(sel + x, co, co_len) != 0 ||
						strncmp(sel + x + co_len, editor_prefs.comment_toggle_mark, tm_len) != 0)
						continue;

					co_len += tm_len;
				}
				else
				{
					if (strncmp(sel + x, co, co_len) != 0)
						continue;
				}

				SSM(editor->sci, SCI_SETSEL, line_start + x, line_start + x + co_len);
				sci_replace_sel(editor->sci, "");
				count++;
			}
			/* use multi line comment */
			else
			{
				gint style_comment;
				gint lexer = SSM(editor->sci, SCI_GETLEXER, 0, 0);

				/* process only lines which are already comments */
				switch (lexer)
				{	/* I will list only those lexers which support multi line comments */
					case SCLEX_XML:
					case SCLEX_HTML:
					{
						if (is_style_php(sci_get_style_at(editor->sci, line_start)))
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
				if (sci_get_style_at(editor->sci, line_start + x) == style_comment)
				{
					real_uncomment_multiline(editor);
					count = 1;
				}

				/* break because we are already on the last line */
				break_loop = TRUE;
				break;
			}
		}
	}
	SSM(editor->sci, SCI_ENDUNDOACTION, 0, 0);

	/* restore selection if there is one
	 * but don't touch the selection if caller is editor_do_comment_toggle */
	if (! toggle && sel_start < sel_end)
	{
		if (single_line)
		{
			sci_set_selection_start(editor->sci, sel_start - co_len);
			sci_set_selection_end(editor->sci, sel_end - (count * co_len));
		}
		else
		{
			gint eol_len = editor_get_eol_char_len(editor);
			sci_set_selection_start(editor->sci, sel_start - co_len - eol_len);
			sci_set_selection_end(editor->sci, sel_end - co_len - eol_len);
		}
	}

	return count;
}


void editor_do_comment_toggle(GeanyEditor *editor)
{
	gint first_line, last_line;
	gint x, i, line_start, line_len, first_line_start;
	gint sel_start, sel_end;
	gint count_commented = 0, count_uncommented = 0;
	gchar sel[256], *co, *cc;
	gboolean break_loop = FALSE, single_line = FALSE;
	gboolean first_line_was_comment = FALSE;
	gsize co_len;
	gsize tm_len = strlen(editor_prefs.comment_toggle_mark);
	GeanyFiletype *ft;

	if (editor == NULL || editor->document->file_type == NULL)
		return;

	sel_start = sci_get_selection_start(editor->sci);
	sel_end = sci_get_selection_end(editor->sci);

	ft = editor->document->file_type;

	first_line = sci_get_line_from_position(editor->sci,
		sci_get_selection_start(editor->sci));
	/* Find the last line with chars selected (not EOL char) */
	last_line = sci_get_line_from_position(editor->sci,
		sci_get_selection_end(editor->sci) - editor_get_eol_char_len(editor));
	last_line = MAX(first_line, last_line);

	/* detection of HTML vs PHP code, if non-PHP set filetype to XML */
	first_line_start = sci_get_position_from_line(editor->sci, first_line);
	if (ft->id == GEANY_FILETYPES_PHP)
	{
		if (! is_style_php(sci_get_style_at(editor->sci, first_line_start)))
			ft = filetypes[GEANY_FILETYPES_XML];
	}

	co = ft->comment_open;
	cc = ft->comment_close;
	if (co == NULL)
		return;

	co_len = strlen(co);
	if (co_len == 0)
		return;

	SSM(editor->sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		gint buf_len;

		line_start = sci_get_position_from_line(editor->sci, i);
		line_len = sci_get_line_length(editor->sci, i);
		x = 0;

		buf_len = MIN((gint)sizeof(sel) - 1, line_len - 1);
		if (buf_len < 0)
			continue;
		sci_get_text_range(editor->sci, line_start, line_start + buf_len, sel);
		sel[buf_len] = '\0';

		while (isspace(sel[x])) x++;

		/* use single line comment */
		if (cc == NULL || strlen(cc) == 0)
		{
			gboolean do_continue = FALSE;
			single_line = TRUE;

			if (strncmp(sel + x, co, co_len) == 0 &&
				strncmp(sel + x + co_len, editor_prefs.comment_toggle_mark, tm_len) == 0)
			{
				do_continue = TRUE;
			}

			if (do_continue && i == first_line)
				first_line_was_comment = TRUE;

			if (do_continue)
			{
				count_uncommented += editor_do_uncomment(editor, i, TRUE);
				continue;
			}

			/* we are still here, so the above lines were not already comments, so comment it */
			editor_do_comment(editor, i, TRUE, TRUE);
			count_commented++;
		}
		/* use multi line comment */
		else
		{
			gint style_comment;
			gint lexer = SSM(editor->sci, SCI_GETLEXER, 0, 0);

			/* skip lines which are already comments */
			switch (lexer)
			{	/* I will list only those lexers which support multi line comments */
				case SCLEX_XML:
				case SCLEX_HTML:
				{
					if (is_style_php(sci_get_style_at(editor->sci, line_start)))
						style_comment = SCE_HPHP_COMMENT;
					else style_comment = SCE_H_COMMENT;
					break;
				}
				case SCLEX_CSS: style_comment = SCE_CSS_COMMENT; break;
				case SCLEX_SQL: style_comment = SCE_SQL_COMMENT; break;
				case SCLEX_CAML: style_comment = SCE_CAML_COMMENT; break;
				case SCLEX_D: style_comment = SCE_D_COMMENT; break;
				case SCLEX_RUBY: style_comment = SCE_RB_POD; break;
				case SCLEX_PERL: style_comment = SCE_PL_POD; break;
				default: style_comment = SCE_C_COMMENT;
			}
			if (sci_get_style_at(editor->sci, line_start + x) == style_comment)
			{
				real_uncomment_multiline(editor);
				count_uncommented++;
			}
			else
			{
				real_comment_multiline(editor, line_start, last_line);
				count_commented++;
			}

			/* break because we are already on the last line */
			break_loop = TRUE;
			break;
		}
	}

	SSM(editor->sci, SCI_ENDUNDOACTION, 0, 0);

	co_len += tm_len;

	/* restore selection if there is one */
	if (sel_start < sel_end)
	{
		if (single_line)
		{
			gint a = (first_line_was_comment) ? - co_len : co_len;

			/* don't modify sel_start when the selection starts within indentation */
			read_indent(editor, sel_start);
			if ((sel_start - first_line_start) <= (gint) strlen(indent))
				a = 0;

			sci_set_selection_start(editor->sci, sel_start + a);
			sci_set_selection_end(editor->sci, sel_end +
								(count_commented * co_len) - (count_uncommented * co_len));
		}
		else
		{
			gint eol_len = editor_get_eol_char_len(editor);
			if (count_uncommented > 0)
			{
				sci_set_selection_start(editor->sci, sel_start - co_len - eol_len);
				sci_set_selection_end(editor->sci, sel_end - co_len - eol_len);
			}
			else
			{
				sci_set_selection_start(editor->sci, sel_start + co_len + eol_len);
				sci_set_selection_end(editor->sci, sel_end + co_len + eol_len);
			}
		}
	}
	else if (count_uncommented > 0)
	{
		sci_set_current_position(editor->sci, sel_start - co_len, TRUE);
	}
}


/* set toggle to TRUE if the caller is the toggle function, FALSE otherwise */
void editor_do_comment(GeanyEditor *editor, gint line, gboolean allow_empty_lines, gboolean toggle)
{
	gint first_line, last_line;
	gint x, i, line_start, line_len;
	gint sel_start, sel_end, co_len;
	gchar sel[256], *co, *cc;
	gboolean break_loop = FALSE, single_line = FALSE;
	GeanyFiletype *ft;

	if (editor == NULL || editor->document->file_type == NULL)
		return;

	if (line < 0)
	{	/* use selection or current line */
		sel_start = sci_get_selection_start(editor->sci);
		sel_end = sci_get_selection_end(editor->sci);

		first_line = sci_get_line_from_position(editor->sci, sel_start);
		/* Find the last line with chars selected (not EOL char) */
		last_line = sci_get_line_from_position(editor->sci,
			sel_end - editor_get_eol_char_len(editor));
		last_line = MAX(first_line, last_line);
	}
	else
	{
		first_line = last_line = line;
		sel_start = sel_end = sci_get_position_from_line(editor->sci, line);
	}

	ft = editor->document->file_type;

	/* detection of HTML vs PHP code, if non-PHP set filetype to XML */
	line_start = sci_get_position_from_line(editor->sci, first_line);
	if (ft->id == GEANY_FILETYPES_PHP)
	{
		if (! is_style_php(sci_get_style_at(editor->sci, line_start)))
			ft = filetypes[GEANY_FILETYPES_XML];
	}

	co = ft->comment_open;
	cc = ft->comment_close;
	if (co == NULL)
		return;

	co_len = strlen(co);
	if (co_len == 0)
		return;

	SSM(editor->sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; (i <= last_line) && (! break_loop); i++)
	{
		gint buf_len;

		line_start = sci_get_position_from_line(editor->sci, i);
		line_len = sci_get_line_length(editor->sci, i);
		x = 0;

		buf_len = MIN((gint)sizeof(sel) - 1, line_len - 1);
		if (buf_len < 0)
			continue;
		sci_get_text_range(editor->sci, line_start, line_start + buf_len, sel);
		sel[buf_len] = '\0';

		while (isspace(sel[x])) x++;

		/* to skip blank lines */
		if (allow_empty_lines || (x < line_len && sel[x] != '\0'))
		{
			/* use single line comment */
			if (cc == NULL || strlen(cc) == 0)
			{
				gint start = line_start;
				single_line = TRUE;

				if (ft->comment_use_indent)
					start = line_start + x;

				if (toggle)
				{
					gchar *text = g_strconcat(co, editor_prefs.comment_toggle_mark, NULL);
					sci_insert_text(editor->sci, start, text);
					g_free(text);
				}
				else
					sci_insert_text(editor->sci, start, co);
			}
			/* use multi line comment */
			else
			{
				gint style_comment;
				gint lexer = SSM(editor->sci, SCI_GETLEXER, 0, 0);

				/* skip lines which are already comments */
				switch (lexer)
				{	/* I will list only those lexers which support multi line comments */
					case SCLEX_XML:
					case SCLEX_HTML:
					{
						if (is_style_php(sci_get_style_at(editor->sci, line_start)))
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
				if (sci_get_style_at(editor->sci, line_start + x) == style_comment) continue;

				real_comment_multiline(editor, line_start, last_line);

				/* break because we are already on the last line */
				break_loop = TRUE;
				break;
			}
		}
	}
	SSM(editor->sci, SCI_ENDUNDOACTION, 0, 0);

	/* restore selection if there is one
	 * but don't touch the selection if caller is editor_do_comment_toggle */
	if (! toggle && sel_start < sel_end)
	{
		if (single_line)
		{
			sci_set_selection_start(editor->sci, sel_start + co_len);
			sci_set_selection_end(editor->sci, sel_end + ((i - first_line) * co_len));
		}
		else
		{
			gint eol_len = editor_get_eol_char_len(editor);
			sci_set_selection_start(editor->sci, sel_start + co_len + eol_len);
			sci_set_selection_end(editor->sci, sel_end + co_len + eol_len);
		}
	}
}


void editor_highlight_braces(GeanyEditor *editor, gint cur_pos)
{
	gint brace_pos = cur_pos - 1;
	gint end_pos;

	if (! utils_isbrace(sci_get_char_at(editor->sci, brace_pos), editor_prefs.brace_match_ltgt))
	{
		brace_pos++;
		if (! utils_isbrace(sci_get_char_at(editor->sci, brace_pos), editor_prefs.brace_match_ltgt))
		{
			SSM(editor->sci, SCI_SETHIGHLIGHTGUIDE, 0, 0);
			SSM(editor->sci, SCI_BRACEBADLIGHT, (uptr_t)-1, 0);
			return;
		}
	}
	end_pos = SSM(editor->sci, SCI_BRACEMATCH, brace_pos, 0);

	if (end_pos >= 0)
	{
		gint col = MIN(sci_get_col_from_position(editor->sci, brace_pos),
			sci_get_col_from_position(editor->sci, end_pos));
		SSM(editor->sci, SCI_SETHIGHLIGHTGUIDE, col, 0);
		SSM(editor->sci, SCI_BRACEHIGHLIGHT, brace_pos, end_pos);
	}
	else
	{
		SSM(editor->sci, SCI_SETHIGHLIGHTGUIDE, 0, 0);
		SSM(editor->sci, SCI_BRACEBADLIGHT, brace_pos, 0);
	}
}


static gboolean in_block_comment(gint lexer, gint style)
{
	switch (lexer)
	{
		case SCLEX_CPP:
			return (style == SCE_C_COMMENT ||
				style == SCE_C_COMMENTDOC);

		case SCLEX_D:
			return (style == SCE_D_COMMENT ||
				style == SCE_D_COMMENTDOC ||
				style == SCE_D_COMMENTNESTED);

		case SCLEX_HTML:
			return (style == SCE_HPHP_COMMENT);

		case SCLEX_CSS:
			return (style == SCE_CSS_COMMENT);

		default:
			return FALSE;
	}
}


static gboolean is_comment_char(gchar c, gint lexer)
{
	if ((c == '*' || c == '+') && lexer == SCLEX_D)
		return TRUE;
	else
	if (c == '*')
		return TRUE;

	return FALSE;
}


static void auto_multiline(GeanyEditor *editor, gint cur_line)
{
	ScintillaObject *sci = editor->sci;
	gint indent_pos, style;
	gint lexer = sci_get_lexer(sci);

	/* Use the start of the line enter was pressed on, to avoid any doc keyword styles */
	indent_pos = sci_get_line_indent_position(sci, cur_line - 1);
	style = sci_get_style_at(sci, indent_pos);

	if (in_block_comment(lexer, style))
	{
		gchar *previous_line = sci_get_line(sci, cur_line - 1);
		/* the type of comment, '*' (C/C++/Java), '+' and the others (D) */
		gchar *continuation = "*";
		gchar *whitespace = ""; /* to hold whitespace if needed */
		gchar *result;
		gint len = strlen(previous_line);
		gint i;

		/* find and stop at end of multi line comment */
		i = len - 1;
		while (i >= 0 && isspace(previous_line[i])) i--;
		if (i >= 1 && is_comment_char(previous_line[i - 1], lexer) && previous_line[i] == '/')
		{
			gint indent_len, indent_width;

			indent_pos = sci_get_line_indent_position(sci, cur_line);
			indent_len = sci_get_col_from_position(sci, indent_pos);
			indent_width = editor_get_indent_prefs(editor)->width;

			/* if there is one too many spaces, delete the last space,
			 * to return to the indent used before the multiline comment was started. */
			if (indent_len % indent_width == 1)
				SSM(sci, SCI_DELETEBACKNOTLINE, 0, 0);	/* remove whitespace indent */
			g_free(previous_line);
			return;
		}
		/* check whether we are on the second line of multi line comment */
		i = 0;
		while (i < len && isspace(previous_line[i])) i++; /* get to start of the line */

		if (i + 1 < len &&
			previous_line[i] == '/' && is_comment_char(previous_line[i + 1], lexer))
		{ /* we are on the second line of a multi line comment, so we have to insert white space */
			whitespace = " ";
		}

		if (style == SCE_D_COMMENTNESTED)
			continuation = "+"; /* for nested comments in D */

		result = g_strconcat(whitespace, continuation, " ", NULL);
		sci_add_text(sci, result);
		g_free(result);

		g_free(previous_line);
	}
}


/* Checks whether the given style is a string for the given lexer.
 * It doesn't handle LEX_HTML, this should be done by the caller.
 * Returns true if the style is a string, FALSE otherwise.
 */
static gboolean is_string_style(gint lexer, gint style)
{
	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_PASCAL:
			return (style == SCE_C_CHARACTER ||
				style == SCE_C_STRING);

		case SCLEX_D:
			return (style == SCE_D_CHARACTER ||
				style == SCE_D_STRING);

		case SCLEX_PYTHON:
			return (style == SCE_P_STRING ||
				style == SCE_P_TRIPLE ||
				style == SCE_P_TRIPLEDOUBLE ||
				style == SCE_P_CHARACTER);

		case SCLEX_F77:
		case SCLEX_FORTRAN:
			return (style == SCE_F_STRING1 ||
				style == SCE_F_STRING2);

		case SCLEX_PERL:
			return (style == SCE_PL_STRING ||
				style == SCE_PL_HERE_DELIM ||
				style == SCE_PL_HERE_Q ||
				style == SCE_PL_HERE_QQ ||
				style == SCE_PL_HERE_QX ||
				style == SCE_PL_POD ||
				style == SCE_PL_POD_VERB);

		case SCLEX_RUBY:
			return (style == SCE_RB_CHARACTER ||
				style == SCE_RB_STRING ||
				style == SCE_RB_HERE_DELIM ||
				style == SCE_RB_HERE_Q ||
				style == SCE_RB_HERE_QQ ||
				style == SCE_RB_HERE_QX ||
				style == SCE_RB_POD);

		case SCLEX_BASH:
			return (style == SCE_SH_STRING);

		case SCLEX_SQL:
			return (style == SCE_SQL_STRING);

		case SCLEX_TCL:
			return (style == SCE_TCL_IN_QUOTE);

		case SCLEX_LUA:
			return (style == SCE_LUA_LITERALSTRING ||
				style == SCE_LUA_CHARACTER ||
				style == SCE_LUA_STRING);

		case SCLEX_HASKELL:
			return (style == SCE_HA_CHARACTER ||
				style == SCE_HA_STRING);

		case SCLEX_FREEBASIC:
			return (style == SCE_B_STRING);

		case SCLEX_HTML:
			return (style == SCE_HPHP_SIMPLESTRING ||
				style == SCE_HPHP_HSTRING ||  /* HSTRING is a heredoc */
				style == SCE_HPHP_HSTRING_VARIABLE ||
				style == SCE_H_DOUBLESTRING ||
				style == SCE_H_SINGLESTRING ||
				style == SCE_H_CDATA ||
				style == SCE_H_SGML_DOUBLESTRING ||
				style == SCE_H_SGML_SIMPLESTRING);
	}
	return FALSE;
}


/* Checks whether the given style is a comment for the given lexer.
 * It doesn't handle LEX_HTML, this should be done by the caller.
 * Returns true if the style is a comment, FALSE otherwise.
 */
static gboolean is_comment_style(gint lexer, gint style)
{
	switch (lexer)
	{
		case SCLEX_CPP:
		case SCLEX_PASCAL:
			return (style == SCE_C_COMMENT ||
				style == SCE_C_COMMENTLINE ||
				style == SCE_C_COMMENTDOC ||
				style == SCE_C_COMMENTLINEDOC ||
				style == SCE_C_COMMENTDOCKEYWORD ||
				style == SCE_C_COMMENTDOCKEYWORDERROR);

		case SCLEX_D:
			return (style == SCE_D_COMMENT ||
				style == SCE_D_COMMENTLINE ||
				style == SCE_D_COMMENTDOC ||
				style == SCE_D_COMMENTNESTED ||
				style == SCE_D_COMMENTLINEDOC ||
				style == SCE_D_COMMENTDOCKEYWORD ||
				style == SCE_D_COMMENTDOCKEYWORDERROR);

		case SCLEX_PYTHON:
			return (style == SCE_P_COMMENTLINE ||
				style == SCE_P_COMMENTBLOCK);

		case SCLEX_F77:
		case SCLEX_FORTRAN:
			return (style == SCE_F_COMMENT);

		case SCLEX_PERL:
			return (style == SCE_PL_COMMENTLINE);

		case SCLEX_PROPERTIES:
			return (style == SCE_PROPS_COMMENT);

		case SCLEX_LATEX:
			return (style == SCE_L_COMMENT);

		case SCLEX_MAKEFILE:
			return (style == SCE_MAKE_COMMENT);

		case SCLEX_RUBY:
			return (style == SCE_RB_COMMENTLINE);

		case SCLEX_BASH:
			return (style == SCE_SH_COMMENTLINE);

		case SCLEX_SQL:
			return (style == SCE_SQL_COMMENT ||
				style == SCE_SQL_COMMENTLINE ||
				style == SCE_SQL_COMMENTDOC);

		case SCLEX_TCL:
			return (style == SCE_TCL_COMMENT ||
				style == SCE_TCL_COMMENTLINE ||
				style == SCE_TCL_COMMENT_BOX ||
				style == SCE_TCL_BLOCK_COMMENT);

		case SCLEX_LUA:
			return (style == SCE_LUA_COMMENT ||
				style == SCE_LUA_COMMENTLINE ||
				style == SCE_LUA_COMMENTDOC);

		case SCLEX_HASKELL:
			return (style == SCE_HA_COMMENTLINE ||
				style == SCE_HA_COMMENTBLOCK ||
				style == SCE_HA_COMMENTBLOCK2 ||
				style == SCE_HA_COMMENTBLOCK3);

		case SCLEX_FREEBASIC:
			return (style == SCE_B_COMMENT);

		case SCLEX_HTML:
			return (style == SCE_HPHP_COMMENTLINE ||
				style == SCE_HPHP_COMMENT ||
				style == SCE_H_COMMENT ||
				style == SCE_H_SGML_COMMENT);
	}
	return FALSE;
}


/* Checks whether the given style is normal code (not string, comment, preprocessor, etc).
 * It doesn't handle LEX_HTML, this should be done by the caller.
 */
static gboolean is_code_style(gint lexer, gint style)
{
	switch (lexer)
	{
		case SCLEX_CPP:
			if (style == SCE_C_PREPROCESSOR)
				return FALSE;
			break;
	}
	return !(is_comment_style(lexer, style) ||
		is_string_style(lexer, style));
}


#if 0
gboolean editor_lexer_is_c_like(gint lexer)
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


/* Returns: -1 if lexer doesn't support type keywords */
gint editor_lexer_get_type_keyword_idx(gint lexer)
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


/* inserts a three-line comment at one line above current cursor position */
void editor_insert_multiline_comment(GeanyEditor *editor)
{
	gchar *text;
	gint text_len;
	gint line;
	gint pos;
	gboolean have_multiline_comment = FALSE;
	GeanyDocument *doc;

	if (editor == NULL || editor->document->file_type == NULL ||
		editor->document->file_type->comment_open == NULL)
	{
		return;
	}
	doc = editor->document;

	if (doc->file_type->comment_close != NULL && strlen(doc->file_type->comment_close) > 0)
		have_multiline_comment = TRUE;

	/* insert three lines one line above of the current position */
	line = sci_get_line_from_position(editor->sci, editor_info.click_pos);
	pos = sci_get_position_from_line(editor->sci, line);

	/* use the indent on the current line but only when comment indentation is used
	 * and we don't have multi line comment characters */
	if (editor->auto_indent &&
		! have_multiline_comment &&	doc->file_type->comment_use_indent)
	{
		read_indent(editor, editor_info.click_pos);
		text = g_strdup_printf("%s\n%s\n%s\n", indent, indent, indent);
		text_len = strlen(text);
	}
	else
	{
		text = g_strdup("\n\n\n");
		text_len = 3;
	}
	sci_insert_text(editor->sci, pos, text);
	g_free(text);


	/* select the inserted lines for commenting */
	sci_set_selection_start(editor->sci, pos);
	sci_set_selection_end(editor->sci, pos + text_len);

	editor_do_comment(editor, -1, TRUE, FALSE);

	/* set the current position to the start of the first inserted line */
	pos += strlen(doc->file_type->comment_open);

	/* on multi line comment jump to the next line, otherwise add the length of added indentation */
	if (have_multiline_comment)
		pos += 1;
	else
		pos += strlen(indent);

	sci_set_current_position(editor->sci, pos, TRUE);
	/* reset the selection */
	sci_set_anchor(editor->sci, pos);
}


/* Note: If the editor is pending a redraw, set document::scroll_percent instead.
 * Scroll the view to make line appear at percent_of_view.
 * line can be -1 to use the current position. */
void editor_scroll_to_line(GeanyEditor *editor, gint line, gfloat percent_of_view)
{
	gint vis1, los, delta;
	GtkWidget *wid;

	if (editor == NULL)
		return;

	wid = GTK_WIDGET(editor->sci);

	if (! wid->window || ! gdk_window_is_viewable(wid->window))
		return;	/* prevent gdk_window_scroll warning */

	if (line == -1)
		line = sci_get_current_line(editor->sci);

	/* sci 'visible line' != doc line number because of folding and line wrapping */
	/* calling SCI_VISIBLEFROMDOCLINE for line is more accurate than calling
	 * SCI_DOCLINEFROMVISIBLE for vis1. */
	line = SSM(editor->sci, SCI_VISIBLEFROMDOCLINE, line, 0);
	vis1 = SSM(editor->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	los = SSM(editor->sci, SCI_LINESONSCREEN, 0, 0);
	delta = (line - vis1) - los * percent_of_view;
	sci_scroll_lines(editor->sci, delta);
	sci_scroll_caret(editor->sci); /* needed for horizontal scrolling */
}


/* creates and inserts one tab or whitespace of the amount of the tab width */
void editor_insert_alternative_whitespace(GeanyEditor *editor)
{
	gchar *text;
	GeanyIndentPrefs iprefs = *editor_get_indent_prefs(editor);

	switch (iprefs.type)
	{
		case GEANY_INDENT_TYPE_TABS:
			iprefs.type = GEANY_INDENT_TYPE_SPACES;
			break;
		case GEANY_INDENT_TYPE_SPACES:
		case GEANY_INDENT_TYPE_BOTH:	/* most likely we want a tab */
			iprefs.type = GEANY_INDENT_TYPE_TABS;
			break;
	}
	text = get_whitespace(&iprefs, iprefs.width);
	sci_add_text(editor->sci, text);
	g_free(text);
}


void editor_select_word(GeanyEditor *editor)
{
	gint pos;
	gint start;
	gint end;

	g_return_if_fail(editor != NULL);

	pos = SSM(editor->sci, SCI_GETCURRENTPOS, 0, 0);
	start = SSM(editor->sci, SCI_WORDSTARTPOSITION, pos, TRUE);
	end = SSM(editor->sci, SCI_WORDENDPOSITION, pos, TRUE);

	if (start == end) /* caret in whitespaces sequence */
	{
		/* look forward but reverse the selection direction,
		 * so the caret end up stay as near as the original position. */
		end = SSM(editor->sci, SCI_WORDENDPOSITION, pos, FALSE);
		start = SSM(editor->sci, SCI_WORDENDPOSITION, end, TRUE);
		if (start == end)
			return;
	}

	SSM(editor->sci, SCI_SETSEL, start, end);
}


/* extra_line is for selecting the cursor line or anchor line at the bottom of a selection,
 * when those lines have no selection. */
void editor_select_lines(GeanyEditor *editor, gboolean extra_line)
{
	gint start, end, line;

	g_return_if_fail(editor != NULL);

	start = sci_get_selection_start(editor->sci);
	end = sci_get_selection_end(editor->sci);

	/* check if whole lines are already selected */
	if (! extra_line && start != end &&
		sci_get_col_from_position(editor->sci, start) == 0 &&
		sci_get_col_from_position(editor->sci, end) == 0)
			return;

	line = sci_get_line_from_position(editor->sci, start);
	start = sci_get_position_from_line(editor->sci, line);

	line = sci_get_line_from_position(editor->sci, end);
	end = sci_get_position_from_line(editor->sci, line + 1);

	SSM(editor->sci, SCI_SETSEL, start, end);
}


/* find the start or end of a paragraph by searching all lines in direction (UP or DOWN)
 * starting at the given line and return the found line or return -1 if called on an empty line */
static gint find_paragraph_stop(GeanyEditor *editor, gint line, gint direction)
{
	gboolean found_end = FALSE;
	gint step;
	gchar *line_buf, *x;
	ScintillaObject *sci = editor->sci;

	/* first check current line and return -1 if it is empty to skip creating of a selection */
	line_buf = x = sci_get_line(sci, line);
	while (isspace(*x))
		x++;
	if (*x == '\0')
	{
		g_free(line_buf);
		return -1;
	}

	if (direction == UP)
		step = -1;
	else
		step = 1;

	while (! found_end)
	{
		line += step;

		/* sci_get_line checks for sanity of the given line, sci_get_line always return a string
		 * containing at least '\0' so no need to check for NULL */
		line_buf = x = sci_get_line(sci, line);

		/* check whether after skipping all whitespace we are at end of line and if so, assume
		 * this line as end of paragraph */
		while (isspace(*x))
			x++;
		if (*x == '\0')
		{
			found_end = TRUE;
			if (line == -1)
				/* called on the first line but there is no previous line so return line 0 */
				line = 0;
		}
		g_free(line_buf);
	}
	return line;
}


void editor_select_paragraph(GeanyEditor *editor)
{
	gint pos_start, pos_end, line_start, line_found;

	g_return_if_fail(editor != NULL);

	line_start = SSM(editor->sci, SCI_LINEFROMPOSITION,
						SSM(editor->sci, SCI_GETCURRENTPOS, 0, 0), 0);

	line_found = find_paragraph_stop(editor, line_start, UP);
	if (line_found == -1)
		return;

	/* find_paragraph_stop returns the emtpy line(previous to the real start of the paragraph),
	 * so use the next line for selection start */
	if (line_found > 0)
		line_found++;

	pos_start = SSM(editor->sci, SCI_POSITIONFROMLINE, line_found, 0);

	line_found = find_paragraph_stop(editor, line_start, DOWN);
	pos_end = SSM(editor->sci, SCI_POSITIONFROMLINE, line_found, 0);

	SSM(editor->sci, SCI_SETSEL, pos_start, pos_end);
}


/* simple indentation to indent the current line with the same indent as the previous one */
static void smart_line_indentation(GeanyEditor *editor, gint first_line, gint last_line)
{
	gint i, sel_start = 0, sel_end = 0;

	/* get previous line and use it for read_indent to use that line
	 * (otherwise it would fail on a line only containing "{" in advanced indentation mode) */
	read_indent(editor, sci_get_position_from_line(editor->sci, first_line - 1));

	for (i = first_line; i <= last_line; i++)
	{
		/* skip the first line or if the indentation of the previous and current line are equal */
		if (i == 0 ||
			SSM(editor->sci, SCI_GETLINEINDENTATION, i - 1, 0) ==
			SSM(editor->sci, SCI_GETLINEINDENTATION, i, 0))
			continue;

		sel_start = SSM(editor->sci, SCI_POSITIONFROMLINE, i, 0);
		sel_end = SSM(editor->sci, SCI_GETLINEINDENTPOSITION, i, 0);
		if (sel_start < sel_end)
		{
			SSM(editor->sci, SCI_SETSEL, sel_start, sel_end);
			sci_replace_sel(editor->sci, "");
		}
		sci_insert_text(editor->sci, sel_start, indent);
	}
}


/* simple indentation to indent the current line with the same indent as the previous one */
void editor_smart_line_indentation(GeanyEditor *editor, gint pos)
{
	gint first_line, last_line;
	gint first_sel_start, first_sel_end;
	ScintillaObject *sci;

	g_return_if_fail(editor != NULL);

	sci = editor->sci;

	first_sel_start = sci_get_selection_start(sci);
	first_sel_end = sci_get_selection_end(sci);

	first_line = sci_get_line_from_position(sci, first_sel_start);
	/* Find the last line with chars selected (not EOL char) */
	last_line = sci_get_line_from_position(sci, first_sel_end - editor_get_eol_char_len(editor));
	last_line = MAX(first_line, last_line);

	if (pos == -1)
		pos = first_sel_start;

	SSM(sci, SCI_BEGINUNDOACTION, 0, 0);

	smart_line_indentation(editor, first_line, last_line);

	/* set cursor position if there was no selection */
	if (first_sel_start == first_sel_end)
	{
		gint indent_pos = SSM(sci, SCI_GETLINEINDENTPOSITION, first_line, 0);

		/* use indent position as user may wish to change indentation afterwards */
		sci_set_current_position(sci, indent_pos, FALSE);
	}
	else
	{
		/* fully select all the lines affected */
		sci_set_selection_start(sci, sci_get_position_from_line(sci, first_line));
		sci_set_selection_end(sci, sci_get_position_from_line(sci, last_line + 1));
	}

	SSM(sci, SCI_ENDUNDOACTION, 0, 0);
}


/* increase / decrease current line or selection by one space */
void editor_indentation_by_one_space(GeanyEditor *editor, gint pos, gboolean decrease)
{
	gint i, first_line, last_line, line_start, indentation_end, count = 0;
	gint sel_start, sel_end, first_line_offset = 0;

	g_return_if_fail(editor != NULL);

	sel_start = sci_get_selection_start(editor->sci);
	sel_end = sci_get_selection_end(editor->sci);

	first_line = sci_get_line_from_position(editor->sci, sel_start);
	/* Find the last line with chars selected (not EOL char) */
	last_line = sci_get_line_from_position(editor->sci, sel_end - editor_get_eol_char_len(editor));
	last_line = MAX(first_line, last_line);

	if (pos == -1)
		pos = sel_start;

	SSM(editor->sci, SCI_BEGINUNDOACTION, 0, 0);

	for (i = first_line; i <= last_line; i++)
	{
		indentation_end = SSM(editor->sci, SCI_GETLINEINDENTPOSITION, i, 0);
		if (decrease)
		{
			line_start = SSM(editor->sci, SCI_POSITIONFROMLINE, i, 0);
			/* searching backwards for a space to remove */
			while (sci_get_char_at(editor->sci, indentation_end) != ' ' && indentation_end > line_start)
				indentation_end--;

			if (sci_get_char_at(editor->sci, indentation_end) == ' ')
			{
				SSM(editor->sci, SCI_SETSEL, indentation_end, indentation_end + 1);
				sci_replace_sel(editor->sci, "");
				count--;
				if (i == first_line)
					first_line_offset = -1;
			}
		}
		else
		{
			sci_insert_text(editor->sci, indentation_end, " ");
			count++;
			if (i == first_line)
				first_line_offset = 1;
		}
	}

	/* set cursor position */
	if (sel_start < sel_end)
	{
		gint start = sel_start + first_line_offset;
		if (first_line_offset < 0)
			start = MAX(sel_start + first_line_offset,
						SSM(editor->sci, SCI_POSITIONFROMLINE, first_line, 0));

		sci_set_selection_start(editor->sci, start);
		sci_set_selection_end(editor->sci, sel_end + count);
	}
	else
		sci_set_current_position(editor->sci, pos + count, FALSE);

	SSM(editor->sci, SCI_ENDUNDOACTION, 0, 0);
}


void editor_finalize()
{
	scintilla_release_resources();
}


/* wordchars: NULL or a string containing characters to match a word.
 * Returns: the current selection or the current word. */
gchar *editor_get_default_selection(GeanyEditor *editor, gboolean use_current_word,
									const gchar *wordchars)
{
	gchar *s = NULL;

	if (editor == NULL)
		return NULL;

	if (sci_get_lines_selected(editor->sci) == 1)
	{
		gint len = sci_get_selected_text_length(editor->sci);

		s = g_malloc(len + 1);
		sci_get_selected_text(editor->sci, s);
	}
	else if (sci_get_lines_selected(editor->sci) == 0 && use_current_word)
	{	/* use the word at current cursor position */
		gchar word[GEANY_MAX_WORD_LENGTH];

		editor_find_current_word(editor, -1, word, sizeof(word), wordchars);
		if (word[0] != '\0')
			s = g_strdup(word);
	}
	return s;
}


/* Note: Usually the line should be made visible (not folded) before calling this.
 * Returns: TRUE if line is/will be displayed to the user, or FALSE if it is
 * outside the view. */
gboolean editor_line_in_view(GeanyEditor *editor, gint line)
{
	gint vis1, los;

	if (editor == NULL)
		return FALSE;

	/* If line is wrapped the result may occur on another virtual line than the first and may be
	 * still hidden, so increase the line number to check for the next document line */
	if (SSM(editor->sci, SCI_WRAPCOUNT, line, 0) > 1)
		line++;

	line = SSM(editor->sci, SCI_VISIBLEFROMDOCLINE, line, 0);	/* convert to visible line number */
	vis1 = SSM(editor->sci, SCI_GETFIRSTVISIBLELINE, 0, 0);
	los = SSM(editor->sci, SCI_LINESONSCREEN, 0, 0);

	return (line >= vis1 && line < vis1 + los);
}


/* If the current line is outside the current view window, scroll the line
 * so it appears at percent_of_view. */
void editor_display_current_line(GeanyEditor *editor, gfloat percent_of_view)
{
	gint line;

	if (editor == NULL)
		return;

	line = sci_get_current_line(editor->sci);

	/* unfold maybe folded results */
	sci_ensure_line_is_visible(editor->sci, line);

	/* scroll the line if it's off screen */
	if (! editor_line_in_view(editor, line))
		editor->scroll_percent = percent_of_view;
}


/**
 *  Deletes all currently set indicators in the @a editor window.
 *  Error indicators (red squiggly underlines) and usual line markers are removed.
 *
 *  @param editor The editor to operate on.
 **/
void editor_clear_indicators(GeanyEditor *editor)
{
	glong last_pos;

	g_return_if_fail(editor != NULL);

	last_pos = sci_get_length(editor->sci);
	if (last_pos > 0)
		sci_indicator_clear(editor->sci, 0, last_pos);

	sci_marker_delete_all(editor->sci, 0);	/* remove the yellow error line marker */
}


/**
 *  This is a convenience function for editor_set_indicator(). It sets an error indicator
 *  (red squiggly underline) on the whole given line.
 *  Whitespace at the start and the end of the line is not marked.
 *
 *  @param editor The editor to operate on.
 *  @param line The line number which should be marked.
 **/
void editor_set_indicator_on_line(GeanyEditor *editor, gint line)
{
	gint start, end;
	guint i = 0, len;
	gchar *linebuf;

	if (editor == NULL)
		return;

	start = sci_get_position_from_line(editor->sci, line);
	end = sci_get_position_from_line(editor->sci, line + 1);

	/* skip blank lines */
	if ((start + 1) == end ||
		sci_get_line_length(editor->sci, line) == editor_get_eol_char_len(editor))
		return;

	/* don't set the indicator on whitespace */
	len = end - start;
	linebuf = sci_get_line(editor->sci, line);

	while (isspace(linebuf[i])) i++;
	while (len > 1 && len > i && isspace(linebuf[len-1]))
	{
		len--;
		end--;
	}
	g_free(linebuf);

	editor_set_indicator(editor, start + i, end);
}


/**
 *  Sets an error indicator (red squiggly underline) on the range specified by @c start and @c end.
 *  No error checking or whitespace removal is performed, this should be done by the calling
 *  function if necessary.
 *
 *  @param editor The editor to operate on.
 *  @param start The starting position for the marker.
 *  @param end The ending position for the marker.
 **/
void editor_set_indicator(GeanyEditor *editor, gint start, gint end)
{
	if (editor == NULL || start >= end)
		return;

	sci_set_indicator(editor->sci, 0);
	sci_indicator_fill(editor->sci, start, end - start);
}


/* Inserts the given colour (format should be #...), if there is a selection starting with 0x...
 * the replacement will also start with 0x... */
void editor_insert_color(GeanyEditor *editor, const gchar *colour)
{
	g_return_if_fail(editor != NULL);

	if (sci_has_selection(editor->sci))
	{
		gint start = sci_get_selection_start(editor->sci);
		const gchar *replacement = colour;

		if (sci_get_char_at(editor->sci, start) == '0' &&
			sci_get_char_at(editor->sci, start + 1) == 'x')
		{
			sci_set_selection_start(editor->sci, start + 2);
			sci_set_selection_end(editor->sci, start + 8);
			replacement++; /* skip the leading "0x" */
		}
		else if (sci_get_char_at(editor->sci, start - 1) == '#')
		{	/* double clicking something like #00ffff may only select 00ffff because of wordchars */
			replacement++; /* so skip the '#' to only replace the colour value */
		}
		sci_replace_sel(editor->sci, replacement);
	}
	else
		sci_add_text(editor->sci, colour);
}


const gchar *editor_get_eol_char_name(GeanyEditor *editor)
{
	gint mode = file_prefs.default_eol_character;

	if (editor != NULL)
		mode = sci_get_eol_mode(editor->sci);

	switch (mode)
	{
		case SC_EOL_CRLF: return _("Win (CRLF)"); break;
		case SC_EOL_CR: return _("Mac (CR)"); break;
		default: return _("Unix (LF)"); break;
	}
}


/* returns the end-of-line character(s) length of the specified editor */
gint editor_get_eol_char_len(GeanyEditor *editor)
{
	gint mode = file_prefs.default_eol_character;

	if (editor != NULL)
		mode = sci_get_eol_mode(editor->sci);

	switch (mode)
	{
		case SC_EOL_CRLF: return 2; break;
		default: return 1; break;
	}
}


/* returns the end-of-line character(s) of the specified editor */
const gchar *editor_get_eol_char(GeanyEditor *editor)
{
	gint mode = file_prefs.default_eol_character;

	if (editor != NULL)
		mode = sci_get_eol_mode(editor->sci);

	switch (mode)
	{
		case SC_EOL_CRLF: return "\r\n"; break;
		case SC_EOL_CR: return "\r"; break;
		default: return "\n"; break;
	}
}


static void fold_all(GeanyEditor *editor, gboolean want_fold)
{
	gint lines, first, i;

	if (editor == NULL || ! editor_prefs.folding)
		return;

	lines = sci_get_line_count(editor->sci);
	first = sci_get_first_visible_line(editor->sci);

	for (i = 0; i < lines; i++)
	{
		gint level = sci_get_fold_level(editor->sci, i);
		if (level & SC_FOLDLEVELHEADERFLAG)
		{
			if (sci_get_fold_expanded(editor->sci, i) == want_fold)
					sci_toggle_fold(editor->sci, i);
		}
	}
	editor_scroll_to_line(editor, first, 0.0F);
}


void editor_unfold_all(GeanyEditor *editor)
{
	fold_all(editor, FALSE);
}


void editor_fold_all(GeanyEditor *editor)
{
	fold_all(editor, TRUE);
}


void editor_replace_tabs(GeanyEditor *editor)
{
	gint search_pos, pos_in_line, current_tab_true_length;
	gint tab_len;
	gchar *tab_str;
	struct TextToFind ttf;

	if (editor == NULL)
		return;

	sci_start_undo_action(editor->sci);
	tab_len = sci_get_tab_width(editor->sci);
	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(editor->sci);
	ttf.lpstrText = (gchar*) "\t";

	while (TRUE)
	{
		search_pos = sci_find_text(editor->sci, SCFIND_MATCHCASE, &ttf);
		if (search_pos == -1)
			break;

		pos_in_line = sci_get_col_from_position(editor->sci,search_pos);
		current_tab_true_length = tab_len - (pos_in_line % tab_len);
		tab_str = g_strnfill(current_tab_true_length, ' ');
		sci_target_start(editor->sci, search_pos);
		sci_target_end(editor->sci, search_pos + 1);
		sci_target_replace(editor->sci, tab_str, FALSE);
		/* next search starts after replacement */
		ttf.chrg.cpMin = search_pos + current_tab_true_length - 1;
		/* update end of range now text has changed */
		ttf.chrg.cpMax += current_tab_true_length - 1;
		g_free(tab_str);
	}
	sci_end_undo_action(editor->sci);
}


/* Replaces all occurrences all spaces of the length of a given tab_width. */
void editor_replace_spaces(GeanyEditor *editor)
{
	gint search_pos;
	static gdouble tab_len_f = -1.0; /* keep the last used value */
	gint tab_len;
	struct TextToFind ttf;

	if (editor == NULL)
		return;

	if (tab_len_f < 0.0)
		tab_len_f = sci_get_tab_width(editor->sci);

	if (! dialogs_show_input_numeric(
		_("Enter Tab Width"),
		_("Enter the amount of spaces which should be replaced by a tab character."),
		&tab_len_f, 1, 100, 1))
	{
		return;
	}
	tab_len = (gint) tab_len_f;

	sci_start_undo_action(editor->sci);
	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(editor->sci);
	ttf.lpstrText = g_strnfill(tab_len, ' ');

	while (TRUE)
	{
		search_pos = sci_find_text(editor->sci, SCFIND_MATCHCASE, &ttf);
		if (search_pos == -1)
			break;

		sci_target_start(editor->sci, search_pos);
		sci_target_end(editor->sci, search_pos + tab_len);
		sci_target_replace(editor->sci, "\t", FALSE);
		ttf.chrg.cpMin = search_pos;
		/* update end of range now text has changed */
		ttf.chrg.cpMax -= tab_len - 1;
	}
	sci_end_undo_action(editor->sci);
	g_free(ttf.lpstrText);
}


void editor_strip_line_trailing_spaces(GeanyEditor *editor, gint line)
{
	gint line_start = sci_get_position_from_line(editor->sci, line);
	gint line_end = sci_get_line_end_position(editor->sci, line);
	gint i = line_end - 1;
	gchar ch = sci_get_char_at(editor->sci, i);

	while ((i >= line_start) && ((ch == ' ') || (ch == '\t')))
	{
		i--;
		ch = sci_get_char_at(editor->sci, i);
	}
	if (i < (line_end-1))
	{
		sci_target_start(editor->sci, i + 1);
		sci_target_end(editor->sci, line_end);
		sci_target_replace(editor->sci, "", FALSE);
	}
}


void editor_strip_trailing_spaces(GeanyEditor *editor)
{
	gint max_lines = sci_get_line_count(editor->sci);
	gint line;

	sci_start_undo_action(editor->sci);

	for (line = 0; line < max_lines; line++)
	{
		editor_strip_line_trailing_spaces(editor, line);
	}
	sci_end_undo_action(editor->sci);
}


void editor_ensure_final_newline(GeanyEditor *editor)
{
	gint max_lines = sci_get_line_count(editor->sci);
	gboolean append_newline = (max_lines == 1);
	gint end_document = sci_get_position_from_line(editor->sci, max_lines);

	if (max_lines > 1)
	{
		append_newline = end_document > sci_get_position_from_line(editor->sci, max_lines - 1);
	}
	if (append_newline)
	{
		const gchar *eol = "\n";
		switch (sci_get_eol_mode(editor->sci))
		{
			case SC_EOL_CRLF:
				eol = "\r\n";
				break;
			case SC_EOL_CR:
				eol = "\r";
				break;
		}
		sci_insert_text(editor->sci, end_document, eol);
	}
}


void editor_set_font(GeanyEditor *editor, const gchar *font)
{
	gint style, size;
	gchar *font_name;
	PangoFontDescription *pfd;

	g_return_if_fail(editor);

	pfd = pango_font_description_from_string(font);
	size = pango_font_description_get_size(pfd) / PANGO_SCALE;
	font_name = g_strdup_printf("!%s", pango_font_description_get_family(pfd));
	pango_font_description_free(pfd);

	for (style = 0; style <= 127; style++)
		sci_set_font(editor->sci, style, font_name, size);

	/* line number and braces */
	sci_set_font(editor->sci, STYLE_LINENUMBER, font_name, size);
	sci_set_font(editor->sci, STYLE_BRACELIGHT, font_name, size);
	sci_set_font(editor->sci, STYLE_BRACEBAD, font_name, size);
	g_free(font_name);

	/* zoom to 100% to prevent confusion */
	sci_zoom_off(editor->sci);
}


void editor_set_line_wrapping(GeanyEditor *editor, gboolean wrap)
{
	g_return_if_fail(editor != NULL);

	editor->line_wrapping = wrap;
	sci_set_lines_wrapped(editor->sci, wrap);
}


/* Also sets indent width, tab width. */
void editor_set_indent_type(GeanyEditor *editor, GeanyIndentType type)
{
	const GeanyIndentPrefs *iprefs = editor_get_indent_prefs(editor);
	ScintillaObject *sci = editor->sci;
	gboolean use_tabs = type != GEANY_INDENT_TYPE_SPACES;

	editor->indent_type = type;
	sci_set_use_tabs(sci, use_tabs);

	if (type == GEANY_INDENT_TYPE_BOTH)
		sci_set_tab_width(sci, iprefs->hard_tab_width);
	else
		sci_set_tab_width(sci, iprefs->width);
	SSM(sci, SCI_SETINDENT, iprefs->width, 0);

	/* remove indent spaces on backspace, if using any spaces to indent */
	SSM(sci, SCI_SETBACKSPACEUNINDENTS, type != GEANY_INDENT_TYPE_TABS, 0);
}


/* Move to position @a pos, switching to the document if necessary,
 * setting a marker if @a mark is set. */
gboolean editor_goto_pos(GeanyEditor *editor, gint pos, gboolean mark)
{
	gint page_num;

	g_return_val_if_fail(editor, FALSE);
	if (pos < 0)
		return FALSE;

	if (mark)
	{
		gint line = sci_get_line_from_position(editor->sci, pos);

		/* mark the tag with the yellow arrow */
		sci_marker_delete_all(editor->sci, 0);
		sci_set_marker_at_line(editor->sci, line, TRUE, 0);
	}

	sci_goto_pos(editor->sci, pos, TRUE);
	editor->scroll_percent = 0.25F;

	/* finally switch to the page */
	page_num = gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook), GTK_WIDGET(editor->sci));
	gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), page_num);

	return TRUE;
}


static gboolean
on_editor_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
	/* Handle scroll events if Shift or Alt is pressed and scroll whole pages instead of a
	 * few lines only, maybe this could/should be done in Scintilla directly */
	if (event->state & GDK_MOD1_MASK)
	{
		GeanyEditor *editor = user_data;
		sci_cmd(editor->sci, (event->direction == GDK_SCROLL_DOWN) ? SCI_PAGEDOWN : SCI_PAGEUP);
		return TRUE;
	}

	return FALSE; /* let Scintilla handle all other cases */
}


static void editor_colourise(ScintillaObject *sci)
{
	sci_colourise(sci, 0, -1);

	/* now that the current document is colourised, fold points are now accurate,
	 * so force an update of the current function/tag. */
	symbols_get_current_function(NULL, NULL);
	ui_update_statusbar(NULL, -1);
}


static gboolean on_editor_expose_event(GtkWidget *widget, GdkEventExpose *event,
		gpointer user_data)
{
	GeanyDocument *doc = user_data;

	if (DOCUMENT(doc)->colourise_needed)
	{
		editor_colourise(doc->editor->sci);
		DOCUMENT(doc)->colourise_needed = FALSE;
	}
	return FALSE;	/* propagate event */
}


static void setup_sci_keys(ScintillaObject *sci)
{
	/* disable some Scintilla keybindings to be able to redefine them cleanly */
	sci_clear_cmdkey(sci, 'A' | (SCMOD_CTRL << 16)); /* select all */
	sci_clear_cmdkey(sci, 'D' | (SCMOD_CTRL << 16)); /* duplicate */
	sci_clear_cmdkey(sci, 'T' | (SCMOD_CTRL << 16)); /* line transpose */
	sci_clear_cmdkey(sci, 'T' | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16)); /* line copy */
	sci_clear_cmdkey(sci, 'L' | (SCMOD_CTRL << 16)); /* line cut */
	sci_clear_cmdkey(sci, 'L' | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16)); /* line delete */
	sci_clear_cmdkey(sci, SCK_UP | (SCMOD_CTRL << 16)); /* scroll line up */
	sci_clear_cmdkey(sci, SCK_DOWN | (SCMOD_CTRL << 16)); /* scroll line down */
	sci_clear_cmdkey(sci, SCK_HOME);	/* line start */
	sci_clear_cmdkey(sci, SCK_END);	/* line end */

	if (editor_prefs.use_gtk_word_boundaries)
	{
		/* use GtkEntry-like word boundaries */
		sci_assign_cmdkey(sci, SCK_RIGHT | (SCMOD_CTRL << 16), SCI_WORDRIGHTEND);
		sci_assign_cmdkey(sci, SCK_RIGHT | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16), SCI_WORDRIGHTENDEXTEND);
		sci_assign_cmdkey(sci, SCK_DELETE | (SCMOD_CTRL << 16), SCI_DELWORDRIGHTEND);
	}
	sci_assign_cmdkey(sci, SCK_UP | (SCMOD_ALT << 16), SCI_LINESCROLLUP);
	sci_assign_cmdkey(sci, SCK_DOWN | (SCMOD_ALT << 16), SCI_LINESCROLLDOWN);
	sci_assign_cmdkey(sci, SCK_UP | (SCMOD_CTRL << 16), SCI_PARAUP);
	sci_assign_cmdkey(sci, SCK_UP | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16), SCI_PARAUPEXTEND);
	sci_assign_cmdkey(sci, SCK_DOWN | (SCMOD_CTRL << 16), SCI_PARADOWN);
	sci_assign_cmdkey(sci, SCK_DOWN | (SCMOD_CTRL << 16) | (SCMOD_SHIFT << 16), SCI_PARADOWNEXTEND);

	sci_clear_cmdkey(sci, SCK_BACK | (SCMOD_ALT << 16)); /* clear Alt-Backspace (Undo) */
}


/* Create new editor widget (scintilla).
 * @note The @c "sci-notify" signal is connected separately. */
/* TODO: change to use GeanyEditor */
static ScintillaObject *create_new_sci(GeanyDocument *doc)
{
	ScintillaObject	*sci;

	sci = SCINTILLA(scintilla_new());
	scintilla_set_id(sci, doc->index);

	gtk_widget_show(GTK_WIDGET(sci));

	sci_set_codepage(sci, SC_CP_UTF8);
	/*SSM(sci, SCI_SETWRAPSTARTINDENT, 4, 0);*/
	/* disable scintilla provided popup menu */
	sci_use_popup(sci, FALSE);

	setup_sci_keys(sci);

	sci_set_symbol_margin(sci, editor_prefs.show_markers_margin);
	sci_set_lines_wrapped(sci, editor_prefs.line_wrapping);
	sci_set_scrollbar_mode(sci, editor_prefs.show_scrollbars);
	sci_set_caret_policy_x(sci, CARET_JUMPS | CARET_EVEN, 0);
	/*sci_set_caret_policy_y(sci, CARET_JUMPS | CARET_EVEN, 0);*/
	SSM(sci, SCI_AUTOCSETSEPARATOR, '\n', 0);
	SSM(sci, SCI_SETSCROLLWIDTHTRACKING, 1, 0);

	/* signal for insert-key(works without too, but to update the right status bar) */
	/*g_signal_connect(sci, "key-press-event",
					 G_CALLBACK(keybindings_got_event), GINT_TO_POINTER(new_idx));*/
	/* signal for the popup menu */
	g_signal_connect(sci, "button-press-event", G_CALLBACK(on_editor_button_press_event), doc);
	g_signal_connect(sci, "scroll-event", G_CALLBACK(on_editor_scroll_event), doc->editor);
	g_signal_connect(sci, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	g_signal_connect(sci, "expose-event", G_CALLBACK(on_editor_expose_event), doc);

	return sci;
}


/** Create a new Scintilla @c GtkWidget based on the settings for @a editor.
 * @param editor Editor settings.
 * @return The new widget. */
ScintillaObject *editor_create_widget(GeanyEditor *editor)
{
	const GeanyIndentPrefs *iprefs = get_default_indent_prefs();
	ScintillaObject *old, *sci;

	/* temporarily change editor to use the new sci widget */
	old = editor->sci;
	sci = create_new_sci(editor->document);
	editor->sci = sci;

	editor_set_indent_type(editor, iprefs->type);
	editor_set_font(editor, interface_prefs.editor_font);

	/* if editor already had a widget, restore it */
	if (old)
		editor->sci = old;
	return sci;
}


GeanyEditor *editor_create(GeanyDocument *doc)
{
	const GeanyIndentPrefs *iprefs = get_default_indent_prefs();
	GeanyEditor *editor = g_new0(GeanyEditor, 1);

	editor->document = doc;

	editor->auto_indent = (iprefs->auto_indent_mode != GEANY_AUTOINDENT_NONE);
	editor->line_wrapping = editor_prefs.line_wrapping;
	editor->scroll_percent = -1.0F;
	editor->line_breaking = FALSE;

	editor->sci = editor_create_widget(editor);
	return editor;
}


/* in case we need to free some fields in future */
void editor_destroy(GeanyEditor *editor)
{
	g_free(editor);
}


void editor_init(void)
{
	static GeanyIndentPrefs indent_prefs;

	memset(&editor_prefs, 0, sizeof(GeanyEditorPrefs));
	memset(&indent_prefs, 0, sizeof(GeanyIndentPrefs));
	editor_prefs.indentation = &indent_prefs;
}


/** TODO: Should these be user-defined instead of hard-coded? */
void editor_set_indentation_guides(GeanyEditor *editor)
{
	gint mode;
	gint lexer;

	g_return_if_fail(editor != NULL);

	if (! editor_prefs.show_indent_guide)
	{
		sci_set_indentation_guides(editor->sci, SC_IV_NONE);
		return;
	}

	lexer = sci_get_lexer(editor->sci);
	switch (lexer)
	{
		/* Lines added/removed are prefixed with +/- characters, so
		 * those lines will not be shown with any indentation guides.
		 * It can be distracting that only a few of lines in a diff/patch
		 * file will show the guides. */
		case SCLEX_DIFF:
			mode = SC_IV_NONE;

		/* These languages use indentation for control blocks; the "look forward" method works
		 * best here */
		case SCLEX_PYTHON:
		case SCLEX_HASKELL:
		case SCLEX_MAKEFILE:
		case SCLEX_ASM:
		case SCLEX_SQL:
		case SCLEX_PROPERTIES:
		case SCLEX_FORTRAN: /* Is this the best option for Fortran? */
		case SCLEX_CAML:
			mode = SC_IV_LOOKFORWARD;

		/* C-like (structured) languages benefit from the "look both" method */
		case SCLEX_CPP:
		case SCLEX_HTML:
		case SCLEX_XML:
		case SCLEX_PERL:
		case SCLEX_LATEX:
		case SCLEX_LUA:
		case SCLEX_PASCAL:
		case SCLEX_RUBY:
		case SCLEX_TCL:
		case SCLEX_F77:
		case SCLEX_CSS:
		case SCLEX_BASH:
		case SCLEX_VHDL:
		case SCLEX_FREEBASIC:
		case SCLEX_D:
		case SCLEX_OMS:
			mode = SC_IV_LOOKBOTH;
		default:
			mode = SC_IV_REAL;
	}

	sci_set_indentation_guides(editor->sci, mode);
}


/* Apply just the prefs that can change in the Preferences dialog */
void editor_apply_update_prefs(GeanyEditor *editor)
{
	ScintillaObject *sci;

	g_return_if_fail(editor != NULL);

	sci = editor->sci;

	sci_set_mark_long_lines(sci, editor_prefs.long_line_type,
		editor_prefs.long_line_column, editor_prefs.long_line_color);

	/* update indent width, tab width */
	editor_set_indent_type(editor, editor->indent_type);
	sci_set_tab_indents(sci, editor_prefs.use_tab_to_indent);

	sci_set_autoc_max_height(sci, editor_prefs.symbolcompletion_max_height);

	editor_set_indentation_guides(editor);

	sci_set_visible_white_spaces(sci, editor_prefs.show_white_space);
	sci_set_visible_eols(sci, editor_prefs.show_line_endings);

	sci_set_folding_margin_visible(sci, editor_prefs.folding);

	/* (dis)allow scrolling past end of document */
	sci_set_scroll_stop_at_last_line(sci, editor_prefs.scroll_stop_at_last_line);

	sci_assign_cmdkey(sci, SCK_HOME,
		editor_prefs.smart_home_key ? SCI_VCHOMEWRAP : SCI_HOMEWRAP);
	sci_assign_cmdkey(sci, SCK_END,  SCI_LINEENDWRAP);
}

