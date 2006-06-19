/*
 *      callbacks.c - this file is part of Geany, a fast and lightweight IDE
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


#include "geany.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <gdk/gdkkeysyms.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#include "highlighting.h"
#include "keyfile.h"
#include "document.h"
#include "sciwrappers.h"
#include "sci_cb.h"
#include "utils.h"
#include "dialogs.h"
#include "about.h"
#include "msgwindow.h"
#include "build.h"
#include "prefs.h"
#include "templates.h"
#include "treeviews.h"
#include "keybindings.h"


#ifdef GEANY_WIN32
# include "win32.h"
#endif

// include vte.h on non-Win32 systems, else define fake vte_init
#if defined(GEANY_WIN32) || ! defined(HAVE_VTE)
# define vte_close() ;
#else
# include "vte.h"
#endif



// represents the word under the mouse pointer when right button(no. 3) is pressed
gchar current_word[GEANY_MAX_WORD_LENGTH];

// represents the state while closing all tabs(used to prevent notebook switch page signals)
static gboolean closing_all = FALSE;

// represents the state at switching a notebook page, to update check menu items in document menu
static gboolean switch_notebook_page = FALSE;

// represents the state at switching a notebook page(in the left treeviews widget), to not emit
// the selection-changed signal from tv.tree_openfiles
//static gboolean switch_tv_notebook_page = FALSE;

// the flags given in the search dialog(stored statically for "find next" and "replace")
static gint search_flags;
static gboolean search_backwards;
static gint search_flags_re;
static gboolean search_backwards_re;



void signal_cb(gint sig)
{
	if (sig == SIGTERM)
	{
		on_exit_clicked(NULL, NULL);
	}
}


// real exit function
gint destroyapp(GtkWidget *widget, gpointer gdata)
{
	geany_debug("Quitting...");

#ifdef HAVE_FIFO
	if (! app->ignore_fifo)
	{
		gchar *fifo = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, GEANY_FIFO_NAME, NULL);
		// delete the fifo early, because we don't accept new files anymore
		if (app->fifo_ioc != NULL)
		{
			g_io_channel_unref(app->fifo_ioc);
			g_io_channel_shutdown(app->fifo_ioc, FALSE, NULL);
		}
		unlink(fifo);
		g_free(fifo);
	}
#endif

	keybindings_free();
	filetypes_free_types();
	styleset_free_styles();
	templates_free_templates();
	tm_workspace_free(TM_WORK_OBJECT(app->tm_workspace));
	g_free(app->configdir);
	g_free(app->search_text);
	g_free(app->editor_font);
	g_free(app->tagbar_font);
	g_free(app->msgwin_font);
	g_free(app->long_line_color);
	g_free(app->pref_template_developer);
	g_free(app->pref_template_company);
	g_free(app->pref_template_mail);
	g_free(app->pref_template_initial);
	g_free(app->pref_template_version);
	g_free(app->build_make_cmd);
	g_free(app->build_term_cmd);
	g_free(app->build_browser_cmd);
	while (! g_queue_is_empty(app->recent_queue))
	{
		g_free(g_queue_pop_tail(app->recent_queue));
	}
	g_queue_free(app->recent_queue);

	if (app->prefs_dialog && GTK_IS_WIDGET(app->prefs_dialog)) gtk_widget_destroy(app->prefs_dialog);
	if (app->find_dialog && GTK_IS_WIDGET(app->find_dialog)) gtk_widget_destroy(app->find_dialog);
	if (app->replace_dialog && GTK_IS_WIDGET(app->replace_dialog)) gtk_widget_destroy(app->replace_dialog);
	if (app->save_filesel && GTK_IS_WIDGET(app->save_filesel)) gtk_widget_destroy(app->save_filesel);
	if (app->open_filesel && GTK_IS_WIDGET(app->open_filesel)) gtk_widget_destroy(app->open_filesel);
	if (app->open_fontsel && GTK_IS_WIDGET(app->open_fontsel)) gtk_widget_destroy(app->open_fontsel);
	if (app->open_colorsel && GTK_IS_WIDGET(app->open_colorsel)) gtk_widget_destroy(app->open_colorsel);
	if (app->default_tag_tree && GTK_IS_WIDGET(app->default_tag_tree))
	{
		g_object_unref(app->default_tag_tree);
		gtk_widget_destroy(app->default_tag_tree);
	}
	scintilla_release_resources();
#ifdef HAVE_VTE
	if (app->have_vte) vte_close();
	g_free(app->lib_vte);
#endif
	gtk_widget_destroy(app->window);

	// destroy popup menus
	if (app->popup_menu && GTK_IS_WIDGET(app->popup_menu))
					gtk_widget_destroy(app->popup_menu);
	if (app->toolbar_menu && GTK_IS_WIDGET(app->toolbar_menu))
					gtk_widget_destroy(app->toolbar_menu);
	if (tv.popup_taglist && GTK_IS_WIDGET(tv.popup_taglist))
					gtk_widget_destroy(tv.popup_taglist);
	if (tv.popup_openfiles && GTK_IS_WIDGET(tv.popup_openfiles))
					gtk_widget_destroy(tv.popup_openfiles);
	if (msgwindow.popup_status_menu && GTK_IS_WIDGET(msgwindow.popup_status_menu))
					gtk_widget_destroy(msgwindow.popup_status_menu);
	if (msgwindow.popup_msg_menu && GTK_IS_WIDGET(msgwindow.popup_msg_menu))
					gtk_widget_destroy(msgwindow.popup_msg_menu);
	if (msgwindow.popup_compiler_menu && GTK_IS_WIDGET(msgwindow.popup_compiler_menu))
					gtk_widget_destroy(msgwindow.popup_compiler_menu);

	g_free(app);

	gtk_main_quit();
	return (FALSE);
}


// wrapper function to abort exit process if cancel button is pressed
gboolean
on_exit_clicked                        (GtkWidget *widget, gpointer gdata)
{
	app->quitting = TRUE;
	configuration_save();
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) > 0)
	{
		gint i;
		gboolean has_dirty_editors = FALSE;

		for (i = 0; i < GEANY_MAX_OPEN_FILES; i++)
		{
			if (doc_list[i].is_valid && doc_list[i].changed)
			{
				has_dirty_editors = TRUE;
				break;
			}
		}
		if (has_dirty_editors)
		{
			if (on_close_all1_activate(NULL, NULL)) destroyapp(NULL, gdata);
			else app->quitting = FALSE;
		}
		else
		{
			if (app->pref_main_confirm_exit)
			{
				if (dialogs_show_question(_("Do you really want to quit?")) &&
					on_close_all1_activate(NULL, NULL)) destroyapp(NULL, gdata);
				else app->quitting = FALSE;
			}
			else
			{
				if (on_close_all1_activate(NULL, NULL)) destroyapp(NULL, gdata);
				else app->quitting = FALSE;
			}
		}
	}
	else
	{
		if (app->pref_main_confirm_exit)
		{
			if (dialogs_show_question(_("Do you really want to quit?"))) destroyapp(NULL, gdata);
			else app->quitting = FALSE;
		}
		else
		{
			destroyapp(NULL, gdata);
		}
	}
	return TRUE;
}


/*
 * GUI callbacks
 */

void
on_new1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	document_new_file(NULL);
}


void
on_new_with_template                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	document_new_file((filetype*) user_data);
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
			document_save_file(idx);
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
	for(i = 0; i < max; i++)
	{
		idx = document_get_n_idx(i);
		if (! doc_list[idx].changed) continue;
		if (doc_list[idx].file_name == NULL)
			dialogs_show_save_as();
		else
			document_save_file(idx);
	}
	utils_update_tag_list(cur_idx, TRUE);
	utils_set_window_title(cur_idx);
}


gboolean
on_close_all1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gboolean ret = TRUE;
	gint i, max = gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook));

	closing_all = TRUE;
	for(i = 0; i < max; i++)
	{
		if (! document_remove(0))
		{
			ret = FALSE;
			break;
		}
	}
	closing_all = FALSE;
	tm_workspace_update(TM_WORK_OBJECT(app->tm_workspace), TRUE, TRUE, FALSE);
	// if cancel is clicked, cancel the complete exit process
	return ret;
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


// edit actions, c&p & co, from menu bar and from popup menu
void
on_edit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	utils_update_menu_copy_items(idx);
	utils_update_insert_include_item(idx, 1);
}


void
on_undo1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (sci_can_undo(doc_list[idx].sci)) sci_undo(doc_list[idx].sci);
}


void
on_redo1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (sci_can_redo(doc_list[idx].sci)) sci_redo(doc_list[idx].sci);
}


void
on_cut1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (idx >= 0)
		sci_cut(doc_list[idx].sci);
}


void
on_copy1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (idx >= 0)
		sci_copy(doc_list[idx].sci);
}


void
on_paste1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (idx >= 0)
		sci_paste(doc_list[idx].sci);
}


void
on_delete1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (idx >= 0)
		sci_clear(doc_list[idx].sci);
}


void
on_preferences1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_prefs_dialog();
}


// about menu item
void
on_info1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	about_dialog_show();
}


// open file
void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_open_file();
}


// quit toolbar button
void
on_toolbutton19_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	on_exit_clicked(NULL, NULL);
}


// reload file
void
on_toolbutton23_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *basename = g_path_get_basename(doc_list[idx].file_name);

	if (dialogs_show_question(_
				 ("Are you sure you want to reload '%s'?\nAny unsaved changes will be lost."),
				 basename))
	{
		document_open_file(idx, NULL, 0, doc_list[idx].readonly, doc_list[idx].file_type);
	}

	g_free(basename);
}


void
on_images_and_text2_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), GTK_TOOLBAR_BOTH);
	app->toolbar_icon_style = GTK_TOOLBAR_BOTH;
}


void
on_images_only2_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), GTK_TOOLBAR_ICONS);
	app->toolbar_icon_style = GTK_TOOLBAR_ICONS;
}


void
on_text_only2_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), GTK_TOOLBAR_TEXT);
	app->toolbar_icon_style = GTK_TOOLBAR_TEXT;
}


void
on_change_font1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_open_font();
}


// new file
void
on_toolbutton8_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	document_new_file(NULL);
}

// open file
void
on_toolbutton9_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	dialogs_show_open_file();
}


// save file
void
on_toolbutton10_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	on_save1_activate(NULL, user_data);
}


// search text
void
on_entry1_activate                     (GtkEntry        *entry,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	g_free(app->search_text);
	app->search_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	document_find_next(idx, app->search_text, 0, FALSE, FALSE);
}


// search text
void
on_entry1_changed                      (GtkEditable     *editable,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	g_free(app->search_text);
	app->search_text = g_strdup(gtk_editable_get_chars(editable, 0, -1));
	document_find_next(idx, app->search_text, 0, FALSE, TRUE);
}


// search text
void
on_toolbutton18_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	//on_entry1_changed(NULL, NULL);
	gint idx = document_get_cur_idx();
	GtkWidget *entry = lookup_widget(GTK_WIDGET(app->window), "entry1");

	g_free(app->search_text);
	app->search_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	document_find_next(idx, app->search_text, 0, TRUE, FALSE);
}


void
on_toolbar_large_icons1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	app->toolbar_icon_size = GTK_ICON_SIZE_LARGE_TOOLBAR;
	utils_update_toolbar_icons(GTK_ICON_SIZE_LARGE_TOOLBAR);
}


void
on_toolbar_small_icons1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	app->toolbar_icon_size = GTK_ICON_SIZE_SMALL_TOOLBAR;
	utils_update_toolbar_icons(GTK_ICON_SIZE_SMALL_TOOLBAR);
}


// hides toolbar from toolbar popup menu
void
on_hide_toolbar1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *tool_item = lookup_widget(GTK_WIDGET(app->window), "menu_show_toolbar1");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(tool_item), FALSE);
}


// zoom in from menu bar and popup menu
void
on_zoom_in1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	static gboolean done = 1;

	if (doc_list[idx].is_valid)
	{
		if (done++ % 3 == 0) sci_set_line_numbers(doc_list[idx].sci, app->show_linenumber_margin,
				(sci_get_zoom(doc_list[idx].sci) / 2));
		sci_zoom_in(doc_list[idx].sci);
	}
}

// zoom out from menu bar and popup menu
void
on_zoom_out1_activate                   (GtkMenuItem     *menuitem,
                                         gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].is_valid)
	{
		if (sci_get_zoom(doc_list[idx].sci) == 0)
			sci_set_line_numbers(doc_list[idx].sci, app->show_linenumber_margin, 0);
		sci_zoom_out(doc_list[idx].sci);
	}
}


void
on_normal_size1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (doc_list[idx].is_valid)
	{
		sci_zoom_off(doc_list[idx].sci);
		sci_set_line_numbers(doc_list[idx].sci, app->show_linenumber_margin, 0);
	}
}


// close tab
void
on_toolbutton15_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	gint cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(app->notebook));
	document_remove(cur_page);
}


// close tab
void
on_tab_close_clicked                (GtkButton   *button,
                                     gpointer     user_data)
{
	gint cur_page = gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook), GTK_WIDGET(user_data));
	document_remove(cur_page);
}


// changes window-title on switching tabs and lots of other things
void
on_notebook1_switch_page               (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
	gint idx;

	if (closing_all) return;

	// guint == -1 seems useless, but it isn't!
	if (page_num == -1 && page != NULL)
		idx = document_find_by_sci(SCINTILLA(page));
	else
		idx = document_get_n_idx(page_num);

	if (idx >= 0 && app->opening_session_files == FALSE)
	{
		switch_notebook_page = TRUE;
		gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_line_breaking1")),
				doc_list[idx].line_breaking);
		gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_use_auto_indention1")),
				doc_list[idx].use_auto_indention);
		gtk_check_menu_item_set_active(
				GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "set_file_readonly1")),
				doc_list[idx].readonly);

		gtk_tree_model_foreach(GTK_TREE_MODEL(tv.store_openfiles), treeviews_find_node, GINT_TO_POINTER(idx));

		document_set_text_changed(idx);
		utils_build_show_hide(idx);
		utils_update_statusbar(idx, -1);
		utils_set_window_title(idx);
		utils_update_tag_list(idx, FALSE);

		switch_notebook_page = FALSE;
	}
}


void
on_notebook1_switch_page_after         (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
	gint idx;

	if (closing_all) return;

	// guint == -1 seems useless, but it isn't!
	if (page_num == -1 && page != NULL)
		idx = document_find_by_sci(SCINTILLA(page));
	else
		idx = document_get_n_idx(page_num);

	if (idx >= 0 && app->opening_session_files == FALSE)
	{
		utils_check_disk_status(idx);
	}
}


void
on_tv_notebook_switch_page             (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data)
{
	//switch_tv_notebook_page = TRUE;
}


/*
 * open dialog callbacks
*/

// file open dialog, opened
void
on_file_open_dialog_response           (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	gtk_widget_hide(app->open_filesel);

	if (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_APPLY)
	{
		GSList *filelist;
		GSList *flist;
		gint ft_id = gtk_combo_box_get_active(GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(dialog), "filetype_combo")));
		filetype *ft = NULL;
		gboolean ro = (response == GTK_RESPONSE_APPLY);	// View clicked

		if (ft_id >= 0 && ft_id < GEANY_FILETYPES_ALL) ft = filetypes[ft_id];

		filelist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(app->open_filesel));
		flist = filelist;
		while(flist != NULL)
		{
			if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) < GEANY_MAX_OPEN_FILES)
			{
				if (g_file_test((gchar*) flist->data, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
				{
					document_open_file(-1, (gchar*) flist->data, 0, ro, ft);
				}
			}
			else
			{
				dialogs_show_file_open_error();
				g_slist_foreach(flist, (GFunc)g_free, NULL);
				break;
			}
			g_free(flist->data);
			flist = flist->next;
		}
		g_slist_free(filelist);
	}
}


// callback for the text entry for typing in filename
void
on_file_open_entry_activate            (GtkEntry        *entry,
                                        gpointer         user_data)
{
	gchar *locale_filename = g_locale_from_utf8(gtk_entry_get_text(entry), -1, NULL, NULL, NULL);
	if (locale_filename == NULL) locale_filename = g_strdup(gtk_entry_get_text(entry));

	if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
	{
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(app->open_filesel), locale_filename);
	}
	else if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
	{
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(app->open_filesel), locale_filename);
		on_file_open_dialog_response(GTK_DIALOG(app->open_filesel), GTK_RESPONSE_ACCEPT, NULL);
	}

	g_free(locale_filename);
}


void
on_file_open_selection_changed         (GtkFileChooser  *filechooser,
                                        gpointer         user_data)
{
	gchar *filename = gtk_file_chooser_get_filename(filechooser);
	gboolean is_on = gtk_file_chooser_get_show_hidden(filechooser);

	if (filename)
	{
		// try to get the UTF-8 equivalent for the filename, fallback to filename if error
		gchar *utf8_filename = g_locale_to_utf8(filename, -1, NULL, NULL, NULL);
		if (utf8_filename == NULL) utf8_filename = g_strdup(filename);

		gtk_entry_set_text(GTK_ENTRY(lookup_widget(
				GTK_WIDGET(filechooser), "file_entry")), utf8_filename);
		g_free(utf8_filename);
		g_free(filename);
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
			lookup_widget(GTK_WIDGET(filechooser), "check_hidden")), is_on);
}


/*
 * save dialog callbacks
 */
void
on_file_save_dialog_response           (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gint idx = document_get_cur_idx();
		gchar *new_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(app->save_filesel));

		// check if file exists and ask whether to overwrite or not
		if (g_file_test(new_filename, G_FILE_TEST_EXISTS))
		{
			if (dialogs_show_question(
						_("The file '%s' already exists. Do you want to overwrite it?"),
						new_filename) == FALSE) return;
		}
		gtk_widget_hide(app->save_filesel);

		if (doc_list[idx].file_name) g_free(doc_list[idx].file_name);
		doc_list[idx].file_name = new_filename;

		utils_replace_filename(idx);
		document_save_file(idx);

		utils_build_show_hide(idx);

		// finally add current file to recent files menu
		if (g_queue_find_custom(app->recent_queue, doc_list[idx].file_name, (GCompareFunc) strcmp) == NULL)
		{
			g_queue_push_head(app->recent_queue, g_strdup(doc_list[idx].file_name));
			if (g_queue_get_length(app->recent_queue) > app->mru_length)
			{
				g_free(g_queue_pop_tail(app->recent_queue));
			}
			utils_update_recent_menu();
		}
	}
	else gtk_widget_hide(app->save_filesel);
}


/*
 * font dialog callbacks
 */
void
on_font_ok_button_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
	// We do the same thing as apply, but we close the dialog after.
	on_font_apply_button_clicked(button, NULL);
	gtk_widget_hide(app->open_fontsel);
}


void
on_font_apply_button_clicked           (GtkButton       *button,
                                        gpointer         user_data)
{
	g_free(app->editor_font);
	app->editor_font = g_strdup(
				gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(app->open_fontsel)));

	utils_set_font();
}


void
on_font_cancel_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
	gtk_widget_hide(app->open_fontsel);
}


/*
 * color dialog callbacks
 */
void
on_color_cancel_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
	gtk_widget_hide(app->open_colorsel);
}


void
on_color_ok_button_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
	GdkColor color;
	gint idx = document_get_cur_idx();
	gchar *hex;

	gtk_widget_hide(app->open_colorsel);
	if (idx == -1 || ! doc_list[idx].is_valid) return;

	gtk_color_selection_get_current_color(
			GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(app->open_colorsel)->colorsel), &color);

	hex = utils_get_hex_from_color(&color);
	sci_add_text(doc_list[idx].sci, hex);
	g_free(hex);
}


gboolean
on_window_configure_event              (GtkWidget *widget,
                                        GdkEventConfigure *event,
                                        gpointer user_data)
{
	//GtkWidget *vpaned1 = lookup_widget(app->window, "vpaned1");
	//gtk_paned_set_position(GTK_PANED(vpaned1), event->height - GEANY_MSGWIN_HEIGHT);
	app->geometry[0] = event->x;
	app->geometry[1] = event->y;
	app->geometry[2] = event->width;
	app->geometry[3] = event->height;

	return FALSE;
}


gboolean
on_window_key_press_event              (GtkWidget *widget,
                                        GdkEventKey *event,
                                        gpointer user_data)
{
	return keybindings_got_event(widget, event, user_data);
}


// calls the edit popup menu in the editor
gboolean
on_editor_button_press_event           (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{
	gint idx = GPOINTER_TO_INT(user_data);
	gint clickpos = sci_get_position_from_xy(doc_list[idx].sci, event->x, event->y, FALSE);

#ifndef GEANY_WIN32
	if (event->button == 1)
	{
		utils_check_disk_status(idx);
	}
#endif

	if (event->button == 3)
	{
		utils_find_current_word(doc_list[idx].sci, clickpos,
			current_word, sizeof current_word);

		utils_update_popup_goto_items((current_word[0] != '\0') ? TRUE : FALSE);
		utils_update_popup_copy_items(idx);
		utils_update_insert_include_item(idx, 0);
		gtk_menu_popup(GTK_MENU(app->popup_menu), NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}
	return FALSE;
}


void
on_crlf_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_convert_eols(doc_list[idx].sci, SC_EOL_CRLF);
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_CRLF);
}


void
on_lf_activate                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_convert_eols(doc_list[idx].sci, SC_EOL_LF);
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_LF);
}


void
on_cr_activate                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;
	sci_convert_eols(doc_list[idx].sci, SC_EOL_CR);
	sci_set_eol_mode(doc_list[idx].sci, SC_EOL_CR);
}


void
on_replace_tabs_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (idx == -1 || ! doc_list[idx].is_valid) return;
	utils_replace_tabs(idx);
}


gboolean
toolbar_popup_menu                     (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_menu_popup(GTK_MENU(app->toolbar_menu), NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}
	return FALSE;
}


void
on_taglist_tree_selection_changed      (GtkTreeSelection *selection,
                                        gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *string;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 0, &string, -1);
		if (string && (strlen(string) > 0))
		{
			gint idx = document_get_cur_idx();
			utils_goto_line(idx, utils_get_local_tag(idx, string));
			g_free(string);
		}
	}
}


void
on_openfiles_tree_selection_changed    (GtkTreeSelection *selection,
                                        gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gint idx = 0;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 1, &idx, -1);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook),
					gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook),
					(GtkWidget*) doc_list[idx].sci));
	}
}


void
on_filetype_change                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx < 0) return;

	document_set_filetype(idx, (filetype*)user_data);

	//sci_colourise(doc_list[idx].sci, 0, -1);
	//document_update_tag_list(idx);
}


void
on_to_lower_case1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;
	if (idx < 0) return;

	text = g_malloc(sci_get_selected_text_length(doc_list[idx].sci) + 1);
	sci_get_selected_text(doc_list[idx].sci, text);
	sci_replace_sel(doc_list[idx].sci, g_ascii_strdown(text, -1));
	g_free(text);
}


void
on_to_upper_case1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;
	if (idx < 0) return;

	text = malloc(sci_get_selected_text_length(doc_list[idx].sci) + 1);
	sci_get_selected_text(doc_list[idx].sci, text);
	sci_replace_sel(doc_list[idx].sci, g_ascii_strup(text, -1));
	g_free(text);
}


void
on_show_toolbar1_toggled               (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (app->toolbar_visible)
	{
		gtk_widget_hide(GTK_WIDGET(app->toolbar));
	}
	else
	{
		gtk_widget_show(GTK_WIDGET(app->toolbar));
	}
	app->toolbar_visible = (app->toolbar_visible) ? FALSE : TRUE;;
}


void
on_fullscreen1_toggled                 (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	app->fullscreen = (app->fullscreen) ? FALSE : TRUE;
	utils_set_fullscreen();
}


void
on_show_messages_window1_toggled       (GtkCheckMenuItem *checkmenuitem,
                                        gpointer          user_data)
{
	if (app->msgwindow_visible)
	{
		gtk_container_remove(GTK_CONTAINER(lookup_widget(app->window, "vpaned1")), lookup_widget(app->window, "scrolledwindow1"));
	}
	else
	{
		gtk_container_add(GTK_CONTAINER(lookup_widget(app->window, "vpaned1")), lookup_widget(app->window, "scrolledwindow1"));
	}
	app->msgwindow_visible = (app->msgwindow_visible) ? FALSE : TRUE;
}


void
on_markers_margin1_toggled             (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	app->show_markers_margin = (app->show_markers_margin) ? FALSE : TRUE;
	utils_show_markers_margin();
}


void
on_show_line_numbers1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	app->show_linenumber_margin = (app->show_linenumber_margin) ? FALSE : TRUE;
	utils_show_linenumber_margin();
}


void
on_line_breaking1_toggled              (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (! switch_notebook_page)
	{
		gint idx = document_get_cur_idx();
		if (idx == -1 || ! doc_list[idx].is_valid) return;
		doc_list[idx].line_breaking = ! doc_list[idx].line_breaking;
		sci_set_lines_wrapped(doc_list[idx].sci, doc_list[idx].line_breaking);
	}
}


void
on_set_file_readonly1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (! switch_notebook_page)
	{
		gint idx = document_get_cur_idx();
		if (idx == -1 || ! doc_list[idx].is_valid) return;
		doc_list[idx].readonly = ! doc_list[idx].readonly;
		sci_set_readonly(doc_list[idx].sci, doc_list[idx].readonly);
		utils_update_statusbar(idx, -1);
	}
}


void
on_use_auto_indention1_toggled         (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data)
{
	if (! switch_notebook_page)
	{
		gint idx = document_get_cur_idx();
		if (idx == -1 || ! doc_list[idx].is_valid) return;
		doc_list[idx].use_auto_indention = ! doc_list[idx].use_auto_indention;
	}
}


void
on_find_usage1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint i, pos, line;
	gint flags = SCFIND_MATCHCASE | SCFIND_WHOLEWORD;
	struct TextToFind ttf;
	gchar *buffer, *short_file_name, *string;

	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_MESSAGE);
	gtk_list_store_clear(msgwindow.store_msg);
	for(i = 0; i < GEANY_MAX_OPEN_FILES; i++)
	{
		if (doc_list[i].sci)
		{
			ttf.chrg.cpMin = 0;
			ttf.chrg.cpMax = sci_get_length(doc_list[i].sci);
			ttf.lpstrText = g_malloc(sizeof current_word);
			strncpy(ttf.lpstrText, current_word, sizeof current_word);
			while (1)
			{
				pos = sci_find_text(doc_list[i].sci, flags, &ttf);
				if (pos == -1) break;

				line = sci_get_line_from_position(doc_list[i].sci, pos);
				buffer = g_malloc0(sci_get_line_length(doc_list[i].sci, line) + 1);
				sci_get_line(doc_list[i].sci, line, buffer);

				short_file_name = g_path_get_basename(doc_list[i].file_name);
				string = g_strdup_printf("%s:%d : %s", short_file_name, line + 1, g_strstrip(buffer));
				msgwin_msg_add(line + 1, doc_list[i].file_name, string);

				g_free(buffer);
				g_free(short_file_name);
				g_free(string);
				ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
			}
			g_free(ttf.lpstrText);
		}
	}
}


void
on_goto_tag_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint type;
	guint i, j;
	const GPtrArray *tags;

	if (menuitem == GTK_MENU_ITEM(lookup_widget(app->popup_menu, "goto_tag_definition1")))
		type = tm_tag_function_t;
	else
		type = tm_tag_prototype_t;

	for (j = 0; j < app->tm_workspace->work_objects->len; j++)
	{
		tags = tm_tags_extract(TM_WORK_OBJECT(app->tm_workspace->work_objects->pdata[j])->tags_array, type);
		if (tags)
		{
			for (i = 0; i < tags->len; ++i)
			{
				if (utils_strcmp(TM_TAG(tags->pdata[i])->name, current_word))
				{
					if (! utils_goto_file_line(
							TM_TAG(tags->pdata[i])->atts.entry.file->work_object.file_name,
							TRUE,
							TM_TAG(tags->pdata[i])->atts.entry.line))
					{
						utils_beep();
						msgwin_status_add(_("Declaration or definition of \"%s()\" not found"), current_word);
					}
					return;
				}
			}
		}
	}
	// if we are here, there was no match and we are beeping ;-)
	utils_beep();
	msgwin_status_add(_("Declaration or definition of \"%s()\" not found"), current_word);
}


void
on_count_words1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_word_count();
}


void
on_show_color_chooser1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_color();
}


/* callback function for all treeview widgets */
gboolean
on_tree_view_button_press_event        (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{
	if (event->type == GDK_2BUTTON_PRESS && user_data)
	{
		if (GPOINTER_TO_INT(user_data) == 4)
		{	// double click in the message treeview (results of 'Find usage')
			GtkTreeIter iter;
			GtkTreeModel *model;
			GtkTreeSelection *selection;
			gchar *file;
			gint line;

			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_msg));
			if (gtk_tree_selection_get_selected(selection, &model, &iter))
			{
				gtk_tree_model_get(model, &iter, 0, &line, 1, &file, -1);
				if (file && strlen (file) > 0)
				{
					utils_goto_file_line(file, FALSE, line);
				}
				g_free(file);
			}
		}
		else if (GPOINTER_TO_INT(user_data) == 5)
		{	// double click in the compiler treeview
			GtkTreeIter iter;
			GtkTreeModel *model;
			GtkTreeSelection *selection;
			gchar *string;
			static GdkColor red = {0, 65535, 0, 0};
			GdkColor *color;

			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_compiler));
			if (gtk_tree_selection_get_selected(selection, &model, &iter))
			{
				gtk_tree_model_get(model, &iter, 0, &color, 1, &string, -1);
				if (string && gdk_color_equal(&red, color))
				{
					gint line;
					gint idx;
					gchar *filename;
					utils_parse_compiler_error_line(string, &filename, &line);
					if (filename != NULL && line > -1)
					{
						// use document_open_file to find an already open file, or open it in place
						idx = document_open_file(-1, filename, 0, FALSE, NULL);
						// document_set_indicator will check valid idx
						document_set_indicator(idx, line - 1);
						// utils_goto_file_line will check valid filename.
						utils_goto_file_line(filename, FALSE, line);
					}
					g_free(filename);
				}
				g_free(string);
			}
		}
	}

	if (event->button == 1 && user_data && GPOINTER_TO_INT(user_data) == 7)
	{	// allow reclicking of taglist treeview item
		GtkTreeSelection *select =
			gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		on_taglist_tree_selection_changed(select, user_data);
	}

	if (user_data && event->button == 3)
	{	// popupmenu to hide or clear the active treeview
		if (user_data && GPOINTER_TO_INT(user_data) == 3)
			gtk_menu_popup(GTK_MENU(msgwindow.popup_status_menu), NULL, NULL, NULL, NULL, event->button, event->time);
		else if (user_data && GPOINTER_TO_INT(user_data) == 4)
			gtk_menu_popup(GTK_MENU(msgwindow.popup_msg_menu), NULL, NULL, NULL, NULL, event->button, event->time);
		else if (user_data && GPOINTER_TO_INT(user_data) == 5)
			gtk_menu_popup(GTK_MENU(msgwindow.popup_compiler_menu), NULL, NULL, NULL, NULL, event->button, event->time);
		else if (user_data && GPOINTER_TO_INT(user_data) == 6)
			gtk_menu_popup(GTK_MENU(tv.popup_openfiles), NULL, NULL, NULL, NULL, event->button, event->time);
		else if (user_data && GPOINTER_TO_INT(user_data) == 7)
			gtk_menu_popup(GTK_MENU(tv.popup_taglist), NULL, NULL, NULL, NULL, event->button, event->time);
	}
	return FALSE;
}


void
on_openfiles_tree_popup_clicked        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv.tree_openfiles));
	GtkTreeModel *model;
	gint idx = -1;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 1, &idx, -1);
		if (idx >= 0)
		{
			switch (GPOINTER_TO_INT(user_data))
			{
				case 0:
				{
					document_remove(gtk_notebook_page_num(GTK_NOTEBOOK(app->notebook), GTK_WIDGET(doc_list[idx].sci)));
					break;
				}
				case 1:
				{
					if (doc_list[idx].changed) document_save_file(idx);
					break;
				}
				case 2:
				{
					on_toolbutton23_clicked(NULL, NULL);
					break;
				}
				case 3:
				{
					app->treeview_openfiles_visible = FALSE;
					utils_treeviews_showhide();
					break;
				}
				case 4:
				{
					app->treeview_openfiles_visible = FALSE;
					app->treeview_symbol_visible = FALSE;
					utils_treeviews_showhide();
					break;
				}
			}
		}
	}
}


void
on_taglist_tree_popup_clicked          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	switch (GPOINTER_TO_INT(user_data))
	{
		case 0:
		{
			app->treeview_symbol_visible = FALSE;
			utils_treeviews_showhide();
			break;
		}
		case 1:
		{
			app->treeview_openfiles_visible = FALSE;
			app->treeview_symbol_visible = FALSE;
			utils_treeviews_showhide();
			break;
		}
	}
}


void
on_message_treeview_clear_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gtk_list_store_clear(GTK_LIST_STORE(user_data));
}


void
on_compiler_treeview_copy_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(msgwindow.tree_compiler));
	GtkTreeModel *model;
	gchar *string;

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, 1, &string, -1);
		if (string || strlen (string) > 0)
		{
			gtk_clipboard_set_text(gtk_clipboard_get(gdk_atom_intern("CLIPBOARD", FALSE)), string, -1);
		}
		g_free(string);
	}

}


void
on_compile_button_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	on_build_compile_activate(NULL, NULL);
}


void
on_build_compile_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GPid child_pid = (GPid) 0;

	if (doc_list[idx].changed) document_save_file(idx);

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_LATEX)
		child_pid = build_compile_tex_file(idx, 0);
	else
		child_pid = build_compile_file(idx);

	if (child_pid != (GPid) 0)
	{
		gtk_widget_set_sensitive(app->compile_button, FALSE);
		g_child_watch_add(child_pid, build_exit_cb, NULL);
	}
}


void
on_build_tex_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GPid child_pid = (GPid) 0;

	if (doc_list[idx].changed) document_save_file(idx);

	switch (GPOINTER_TO_INT(user_data))
	{
		case 0: child_pid = build_compile_tex_file(idx, 0); break;
		case 1: child_pid = build_compile_tex_file(idx, 1); break;
		case 2: child_pid = build_view_tex_file(idx, 0); break;
		case 3: child_pid = build_view_tex_file(idx, 1); break;
	}

	if (GPOINTER_TO_INT(user_data) <= 1 && child_pid != (GPid) 0)
	{
		gtk_widget_set_sensitive(app->compile_button, FALSE);
		g_child_watch_add(child_pid, build_exit_cb, NULL);
	}
}


void
on_build_build_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GPid child_pid = (GPid) 0;

	if (doc_list[idx].changed) document_save_file(idx);

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_LATEX)
		child_pid = build_compile_tex_file(idx, 1);
	else
		child_pid = build_link_file(idx);

	if (child_pid != (GPid) 0)
	{
		gtk_widget_set_sensitive(app->compile_button, FALSE);
		g_child_watch_add(child_pid, build_exit_cb, NULL);
	}
}


void
on_build_make_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (GPOINTER_TO_INT(user_data) == 1)
	{
		dialogs_show_make_target();
	}
	else
	{
		GPid child_pid;

		if (doc_list[idx].changed) document_save_file(idx);

		child_pid = build_make_file(idx, FALSE);
		if (child_pid != (GPid) 0)
		{
			gtk_widget_set_sensitive(app->compile_button, FALSE);
			g_child_watch_add(child_pid, build_exit_cb, NULL);
		}
	}
}


void
on_build_execute_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_LATEX && user_data != NULL)
	{
		if (build_view_tex_file(idx, GPOINTER_TO_INT(user_data)) == (GPid) 0)
		{
			msgwin_status_add(_("Failed to execute the view program"));
		}
	}
	else
	{
		// save the file only if the run command uses it
		if (doc_list[idx].changed &&
			strstr(doc_list[idx].file_type->programs->run_cmd, "%f") != NULL)
				document_save_file(idx);
		if (build_run_cmd(idx) == (GPid) 0)
		{
			msgwin_status_add(_("Failed to execute the terminal program"));
		}
	}
	//gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
}


void
on_build_arguments_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_includes_arguments_gen();
}


void
on_build_tex_arguments_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_includes_arguments_tex();
}


void
on_make_target_dialog_response         (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gint idx = document_get_cur_idx();
		GPid child_pid;

		if (doc_list[idx].changed) document_save_file(idx);

		strncpy(app->build_make_custopt, gtk_entry_get_text(GTK_ENTRY(user_data)), 255);

		child_pid = build_make_file(idx, TRUE);
		if (child_pid != (GPid) 0)
		{
			gtk_widget_set_sensitive(app->compile_button, FALSE);
			g_child_watch_add(child_pid, build_exit_cb, NULL);
		}
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


void
on_make_target_entry_activate          (GtkEntry        *entry,
                                        gpointer         user_data)
{
	on_make_target_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_ACCEPT, entry);
}


void
on_find1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_find();
}


void
on_replace1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_replace();
}


void
on_find_next1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (app->search_text)
	{
		document_find_text(idx, app->search_text, search_flags, search_backwards);
	}
}


void
on_find_dialog_response                (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		gint idx = document_get_cur_idx();
		gboolean
			fl1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(app->find_dialog), "check_case"))),
			fl2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(app->find_dialog), "check_word"))),
			fl3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(app->find_dialog), "check_regexp"))),
			fl4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(app->find_dialog), "check_wordstart")));
		search_backwards = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(app->find_dialog), "check_back")));

		g_free(app->search_text);
		app->search_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(user_data)))));
		if (strlen(app->search_text) == 0)
		{
			utils_beep();
			gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(app->find_dialog, "entry"))->child));
			return;
		}
		gtk_widget_hide(app->find_dialog);

		gtk_combo_box_prepend_text(GTK_COMBO_BOX(user_data), app->search_text);
		search_flags = (fl1 ? SCFIND_MATCHCASE : 0) |
				(fl2 ? SCFIND_WHOLEWORD : 0) |
				(fl3 ? SCFIND_REGEXP : 0) |
				(fl4 ? SCFIND_WORDSTART : 0);
		document_find_text(idx, app->search_text, search_flags, search_backwards);
	}
	else gtk_widget_hide(app->find_dialog);
}


void
on_find_entry_activate                 (GtkEntry        *entry,
                                        gpointer         user_data)
{
	on_find_dialog_response(NULL, GTK_RESPONSE_ACCEPT,
				lookup_widget(GTK_WIDGET(entry), "entry"));
}


void
on_replace_dialog_response             (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	gint idx = document_get_cur_idx();
	GtkWidget *entry_find = lookup_widget(GTK_WIDGET(app->replace_dialog), "entry_find");
	GtkWidget *entry_replace = lookup_widget(GTK_WIDGET(app->replace_dialog), "entry_replace");
	gboolean fl1, fl2, fl3, fl4;
	const gchar *find, *replace;

	if (response == GTK_RESPONSE_CANCEL)
	{
		gtk_widget_hide(app->replace_dialog);
		return;
	}

	fl1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(app->replace_dialog), "check_case")));
	fl2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(app->replace_dialog), "check_word")));
	fl3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(app->replace_dialog), "check_regexp")));
	fl4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(app->replace_dialog), "check_wordstart")));
	search_backwards_re = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(app->replace_dialog), "check_back")));
	find = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))));
	replace = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))));

	if ((! fl1) && (strcasecmp(find, replace) == 0))
	{
		utils_beep();
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(app->replace_dialog, "entry_find"))->child));
		return;
	}

	gtk_combo_box_prepend_text(GTK_COMBO_BOX(entry_find), find);
	gtk_combo_box_prepend_text(GTK_COMBO_BOX(entry_replace), replace);

	search_flags_re = (fl1 ? SCFIND_MATCHCASE : 0) |
					  (fl2 ? SCFIND_WHOLEWORD : 0) |
					  (fl3 ? SCFIND_REGEXP : 0) |
					  (fl4 ? SCFIND_WORDSTART : 0);
	switch (response)
	{
		case GEANY_RESPONSE_REPLACE:
		{
			document_replace_text(idx, find, replace, search_flags_re, search_backwards_re);
			break;
		}
		case GEANY_RESPONSE_REPLACE_ALL:
		{
			document_replace_all(idx, find, replace, search_flags_re);
			break;
		}
		case GEANY_RESPONSE_REPLACE_SEL:
		{
			document_replace_sel(idx, find, replace, search_flags_re);
			break;
		}
	}
}


void
on_replace_entry_activate              (GtkEntry        *entry,
                                        gpointer         user_data)
{
	on_replace_dialog_response(NULL, GEANY_RESPONSE_REPLACE, NULL);
}


void
on_toolbutton_new_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	document_new_file(NULL);
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
#ifdef GEANY_WIN32
	gchar *pwd = g_get_current_dir();
	gchar *uri = g_strconcat("file:///", g_path_skip_root(pwd), "/doc/index.html", NULL);
	g_free(pwd);
#else
	gchar *uri = g_strconcat("file://", DOCDIR, "index.html", NULL);
#endif

	if (! g_file_test(uri + 7, G_FILE_TEST_IS_REGULAR))
	{
		g_free(uri);
		uri = g_strconcat(GEANY_HOMEPAGE, "manual/index.html", NULL);
	}

	utils_start_browser(uri);
	g_free(uri);
}


void
on_help_shortcuts1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_keyboard_shortcuts();
}


void
on_website1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	utils_start_browser(GEANY_HOMEPAGE);
}


void
on_pref_tools_button_clicked           (GtkButton       *button,
                                        gpointer         item)
{
#ifdef GEANY_WIN32
	win32_show_pref_file_dialog(item);
#else
	GtkWidget *dialog;
	gchar *filename, *tmp, **field;

	// initialize the dialog
	dialog = gtk_file_chooser_dialog_new(_("Open File"), GTK_WINDOW(app->prefs_dialog),
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	// cut the options from the command line
	field = g_strsplit(gtk_entry_get_text(GTK_ENTRY(item)), " ", 2);
	if (field[0])
	{
		filename = g_find_program_in_path(field[0]);
		if (filename)
		{
			gtk_file_chooser_select_filename(GTK_FILE_CHOOSER(dialog), filename);
			g_free(filename);
		}
	}

	// run it
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		tmp = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (g_strv_length(field) > 1)
			filename = g_strconcat(tmp, " ", field[1], NULL);
		else
		{
			filename = tmp;
			tmp = NULL;
		}
		gtk_entry_set_text(GTK_ENTRY(item), filename);
		g_free(filename);
		g_free(tmp);
	}

	g_strfreev(field);
	gtk_widget_destroy(dialog);
#endif
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
		msgwin_status_add(_("Please set the filetype for the current file before using this function."));
		return;
	}

	// utils_get_current_function returns -1 on failure, so sci_get_position_from_line
	// returns the current position, so it should be safe
	line = utils_get_current_function(idx, &cur_tag);
	pos = sci_get_position_from_line(doc_list[idx].sci, line - 1);

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_PASCAL:
		{
			text = templates_get_template_function(GEANY_TEMPLATE_FUNCTION_PASCAL, cur_tag);
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		{
			text = templates_get_template_function(GEANY_TEMPLATE_FUNCTION_ROUTE, cur_tag);
			break;
		}
		default:
		{
			text = templates_get_template_function(GEANY_TEMPLATE_FUNCTION, cur_tag);
		}
	}

	sci_insert_text(doc_list[idx].sci, pos, text);
	g_free(text);
}


void
on_comments_multiline_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;

	if (doc_list[idx].file_type == NULL)
	{
		msgwin_status_add(_("Please set the filetype for the current file before using this function."));
		return;
	}

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_PASCAL:
		{
			text = templates_get_template_generic(GEANY_TEMPLATE_MULTILINE_PASCAL);
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		{
			text = templates_get_template_generic(GEANY_TEMPLATE_MULTILINE_ROUTE);
			break;
		}
		default:
		{
			text = templates_get_template_generic(GEANY_TEMPLATE_MULTILINE);
		}
	}

	sci_insert_text(doc_list[idx].sci, -1, text);
	g_free(text);
}


void
on_comments_gpl_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;

	if (doc_list[idx].file_type == NULL)
	{
		msgwin_status_add(_("Please set the filetype for the current file before using this function."));
		return;
	}

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_PASCAL:
		{
			text = templates_get_template_gpl(GEANY_TEMPLATE_GPL_PASCAL);
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		{
			text = templates_get_template_gpl(GEANY_TEMPLATE_GPL_ROUTE);
			break;
		}
		default:
		{
			text = templates_get_template_gpl(GEANY_TEMPLATE_GPL);
		}
	}

	sci_insert_text(doc_list[idx].sci, -1, text);
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
	// sets the cursor to the right position to type the changelog text,
	// the template has 21 chars + length of name and email
	sci_goto_pos(doc_list[idx].sci, 21 + strlen(app->pref_template_developer) + strlen(app->pref_template_mail), TRUE);

	g_free(text);
}




void
on_comments_fileheader_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;
	gchar *ext = NULL;

	if (doc_list[idx].file_type == NULL)
	{
		msgwin_status_add(_("Please set the filetype for the current file before using this function."));
		return;
	}

	if (doc_list[idx].file_name == NULL)
		ext = doc_list[idx].file_type->extension;

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_PASCAL:
		{
			text = templates_get_template_fileheader(GEANY_TEMPLATE_FILEHEADER_PASCAL, ext, idx);
			break;
		}
		case GEANY_FILETYPES_PYTHON:
		case GEANY_FILETYPES_RUBY:
		case GEANY_FILETYPES_SH:
		case GEANY_FILETYPES_MAKE:
		case GEANY_FILETYPES_PERL:
		{
			text = templates_get_template_fileheader(GEANY_TEMPLATE_FILEHEADER_ROUTE, ext, idx);
			break;
		}
		default:
		{
			text = templates_get_template_fileheader(GEANY_TEMPLATE_FILEHEADER, ext, idx);
		}
	}

	sci_insert_text(doc_list[idx].sci, 0, text);
	sci_goto_pos(doc_list[idx].sci, 0, FALSE);
	g_free(text);
}


void
on_insert_include_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gint pos = sci_get_current_position(doc_list[idx].sci);
	gchar *text;

	if (utils_strcmp(user_data, "(blank)"))
	{
		text = g_strdup("#include \"\"\n");
	}
	else
	{
		text = g_strconcat("#include <", user_data, ">\n", NULL);
	}

	sci_insert_text(doc_list[idx].sci, pos, text);
	g_free(text);
}


void
on_includes_arguments_dialog_response  (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		filetype *ft = doc_list[GPOINTER_TO_INT(user_data)].file_type;

		if (ft->menu_items->can_compile)
		{
			if (ft->programs->compiler) g_free(ft->programs->compiler);
			ft->programs->compiler = g_strdup(gtk_entry_get_text(
					GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry1"))));
		}
		if (ft->menu_items->can_link)
		{
			if (ft->programs->linker) g_free(ft->programs->linker);
			ft->programs->linker = g_strdup(gtk_entry_get_text(
					GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry2"))));
		}
		if (ft->menu_items->can_exec)
		{
			if (ft->programs->run_cmd) g_free(ft->programs->run_cmd);
			ft->programs->run_cmd = g_strdup(gtk_entry_get_text(
					GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry3"))));
		}
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


void
on_includes_arguments_tex_dialog_response  (GtkDialog *dialog,
                                            gint response,
                                            gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		filetype *ft = doc_list[GPOINTER_TO_INT(user_data)].file_type;

		if (ft->programs->compiler) g_free(ft->programs->compiler);
		ft->programs->compiler = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry1"))));
		if (ft->programs->linker) g_free(ft->programs->linker);
		ft->programs->linker = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry2"))));
		if (ft->programs->run_cmd) g_free(ft->programs->run_cmd);
		ft->programs->run_cmd = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry3"))));
		if (ft->programs->run_cmd2) g_free(ft->programs->run_cmd2);
		ft->programs->run_cmd2 = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry4"))));
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


void
on_recent_file_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar *locale_filename = g_locale_from_utf8((gchar*) user_data, -1, NULL, NULL, NULL);

	if (locale_filename == NULL) locale_filename = g_strdup((gchar*) user_data);

	document_open_file(-1, locale_filename, 0, FALSE, NULL);

	g_free(locale_filename);
}


void
on_file_open_check_hidden_toggled      (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
	gboolean is_on = gtk_toggle_button_get_active(togglebutton);

	gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(app->open_filesel), is_on);
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
	on_build_execute_activate(NULL, NULL);
}


void
on_find_previous1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (app->search_text)
	{
		document_find_text(idx, app->search_text, search_flags,
			!search_backwards);
	}
}


void
on_go_to_line1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	// this is search menu cb; call popup menu goto cb
	on_go_to_line_activate(menuitem, user_data);
}


void
on_menu_remove_indicators1_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	document_clear_indicators(idx);
}

