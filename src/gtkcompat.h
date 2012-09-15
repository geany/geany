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
#define GTK_COMPAT_H

#include <gtk/gtk.h>
#if GTK_CHECK_VERSION(3, 0, 0)
#	include <gdk/gdkkeysyms-compat.h>
#endif

G_BEGIN_DECLS


/* GtkWidget */
#if ! GTK_CHECK_VERSION(2, 18, 0)
#	define compat_widget_set_flag(widget, flag, enable) \
		((enable) ? GTK_WIDGET_SET_FLAGS((widget), (flag)) : GTK_WIDGET_UNSET_FLAGS((widget), (flag)))
#	define gtk_widget_set_can_default(widget, can_default) \
		compat_widget_set_flag((widget), GTK_CAN_DEFAULT, (can_default))
#	define gtk_widget_is_toplevel(widget)		GTK_WIDGET_TOPLEVEL(widget)
#	define gtk_widget_is_sensitive(widget)		GTK_WIDGET_IS_SENSITIVE(widget)
#	define gtk_widget_has_focus(widget)			GTK_WIDGET_HAS_FOCUS(widget)
#	define gtk_widget_get_sensitive(widget)		GTK_WIDGET_SENSITIVE(widget)
#	define gtk_widget_set_has_window(widget, has_window) \
		compat_widget_set_flag((widget), GTK_NO_WINDOW, !(has_window))
#	define gtk_widget_set_can_focus(widget, can_focus) \
		compat_widget_set_flag((widget), GTK_CAN_FOCUS, (can_focus))
#endif
#if ! GTK_CHECK_VERSION(2, 20, 0)
#	define gtk_widget_get_mapped(widget)	GTK_WIDGET_MAPPED(widget)
#endif
#if ! GTK_CHECK_VERSION(3, 0, 0)
#	define gtk_widget_get_allocated_height(widget)	(((GtkWidget *) (widget))->allocation.height)
#	define gtk_widget_get_allocated_width(widget)	(((GtkWidget *) (widget))->allocation.width)
#endif


G_END_DECLS

#endif /* GTK_COMPAT_H */
