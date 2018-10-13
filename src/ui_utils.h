/*
 *      ui_utils.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef GEANY_UI_UTILS_H
#define GEANY_UI_UTILS_H 1

#include "geany.h" /* for GEANY_DEPRECATED */
#include "document.h"

#include "gtkcompat.h"

G_BEGIN_DECLS

/** Sets a name to lookup @a widget from @a owner.
 * @param owner Usually a window, dialog or popup menu.
 * @param widget Widget.
 * @param widget_name Name.
 * @see ui_lookup_widget().
 *
 *  @since 0.16
 **/
#define ui_hookup_widget(owner, widget, widget_name) \
	g_object_set_data_full(G_OBJECT(owner), widget_name, \
		g_object_ref(widget), (GDestroyNotify)g_object_unref);


/** Interface preferences */
typedef struct GeanyInterfacePrefs
{
	gboolean		sidebar_symbol_visible;		/**< whether the symbol sidebar is visible */
	gboolean		sidebar_openfiles_visible;	/**< whether the open file list is visible */
	gchar			*editor_font;				/**< editor font */
	gchar			*tagbar_font;				/**< symbol sidebar font */
	gchar			*msgwin_font;				/**< message window font */
	gboolean		show_notebook_tabs;			/**< whether editor tabs are visible */
	gint			tab_pos_editor;				/**< positions of editor's tabs */
	gint			tab_pos_msgwin;				/**< positions of message window's tabs */
	gint			tab_pos_sidebar;			/**< positions of sidebar's tabs */
	gboolean		statusbar_visible;			/**< whether the status bar is visible */
	gboolean		show_symbol_list_expanders;	/**< whether to show expanders in the symbol list */
	/** whether a double click on notebook tabs hides all other windows */
	gboolean		notebook_double_click_hides_widgets;
	gboolean		highlighting_invert_all; 	/**< whether highlighting colors are inverted */
	gint			sidebar_pos; 				/**< position of the sidebar (left or right) */
	gboolean		msgwin_status_visible; 		/**< whether message window's status tab is visible */
	gboolean		msgwin_compiler_visible;	/**< whether message window's compiler tab is visible */
	gboolean		msgwin_messages_visible;	/**< whether message window's messages tab is visible */
	gboolean		msgwin_scribble_visible;	/**< whether message window's scribble tab is visible */
	/** whether to use native Windows' dialogs (only used on Windows) */
	gboolean		use_native_windows_dialogs;
	/** whether compiler messages window is automatically scrolled to show new messages */
	gboolean		compiler_tab_autoscroll;
	gint			msgwin_orientation;			/**< orientation of the message window */
	gint 			symbols_sort_mode;			/**< symbol list sorting mode */
}
GeanyInterfacePrefs;


/** Important widgets in the main window.
 * Accessed by @c geany->main_widgets. */
typedef struct GeanyMainWidgets
{
	GtkWidget	*window;			/**< Main window. */
	GtkWidget	*toolbar;			/**< Main toolbar. */
	GtkWidget	*sidebar_notebook;	/**< Sidebar notebook. */
	GtkWidget	*notebook;			/**< Document notebook. */
	GtkWidget	*editor_menu;		/**< Popup editor menu. */
	GtkWidget	*tools_menu;		/**< Most plugins add menu items to the Tools menu. */
	/** Progress bar widget in the status bar to show progress of various actions.
	 * See ui_progress_bar_start() for details. */
	GtkWidget	*progressbar;
	GtkWidget	*message_window_notebook; /**< Message Window notebook. */
	/** Plugins modifying the project can add their items to the Project menu. */
	GtkWidget	*project_menu;
}
GeanyMainWidgets;


#define GEANY_STOCK_SAVE_ALL "geany-save-all"
#define GEANY_STOCK_CLOSE_ALL "geany-close-all"
#define GEANY_STOCK_BUILD "geany-build"


GtkWidget *ui_dialog_vbox_new(GtkDialog *dialog);

void ui_set_statusbar(gboolean log, const gchar *format, ...) G_GNUC_PRINTF (2, 3);

void ui_table_add_row(GtkTable *table, gint row, ...) G_GNUC_NULL_TERMINATED;

GtkWidget *ui_path_box_new(const gchar *title, GtkFileChooserAction action, GtkEntry *entry);

GtkWidget *ui_button_new_with_image(const gchar *stock_id, const gchar *text);

void ui_add_document_sensitive(GtkWidget *widget);

GtkWidget *ui_image_menu_item_new(const gchar *stock_id, const gchar *label);

GtkWidget *ui_lookup_widget(GtkWidget *widget, const gchar *widget_name);

void ui_progress_bar_start(const gchar *text);

void ui_progress_bar_stop(void);

void ui_entry_add_clear_icon(GtkEntry *entry);

void ui_menu_add_document_items(GtkMenu *menu, GeanyDocument *active, GCallback callback);

void ui_menu_add_document_items_sorted(GtkMenu *menu, GeanyDocument *active,
		GCallback callback, GCompareFunc sort_func);

void ui_widget_modify_font_from_string(GtkWidget *wid, const gchar *str);

gboolean ui_is_keyval_enter_or_return(guint keyval);

gint ui_get_gtk_settings_integer(const gchar *property_name, gint default_value);

void ui_combo_box_add_to_history(GtkComboBoxText *combo_entry,
		const gchar *text, gint history_len);

const gchar *ui_lookup_stock_label(const gchar *stock_id);

void ui_tree_view_set_tooltip_text_column(GtkTreeView *tree_view, gint column);


#ifndef GEANY_DISABLE_DEPRECATED
GtkWidget *ui_frame_new_with_alignment(const gchar *label_text, GtkWidget **alignment) GEANY_DEPRECATED;

void ui_widget_set_tooltip_text(GtkWidget *widget, const gchar *text) GEANY_DEPRECATED_FOR(gtk_widget_set_tooltip_text);
#endif	/* GEANY_DISABLE_DEPRECATED */


#ifdef GEANY_PRIVATE

extern GeanyInterfacePrefs interface_prefs;

extern GeanyMainWidgets main_widgets;


/* User Interface settings not shown in the Prefs dialog. */
typedef struct UIPrefs
{
	/* State of the main window when Geany was closed */
	gint		geometry[5];	/* 0:x, 1:y, 2:width, 3:height, flag for maximized state */
	gboolean	fullscreen;
	gboolean	sidebar_visible;
	gint		sidebar_page;
	gboolean	msgwindow_visible;
	gboolean	allow_always_save; /* if set, files can always be saved, even if unchanged */
	gchar		*statusbar_template;
	gboolean	new_document_after_close;

	/* Menu-item related data */
	GQueue		*recent_queue;
	GQueue		*recent_projects_queue;
	gchar		*custom_date_format;
	gchar		**custom_commands;
	gchar		**custom_commands_labels;
}
UIPrefs;

extern UIPrefs ui_prefs;


/* Less commonly used widgets */
typedef struct UIWidgets
{
	/* menu widgets */
	GtkWidget	*toolbar_menu;
	GtkWidget	*recent_files_menuitem;
	GtkWidget	*recent_files_menu_menubar;
	GtkWidget	*print_page_setup;
	GtkWidget	*recent_projects_menuitem;
	GtkWidget	*recent_projects_menu_menubar;
	GtkWidget	*config_files_filetype_menu;

	/* dialogs */
	GtkWidget	*open_colorsel;
	GtkWidget	*open_fontsel;
	GtkWidget	*prefs_dialog;

	/* other widgets not needed in GeanyMainWidgets */
	GtkWidget	*statusbar;			/* use ui_set_statusbar() to set */
}
UIWidgets;

extern UIWidgets ui_widgets;


/* The following block of types & functions are more generic and closely related to
 * certain GTK+ widgets. */

typedef struct GeanyAutoSeparator
{
	GtkWidget	*widget;	/* e.g. GtkSeparatorToolItem, GtkSeparatorMenuItem */
	gint		show_count;	/* visible items, set to zero initially */
	gint		item_count;	/* total items, set to zero initially */
}
GeanyAutoSeparator;


typedef enum
{
	GEANY_EDITOR_SHOW_MARKERS_MARGIN,
	GEANY_EDITOR_SHOW_LINE_NUMBERS,
	GEANY_EDITOR_SHOW_WHITE_SPACE,
	GEANY_EDITOR_SHOW_INDENTATION_GUIDES,
	GEANY_EDITOR_SHOW_LINE_ENDINGS
}
GeanyUIEditorFeatures;


void ui_widget_show_hide(GtkWidget *widget, gboolean show);

gchar *ui_menu_item_get_text(GtkMenuItem *menu_item);

void ui_dialog_set_primary_button_order(GtkDialog *dialog, gint response, ...);

void ui_hbutton_box_copy_layout(GtkButtonBox *master, GtkButtonBox *copy);

void ui_combo_box_prepend_text_once(GtkComboBoxText *combo, const gchar *text);

void ui_setup_open_button_callback(GtkWidget *open_btn, const gchar *title,
		GtkFileChooserAction action, GtkEntry *entry);

void ui_auto_separator_add_ref(GeanyAutoSeparator *autosep, GtkWidget *item);

gpointer ui_builder_get_object (const gchar *name);

/* Compatibility functions */
GtkWidget *create_edit_menu1(void);
GtkWidget *create_prefs_dialog(void);
GtkWidget *create_project_dialog(void);
GtkWidget *create_toolbar_popup_menu1(void);
GtkWidget *create_window1(void);

void ui_widget_set_sensitive(GtkWidget *widget, gboolean set);

void ui_entry_add_activate_backward_signal(GtkEntry *entry);

void ui_editable_insert_text_callback(GtkEditable *editable, gchar *new_text,
									  gint new_text_len, gint *position, gpointer data);

GtkWidget *ui_label_new_bold(const gchar *text);

void ui_label_set_markup(GtkLabel *label, const gchar *format, ...) G_GNUC_PRINTF(2, 3);

/* End of general widget functions */

void ui_init_builder(void);

void ui_init(void);

void ui_init_prefs(void);

void ui_finalize_builder(void);

void ui_init_toolbar_widgets(void);

void ui_init_stock_items(void);

void ui_add_config_file_menu_item(const gchar *real_path, const gchar *label,
		GtkContainer *parent);

void ui_update_statusbar(GeanyDocument *doc, gint pos);


/* This sets the window title according to the current filename. */
void ui_set_window_title(GeanyDocument *doc);

void ui_set_editor_font(const gchar *font_name);

void ui_set_fullscreen(void);


void ui_update_popup_reundo_items(GeanyDocument *doc);

void ui_update_popup_copy_items(GeanyDocument *doc);

void ui_update_popup_goto_items(gboolean enable);


void ui_menu_copy_items_set_sensitive(gboolean sensitive);

void ui_update_menu_copy_items(GeanyDocument *doc);

void ui_update_insert_include_item(GeanyDocument *doc, gint item);

void ui_update_fold_items(void);


void ui_create_insert_menu_items(void);

void ui_create_insert_date_menu_items(void);


void ui_save_buttons_toggle(gboolean enable);

void ui_document_buttons_update(void);


void ui_sidebar_show_hide(void);

void ui_document_show_hide(GeanyDocument *doc);

void ui_set_search_entry_background(GtkWidget *widget, gboolean success);


void ui_create_recent_menus(void);

void ui_add_recent_document(GeanyDocument *doc);

void ui_add_recent_project_file(const gchar *utf8_filename);

void ui_update_recent_project_menu(void);


void ui_update_tab_status(GeanyDocument *doc);


typedef gboolean TVMatchCallback(gboolean);

gboolean ui_tree_view_find_next(GtkTreeView *treeview, TVMatchCallback cb);

gboolean ui_tree_view_find_previous(GtkTreeView *treeview, TVMatchCallback cb);

gboolean ui_tree_model_iter_any_next(GtkTreeModel *model, GtkTreeIter *iter, gboolean down);

void ui_statusbar_showhide(gboolean state);

void ui_toggle_editor_features(GeanyUIEditorFeatures feature);

void ui_update_view_editor_menu_items(void);

void ui_swap_sidebar_pos(void);

GIcon *ui_get_mime_icon(const gchar *mime_type);

void ui_focus_current_document(void);

GtkWidget *ui_create_encodings_combo_box(gboolean has_detect, gint default_enc);

gint ui_encodings_combo_box_get_active_encoding(GtkComboBox *combo);

gboolean ui_encodings_combo_box_set_active_encoding(GtkComboBox *combo, gint enc);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_UI_UTILS_H */
