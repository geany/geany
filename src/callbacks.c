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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#include "geany.h"

#include <stdlib.h>
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
#include "msgwindow.h"
#include "build.h"
#include "prefs.h"
#include "templates.h"
#include "treeviews.h"


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
static gchar current_word[128];

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
/*	else if (sig == SIGUSR1)
	{
#define BUFLEN 512
		gint fd;
		gchar *buffer = g_malloc0(BUFLEN);

		geany_debug("got SIGUSR1 signal, try to read from named pipe");

		if ((fd = open(fifo_name, O_RDONLY | O_NONBLOCK)) == -1)
		{
			geany_debug("error opening named pipe (%s)", strerror(errno));
			return;
		}
		usleep(10000);
		if (read(fd, buffer, BUFLEN) != -1) geany_debug("Inhalt: %s", buffer);

		close(fd);
		g_free(buffer);
	}
*/
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
		unlink(fifo);
		g_free(fifo);
	}
#endif

	filetypes_free_types();
	styleset_free_styles();
	templates_free_templates();
	tm_workspace_free(TM_WORK_OBJECT(app->tm_workspace));
	g_free(app->configdir);
#ifdef HAVE_VTE
	g_free(app->lib_vte);
#endif
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
	g_free(app->build_c_cmd);
	g_free(app->build_cpp_cmd);
	g_free(app->build_java_cmd);
	g_free(app->build_javac_cmd);
	g_free(app->build_fpc_cmd);
	g_free(app->build_make_cmd);
	g_free(app->build_term_cmd);
	g_free(app->build_browser_cmd);
	g_free(app->build_args_inc);
	g_free(app->build_args_libs);
	g_free(app->build_args_prog);
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
	gtk_widget_destroy(app->window);
	// kill explicitly since only one or none menu is shown at a time
	if (dialogs_build_menus.menu_c.menu && GTK_IS_WIDGET(dialogs_build_menus.menu_c.menu))
					gtk_widget_destroy(dialogs_build_menus.menu_c.menu);
	if (dialogs_build_menus.menu_misc.menu && GTK_IS_WIDGET(dialogs_build_menus.menu_misc.menu))
					gtk_widget_destroy(dialogs_build_menus.menu_misc.menu);
	if (dialogs_build_menus.menu_tex.menu && GTK_IS_WIDGET(dialogs_build_menus.menu_tex.menu))
					gtk_widget_destroy(dialogs_build_menus.menu_tex.menu);

	/// destroy popup menus - FIXME TEST THIS CODE
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

	if (app->have_vte) vte_close();

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
				if (dialogs_show_confirm_exit() && on_close_all1_activate(NULL, NULL)) destroyapp(NULL, gdata);
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
			if (dialogs_show_confirm_exit()) destroyapp(NULL, gdata);
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
	if (app->prefs_dialog)
	{
		prefs_init_dialog();
		gtk_widget_show(app->prefs_dialog);
	}
	else
	{
		app->prefs_dialog = create_prefs_dialog();
		g_signal_connect((gpointer) app->prefs_dialog, "response", G_CALLBACK(on_prefs_button_clicked), NULL);
		g_signal_connect((gpointer) app->prefs_dialog, "delete_event", G_CALLBACK(on_prefs_delete_event), NULL);
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "tagbar_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(1));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "msgwin_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(2));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "editor_font"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(3));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "font_term"),
				"font-set", G_CALLBACK(on_prefs_font_choosed), GINT_TO_POINTER(4));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "long_line_color"),
				"color-set", G_CALLBACK(on_prefs_color_choosed), GINT_TO_POINTER(1));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "color_fore"),
				"color-set", G_CALLBACK(on_prefs_color_choosed), GINT_TO_POINTER(2));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "color_back"),
				"color-set", G_CALLBACK(on_prefs_color_choosed), GINT_TO_POINTER(3));
		// file chooser buttons in the tools tab
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "button_gcc"),
				"clicked", G_CALLBACK(on_pref_tools_button_clicked), lookup_widget(app->prefs_dialog, "entry_com_c"));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "button_gpp"),
				"clicked", G_CALLBACK(on_pref_tools_button_clicked), lookup_widget(app->prefs_dialog, "entry_com_cpp"));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "button_javac"),
				"clicked", G_CALLBACK(on_pref_tools_button_clicked), lookup_widget(app->prefs_dialog, "entry_com_javac"));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "button_java"),
				"clicked", G_CALLBACK(on_pref_tools_button_clicked), lookup_widget(app->prefs_dialog, "entry_com_java"));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "button_make"),
				"clicked", G_CALLBACK(on_pref_tools_button_clicked), lookup_widget(app->prefs_dialog, "entry_com_make"));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "button_term"),
				"clicked", G_CALLBACK(on_pref_tools_button_clicked), lookup_widget(app->prefs_dialog, "entry_com_term"));
		g_signal_connect((gpointer) lookup_widget(app->prefs_dialog, "button_browser"),
				"clicked", G_CALLBACK(on_pref_tools_button_clicked), lookup_widget(app->prefs_dialog, "entry_browser"));

		prefs_init_dialog();
		gtk_widget_show(app->prefs_dialog);
	}
}


// about menu item
void
on_info1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_about();
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
	gchar *buffer = g_strdup_printf(_
				 ("Are you sure you want to reload '%s'?\nAny unsaved changes will be lost."),
				 basename);

	if (dialogs_show_reload_warning(buffer))
	{
		document_open_file(idx, NULL, 0, doc_list[idx].readonly, doc_list[idx].file_type);
	}

	g_free(basename);
	g_free(buffer);
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
		utils_update_statusbar(idx);
		utils_set_window_title(idx);
		utils_update_tag_list(idx, FALSE);
		utils_check_disk_status(idx, FALSE);

		switch_notebook_page = FALSE;
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

	if (response == GTK_RESPONSE_ACCEPT)
	{
		GSList *flist;
		gint ft_id = gtk_combo_box_get_active(GTK_COMBO_BOX(lookup_widget(GTK_WIDGET(dialog), "filetype_combo")));
		filetype *ft = NULL;
		gboolean ro = gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "check_readonly")));

		if (ft_id >= 0 && ft_id < GEANY_FILETYPES_ALL) ft = filetypes[ft_id];

		flist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(app->open_filesel));
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
				break;
			}
			g_free(flist->data);
			flist = flist->next;
		}
		g_slist_free(flist);
	}
}


// callback for the text entry for typing in filename
void
on_file_open_entry_activate            (GtkEntry        *entry,
                                        gpointer         user_data)
{
	gchar *locale_filename = g_locale_from_utf8(gtk_entry_get_text(entry), -1, NULL, NULL, NULL);
	if (locale_filename == NULL) locale_filename = g_strdup(gtk_entry_get_text(entry));

	if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
	{
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(app->open_filesel), locale_filename);
		on_file_open_dialog_response(GTK_DIALOG(app->open_filesel), GTK_RESPONSE_ACCEPT, NULL);
	}
	else if (g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
	{
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(app->open_filesel), locale_filename);
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
	gtk_widget_hide(app->save_filesel);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		gint idx = document_get_cur_idx();

		if (doc_list[idx].file_name) g_free(doc_list[idx].file_name);
		doc_list[idx].file_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(app->save_filesel));
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
	if (event->keyval == GDK_F12)
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_SCRATCH);
		gtk_widget_grab_focus(lookup_widget(app->window, "textview_scribble"));
		return TRUE;
	}
#ifdef HAVE_VTE
	if (event->keyval == GDK_F6 && app->have_vte)
	{
		gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_VTE);
		gtk_widget_grab_focus(vc->vte);
		return TRUE;
	}
#endif
	return FALSE;
}


gboolean
on_editor_key_press_event              (GtkWidget *widget,
                                        GdkEventKey *event,
                                        gpointer user_data)
{
	//gint idx = geany_document_get_cur_idx();
	gboolean ret = FALSE;
	gint idx = GPOINTER_TO_INT(user_data);
	gint pos = sci_get_current_position(doc_list[idx].sci);


	switch(event->keyval)
	{
		// show userlist with macros and variables on strg+space
		case 'g':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				sci_line_duplicate(doc_list[idx].sci);
				ret = TRUE;
			}
			break;
		}
		// show userlist with macros and variables on strg+space
		case ' ':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				sci_cb_start_auto_complete(
						doc_list[GPOINTER_TO_INT(user_data)].sci,
						sci_get_current_position(doc_list[idx].sci));
				ret = TRUE;
			}
			else if (event->state & GDK_MOD1_MASK)
			{	// ALT+Space
				sci_cb_show_calltip(doc_list[idx].sci, -1);
				ret = TRUE;
			}
			else if (event->state & GDK_SHIFT_MASK)
			{	// Shift+Space, catch this explicitly to suppress sci_cb_auto_forif() ;-)
				sci_add_text(doc_list[idx].sci, " ");
				ret = TRUE;
			}
			break;
		}
		// refreshs the tag lists
		case 'R':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				document_update_tag_list(idx);
				ret = TRUE;
			}
			break;
		}
		// reloads the document
		case 'r':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				gchar *basename = g_path_get_basename(doc_list[idx].file_name);
				gchar *buffer = g_strdup_printf(_
				 ("Are you sure you want to reload '%s'?\nAny unsaved changes will be lost."),
				 basename);

				if (dialogs_show_reload_warning(buffer))
				{
					document_open_file(idx, NULL, 0, doc_list[idx].readonly, doc_list[idx].file_type);
				}

				g_free(basename);
				g_free(buffer);
				ret = TRUE;
			}
			break;
		}
		// comment the current line or selected lines
		case 'd':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				sci_cb_do_comment(idx);
				ret = TRUE;
			}
			break;
		}
		// uri handling testing
		case '^':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				sci_cb_handle_uri(doc_list[idx].sci, pos);
				ret = TRUE;
			}
			break;
		}
		// zoom in the text
		case '+':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				sci_zoom_in(doc_list[idx].sci);
				ret = TRUE;
			}
			break;
		}
		// zoom out the text
		case '-':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				sci_zoom_out(doc_list[idx].sci);
				ret = TRUE;
			}
			break;
		}
		// open the preferences dialog
		case 'p':
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				on_preferences1_activate(NULL, NULL);
				ret = TRUE;
			}
			break;
		}
		// switch to the next open notebook tab to the right
		case GDK_Right:
		{
			if (event->state & GDK_MOD1_MASK)
			{
				utils_switch_document(RIGHT);
				ret = TRUE;
			}
			break;
		}
		// switch to the next open notebook tab to the right
		case GDK_Left:
		{
			if (event->state & GDK_MOD1_MASK)
			{
				utils_switch_document(LEFT);
				ret = TRUE;
			}
			break;
		}
		// show macro list
		case GDK_Return:
		{
			if (event->state & GDK_CONTROL_MASK)
			{
				sci_cb_show_macro_list(doc_list[idx].sci);
				ret = TRUE;
			}
			break;
		}

		case GDK_Insert:
		{
			if (! (event->state & GDK_SHIFT_MASK))
				doc_list[idx].do_overwrite = (doc_list[idx].do_overwrite) ? FALSE : TRUE;
			break;
		}
		case GDK_F12:
		{
			gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_SCRATCH);
			gtk_widget_grab_focus(lookup_widget(app->window, "textview_scribble"));
			ret = TRUE;
			break;
		}
#ifdef HAVE_VTE
		case GDK_F6:
		{
			if (app->have_vte)
			{
				gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_VTE);
				gtk_widget_grab_focus(vc->vte);
				break;
			}
			ret = TRUE;
			break;
		}
#endif
/* following code is unusable unless I get a signal for a line changed, don't want to do this with
 * updateUI(), additional problem: at line changes the column isn't kept
		case GDK_End:
		{	// extending HOME and END default behaviour, for details look at the start of this function
			if (cursor_pos_end == -1 || current_line != sci_get_current_line(doc_list[idx].sci, pos))
			{
				cursor_pos_end = pos;
				sci_cmd(doc_list[idx].sci, SCI_LINEEND);
				current_line = sci_get_current_line(doc_list[idx].sci, pos);
			}
			else if (current_line == sci_get_current_line(doc_list[idx].sci, pos))
			{
				sci_set_current_position(doc_list[idx].sci, cursor_pos_end);
				cursor_pos_end = -1;
			}
			break;
		}
		case GDK_Home:
		{
			if (cursor_pos_home_state == 0 || current_line != sci_get_current_line(doc_list[idx].sci, pos))
			{
				cursor_pos_home = sci_get_current_position(doc_list[idx].sci);
				sci_cmd(doc_list[idx].sci, SCI_VCHOME);
				cursor_pos_home_state = 1;
				current_line = sci_get_current_line(doc_list[idx].sci, pos);
			}
			else if (cursor_pos_home_state == 1 && current_line == sci_get_current_line(doc_list[idx].sci, pos))
			{
				sci_cmd(doc_list[idx].sci, SCI_HOME);
				cursor_pos_home_state = 2;
				cursor_pos_home_state = 0;
			}
			else// if (current_line == sci_get_current_line(doc_list[idx].sci, pos))
			{
				sci_set_current_position(doc_list[idx].sci, cursor_pos_home);
				cursor_pos_home_state = 0;
			}
			break;
		}
*/	}
	return ret;
}


// calls the edit popup menu in the editor
gboolean
on_editor_button_press_event           (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data)
{

#ifndef GEANY_WIN32
	if (event->button == 1)
	{
		utils_check_disk_status(GPOINTER_TO_INT(user_data), FALSE);
	}
#endif

	//if (event->type == GDK_2BUTTON_PRESS) geany_debug("double click");

	if (event->button == 3)
	{
		/// TODO pos should possibly be the position of the mouse pointer instead of the current sci position
		gint pos = sci_get_current_position(doc_list[GPOINTER_TO_INT(user_data)].sci);

		utils_find_current_word(doc_list[GPOINTER_TO_INT(user_data)].sci, pos, current_word);

		utils_update_popup_goto_items((current_word[0] != '\0') ? TRUE : FALSE);
		utils_update_popup_copy_items(GPOINTER_TO_INT(user_data));
			utils_update_insert_include_item(GPOINTER_TO_INT(user_data), 0);
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
	if (idx < 0) return;

	gchar *text = g_malloc(sci_get_selected_text_length(doc_list[idx].sci) + 1);
	sci_get_selected_text(doc_list[idx].sci, text);
	sci_replace_sel(doc_list[idx].sci, g_ascii_strdown(text, -1));
	g_free(text);
}


void
on_to_upper_case1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	if (idx < 0) return;

	gchar *text = malloc(sci_get_selected_text_length(doc_list[idx].sci) + 1);
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
		utils_update_statusbar(idx);
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
	gint i, j, type;
	const GPtrArray *tags;

	if (utils_strcmp(_("Goto tag definition"), gtk_label_get_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(menuitem))))))
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
					if (! utils_goto_workspace_tag(
							TM_TAG(tags->pdata[i])->atts.entry.file->work_object.file_name,
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
					utils_goto_workspace_tag(file, line);
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
				if (gdk_color_equal(&red, color))
				{
					gchar **array = g_strsplit(string, ":", 3);
					gint idx = document_get_cur_idx();
					gchar *file = g_path_get_basename(doc_list[idx].file_name);
					gint line = strtol(array[1], NULL, 10);

					if (line && utils_strcmp(array[0], file))
					{
						utils_goto_line(idx, line);
					}
					g_free(file);
					g_strfreev(array);
				}
				g_free(string);
			}
		}
	}

	if (event->button == 3)
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
			//gtk_clipboard_set_text(gtk_clipboard_get(gdk_atom_intern("CLIPBOARD", FALSE)), strrchr(string, ':') + 2, -1);
			gtk_clipboard_set_text(gtk_clipboard_get(gdk_atom_intern("CLIPBOARD", FALSE)), string, -1);
		}
		g_free(string);
	}

}


void
on_compile_button_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();

	if (sci_get_lexer(doc_list[idx].sci) == SCLEX_HTML ||
		sci_get_lexer(doc_list[idx].sci) == SCLEX_XML)
	{
#ifdef GEANY_WIN32
		gchar *uri = g_strconcat("file:///", g_path_skip_root(doc_list[idx].file_name), NULL);
#else
		gchar *uri = g_strconcat("file://", doc_list[idx].file_name, NULL);
#endif
		utils_start_browser(uri);
		g_free(uri);
	}
	else
	{
		on_build_compile_activate(NULL, NULL);
	}
}


void
on_build_compile_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	GPid child_pid = (GPid) 0;

	if (doc_list[idx].changed) document_save_file(idx);

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_C: child_pid = build_compile_c_file(idx); break;
		case GEANY_FILETYPES_CPP: child_pid = build_compile_cpp_file(idx); break;
		case GEANY_FILETYPES_JAVA: child_pid = build_compile_java_file(idx); break;
		case GEANY_FILETYPES_PASCAL: child_pid = build_compile_pascal_file(idx); break;
		case GEANY_FILETYPES_TEX: child_pid = build_compile_tex_file(idx, 0); break;
	}

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

	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_C: child_pid = build_link_c_file(idx); break;
		case GEANY_FILETYPES_CPP: child_pid = build_link_cpp_file(idx); break;
		/* FIXME: temporary switch to catch F5-shortcut pressed on LaTeX files, as long as
		 * LaTeX build menu has no key accelerator */
		case GEANY_FILETYPES_TEX: child_pid = build_compile_tex_file(idx, 1); break;
	}

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

		child_pid = build_make_c_file(idx, FALSE);
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

	/* FIXME: temporary switch to catch F5-shortcut pressed on LaTeX files, as long as
	 * LaTeX build menu has no key accelerator */
	switch (doc_list[idx].file_type->id)
	{
		case GEANY_FILETYPES_TEX:
		{
			if (build_view_tex_file(idx, 0) == (GPid) 0)
			{
				msgwin_status_add(_("Failed to execute the DVI view program"));
			}
			break;
		}
		default:
		{
			if (build_run_cmd(idx) == (GPid) 0)
			{
				msgwin_status_add(_("Failed to execute the terminal program"));
			}
		}
	}
	//gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
}


void
on_build_arguments_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	dialogs_show_includes_arguments_gen(GPOINTER_TO_INT(user_data));
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

		child_pid = build_make_c_file(idx, TRUE);
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
			document_replace_sel(idx, find, replace, search_flags_re, search_backwards_re);
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
	utils_start_browser(uri);
	g_free(uri);
}


void
on_help_shortcuts1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
#ifdef GEANY_WIN32
	gchar *pwd = g_get_current_dir();
	gchar *uri = g_strconcat("file:///", g_path_skip_root(pwd), "/doc/apa.html", NULL);
	g_free(pwd);
#else
	gchar *uri = g_strconcat("file://", DOCDIR, "apa.html", NULL);
#endif
	utils_start_browser(uri);
	g_free(uri);
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
	gchar *cur_tag = NULL;
	gint line = -1, pos = 0;

	if (doc_list[idx].file_type->id != GEANY_FILETYPES_JAVA &&
		doc_list[idx].file_type->id != GEANY_FILETYPES_ALL)
	{
		line = utils_get_current_tag(idx, &cur_tag);
		// utils_get_current_tag returns -1 on failure, so sci_get_position_from_line
		// returns the current position, soit should be safe
		pos = sci_get_position_from_line(doc_list[idx].sci, line - 1);
	}

	// if function name is unknown, set function name to "unknown"
	if (line == -1)
	{
		g_free(cur_tag);
		cur_tag = g_strdup(_("unknown"));
	}

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_PASCAL)
	{
		text = templates_get_template_function(GEANY_TEMPLATE_FUNCTION_PASCAL, cur_tag);
	}
	else
	{
		text = templates_get_template_function(GEANY_TEMPLATE_FUNCTION, cur_tag);
	}
	sci_insert_text(doc_list[idx].sci, pos, text);
	g_free(cur_tag);
	g_free(text);
}


void
on_comments_multiline_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gint idx = document_get_cur_idx();
	gchar *text;

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_PASCAL)
	{
		text = templates_get_template_generic(GEANY_TEMPLATE_MULTILINE_PASCAL);
	}
	else
	{
		text = templates_get_template_generic(GEANY_TEMPLATE_MULTILINE);
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

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_PASCAL)
	{
		text = templates_get_template_gpl(GEANY_TEMPLATE_GPL_PASCAL);
	}
	else
	{
		text = templates_get_template_gpl(GEANY_TEMPLATE_GPL);
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

	if (doc_list[idx].file_type->id == GEANY_FILETYPES_PASCAL)
	{
		text = templates_get_template_fileheader(GEANY_TEMPLATE_FILEHEADER_PASCAL, NULL, idx);
	}
	else
	{
		text = templates_get_template_fileheader(GEANY_TEMPLATE_FILEHEADER, NULL, idx);
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
		if (app->build_args_inc) g_free(app->build_args_inc);
		app->build_args_inc = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry1"))));
		if (app->build_args_libs) g_free(app->build_args_libs);
		app->build_args_libs = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry2"))));
		if (app->build_args_prog) g_free(app->build_args_prog);
		app->build_args_prog = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "includes_entry3"))));
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
		if (app->build_tex_dvi_cmd) g_free(app->build_tex_dvi_cmd);
		app->build_tex_dvi_cmd = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry1"))));
		if (app->build_tex_pdf_cmd) g_free(app->build_tex_pdf_cmd);
		app->build_tex_pdf_cmd = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry2"))));
		if (app->build_tex_view_dvi_cmd) g_free(app->build_tex_view_dvi_cmd);
		app->build_tex_view_dvi_cmd = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry3"))));
		if (app->build_tex_view_pdf_cmd) g_free(app->build_tex_view_pdf_cmd);
		app->build_tex_view_pdf_cmd = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "tex_entry4"))));
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}


void
on_recent_file_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gchar *locale_filename = g_locale_from_utf8((gchar*) user_data, -1, NULL, NULL, NULL);
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


