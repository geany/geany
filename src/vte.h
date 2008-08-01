/*
 *      vte.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <<nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#ifndef GEANY_VTE_H
#define GEANY_VTE_H 1

#ifdef HAVE_VTE

/* include stdlib.h AND unistd.h, because on GNU/Linux pid_t seems to be
 * in stdlib.h, on FreeBSD in unistd.h, sys/types.h is needed for C89 */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


typedef struct
{
	gboolean load_vte;
	gboolean have_vte;
	gchar	*lib_vte;
	gchar	*dir;
} VteInfo;

extern VteInfo vte_info;


typedef struct
{
	GtkWidget *vte;
	GtkWidget *menu;
	GtkWidget *im_submenu;
	gboolean scroll_on_key;
	gboolean scroll_on_out;
	gboolean ignore_menu_bar_accel;
	gboolean follow_path;
	gboolean run_in_vte;
	gboolean skip_run_script;
	gboolean enable_bash_keys;
	gint scrollback_lines;
	gchar *emulation;
	gchar *shell;
	gchar *font;
	GdkColor *colour_fore;
	GdkColor *colour_back;
} VteConfig;
extern VteConfig *vc;


void vte_init(void);

void vte_close(void);

void vte_apply_user_settings(void);

gboolean vte_send_cmd(const gchar *cmd);

const gchar* vte_get_working_directory(void);

void vte_cwd(const gchar *filename, gboolean force);

void vte_append_preferences_tab(void);

/*
void vte_drag_data_received(GtkWidget *widget, GdkDragContext  *drag_context, gint x, gint y,
							GtkSelectionData *data, guint info, guint time);

gboolean vte_drag_drop(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time,
					   gpointer user_data);
*/

#endif

#endif
