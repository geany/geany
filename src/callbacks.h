/*
 *      callbacks.h - this file is part of Geany, a fast and lightweight IDE
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


#include "geany.h" /* necessary for interface.c */


typedef struct
{
	GeanyDocument *last_doc;
} CallbacksData;

extern CallbacksData callbacks_data;


gboolean
on_exit_clicked                        (GtkWidget *widget, gpointer gdata);

void
on_new1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_save_as1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_quit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_info1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_open1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton19_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_toolbutton6_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_change_font1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton8_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_toolbutton9_clicked                 (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_toolbutton10_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_toolbutton15_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_toolbutton23_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_notebook1_switch_page               (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_save_all1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_close1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_close_all1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_crlf_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_lf_activate                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_cr_activate                         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_replace_tabs_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton18_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_entry1_activate                     (GtkEntry        *entry,
                                        gpointer         user_data);

gboolean
toolbar_popup_menu                     (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data);

void
on_images_and_text2_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_images_only2_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_text_only2_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_hide_toolbar1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_undo1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_redo1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_cut1_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_copy1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_paste1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_delete1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton20_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_toolbutton21_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_zoom_in1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_zoom_out1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_entry1_changed                      (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_toggle_case1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_fullscreen1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_show_toolbar1_toggled               (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_fullscreen1_toggled                 (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_markers_margin1_toggled             (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_invisible1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton13_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_find_usage1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_show_messages_window1_toggled       (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_goto_tag_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_construct_completion1_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_count_words1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_preferences1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_normal_size1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_edit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_show_color_chooser1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_find1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_find_next1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_find_previous1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
void
on_find_nextsel1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
void
on_find_prevsel1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_replace1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_find_in_files1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton_new_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_go_to_line_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_goto_line_dialog_response           (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_goto_line_entry_activate            (GtkEntry        *entry,
                                        gpointer         user_data);


void
on_help1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_compile_button_clicked              (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_website1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_line_wrapping1_toggled              (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_comments_function_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_comments_multiline_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_comments_changelog_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_comments_gpl_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_comments_fileheader_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_insert_include_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_custom_date_dialog_response         (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_custom_date_entry_activate          (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_insert_date_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_large_icons1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_small_icons1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_set_file_readonly1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_tv_notebook_switch_page             (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_help_shortcuts1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


void
on_file_properties_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_show_line_numbers1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_use_auto_indentation1_toggled       (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);



void
on_menu_fold_all1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_unfold_all1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_entry_goto_line_activate            (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_toolbutton_goto_clicked             (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_run_button_clicked                  (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_go_to_line1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_notebook1_switch_page_after         (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_menu_remove_indicators1_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_encoding_change                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_reload_as_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_print1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_select_all1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_show_sidebar1_toggled          (GtkCheckMenuItem *checkmenuitem,
                                        gpointer          user_data);

void
on_menu_write_unicode_bom1_toggled     (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_menu_comment_line1_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_uncomment_line1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_duplicate_line1_activate       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_increase_indent1_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_decrease_indent1_activate      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_toggle_line_commentation1_activate
                                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_next_message1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


void
on_menu_comments_multiline_activate    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_comments_gpl_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_insert_include_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_insert_date_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_comments_bsd_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_comments_bsd_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_project_new1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_project_open1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_project_close1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_project_properties1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_project1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_open_selected_file1_activate   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_remove_markers1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_load_tags1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_context_action1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_menu_toggle_all_additional_widgets1_activate
                                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
void
on_back_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_forward_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_file1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_motion_event                        (GtkWidget       *widget,
                                        GdkEventMotion  *event,
                                        gpointer         user_data);

void
on_tv_notebook_switch_page_after       (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_tabs1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_spaces1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


void
on_strip_trailing_spaces1_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_page_setup1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_tools1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_escape_key_press_event              (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
on_line_breaking1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


void
on_replace_spaces_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_previous_message1_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_search1_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_close_other_documents1_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
