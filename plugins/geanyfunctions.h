/* @file geanyfunctions.h @ref geany_functions wrappers. 
This allows the use of normal API function names in plugins. */

#ifndef GEANY_FUNCTIONS_H
#define GEANY_FUNCTIONS_H

#include "pluginmacros.h"

#define plugin_add_toolbar_item \
	p_plugin->add_toolbar_item
#define document_new_file \
	p_document->new_file
#define document_get_current \
	p_document->get_current
#define document_get_from_page \
	p_document->get_from_page
#define document_find_by_filename \
	p_document->find_by_filename
#define document_find_by_real_path \
	p_document->find_by_real_path
#define document_save_file \
	p_document->save_file
#define document_open_file \
	p_document->open_file
#define document_open_files \
	p_document->open_files
#define document_remove_page \
	p_document->remove_page
#define document_reload_file \
	p_document->reload_file
#define document_set_encoding \
	p_document->set_encoding
#define document_set_text_changed \
	p_document->set_text_changed
#define document_set_filetype \
	p_document->set_filetype
#define document_close \
	p_document->close
#define document_index \
	p_document->index
#define editor_get_indent_prefs \
	p_editor->get_indent_prefs
#define editor_create_widget \
	p_editor->create_widget
#define editor_indicator_set_on_range \
	p_editor->indicator_set_on_range
#define editor_indicator_set_on_line \
	p_editor->indicator_set_on_line
#define editor_indicator_clear \
	p_editor->indicator_clear
#define editor_set_indent_type \
	p_editor->set_indent_type
#define scintilla_send_message \
	p_scintilla->send_message
#define scintilla_new \
	p_scintilla->new
#define dummyprefix_scintilla_send_message \
	p_dummyprefix->scintilla_send_message
#define sci_cmd \
	p_sci->cmd
#define sci_start_undo_action \
	p_sci->start_undo_action
#define sci_end_undo_action \
	p_sci->end_undo_action
#define sci_set_text \
	p_sci->set_text
#define sci_insert_text \
	p_sci->insert_text
#define sci_get_text \
	p_sci->get_text
#define sci_get_length \
	p_sci->get_length
#define sci_get_current_position \
	p_sci->get_current_position
#define sci_set_current_position \
	p_sci->set_current_position
#define sci_get_col_from_position \
	p_sci->get_col_from_position
#define sci_get_line_from_position \
	p_sci->get_line_from_position
#define sci_get_position_from_line \
	p_sci->get_position_from_line
#define sci_replace_sel \
	p_sci->replace_sel
#define sci_get_selected_text \
	p_sci->get_selected_text
#define sci_get_selected_text_length \
	p_sci->get_selected_text_length
#define sci_get_selection_start \
	p_sci->get_selection_start
#define sci_get_selection_end \
	p_sci->get_selection_end
#define sci_get_selection_mode \
	p_sci->get_selection_mode
#define sci_set_selection_mode \
	p_sci->set_selection_mode
#define sci_set_selection_start \
	p_sci->set_selection_start
#define sci_set_selection_end \
	p_sci->set_selection_end
#define sci_get_text_range \
	p_sci->get_text_range
#define sci_get_line \
	p_sci->get_line
#define sci_get_line_length \
	p_sci->get_line_length
#define sci_get_line_count \
	p_sci->get_line_count
#define sci_get_line_is_visible \
	p_sci->get_line_is_visible
#define sci_ensure_line_is_visible \
	p_sci->ensure_line_is_visible
#define sci_scroll_caret \
	p_sci->scroll_caret
#define sci_find_matching_brace \
	p_sci->find_matching_brace
#define sci_get_style_at \
	p_sci->get_style_at
#define sci_get_char_at \
	p_sci->get_char_at
#define sci_get_current_line \
	p_sci->get_current_line
#define sci_has_selection \
	p_sci->has_selection
#define sci_get_tab_width \
	p_sci->get_tab_width
#define sci_indicator_clear \
	p_sci->indicator_clear
#define sci_indicator_set \
	p_sci->indicator_set
#define templates_get_template_fileheader \
	p_templates->get_template_fileheader
#define utils_str_equal \
	p_utils->str_equal
#define utils_string_replace_all \
	p_utils->string_replace_all
#define utils_get_file_list \
	p_utils->get_file_list
#define utils_write_file \
	p_utils->write_file
#define utils_get_locale_from_utf \
	p_utils->get_locale_from_utf
#define utils_get_utf \
	p_utils->get_utf
#define utils_remove_ext_from_filename \
	p_utils->remove_ext_from_filename
#define utils_mkdir \
	p_utils->mkdir
#define utils_get_setting_boolean \
	p_utils->get_setting_boolean
#define utils_get_setting_integer \
	p_utils->get_setting_integer
#define utils_get_setting_string \
	p_utils->get_setting_string
#define utils_spawn_sync \
	p_utils->spawn_sync
#define utils_spawn_async \
	p_utils->spawn_async
#define utils_str_casecmp \
	p_utils->str_casecmp
#define utils_get_date_time \
	p_utils->get_date_time
#define ui_dialog_vbox_new \
	p_ui->dialog_vbox_new
#define ui_frame_new_with_alignment \
	p_ui->frame_new_with_alignment
#define ui_set_statusbar \
	p_ui->set_statusbar
#define ui_table_add_row \
	p_ui->table_add_row
#define ui_path_box_new \
	p_ui->path_box_new
#define ui_button_new_with_image \
	p_ui->button_new_with_image
#define ui_add_document_sensitive \
	p_ui->add_document_sensitive
#define ui_widget_set_tooltip_text \
	p_ui->widget_set_tooltip_text
#define ui_image_menu_item_new \
	p_ui->image_menu_item_new
#define ui_lookup_widget \
	p_ui->lookup_widget
#define dialogs_show_question \
	p_dialogs->show_question
#define dialogs_show_msgbox \
	p_dialogs->show_msgbox
#define dialogs_show_save_as \
	p_dialogs->show_save_as
#define lookup_widget \
	p_lookup->widget
#define msgwin_status_add \
	p_msgwin->status_add
#define msgwin_compiler_add_fmt \
	p_msgwin->compiler_add_fmt
#define msgwin_msg_add_fmt \
	p_msgwin->msg_add_fmt
#define msgwin_clear_tab \
	p_msgwin->clear_tab
#define msgwin_switch_tab \
	p_msgwin->switch_tab
#define encodings_convert_to_utf \
	p_encodings->convert_to_utf
#define encodings_convert_to_utf \
	p_encodings->convert_to_utf
#define encodings_get_charset_from_index \
	p_encodings->get_charset_from_index
#define keybindings_send_command \
	p_keybindings->send_command
#define keybindings_set_item \
	p_keybindings->set_item
#define tm_get_real_path \
	p_tm->get_real_path
#define tm_source_file_new \
	p_tm->source_file_new
#define tm_workspace_add_object \
	p_tm->workspace_add_object
#define tm_source_file_update \
	p_tm->source_file_update
#define tm_work_object_free \
	p_tm->work_object_free
#define tm_workspace_remove_object \
	p_tm->workspace_remove_object
#define search_show_find_in_files_dialog \
	p_search->show_find_in_files_dialog
#define highlighting_get_style \
	p_highlighting->get_style
#define filetypes_detect_from_file \
	p_filetypes->detect_from_file
#define filetypes_lookup_by_name \
	p_filetypes->lookup_by_name
#define filetypes_index \
	p_filetypes->index
#define navqueue_goto_line \
	p_navqueue->goto_line
#define main_reload_configuration \
	p_main->reload_configuration
#define main_locale_init \
	p_main->locale_init

#endif
