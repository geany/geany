/*
 *      gtkcompat.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2012 Colomban Wendling <ban(at)herbesfolles(dot)org>
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

/* Compatibility macros to support older GTK+ versions */

#ifndef GTK_COMPAT_H
#define GTK_COMPAT_H 1

#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(3, 0, 0)
#	include <gdk/gdkkeysyms-compat.h>
#endif

G_BEGIN_DECLS


/* GtkComboBoxText */
/* This is actually available in GTK 2.24, but we expose GtkComboBoxText in the
 * API so we don't want the type to change for no good reason (although this
 * should probably be harmless since it's only a derivated type).  However, since
 * a plugin needs to be rebuilt and tuned to work with GTK3 we don't mind that
 * a type changes between the GTK2 and GTK3 version */
#if ! GTK_CHECK_VERSION(3, 0, 0)
/* undef those not to get warnings about redefinitions under GTK 2.24 */
#	undef GTK_COMBO_BOX_TEXT
#	undef GTK_COMBO_BOX_TEXT_CLASS
#	undef GTK_COMBO_BOX_TEXT_GET_CLASS
#	undef GTK_IS_COMBO_BOX_TEXT
#	undef GTK_IS_COMBO_BOX_TEXT_CLASS
#	undef GTK_TYPE_COMBO_BOX_TEXT

#	define GTK_COMBO_BOX_TEXT					GTK_COMBO_BOX
#	define GTK_COMBO_BOX_TEXT_CLASS				GTK_COMBO_BOX_CLASS
#	define GTK_COMBO_BOX_TEXT_GET_CLASS			GTK_COMBO_BOX_GET_CLASS
#	define GTK_IS_COMBO_BOX_TEXT				GTK_IS_COMBO_BOX
#	define GTK_IS_COMBO_BOX_TEXT_CLASS			GTK_IS_COMBO_BOX_CLASS
#	define GTK_TYPE_COMBO_BOX_TEXT				GTK_TYPE_COMBO_BOX
#	define GtkComboBoxText						GtkComboBox
#	define gtk_combo_box_text_new				gtk_combo_box_new_text
#	define gtk_combo_box_text_new_with_entry	gtk_combo_box_entry_new_text
#	define gtk_combo_box_text_append_text		gtk_combo_box_append_text
#	define gtk_combo_box_text_insert_text		gtk_combo_box_insert_text
#	define gtk_combo_box_text_prepend_text		gtk_combo_box_prepend_text
#	define gtk_combo_box_text_remove			gtk_combo_box_remove_text
#	define gtk_combo_box_text_get_active_text	gtk_combo_box_get_active_text
#endif

/* GtkWidget */
#if ! GTK_CHECK_VERSION(3, 0, 0)
#	define gtk_widget_get_allocated_height(widget)	(((GtkWidget *) (widget))->allocation.height)
#	define gtk_widget_get_allocated_width(widget)	(((GtkWidget *) (widget))->allocation.width)
#endif


/* Mappings below only prevent some deprecation warnings on GTK3 for things
 * that didn't exist on GTK2.  That's not future-proof. */
#if GTK_CHECK_VERSION(3, 0, 0)
/* Gtk[VH]Box */
#	define compat_gtk_box_new(orientation, homogeneous, spacing) \
		((GtkWidget *)g_object_new(GTK_TYPE_BOX, \
					 "orientation", (orientation), \
					 "homogeneous", (homogeneous), \
					 "spacing", (spacing), \
					 NULL))
#	define gtk_vbox_new(homogeneous, spacing) \
		compat_gtk_box_new(GTK_ORIENTATION_VERTICAL, (homogeneous), (spacing))
#	define gtk_hbox_new(homogeneous, spacing) \
		compat_gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (homogeneous), (spacing))
/* Gtk[VH]ButtonBox */
#	define gtk_vbutton_box_new()	gtk_button_box_new(GTK_ORIENTATION_VERTICAL)
#	define gtk_hbutton_box_new()	gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)
/* Gtk[VH]Separator */
#	define gtk_vseparator_new()	gtk_separator_new(GTK_ORIENTATION_VERTICAL)
#	define gtk_hseparator_new()	gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)
/* Gtk[VH]Paned */
#	define gtk_vpaned_new()	gtk_paned_new(GTK_ORIENTATION_VERTICAL)
#	define gtk_hpaned_new()	gtk_paned_new(GTK_ORIENTATION_HORIZONTAL)
/* Gtk[VH]Scrollbar */
#	define gtk_vscrollbar_new(adj)	gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, (adj))
#	define gtk_hscrollbar_new(adj)	gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, (adj))
#endif


G_END_DECLS

#endif /* GTK_COMPAT_H */
