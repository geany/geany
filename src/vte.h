/*
 *      vte.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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

#include <stdlib.h>


void vte_init(void);

gboolean vte_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data);

void vte_start(GtkWidget *widget, gpointer data);

void vte_register_symbols(GModule *module);

void vte_close(void);



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
	char *window_title;
	char *icon_title;
	VteTerminalPrivate *pvt;
};



/* store function pointers in a struct to avoid a strange segfault if they are stored directly
 * if accessed directly, gdb says the segfault arrives at old_tab_width(prefs.c), don't ask me */
struct vte_funcs
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
};


#endif
