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
