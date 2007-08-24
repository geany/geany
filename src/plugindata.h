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
 *
 * "document-open"
 * 	Sent when a file is opened.
 *
 * "document-save"
 * 	Sent when a file is saved.
 *
 * "document-activate"
 * 	Sent when switching notebook pages.
 */


/* The API version should be incremented whenever any plugin data types below are
 * modified. */
static const gint api_version = 14;

/* The ABI version should be incremented whenever existing fields in the plugin
 * data types below have to be changed or reordered. It should stay the same if fields
 * are only appended, as this doesn't affect existing fields. */
static const gint abi_version = 5;

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
}
PluginInfo;

/* Sets the plugin name and a brief description of what it is. */
#define PLUGIN_INFO(p_name, p_description) \
	PluginInfo *info() \
	{ \
		static PluginInfo p_info; \
		 \
		p_info.name = (p_name); \
		p_info.description = (p_description); \
		return &p_info; \
	}


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

	struct DocumentFuncs	*document;
	struct ScintillaFuncs	*sci;
	struct TemplateFuncs	*templates;
	struct UtilsFuncs		*utils;
	struct UIUtilsFuncs		*ui;
	struct SupportFuncs		*support;
	struct DialogFuncs		*dialogs;
	struct MsgWinFuncs		*msgwindow;
}
GeanyData;

typedef GeanyData PluginData;	// for compatibility with API < 7


/* For more info about these functions, see the main source code.
 * E.g. for GeanyData::document->new_file(), see document_new_file() in document.[hc]. */

struct filetype;

typedef struct DocumentFuncs
{
	gint	(*new_file) (const gchar *filename, struct filetype *ft);
	gint	(*get_cur_idx) ();
	struct document*	(*get_current) ();
	gboolean (*save_file)(gint idx, gboolean force);
	gboolean (*open_file)(gint idx, const gchar *filename, gint pos, gboolean readonly,
		struct filetype *ft, const gchar *forced_enc);
	void	(*open_files)(const GSList *filenames, gboolean readonly, struct filetype *ft,
			const gchar *forced_enc);
	gboolean (*remove)(guint page_num);
}
DocumentFuncs;

struct _ScintillaObject;

typedef struct ScintillaFuncs
{
	long int	(*send_message) (struct _ScintillaObject* sci, unsigned int iMessage,
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
	gboolean	(*get_line_is_visible) (struct _ScintillaObject* sci, gint line);
	void	(*ensure_line_is_visible) (struct _ScintillaObject* sci, gint line);
	void	(*scroll_caret) (struct _ScintillaObject* sci);
	gint	(*find_bracematch) (struct _ScintillaObject* sci, gint pos);
	gint	(*get_style_at) (struct _ScintillaObject *sci, gint position);
	gchar	(*get_char_at) (struct _ScintillaObject *sci, gint pos);
	gint	(*get_zoom) (struct _ScintillaObject * sci);
}
ScintillaFuncs;

typedef struct TemplateFuncs
{
	gchar*	(*get_template_fileheader) (gint filetype_idx, const gchar *fname);
}
TemplateFuncs;

typedef struct UtilsFuncs
{
	gboolean	(*str_equal) (const gchar *a, const gchar *b);
	gchar*		(*str_replace) (gchar *haystack, const gchar *needle, const gchar *replacement);
	GSList*		(*get_file_list) (const gchar *path, guint *length, GError **error);
	gint		(*write_file) (const gchar *filename, const gchar *text);
	gchar*		(*get_locale_from_utf8) (const gchar *utf8_text);
	gchar*		(*get_utf8_from_locale) (const gchar *locale_text);
	gchar*		(*remove_ext_from_filename) (const gchar *filename);
}
UtilsFuncs;

typedef struct UIUtilsFuncs
{
	GtkWidget*	(*dialog_vbox_new) (GtkDialog *dialog);
	GtkWidget*	(*frame_new_with_alignment) (const gchar *label_text, GtkWidget **alignment);
}
UIUtilsFuncs;

typedef struct DialogFuncs
{
	gboolean	(*show_question) (const gchar *text, ...);
	void		(*show_msgbox) (gint type, const gchar *text, ...);
}
DialogFuncs;

typedef struct SupportFuncs
{
	GtkWidget*	(*lookup_widget) (GtkWidget *widget, const gchar *widget_name);
}
SupportFuncs;


typedef struct MsgWinFuncs
{
	void		(*status_add) (const gchar *format, ...);
}
MsgWinFuncs;


typedef struct GeanyCallback
{
	gchar		*signal_name;
	GCallback	callback;
	gboolean	after;
	gpointer	user_data;
}
GeanyCallback;

#endif
