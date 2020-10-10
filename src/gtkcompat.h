/*
 *      gtkcompat.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2012 The Geany contributors
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
#include <gdk/gdkkeysyms-compat.h>

G_BEGIN_DECLS


/* Mappings below only prevent some deprecation warnings on GTK3 for things
 * that didn't exist on GTK2.  That's not future-proof. */
/* Gtk[VH]Box */
#define compat_gtk_box_new(orientation, homogeneous, spacing) \
	((GtkWidget *)g_object_new(GTK_TYPE_BOX, \
				 "orientation", (orientation), \
				 "homogeneous", (homogeneous), \
				 "spacing", (spacing), \
				 NULL))
#define gtk_vbox_new(homogeneous, spacing) \
	compat_gtk_box_new(GTK_ORIENTATION_VERTICAL, (homogeneous), (spacing))
#define gtk_hbox_new(homogeneous, spacing) \
	compat_gtk_box_new(GTK_ORIENTATION_HORIZONTAL, (homogeneous), (spacing))
/* Gtk[VH]ButtonBox */
#define gtk_vbutton_box_new()	gtk_button_box_new(GTK_ORIENTATION_VERTICAL)
#define gtk_hbutton_box_new()	gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)
/* Gtk[VH]Separator */
#define gtk_vseparator_new()	gtk_separator_new(GTK_ORIENTATION_VERTICAL)
#define gtk_hseparator_new()	gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)
/* Gtk[VH]Paned */
#define gtk_vpaned_new()	gtk_paned_new(GTK_ORIENTATION_VERTICAL)
#define gtk_hpaned_new()	gtk_paned_new(GTK_ORIENTATION_HORIZONTAL)
/* Gtk[VH]Scrollbar */
#define gtk_vscrollbar_new(adj)	gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, (adj))
#define gtk_hscrollbar_new(adj)	gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, (adj))


G_END_DECLS

#endif /* GTK_COMPAT_H */
