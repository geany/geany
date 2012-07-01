/*
 *	  callbacks.h - this file is part of Geany, a fast and lightweight IDE
 *
 *	  Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *	  Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *	  This program is free software; you can redistribute it and/or modify
 *	  it under the terms of the GNU General Public License as published by
 *	  the Free Software Foundation; either version 2 of the License, or
 *	  (at your option) any later version.
 *
 *	  This program is distributed in the hope that it will be useful,
 *	  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	  GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include "geany.h" /* necessary for interface.c */


G_MODULE_EXPORT gboolean
on_exit_clicked						(GtkWidget *widget, gpointer gdata);

G_MODULE_EXPORT void
on_new1_activate					   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_save1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_save_as1_activate				   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_quit1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_info1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_open1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_change_font1_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_close_clicked			(GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_close_all_clicked		(GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_save_all1_activate				  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_close1_activate					 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_close_all1_activate				 (GtkMenuItem	 *menuitem,
										gpointer		  user_data);

G_MODULE_EXPORT void
on_crlf_activate					   (GtkCheckMenuItem *menuitem,
										gpointer		  user_data);

G_MODULE_EXPORT void
on_lf_activate						 (GtkCheckMenuItem *menuitem,
										gpointer		  user_data);

G_MODULE_EXPORT void
on_cr_activate						 (GtkCheckMenuItem *menuitem,
										gpointer		  user_data);

G_MODULE_EXPORT void
on_replace_tabs_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_search_clicked		   (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT gboolean
toolbar_popup_menu					 (GtkWidget *widget,
										GdkEventButton *event,
										gpointer user_data);

G_MODULE_EXPORT void
on_hide_toolbar1_activate			  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_undo1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_redo1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_cut1_activate					   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_copy1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_paste1_activate					 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_delete1_activate					(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_zoom_in1_activate				   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_zoom_out1_activate				  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbar_search_entry_changed		(GtkAction *action,
										const gchar *text,
										gpointer user_data);

G_MODULE_EXPORT void
on_toolbar_search_entry_activate	   (GtkAction *action,
										const gchar *text,
										gpointer user_data);

G_MODULE_EXPORT void
on_toggle_case1_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_fullscreen1_activate				(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_show_toolbar1_toggled			   (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_fullscreen1_toggled				 (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_markers_margin1_toggled			 (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_invisible1_activate				 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_find_usage1_activate				(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_show_messages_window1_toggled	   (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_color_schemes_activate		   (GtkImageMenuItem *imagemenuitem,
										gpointer user_data);

G_MODULE_EXPORT void
on_construct_completion1_activate	  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_count_words1_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_preferences1_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_normal_size1_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_edit1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_show_color_chooser1_activate		(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_find1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_find_next1_activate				 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_find_previous1_activate			 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);
G_MODULE_EXPORT void
on_find_nextsel1_activate			  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);
G_MODULE_EXPORT void
on_find_prevsel1_activate			  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_replace1_activate				   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_find_in_files1_activate			 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_new_clicked			  (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_open_clicked			 (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_save_clicked			 (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_quit_clicked			 (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_preferences_clicked	  (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_reload_clicked		   (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_go_to_line_activate				 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_help1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_compile_clicked		  (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_website1_activate				   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_help_menu_item_donate_activate(GtkMenuItem *item, gpointer user_data);

G_MODULE_EXPORT void
on_help_menu_item_wiki_activate(GtkMenuItem *item, gpointer user_data);

G_MODULE_EXPORT void
on_help_menu_item_bug_report_activate(GtkMenuItem *item, gpointer user_data);

G_MODULE_EXPORT void
on_line_wrapping1_toggled			   (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_comments_function_activate		  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_comments_multiline_activate		 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_comments_changelog_activate		 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_comments_gpl_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_comments_fileheader_activate		(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_insert_include_activate			 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_custom_date_dialog_response		 (GtkDialog *dialog,
										gint response,
										gpointer user_data);

G_MODULE_EXPORT void
on_custom_date_entry_activate		  (GtkEntry		*entry,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_insert_date_activate				(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_set_file_readonly1_toggled		  (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_tv_notebook_switch_page			 (GtkNotebook	 *notebook,
										GtkNotebookPage *page,
										guint			page_num,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_help_shortcuts1_activate			(GtkMenuItem	 *menuitem,
										gpointer		 user_data);


G_MODULE_EXPORT void
on_file_properties_activate			(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_show_line_numbers1_toggled		  (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_use_auto_indentation1_toggled	   (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);



G_MODULE_EXPORT void
on_menu_fold_all1_activate			 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_unfold_all1_activate		   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_goto_entry_activate	  (GtkAction *action, const gchar *text, gpointer user_data);

G_MODULE_EXPORT void
on_toolbutton_goto_clicked			 (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_toolbutton_run_clicked			  (GtkAction	   *action,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_notebook1_switch_page_after		 (GtkNotebook	 *notebook,
										GtkNotebookPage *page,
										guint			page_num,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_remove_indicators1_activate	(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_reload_as_activate				  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_print1_activate					 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_select_all1_activate		   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_show_sidebar1_toggled		  (GtkCheckMenuItem *checkmenuitem,
										gpointer		  user_data);

G_MODULE_EXPORT void
on_menu_write_unicode_bom1_toggled	 (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_comment_line1_activate		 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_uncomment_line1_activate	   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_increase_indent1_activate	  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_decrease_indent1_activate	  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_toggle_line_commentation1_activate
									   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_next_message1_activate			  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);


G_MODULE_EXPORT void
on_menu_comments_multiline_activate	(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_comments_gpl_activate		  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_insert_include_activate		(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_insert_date_activate		   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_comments_bsd_activate		  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_comments_bsd_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_project_new1_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_project_open1_activate			  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_project_close1_activate			 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_project_properties1_activate		(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_project1_activate			  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_open_selected_file1_activate   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_remove_markers1_activate			(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_load_tags1_activate				 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_context_action1_activate			(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_toggle_all_additional_widgets1_activate
										(GtkMenuItem	 *menuitem,
										gpointer		 user_data);
G_MODULE_EXPORT void
on_back_activate					   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_forward_activate					(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_file1_activate					  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT gboolean
on_motion_event						(GtkWidget	   *widget,
										GdkEventMotion  *event,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_tv_notebook_switch_page_after	   (GtkNotebook	 *notebook,
										GtkNotebookPage *page,
										guint			page_num,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_tabs1_activate					  (GtkCheckMenuItem *menuitem,
										gpointer		  user_data);

G_MODULE_EXPORT void
on_spaces1_activate					(GtkCheckMenuItem *menuitem,
										gpointer		  user_data);


G_MODULE_EXPORT void
on_strip_trailing_spaces1_activate	 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_page_setup1_activate				(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT gboolean
on_escape_key_press_event			  (GtkWidget	   *widget,
										GdkEventKey	 *event,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_line_breaking1_activate			 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);


G_MODULE_EXPORT void
on_replace_spaces_activate			 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_previous_message1_activate		  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_search1_activate					(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_close_other_documents1_activate	 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_reload_configuration1_activate (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_find_document_usage1_activate	   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_tabs_and_spaces1_activate		   (GtkCheckMenuItem *menuitem,
										gpointer		  user_data);
G_MODULE_EXPORT void
on_debug_messages1_activate			(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_show_white_space1_toggled	  (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_show_line_endings1_toggled	 (GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_menu_show_indentation_guides1_toggled
										(GtkCheckMenuItem *checkmenuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_send_selection_to_vte1_activate	 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT gboolean
on_window_state_event		 (GtkWidget		   *widget,
										GdkEventWindowState *event,
										gpointer			 user_data);

G_MODULE_EXPORT void
on_customize_toolbar1_activate		 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_button_customize_toolbar_clicked	(GtkButton	   *button,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_cut_current_lines1_activate	   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_copy_current_lines1_activate	  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_delete_current_lines1_activate	(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_duplicate_line_or_selection1_activate
										(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_select_current_lines1_activate	(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_select_current_paragraph1_activate  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_insert_alternative_white_space1_activate
										(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_go_to_next_marker1_activate		 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_go_to_previous_marker1_activate	 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_reflow_lines_block1_activate		(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_transpose_current_line1_activate	(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_smart_line_indent1_activate		 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_plugin_preferences1_activate		(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_goto_tag_definition1				(GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_goto_tag_declaration1			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_indent_width_activate			   (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_reset_indentation1_activate		 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);


G_MODULE_EXPORT void
on_mark_all1_activate				  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_detect_type_from_file_activate	  (GtkMenuItem	 *menuitem,
										gpointer		 user_data);

G_MODULE_EXPORT void
on_detect_width_from_file_activate	 (GtkMenuItem	 *menuitem,
										gpointer		 user_data);
