/*
 *      vte.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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
 * in stdlib.h, on FreeBSD in unistd.h */
#include <stdlib.h>
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
	gint scrollback_lines;
	gchar *emulation;
	gchar *shell;
	gchar *font;
	GdkColor *colour_fore;
	GdkColor *colour_back;
} VteConfig;
VteConfig *vc;


void vte_init(void);

void vte_close(void);

void vte_apply_user_settings(void);

void vte_send_cmd(const gchar *cmd);

const gchar* vte_get_working_directory(void);

void vte_cwd(const gchar *filename);

void vte_append_preferences_tab();

/*
void vte_drag_data_received(GtkWidget *widget, GdkDragContext  *drag_context, gint x, gint y,
							GtkSelectionData *data, guint info, guint time);

gboolean vte_drag_drop(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time,
					   gpointer user_data);
*/

/* taken from original vte.h to make my life easier ;-) */

typedef struct _VteTerminalPrivate VteTerminalPrivate;

typedef struct _VteTerminal VteTerminal;
struct _VteTerminal
{
	GtkWidget widget;
	GtkAdjustment *adjustment;
	glong char_width, char_height;
	glong char_ascent, char_descent;
	glong row_count, column_count;
	gchar *window_title;
	gchar *icon_title;
	VteTerminalPrivate *pvt;
};


/* store function pointers in a struct to avoid a strange segfault if they are stored directly
 * if accessed directly, gdb says the segfault arrives at old_tab_width(prefs.c), don't ask me */
struct VteFunctions
{
	GtkWidget* (*vte_terminal_new) (void);
	pid_t (*vte_terminal_fork_command) (VteTerminal *terminal, const char *command, char **argv,
										char **envv, const char *directory, gboolean lastlog,
										gboolean utmp, gboolean wtmp);
	void (*vte_terminal_set_size) (VteTerminal *terminal, glong columns, glong rows);
	void (*vte_terminal_set_word_chars) (VteTerminal *terminal, const char *spec);
	void (*vte_terminal_set_mouse_autohide) (VteTerminal *terminal, gboolean setting);
	void (*vte_terminal_reset) (VteTerminal *terminal, gboolean full, gboolean clear_history);
	void (*vte_terminal_set_encoding) (VteTerminal *terminal, const char *codeset);
	void (*vte_terminal_set_cursor_blinks) (VteTerminal *terminal, gboolean blink);
	GtkType (*vte_terminal_get_type) (void);
	void (*vte_terminal_set_scroll_on_output) (VteTerminal *terminal, gboolean scroll);
	void (*vte_terminal_set_scroll_on_keystroke) (VteTerminal *terminal, gboolean scroll);
	void (*vte_terminal_set_font_from_string) (VteTerminal *terminal, const char *name);
	void (*vte_terminal_set_scrollback_lines) (VteTerminal *terminal, glong lines);
	gboolean (*vte_terminal_get_has_selection) (VteTerminal *terminal);
	void (*vte_terminal_copy_clipboard) (VteTerminal *terminal);
	void (*vte_terminal_paste_clipboard) (VteTerminal *terminal);
	void (*vte_terminal_set_emulation) (VteTerminal *terminal, const gchar *emulation);
	void (*vte_terminal_set_color_foreground) (VteTerminal *terminal, const GdkColor *foreground);
	void (*vte_terminal_set_color_background) (VteTerminal *terminal, const GdkColor *background);
	void (*vte_terminal_feed_child) (VteTerminal *terminal, const char *data, glong length);
	void (*vte_terminal_im_append_menuitems) (VteTerminal *terminal, GtkMenuShell *menushell);
};

#endif

#endif
