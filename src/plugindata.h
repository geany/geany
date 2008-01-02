/*
 *      plugindata.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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

#ifndef PLUGIN_H
#define PLUGIN_H

/**
 * Public symbols supported:
 *
 * version_check()
 * 	Use VERSION_CHECK() macro instead. Required by Geany.
 *
 * PluginInfo* info()
 * 	Use PLUGIN_INFO() macro to define it. Required by Geany.
 *
 * GeanyData* geany_data
 * 	Geany owned fields and functions.
 *
 * PluginFields* plugin_fields
 * 	Plugin owned fields, including flags.
 *
 * GeanyCallback geany_callbacks[]
 * 	An array for connecting GeanyObject events, which should be terminated with
 * 	{NULL, NULL, FALSE, NULL}. See signals below.
 *
 * void configure(GtkWidget *parent)
 * 	Called when the plugin should show a configure dialog to let the user set some basic
 *  plugin configuration. Optionally, can be omitted when not needed.
 *
 * void init(GeanyData *data)
 * 	Called after loading the plugin. data is the same as geany_data.
 *
 * void cleanup()
 * 	Called before unloading the plugin. Required for normal plugins - it should undo
 * 	everything done in init() - e.g. destroy menu items, free memory.
 */

/**
 * Signals:
 *
 * "document-new"
 * 	Sent when a new document is created.
 *  Handler: void user_function(GObject *obj, gint idx, gpointer user_data);
 *
 * "document-open"
 * 	Sent when a file is opened.
 *  Handler: void user_function(GObject *obj, gint idx, gpointer user_data);
 *
 * "document-save"
 * 	Sent when a file is saved.
 *  Handler: void user_function(GObject *obj, gint idx, gpointer user_data);
 *
 * "document-activate"
 * 	Sent when switching notebook pages.
 *  Handler: void user_function(GObject *obj, gint idx, gpointer user_data);
 *
 * "project-open"
 * 	Sent after a project is opened but before session files are loaded.
 *  Handler: void user_function(GObject *obj, GKeyFile *config, gpointer user_data);
 *
 * "project-save"
 * 	Sent when a project is saved(happens when the project is created, the properties
 *  dialog is closed or Geany is exited). This signal is emitted shortly before Geany
 *  will write the contents of the GKeyFile to the disc.
 *  Handler: void user_function(GObject *obj, GKeyFile *config, gpointer user_data);
 *
 * "project-close"
 * 	Sent after a project is closed.
 *  Handler: void user_function(GObject *obj, gpointer user_data);
 */


/* The API version should be incremented whenever any plugin data types below are
 * modified or appended to. */
static const gint api_version = 37;

/* The ABI version should be incremented whenever existing fields in the plugin
 * data types below have to be changed or reordered. It should stay the same if fields
 * are only appended, as this doesn't affect existing fields. */
static const gint abi_version = 19;

/* This performs runtime checks that try to ensure:
 * 1. Geany ABI data types are compatible with this plugin.
 * 2. Geany sources provide the required API for this plugin. */
/* TODO: if possible, the API version should be checked at compile time, not runtime. */
#define VERSION_CHECK(api_required) \
	gint version_check(gint abi_ver) \
	{ \
		if (abi_ver != abi_version) \
			return -1; \
		if (api_version < (api_required)) \
			return (api_required); \
		else return 0; \
	}


typedef struct PluginInfo
{
	gchar	*name;			// name of plugin
	gchar	*description;	// description of plugin
	gchar	*version;		// version of plugin
	gchar	*author;		// author of plugin
	gpointer reserved2;		// reserved for later use
}
PluginInfo;

#include <string.h>

/* Sets the plugin name and a brief description of what it is. */
#define PLUGIN_INFO(p_name, p_description, p_version, p_author) \
	PluginInfo *info() \
	{ \
		static PluginInfo p_info; \
		\
		memset(&p_info, 0, sizeof(PluginInfo)); \
		p_info.name = (p_name); \
		p_info.description = (p_description); \
		p_info.version = (p_version); \
		p_info.author = (p_author); \
		return &p_info; \
	}


/* For geany_callbacks array - see top of file. */
typedef struct GeanyCallback
{
	gchar		*signal_name;
	GCallback	callback;
	gboolean	after;
	gpointer	user_data;
}
GeanyCallback;


typedef enum
{
	PLUGIN_IS_DOCUMENT_SENSITIVE	= 1 << 0	// if menu_item should be disabled when there are no documents
}
PluginFlags;

/* Fields set and owned by the plugin.
 * Note: Remember to increment api_version (and abi_version if necessary) when
 * making changes. */
typedef struct PluginFields
{
	PluginFlags	flags;
	GtkWidget	*menu_item;	// required if using PLUGIN_IS_DOCUMENT_SENSITIVE, ignored otherwise
}
PluginFields;


/* These are fields and functions owned by Geany.
 * Fields and functions will be appended when needed by plugin authors.
 * Note: Remember to increment api_version (and abi_version if necessary) when
 * making changes. */
typedef struct GeanyData
{
	GeanyApp	*app;	// Geany application data fields
	GtkWidget	*tools_menu;	// Almost all plugins should add menu items to the Tools menu only
	GArray		*doc_array;	// array of document pointers
	struct filetype		**filetypes;
	struct GeanyPrefs	*prefs;
	struct EditorPrefs	*editor_prefs;
	struct BuildInfo	*build_info;

	struct DocumentFuncs		*documents;
	struct ScintillaFuncs		*sci;
	struct TemplateFuncs		*templates;
	struct UtilsFuncs			*utils;
	struct UIUtilsFuncs			*ui;
	struct SupportFuncs			*support;
	struct DialogFuncs			*dialogs;
	struct MsgWinFuncs			*msgwindow;
	struct EncodingFuncs		*encoding;
	struct KeybindingFuncs		*keybindings;
	struct TagManagerFuncs		*tm;
	struct SearchFuncs			*search;
	struct HighlightingFuncs	*highlighting;
	struct FiletypeFuncs		*filetype;
}
GeanyData;

typedef GeanyData PluginData;	// for compatibility with API < 7


/* For more info about these functions, see the main source code.
 * E.g. for GeanyData::document->new_file(), see document_new_file() in document.[hc]. */


typedef struct DocumentFuncs
{
	gint	(*new_file) (const gchar *filename, struct filetype *ft, const gchar *text);
	gint	(*get_cur_idx) ();
	gint	(*get_n_idx) (guint i);
	gint	(*find_by_filename) (const gchar *filename, gboolean is_tm_filename);
	struct document* (*get_current) ();
	gboolean (*save_file)(gint idx, gboolean force);
	gint	(*open_file)(const gchar *locale_filename, gboolean readonly,
			struct filetype *ft, const gchar *forced_enc);
	void	(*open_files)(const GSList *filenames, gboolean readonly, struct filetype *ft,
			const gchar *forced_enc);
	gboolean (*remove)(guint page_num);
	gboolean (*reload_file)(gint idx, const gchar *forced_enc);
	void	(*set_encoding)(gint idx, const gchar *new_encoding);
	void	(*set_text_changed)(gint idx);
}
DocumentFuncs;

struct _ScintillaObject;

typedef struct ScintillaFuncs
{
	long int (*send_message) (struct _ScintillaObject* sci, unsigned int iMessage,
			long unsigned int wParam, long int lParam);
	void	(*send_command) (struct _ScintillaObject* sci, gint cmd);

	void	(*start_undo_action) (struct _ScintillaObject* sci);
	void	(*end_undo_action) (struct _ScintillaObject* sci);
	void	(*set_text) (struct _ScintillaObject *sci, const gchar *text);
	void	(*insert_text) (struct _ScintillaObject *sci, gint pos, const gchar *text);
	void	(*get_text) (struct _ScintillaObject *sci, gint len, gchar* text);
	gint	(*get_length) (struct _ScintillaObject *sci);
	gint	(*get_current_position) (struct _ScintillaObject *sci);
	void	(*set_current_position) (struct _ScintillaObject* sci, gint position, gboolean scroll_to_caret);
	gint	(*get_col_from_position) (struct _ScintillaObject* sci, gint position);
	gint	(*get_line_from_position) (struct _ScintillaObject* sci, gint position);
	gint	(*get_position_from_line) (struct _ScintillaObject* sci, gint line);
	void	(*replace_sel) (struct _ScintillaObject* sci, const gchar* text);
	void	(*get_selected_text) (struct _ScintillaObject* sci, gchar* text);
	gint	(*get_selected_text_length) (struct _ScintillaObject* sci);
	gint	(*get_selection_start) (struct _ScintillaObject* sci);
	gint	(*get_selection_end) (struct _ScintillaObject* sci);
	gint	(*get_selection_mode) (struct _ScintillaObject* sci);
	void	(*set_selection_mode) (struct _ScintillaObject* sci, gint mode);
	void	(*set_selection_start) (struct _ScintillaObject* sci, gint position);
	void	(*set_selection_end) (struct _ScintillaObject* sci, gint position);
	void	(*get_text_range) (struct _ScintillaObject* sci, gint start, gint end, gchar*text);
	gchar*	(*get_line) (struct _ScintillaObject* sci, gint line_num);
	gint	(*get_line_length) (struct _ScintillaObject* sci, gint line);
	gint	(*get_line_count) (struct _ScintillaObject* sci);
	gboolean (*get_line_is_visible) (struct _ScintillaObject* sci, gint line);
	void	(*ensure_line_is_visible) (struct _ScintillaObject* sci, gint line);
	void	(*scroll_caret) (struct _ScintillaObject* sci);
	gint	(*find_bracematch) (struct _ScintillaObject* sci, gint pos);
	gint	(*get_style_at) (struct _ScintillaObject *sci, gint position);
	gchar	(*get_char_at) (struct _ScintillaObject *sci, gint pos);
}
ScintillaFuncs;

typedef struct TemplateFuncs
{
	gchar*		(*get_template_fileheader) (gint filetype_idx, const gchar *fname);
}
TemplateFuncs;

typedef struct UtilsFuncs
{
	gboolean	(*str_equal) (const gchar *a, const gchar *b);
	gboolean	(*string_replace_all) (GString *haystack, const gchar *needle,
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
}
UtilsFuncs;

typedef struct UIUtilsFuncs
{
	GtkWidget*	(*dialog_vbox_new) (GtkDialog *dialog);
	GtkWidget*	(*frame_new_with_alignment) (const gchar *label_text, GtkWidget **alignment);

	/* set_statusbar() also appends to the message window status tab if log is TRUE. */
	void		(*set_statusbar) (gboolean log, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
}
UIUtilsFuncs;

typedef struct DialogFuncs
{
	gboolean	(*show_question) (const gchar *text, ...);
	void		(*show_msgbox) (gint type, const gchar *text, ...);
	gboolean	(*show_save_as) (void);
}
DialogFuncs;

typedef struct SupportFuncs
{
	GtkWidget*	(*lookup_widget) (GtkWidget *widget, const gchar *widget_name);
}
SupportFuncs;


typedef struct MsgWinFuncs
{
	/* status_add() does not set the status bar - use ui->set_statusbar() instead. */
	void		(*status_add) (const gchar *format, ...);
	void		(*compiler_add) (gint msg_color, const gchar *format, ...) G_GNUC_PRINTF (2, 3);
}
MsgWinFuncs;


typedef struct EncodingFuncs
{
	gchar*	(*convert_to_utf8) (const gchar *buffer, gsize size, gchar **used_encoding);
	gchar* 	(*convert_to_utf8_from_charset) (const gchar *buffer, gsize size,
											const gchar *charset, gboolean fast);
}
EncodingFuncs;


typedef struct KeybindingFuncs
{
	/* See GeanyKeyCommand enum for cmd_id. */
	void		(*send_command) (gint cmd_id);
}
KeybindingFuncs;


typedef struct HighlightingFuncs
{
	const struct HighlightingStyle* (*get_style) (gint ft_id, gint style_id);
}
HighlightingFuncs;


typedef struct FiletypeFuncs
{
	filetype*	(*detect_from_filename) (const gchar *utf8_filename);
}
FiletypeFuncs;


typedef struct SearchFuncs
{
	void		(*show_find_in_files_dialog) (const gchar *dir);
}
SearchFuncs;

typedef struct TagManagerFuncs
{
	gchar*			(*get_real_path) (const gchar *file_name);
	TMWorkObject*	(*source_file_new) (const char *file_name, gboolean update, const char *name);
	gboolean		(*workspace_add_object) (TMWorkObject *work_object);
	gboolean		(*source_file_update) (TMWorkObject *source_file, gboolean force,
					 gboolean recurse, gboolean update_parent);
	void			(*work_object_free) (gpointer work_object);
	gboolean		(*workspace_remove_object) (TMWorkObject *w, gboolean do_free);
}
TagManagerFuncs;

#endif
