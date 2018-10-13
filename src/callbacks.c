/*
 *      callbacks.c - this file is part of Geany, a fast and lightweight IDE
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

/*
 * Callbacks used by Glade. These are mainly in response to menu item and button events in the
 * main window. Callbacks not used by Glade should go elsewhere.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "callbacks.h"

#include "about.h"
#include "app.h"
#include "build.h"
#include "dialogs.h"
#include "documentprivate.h"
#include "encodings.h"
#include "filetypes.h"
#include "geanyobject.h"
#include "highlighting.h"
#include "keybindings.h"
#include "keyfile.h"
#include "log.h"
#include "main.h"
#include "msgwindow.h"
#include "navqueue.h"
#include "plugins.h"
#include "pluginutils.h"
#include "prefs.h"
#include "printing.h"
#include "sciwrappers.h"
#include "sidebar.h"
#include "spawn.h"
#ifdef HAVE_SOCKET
# include "socket.h"
#endif
#include "support.h"
#include "symbols.h"
#include "templates.h"
#include "toolbar.h"
#include "tools.h"
#include "ui_utils.h"
#include "utils.h"
#include "vte.h"

#include "gtkcompat.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>
#include <time.h>


/* represents the state at switching a notebook page(in the left treeviews widget), to not emit
 * the selection-changed signal from tv.tree_openfiles */
/*static gboolean switch_tv_notebook_page = FALSE; */



/* wrapper function to abort exit process if cancel button is pressed */
static gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer gdata)
{
	return !main_quit();
}


/*
 * GUI callbacks
 */

void on_new1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	document_new_file(NULL, NULL, NULL);
}


/* create a new file and copy file content and properties */
static void on_clone1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *old_doc = document_get_current();

	if (old_doc)
		document_clone(old_doc);
}


void on_save1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	if (doc != NULL)
	{
		document_save_file(doc, ui_prefs.allow_always_save);
	}
}


void on_save_as1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	dialogs_show_save_as();
}


void on_save_all1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	guint i, max = (guint) gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	GeanyDocument *cur_doc = document_get_current();
	guint count = 0;

	/* iterate over documents in tabs order */
	for (i = 0; i < max; i++)
	{
		GeanyDocument *doc = document_get_from_page(i);

		if (! doc->changed)
			continue;

		if (document_save_file(doc, FALSE))
			count++;
	}
	if (!count)
		return;

	ui_set_statusbar(FALSE, ngettext("%d file saved.", "%d files saved.", count), count);
	/* saving may have changed window title, sidebar for another doc, so update */
	document_show_tab(cur_doc);
	sidebar_update_tag_list(cur_doc, TRUE);
	ui_set_window_title(cur_doc);
}


void on_close_all1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	document_close_all();
}


void on_close1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	if (doc != NULL)
		document_close(doc);
}


void on_quit1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	main_quit();
}


static void on_file1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gtk_widget_set_sensitive(ui_widgets.recent_files_menuitem,
						g_queue_get_length(ui_prefs.recent_queue) > 0);
	/* hide Page setup when GTK printing is not used */
	ui_widget_show_hide(ui_widgets.print_page_setup, printing_prefs.use_gtk_printing);
}


/* edit actions, c&p & co, from menu bar and from popup menu */
static void on_edit1_select(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *item;
	GeanyDocument *doc = document_get_current();

	ui_update_menu_copy_items(doc);
	ui_update_insert_include_item(doc, 1);

	item = ui_lookup_widget(main_widgets.window, "plugin_preferences1");
#ifndef HAVE_PLUGINS
	gtk_widget_hide(item);
#else
	gtk_widget_set_sensitive(item, plugins_have_preferences());
#endif
}


static void on_edit1_deselect(GtkMenuShell *menushell, gpointer user_data)
{
	/* we re-enable items that were disabled in on_edit1_select() on menu popdown to
	 * workaround mutli-layout keyboard issues in our keybinding handling code, so that
	 * GTK's accelerator handling can catch them.
	 * See https://github.com/geany/geany/issues/1368#issuecomment-273678207 */
	ui_menu_copy_items_set_sensitive(TRUE);
}


void on_undo1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	if (document_can_undo(doc))
	{
		sci_cancel(doc->editor->sci);
		document_undo(doc);
	}
}


void on_redo1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	if (document_can_redo(doc))
	{
		sci_cancel(doc->editor->sci);
		document_redo(doc);
	}
}


void on_cut1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_cut_clipboard(GTK_EDITABLE(focusw));
	else if (IS_SCINTILLA(focusw))
		sci_cut(SCINTILLA(focusw));
	else if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_cut_clipboard(buffer, gtk_clipboard_get(GDK_NONE), TRUE);
	}
}


void on_copy1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_copy_clipboard(GTK_EDITABLE(focusw));
	else if (IS_SCINTILLA(focusw))
		sci_copy(SCINTILLA(focusw));
	else if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_copy_clipboard(buffer, gtk_clipboard_get(GDK_NONE));
	}
}


void on_paste1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_paste_clipboard(GTK_EDITABLE(focusw));
	else if (IS_SCINTILLA(focusw))
		sci_paste(SCINTILLA(focusw));
	else if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_paste_clipboard(buffer, gtk_clipboard_get(GDK_NONE), NULL,
			TRUE);
	}
}


void on_delete1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_delete_selection(GTK_EDITABLE(focusw));
	else if (IS_SCINTILLA(focusw) && sci_has_selection(SCINTILLA(focusw)))
		sci_clear(SCINTILLA(focusw));
	else if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_delete_selection(buffer, TRUE, TRUE);
	}
}


void on_preferences1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	prefs_show_dialog();
}


/* about menu item */
static void on_info1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	about_dialog_show();
}


/* open file */
void on_open1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	dialogs_show_open_file();
}


/* reload file */
void on_toolbutton_reload_clicked(GtkAction *action, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	document_reload_prompt(doc, NULL);
}


static void on_change_font1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	dialogs_show_open_font();
}


/* store text, clear search flags so we can use Search->Find Next/Previous */
static void setup_find(const gchar *text, gboolean backwards)
{
	SETPTR(search_data.text, g_strdup(text));
	SETPTR(search_data.original_text, g_strdup(text));
	search_data.flags = 0;
	search_data.backwards = backwards;
	search_data.search_bar = TRUE;
}


static void do_toolbar_search(const gchar *text, gboolean incremental, gboolean backwards)
{
	GeanyDocument *doc = document_get_current();
	gboolean result;

	setup_find(text, backwards);
	result = document_search_bar_find(doc, search_data.text, incremental, backwards);
	if (search_data.search_bar)
		ui_set_search_entry_background(toolbar_get_widget_child_by_name("SearchEntry"), result);
}


/* search text */
void on_toolbar_search_entry_changed(GtkAction *action, const gchar *text, gpointer user_data)
{
	do_toolbar_search(text, TRUE, FALSE);
}


/* search text */
void on_toolbar_search_entry_activate(GtkAction *action, const gchar *text, gpointer user_data)
{
	do_toolbar_search(text, FALSE, GPOINTER_TO_INT(user_data));
}


/* search text */
void on_toolbutton_search_clicked(GtkAction *action, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gboolean result;
	GtkWidget *entry = toolbar_get_widget_child_by_name("SearchEntry");

	if (entry != NULL)
	{
		const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));

		setup_find(text, FALSE);
		result = document_search_bar_find(doc, search_data.text, FALSE, FALSE);
		if (search_data.search_bar)
			ui_set_search_entry_background(entry, result);
	}
	else
		on_find1_activate(NULL, NULL);
}


/* hides toolbar from toolbar popup menu */
static void on_hide_toolbar1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *tool_item = ui_lookup_widget(GTK_WIDGET(main_widgets.window), "menu_show_toolbar1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(tool_item), FALSE);
}


/* zoom in from menu bar and popup menu */
void on_zoom_in1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	sci_zoom_in(doc->editor->sci);
}


/* zoom out from menu bar and popup menu */
void on_zoom_out1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	sci_zoom_out(doc->editor->sci);
}


void on_normal_size1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	sci_zoom_off(doc->editor->sci);
}


/* Changes window-title after switching tabs and lots of other things.
 * note: using 'after' makes Scintilla redraw before the UI, appearing more responsive */
static void on_notebook1_switch_page_after(GtkNotebook *notebook, gpointer page,
		guint page_num, gpointer user_data)
{
	GeanyDocument *doc;

	if (G_UNLIKELY(main_status.opening_session_files || main_status.closing_all))
		return;

	doc = document_get_from_notebook_child(page);

	if (doc != NULL)
	{
		sidebar_select_openfiles_item(doc);
		ui_save_buttons_toggle(doc->changed);
		ui_set_window_title(doc);
		ui_update_statusbar(doc, -1);
		ui_update_popup_reundo_items(doc);
		ui_document_show_hide(doc); /* update the document menu */
		build_menu_update(doc);
		sidebar_update_tag_list(doc, FALSE);
		document_highlight_tags(doc);

		document_check_disk_status(doc, TRUE);

#ifdef HAVE_VTE
		vte_cwd((doc->real_path != NULL) ? doc->real_path : doc->file_name, FALSE);
#endif

		g_signal_emit_by_name(geany_object, "document-activate", doc);
	}
}


static void on_tv_notebook_switch_page(GtkNotebook *notebook, gpointer page,
		guint page_num, gpointer user_data)
{
	/* suppress selection changed signal when switching to the open files list */
	ignore_callback = TRUE;
}


static void on_tv_notebook_switch_page_after(GtkNotebook *notebook, gpointer page,
		guint page_num, gpointer user_data)
{
	ignore_callback = FALSE;
}


static void convert_eol(gint mode)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	/* sci_convert_eols() adds UNDO_SCINTILLA action in on_editor_notify().
	 * It is added to the undo stack before sci_convert_eols() finishes
	 * so after adding UNDO_EOL, UNDO_EOL will be at the top of the stack
	 * and UNDO_SCINTILLA below it. */
	sci_convert_eols(doc->editor->sci, mode);
	document_undo_add(doc, UNDO_EOL, GINT_TO_POINTER(sci_get_eol_mode(doc->editor->sci)));

	sci_set_eol_mode(doc->editor->sci, mode);

	ui_update_statusbar(doc, -1);
}


static void on_crlf_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	if (ignore_callback || ! gtk_check_menu_item_get_active(menuitem))
		return;

	convert_eol(SC_EOL_CRLF);
}


static void on_lf_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	if (ignore_callback || ! gtk_check_menu_item_get_active(menuitem))
		return;

	convert_eol(SC_EOL_LF);
}


static void on_cr_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	if (ignore_callback || ! gtk_check_menu_item_get_active(menuitem))
		return;

	convert_eol(SC_EOL_CR);
}


void on_replace_tabs_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	editor_replace_tabs(doc->editor, FALSE);
}


gboolean toolbar_popup_menu(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_menu_popup(GTK_MENU(ui_widgets.toolbar_menu), NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}
	return FALSE;
}


void on_toggle_case1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	ScintillaObject *sci;
	gboolean keep_sel = TRUE;

	g_return_if_fail(doc != NULL);

	sci = doc->editor->sci;
	if (! sci_has_selection(sci))
	{
		keybindings_send_command(GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_WORD);
		keep_sel = FALSE;
	}

	/* either we already had a selection or we created one for current word */
	if (sci_has_selection(sci))
	{
		gchar *result = NULL;
		gint cmd = SCI_LOWERCASE;
		gboolean rectsel = (gboolean) SSM(sci, SCI_SELECTIONISRECTANGLE, 0, 0);
		gchar *text = sci_get_selection_contents(sci);

		if (utils_str_has_upper(text))
		{
			if (rectsel)
				cmd = SCI_LOWERCASE;
			else
				result = g_utf8_strdown(text, -1);
		}
		else
		{
			if (rectsel)
				cmd = SCI_UPPERCASE;
			else
				result = g_utf8_strup(text, -1);
		}

		if (result != NULL)
		{
			sci_replace_sel(sci, result);
			g_free(result);
			if (keep_sel)
				sci_set_selection_start(sci, sci_get_current_position(sci) - strlen(text));
		}
		else
			sci_send_command(sci, cmd);

		g_free(text);
	}
}


static void on_show_toolbar1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback) return;

	toolbar_prefs.visible = (toolbar_prefs.visible) ? FALSE : TRUE;;
	ui_widget_show_hide(GTK_WIDGET(main_widgets.toolbar), toolbar_prefs.visible);
}


static void on_fullscreen1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	ui_prefs.fullscreen = (ui_prefs.fullscreen) ? FALSE : TRUE;
	ui_set_fullscreen();
}


static void on_show_messages_window1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	ui_prefs.msgwindow_visible = (ui_prefs.msgwindow_visible) ? FALSE : TRUE;
	msgwin_show_hide(ui_prefs.msgwindow_visible);
}


static void on_menu_color_schemes_activate(GtkImageMenuItem *imagemenuitem, gpointer user_data)
{
	highlighting_show_color_scheme_dialog();
}


static void on_markers_margin1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	editor_prefs.show_markers_margin = ! editor_prefs.show_markers_margin;
	ui_toggle_editor_features(GEANY_EDITOR_SHOW_MARKERS_MARGIN);
}


static void on_show_line_numbers1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	editor_prefs.show_linenumber_margin = ! editor_prefs.show_linenumber_margin;
	ui_toggle_editor_features(GEANY_EDITOR_SHOW_LINE_NUMBERS);
}


static void on_menu_show_white_space1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	editor_prefs.show_white_space = ! editor_prefs.show_white_space;
	ui_toggle_editor_features(GEANY_EDITOR_SHOW_WHITE_SPACE);
}


static void on_menu_show_line_endings1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	editor_prefs.show_line_endings = ! editor_prefs.show_line_endings;
	ui_toggle_editor_features(GEANY_EDITOR_SHOW_LINE_ENDINGS);
}


static void on_menu_show_indentation_guides1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	editor_prefs.show_indent_guide = ! editor_prefs.show_indent_guide;
	ui_toggle_editor_features(GEANY_EDITOR_SHOW_INDENTATION_GUIDES);
}


void on_line_wrapping1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (! ignore_callback)
	{
		GeanyDocument *doc = document_get_current();
		g_return_if_fail(doc != NULL);

		editor_set_line_wrapping(doc->editor, ! doc->editor->line_wrapping);
	}
}


static void on_set_file_readonly1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (! ignore_callback)
	{
		GeanyDocument *doc = document_get_current();
		g_return_if_fail(doc != NULL);

		doc->readonly = ! doc->readonly;
		sci_set_readonly(doc->editor->sci, doc->readonly);
		ui_update_tab_status(doc);
		ui_update_statusbar(doc, -1);
	}
}


static void on_use_auto_indentation1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (! ignore_callback)
	{
		GeanyDocument *doc = document_get_current();
		g_return_if_fail(doc != NULL);

		doc->editor->auto_indent = ! doc->editor->auto_indent;
	}
}


static void find_usage(gboolean in_session)
{
	GeanyFindFlags flags;
	gchar *search_text;
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	if (sci_has_selection(doc->editor->sci))
	{	/* take selected text if there is a selection */
		search_text = sci_get_selection_contents(doc->editor->sci);
		flags = GEANY_FIND_MATCHCASE;
	}
	else
	{
		editor_find_current_word_sciwc(doc->editor, -1,
			editor_info.current_word, GEANY_MAX_WORD_LENGTH);
		search_text = g_strdup(editor_info.current_word);
		flags = GEANY_FIND_MATCHCASE | GEANY_FIND_WHOLEWORD;
	}

	search_find_usage(search_text, search_text, flags, in_session);
	g_free(search_text);
}


void on_find_document_usage1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	find_usage(FALSE);
}


void on_find_usage1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	find_usage(TRUE);
}


static void goto_tag(gboolean definition)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	/* update cursor pos for navigating back afterwards */
	if (!sci_has_selection(doc->editor->sci))
		sci_set_current_position(doc->editor->sci, editor_info.click_pos, FALSE);

	/* use the keybinding callback as it checks for selections as well as current word */
	if (definition)
		keybindings_send_command(GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TAGDEFINITION);
	else
		keybindings_send_command(GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_TAGDECLARATION);
}


static void on_goto_tag_definition1(GtkMenuItem *menuitem, gpointer user_data)
{
	goto_tag(TRUE);
}


static void on_goto_tag_declaration1(GtkMenuItem *menuitem, gpointer user_data)
{
	goto_tag(FALSE);
}


static void on_count_words1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	tools_word_count();
}


void on_show_color_chooser1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar colour[9];
	GeanyDocument *doc = document_get_current();
	gint pos;

	g_return_if_fail(doc != NULL);

	pos = sci_get_current_position(doc->editor->sci);
	editor_find_current_word(doc->editor, pos, colour, sizeof colour, GEANY_WORDCHARS"#");
	tools_color_chooser(colour);
}


void on_toolbutton_compile_clicked(GtkAction *action, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_COMPILE);
}


void on_find1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	search_show_find_dialog();
}


void on_find_next1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	search_find_again(FALSE);
}


void on_find_previous1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	if (search_data.flags & GEANY_FIND_REGEXP)
		/* Can't reverse search order for a regex (find next ignores search backwards) */
		utils_beep();
	else
		search_find_again(TRUE);
}


void on_find_nextsel1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	search_find_selection(document_get_current(), FALSE);
}


void on_find_prevsel1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	search_find_selection(document_get_current(), TRUE);
}


void on_replace1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	search_show_replace_dialog();
}


void on_find_in_files1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	search_show_find_in_files_dialog(NULL);
}


static void get_line_and_offset_from_text(const gchar *text, gint *line_no, gint *offset)
{
	if (*text == '+' || *text == '-')
	{
		*line_no = atoi(text + 1);
		*offset = (*text == '+') ? 1 : -1;
	}
	else
	{
		*line_no = atoi(text) - 1;
		*offset = 0;
	}
}


void on_go_to_line_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	static gchar value[16] = "";
	gchar *result;

	result = dialogs_show_input_goto_line(
		_("Go to Line"), GTK_WINDOW(main_widgets.window),
		_("Enter the line you want to go to:"), value);
	if (result != NULL)
	{
		GeanyDocument *doc = document_get_current();
		gint offset;
		gint line_no;

		g_return_if_fail(doc != NULL);

		get_line_and_offset_from_text(result, &line_no, &offset);
		if (! editor_goto_line(doc->editor, line_no, offset))
			utils_beep();
		/* remember value for future calls */
		g_snprintf(value, sizeof(value), "%s", result);

		g_free(result);
	}
}


void on_toolbutton_goto_entry_activate(GtkAction *action, const gchar *text, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gint offset;
	gint line_no;

	g_return_if_fail(doc != NULL);

	get_line_and_offset_from_text(text, &line_no, &offset);
	if (! editor_goto_line(doc->editor, line_no, offset))
		utils_beep();
	else
		keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);
}


void on_toolbutton_goto_clicked(GtkAction *action, gpointer user_data)
{
	GtkWidget *entry = toolbar_get_widget_child_by_name("GotoEntry");

	if (entry != NULL)
	{
		const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));

		on_toolbutton_goto_entry_activate(NULL, text, NULL);
	}
	else
		on_go_to_line_activate(NULL, NULL);
}


void on_help1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *uri;

	uri = utils_get_help_url(NULL);
	utils_open_browser(uri);
	g_free(uri);
}


static void on_help_shortcuts1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_show_shortcuts();
}


static void on_website1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	utils_open_browser(GEANY_HOMEPAGE);
}


static void on_help_menu_item_donate_activate(GtkMenuItem *item, gpointer user_data)
{
	utils_open_browser(GEANY_DONATE);
}


static void on_help_menu_item_wiki_activate(GtkMenuItem *item, gpointer user_data)
{
	utils_open_browser(GEANY_WIKI);
}


static void on_help_menu_item_bug_report_activate(GtkMenuItem *item, gpointer user_data)
{
	utils_open_browser(GEANY_BUG_REPORT);
}


static void on_comments_function_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gchar *text;
	const gchar *cur_tag = NULL;
	gint line = -1, pos = 0;

	if (doc == NULL || doc->file_type == NULL)
	{
		ui_set_statusbar(FALSE,
			_("Please set the filetype for the current file before using this function."));
		return;
	}

	/* symbols_get_current_function returns -1 on failure, so sci_get_position_from_line
	 * returns the current position, so it should be safe */
	line = symbols_get_current_function(doc, &cur_tag);
	pos = sci_get_position_from_line(doc->editor->sci, line);

	text = templates_get_template_function(doc, cur_tag);

	sci_start_undo_action(doc->editor->sci);
	sci_insert_text(doc->editor->sci, pos, text);
	sci_end_undo_action(doc->editor->sci);
	g_free(text);
}


static void insert_multiline_comment(GeanyDocument *doc, gint pos)
{
	g_return_if_fail(doc != NULL);
	g_return_if_fail(pos == -1 || pos >= 0);

	if (doc->file_type == NULL)
	{
		ui_set_statusbar(FALSE,
			_("Please set the filetype for the current file before using this function."));
		return;
	}

	if (doc->file_type->comment_open || doc->file_type->comment_single)
	{
		/* editor_insert_multiline_comment() uses editor_info.click_pos */
		if (pos == -1)
			editor_info.click_pos = sci_get_current_position(doc->editor->sci);
		else
			editor_info.click_pos = pos;
		editor_insert_multiline_comment(doc->editor);
	}
	else
		utils_beep();
}


static void on_comments_multiline_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_multiline_comment(document_get_current(), editor_info.click_pos);
}


static void on_menu_comments_multiline_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_multiline_comment(document_get_current(), -1);
}


static void insert_comment_template(GeanyDocument *doc, gint pos, guint template)
{
	gchar *text;

	g_return_if_fail(doc != NULL);
	g_return_if_fail(pos == -1 || pos >= 0);
	g_return_if_fail(template < GEANY_MAX_TEMPLATES);

	if (pos == -1)
		pos = sci_get_current_position(doc->editor->sci);

	text = templates_get_template_licence(doc, template);

	sci_start_undo_action(doc->editor->sci);
	sci_insert_text(doc->editor->sci, pos, text);
	sci_end_undo_action(doc->editor->sci);
	g_free(text);
}


static void on_comments_gpl_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_comment_template(document_get_current(), editor_info.click_pos, GEANY_TEMPLATE_GPL);
}


static void on_menu_comments_gpl_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_comment_template(document_get_current(), -1, GEANY_TEMPLATE_GPL);
}


static void on_comments_bsd_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_comment_template(document_get_current(), editor_info.click_pos, GEANY_TEMPLATE_BSD);
}


static void on_menu_comments_bsd_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	insert_comment_template(document_get_current(), -1, GEANY_TEMPLATE_BSD);
}


static void on_comments_changelog_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gchar *text;

	g_return_if_fail(doc != NULL);

	text = templates_get_template_changelog(doc);
	sci_start_undo_action(doc->editor->sci);
	sci_insert_text(doc->editor->sci, 0, text);
	/* sets the cursor to the right position to type the changelog text,
	 * the template has 21 chars + length of name and email */
	sci_goto_pos(doc->editor->sci, 21 + strlen(template_prefs.developer) + strlen(template_prefs.mail), TRUE);
	sci_end_undo_action(doc->editor->sci);

	g_free(text);
}


static void on_comments_fileheader_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gchar *text;
	const gchar *fname;
	GeanyFiletype *ft;

	g_return_if_fail(doc != NULL);

	ft = doc->file_type;
	fname = doc->file_name;
	text = templates_get_template_fileheader(FILETYPE_ID(ft), fname);

	sci_start_undo_action(doc->editor->sci);
	sci_insert_text(doc->editor->sci, 0, text);
	sci_goto_pos(doc->editor->sci, 0, FALSE);
	sci_end_undo_action(doc->editor->sci);
	g_free(text);
}


void on_file_properties_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	dialogs_show_file_properties(doc);
}


static void on_menu_fold_all1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_fold_all(doc->editor);
}


static void on_menu_unfold_all1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_unfold_all(doc->editor);
}


void on_toolbutton_run_clicked(GtkAction *action, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_RUN);
}


void on_menu_remove_indicators1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_indicator_clear(doc->editor, GEANY_INDICATOR_ERROR);
}


void on_print1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	printing_print_doc(doc);
}


void on_menu_select_all1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(main_widgets.window));

	/* special case for Select All in the scribble widget */
	if (GTK_IS_TEXT_VIEW(focusw))
	{
		g_signal_emit_by_name(focusw, "select-all", TRUE);
	}
	/* special case for Select All in the VTE widget */
#ifdef HAVE_VTE
	else if (vte_info.have_vte && focusw == vc->vte)
	{
		vte_select_all();
	}
#endif
	else if (GTK_IS_EDITABLE(focusw))
	{
		gtk_editable_select_region(GTK_EDITABLE(focusw), 0, -1);
	}
	else if (IS_SCINTILLA(focusw))
	{
		sci_select_all(SCINTILLA(focusw));
	}
}


void on_menu_show_sidebar1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (ignore_callback)
		return;

	ui_prefs.sidebar_visible = ! ui_prefs.sidebar_visible;

	/* show built-in tabs if no tabs visible */
	if (ui_prefs.sidebar_visible &&
		! interface_prefs.sidebar_openfiles_visible && ! interface_prefs.sidebar_symbol_visible &&
		gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.sidebar_notebook)) <= 2)
	{
		interface_prefs.sidebar_openfiles_visible = TRUE;
		interface_prefs.sidebar_symbol_visible = TRUE;
	}

	/* if window has input focus, set it back to the editor before toggling off */
	if (! ui_prefs.sidebar_visible &&
		gtk_container_get_focus_child(GTK_CONTAINER(main_widgets.sidebar_notebook)) != NULL)
	{
		keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);
	}

	ui_sidebar_show_hide();
}


static void on_menu_write_unicode_bom1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data)
{
	if (! ignore_callback)
	{
		GeanyDocument *doc = document_get_current();

		g_return_if_fail(doc != NULL);
		if (doc->readonly)
		{
			utils_beep();
			return;
		}

		document_undo_add(doc, UNDO_BOM, GINT_TO_POINTER(doc->has_bom));

		doc->has_bom = ! doc->has_bom;

		ui_update_statusbar(doc, -1);
	}
}


void on_menu_comment_line1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_do_comment(doc->editor, -1, FALSE, FALSE, TRUE);
}


void on_menu_uncomment_line1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_do_uncomment(doc->editor, -1, FALSE);
}


void on_menu_toggle_line_commentation1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_do_comment_toggle(doc->editor);
}


void on_menu_increase_indent1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_indent(doc->editor, TRUE);
}


void on_menu_decrease_indent1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_indent(doc->editor, FALSE);
}


void on_next_message1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	if (! ui_tree_view_find_next(GTK_TREE_VIEW(msgwindow.tree_msg),
		msgwin_goto_messages_file_line))
		ui_set_statusbar(FALSE, _("No more message items."));
}


void on_previous_message1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	if (! ui_tree_view_find_previous(GTK_TREE_VIEW(msgwindow.tree_msg),
		msgwin_goto_messages_file_line))
		ui_set_statusbar(FALSE, _("No more message items."));
}


void on_project_new1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	project_new();
}


void on_project_open1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	project_open();
}


void on_project_close1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	project_close(TRUE);
}


void on_project_properties1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	project_properties();
}


static void on_menu_project1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	static GtkWidget *item_close = NULL;
	static GtkWidget *item_properties = NULL;

	if (item_close == NULL)
	{
		item_close = ui_lookup_widget(main_widgets.window, "project_close1");
		item_properties = ui_lookup_widget(main_widgets.window, "project_properties1");
	}

	gtk_widget_set_sensitive(item_close, (app->project != NULL));
	gtk_widget_set_sensitive(item_properties, (app->project != NULL));
	gtk_widget_set_sensitive(ui_widgets.recent_projects_menuitem,
						g_queue_get_length(ui_prefs.recent_projects_queue) > 0);
}


void on_menu_open_selected_file1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gchar *sel = NULL;
	const gchar *wc;

#ifdef G_OS_WIN32
	wc = GEANY_WORDCHARS "./-" "\\";
#else
	wc = GEANY_WORDCHARS "./-";
#endif

	g_return_if_fail(doc != NULL);

	sel = editor_get_default_selection(doc->editor, TRUE, wc);
	SETPTR(sel, utils_get_locale_from_utf8(sel));

	if (sel != NULL)
	{
		gchar *filename = NULL;

		if (g_path_is_absolute(sel))
			filename = g_strdup(sel);
		else
		{	/* relative filename, add the path of the current file */
			gchar *path;

			path = utils_get_current_file_dir_utf8();
			SETPTR(path, utils_get_locale_from_utf8(path));
			if (!path)
				path = g_get_current_dir();

			filename = g_build_path(G_DIR_SEPARATOR_S, path, sel, NULL);

			if (! g_file_test(filename, G_FILE_TEST_EXISTS) &&
				app->project != NULL && !EMPTY(app->project->base_path))
			{
				/* try the project's base path */
				SETPTR(path, project_get_base_path());
				SETPTR(path, utils_get_locale_from_utf8(path));
				SETPTR(filename, g_build_path(G_DIR_SEPARATOR_S, path, sel, NULL));
			}
			g_free(path);
#ifdef G_OS_UNIX
			if (! g_file_test(filename, G_FILE_TEST_EXISTS))
				SETPTR(filename, g_build_path(G_DIR_SEPARATOR_S, "/usr/local/include", sel, NULL));

			if (! g_file_test(filename, G_FILE_TEST_EXISTS))
				SETPTR(filename, g_build_path(G_DIR_SEPARATOR_S, "/usr/include", sel, NULL));
#endif
		}

		if (g_file_test(filename, G_FILE_TEST_EXISTS))
			document_open_file(filename, FALSE, NULL, NULL);
		else
		{
			SETPTR(sel, utils_get_utf8_from_locale(sel));
			ui_set_statusbar(TRUE, _("Could not open file %s (File not found)"), sel);
		}

		g_free(filename);
		g_free(sel);
	}
}


void on_remove_markers1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	g_return_if_fail(doc != NULL);

	sci_marker_delete_all(doc->editor->sci, 0);	/* delete the yellow tag marker */
	sci_marker_delete_all(doc->editor->sci, 1);	/* delete user markers */
	editor_indicator_clear(doc->editor, GEANY_INDICATOR_SEARCH);
}


static void on_load_tags1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	symbols_show_load_tags_dialog();
}


void on_context_action1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *word, *command;
	GError *error = NULL;
	GeanyDocument *doc = document_get_current();
	const gchar *check_msg;

	g_return_if_fail(doc != NULL);

	if (sci_has_selection(doc->editor->sci))
	{	/* take selected text if there is a selection */
		word = sci_get_selection_contents(doc->editor->sci);
	}
	else
	{
		word = g_strdup(editor_info.current_word);
	}

	/* use the filetype specific command if available, fallback to global command otherwise */
	if (doc->file_type != NULL &&
		!EMPTY(doc->file_type->context_action_cmd))
	{
		command = g_strdup(doc->file_type->context_action_cmd);
		check_msg = _("Check the path setting in Filetype configuration.");
	}
	else
	{
		command = g_strdup(tool_prefs.context_action_cmd);
		check_msg = _("Check the path setting in Preferences.");
	}

	/* substitute the wildcard %s and run the command if it is non empty */
	if (G_LIKELY(!EMPTY(command)))
	{
		gchar *command_line = g_strdup(command);

		utils_str_replace_all(&command_line, "%s", word);

		if (!spawn_async(NULL, command_line, NULL, NULL, NULL, &error))
		{
			/* G_SHELL_ERROR is parsing error, it may be caused by %s word with quotes */
			ui_set_statusbar(TRUE, _("Cannot execute context action command \"%s\": %s. %s"),
				error->domain == G_SHELL_ERROR ? command_line : command, error->message,
				check_msg);
			g_error_free(error);
		}
		g_free(command_line);
	}
	else
	{
		ui_set_statusbar(TRUE, _("No context action set."));
	}
	g_free(word);
	g_free(command);
}


void on_menu_toggle_all_additional_widgets1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	static gint hide_all = -1;
	GtkCheckMenuItem *msgw = GTK_CHECK_MENU_ITEM(
		ui_lookup_widget(main_widgets.window, "menu_show_messages_window1"));
	GtkCheckMenuItem *toolbari = GTK_CHECK_MENU_ITEM(
		ui_lookup_widget(main_widgets.window, "menu_show_toolbar1"));

	/* get the initial state (necessary if Geany was closed with hide_all = TRUE) */
	if (G_UNLIKELY(hide_all == -1))
	{
		if (! gtk_check_menu_item_get_active(msgw) &&
			! interface_prefs.show_notebook_tabs &&
			! gtk_check_menu_item_get_active(toolbari))
		{
			hide_all = TRUE;
		}
		else
			hide_all = FALSE;
	}

	hide_all = ! hide_all; /* toggle */

	if (hide_all)
	{
		if (gtk_check_menu_item_get_active(msgw))
			gtk_check_menu_item_set_active(msgw, ! gtk_check_menu_item_get_active(msgw));

		interface_prefs.show_notebook_tabs = FALSE;
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(main_widgets.notebook), interface_prefs.show_notebook_tabs);

		ui_statusbar_showhide(FALSE);

		if (gtk_check_menu_item_get_active(toolbari))
			gtk_check_menu_item_set_active(toolbari, ! gtk_check_menu_item_get_active(toolbari));
	}
	else
	{

		if (! gtk_check_menu_item_get_active(msgw))
			gtk_check_menu_item_set_active(msgw, ! gtk_check_menu_item_get_active(msgw));

		interface_prefs.show_notebook_tabs = TRUE;
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(main_widgets.notebook), interface_prefs.show_notebook_tabs);

		ui_statusbar_showhide(TRUE);

		if (! gtk_check_menu_item_get_active(toolbari))
			gtk_check_menu_item_set_active(toolbari, ! gtk_check_menu_item_get_active(toolbari));
	}
}


void on_toolbutton_forward_activate(GtkAction *menuitem, gpointer user_data)
{
	navqueue_go_forward();
}


void on_toolbutton_back_activate(GtkAction *menuitem, gpointer user_data)
{
	navqueue_go_back();
}


gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	if (prefs.auto_focus && ! gtk_widget_has_focus(widget))
		gtk_widget_grab_focus(widget);

	return FALSE;
}


static void set_indent_type(GtkCheckMenuItem *menuitem, GeanyIndentType type)
{
	GeanyDocument *doc;

	if (ignore_callback || ! gtk_check_menu_item_get_active(menuitem))
		return;

	doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_set_indent(doc->editor, type, doc->editor->indent_width);
	ui_update_statusbar(doc, -1);
}


static void on_tabs1_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	set_indent_type(menuitem, GEANY_INDENT_TYPE_TABS);
}


static void on_spaces1_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	set_indent_type(menuitem, GEANY_INDENT_TYPE_SPACES);
}


static void on_tabs_and_spaces1_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	set_indent_type(menuitem, GEANY_INDENT_TYPE_BOTH);
}


static void on_strip_trailing_spaces1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc;

	if (ignore_callback)
		return;

	doc = document_get_current();
	g_return_if_fail(doc != NULL);

	editor_strip_trailing_spaces(doc->editor, FALSE);
}


static void on_page_setup1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	printing_page_setup_gtk();
}


gboolean on_escape_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	guint state = keybindings_get_modifiers(event->state);

	/* make pressing escape in the sidebar and toolbar focus the editor */
	if (event->keyval == GDK_Escape && state == 0)
	{
		keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);
		return TRUE;
	}
	return FALSE;
}


void on_line_breaking1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc;

	if (ignore_callback)
		return;

	doc = document_get_current();
	g_return_if_fail(doc != NULL);

	doc->editor->line_breaking = !doc->editor->line_breaking;
}


void on_replace_spaces_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc != NULL);

	editor_replace_spaces(doc->editor, FALSE);
}


static void on_search1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *next_message = ui_lookup_widget(main_widgets.window, "next_message1");
	GtkWidget *previous_message = ui_lookup_widget(main_widgets.window, "previous_message1");
	gboolean have_messages;

	/* enable commands if the messages window has any items */
	have_messages = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(msgwindow.store_msg),
		NULL) > 0;

	gtk_widget_set_sensitive(next_message, have_messages);
	gtk_widget_set_sensitive(previous_message, have_messages);
}


/* simple implementation (vs. close all which doesn't close documents if cancelled),
 * if user_data is set, it is the GeanyDocument to keep */
void on_close_other_documents1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	guint i;
	GeanyDocument *cur_doc = user_data;

	if (cur_doc == NULL)
		cur_doc = document_get_current();

	for (i = 0; i < documents_array->len; i++)
	{
		GeanyDocument *doc = documents[i];

		if (doc == cur_doc || ! doc->is_valid)
			continue;

		if (! document_close(doc))
			break;
	}
}


static void on_menu_reload_configuration1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	main_reload_configuration();
}


static void on_debug_messages1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	log_show_debug_messages_dialog();
}


void on_send_selection_to_vte1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
#ifdef HAVE_VTE
	if (vte_info.have_vte)
		vte_send_selection_to_vte();
#endif
}


static gboolean on_window_state_event(GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{

	if (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
	{
		static GtkWidget *menuitem = NULL;

		if (menuitem == NULL)
			menuitem = ui_lookup_widget(widget, "menu_fullscreen1");

		ignore_callback = TRUE;

		ui_prefs.fullscreen = (event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) ? TRUE : FALSE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), ui_prefs.fullscreen);

		ignore_callback = FALSE;
	}
	return FALSE;
}


static void show_notebook_page(const gchar *notebook_name, const gchar *page_name)
{
	GtkWidget *widget;
	GtkNotebook *notebook;

	widget = ui_lookup_widget(ui_widgets.prefs_dialog, page_name);
	notebook = GTK_NOTEBOOK(ui_lookup_widget(ui_widgets.prefs_dialog, notebook_name));

	if (notebook != NULL && widget != NULL)
		gtk_notebook_set_current_page(notebook, gtk_notebook_page_num(notebook, widget));
}


static void on_customize_toolbar1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	prefs_show_dialog();

	/* select the Interface page */
	show_notebook_page("notebook2", "notebook6");
	/* select the Toolbar subpage */
	show_notebook_page("notebook6", "vbox15");
}


static void on_button_customize_toolbar_clicked(GtkButton *button, gpointer user_data)
{
	toolbar_configure(GTK_WINDOW(ui_widgets.prefs_dialog));
}


static void on_cut_current_lines1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_CUTLINE);
}


static void on_copy_current_lines1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_CLIPBOARD, GEANY_KEYS_CLIPBOARD_COPYLINE);
}


static void on_delete_current_lines1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DELETELINE);
}


static void on_duplicate_line_or_selection1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DUPLICATELINE);
}


static void on_select_current_lines1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_LINE);
}


static void on_select_current_paragraph1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_PARAGRAPH);
}


static void on_insert_alternative_white_space1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_INSERT, GEANY_KEYS_INSERT_ALTWHITESPACE);
}


static void on_go_to_next_marker1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_NEXTMARKER);
}


static void on_go_to_previous_marker1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_GOTO, GEANY_KEYS_GOTO_PREVIOUSMARKER);
}


static void on_reflow_lines_block1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_REFLOWPARAGRAPH);
}


static void on_move_lines_up1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_MOVELINEUP);
}


static void on_move_lines_down1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_MOVELINEDOWN);
}


static void on_smart_line_indent1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_FORMAT, GEANY_KEYS_FORMAT_AUTOINDENT);
}


void on_plugin_preferences1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
#ifdef HAVE_PLUGINS
	plugin_show_configure(NULL);
#endif
}


static void on_indent_width_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc;
	gchar *label;
	gint width;

	if (ignore_callback)
		return;

	label = ui_menu_item_get_text(menuitem);
	width = atoi(label);
	g_free(label);

	doc = document_get_current();
	if (doc != NULL && width > 0)
		editor_set_indent_width(doc->editor, width);
}


static void on_reset_indentation1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	guint i;

	foreach_document(i)
		document_apply_indent_settings(documents[i]);

	ui_update_statusbar(NULL, -1);
	ui_document_show_hide(NULL);
}


static void on_mark_all1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_SEARCH, GEANY_KEYS_SEARCH_MARKALL);
}


static void on_detect_type_from_file_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	GeanyIndentType type;

	if (doc != NULL && document_detect_indent_type(doc, &type))
	{
		editor_set_indent_type(doc->editor, type);
		ui_document_show_hide(doc);
		ui_update_statusbar(doc, -1);
	}
}


static void on_show_symbol_list_toggled(GtkToggleButton *button, gpointer user_data)
{
	GtkWidget *widget = ui_lookup_widget(ui_widgets.prefs_dialog, "box_show_symbol_list_children");

	gtk_widget_set_sensitive(widget, gtk_toggle_button_get_active(button));
}


static void on_detect_width_from_file_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gint width;

	if (doc != NULL && document_detect_indent_width(doc, &width))
	{
		editor_set_indent_width(doc->editor, width);
		ui_document_show_hide(doc);
	}
}


static void builder_connect_func(GtkBuilder *builder, GObject *object,
	const gchar *signal_name, const gchar *handler_name, GObject *connect_obj,
	GConnectFlags flags, gpointer user_data)
{
	GHashTable *hash = user_data;
	GCallback callback;

	callback = g_hash_table_lookup(hash, handler_name);
	g_return_if_fail(callback);

	if (connect_obj == NULL)
		g_signal_connect_data(object, signal_name, callback, NULL, NULL, flags);
	else
		g_signal_connect_object(object, signal_name, callback, connect_obj, flags);
}


void callbacks_connect(GtkBuilder *builder)
{
	GHashTable *hash;

	g_return_if_fail(GTK_IS_BUILDER(builder));

	hash = g_hash_table_new(g_str_hash, g_str_equal);

#define ITEM(n) g_hash_table_insert(hash, (gpointer) #n, G_CALLBACK(n));
#	include "signallist.i"
#undef ITEM

	gtk_builder_connect_signals_full(builder, builder_connect_func, hash);
	g_hash_table_destroy(hash);
}
