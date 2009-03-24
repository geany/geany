/*
 *      plugindata.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2009 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *
 * $Id$
 */

/**
 * @file plugindata.h
 * This file defines the plugin API, the interface between Geany and its plugins.
 * For detailed documentation of the plugin system please read the plugin
 * API documentation.
 **/
/* Note: Remember to increment GEANY_API_VERSION (and GEANY_ABI_VERSION if necessary)
 * when making changes (see 'Keeping the plugin ABI stable' in the HACKING file). */


#ifndef PLUGINDATA_H
#define PLUGINDATA_H

#include "editor.h"	/* GeanyIndentType */


/* Note: We use enum instead of 'static const gint' to allow its use in global variable
 * initializing, otherwise we get errors like:
 * error: initializer element is not constant */
/** Versioning data */
enum {
	/** The Application Programming Interface (API) version, incremented
	 * whenever any plugin data types are modified or appended to. */
	GEANY_API_VERSION = 136,

	/** The Application Binary Interface (ABI) version, incremented whenever
	 * existing fields in the plugin data types have to be changed or reordered. */
	/* This should usually stay the same if fields are only appended, assuming only pointers to
	 * structs and not structs themselves are declared by plugins. */
	GEANY_ABI_VERSION = 59
};

/** Check the plugin can be loaded by Geany.
 * This performs runtime checks that try to ensure:
 * - Geany ABI data types are compatible with this plugin.
 * - Geany sources provide the required API for this plugin. */
#define PLUGIN_VERSION_CHECK(api_required) \
	gint plugin_version_check(gint abi_ver) \
	{ \
		if (abi_ver != GEANY_ABI_VERSION) \
			return -1; \
		if (GEANY_API_VERSION < (api_required)) \
			return (api_required); \
		else return 0; \
	}


/** Basic information about a plugin available to Geany without loading the plugin.
 * The fields are set in plugin_set_info(), usually with the PLUGIN_SET_INFO() macro. */
typedef struct PluginInfo
{
	/** The name of the plugin. */
	gchar	*name;
	/** The description of the plugin. */
	gchar	*description;
	/** The version of the plugin. */
	gchar	*version;
	/** The author of the plugin. */
	gchar	*author;
}
PluginInfo;


/** Basic information for the plugin and identification. */
typedef struct GeanyPlugin
{
	PluginInfo	*info;	/**< Fields set in plugin_set_info(). */

	struct GeanyPluginPrivate *priv;	/* private */
}
GeanyPlugin;


/** Set the plugin name and some other basic information about a plugin.
 * This declares a function, so you can use the _() translation macro for arguments.
 *
 * Example:
 * @code PLUGIN_SET_INFO(_("Cool Feature"), _("Adds cool feature support."), "0.1", "Joe Author") @endcode */
/* plugin_set_info() could be written manually for plugins if we want to add any
 * extra PluginInfo features (such as an icon), so we don't need to break API
 * compatibility. Alternatively just add a new macro, PLUGIN_SET_INFO_FULL(). -ntrel */
#define PLUGIN_SET_INFO(p_name, p_description, p_version, p_author) \
	void plugin_set_info(PluginInfo* info) \
	{ \
		info->name = (p_name); \
		info->description = (p_description); \
		info->version = (p_version); \
		info->author = (p_author); \
	}


/** Declare and initialise a keybinding group.
 * @code GeanyKeyGroup plugin_key_group[1]; @endcode
 * You must then set the @c plugin_key_group::keys[] entries for the group in plugin_init().
 * The @c plugin_key_group::label field is set by Geany after @c plugin_init()
 * is called, to the name of the plugin.
 * @param group_name A unique group name (without quotes) to be used in the
 * configuration file, such as @c html_chars.
 * @param key_count	The number of keybindings the group will hold. */
#define PLUGIN_KEY_GROUP(group_name, key_count) \
	static GeanyKeyBinding plugin_keys[key_count]; \
	\
	/* We have to declare plugin_key_group as a single element array.
	 * Declaring as a pointer to a struct doesn't work with g_module_symbol(). */ \
	GeanyKeyGroup plugin_key_group[1] = \
	{ \
		{G_STRINGIFY(group_name), NULL, key_count, plugin_keys} \
	};


/** callback array entry */
typedef struct PluginCallback
{
	/** The name of signal, must be an existing signal. For a list of available signals,
	 *  please see the @link signals Signal documentation @endlink. */
	gchar		*signal_name;
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
	/** Bitmask of PluginFlags. */
	PluginFlags	flags;
	/** Pointer to a plugin's menu item which will be automatically enabled/disabled when there
	 *  are no open documents and PLUGIN_IS_DOCUMENT_SENSITIVE is set.
	 *  This is required if using PLUGIN_IS_DOCUMENT_SENSITIVE, ignored otherwise */
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
	struct NavQueueFuncs        *p_navqueue;		/**< See navqueue.h */
	struct EditorFuncs        	*p_editor;			/**< See editor.h */
	struct MainFuncs        	*p_main;			/**< See main.h */
	struct PluginFuncs        	*p_plugin;			/**< See plugins.c */
	struct ScintillaFuncs		*p_scintilla;		/**< See ScintillaFuncs */
	struct MsgWinFuncs			*p_msgwin;			/**< See msgwindow.h */
}
GeanyFunctions;


/* For more information about these functions, see the main source code.
 * E.g. for p_document->new_file(), see document_new_file() in document.c. */


/* See document.h */
typedef struct DocumentFuncs
{
	struct GeanyDocument*	(*new_file) (const gchar *utf8_filename, struct GeanyFiletype *ft,
			const gchar *text);
	struct GeanyDocument*	(*get_current) (void);
	struct GeanyDocument*	(*get_from_page) (guint page_num);
	struct GeanyDocument*	(*find_by_filename) (const gchar *utf8_filename);
	struct GeanyDocument*	(*find_by_real_path) (const gchar *realname);
	gboolean				(*save_file) (struct GeanyDocument *doc, gboolean force);
	struct GeanyDocument*	(*open_file) (const gchar *locale_filename, gboolean readonly,
			struct GeanyFiletype *ft, const gchar *forced_enc);
	void		(*open_files) (const GSList *filenames, gboolean readonly,
			struct GeanyFiletype *ft, const gchar *forced_enc);
	gboolean	(*remove_page) (guint page_num);
	gboolean	(*reload_file) (struct GeanyDocument *doc, const gchar *forced_enc);
	void		(*set_encoding) (struct GeanyDocument *doc, const gchar *new_encoding);
	void		(*set_text_changed) (struct GeanyDocument *doc, gboolean changed);
	void		(*set_filetype) (struct GeanyDocument *doc, struct GeanyFiletype *type);
	gboolean	(*close) (struct GeanyDocument *doc);
	struct GeanyDocument*	(*index)(gint idx);
	gboolean	(*save_file_as) (struct GeanyDocument *doc, const gchar *utf8_fname);
	void		(*rename_file) (struct GeanyDocument *doc, const gchar *new_filename);
	const GdkColor*	(*get_status_color) (struct GeanyDocument *doc);
}
DocumentFuncs;


struct _ScintillaObject;

/** See http://scintilla.org for the full documentation. */
typedef struct ScintillaFuncs
{
	/** Send Scintilla a message. */
	long int	(*send_message) (struct _ScintillaObject* sci, unsigned int iMessage,
			long unsigned int wParam, long int lParam);
	/** Create a new ScintillaObject widget. */
	GtkWidget*	(*new)(void);
}
ScintillaFuncs;


/** Wrapper functions for Scintilla messages.
 * See sciwrappers.h for the list of functions. */
typedef struct SciFuncs
{
	/** @deprecated Use @ref ScintillaFuncs::send_message() instead. */
	long int (*send_message) (struct _ScintillaObject* sci, unsigned int iMessage,
			long unsigned int wParam, long int lParam);
	void	(*send_command) (struct _ScintillaObject* sci, gint cmd);

	void	(*start_undo_action) (struct _ScintillaObject* sci);
	void	(*end_undo_action) (struct _ScintillaObject* sci);
	void	(*set_text) (struct _ScintillaObject *sci, const gchar *text);
	void	(*insert_text) (struct _ScintillaObject *sci, gint pos, const gchar *text);
	void	(*get_text) (struct _ScintillaObject *sci, gint len, gchar *text);
	gint	(*get_length) (struct _ScintillaObject *sci);
	gint	(*get_current_position) (struct _ScintillaObject *sci);
	void	(*set_current_position) (struct _ScintillaObject* sci, gint position,
			 gboolean scroll_to_caret);
	gint	(*get_col_from_position) (struct _ScintillaObject* sci, gint position);
	gint	(*get_line_from_position) (struct _ScintillaObject* sci, gint position);
	gint	(*get_position_from_line) (struct _ScintillaObject* sci, gint line);
	void	(*replace_sel) (struct _ScintillaObject* sci, const gchar *text);
	void	(*get_selected_text) (struct _ScintillaObject* sci, gchar *text);
	gint	(*get_selected_text_length) (struct _ScintillaObject* sci);
	gint	(*get_selection_start) (struct _ScintillaObject* sci);
	gint	(*get_selection_end) (struct _ScintillaObject* sci);
	gint	(*get_selection_mode) (struct _ScintillaObject* sci);
	void	(*set_selection_mode) (struct _ScintillaObject* sci, gint mode);
	void	(*set_selection_start) (struct _ScintillaObject* sci, gint position);
	void	(*set_selection_end) (struct _ScintillaObject* sci, gint position);
	void	(*get_text_range) (struct _ScintillaObject* sci, gint start, gint end, gchar *text);
	gchar*	(*get_line) (struct _ScintillaObject* sci, gint line_num);
	gint	(*get_line_length) (struct _ScintillaObject* sci, gint line);
	gint	(*get_line_count) (struct _ScintillaObject* sci);
	gboolean (*get_line_is_visible) (struct _ScintillaObject* sci, gint line);
	void	(*ensure_line_is_visible) (struct _ScintillaObject* sci, gint line);
	void	(*scroll_caret) (struct _ScintillaObject* sci);
	gint	(*find_matching_brace) (struct _ScintillaObject* sci, gint pos);
	gint	(*get_style_at) (struct _ScintillaObject *sci, gint position);
	gchar	(*get_char_at) (struct _ScintillaObject *sci, gint pos);
	gint	(*get_current_line) (struct _ScintillaObject *sci);
	gboolean (*has_selection) (struct _ScintillaObject *sci);
	gint	(*get_tab_width) (struct _ScintillaObject *sci);
	void	(*indicator_clear) (struct _ScintillaObject *sci, gint start, gint end);
	void	(*indicator_set) (struct _ScintillaObject *sci, gint indic);
	gchar*	(*get_contents) (struct _ScintillaObject *sci, gint len);
	gchar*	(*get_contents_range) (struct _ScintillaObject *sci, gint start, gint end);
	gchar*	(*get_selection_contents) (struct _ScintillaObject *sci);
}
SciFuncs;


/* See templates.h */
typedef struct TemplateFuncs
{
	gchar*		(*get_template_fileheader) (gint filetype_idx, const gchar *fname);
}
TemplateFuncs;


/* See utils.h */
typedef struct UtilsFuncs
{
	gboolean	(*str_equal) (const gchar *a, const gchar *b);
	guint		(*string_replace_all) (GString *haystack, const gchar *needle,
				 const gchar *replacement);
	GSList*		(*get_file_list) (const gchar *path, guint *length, GError **error);
	gint		(*write_file) (const gchar *filename, const gchar *text);
	gchar*		(*get_locale_from_utf8) (const gchar *utf8_text);
	gchar*		(*get_utf8_from_locale) (const gchar *locale_text);
	gchar*		(*remove_ext_from_filename) (const gchar *filename);
	gint		(*mkdir) (const gchar *path, gboolean create_parent_dirs);
	gboolean	(*get_setting_boolean) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gboolean default_value);
	gint		(*get_setting_integer) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gint default_value);
	gchar*		(*get_setting_string) (GKeyFile *config, const gchar *section, const gchar *key,
				 const gchar *default_value);
	gboolean	(*spawn_sync) (const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
				 GSpawnChildSetupFunc child_setup, gpointer user_data, gchar **std_out,
				 gchar **std_err, gint *exit_status, GError **error);
	gboolean	(*spawn_async) (const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
				 GSpawnChildSetupFunc child_setup, gpointer user_data, GPid *child_pid,
				 GError **error);
	gint		(*str_casecmp) (const gchar *s1, const gchar *s2);
	gchar*		(*get_date_time) (const gchar *format, time_t *time_to_use);
	void		(*open_browser) (const gchar *uri);
	guint		(*string_replace_first) (GString *haystack, const gchar *needle,
				 const gchar *replace);
}
UtilsFuncs;


/* See main.h */
typedef struct MainFuncs
{
	void		(*reload_configuration) (void);
	void		(*locale_init) (const gchar *locale_dir, const gchar *package);

}
MainFuncs;


/* See ui_utils.h */
typedef struct UIUtilsFuncs
{
	GtkWidget*	(*dialog_vbox_new) (GtkDialog *dialog);
	GtkWidget*	(*frame_new_with_alignment) (const gchar *label_text, GtkWidget **alignment);

	/* set_statusbar() also appends to the message window status tab if log is TRUE. */
	void		(*set_statusbar) (gboolean log, const gchar *format, ...) G_GNUC_PRINTF (2, 3);

	void		(*table_add_row) (GtkTable *table, gint row, ...) G_GNUC_NULL_TERMINATED;
	GtkWidget*	(*path_box_new) (const gchar *title, GtkFileChooserAction action, GtkEntry *entry);
	GtkWidget*	(*button_new_with_image) (const gchar *stock_id, const gchar *text);
	void		(*add_document_sensitive) (GtkWidget *widget);
	void		(*widget_set_tooltip_text) (GtkWidget *widget, const gchar *text);
	GtkWidget*	(*image_menu_item_new) (const gchar *stock_id, const gchar *label);
	GtkWidget*	(*lookup_widget) (GtkWidget *widget, const gchar *widget_name);
	void		(*progress_bar_start) (const gchar *text);
	void		(*progress_bar_stop) (void);
	void		(*entry_add_clear_icon) (GtkWidget *entry);
}
UIUtilsFuncs;


/* See dialogs.h */
typedef struct DialogFuncs
{
	gboolean	(*show_question) (const gchar *text, ...);
	void		(*show_msgbox) (gint type, const gchar *text, ...);
	gboolean	(*show_save_as) (void);
	gboolean	(*show_input_numeric) (const gchar *title, const gchar *label_text,
				 gdouble *value, gdouble min, gdouble max, gdouble step);

}
DialogFuncs;


/* @deprecated Use ui_lookup_widget() instead. */
typedef struct SupportFuncs
{
	GtkWidget*	(*lookup_widget) (GtkWidget *widget, const gchar *widget_name);
}
SupportFuncs;


/* See msgwindow.h */
typedef struct MsgWinFuncs
{
	/* status_add() does not set the status bar - use ui->set_statusbar() instead. */
	void		(*status_add) (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
	void		(*compiler_add) (gint msg_color, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
	void		(*msg_add) (gint msg_color, gint line, struct GeanyDocument *doc,
				 const gchar *format, ...) G_GNUC_PRINTF (4, 5);
	void		(*clear_tab) (gint tabnum);
	void		(*switch_tab) (gint tabnum, gboolean show);
}
MsgWinFuncs;


/* See encodings.h */
typedef struct EncodingFuncs
{
	gchar*			(*convert_to_utf8) (const gchar *buffer, gsize size, gchar **used_encoding);
	gchar* 			(*convert_to_utf8_from_charset) (const gchar *buffer, gsize size,
													 const gchar *charset, gboolean fast);
	const gchar*	(*get_charset_from_index) (gint idx);
}
EncodingFuncs;


struct GeanyKeyGroup;
/* avoid including keybindings.h */
typedef void (*_GeanyKeyCallback) (guint key_id);

/* See keybindings.h */
typedef struct KeybindingFuncs
{
	void		(*send_command) (guint group_id, guint key_id);
	void		(*set_item) (struct GeanyKeyGroup *group, gsize key_id,
					_GeanyKeyCallback callback, guint key, GdkModifierType mod,
					gchar *name, gchar *label, GtkWidget *menu_item);
}
KeybindingFuncs;


/* See highlighting.h */
typedef struct HighlightingFuncs
{
	const struct GeanyLexerStyle* (*get_style) (gint ft_id, gint style_id);
}
HighlightingFuncs;


/* See filetypes.h */
typedef struct FiletypeFuncs
{
	GeanyFiletype*	(*detect_from_file) (const gchar *utf8_filename);
	GeanyFiletype*	(*lookup_by_name) (const gchar *name);
	GeanyFiletype*	(*index)(gint idx);
	/* Remember to convert any filetype_id arguments to GeanyFiletype pointers in any
	 * appended functions */
}
FiletypeFuncs;


/* See search.h */
typedef struct SearchFuncs
{
	void		(*show_find_in_files_dialog) (const gchar *dir);
}
SearchFuncs;


/* See tagmanager/include */
typedef struct TagManagerFuncs
{
	gchar*			(*get_real_path) (const gchar *file_name);
	TMWorkObject*	(*source_file_new) (const char *file_name, gboolean update, const char *name);
	gboolean		(*workspace_add_object) (TMWorkObject *work_object);
	gboolean		(*source_file_update) (TMWorkObject *source_file, gboolean force,
					 gboolean recurse, gboolean update_parent);
	void			(*work_object_free) (gpointer work_object);
	gboolean		(*workspace_remove_object) (TMWorkObject *w, gboolean do_free, gboolean update);
}
TagManagerFuncs;


/* See navqueue.h */
typedef struct NavQueueFuncs
{
	gboolean		(*goto_line) (struct GeanyDocument *old_doc, struct GeanyDocument *new_doc,
					 gint line);
}
NavQueueFuncs;


struct GeanyEditor;

/* See editor.h */
typedef struct EditorFuncs
{
	const struct GeanyIndentPrefs* (*get_indent_prefs)(struct GeanyEditor *editor);
	struct _ScintillaObject* (*create_widget)(struct GeanyEditor *editor);

	void	(*indicator_set_on_range) (struct GeanyEditor *editor, gint indic, gint start, gint end);
	void	(*indicator_set_on_line) (struct GeanyEditor *editor, gint indic, gint line);
	void	(*indicator_clear) (struct GeanyEditor *editor, gint indic);

	void	(*set_indent_type)(struct GeanyEditor *editor, GeanyIndentType type);
	gchar*	(*get_word_at_pos) (struct GeanyEditor *editor, gint pos, const gchar *wordchars);

	/* Remember to convert any GeanyDocument or ScintillaObject pointers in any
	 * appended functions to GeanyEditor pointers. */
}
EditorFuncs;


/* See plugins.c */
typedef struct PluginFuncs
{
	void	(*add_toolbar_item)(GeanyPlugin *plugin, GtkToolItem *item);
	void	(*module_make_resident) (GeanyPlugin *plugin);
}
PluginFuncs;


/* Deprecated aliases */
#ifndef GEANY_DISABLE_DEPRECATED

/** NULL-safe way to get the index of @a doc_ptr in the documents array. */
#define DOC_IDX(doc_ptr) \
	(doc_ptr ? doc_ptr->index : -1)
#define DOC_IDX_VALID(doc_idx) \
	((doc_idx) >= 0 && (guint)(doc_idx) < documents_array->len && documents[doc_idx]->is_valid)

#endif	/* GEANY_DISABLE_DEPRECATED */

#endif
