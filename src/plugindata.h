/*
 *      plugindata.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/**
 * @file plugindata.h
 * This file defines the plugin API, the interface between Geany and its plugins.
 * For detailed documentation of the plugin system please read the plugin
 * API documentation.
 **/
/* Note: Remember to increment GEANY_API_VERSION (and GEANY_ABI_VERSION if necessary)
 * when making changes (see 'Keeping the plugin ABI stable' in the HACKING file). */


#ifndef GEANY_PLUGINDATA_H
#define GEANY_PLUGINDATA_H

G_BEGIN_DECLS

/* Compatibility for sharing macros between API and core.
 * First include geany.h, then plugindata.h, then other API headers. */
#undef GEANY
#define GEANY(symbol_name) geany->symbol_name

#include "editor.h"	/* GeanyIndentType */
#include "build.h"  /* GeanyBuildGroup, GeanyBuildSource, GeanyBuildCmdEntries enums */


/** The Application Programming Interface (API) version, incremented
 * whenever any plugin data types are modified or appended to.
 *
 * You can protect code that needs a higher API than e.g. 200 with:
 * @code #if GEANY_API_VERSION >= 200
 * 	some_newer_function();
 * #endif @endcode
 *
 * @warning You should not test for values below 200 as previously
 * @c GEANY_API_VERSION was defined as an enum value, not a macro.
 */
#define GEANY_API_VERSION 215

/** The Application Binary Interface (ABI) version, incremented whenever
 * existing fields in the plugin data types have to be changed or reordered.
 * Changing this forces all plugins to be recompiled before Geany can load them. */
/* This should usually stay the same if fields are only appended, assuming only pointers to
 * structs and not structs themselves are declared by plugins. */
#define GEANY_ABI_VERSION 69


/** Defines a function to check the plugin is safe to load.
 * This performs runtime checks that try to ensure:
 * - Geany ABI data types are compatible with this plugin.
 * - Geany sources provide the required API for this plugin.
 * @param api_required The minimum API number your plugin requires.
 * Look at the source for the value of @c GEANY_API_VERSION to use if you
 * want your plugin to require the current Geany version on your machine.
 * You should update this value when using any new API features. */
#define PLUGIN_VERSION_CHECK(api_required) \
	gint plugin_version_check(gint abi_ver) \
	{ \
		if (abi_ver != GEANY_ABI_VERSION) \
			return -1; \
		return (api_required); \
	}


/** Basic information about a plugin available to Geany without loading the plugin.
 * The fields are set in plugin_set_info(), usually with the PLUGIN_SET_INFO() macro. */
typedef struct PluginInfo
{
	/** The name of the plugin. */
	const gchar	*name;
	/** The description of the plugin. */
	const gchar	*description;
	/** The version of the plugin. */
	const gchar	*version;
	/** The author of the plugin. */
	const gchar	*author;
}
PluginInfo;


/** Basic information for the plugin and identification.
 * @see geany_plugin. */
typedef struct GeanyPlugin
{
	PluginInfo	*info;	/**< Fields set in plugin_set_info(). */

	struct GeanyPluginPrivate *priv;	/* private */
}
GeanyPlugin;


/** Sets the plugin name and some other basic information about a plugin.
 *
 * @note If you want some of the arguments to be translated, see @ref PLUGIN_SET_TRANSLATABLE_INFO()
 *
 * Example:
 * @code PLUGIN_SET_INFO("Cool Feature", "Adds cool feature support.", "0.1", "Joe Author") @endcode */
/* plugin_set_info() could be written manually for plugins if we want to add any
 * extra PluginInfo features (such as an icon), so we don't need to break API
 * compatibility. Alternatively just add a new macro, PLUGIN_SET_INFO_FULL(). -ntrel */
#define PLUGIN_SET_INFO(p_name, p_description, p_version, p_author) \
	void plugin_set_info(PluginInfo *info) \
	{ \
		info->name = (p_name); \
		info->description = (p_description); \
		info->version = (p_version); \
		info->author = (p_author); \
	}

/** Sets the plugin name and some other basic information about a plugin.
 * This macro is like @ref PLUGIN_SET_INFO() but allows the passed information to be translated
 * by setting up the translation mechanism with @ref main_locale_init().
 * You therefore don't need to call it manually in plugin_init().
 *
 * Example:
 * @code PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE, _("Cool Feature"), _("Adds a cool feature."), "0.1", "John Doe") @endcode
 *
 * @since 0.19 */
#define PLUGIN_SET_TRANSLATABLE_INFO(localedir, package, p_name, p_description, p_version, p_author) \
	void plugin_set_info(PluginInfo *info) \
	{ \
		main_locale_init((localedir), (package)); \
		info->name = (p_name); \
		info->description = (p_description); \
		info->version = (p_version); \
		info->author = (p_author); \
	}


#ifndef GEANY_PRIVATE

/* Prototypes for building plugins with -Wmissing-prototypes */
gint plugin_version_check(gint abi_ver);
void plugin_set_info(PluginInfo *info);

#endif


/** @deprecated - use plugin_set_key_group() instead.
 * @see PLUGIN_KEY_GROUP() macro. */
typedef struct GeanyKeyGroupInfo
{
	const gchar *name;		/**< Group name used in the configuration file, such as @c "html_chars" */
	gsize count;			/**< The number of keybindings the group will hold */
}
GeanyKeyGroupInfo;

/** @deprecated - use plugin_set_key_group() instead.
 * Declare and initialise a keybinding group.
 * @code GeanyKeyGroup *plugin_key_group; @endcode
 * You must then set the @c plugin_key_group::keys[] entries for the group in plugin_init(),
 * normally using keybindings_set_item().
 * The @c plugin_key_group::label field is set by Geany after @c plugin_init()
 * is called, to the name of the plugin.
 * @param group_name A unique group name (without quotes) to be used in the
 * configuration file, such as @c html_chars.
 * @param key_count	The number of keybindings the group will hold. */
#define PLUGIN_KEY_GROUP(group_name, key_count) \
	/* We have to declare this as a single element array.
	 * Declaring as a pointer to a struct doesn't work with g_module_symbol(). */ \
	GeanyKeyGroupInfo plugin_key_group_info[1] = \
	{ \
		{G_STRINGIFY(group_name), key_count} \
	};\
	GeanyKeyGroup *plugin_key_group = NULL;


/** Callback array entry type used with the @ref plugin_callbacks symbol. */
typedef struct PluginCallback
{
	/** The name of signal, must be an existing signal. For a list of available signals,
	 *  please see the @link pluginsignals.c Signal documentation @endlink. */
	const gchar	*signal_name;
	/** A callback function which is called when the signal is emitted. */
	GCallback	callback;
	/** Set to TRUE to connect your handler with g_signal_connect_after(). */
	gboolean	after;
	/** The user data passed to the signal handler. */
	gpointer	user_data;
}
PluginCallback;


/** @deprecated Use @ref ui_add_document_sensitive() instead.
 * Flags to be set by plugins in PluginFields struct. */
typedef enum
{
	/** Whether a plugin's menu item should be disabled when there are no open documents */
	PLUGIN_IS_DOCUMENT_SENSITIVE	= 1 << 0
}
PluginFlags;

/** @deprecated Use @ref ui_add_document_sensitive() instead.
 * Fields set and owned by the plugin. */
typedef struct PluginFields
{
	/** Bitmask of @c PluginFlags. */
	PluginFlags	flags;
	/** Pointer to a plugin's menu item which will be automatically enabled/disabled when there
	 *  are no open documents and @c PLUGIN_IS_DOCUMENT_SENSITIVE is set.
	 *  This is required if using @c PLUGIN_IS_DOCUMENT_SENSITIVE, ignored otherwise */
	GtkWidget	*menu_item;
}
PluginFields;


/** This contains pointers to global variables owned by Geany for plugins to use.
 * Core variable pointers can be appended when needed by plugin authors, if appropriate. */
typedef struct GeanyData
{
	struct GeanyApp				*app;				/**< Geany application data fields */
	struct GeanyMainWidgets		*main_widgets;		/**< Important widgets in the main window */
	GPtrArray					*documents_array;	/**< See document.h#documents_array. */
	GPtrArray					*filetypes_array;	/**< Dynamic array of GeanyFiletype pointers */
	struct GeanyPrefs			*prefs;				/**< General settings */
	struct GeanyInterfacePrefs	*interface_prefs;	/**< Interface settings */
	struct GeanyToolbarPrefs	*toolbar_prefs;		/**< Toolbar settings */
	struct GeanyEditorPrefs		*editor_prefs;		/**< Editor settings */
	struct GeanyFilePrefs		*file_prefs;		/**< File-related settings */
	struct GeanySearchPrefs		*search_prefs;		/**< Search-related settings */
	struct GeanyToolPrefs		*tool_prefs;		/**< Tool settings */
	struct GeanyTemplatePrefs	*template_prefs;	/**< Template settings */
	struct GeanyBuildInfo		*build_info;		/**< Current build information */
	GSList						*filetypes_by_title; /**< See filetypes.h#filetypes_by_title. */
}
GeanyData;

#define geany			geany_data	/**< Simple macro for @c geany_data that reduces typing. */


/** This contains pointers to functions owned by Geany for plugins to use.
 * Functions from the core can be appended when needed by plugin authors, but may
 * require some changes. */
typedef struct GeanyFunctions
{
	struct DocumentFuncs		*p_document;		/**< See document.h */
	struct SciFuncs				*p_sci;				/**< See sciwrappers.h */
	struct TemplateFuncs		*p_templates;		/**< See templates.h */
	struct UtilsFuncs			*p_utils;			/**< See utils.h */
	struct UIUtilsFuncs			*p_ui;				/**< See ui_utils.h */
	/** @deprecated Use ui_lookup_widget() instead. */
	struct SupportFuncs			*p_support;
	struct DialogFuncs			*p_dialogs;			/**< See dialogs.h */
	/** @deprecated Use @ref GeanyFunctions::p_msgwin instead. */
	struct MsgWinFuncs			*p_msgwindow;
	struct EncodingFuncs		*p_encodings;		/**< See encodings.h */
	struct KeybindingFuncs		*p_keybindings;		/**< See keybindings.h */
	struct TagManagerFuncs		*p_tm;				/**< See tagmanager/include */
	struct SearchFuncs			*p_search;			/**< See search.h */
	struct HighlightingFuncs	*p_highlighting;	/**< See highlighting.h */
	struct FiletypeFuncs		*p_filetypes;		/**< See filetypes.h */
	struct NavQueueFuncs		*p_navqueue;		/**< See navqueue.h */
	struct EditorFuncs			*p_editor;			/**< See editor.h */
	struct MainFuncs			*p_main;			/**< See main.h */
	struct PluginFuncs			*p_plugin;			/**< See pluginutils.c */
	struct ScintillaFuncs		*p_scintilla;		/**< See ScintillaFuncs */
	struct MsgWinFuncs			*p_msgwin;			/**< See msgwindow.h */
	struct StashFuncs			*p_stash;			/**< See stash.h */
	struct SymbolsFuncs			*p_symbols;			/**< See symbols.h */
	struct BuildFuncs			*p_build;			/**< See build.h */
}
GeanyFunctions;


/* For more information about these functions, see the main source code.
 * E.g. for p_document->new_file(), see document_new_file() in document.c. */


/* See document.h */
typedef struct DocumentFuncs
{
	struct GeanyDocument*	(*document_new_file) (const gchar *utf8_filename, struct GeanyFiletype *ft,
			const gchar *text);
	struct GeanyDocument*	(*document_get_current) (void);
	struct GeanyDocument*	(*document_get_from_page) (guint page_num);
	struct GeanyDocument*	(*document_find_by_filename) (const gchar *utf8_filename);
	struct GeanyDocument*	(*document_find_by_real_path) (const gchar *realname);
	gboolean				(*document_save_file) (struct GeanyDocument *doc, gboolean force);
	struct GeanyDocument*	(*document_open_file) (const gchar *locale_filename, gboolean readonly,
			struct GeanyFiletype *ft, const gchar *forced_enc);
	void		(*document_open_files) (const GSList *filenames, gboolean readonly,
			struct GeanyFiletype *ft, const gchar *forced_enc);
	gboolean	(*document_remove_page) (guint page_num);
	gboolean	(*document_reload_file) (struct GeanyDocument *doc, const gchar *forced_enc);
	void		(*document_set_encoding) (struct GeanyDocument *doc, const gchar *new_encoding);
	void		(*document_set_text_changed) (struct GeanyDocument *doc, gboolean changed);
	void		(*document_set_filetype) (struct GeanyDocument *doc, struct GeanyFiletype *type);
	gboolean	(*document_close) (struct GeanyDocument *doc);
	struct GeanyDocument*	(*document_index)(gint idx);
	gboolean	(*document_save_file_as) (struct GeanyDocument *doc, const gchar *utf8_fname);
	void		(*document_rename_file) (struct GeanyDocument *doc, const gchar *new_filename);
	const GdkColor*	(*document_get_status_color) (struct GeanyDocument *doc);
	gchar*		(*document_get_basename_for_display) (struct GeanyDocument *doc, gint length);
	gint		(*document_get_notebook_page) (struct GeanyDocument *doc);
	gint		(*document_compare_by_display_name) (gconstpointer a, gconstpointer b);
	gint		(*document_compare_by_tab_order) (gconstpointer a, gconstpointer b);
	gint		(*document_compare_by_tab_order_reverse) (gconstpointer a, gconstpointer b);
}
DocumentFuncs;


struct _ScintillaObject;

/** See http://scintilla.org for the full documentation. */
typedef struct ScintillaFuncs
{
	/** Send Scintilla a message. */
	long int	(*scintilla_send_message) (struct _ScintillaObject *sci, unsigned int iMessage,
			long unsigned int wParam, long int lParam);
	/** Create a new ScintillaObject widget. */
	GtkWidget*	(*scintilla_new)(void);
}
ScintillaFuncs;


/** Wrapper functions for Scintilla messages.
 * See sciwrappers.h for the list of functions. */
typedef struct SciFuncs
{
	/** @deprecated Use @c scintilla_send_message() instead. */
	long int (*sci_send_message) (struct _ScintillaObject *sci, unsigned int iMessage,
			long unsigned int wParam, long int lParam);
	void	(*sci_send_command) (struct _ScintillaObject *sci, gint cmd);

	void	(*sci_start_undo_action) (struct _ScintillaObject *sci);
	void	(*sci_end_undo_action) (struct _ScintillaObject *sci);
	void	(*sci_set_text) (struct _ScintillaObject *sci, const gchar *text);
	void	(*sci_insert_text) (struct _ScintillaObject *sci, gint pos, const gchar *text);
	void	(*sci_get_text) (struct _ScintillaObject *sci, gint len, gchar *text);
	gint	(*sci_get_length) (struct _ScintillaObject *sci);
	gint	(*sci_get_current_position) (struct _ScintillaObject *sci);
	void	(*sci_set_current_position) (struct _ScintillaObject *sci, gint position,
			 gboolean scroll_to_caret);
	gint	(*sci_get_col_from_position) (struct _ScintillaObject *sci, gint position);
	gint	(*sci_get_line_from_position) (struct _ScintillaObject *sci, gint position);
	gint	(*sci_get_position_from_line) (struct _ScintillaObject *sci, gint line);
	void	(*sci_replace_sel) (struct _ScintillaObject *sci, const gchar *text);
	void	(*sci_get_selected_text) (struct _ScintillaObject *sci, gchar *text);
	gint	(*sci_get_selected_text_length) (struct _ScintillaObject *sci);
	gint	(*sci_get_selection_start) (struct _ScintillaObject *sci);
	gint	(*sci_get_selection_end) (struct _ScintillaObject *sci);
	gint	(*sci_get_selection_mode) (struct _ScintillaObject *sci);
	void	(*sci_set_selection_mode) (struct _ScintillaObject *sci, gint mode);
	void	(*sci_set_selection_start) (struct _ScintillaObject *sci, gint position);
	void	(*sci_set_selection_end) (struct _ScintillaObject *sci, gint position);
	void	(*sci_get_text_range) (struct _ScintillaObject *sci, gint start, gint end, gchar *text);
	gchar*	(*sci_get_line) (struct _ScintillaObject *sci, gint line_num);
	gint	(*sci_get_line_length) (struct _ScintillaObject *sci, gint line);
	gint	(*sci_get_line_count) (struct _ScintillaObject *sci);
	gboolean (*sci_get_line_is_visible) (struct _ScintillaObject *sci, gint line);
	void	(*sci_ensure_line_is_visible) (struct _ScintillaObject *sci, gint line);
	void	(*sci_scroll_caret) (struct _ScintillaObject *sci);
	gint	(*sci_find_matching_brace) (struct _ScintillaObject *sci, gint pos);
	gint	(*sci_get_style_at) (struct _ScintillaObject *sci, gint position);
	gchar	(*sci_get_char_at) (struct _ScintillaObject *sci, gint pos);
	gint	(*sci_get_current_line) (struct _ScintillaObject *sci);
	gboolean (*sci_has_selection) (struct _ScintillaObject *sci);
	gint	(*sci_get_tab_width) (struct _ScintillaObject *sci);
	void	(*sci_indicator_clear) (struct _ScintillaObject *sci, gint start, gint end);
	void	(*sci_indicator_set) (struct _ScintillaObject *sci, gint indic);
	gchar*	(*sci_get_contents) (struct _ScintillaObject *sci, gint len);
	gchar*	(*sci_get_contents_range) (struct _ScintillaObject *sci, gint start, gint end);
	gchar*	(*sci_get_selection_contents) (struct _ScintillaObject *sci);
	void	(*sci_set_font) (struct _ScintillaObject *sci, gint style, const gchar *font, gint size);
	gint	(*sci_get_line_end_position) (struct _ScintillaObject *sci, gint line);
	void	(*sci_set_target_start) (struct _ScintillaObject *sci, gint start);
	void	(*sci_set_target_end) (struct _ScintillaObject *sci, gint end);
	gint	(*sci_replace_target) (struct _ScintillaObject *sci, const gchar *text, gboolean regex);
	void	(*sci_set_marker_at_line) (struct _ScintillaObject *sci, gint line_number, gint marker);
	void	(*sci_delete_marker_at_line) (struct _ScintillaObject *sci, gint line_number, gint marker);
	gboolean (*sci_is_marker_set_at_line) (struct _ScintillaObject *sci, gint line, gint marker);
	void 	(*sci_goto_line) (struct _ScintillaObject *sci, gint line, gboolean unfold);
	gint	(*sci_find_text) (struct _ScintillaObject *sci, gint flags, struct Sci_TextToFind *ttf);
	void	(*sci_set_line_indentation) (struct _ScintillaObject *sci, gint line, gint indent);
	gint	(*sci_get_line_indentation) (struct _ScintillaObject *sci, gint line);
	gint	(*sci_get_lexer) (struct _ScintillaObject *sci);
}
SciFuncs;


/* See templates.h */
typedef struct TemplateFuncs
{
	gchar*		(*templates_get_template_fileheader) (gint filetype_idx, const gchar *fname);
}
TemplateFuncs;


/* See utils.h */
typedef struct UtilsFuncs
{
	gboolean	(*utils_str_equal) (const gchar *a, const gchar *b);
	guint		(*utils_string_replace_all) (GString *haystack, const gchar *needle,
				 const gchar *replacement);
	GSList*		(*utils_get_file_list) (const gchar *path, guint *length, GError **error);
	gint		(*utils_write_file) (const gchar *filename, const gchar *text);
	gchar*		(*utils_get_locale_from_utf8) (const gchar *utf8_text);
	gchar*		(*utils_get_utf8_from_locale) (const gchar *locale_text);
	gchar*		(*utils_remove_ext_from_filename) (const gchar *filename);
	gint		(*utils_mkdir) (const gchar *path, gboolean create_parent_dirs);
	gboolean	(*utils_get_setting_boolean) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gboolean default_value);
	gint		(*utils_get_setting_integer) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gint default_value);
	gchar*		(*utils_get_setting_string) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gchar *default_value);
	gboolean	(*utils_spawn_sync) (const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
				 GSpawnChildSetupFunc child_setup, gpointer user_data, gchar **std_out,
				 gchar **std_err, gint *exit_status, GError **error);
	gboolean	(*utils_spawn_async) (const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
				 GSpawnChildSetupFunc child_setup, gpointer user_data, GPid *child_pid,
				 GError **error);
	gint		(*utils_str_casecmp) (const gchar *s1, const gchar *s2);
	gchar*		(*utils_get_date_time) (const gchar *format, time_t *time_to_use);
	void		(*utils_open_browser) (const gchar *uri);
	guint		(*utils_string_replace_first) (GString *haystack, const gchar *needle,
				 const gchar *replace);
	gchar*		(*utils_str_middle_truncate) (const gchar *string, guint truncate_length);
	gchar*		(*utils_str_remove_chars) (gchar *string, const gchar *chars);
	GSList*		(*utils_get_file_list_full)(const gchar *path, gboolean full_path, gboolean sort,
				GError **error);
	gchar**		(*utils_copy_environment)(const gchar **exclude_vars, const gchar *first_varname, ...);
	gchar*		(*utils_find_open_xml_tag) (const gchar sel[], gint size);
	const gchar*	(*utils_find_open_xml_tag_pos) (const gchar sel[], gint size);
}
UtilsFuncs;


/* See main.h */
typedef struct MainFuncs
{
	void		(*main_reload_configuration) (void);
	void		(*main_locale_init) (const gchar *locale_dir, const gchar *package);
	gboolean	(*main_is_realized) (void);
}
MainFuncs;


/* See ui_utils.h */
typedef struct UIUtilsFuncs
{
	GtkWidget*	(*ui_dialog_vbox_new) (GtkDialog *dialog);
	GtkWidget*	(*ui_frame_new_with_alignment) (const gchar *label_text, GtkWidget **alignment);
	void		(*ui_set_statusbar) (gboolean log, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
	void		(*ui_table_add_row) (GtkTable *table, gint row, ...) G_GNUC_NULL_TERMINATED;
	GtkWidget*	(*ui_path_box_new) (const gchar *title, GtkFileChooserAction action, GtkEntry *entry);
	GtkWidget*	(*ui_button_new_with_image) (const gchar *stock_id, const gchar *text);
	void		(*ui_add_document_sensitive) (GtkWidget *widget);
	void		(*ui_widget_set_tooltip_text) (GtkWidget *widget, const gchar *text);
	GtkWidget*	(*ui_image_menu_item_new) (const gchar *stock_id, const gchar *label);
	GtkWidget*	(*ui_lookup_widget) (GtkWidget *widget, const gchar *widget_name);
	void		(*ui_progress_bar_start) (const gchar *text);
	void		(*ui_progress_bar_stop) (void);
	void		(*ui_entry_add_clear_icon) (GtkEntry *entry);
	void		(*ui_menu_add_document_items) (GtkMenu *menu, struct GeanyDocument *active,
				GCallback callback);
	void		(*ui_widget_modify_font_from_string) (GtkWidget *widget, const gchar *str);
	gboolean	(*ui_is_keyval_enter_or_return) (guint keyval);
	gint		(*ui_get_gtk_settings_integer) (const gchar *property_name, gint default_value);
	void		(*ui_combo_box_add_to_history) (GtkComboBoxEntry *combo_entry,
				const gchar *text, gint history_len);
	void		(*ui_menu_add_document_items_sorted) (GtkMenu *menu, struct GeanyDocument *active,
				GCallback callback, GCompareFunc compare_func);
	const gchar* (*ui_lookup_stock_label)(const gchar *stock_id);
}
UIUtilsFuncs;


/* See dialogs.h */
typedef struct DialogFuncs
{
	gboolean	(*dialogs_show_question) (const gchar *text, ...) G_GNUC_PRINTF (1, 2);
	void		(*dialogs_show_msgbox) (GtkMessageType type, const gchar *text, ...) G_GNUC_PRINTF (2, 3);
	gboolean	(*dialogs_show_save_as) (void);
	gboolean	(*dialogs_show_input_numeric) (const gchar *title, const gchar *label_text,
				 gdouble *value, gdouble min, gdouble max, gdouble step);
	gchar*		(*dialogs_show_input)(const gchar *title, GtkWindow *parent, const gchar *label_text,
				const gchar *default_text);
}
DialogFuncs;


/* @deprecated Use ui_lookup_widget() instead. */
typedef struct SupportFuncs
{
	GtkWidget*	(*support_lookup_widget) (GtkWidget *widget, const gchar *widget_name);
}
SupportFuncs;


/* See msgwindow.h */
typedef struct MsgWinFuncs
{
	/* status_add() does not set the status bar - use ui->set_statusbar() instead. */
	void		(*msgwin_status_add) (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
	void		(*msgwin_compiler_add) (gint msg_color, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
	void		(*msgwin_msg_add) (gint msg_color, gint line, struct GeanyDocument *doc,
				 const gchar *format, ...) G_GNUC_PRINTF (4, 5);
	void		(*msgwin_clear_tab) (gint tabnum);
	void		(*msgwin_switch_tab) (gint tabnum, gboolean show);
	void		(*msgwin_set_messages_dir) (const gchar *messages_dir);
}
MsgWinFuncs;


/* See encodings.h */
typedef struct EncodingFuncs
{
	gchar*			(*encodings_convert_to_utf8) (const gchar *buffer, gssize size, gchar **used_encoding);
	gchar* 			(*encodings_convert_to_utf8_from_charset) (const gchar *buffer, gssize size,
													 const gchar *charset, gboolean fast);
	const gchar*	(*encodings_get_charset_from_index) (gint idx);
}
EncodingFuncs;


struct GeanyKeyGroup;
/* avoid including keybindings.h */
typedef void (*_GeanyKeyCallback) (guint key_id);

/* See keybindings.h */
typedef struct KeybindingFuncs
{
	void		(*keybindings_send_command) (guint group_id, guint key_id);
	struct GeanyKeyBinding* (*keybindings_set_item) (struct GeanyKeyGroup *group, gsize key_id,
					_GeanyKeyCallback callback, guint key, GdkModifierType mod,
					const gchar *name, const gchar *label, GtkWidget *menu_item);
	struct GeanyKeyBinding* (*keybindings_get_item)(struct GeanyKeyGroup *group, gsize key_id);

}
KeybindingFuncs;


/* See highlighting.h */
typedef struct HighlightingFuncs
{
	const struct GeanyLexerStyle* (*highlighting_get_style) (gint ft_id, gint style_id);
	void		(*highlighting_set_styles) (struct _ScintillaObject *sci, struct GeanyFiletype *ft);
	gboolean	(*highlighting_is_string_style) (gint lexer, gint style);
	gboolean	(*highlighting_is_comment_style) (gint lexer, gint style);
	gboolean	(*highlighting_is_code_style) (gint lexer, gint style);
}
HighlightingFuncs;


/* See filetypes.h */
typedef struct FiletypeFuncs
{
	GeanyFiletype*	(*filetypes_detect_from_file) (const gchar *utf8_filename);
	GeanyFiletype*	(*filetypes_lookup_by_name) (const gchar *name);
	GeanyFiletype*	(*filetypes_index)(gint idx);
	const gchar*	(*filetypes_get_display_name)(GeanyFiletype *ft);
	const GSList*	(*filetypes_get_sorted_by_name)(void);
	/* Remember to convert any filetype_id arguments to GeanyFiletype pointers in any
	 * appended functions */
}
FiletypeFuncs;


/* See search.h */
typedef struct SearchFuncs
{
	void		(*search_show_find_in_files_dialog) (const gchar *dir);
}
SearchFuncs;


/* See tagmanager/include */
typedef struct TagManagerFuncs
{
	gchar*			(*tm_get_real_path) (const gchar *file_name);
	TMWorkObject*	(*tm_source_file_new) (const char *file_name, gboolean update, const char *name);
	gboolean		(*tm_workspace_add_object) (TMWorkObject *work_object);
	gboolean		(*tm_source_file_update) (TMWorkObject *source_file, gboolean force,
					 gboolean recurse, gboolean update_parent);
	void			(*tm_work_object_free) (gpointer work_object);
	gboolean		(*tm_workspace_remove_object) (TMWorkObject *w, gboolean do_free, gboolean update);
}
TagManagerFuncs;


/* See navqueue.h */
typedef struct NavQueueFuncs
{
	gboolean		(*navqueue_goto_line) (struct GeanyDocument *old_doc, struct GeanyDocument *new_doc,
					 gint line);
}
NavQueueFuncs;


struct GeanyEditor;

/* See editor.h */
typedef struct EditorFuncs
{
	const struct GeanyIndentPrefs* (*editor_get_indent_prefs)(struct GeanyEditor *editor);
	struct _ScintillaObject* (*editor_create_widget)(struct GeanyEditor *editor);

	void	(*editor_indicator_set_on_range) (struct GeanyEditor *editor, gint indic, gint start, gint end);
	void	(*editor_indicator_set_on_line) (struct GeanyEditor *editor, gint indic, gint line);
	void	(*editor_indicator_clear) (struct GeanyEditor *editor, gint indic);

	void	(*editor_set_indent_type)(struct GeanyEditor *editor, GeanyIndentType type);
	gchar*	(*editor_get_word_at_pos) (struct GeanyEditor *editor, gint pos, const gchar *wordchars);

	const gchar*	(*editor_get_eol_char_name) (struct GeanyEditor *editor);
	gint			(*editor_get_eol_char_len) (struct GeanyEditor *editor);
	const gchar*	(*editor_get_eol_char) (struct GeanyEditor *editor);

	void	(*editor_insert_text_block) (struct GeanyEditor *editor, const gchar *text,
			 gint insert_pos, gint cursor_index, gint newline_indent_size,
			 gboolean replace_newlines);

	gint	(*editor_get_eol_char_mode) (struct GeanyEditor *editor);
	gboolean (*editor_goto_pos) (struct GeanyEditor *editor, gint pos, gboolean mark);

	const gchar* (*editor_find_snippet) (struct GeanyEditor *editor, const gchar *snippet_name);
	void	(*editor_insert_snippet) (struct GeanyEditor *editor, gint pos, const gchar *snippet);
}
EditorFuncs;


/* avoid including keybindings.h */
typedef gboolean (*_GeanyKeyGroupCallback) (guint key_id);

/* See pluginutils.c */
typedef struct PluginFuncs
{
	void	(*plugin_add_toolbar_item)(GeanyPlugin *plugin, GtkToolItem *item);
	void	(*plugin_module_make_resident) (GeanyPlugin *plugin);
	void	(*plugin_signal_connect) (GeanyPlugin *plugin,
		GObject *object, const gchar *signal_name, gboolean after,
		GCallback callback, gpointer user_data);
	struct GeanyKeyGroup* (*plugin_set_key_group)(GeanyPlugin *plugin,
		const gchar *section_name, gsize count, _GeanyKeyGroupCallback callback);
	void	(*plugin_show_configure)(GeanyPlugin *plugin);
	guint	(*plugin_timeout_add) (GeanyPlugin *plugin, guint interval, GSourceFunc function,
		gpointer data);
	guint	(*plugin_timeout_add_seconds) (GeanyPlugin *plugin, guint interval,
		GSourceFunc function, gpointer data);
	guint	(*plugin_idle_add) (GeanyPlugin *plugin, GSourceFunc function, gpointer data);
}
PluginFuncs;


struct StashGroup;

/* See stash.h */
typedef struct StashFuncs
{
	struct StashGroup *(*stash_group_new)(const gchar *name);
	void (*stash_group_add_boolean)(struct StashGroup *group, gboolean *setting,
			const gchar *key_name, gboolean default_value);
	void (*stash_group_add_integer)(struct StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value);
	void (*stash_group_add_string)(struct StashGroup *group, gchar **setting,
			const gchar *key_name, const gchar *default_value);
	void (*stash_group_add_string_vector)(struct StashGroup *group, gchar ***setting,
			const gchar *key_name, const gchar **default_value);
	void (*stash_group_load_from_key_file)(struct StashGroup *group, GKeyFile *keyfile);
	void (*stash_group_save_to_key_file)(struct StashGroup *group, GKeyFile *keyfile);
	void (*stash_group_free)(struct StashGroup *group);
	gboolean (*stash_group_load_from_file)(struct StashGroup *group, const gchar *filename);
	gint (*stash_group_save_to_file)(struct StashGroup *group, const gchar *filename,
			GKeyFileFlags flags);
	void (*stash_group_add_toggle_button)(struct StashGroup *group, gboolean *setting,
			const gchar *key_name, gboolean default_value, gconstpointer widget_id);
	void (*stash_group_add_radio_buttons)(struct StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value,
			gconstpointer widget_id, gint enum_id, ...) G_GNUC_NULL_TERMINATED;
	void (*stash_group_add_spin_button_integer)(struct StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value, gconstpointer widget_id);
	void (*stash_group_add_combo_box)(struct StashGroup *group, gint *setting,
			const gchar *key_name, gint default_value, gconstpointer widget_id);
	void (*stash_group_add_combo_box_entry)(struct StashGroup *group, gchar **setting,
			const gchar *key_name, const gchar *default_value, gconstpointer widget_id);
	void (*stash_group_add_entry)(struct StashGroup *group, gchar **setting,
			const gchar *key_name, const gchar *default_value, gconstpointer widget_id);
	void (*stash_group_add_widget_property)(struct StashGroup *group, gpointer setting,
			const gchar *key_name, gpointer default_value, gconstpointer widget_id,
			const gchar *property_name, GType type);
	void (*stash_group_display)(struct StashGroup *group, GtkWidget *owner);
	void (*stash_group_update)(struct StashGroup *group, GtkWidget *owner);
	void (*stash_group_free_settings)(struct StashGroup *group);
}
StashFuncs;


/* See symbols.h */
typedef struct SymbolsFuncs
{
	const gchar*	(*symbols_get_context_separator)(gint ft_id);
}
SymbolsFuncs;

/* See build.h */
typedef struct BuildFuncs
{
	void (*build_activate_menu_item)(const GeanyBuildGroup grp, const guint cmd);
	const gchar *(*build_get_current_menu_item)(const GeanyBuildGroup grp, const guint cmd,
			const GeanyBuildCmdEntries field);
	void (*build_remove_menu_item)(const GeanyBuildSource src, const GeanyBuildGroup grp,
			const gint cmd);
	void (*build_set_menu_item)(const GeanyBuildSource src, const GeanyBuildGroup grp,
			const guint cmd, const GeanyBuildCmdEntries field, const gchar *value);
	guint (*build_get_group_count)(const GeanyBuildGroup grp);
}
BuildFuncs;

/* Deprecated aliases */
#ifndef GEANY_DISABLE_DEPRECATED

/** @deprecated - copy into your plugin code if needed.
 * @c NULL-safe way to get the index of @a doc_ptr in the documents array. */
#define DOC_IDX(doc_ptr) \
	(doc_ptr ? doc_ptr->index : -1)
#define DOC_IDX_VALID(doc_idx) \
	((doc_idx) >= 0 && (guint)(doc_idx) < documents_array->len && documents[doc_idx]->is_valid)

#define GEANY_WINDOW_MINIMAL_WIDTH		550
#define GEANY_WINDOW_MINIMAL_HEIGHT		GEANY_DEFAULT_DIALOG_HEIGHT

#endif	/* GEANY_DISABLE_DEPRECATED */

G_END_DECLS

#endif
