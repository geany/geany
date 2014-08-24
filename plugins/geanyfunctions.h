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
#define plugin_builder_connect_signals \
	geany_functions->p_plugin->plugin_builder_connect_signals
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
