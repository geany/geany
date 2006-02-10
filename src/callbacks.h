/*
 *      callbacks.h - this file is part of Geany, a fast and lightweight IDE
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


void
signal_cb							   (gint sig);

gint
destroyapp_early                       (void);

gint
destroyapp                             (GtkWidget *widget, gpointer gdata);

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
on_tab_close_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_file_open_dialog_response           (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_notebook1_switch_page               (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_file_save_cancel_button_clicked     (GtkButton       *button,
                                        gpointer         user_data);

void
on_file_save_save_button_clicked       (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_filesavedialog1_delete_event        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_color_ok_button_clicked             (GtkButton       *button,
                                        gpointer         user_data);

void
on_color_cancel_button_clicked         (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_color_delete_event                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_save_all1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_close1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_font_ok_button_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_font_cancel_button_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_font_apply_button_clicked           (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_fontselectiondialog1_delete_event   (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_close_all1_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_editor_key_press_event              (GtkWidget *widget,
                                        GdkEventKey *event,
                                        gpointer user_data);


gboolean
on_editor_button_press_event           (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data);

gboolean
on_tree_view_button_press_event        (GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer user_data);

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
on_indention_guides_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_white_space_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton18_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_entry1_activate                     (GtkEntry        *entry,
                                        gpointer         user_data);

gboolean
toolbar_popup_menu                    (GtkWidget *widget,
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
on_openfiles_tree_selection_changed    (GtkTreeSelection *selection,
                                        gpointer data);

void
on_taglist_tree_selection_changed      (GtkTreeSelection *selection,
                                        gpointer data);

void
on_filetype_change                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_to_lower_case1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_to_upper_case1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_fullscreen1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_window_configure_event              (GtkWidget *widget,
                                        GdkEventConfigure *event,
                                        gpointer user_data);

void
on_markers_margin1_activate            (GtkMenuItem     *menuitem,
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
on_show_indention_guides1_toggled      (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_show_white_space_toggled            (GtkCheckMenuItem *checkmenuitem,
                                        gpointer         user_data);

void
on_fullscreen1_deselect                (GtkItem         *item,
                                        gpointer         user_data);

void
on_fullscreen1_select                  (GtkItem         *item,
                                        gpointer         user_data);

void
on_invisible1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbutton13_clicked                (GtkToolButton   *toolbutton,
                                        gpointer         user_data);

void
on_xml_tag1_activate                   (GtkMenuItem     *menuitem,
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
on_compiler_treeview_copy_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_message_treeview_clear_activate     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_show_line_endings1_toggled          (GtkCheckMenuItem *checkmenuitem,
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
on_build_compile_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_build_tex_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_build_build_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_build_make_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_build_execute_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_build_arguments_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_build_tex_arguments_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_make_target_dialog_response         (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_make_target_entry_activate          (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_find1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_find_next1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_find_dialog_response                (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_find_entry_activate                 (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_replace_dialog_response                (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_replace_entry_activate                 (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_new_with_template                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


void
on_toolbutton_new_clicked              (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbutton_new_pressed              (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbutton_new_released             (GtkButton       *button,
                                        gpointer         user_data);
gint
show_new_file_menu                     (gpointer         data);


void
on_go_to_line_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_includes_arguments_dialog_response  (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_includes_arguments_tex_dialog_response  (GtkDialog *dialog,
                                            gint response,
                                            gpointer user_data);

void
on_goto_line_dialog_response           (GtkDialog *dialog,
                                        gint response,
                                        gpointer user_data);

void
on_goto_line_entry_activate            (GtkEntry        *entry,
                                        gpointer         user_data);


void
on_use_auto_indention1_activate        (GtkMenuItem     *menuitem,
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
on_pref_tools_button_clicked           (GtkButton       *button,
                                        gpointer         user_data);


void
on_line_breaking1_toggled              (GtkCheckMenuItem *checkmenuitem,
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
on_recent_file_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_file_open_selection_changed         (GtkFileChooser  *filechooser,
                                        gpointer         user_data);

void
on_file_open_entry_activate            (GtkEntry        *entry,
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
on_file_open_check_hidden_toggled      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_replace1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_tv_notebook_switch_page             (GtkNotebook     *notebook,
                                        GtkNotebookPage *page,
                                        guint            page_num,
                                        gpointer         user_data);

void
on_openfiles_tree_popup_clicked        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_taglist_tree_popup_clicked          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_window_key_press_event              (GtkWidget *widget,
                                        GdkEventKey *event,
                                        gpointer user_data);

void
on_help_shortcuts1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

