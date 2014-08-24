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
