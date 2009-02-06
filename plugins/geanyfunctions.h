/* This file is generated automatically by genapi.py - do not edit.
 *
 * @file geanyfunctions.h @ref geany_functions wrappers.
 * This allows the use of normal API function names in plugins.
 * You need to declare the @ref geany_functions symbol yourself.
 */

#ifndef GEANY_FUNCTIONS_H
#define GEANY_FUNCTIONS_H

#define plugin_add_toolbar_item \
	geany_functions->p_plugin->add_toolbar_item
#define plugin_module_make_resident \
	geany_functions->p_plugin->module_make_resident
#define document_new_file \
	geany_functions->p_document->new_file
#define document_get_current \
	geany_functions->p_document->get_current
#define document_get_from_page \
	geany_functions->p_document->get_from_page
#define document_find_by_filename \
	geany_functions->p_document->find_by_filename
#define document_find_by_real_path \
	geany_functions->p_document->find_by_real_path
#define document_save_file \
	geany_functions->p_document->save_file
#define document_open_file \
	geany_functions->p_document->open_file
#define document_open_files \
	geany_functions->p_document->open_files
#define document_remove_page \
	geany_functions->p_document->remove_page
#define document_reload_file \
	geany_functions->p_document->reload_file
#define document_set_encoding \
	geany_functions->p_document->set_encoding
#define document_set_text_changed \
	geany_functions->p_document->set_text_changed
#define document_set_filetype \
	geany_functions->p_document->set_filetype
#define document_close \
	geany_functions->p_document->close
#define document_index \
	geany_functions->p_document->index
#define document_save_file_as \
	geany_functions->p_document->save_file_as
#define document_rename_file \
	geany_functions->p_document->rename_file
#define editor_get_indent_prefs \
	geany_functions->p_editor->get_indent_prefs
#define editor_create_widget \
	geany_functions->p_editor->create_widget
#define editor_indicator_set_on_range \
	geany_functions->p_editor->indicator_set_on_range
#define editor_indicator_set_on_line \
	geany_functions->p_editor->indicator_set_on_line
#define editor_indicator_clear \
	geany_functions->p_editor->indicator_clear
#define editor_set_indent_type \
	geany_functions->p_editor->set_indent_type
#define scintilla_send_message \
	geany_functions->p_scintilla->send_message
#define scintilla_new \
	geany_functions->p_scintilla->new
#define sci_send_command \
	geany_functions->p_sci->send_command
#define sci_start_undo_action \
	geany_functions->p_sci->start_undo_action
#define sci_end_undo_action \
	geany_functions->p_sci->end_undo_action
#define sci_set_text \
	geany_functions->p_sci->set_text
#define sci_insert_text \
	geany_functions->p_sci->insert_text
#define sci_get_text \
	geany_functions->p_sci->get_text
#define sci_get_length \
	geany_functions->p_sci->get_length
#define sci_get_current_position \
	geany_functions->p_sci->get_current_position
#define sci_set_current_position \
	geany_functions->p_sci->set_current_position
#define sci_get_col_from_position \
	geany_functions->p_sci->get_col_from_position
#define sci_get_line_from_position \
	geany_functions->p_sci->get_line_from_position
#define sci_get_position_from_line \
	geany_functions->p_sci->get_position_from_line
#define sci_replace_sel \
	geany_functions->p_sci->replace_sel
#define sci_get_selected_text \
	geany_functions->p_sci->get_selected_text
#define sci_get_selected_text_length \
	geany_functions->p_sci->get_selected_text_length
#define sci_get_selection_start \
	geany_functions->p_sci->get_selection_start
#define sci_get_selection_end \
	geany_functions->p_sci->get_selection_end
#define sci_get_selection_mode \
	geany_functions->p_sci->get_selection_mode
#define sci_set_selection_mode \
	geany_functions->p_sci->set_selection_mode
#define sci_set_selection_start \
	geany_functions->p_sci->set_selection_start
#define sci_set_selection_end \
	geany_functions->p_sci->set_selection_end
#define sci_get_text_range \
	geany_functions->p_sci->get_text_range
#define sci_get_line \
	geany_functions->p_sci->get_line
#define sci_get_line_length \
	geany_functions->p_sci->get_line_length
#define sci_get_line_count \
	geany_functions->p_sci->get_line_count
#define sci_get_line_is_visible \
	geany_functions->p_sci->get_line_is_visible
#define sci_ensure_line_is_visible \
	geany_functions->p_sci->ensure_line_is_visible
#define sci_scroll_caret \
	geany_functions->p_sci->scroll_caret
#define sci_find_matching_brace \
	geany_functions->p_sci->find_matching_brace
#define sci_get_style_at \
	geany_functions->p_sci->get_style_at
#define sci_get_char_at \
	geany_functions->p_sci->get_char_at
#define sci_get_current_line \
	geany_functions->p_sci->get_current_line
#define sci_has_selection \
	geany_functions->p_sci->has_selection
#define sci_get_tab_width \
	geany_functions->p_sci->get_tab_width
#define sci_indicator_clear \
	geany_functions->p_sci->indicator_clear
#define sci_indicator_set \
	geany_functions->p_sci->indicator_set
#define templates_get_template_fileheader \
	geany_functions->p_templates->get_template_fileheader
#define utils_str_equal \
	geany_functions->p_utils->str_equal
#define utils_string_replace_all \
	geany_functions->p_utils->string_replace_all
#define utils_get_file_list \
	geany_functions->p_utils->get_file_list
#define utils_write_file \
	geany_functions->p_utils->write_file
#define utils_get_locale_from_utf8 \
	geany_functions->p_utils->get_locale_from_utf8
#define utils_get_utf8_from_locale \
	geany_functions->p_utils->get_utf8_from_locale
#define utils_remove_ext_from_filename \
	geany_functions->p_utils->remove_ext_from_filename
#define utils_mkdir \
	geany_functions->p_utils->mkdir
#define utils_get_setting_boolean \
	geany_functions->p_utils->get_setting_boolean
#define utils_get_setting_integer \
	geany_functions->p_utils->get_setting_integer
#define utils_get_setting_string \
	geany_functions->p_utils->get_setting_string
#define utils_spawn_sync \
	geany_functions->p_utils->spawn_sync
#define utils_spawn_async \
	geany_functions->p_utils->spawn_async
#define utils_str_casecmp \
	geany_functions->p_utils->str_casecmp
#define utils_get_date_time \
	geany_functions->p_utils->get_date_time
#define utils_open_browser \
	geany_functions->p_utils->open_browser
#define utils_string_replace_first \
	geany_functions->p_utils->string_replace_first
#define ui_dialog_vbox_new \
	geany_functions->p_ui->dialog_vbox_new
#define ui_frame_new_with_alignment \
	geany_functions->p_ui->frame_new_with_alignment
#define ui_set_statusbar \
	geany_functions->p_ui->set_statusbar
#define ui_table_add_row \
	geany_functions->p_ui->table_add_row
#define ui_path_box_new \
	geany_functions->p_ui->path_box_new
#define ui_button_new_with_image \
	geany_functions->p_ui->button_new_with_image
#define ui_add_document_sensitive \
	geany_functions->p_ui->add_document_sensitive
#define ui_widget_set_tooltip_text \
	geany_functions->p_ui->widget_set_tooltip_text
#define ui_image_menu_item_new \
	geany_functions->p_ui->image_menu_item_new
#define ui_lookup_widget \
	geany_functions->p_ui->lookup_widget
#define ui_progress_bar_start \
	geany_functions->p_ui->progress_bar_start
#define ui_progress_bar_stop \
	geany_functions->p_ui->progress_bar_stop
#define ui_entry_add_clear_icon \
	geany_functions->p_ui->entry_add_clear_icon
#define dialogs_show_question \
	geany_functions->p_dialogs->show_question
#define dialogs_show_msgbox \
	geany_functions->p_dialogs->show_msgbox
#define dialogs_show_save_as \
	geany_functions->p_dialogs->show_save_as
#define dialogs_show_input_numeric \
	geany_functions->p_dialogs->show_input_numeric
#define msgwin_status_add \
	geany_functions->p_msgwin->status_add
#define msgwin_compiler_add \
	geany_functions->p_msgwin->compiler_add
#define msgwin_msg_add \
	geany_functions->p_msgwin->msg_add
#define msgwin_clear_tab \
	geany_functions->p_msgwin->clear_tab
#define msgwin_switch_tab \
	geany_functions->p_msgwin->switch_tab
#define encodings_convert_to_utf8 \
	geany_functions->p_encodings->convert_to_utf8
#define encodings_convert_to_utf8_from_charset \
	geany_functions->p_encodings->convert_to_utf8_from_charset
#define encodings_get_charset_from_index \
	geany_functions->p_encodings->get_charset_from_index
#define keybindings_send_command \
	geany_functions->p_keybindings->send_command
#define keybindings_set_item \
	geany_functions->p_keybindings->set_item
#define tm_get_real_path \
	geany_functions->p_tm->get_real_path
#define tm_source_file_new \
	geany_functions->p_tm->source_file_new
#define tm_workspace_add_object \
	geany_functions->p_tm->workspace_add_object
#define tm_source_file_update \
	geany_functions->p_tm->source_file_update
#define tm_work_object_free \
	geany_functions->p_tm->work_object_free
#define tm_workspace_remove_object \
	geany_functions->p_tm->workspace_remove_object
#define search_show_find_in_files_dialog \
	geany_functions->p_search->show_find_in_files_dialog
#define highlighting_get_style \
	geany_functions->p_highlighting->get_style
#define filetypes_detect_from_file \
	geany_functions->p_filetypes->detect_from_file
#define filetypes_lookup_by_name \
	geany_functions->p_filetypes->lookup_by_name
#define filetypes_index \
	geany_functions->p_filetypes->index
#define navqueue_goto_line \
	geany_functions->p_navqueue->goto_line
#define main_reload_configuration \
	geany_functions->p_main->reload_configuration
#define main_locale_init \
	geany_functions->p_main->locale_init

#endif
