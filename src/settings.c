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


struct LegacyField
{
	GType type;
	gpointer field_ptr;
};


GSettings *geany_settings = NULL;


static void on_sidebar_pos_left_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	sidebar_set_position_left(g_settings_get_boolean(settings, key));
}


static void on_font_changed(GSettings *settings, gchar *key, void(*set_font_func)(const gchar*))
{
	gchar *font_name = g_settings_get_string(settings, key);
	set_font_func(font_name);
	g_free(font_name);
}


static void on_symbols_tree_expanders_visible_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	gint indent = 0;
	if (g_settings_get_boolean(settings, key))
		indent = 10;
	gtk_tree_view_set_level_indentation(GTK_TREE_VIEW(ui_lookup_main_widget("treeview2")), indent);
}


static void on_highlighting_inverted_changed(GSettings *settings, gchar *key, gpointer user_data)
{
	filetypes_reload();
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


G_GNUC_NULL_TERMINATED
static void settings_bind_many(GSettings *settings, GtkWidget *top, const gchar *name1, ...)
{
	const gchar *name;
	va_list ap;
	va_start(ap, name1);
	name = name1;
	while (name != NULL)
	{
		const gchar *widget_name = va_arg(ap, const gchar*);
		const gchar *prop_name = va_arg(ap, const gchar*);
		GtkWidget *widget = ui_lookup_widget(top, widget_name);
		g_settings_bind(settings, name, widget, prop_name, G_SETTINGS_BIND_DEFAULT);
		name = va_arg(ap, const gchar*);
	}
	va_end(ap);
}


static void settings_update_legacy_field(GSettings *settings, GType type, const gchar *key, gpointer field_ptr)
{
	if (type == G_TYPE_BOOLEAN)
	{
		*((gboolean*)field_ptr) = g_settings_get_boolean(settings, key);
	}
	else if (type == G_TYPE_INT)
	{
		*((gint*)field_ptr) = g_settings_get_int(settings, key);
	}
	else if (type == G_TYPE_STRING)
	{
		g_free(*((gchar**)field_ptr));
		*((gchar**)field_ptr) = g_settings_get_string(settings, key);
	}
	else if (type == GTK_TYPE_POSITION_TYPE)
	{
		*((gint*)field_ptr) = g_settings_get_enum(settings, key);
	}
	else
	{
		g_warning("unsupported legacy field binding type");
	}
}


static void on_legacy_field_changed(GSettings *settings, gchar *key, struct LegacyField *field)
{
	settings_update_legacy_field(settings, field->type, key, field->field_ptr);
}


static void settings_free_legacy_field(gpointer data, GClosure *closure)
{
	g_slice_free(struct LegacyField, data);
}


static void settings_bind_legacy_field(GSettings *settings, GType type, const gchar *key, gpointer field_ptr)
{
	gchar *signal_name;
	struct LegacyField *field;

	field = g_slice_new(struct LegacyField);
	field->type = type;
	field->field_ptr = field_ptr;

	signal_name = g_strdup_printf("changed::%s", key);
	g_signal_connect_data(settings, signal_name, G_CALLBACK(on_legacy_field_changed), field, settings_free_legacy_field, 0);
	g_free(signal_name);
}


G_GNUC_NULL_TERMINATED
static void settings_bind_many_legacy_fields(GSettings *settings, gboolean init, const gchar *name1, ...)
{
	va_list ap;
	const gchar *name = name1;
	va_start(ap, name1);
	while (name != NULL)
	{
		GType type = va_arg(ap, GType);
		gpointer field = va_arg(ap, gpointer);
		settings_bind_legacy_field(settings, type, name, field);
		if (init)
			settings_update_legacy_field(settings, type, name, field);
		name = va_arg(ap, const gchar*);
	}
	va_end(ap);
}


static void settings_bind_main(GSettings *settings)
{
	g_settings_bind(settings,
		"sidebar-page", ui_lookup_main_widget("notebook3"), "page",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_GET_NO_CHANGES);

	settings_bind_many(settings, main_widgets.window,
		"sidebar-visible",                "notebook3",                  "visible",        // bind to sidebar notebook's visible property
		"sidebar-visible",                "menu_show_sidebar1",         "active",         // bind to main menu item's active property
		"sidebar-documents-visible",      "scrolledwindow7",            "visible",        // bind to Documents tab visibility
		"sidebar-symbols-visible",        "scrolledwindow2",            "visible",        // bind to Symbols tab visibility
		"msgwin-visible",                 "notebook_info",              "visible",        // bind to msgwin notebook's visibility
		"msgwin-visible",                 "menu_show_messages_window1", "active",         // bind to main menu item's active property
		"msgwin-status-visible",          "scrolledwindow4",            "visible",        // bind to Status tab visibility
		"msgwin-compiler-visible",        "scrolledwindow3",            "visible",        // bind to Compiler tab visibility
		"msgwin-messages-visible",        "scrolledwindow5",            "visible",        // bind to Messages tab visibility
		"msgwin-scribble-visible",        "scrolledwindow6",            "visible",        // bind to Scribble tab visibility
		"statusbar-visible",              "statusbar",                  "visible",        // bind to statusbar's visibility
		"document-tabs-visible",          "notebook1",                  "show-tabs",      // bind to document notebook's show-tabs property
		"editor-tab-pos",                 "notebook1",                  "tab-pos",        // bind to document notebook's tab-pos property
		"sidebar-tab-pos",                "notebook3",                  "tab-pos",        // bind to sidebar notebook's tab-pos property
		"msgwin-tab-pos",                 "notebook_info",              "tab-pos",        // bind to message window notebook's tab-pos property
		"symbols-tree-expanders-visible", "treeview2",                  "show-expanders", // bind to Symbols tree view's show-expanders property
		"fullscreen",                     "menu_fullscreen1",           "active",         // bind to main menu item's active property
		NULL);

	settings_bind_many_legacy_fields(settings, TRUE,
		"editor-font",                         G_TYPE_STRING,          &interface_prefs.editor_font,
		"symbols-font",                        G_TYPE_STRING,          &interface_prefs.tagbar_font,
		"msgwin-font",                         G_TYPE_STRING,          &interface_prefs.msgwin_font,
		"editor-tab-pos",                      GTK_TYPE_POSITION_TYPE, &interface_prefs.tab_pos_editor,
		"sidebar-tab-pos",                     GTK_TYPE_POSITION_TYPE, &interface_prefs.tab_pos_sidebar,
		"msgwin-tab-pos",                      GTK_TYPE_POSITION_TYPE, &interface_prefs.tab_pos_msgwin,
		"symbols-tree-expanders-visible",      G_TYPE_BOOLEAN,         &interface_prefs.show_symbol_list_expanders,
		"notebook-double-click-hides-widgets", G_TYPE_BOOLEAN,         &interface_prefs.notebook_double_click_hides_widgets,
		"highlighting-inverted",               G_TYPE_BOOLEAN,         &interface_prefs.highlighting_invert_all,
		"msgwin-status-visible",               G_TYPE_BOOLEAN,         &interface_prefs.msgwin_status_visible,
		"msgwin-compiler-visible",             G_TYPE_BOOLEAN,         &interface_prefs.msgwin_compiler_visible,
		"msgwin-messages-visible",             G_TYPE_BOOLEAN,         &interface_prefs.msgwin_messages_visible,
		"msgwin-scribble-visible",             G_TYPE_BOOLEAN,         &interface_prefs.msgwin_scribble_visible,
		"use-native-windows-dialogs",          G_TYPE_BOOLEAN,         &interface_prefs.use_native_windows_dialogs,
		NULL);

	g_signal_connect(settings, "changed::sidebar-pos-left", G_CALLBACK(on_sidebar_pos_left_changed), NULL);
	g_signal_connect(settings, "changed::editor-font", G_CALLBACK(on_font_changed), ui_set_editor_font);
	g_signal_connect(settings, "changed::symbols-font", G_CALLBACK(on_font_changed), ui_set_symbols_font);
	g_signal_connect(settings, "changed::msgwin-font", G_CALLBACK(on_font_changed), ui_set_msgwin_font);
	g_signal_connect(settings, "changed::symbols-tree-expanders-visible", G_CALLBACK(on_symbols_tree_expanders_visible_changed), NULL);
	g_signal_connect(settings, "changed::highlighting-inverted", G_CALLBACK(on_highlighting_inverted_changed), NULL);
}


static void settings_bind_prefs(GSettings *settings)
{
	settings_bind_many(settings, ui_widgets.prefs_dialog,
		"document-tabs-visible",               "check_show_notebook_tabs",         "active",
		"sidebar-visible",                     "check_sidebar_visible",            "active",
		"sidebar-documents-visible",           "check_list_symbol",                "active",
		"sidebar-symbols-visible",             "check_list_openfiles",             "active",
		"sidebar-pos-left",                    "radio_sidebar_left",               "active",
		"statusbar-visible",                   "check_statusbar_visible",          "active",
		"editor-font",                         "editor_font",                      "font",
		"symbols-font",                        "tagbar_font",                      "font",
		"msgwin-font",                         "msgwin_font",                      "font",
		"notebook-double-click-hides-widgets", "check_double_click_hides_widgets", "active",
		"highlighting-inverted",               "check_highlighting_invert",        "active",
		"use-native-windows-dialogs",          "check_native_windows_dialogs",     "active",
		NULL);

	g_settings_bind_with_mapping(settings, "editor-tab-pos", ui_lookup_pref_widget("combo_tab_editor"), "active", G_SETTINGS_BIND_DEFAULT, map_enum_to_int, map_int_to_enum, GINT_TO_POINTER(GTK_TYPE_POSITION_TYPE), NULL);
	g_settings_bind_with_mapping(settings, "sidebar-tab-pos", ui_lookup_pref_widget("combo_tab_sidebar"), "active", G_SETTINGS_BIND_DEFAULT, map_enum_to_int, map_int_to_enum, GINT_TO_POINTER(GTK_TYPE_POSITION_TYPE), NULL);
	g_settings_bind_with_mapping(settings, "msgwin-tab-pos", ui_lookup_pref_widget("combo_tab_msgwin"), "active", G_SETTINGS_BIND_DEFAULT, map_enum_to_int, map_int_to_enum, GINT_TO_POINTER(GTK_TYPE_POSITION_TYPE), NULL);
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
