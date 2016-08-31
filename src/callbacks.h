/*
 *      callbacks.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GEANY_CALLBACKS_H
#define GEANY_CALLBACKS_H 1

#include "gtkcompat.h"

G_BEGIN_DECLS

/* Defined in auto-generated code in signalconn.c */
void callbacks_connect(GtkBuilder *builder);

extern gboolean	ignore_callback;

void on_new1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_save1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_save_as1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_quit1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_open1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_save_all1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_close1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_close_all1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_replace_tabs_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_toolbutton_search_clicked(GtkAction *action, gpointer user_data);

gboolean toolbar_popup_menu(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

void on_undo1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_redo1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_cut1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_copy1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_paste1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_delete1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_zoom_in1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_zoom_out1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_toolbar_search_entry_changed(GtkAction *action, const gchar *text, gpointer user_data);

void on_toolbar_search_entry_activate(GtkAction *action, const gchar *text, gpointer user_data);

void on_toggle_case1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find_usage1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_preferences1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_normal_size1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_show_color_chooser1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find_next1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find_previous1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find_nextsel1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find_prevsel1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_replace1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find_in_files1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_toolbutton_reload_clicked(GtkAction *action, gpointer user_data);

void on_go_to_line_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_help1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_toolbutton_compile_clicked(GtkAction *action, gpointer user_data);

void on_line_wrapping1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data);

void on_toolbutton_goto_entry_activate(GtkAction *action, const gchar *text, gpointer user_data);

void on_toolbutton_goto_clicked(GtkAction *action, gpointer user_data);

void on_toolbutton_run_clicked(GtkAction *action, gpointer user_data);

void on_menu_remove_indicators1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_print1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_file_properties_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_select_all1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_show_sidebar1_toggled(GtkCheckMenuItem *checkmenuitem, gpointer user_data);

void on_menu_comment_line1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_uncomment_line1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_increase_indent1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_decrease_indent1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_toggle_line_commentation1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_next_message1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_project_new1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_project_open1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_project_close1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_project_properties1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_open_selected_file1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_remove_markers1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_context_action1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_toggle_all_additional_widgets1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_toolbutton_back_activate(GtkAction *action, gpointer user_data);

void on_toolbutton_forward_activate(GtkAction *action, gpointer user_data);

gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data);

gboolean on_escape_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

void on_line_breaking1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_replace_spaces_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_previous_message1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_close_other_documents1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_find_document_usage1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_send_selection_to_vte1_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_plugin_preferences1_activate(GtkMenuItem *menuitem, gpointer user_data);

G_END_DECLS

#endif /* GEANY_CALLBACKS_H */
