/*
 *      callbacks.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */

/*
 * Callbacks used by Glade. These are mainly in response to menu item and button events in the
 * main window. Callbacks not used by Glade should go elsewhere.
 */

#include "geany.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gstdio.h>
#include <time.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#include "keyfile.h"
#include "document.h"
#include "filetypes.h"
#include "sciwrappers.h"
#include "editor.h"
#include "ui_utils.h"
#include "utils.h"
#include "dialogs.h"
#include "about.h"
#include "msgwindow.h"
#include "build.h"
#include "prefs.h"
#include "templates.h"
#include "treeviews.h"
#include "keybindings.h"
#include "encodings.h"
#include "search.h"
#include "main.h"
#include "symbols.h"
#include "tools.h"
#include "project.h"
#include "navqueue.h"
#include "printing.h"
#include "plugins.h"

#include "geanyobject.h"

#ifdef HAVE_VTE
# include "vte.h"
#endif

#ifdef HAVE_SOCKET
# include "socket.h"
#endif


/* flag to indicate the explicit change of a toggle button of the toolbar menu and so the
 * toggled callback should ignore the change since it is not triggered by the user */
static gboolean ignore_toolbar_toggle = FALSE;

/* flag to indicate that an insert callback was triggered from the file menu,
 * so we need to store the current cursor position in editor_info.click_pos. */
static gboolean insert_callback_from_menu = FALSE;

/* represents the state at switching a notebook page(in the left treeviews widget), to not emit
 * the selection-changed signal from tv.tree_openfiles */
/*static gboolean switch_tv_notebook_page = FALSE; */

CallbacksData callbacks_data = {-1};


static gboolean check_no_unsaved(void)
{
	guint i;

	for (i = 0; i < doc_array->len; i++)
	{
		if (doc_list[i].is_valid && doc_list[i].changed)
		{
			return FALSE;
		}
	}
	return TRUE;	/* no unsaved edits */
}


/* @return TRUE if all files were saved or had their changes discarded. */
static gboolean account_for_unsaved(void)
{
	gint p;

	for (p = 0; p < gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)); p++)
	{
		gint idx = document_get_n_idx(p);

		if (doc_list[idx].changed)
		{
			if (! dialogs_show_unsaved_file(idx))
				return FALSE;
		}
	}
	return TRUE;
}


/* set editor_info.click_pos to the current cursor position if insert_callback_from_menu is TRUE
 * to prevent invalid cursor positions which can cause segfaults */
static void verify_click_pos(gint idx)
{
	if (insert_callback_from_menu)
	{
		editor_info.click_pos = sci_get_current_position(doc_list[idx].sci);
		insert_callback_from_menu = FALSE;
	}
}


static void force_close_all()
{
	guint i, len = doc_array->len;

	main_status.closing_all = TRUE;

	/* all documents should be accounted for, so ignore any changes */
	for (i = 0; i < len; i++)
	{
		if (doc_list[i].is_valid && doc_list[i].changed)
		{
			doc_list[i].changed = FALSE;
		}
	}
	for (i = 0; i < len; i++)
	{
		/* we don't need to check here for whether doc_list[i] is valid,
		 * it's done in document_remove(). */
		document_remove(0);
	}

	main_status.closing_all = FALSE;
}


/* should only be called from on_exit_clicked */
static void quit_app(void)
{
	configuration_save();

	if (app->project != NULL)
		project_close(FALSE);	/* save project session files */

	force_close_all();

	main_quit();
}


/* wrapper function to abort exit process if cancel button is pressed */
gboolean
on_exit_clicked                        (GtkWidget *widget, gpointer gdata)
{
	main_status.quitting = TRUE;

	if (! check_no_unsaved())
	{
		if (account_for_unsaved())
		{
			quit_app();
			return FALSE;
		}
	}
	else
	if (! prefs.confirm_exit ||
		dialogs_show_question_full(NULL, GTK_STOCK_QUIT, GTK_STOCK_CANCEL, NULL,
			_("Do you really want to quit?")))
	{
		quit_app();
		return FALSE;
	}

	main_status.quitting = FALSE;
	return TRUE;
}


/*
 * GUI callbacks
 */

void
on_new1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	document_new_file(NULL, NULL, NULL);
}


void
on_save1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
	gint idx = document_get_cur_idx();

	if (cur_page >= 0)
	{
		if (doc_list[idx].file_name == NULL)
			dialogs_show_save_as();
		else
			document_save_file(idx, FALSE);
	}
}


void
on_save_as1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_save_as();
}


void
on_save_all1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint i, idx, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));
	gint cur_idx = document_get_cur_idx();

	document_delay_colourise();	/* avoid recolourising all C files after each save */

	for (i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		if (! doc_list[idx].changed) continue;
		if (doc_list[idx].file_name == NULL)
		{
			/* display unnamed document */
			gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook),
				document_get_notebook_page(idx));
			dialogs_show_save_as();
		}
		else
			document_save_file(idx, FALSE);
	}
	document_colourise_new();
	treeviews_update_tag_list(cur_idx, TRUE);
	ui_set_window_title(cur_idx);
}


static gboolean close_all(void)
{
	if (! account_for_unsaved())
		return FALSE;

	force_close_all();

	tm_workspace_update(TM_WORK_OBJECT(app->tm_workspace), TRUE, TRUE, FALSE);
	return TRUE;
}


void
on_close_all1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	close_all();
}


void
on_close1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	guint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
	document_remove(cur_page);
}


void
on_quit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	on_exit_clicked(NULL, NULL);
}


void
on_file1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gtk_widget_set_sensitive(ui_widgets.recent_files_menuitem,
						g_queue_get_length(ui_prefs.recent_queue) > 0);
#if GTK_CHECK_VERSION(2, 10, 0)
	/* hide Page setup when GTK printing is not used
	 * (on GTK < 2.10 the menu item is hidden completely) */
	ui_widget_show_hide(ui_widgets.print_page_setup,
		printing_prefs.use_gtk_printing || gtk_check_version(2, 10, 0) != NULL);
#endif
}


/* edit actions, c&p & co, from menu bar and from popup menu */
void
on_edit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	ui_update_menu_copy_items(idx);
	ui_update_insert_include_item(idx, 1);
}


void
on_undo1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (document_can_undo(idx)) document_undo(idx);
}


void
on_redo1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	if (document_can_redo(idx)) document_redo(idx);
}


void
on_cut1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_cut_clipboard(GTK_EDITABLE(focusw));
	else
	if (IS_SCINTILLA(focusw) && idx >= 0)
		sci_cut(doc_list[idx].sci);
	else
	if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_cut_clipboard(buffer, gtk_clipboard_get(GDK_NONE), TRUE);
	}
}


void
on_copy1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_copy_clipboard(GTK_EDITABLE(focusw));
	else
	if (IS_SCINTILLA(focusw) && idx >= 0)
		sci_copy(doc_list[idx].sci);
	else
	if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_copy_clipboard(buffer, gtk_clipboard_get(GDK_NONE));
	}
}


void
on_paste1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_paste_clipboard(GTK_EDITABLE(focusw));
	else
	if (IS_SCINTILLA(focusw) && idx >= 0)
	{
#if 0
//#ifdef G_OS_WIN32
		/* insert the text manually for now, because the auto conversion of EOL characters by
		 * by Scintilla seems to make problems */
		/** TODO this is probably obsolete now since we fixed PLAT_GTK_WIN32 in Scintilla */
		if (gtk_clipboard_wait_is_text_available(gtk_clipboard_get(GDK_NONE)))
		{
			gchar *content = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_NONE));
			if (content != NULL)
			{
				sci_replace_sel(doc_list[idx].sci, content);
				g_free(content);
			}
		}
#else
		sci_paste(doc_list[idx].sci);
#endif
	}
	else
	if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_paste_clipboard(buffer, gtk_clipboard_get(GDK_NONE), NULL,
			TRUE);
	}
}


void
on_delete1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GtkWidget *focusw = gtk_window_get_focus(GTK_WINDOW(app->window));

	if (GTK_IS_EDITABLE(focusw))
		gtk_editable_delete_selection(GTK_EDITABLE(focusw));
	else
	if (IS_SCINTILLA(focusw) && idx >= 0)
		sci_clear(doc_list[idx].sci);
	else
	if (GTK_IS_TEXT_VIEW(focusw))
	{
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(
			GTK_TEXT_VIEW(focusw));
		gtk_text_buffer_delete_selection(buffer, TRUE, TRUE);
	}
}


void
on_preferences1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	prefs_show_dialog();
}


/* about menu item */
void
on_info1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	about_dialog_show();
}


/* open file */
void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_open_file();
}


/* quit toolbar button */
void
on_toolbutton19_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	on_exit_clicked(NULL, NULL);
}


/* reload file */
void
on_toolbutton23_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	on_reload_as_activate(NULL, GINT_TO_POINTER(-1));
}


/* also used for reloading when user_data is -1 */
void
on_reload_as_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *base_name;
	gint i = GPOINTER_TO_INT(user_data);
	gchar *charset = NULL;

	if (idx < 0 || ! doc_list[idx].is_valid || doc_list[idx].file_name == NULL)
		return;
	if (i >= 0)
	{
		if (i >= GEANY_ENCODINGS_MAX || encodings[i].charset == NULL) return;
		charset = encodings[i].charset;
	}

	base_name = g_path_get_basename(doc_list[idx].file_name);
	if (dialogs_show_question_full(NULL, _("_Reload"), GTK_STOCK_CANCEL,
		_("Any unsaved changes will be lost."),
		_("Are you sure you want to reload '%s'?"), base_name))
	{
		document_reload_file(idx, charset);
		if (charset != NULL)
			ui_update_statusbar(idx, -1);
	}
	g_free(base_name);
}


void
on_images_and_text2_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (ignore_toolbar_toggle) return;

	gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), GTK_TOOLBAR_BOTH);
	prefs.toolbar_icon_style = GTK_TOOLBAR_BOTH;
}


void
on_images_only2_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (ignore_toolbar_toggle) return;

	gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), GTK_TOOLBAR_ICONS);
	prefs.toolbar_icon_style = GTK_TOOLBAR_ICONS;
}


void
on_text_only2_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (ignore_toolbar_toggle) return;

	gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), GTK_TOOLBAR_TEXT);
	prefs.toolbar_icon_style = GTK_TOOLBAR_TEXT;
}


void
on_change_font1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_open_font();
}


/* new file */
void
on_toolbutton8_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	document_new_file(NULL, NULL, NULL);
}

/* open file */
void
on_toolbutton9_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	dialogs_show_open_file();
}


/* save file */
void
on_toolbutton10_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	on_save1_activate(NULL, user_data);
}


static void set_search_bar_background(gboolean success)
{
	const GdkColor red   = {0, 0xffff, 0x6666, 0x6666};
	const GdkColor white = {0, 0xffff, 0xffff, 0xffff};
	static gboolean old_value = TRUE;
	GtkWidget *widget = lookup_widget(app->window, "entry1");

	/* only update if really needed */
	if (search_data.search_bar && old_value != success)
	{
		gtk_widget_modify_base(widget, GTK_STATE_NORMAL, success ? NULL : &red);
		gtk_widget_modify_text(widget, GTK_STATE_NORMAL, success ? NULL : &white);

		old_value = success;
	}
}


/* store text, clear search flags so we can use Search->Find Next/Previous */
static void setup_find_next(GtkEditable *editable)
{
	g_free(search_data.text);
	search_data.text = gtk_editable_get_chars(editable, 0, -1);
	search_data.flags = 0;
	search_data.backwards = FALSE;
	search_data.search_bar = TRUE;
}


/* search text */
void
on_entry1_activate                     (GtkEntry        *entry,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gboolean result;

	setup_find_next(GTK_EDITABLE(entry));
	result = document_search_bar_find(idx, search_data.text, 0, FALSE);
	set_search_bar_background(result);
}


/* search text */
void
on_entry1_changed                      (GtkEditable     *editable,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gboolean result;

	setup_find_next(editable);
	result = document_search_bar_find(idx, search_data.text, 0, TRUE);
	set_search_bar_background(result);
}


/* search text */
void
on_toolbutton18_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gboolean result;
	GtkWidget *entry = lookup_widget(GTK_WIDGET(app->window), "entry1");

	setup_find_next(GTK_EDITABLE(entry));
	result = document_search_bar_find(idx, search_data.text, 0, FALSE);
	set_search_bar_background(result);
}


void
on_toolbar_large_icons1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (ignore_toolbar_toggle) return;

	prefs.toolbar_icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
	ui_update_toolbar_icons(GTK_ICON_SIZE_LARGE_TOOLBAR);
}


void
on_toolbar_small_icons1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (ignore_toolbar_toggle) return;

	prefs.toolbar_icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;
	ui_update_toolbar_icons(GTK_ICON_SIZE_SMALL_TOOLBAR);
}


/* hides toolbar from toolbar popup menu */
void
on_hide_toolbar1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *tool_item = lookup_widget(GTK_WIDGET(app->window), "menu_show_toolbar1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(tool_item), FALSE);
}


/* zoom in from menu bar and popup menu */
void
on_zoom_in1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	static gboolean done = 1;

	if (idx >= 0 && doc_list[idx].is_valid)
	{
		if (done++ % 3 == 0) sci_set_line_numbers(doc_list[idx].sci, editor_prefs.show_linenumber_margin,
				(sci_get_zoom(doc_list[idx].sci) / 2));
		sci_zoom_in(doc_list[idx].sci);
	}
}

/* zoom out from menu bar and popup menu */
void
on_zoom_out1_activate                   (GtkMenuItem     *menuitem,
                                         gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx >= 0 && doc_list[idx].is_valid)
	{
		if (sci_get_zoom(doc_list[idx].sci) == 0)
			sci_set_line_numbers(doc_list[idx].sci, editor_prefs.show_linenumber_margin, 0);
		sci_zoom_out(doc_list[idx].sci);
	}
}


void
on_normal_size1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx >= 0 && doc_list[idx].is_valid)
	{
		sci_zoom_off(doc_list[idx].sci);
		sci_set_line_numbers(doc_list[idx].sci, editor_prefs.show_linenumber_margin, 0);
	}
}


/* close tab */
void
on_toolbutton15_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
	document_remove(cur_page);
}


void
on_notebook1_switch_page               (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
	callbacks_data.last_doc_idx = document_get_cur_idx();
}


/* changes window-title on switching tabs and lots of other things */
void
on_notebook1_switch_page_after         (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
	gint idx;

	if (main_status.opening_session_files || main_status.closing_all)
		return;

	/* guint == -1 seems useless, but it isn't! */
	if (page_num == (guint) -1 && page != NULL)
		idx = document_find_by_sci(SCINTILLA(page));
	else
		idx = document_get_n_idx(page_num);

	if (idx >= 0)
	{
		treeviews_select_openfiles_item(idx);
		document_set_text_changed(idx);	/* also sets window title and status bar */
		ui_update_popup_reundo_items(idx);
		ui_document_show_hide(idx); /* update the document menu */
		build_menu_update(idx);
		treeviews_update_tag_list(idx, FALSE);

		utils_check_disk_status(idx, FALSE);

#ifdef HAVE_VTE
		vte_cwd(doc_list[idx].file_name, FALSE);
#endif

		if (geany_object)
		{
			g_signal_emit_by_name(geany_object, "document-activate", idx);
		}
	}
}


void
on_tv_notebook_switch_page             (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
	/* suppress selection changed signal when switching to the open files list */
	app->ignore_callback = TRUE;
}


void
on_tv_notebook_switch_page_after       (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
	app->ignore_callback = FALSE;
}


void
on_crlf_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (app->ignore_callback || idx == -1 || ! doc_list[idx].is_valid) return;
	sci_convert_eols(doc_list[idx].sci, SC_EOL_CRLF);
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_CRLF);
}


void
on_lf_activate                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (app->ignore_callback || idx == -1 || ! doc_list[idx].is_valid) return;
	sci_convert_eols(doc_list[idx].sci, SC_EOL_LF);
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_LF);
}


void
on_cr_activate                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (app->ignore_callback || idx == -1 || ! doc_list[idx].is_valid) return;
	sci_convert_eols(doc_list[idx].sci, SC_EOL_CR);
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_CR);
}


void
on_replace_tabs_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	document_replace_tabs(idx);
}


gboolean
toolbar_popup_menu                     (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{
	if (event->button == 3)
	{
		GtkWidget *w;

		ignore_toolbar_toggle = TRUE;

		switch (prefs.toolbar_icon_style)
		{
			case 0: w = lookup_widget(ui_widgets.toolbar_menu, "images_only2"); break;
			case 1: w = lookup_widget(ui_widgets.toolbar_menu, "text_only2"); break;
			default: w = lookup_widget(ui_widgets.toolbar_menu, "images_and_text2"); break;
		}
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), TRUE);

		switch (prefs.toolbar_icon_size)
		{
			case GTK_ICON_SIZE_LARGE_TOOLBAR:
					w = lookup_widget(ui_widgets.toolbar_menu, "large_icons1"); break;
			default: w = lookup_widget(ui_widgets.toolbar_menu, "small_icons1"); break;
		}
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w), TRUE);

		ignore_toolbar_toggle = FALSE;

		gtk_menu_popup(GTK_MENU(ui_widgets.toolbar_menu), NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}
	return FALSE;
}


void on_toggle_case1_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gint idx = document_get_cur_idx();
	ScintillaObject *sci = doc_list[idx].sci;
	gchar *text;
	gboolean keep_sel = TRUE;

	if (! DOC_IDX_VALID(idx))
		return;

	if (! sci_can_copy(sci))
	{
		keybindings_send_command(GEANY_KEY_GROUP_SELECT, GEANY_KEYS_SELECT_WORD);
		keep_sel = FALSE;
	}

	if (sci_can_copy(sci))
	{
		gchar *result = NULL;
		gint cmd = SCI_LOWERCASE;
		gint text_len = sci_get_selected_text_length(sci);
		gboolean rectsel = scintilla_send_message(sci, SCI_SELECTIONISRECTANGLE, 0, 0);

		text = g_malloc(text_len + 1);
		sci_get_selected_text(sci, text);

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
				sci_set_selection_start(sci, sci_get_current_position(sci) - text_len + 1);
		}
		else
			sci_cmd(sci, cmd);

		g_free(text);

	}
}


void
on_show_toolbar1_toggled               (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (app->ignore_callback) return;

	prefs.toolbar_visible = (prefs.toolbar_visible) ? FALSE : TRUE;;
	ui_widget_show_hide(GTK_WIDGET(app->toolbar), prefs.toolbar_visible);
}


void
on_fullscreen1_toggled                 (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	ui_prefs.fullscreen = (ui_prefs.fullscreen) ? FALSE : TRUE;
	ui_set_fullscreen();
}


void
on_show_messages_window1_toggled       (GtkCheckMenuItem *checkmenuitem,
                                        gpointer          user_data)
{
	if (app->ignore_callback) return;

	ui_prefs.msgwindow_visible = (ui_prefs.msgwindow_visible) ? FALSE : TRUE;
	ui_widget_show_hide(lookup_widget(app->window, "scrolledwindow1"), ui_prefs.msgwindow_visible);
}


void
on_markers_margin1_toggled             (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	editor_prefs.show_markers_margin = (editor_prefs.show_markers_margin) ? FALSE : TRUE;
	ui_show_markers_margin();
}


void
on_show_line_numbers1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	editor_prefs.show_linenumber_margin = (editor_prefs.show_linenumber_margin) ? FALSE : TRUE;
	ui_show_linenumber_margin();
}


void
on_line_breaking1_toggled              (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (! app->ignore_callback)
	{
		gint idx = document_get_cur_idx();
		if (! DOC_IDX_VALID(idx)) return;
		document_set_line_wrapping(idx, ! doc_list[idx].line_wrapping);
	}
}


void
on_set_file_readonly1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (! app->ignore_callback)
	{
		gint idx = document_get_cur_idx();
		if (! DOC_IDX_VALID(idx)) return;
		doc_list[idx].readonly = ! doc_list[idx].readonly;
		sci_set_readonly(doc_list[idx].sci, doc_list[idx].readonly);
		ui_update_tab_status(idx);
		ui_update_statusbar(idx, -1);
	}
}


void
on_use_auto_indentation1_toggled       (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (! app->ignore_callback)
	{
		gint idx = document_get_cur_idx();
		if (! DOC_IDX_VALID(idx)) return;
		doc_list[idx].auto_indent = ! doc_list[idx].auto_indent;
	}
}


void
on_find_usage1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint flags, idx;
	gchar *search_text;

	idx = document_get_cur_idx();
	if (! DOC_IDX_VALID(idx)) return;

	if (sci_can_copy(doc_list[idx].sci))
	{	/* take selected text if there is a selection */
		search_text = g_malloc(sci_get_selected_text_length(doc_list[idx].sci) + 1);
		sci_get_selected_text(doc_list[idx].sci, search_text);
		flags = SCFIND_MATCHCASE;
	}
	else
	{
		search_text = g_strdup(editor_info.current_word);
		flags = SCFIND_MATCHCASE | SCFIND_WHOLEWORD;
	}

	search_find_usage(search_text, flags, TRUE);
	g_free(search_text);
}


void
on_goto_tag_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gboolean definition = (menuitem ==
		GTK_MENU_ITEM(lookup_widget(app->popup_menu, "goto_tag_definition1")));
	document *doc = document_get_current();

	g_return_if_fail(doc);

	sci_set_current_position(doc->sci, editor_info.click_pos, FALSE);
	symbols_goto_tag(editor_info.current_word, definition);
}


void
on_count_words1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	tools_word_count();
}


void
on_show_color_chooser1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar colour[9];
	gint idx = document_get_cur_idx();
	gint pos = sci_get_current_position(doc_list[idx].sci);

	if (idx == -1 || ! doc_list[idx].is_valid)
		return;

	editor_find_current_word(doc_list[idx].sci, pos, colour, sizeof colour, GEANY_WORDCHARS"#");
	tools_color_chooser(colour);
}


void
on_compile_button_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_COMPILE);
}


void
on_find1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	search_show_find_dialog();
}


static void find_again(gboolean change_direction)
{
	gint idx = document_get_cur_idx();

	g_return_if_fail(DOC_IDX_VALID(idx));

	if (search_data.text)
	{
		gboolean forward = ! search_data.backwards;
		gint result = document_find_text(idx, search_data.text, search_data.flags,
			change_direction ? forward : !forward, FALSE, NULL);

		if (result > -1)
			editor_display_current_line(idx, 0.3F);

		set_search_bar_background((result > -1) ? TRUE : FALSE);
	}
}


void
on_find_next1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	find_again(FALSE);
}


void
on_find_previous1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (search_data.flags & SCFIND_REGEXP)
		/* Can't reverse search order for a regex (find next ignores search backwards) */
		utils_beep();
	else
		find_again(TRUE);
}


void
on_find_nextsel1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	search_find_selection(document_get_cur_idx(), FALSE);
}


void
on_find_prevsel1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	search_find_selection(document_get_cur_idx(), TRUE);
}


void
on_replace1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	search_show_replace_dialog();
}


void
on_find_in_files1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	search_show_find_in_files_dialog(NULL);
}


void
on_toolbutton_new_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	document_new_file(NULL, NULL, NULL);
}


void
on_go_to_line_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_goto_line();
}


void
on_goto_line_dialog_response         (GtkDialog *dialog,
                                      gint response,
                                      gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gint idx = document_get_cur_idx();
		gint line = strtol(gtk_entry_get_text(GTK_ENTRY(user_data)), NULL, 10);

		if (line > 0 && line <= sci_get_line_count(doc_list[idx].sci))
		{
			utils_goto_line(idx, line);
		}
		else
		{
			utils_beep();
		}

	}
	if (dialog) gtk_widget_destroy(GTK_WIDGET(dialog));
}


void
on_goto_line_entry_activate          (GtkEntry        *entry,
                                      gpointer         user_data)
{
	on_goto_line_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_ACCEPT, entry);
}


void
on_entry_goto_line_activate            (GtkEntry        *entry,
                                        gpointer         user_data)
{
	on_goto_line_dialog_response(NULL, GTK_RESPONSE_ACCEPT, entry);
}


void
on_toolbutton_goto_clicked             (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	on_goto_line_dialog_response(NULL, GTK_RESPONSE_ACCEPT,
			lookup_widget(app->window, "entry_goto_line"));
}


void
on_help1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint skip;
	gchar *uri;

#ifdef G_OS_WIN32
	skip = 8;
	uri = g_strconcat("file:///", app->docdir, "/Manual.html", NULL);
	g_strdelimit(uri, "\\", '/'); /* replace '\\' by '/' */
#else
	skip = 7;
	uri = g_strconcat("file://", app->docdir, "index.html", NULL);
#endif

	if (! g_file_test(uri + skip, G_FILE_TEST_IS_REGULAR))
	{	/* fall back to online documentation if it is not found on the hard disk */
		g_free(uri);
		uri = g_strconcat(GEANY_HOMEPAGE, "manual/", VERSION, "/index.html", NULL);
	}

	utils_start_browser(uri);
	g_free(uri);
}


void
on_help_shortcuts1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	keybindings_show_shortcuts();
}


void
on_website1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	utils_start_browser(GEANY_HOMEPAGE);
}


void
on_comments_function_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;
	const gchar *cur_tag = NULL;
	gint line = -1, pos = 0;

	if (doc_list[idx].file_type == NULL)
	{
		ui_set_statusbar(FALSE, _("Please set the filetype for the current file before using this function."));
		return;
	}

	/* utils_get_current_function returns -1 on failure, so sci_get_position_from_line
	 * returns the current position, so it should be safe */
	line = utils_get_current_function(idx, &cur_tag);
	pos = sci_get_position_from_line(doc_list[idx].sci, line - 1);

	text = templates_get_template_function(doc_list[idx].file_type->id, cur_tag);

	sci_insert_text(doc_list[idx].sci, pos, text);
	g_free(text);
}


void
on_comments_multiline_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx) || doc_list[idx].file_type == NULL)
	{
		ui_set_statusbar(FALSE, _("Please set the filetype for the current file before using this function."));
		return;
	}

	verify_click_pos(idx); /* make sure that the click_pos is valid */

	editor_insert_multiline_comment(idx);
}


void
on_comments_gpl_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;

	text = templates_get_template_licence(FILETYPE_ID(doc_list[idx].file_type), GEANY_TEMPLATE_GPL);

	verify_click_pos(idx); /* make sure that the click_pos is valid */

	sci_insert_text(doc_list[idx].sci, editor_info.click_pos, text);
	g_free(text);
}


void
on_comments_bsd_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{

	gint idx = document_get_cur_idx();
	gchar *text;

	text = templates_get_template_licence(FILETYPE_ID(doc_list[idx].file_type), GEANY_TEMPLATE_BSD);

	verify_click_pos(idx); /* make sure that the click_pos is valid */

	sci_insert_text(doc_list[idx].sci, editor_info.click_pos, text);
	g_free(text);

}


void
on_comments_changelog_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;

	text = templates_get_template_changelog();
	sci_insert_text(doc_list[idx].sci, 0, text);
	/* sets the cursor to the right position to type the changelog text,
	 * the template has 21 chars + length of name and email */
	sci_goto_pos(doc_list[idx].sci, 21 + strlen(prefs.template_developer) + strlen(prefs.template_mail), TRUE);

	g_free(text);
}


void
on_comments_fileheader_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;
	gchar *fname;
	filetype *ft;

	g_return_if_fail(DOC_IDX_VALID(idx));

	ft = doc_list[idx].file_type;
	fname = doc_list[idx].file_name;
	text = templates_get_template_fileheader(FILETYPE_ID(ft), fname);

	sci_insert_text(doc_list[idx].sci, 0, text);
	sci_goto_pos(doc_list[idx].sci, 0, FALSE);
	g_free(text);
}


static void
on_custom_date_input_response(const gchar *input)
{
	setptr(ui_prefs.custom_date_format, g_strdup(input));
}


void
on_insert_date_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *format;
	gchar time_str[300]; /* the entered format string can be maximal 256 chars long, so we have
						  * 44 additional characters for strtime's conversion */
	time_t t;
	struct tm *tm;

	if (idx < 0 || ! doc_list[idx].is_valid) return;

	if (utils_str_equal(_("dd.mm.yyyy"), (gchar*) user_data))
		format = "%d.%m.%Y";
	else if (utils_str_equal(_("mm.dd.yyyy"), (gchar*) user_data))
		format = "%m.%d.%Y";
	else if (utils_str_equal(_("yyyy/mm/dd"), (gchar*) user_data))
		format = "%Y/%m/%d";
	else if (utils_str_equal(_("dd.mm.yyyy hh:mm:ss"), (gchar*) user_data))
		format = "%d.%m.%Y %H:%M:%S";
	else if (utils_str_equal(_("mm.dd.yyyy hh:mm:ss"), (gchar*) user_data))
		format = "%m.%d.%Y %H:%M:%S";
	else if (utils_str_equal(_("yyyy/mm/dd hh:mm:ss"), (gchar*) user_data))
		format = "%Y/%m/%d %H:%M:%S";
	else if (utils_str_equal(_("_Use Custom Date Format"), (gchar*) user_data))
		format = ui_prefs.custom_date_format;
	else
	{
		/* set default value */
		if (utils_str_equal("", ui_prefs.custom_date_format))
		{
			g_free(ui_prefs.custom_date_format);
			ui_prefs.custom_date_format = g_strdup("%d.%m.%Y");
		}

		dialogs_show_input(_("Custom Date Format"),
			_("Enter here a custom date and time format. You can use any conversion specifiers which can be used with the ANSI C strftime function."),
			ui_prefs.custom_date_format, FALSE, &on_custom_date_input_response);
		return;
	}

	/* get the current time */
	t = time(NULL);
	tm = localtime(&t);
	if (strftime(time_str, sizeof time_str, format, tm) != 0)
	{
		verify_click_pos(idx); /* make sure that the click_pos is valid */

		sci_insert_text(doc_list[idx].sci, editor_info.click_pos, time_str);
		sci_goto_pos(doc_list[idx].sci, editor_info.click_pos + strlen(time_str), FALSE);
	}
	else
	{
		utils_beep();
		ui_set_statusbar(TRUE,
				_("Date format string could not be converted (possibly too long)."));
	}
}


void
on_insert_include_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gint pos = -1;
	gchar *text;

	if (! DOC_IDX_VALID(idx) || user_data == NULL) return;

	verify_click_pos(idx); /* make sure that the click_pos is valid */

	if (utils_str_equal(user_data, "blank"))
	{
		text = g_strdup("#include \"\"\n");
		pos = editor_info.click_pos + 10;
	}
	else
	{
		text = g_strconcat("#include <", user_data, ">\n", NULL);
	}

	sci_insert_text(doc_list[idx].sci, editor_info.click_pos, text);
	g_free(text);
	if (pos >= 0)
		sci_goto_pos(doc_list[idx].sci, pos, FALSE);
}


void
on_file_properties_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	dialogs_show_file_properties(idx);
}


void
on_menu_fold_all1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	document_fold_all(idx);
}


void
on_menu_unfold_all1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	document_unfold_all(idx);
}


void
on_run_button_clicked                  (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_BUILD, GEANY_KEYS_BUILD_RUN);
}


void
on_go_to_line1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	/* this is search menu cb; call popup menu goto cb */
	on_go_to_line_activate(menuitem, user_data);
}


void
on_menu_remove_indicators1_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx))
		return;
	document_clear_indicators(idx);
}


void
on_encoding_change                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	guint i = GPOINTER_TO_INT(user_data);

	if (app->ignore_callback || ! DOC_IDX_VALID(idx) || encodings[i].charset == NULL ||
		utils_str_equal(encodings[i].charset, doc_list[idx].encoding)) return;

	if (doc_list[idx].readonly)
	{
		utils_beep();
		return;
	}
	document_undo_add(idx, UNDO_ENCODING, g_strdup(doc_list[idx].encoding));

	document_set_encoding(idx, encodings[i].charset);
}


void
on_print1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (! DOC_IDX_VALID(idx))
		return;
	printing_print_doc(idx);
}


void
on_menu_select_all1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (idx < 0 || ! doc_list[idx].is_valid) return;

	sci_select_all(doc_list[idx].sci);
}


void
on_menu_show_sidebar1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	static gint active_page = -1;

	if (app->ignore_callback) return;

	if (ui_prefs.sidebar_visible)
	{
		/* to remember the active page because GTK (e.g. 2.8.18) doesn't do it and shows always
		 * the last page (for unknown reason, with GTK 2.6.4 it works) */
		active_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->treeview_notebook));
	}

	ui_prefs.sidebar_visible = ! ui_prefs.sidebar_visible;

	if ((! prefs.sidebar_openfiles_visible && ! prefs.sidebar_symbol_visible))
	{
		prefs.sidebar_openfiles_visible = TRUE;
		prefs.sidebar_symbol_visible = TRUE;
	}

	ui_sidebar_show_hide();
	gtk_notebook_set_current_page(GTK_NOTEBOOK(app->treeview_notebook), active_page);
}


void
on_menu_write_unicode_bom1_toggled     (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (! app->ignore_callback)
	{
		gint idx = document_get_cur_idx();

		if (idx == -1 || ! doc_list[idx].is_valid) return;
		if (doc_list[idx].readonly)
		{
			utils_beep();
			return;
		}

		document_undo_add(idx, UNDO_BOM, GINT_TO_POINTER(doc_list[idx].has_bom));

		doc_list[idx].has_bom = ! doc_list[idx].has_bom;

		ui_update_statusbar(idx, -1);
	}
}


void
on_menu_comment_line1_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	editor_do_comment(idx, -1, FALSE, FALSE);
}


void
on_menu_uncomment_line1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	editor_do_uncomment(idx, -1, FALSE);
}


void
on_menu_toggle_line_commentation1_activate
                                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	editor_do_comment_toggle(idx);
}


void
on_menu_duplicate_line1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	keybindings_send_command(GEANY_KEY_GROUP_EDITOR, GEANY_KEYS_EDITOR_DUPLICATELINE);
}


void
on_menu_increase_indent1_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (! DOC_IDX_VALID(idx)) return;

	if (sci_get_lines_selected(doc_list[idx].sci) > 1)
	{
		sci_cmd(doc_list[idx].sci, SCI_TAB);
	}
	else
	{
		gint line, ind_pos, old_pos, new_pos, step;

		old_pos = sci_get_current_position(doc_list[idx].sci);
		line = sci_get_line_from_position(doc_list[idx].sci, old_pos);
		ind_pos = sci_get_line_indent_position(doc_list[idx].sci, line);
		/* when using tabs increase cur pos by 1, when using space increase it by tab_width */
		step = (doc_list[idx].use_tabs) ? 1 : editor_prefs.tab_width;
		new_pos = (old_pos > ind_pos) ? old_pos + step : old_pos;

		sci_set_current_position(doc_list[idx].sci, ind_pos, TRUE);
		sci_cmd(doc_list[idx].sci, SCI_TAB);
		sci_set_current_position(doc_list[idx].sci, new_pos, TRUE);
	}
}


void
on_menu_decrease_indent1_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (! DOC_IDX_VALID(idx)) return;

	if (sci_get_lines_selected(doc_list[idx].sci) > 1)
	{
		sci_cmd(doc_list[idx].sci, SCI_BACKTAB);
	}
	else
	{
		gint line, ind_pos, old_pos, new_pos, step, indent;

		old_pos = sci_get_current_position(doc_list[idx].sci);
		line = sci_get_line_from_position(doc_list[idx].sci, old_pos);
		ind_pos = sci_get_line_indent_position(doc_list[idx].sci, line);
		step = (doc_list[idx].use_tabs) ? 1 : editor_prefs.tab_width;
		new_pos = (old_pos >= ind_pos) ? old_pos - step : old_pos;

		if (ind_pos == sci_get_position_from_line(doc_list[idx].sci, line))
			return;

		sci_set_current_position(doc_list[idx].sci, ind_pos, TRUE);
		indent = sci_get_line_indentation(doc_list[idx].sci, line);
		indent -= editor_prefs.tab_width;
		if (indent < 0)
			indent = 0;
		sci_set_line_indentation(doc_list[idx].sci, line, indent);

		sci_set_current_position(doc_list[idx].sci, new_pos, TRUE);
	}
}


void
on_next_message1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	if (! ui_tree_view_find_next(GTK_TREE_VIEW(msgwindow.tree_msg),
		msgwin_goto_messages_file_line))
		ui_set_statusbar(FALSE, _("No more message items."));
}


void
on_menu_comments_multiline_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	insert_callback_from_menu = TRUE;
	on_comments_multiline_activate(menuitem, user_data);
}


void
on_menu_comments_gpl_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	insert_callback_from_menu = TRUE;
	on_comments_gpl_activate(menuitem, user_data);
}


void
on_menu_comments_bsd_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	insert_callback_from_menu = TRUE;
	on_comments_bsd_activate(menuitem, user_data);
}


void
on_menu_insert_include_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	insert_callback_from_menu = TRUE;
	on_insert_include_activate(menuitem, user_data);
}


void
on_menu_insert_date_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	insert_callback_from_menu = TRUE;
	on_insert_date_activate(menuitem, user_data);
}


void
on_project_new1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	project_new();
}


void
on_project_open1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	project_open();
}


void
on_project_close1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	project_close(TRUE);
}


void
on_project_properties1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	project_properties();
}


void
on_menu_project1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	static GtkWidget *item_close = NULL;
	static GtkWidget *item_properties = NULL;

	if (item_close == NULL)
	{
		item_close = lookup_widget(app->window, "project_close1");
		item_properties = lookup_widget(app->window, "project_properties1");
	}

	gtk_widget_set_sensitive(item_close, (app->project != NULL));
	gtk_widget_set_sensitive(item_properties, (app->project != NULL));
}


void
on_menu_open_selected_file1_activate   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	if (! DOC_IDX_VALID(idx)) return;

	sel = editor_get_default_selection(idx, TRUE, GEANY_WORDCHARS"./");

	if (sel != NULL)
	{
		gchar *locale_filename, *filename;

		if (g_path_is_absolute(sel))
			filename = g_strdup(sel);
		else
		{	/* relative filename, add the path of the current file */
			gchar *path;

			path = g_path_get_dirname(doc_list[idx].file_name);
			filename = g_build_path(G_DIR_SEPARATOR_S, path, sel, NULL);

			if (! g_file_test(filename, G_FILE_TEST_EXISTS) &&
				app->project != NULL && NZV(app->project->base_path))
			{
				/* try the project's base path */
				setptr(path, project_get_base_path());
				setptr(filename, g_build_path(G_DIR_SEPARATOR_S, path, sel, NULL));
			}
			g_free(path);
		}

		locale_filename = utils_get_locale_from_utf8(filename);
		document_open_file(locale_filename, FALSE, NULL, NULL);

		g_free(filename);
		g_free(locale_filename);
		g_free(sel);
	}
}


void
on_remove_markers1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (! DOC_IDX_VALID(idx))
		return;

	sci_marker_delete_all(doc_list[idx].sci, 0);	/* delete the yellow tag marker */
	sci_marker_delete_all(doc_list[idx].sci, 1);	/* delete user markers */
}


void
on_load_tags1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	symbols_show_load_tags_dialog();
}


void
on_context_action1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx;
	gchar *word, *command;
	GError *error = NULL;

	idx = document_get_cur_idx();
	if (! DOC_IDX_VALID(idx)) return;

	if (sci_can_copy(doc_list[idx].sci))
	{	/* take selected text if there is a selection */
		word = g_malloc(sci_get_selected_text_length(doc_list[idx].sci) + 1);
		sci_get_selected_text(doc_list[idx].sci, word);
	}
	else
	{
		word = g_strdup(editor_info.current_word);
	}

	/* use the filetype specific command if available, fallback to global command otherwise */
	if (doc_list[idx].file_type != NULL &&
		doc_list[idx].file_type->context_action_cmd != NULL &&
		*doc_list[idx].file_type->context_action_cmd != '\0')
	{
		command = g_strdup(doc_list[idx].file_type->context_action_cmd);
	}
	else
	{
		command = g_strdup(prefs.context_action_cmd);
	}

	/* substitute the wildcard %s and run the command if it is non empty */
	if (command != NULL && *command != '\0')
	{
		command = utils_str_replace(command, "%s", word);

		if (! g_spawn_command_line_async(command, &error))
		{
			ui_set_statusbar(TRUE, "Context action command failed: %s", error->message);
			g_error_free(error);
		}
	}
	g_free(word);
	g_free(command);
}


void
on_menu_toggle_all_additional_widgets1_activate
                                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	static gint hide_all = -1;
	GtkCheckMenuItem *msgw = GTK_CHECK_MENU_ITEM(
		lookup_widget(app->window, "menu_show_messages_window1"));
	GtkCheckMenuItem *toolbari = GTK_CHECK_MENU_ITEM(
		lookup_widget(app->window, "menu_show_toolbar1"));

	/* get the initial state (necessary if Geany was closed with hide_all = TRUE) */
	if (hide_all == -1)
	{
		if (! gtk_check_menu_item_get_active(msgw) &&
			! prefs.show_notebook_tabs &&
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

		prefs.show_notebook_tabs = FALSE;
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(app->notebook), prefs.show_notebook_tabs);

		ui_statusbar_showhide(FALSE);

		if (gtk_check_menu_item_get_active(toolbari))
			gtk_check_menu_item_set_active(toolbari, ! gtk_check_menu_item_get_active(toolbari));
	}
	else
	{

		if (! gtk_check_menu_item_get_active(msgw))
			gtk_check_menu_item_set_active(msgw, ! gtk_check_menu_item_get_active(msgw));

		prefs.show_notebook_tabs = TRUE;
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(app->notebook), prefs.show_notebook_tabs);

		ui_statusbar_showhide(TRUE);

		if (! gtk_check_menu_item_get_active(toolbari))
			gtk_check_menu_item_set_active(toolbari, ! gtk_check_menu_item_get_active(toolbari));
	}
}


void
on_forward_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	navqueue_go_forward();
}


void
on_back_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	navqueue_go_back();
}


gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	if (prefs.auto_focus && ! GTK_WIDGET_HAS_FOCUS(widget))
		gtk_widget_grab_focus(widget);

	return FALSE;
}


void
on_tabs1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	document_set_use_tabs(idx, TRUE);
	ui_update_statusbar(idx, -1);
}


void
on_spaces1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	document_set_use_tabs(idx, FALSE);
	ui_update_statusbar(idx, -1);
}


void
on_strip_trailing_spaces1_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	document_strip_trailing_spaces(idx);
}


void
on_page_setup1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
#if GTK_CHECK_VERSION(2, 10, 0)
	printing_page_setup_gtk();
#endif
}


void
on_tools1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
#ifdef HAVE_PLUGINS
	plugins_update_tools_menu();
#endif
}


gboolean
on_escape_key_press_event              (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
	/* make pressing escape in the sidebar and toolbar focus the editor */
	if (event->keyval == GDK_Escape && event->state == 0)
	{
		keybindings_send_command(GEANY_KEY_GROUP_FOCUS, GEANY_KEYS_FOCUS_EDITOR);
		return TRUE;
	}
	return FALSE;
}
