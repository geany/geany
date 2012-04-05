/* This file is generated automatically by genapi.py - do not edit. */

/** @file geanyfunctions.h @ref geany_functions wrappers.
 * This allows the use of normal API function names in plugins by defining macros.
 *
 * E.g.:@code
 * #define plugin_add_toolbar_item \
 * 	geany_functions->p_plugin->plugin_add_toolbar_item @endcode
 *
 * You need to declare the @ref geany_functions symbol yourself.
 *
 * Note: This must be included after all other API headers to prevent conflicts with
 * other header's function prototypes - this is done for you when using geanyplugin.h.
 */

#ifndef GEANY_FUNCTIONS_H
#define GEANY_FUNCTIONS_H

#define plugin_add_toolbar_item \
	geany_functions->p_plugin->plugin_add_toolbar_item
#define plugin_module_make_resident \
	geany_functions->p_plugin->plugin_module_make_resident
#define plugin_signal_connect \
	geany_functions->p_plugin->plugin_signal_connect
#define plugin_set_key_group \
	geany_functions->p_plugin->plugin_set_key_group
#define plugin_show_configure \
	geany_functions->p_plugin->plugin_show_configure
#define plugin_timeout_add \
	geany_functions->p_plugin->plugin_timeout_add
#define plugin_timeout_add_seconds \
	geany_functions->p_plugin->plugin_timeout_add_seconds
#define plugin_idle_add \
	geany_functions->p_plugin->plugin_idle_add
#define document_new_file \
	geany_functions->p_document->document_new_file
#define document_get_current \
	geany_functions->p_document->document_get_current
#define document_get_from_page \
	geany_functions->p_document->document_get_from_page
#define document_find_by_filename \
	geany_functions->p_document->document_find_by_filename
#define document_find_by_real_path \
	geany_functions->p_document->document_find_by_real_path
#define document_save_file \
	geany_functions->p_document->document_save_file
#define document_open_file \
	geany_functions->p_document->document_open_file
#define document_open_files \
	geany_functions->p_document->document_open_files
#define document_remove_page \
	geany_functions->p_document->document_remove_page
#define document_reload_file \
	geany_functions->p_document->document_reload_file
#define document_set_encoding \
	geany_functions->p_document->document_set_encoding
#define document_set_text_changed \
	geany_functions->p_document->document_set_text_changed
#define document_set_filetype \
	geany_functions->p_document->document_set_filetype
#define document_close \
	geany_functions->p_document->document_close
#define document_index \
	geany_functions->p_document->document_index
#define document_save_file_as \
	geany_functions->p_document->document_save_file_as
#define document_rename_file \
	geany_functions->p_document->document_rename_file
#define document_get_status_color \
	geany_functions->p_document->document_get_status_color
#define document_get_basename_for_display \
	geany_functions->p_document->document_get_basename_for_display
#define document_get_notebook_page \
	geany_functions->p_document->document_get_notebook_page
#define document_compare_by_display_name \
	geany_functions->p_document->document_compare_by_display_name
#define document_compare_by_tab_order \
	geany_functions->p_document->document_compare_by_tab_order
#define document_compare_by_tab_order_reverse \
	geany_functions->p_document->document_compare_by_tab_order_reverse
#define editor_get_indent_prefs \
	geany_functions->p_editor->editor_get_indent_prefs
#define editor_create_widget \
	geany_functions->p_editor->editor_create_widget
#define editor_indicator_set_on_range \
	geany_functions->p_editor->editor_indicator_set_on_range
#define editor_indicator_set_on_line \
	geany_functions->p_editor->editor_indicator_set_on_line
#define editor_indicator_clear \
	geany_functions->p_editor->editor_indicator_clear
#define editor_set_indent_type \
	geany_functions->p_editor->editor_set_indent_type
#define editor_get_word_at_pos \
	geany_functions->p_editor->editor_get_word_at_pos
#define editor_get_eol_char_name \
	geany_functions->p_editor->editor_get_eol_char_name
#define editor_get_eol_char_len \
	geany_functions->p_editor->editor_get_eol_char_len
#define editor_get_eol_char \
	geany_functions->p_editor->editor_get_eol_char
#define editor_insert_text_block \
	geany_functions->p_editor->editor_insert_text_block
#define editor_get_eol_char_mode \
	geany_functions->p_editor->editor_get_eol_char_mode
#define editor_goto_pos \
	geany_functions->p_editor->editor_goto_pos
#define editor_find_snippet \
	geany_functions->p_editor->editor_find_snippet
#define editor_insert_snippet \
	geany_functions->p_editor->editor_insert_snippet
#define scintilla_send_message \
	geany_functions->p_scintilla->scintilla_send_message
#define scintilla_new \
	geany_functions->p_scintilla->scintilla_new
#define sci_send_command \
	geany_functions->p_sci->sci_send_command
#define sci_start_undo_action \
	geany_functions->p_sci->sci_start_undo_action
#define sci_end_undo_action \
	geany_functions->p_sci->sci_end_undo_action
#define sci_set_text \
	geany_functions->p_sci->sci_set_text
#define sci_insert_text \
	geany_functions->p_sci->sci_insert_text
#define sci_get_text \
	geany_functions->p_sci->sci_get_text
#define sci_get_length \
	geany_functions->p_sci->sci_get_length
#define sci_get_current_position \
	geany_functions->p_sci->sci_get_current_position
#define sci_set_current_position \
	geany_functions->p_sci->sci_set_current_position
#define sci_get_col_from_position \
	geany_functions->p_sci->sci_get_col_from_position
#define sci_get_line_from_position \
	geany_functions->p_sci->sci_get_line_from_position
#define sci_get_position_from_line \
	geany_functions->p_sci->sci_get_position_from_line
#define sci_replace_sel \
	geany_functions->p_sci->sci_replace_sel
#define sci_get_selected_text \
	geany_functions->p_sci->sci_get_selected_text
#define sci_get_selected_text_length \
	geany_functions->p_sci->sci_get_selected_text_length
#define sci_get_selection_start \
	geany_functions->p_sci->sci_get_selection_start
#define sci_get_selection_end \
	geany_functions->p_sci->sci_get_selection_end
#define sci_get_selection_mode \
	geany_functions->p_sci->sci_get_selection_mode
#define sci_set_selection_mode \
	geany_functions->p_sci->sci_set_selection_mode
#define sci_set_selection_start \
	geany_functions->p_sci->sci_set_selection_start
#define sci_set_selection_end \
	geany_functions->p_sci->sci_set_selection_end
#define sci_get_text_range \
	geany_functions->p_sci->sci_get_text_range
#define sci_get_line \
	geany_functions->p_sci->sci_get_line
#define sci_get_line_length \
	geany_functions->p_sci->sci_get_line_length
#define sci_get_line_count \
	geany_functions->p_sci->sci_get_line_count
#define sci_get_line_is_visible \
	geany_functions->p_sci->sci_get_line_is_visible
#define sci_ensure_line_is_visible \
	geany_functions->p_sci->sci_ensure_line_is_visible
#define sci_scroll_caret \
	geany_functions->p_sci->sci_scroll_caret
#define sci_find_matching_brace \
	geany_functions->p_sci->sci_find_matching_brace
#define sci_get_style_at \
	geany_functions->p_sci->sci_get_style_at
#define sci_get_char_at \
	geany_functions->p_sci->sci_get_char_at
#define sci_get_current_line \
	geany_functions->p_sci->sci_get_current_line
#define sci_has_selection \
	geany_functions->p_sci->sci_has_selection
#define sci_get_tab_width \
	geany_functions->p_sci->sci_get_tab_width
#define sci_indicator_clear \
	geany_functions->p_sci->sci_indicator_clear
#define sci_indicator_set \
	geany_functions->p_sci->sci_indicator_set
#define sci_get_contents \
	geany_functions->p_sci->sci_get_contents
#define sci_get_contents_range \
	geany_functions->p_sci->sci_get_contents_range
#define sci_get_selection_contents \
	geany_functions->p_sci->sci_get_selection_contents
#define sci_set_font \
	geany_functions->p_sci->sci_set_font
#define sci_get_line_end_position \
	geany_functions->p_sci->sci_get_line_end_position
#define sci_set_target_start \
	geany_functions->p_sci->sci_set_target_start
#define sci_set_target_end \
	geany_functions->p_sci->sci_set_target_end
#define sci_replace_target \
	geany_functions->p_sci->sci_replace_target
#define sci_set_marker_at_line \
	geany_functions->p_sci->sci_set_marker_at_line
#define sci_delete_marker_at_line \
	geany_functions->p_sci->sci_delete_marker_at_line
#define sci_is_marker_set_at_line \
	geany_functions->p_sci->sci_is_marker_set_at_line
#define sci_goto_line \
	geany_functions->p_sci->sci_goto_line
#define sci_find_text \
	geany_functions->p_sci->sci_find_text
#define sci_set_line_indentation \
	geany_functions->p_sci->sci_set_line_indentation
#define sci_get_line_indentation \
	geany_functions->p_sci->sci_get_line_indentation
#define sci_get_lexer \
	geany_functions->p_sci->sci_get_lexer
#define templates_get_template_fileheader \
	geany_functions->p_templates->templates_get_template_fileheader
#define utils_str_equal \
	geany_functions->p_utils->utils_str_equal
#define utils_string_replace_all \
	geany_functions->p_utils->utils_string_replace_all
#define utils_get_file_list \
	geany_functions->p_utils->utils_get_file_list
#define utils_write_file \
	geany_functions->p_utils->utils_write_file
#define utils_get_locale_from_utf8 \
	geany_functions->p_utils->utils_get_locale_from_utf8
#define utils_get_utf8_from_locale \
	geany_functions->p_utils->utils_get_utf8_from_locale
#define utils_remove_ext_from_filename \
	geany_functions->p_utils->utils_remove_ext_from_filename
#define utils_mkdir \
	geany_functions->p_utils->utils_mkdir
#define utils_get_setting_boolean \
	geany_functions->p_utils->utils_get_setting_boolean
#define utils_get_setting_integer \
	geany_functions->p_utils->utils_get_setting_integer
#define utils_get_setting_string \
	geany_functions->p_utils->utils_get_setting_string
#define utils_spawn_sync \
	geany_functions->p_utils->utils_spawn_sync
#define utils_spawn_async \
	geany_functions->p_utils->utils_spawn_async
#define utils_str_casecmp \
	geany_functions->p_utils->utils_str_casecmp
#define utils_get_date_time \
	geany_functions->p_utils->utils_get_date_time
#define utils_open_browser \
	geany_functions->p_utils->utils_open_browser
#define utils_string_replace_first \
	geany_functions->p_utils->utils_string_replace_first
#define utils_str_middle_truncate \
	geany_functions->p_utils->utils_str_middle_truncate
#define utils_str_remove_chars \
	geany_functions->p_utils->utils_str_remove_chars
#define utils_get_file_list_full \
	geany_functions->p_utils->utils_get_file_list_full
#define utils_copy_environment \
	geany_functions->p_utils->utils_copy_environment
#define utils_find_open_xml_tag \
	geany_functions->p_utils->utils_find_open_xml_tag
#define utils_find_open_xml_tag_pos \
	geany_functions->p_utils->utils_find_open_xml_tag_pos
#define ui_dialog_vbox_new \
	geany_functions->p_ui->ui_dialog_vbox_new
#define ui_frame_new_with_alignment \
	geany_functions->p_ui->ui_frame_new_with_alignment
#define ui_set_statusbar \
	geany_functions->p_ui->ui_set_statusbar
#define ui_table_add_row \
	geany_functions->p_ui->ui_table_add_row
#define ui_path_box_new \
	geany_functions->p_ui->ui_path_box_new
#define ui_button_new_with_image \
	geany_functions->p_ui->ui_button_new_with_image
#define ui_add_document_sensitive \
	geany_functions->p_ui->ui_add_document_sensitive
#define ui_widget_set_tooltip_text \
	geany_functions->p_ui->ui_widget_set_tooltip_text
#define ui_image_menu_item_new \
	geany_functions->p_ui->ui_image_menu_item_new
#define ui_lookup_widget \
	geany_functions->p_ui->ui_lookup_widget
#define ui_progress_bar_start \
	geany_functions->p_ui->ui_progress_bar_start
#define ui_progress_bar_stop \
	geany_functions->p_ui->ui_progress_bar_stop
#define ui_entry_add_clear_icon \
	geany_functions->p_ui->ui_entry_add_clear_icon
#define ui_menu_add_document_items \
	geany_functions->p_ui->ui_menu_add_document_items
#define ui_widget_modify_font_from_string \
	geany_functions->p_ui->ui_widget_modify_font_from_string
#define ui_is_keyval_enter_or_return \
	geany_functions->p_ui->ui_is_keyval_enter_or_return
#define ui_get_gtk_settings_integer \
	geany_functions->p_ui->ui_get_gtk_settings_integer
#define ui_combo_box_add_to_history \
	geany_functions->p_ui->ui_combo_box_add_to_history
#define ui_menu_add_document_items_sorted \
	geany_functions->p_ui->ui_menu_add_document_items_sorted
#define ui_lookup_stock_label \
	geany_functions->p_ui->ui_lookup_stock_label
#define dialogs_show_question \
	geany_functions->p_dialogs->dialogs_show_question
#define dialogs_show_msgbox \
	geany_functions->p_dialogs->dialogs_show_msgbox
#define dialogs_show_save_as \
	geany_functions->p_dialogs->dialogs_show_save_as
#define dialogs_show_input_numeric \
	geany_functions->p_dialogs->dialogs_show_input_numeric
#define dialogs_show_input \
	geany_functions->p_dialogs->dialogs_show_input
#define msgwin_status_add \
	geany_functions->p_msgwin->msgwin_status_add
#define msgwin_compiler_add \
	geany_functions->p_msgwin->msgwin_compiler_add
#define msgwin_msg_add \
	geany_functions->p_msgwin->msgwin_msg_add
#define msgwin_clear_tab \
	geany_functions->p_msgwin->msgwin_clear_tab
#define msgwin_switch_tab \
	geany_functions->p_msgwin->msgwin_switch_tab
#define msgwin_set_messages_dir \
	geany_functions->p_msgwin->msgwin_set_messages_dir
#define encodings_convert_to_utf8 \
	geany_functions->p_encodings->encodings_convert_to_utf8
#define encodings_convert_to_utf8_from_charset \
	geany_functions->p_encodings->encodings_convert_to_utf8_from_charset
#define encodings_get_charset_from_index \
	geany_functions->p_encodings->encodings_get_charset_from_index
#define keybindings_send_command \
	geany_functions->p_keybindings->keybindings_send_command
#define keybindings_set_item \
	geany_functions->p_keybindings->keybindings_set_item
#define keybindings_get_item \
	geany_functions->p_keybindings->keybindings_get_item
#define tm_get_real_path \
	geany_functions->p_tm->tm_get_real_path
#define tm_source_file_new \
	geany_functions->p_tm->tm_source_file_new
#define tm_workspace_add_object \
	geany_functions->p_tm->tm_workspace_add_object
#define tm_source_file_update \
	geany_functions->p_tm->tm_source_file_update
#define tm_work_object_free \
	geany_functions->p_tm->tm_work_object_free
#define tm_workspace_remove_object \
	geany_functions->p_tm->tm_workspace_remove_object
#define search_show_find_in_files_dialog \
	geany_functions->p_search->search_show_find_in_files_dialog
#define highlighting_get_style \
	geany_functions->p_highlighting->highlighting_get_style
#define highlighting_set_styles \
	geany_functions->p_highlighting->highlighting_set_styles
#define highlighting_is_string_style \
	geany_functions->p_highlighting->highlighting_is_string_style
#define highlighting_is_comment_style \
	geany_functions->p_highlighting->highlighting_is_comment_style
#define highlighting_is_code_style \
	geany_functions->p_highlighting->highlighting_is_code_style
#define filetypes_detect_from_file \
	geany_functions->p_filetypes->filetypes_detect_from_file
#define filetypes_lookup_by_name \
	geany_functions->p_filetypes->filetypes_lookup_by_name
#define filetypes_index \
	geany_functions->p_filetypes->filetypes_index
#define filetypes_get_display_name \
	geany_functions->p_filetypes->filetypes_get_display_name
#define filetypes_get_sorted_by_name \
	geany_functions->p_filetypes->filetypes_get_sorted_by_name
#define navqueue_goto_line \
	geany_functions->p_navqueue->navqueue_goto_line
#define main_reload_configuration \
	geany_functions->p_main->main_reload_configuration
#define main_locale_init \
	geany_functions->p_main->main_locale_init
#define main_is_realized \
	geany_functions->p_main->main_is_realized
#define stash_group_new \
	geany_functions->p_stash->stash_group_new
#define stash_group_add_boolean \
	geany_functions->p_stash->stash_group_add_boolean
#define stash_group_add_integer \
	geany_functions->p_stash->stash_group_add_integer
#define stash_group_add_string \
	geany_functions->p_stash->stash_group_add_string
#define stash_group_add_string_vector \
	geany_functions->p_stash->stash_group_add_string_vector
#define stash_group_load_from_key_file \
	geany_functions->p_stash->stash_group_load_from_key_file
#define stash_group_save_to_key_file \
	geany_functions->p_stash->stash_group_save_to_key_file
#define stash_group_free \
	geany_functions->p_stash->stash_group_free
#define stash_group_load_from_file \
	geany_functions->p_stash->stash_group_load_from_file
#define stash_group_save_to_file \
	geany_functions->p_stash->stash_group_save_to_file
#define stash_group_add_toggle_button \
	geany_functions->p_stash->stash_group_add_toggle_button
#define stash_group_add_radio_buttons \
	geany_functions->p_stash->stash_group_add_radio_buttons
#define stash_group_add_spin_button_integer \
	geany_functions->p_stash->stash_group_add_spin_button_integer
#define stash_group_add_combo_box \
	geany_functions->p_stash->stash_group_add_combo_box
#define stash_group_add_combo_box_entry \
	geany_functions->p_stash->stash_group_add_combo_box_entry
#define stash_group_add_entry \
	geany_functions->p_stash->stash_group_add_entry
#define stash_group_add_widget_property \
	geany_functions->p_stash->stash_group_add_widget_property
#define stash_group_display \
	geany_functions->p_stash->stash_group_display
#define stash_group_update \
	geany_functions->p_stash->stash_group_update
#define stash_group_free_settings \
	geany_functions->p_stash->stash_group_free_settings
#define symbols_get_context_separator \
	geany_functions->p_symbols->symbols_get_context_separator
#define build_activate_menu_item \
	geany_functions->p_build->build_activate_menu_item
#define build_get_current_menu_item \
	geany_functions->p_build->build_get_current_menu_item
#define build_remove_menu_item \
	geany_functions->p_build->build_remove_menu_item
#define build_set_menu_item \
	geany_functions->p_build->build_set_menu_item
#define build_get_group_count \
	geany_functions->p_build->build_get_group_count

#endif
