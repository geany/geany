/*
 * settings.c - this file is part of Geany, a fast and lightweight IDE
 *
 * Copyright 2016 Matthew Brush <matt@geany.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "settings.h"
#include "app.h"
#include "geany.h"
#include "sidebar.h"
#include "ui_utils.h"

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#define SETTINGS_SCHEMA_ID "org.geany.Settings"
#define SETTINGS_PATH "/"
#define SETTINGS_ROOT_GROUP "general"
#define SETTINGS_KEYFILE "settings.ini"

#define ui_lookup_main_widget(name) ui_lookup_widget(main_widgets.window, name)
#define ui_lookup_pref_widget(name) ui_lookup_widget(ui_widgets.prefs_dialog, name)

GSettings *geany_settings = NULL;


static void on_sidebar_pos_left_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	sidebar_set_position_left(g_settings_get_boolean(settings, key));
}


static void on_editor_font_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	g_free(interface_prefs.editor_font);
	interface_prefs.editor_font = g_settings_get_string(settings, key);
	ui_set_editor_font(interface_prefs.editor_font);
}


static void on_symbols_font_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	g_free(interface_prefs.tagbar_font);
	interface_prefs.tagbar_font = g_settings_get_string(settings, key);
	ui_set_symbols_font(interface_prefs.tagbar_font);
}


static void on_msgwin_font_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	g_free(interface_prefs.msgwin_font);
	interface_prefs.msgwin_font = g_settings_get_string(settings, key);
	ui_set_msgwin_font(interface_prefs.msgwin_font);
}


static void on_document_tabs_visible_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	interface_prefs.show_notebook_tabs = g_settings_get_boolean(settings, key);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(main_widgets.notebook), interface_prefs.show_notebook_tabs);
}


static void on_tab_pos_changed(GSettings *settings, gchar *key, gint *pref_field)
{
	*pref_field = g_settings_get_enum(settings, key);
}


static void on_symbols_tree_expanders_visible_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	gint indent = 0;
	interface_prefs.show_symbol_list_expanders = g_settings_get_boolean(settings, key);
	if (interface_prefs.show_symbol_list_expanders)
		indent = 10;
	gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(ui_lookup_main_widget("treeview2")), indent);
}


static void on_notebook_double_click_hides_widgets_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	interface_prefs.notebook_double_click_hides_widgets = g_settings_get_boolean(settings, key);
}


static void on_highlighting_inverted_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	interface_prefs.highlighting_invert_all = g_settings_get_boolean(settings, key);
	filetypes_reload();
}


static void on_message_window_tab_visible_changed(GSettings *settings, gchar *key, gboolean *pref_field)
{
	*pref_field = g_settings_get_boolean(settings, key);
}


static gboolean map_enum_to_int(GValue *value, GVariant *variant, gpointer user_data)
{
	GType enum_type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	enum_type = GPOINTER_TO_INT(user_data);
	enum_class = g_type_class_ref(enum_type);
	enum_value = g_enum_get_value_by_nick(enum_class, g_variant_get_string(variant, NULL));
	g_type_class_unref(enum_class);

	g_value_set_int(value, enum_value->value);

	return TRUE;
}


static GVariant *map_int_to_enum(const GValue *value, const GVariantType *expected_type, gpointer user_data)
{
	GType enum_type;
	GEnumClass *enum_class;
	GEnumValue *enum_value;

	enum_type = GPOINTER_TO_INT(user_data);
	enum_class = g_type_class_ref(enum_type);
	enum_value = g_enum_get_value(enum_class, g_value_get_int(value));
	g_type_class_unref(enum_class);

	return g_variant_new_string(enum_value->value_nick);
}


static void settings_bind_main(GSettings *settings)
{
	g_settings_bind(settings, "sidebar-page", ui_lookup_main_widget("notebook3"), "page", G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_GET_NO_CHANGES);
	g_settings_bind(settings, "sidebar-visible", ui_lookup_main_widget("notebook3"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-visible", ui_lookup_main_widget("menu_show_sidebar1"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-documents-visible", ui_lookup_main_widget("scrolledwindow7"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-symbols-visible", ui_lookup_main_widget("scrolledwindow2"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-visible", ui_lookup_main_widget("notebook_info"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-visible", ui_lookup_main_widget("menu_show_messages_window1"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-status-visible", ui_lookup_main_widget("scrolledwindow4"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-compiler-visible", ui_lookup_main_widget("scrolledwindow3"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-messages-visible", ui_lookup_main_widget("scrolledwindow5"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-scribble-visible", ui_lookup_main_widget("scrolledwindow6"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "statusbar-visible", ui_lookup_main_widget("statusbar"), "visible", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "document-tabs-visible", ui_lookup_main_widget("notebook1"), "show-tabs", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "editor-tab-pos", ui_lookup_main_widget("notebook1"), "tab-pos", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-tab-pos", ui_lookup_main_widget("notebook3"), "tab-pos", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-tab-pos", ui_lookup_main_widget("notebook_info"), "tab-pos", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "symbols-tree-expanders-visible", ui_lookup_main_widget("treeview2"), "show-expanders", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "fullscreen", ui_lookup_main_widget("menu_fullscreen1"), "active", G_SETTINGS_BIND_DEFAULT);

	interface_prefs.editor_font = g_settings_get_string(geany_settings, "editor-font");
	interface_prefs.tagbar_font = g_settings_get_string(geany_settings, "symbols-font");
	interface_prefs.msgwin_font = g_settings_get_string(geany_settings, "msgwin-font");
	interface_prefs.tab_pos_editor = g_settings_get_enum(settings, "editor-tab-pos");
	interface_prefs.tab_pos_sidebar = g_settings_get_enum(settings, "sidebar-tab-pos");
	interface_prefs.tab_pos_msgwin = g_settings_get_enum(settings, "msgwin-tab-pos");
	interface_prefs.show_symbol_list_expanders = g_settings_get_boolean(settings, "symbols-tree-expanders-visible");
	interface_prefs.notebook_double_click_hides_widgets = g_settings_get_boolean(settings, "notebook-double-click-hides-widgets");
	interface_prefs.highlighting_invert_all = g_settings_get_boolean(settings, "highlighting-inverted");
	interface_prefs.msgwin_status_visible = g_settings_get_boolean(settings, "msgwin-status-visible");
	interface_prefs.msgwin_compiler_visible = g_settings_get_boolean(settings, "msgwin-compiler-visible");
	interface_prefs.msgwin_messages_visible = g_settings_get_boolean(settings, "msgwin-messages-visible");
	interface_prefs.msgwin_scribble_visible = g_settings_get_boolean(settings, "msgwin-scribble-visible");

	g_signal_connect(settings, "changed::sidebar-pos-left", G_CALLBACK(on_sidebar_pos_left_changed), NULL);
	g_signal_connect(settings, "changed::editor-font", G_CALLBACK(on_editor_font_changed), NULL);
	g_signal_connect(settings, "changed::symbols-font", G_CALLBACK(on_symbols_font_changed), NULL);
	g_signal_connect(settings, "changed::msgwin-font", G_CALLBACK(on_msgwin_font_changed), NULL);
	g_signal_connect(settings, "changed::document-tabs-visible", G_CALLBACK(on_document_tabs_visible_changed), NULL);
	g_signal_connect(settings, "changed::editor-tab-pos", G_CALLBACK(on_tab_pos_changed), &interface_prefs.tab_pos_editor);
	g_signal_connect(settings, "changed::sidebar-tab-pos", G_CALLBACK(on_tab_pos_changed), &interface_prefs.tab_pos_sidebar);
	g_signal_connect(settings, "changed::msgwin-tab-pos", G_CALLBACK(on_tab_pos_changed), &interface_prefs.tab_pos_msgwin);
	g_signal_connect(settings, "changed::symbols-tree-expanders-visible", G_CALLBACK(on_symbols_tree_expanders_visible_changed), NULL);
	g_signal_connect(settings, "changed::notebook-double-click-hides-widgets", G_CALLBACK(on_notebook_double_click_hides_widgets_changed), NULL);
	g_signal_connect(settings, "changed::highlighting-inverted", G_CALLBACK(on_highlighting_inverted_changed), NULL);
	g_signal_connect(settings, "changed::msgwin-status-visible", G_CALLBACK(on_message_window_tab_visible_changed), &interface_prefs.msgwin_status_visible);
	g_signal_connect(settings, "changed::msgwin-compiler-visible", G_CALLBACK(on_message_window_tab_visible_changed), &interface_prefs.msgwin_compiler_visible);
	g_signal_connect(settings, "changed::msgwin-messages-visible", G_CALLBACK(on_message_window_tab_visible_changed), &interface_prefs.msgwin_messages_visible);
	g_signal_connect(settings, "changed::msgwin-scribble-visible", G_CALLBACK(on_message_window_tab_visible_changed), &interface_prefs.msgwin_scribble_visible);
}


static void settings_bind_prefs(GSettings *settings)
{
	g_settings_bind(settings, "sidebar-visible", ui_lookup_pref_widget("check_sidebar_visible"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-documents-visible", ui_lookup_pref_widget("check_list_symbol"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-symbols-visible", ui_lookup_pref_widget("check_list_openfiles"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "sidebar-pos-left", ui_lookup_pref_widget("radio_sidebar_left"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "statusbar-visible", ui_lookup_pref_widget("check_statusbar_visible"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "editor-font", ui_lookup_pref_widget("editor_font"), "font", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "symbols-font", ui_lookup_pref_widget("tagbar_font"), "font", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "msgwin-font", ui_lookup_pref_widget("msgwin_font"), "font", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "document-tabs-visible", ui_lookup_pref_widget("check_show_notebook_tabs"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind_with_mapping(settings, "editor-tab-pos", ui_lookup_pref_widget("combo_tab_editor"), "active", G_SETTINGS_BIND_DEFAULT, map_enum_to_int, map_int_to_enum, GINT_TO_POINTER(GTK_TYPE_POSITION_TYPE), NULL);
	g_settings_bind_with_mapping(settings, "sidebar-tab-pos", ui_lookup_pref_widget("combo_tab_sidebar"), "active", G_SETTINGS_BIND_DEFAULT, map_enum_to_int, map_int_to_enum, GINT_TO_POINTER(GTK_TYPE_POSITION_TYPE), NULL);
	g_settings_bind_with_mapping(settings, "msgwin-tab-pos", ui_lookup_pref_widget("combo_tab_msgwin"), "active", G_SETTINGS_BIND_DEFAULT, map_enum_to_int, map_int_to_enum, GINT_TO_POINTER(GTK_TYPE_POSITION_TYPE), NULL);
	g_settings_bind(settings, "notebook-double-click-hides-widgets", ui_lookup_pref_widget("check_double_click_hides_widgets"), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(settings, "highlighting-inverted", ui_lookup_pref_widget("check_highlighting_invert"), "active", G_SETTINGS_BIND_DEFAULT);
}


static GSettings *settings_create(void)
{
	gchar *config_fn;
	GSettingsBackend *backend;
	GSettingsSchemaSource *source;
	GSettingsSchema *schema;
	GSettings *settings;
	GError *error = NULL;

	source = g_settings_schema_source_new_from_directory(
		GEANY_SETTINGS_SCHEMA_DIR, NULL, TRUE, &error);
	if (source == NULL)
	{
		g_error("failed to add schema source '" GEANY_SETTINGS_SCHEMA_DIR
			"': %s", error->message);
		g_error_free(error);
		return NULL;
	}

	schema = g_settings_schema_source_lookup(source, SETTINGS_SCHEMA_ID, FALSE);
	g_settings_schema_source_unref(source);
	if (schema == NULL)
	{
		g_error("unable to locate schema in '" GEANY_SETTINGS_SCHEMA_DIR "'");
		return NULL;
	}

	config_fn = g_build_filename(app->configdir, SETTINGS_KEYFILE, NULL);
	backend = g_keyfile_settings_backend_new(config_fn, SETTINGS_PATH, SETTINGS_ROOT_GROUP);
	settings = g_settings_new_full(schema, backend, SETTINGS_PATH);

	g_object_unref(backend);
	g_free(config_fn);
	g_settings_schema_unref(schema);

	g_assert(G_IS_SETTINGS(settings));
	return settings;
}


GSettings *settings_create_for_prefs(void)
{
	GSettings *settings = settings_create();
	settings_bind_prefs(settings);
	return settings;
}


void settings_init(void)
{
	geany_settings = settings_create();
	settings_bind_main(geany_settings);
}


void settings_finalize(void)
{
	g_object_unref(geany_settings);
}
