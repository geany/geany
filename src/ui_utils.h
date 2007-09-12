/*
 *      ui_utils.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 *
 * $Id$
 */

#ifndef GEANY_UI_UTILS_H
#define GEANY_UI_UTILS_H 1

/* User Interface settings not shown in the Prefs dialog. */
typedef struct UIPrefs
{
	/* State of the main window when Geany was closed */
	gint		geometry[5];	// 0:x, 1:y, 2:width, 3:height, flag for maximized state
	gboolean	fullscreen;
	gboolean	sidebar_visible;
	gboolean	msgwindow_visible;

	/* Menu-item related data */
	GQueue		*recent_queue;
	gchar		*custom_date_format;
	gchar		**custom_commands;
}
UIPrefs;

extern UIPrefs ui_prefs;


/* Less commonly used widgets */
typedef struct UIWidgets
{
	/* menu widgets */
	GtkWidget	*toolbar_menu;
	GtkWidget	*new_file_menu;
	GtkWidget	*recent_files_menuitem;
	GtkWidget	*recent_files_menubar;
	GtkWidget	*recent_files_toolbar;
	GtkWidget	*menu_insert_include_items[2];
	GtkWidget	*popup_goto_items[3];
	GtkWidget	*popup_copy_items[3];
	GtkWidget	*menu_copy_items[3];
	GtkWidget	*redo_items[3];
	GtkWidget	*undo_items[3];
	GtkWidget	*save_buttons[4];

	/* dialogs */
	GtkWidget	*open_colorsel;
	GtkWidget	*open_fontsel;
	GtkWidget	*open_filesel;
	GtkWidget	*save_filesel;
	GtkWidget	*prefs_dialog;
}
UIWidgets;

extern UIWidgets ui_widgets;


/* The following block of functions are more generic functions and closely related to
 * certain GTK+ widgets. */

void ui_widget_show_hide(GtkWidget *widget, gboolean show);

void ui_widget_modify_font_from_string(GtkWidget *wid, const gchar *str);

GtkWidget *ui_frame_new_with_alignment(const gchar *label_text, GtkWidget **alignment);

GtkWidget *ui_dialog_vbox_new(GtkDialog *dialog);

GtkWidget *ui_button_new_with_image(const gchar *stock_id, const gchar *text);

void ui_hbutton_box_copy_layout(GtkButtonBox *master, GtkButtonBox *copy);

void ui_combo_box_add_to_history(GtkComboBox *combo, const gchar *text);

GtkWidget *ui_path_box_new(const gchar *title, GtkFileChooserAction action, GtkEntry *entry);

void ui_setup_open_button_callback(GtkWidget *open_btn, const gchar *title,
		GtkFileChooserAction action, GtkEntry *entry);

void ui_table_add_row(GtkTable *table, gint row, ...) G_GNUC_NULL_TERMINATED;

/* End of 'generic' functions */


void ui_init();


// Display text on the statusbar without logging it to the Status window.
void ui_set_statusbar(const gchar *format, ...) G_GNUC_PRINTF (1, 2);

void ui_update_statusbar(gint idx, gint pos);


/* This sets the window title according to the current filename. */
void ui_set_window_title(gint index);

void ui_set_editor_font(const gchar *font_name);

void ui_set_fullscreen();


void ui_update_popup_reundo_items(gint idx);

void ui_update_popup_copy_items(gint idx);

void ui_update_popup_goto_items(gboolean enable);


void ui_update_menu_copy_items(gint idx);

void ui_update_insert_include_item(gint idx, gint item);

void ui_update_fold_items();


void ui_create_insert_menu_items();

void ui_create_insert_date_menu_items();


void ui_save_buttons_toggle(gboolean enable);

void ui_document_buttons_update();


void ui_treeviews_show_hide(gboolean force);

void ui_document_show_hide(gint idx);


void ui_update_toolbar_icons(GtkIconSize size);

void ui_update_toolbar_items();


GdkPixbuf *ui_new_pixbuf_from_inline(gint img, gboolean small_img);

GtkWidget *ui_new_image_from_inline(gint img, gboolean small_img);


void ui_create_recent_menu();

void ui_add_recent_file(const gchar *utf8_filename);


void ui_show_markers_margin();

void ui_show_linenumber_margin();


void ui_update_tab_status(gint idx);


typedef gboolean TVMatchCallback();

gboolean ui_tree_view_find_next(GtkTreeView *treeview, TVMatchCallback cb);


void ui_statusbar_showhide(gboolean state);

#endif
