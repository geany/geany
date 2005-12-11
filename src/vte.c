/*
 *      vte.c - this file is part of Geany, a fast and lightweight IDE
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

#include "geany.h"

#ifdef HAVE_VTE

#include <gdk/gdkkeysyms.h>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "vte.h"
#include "msgwindow.h"
#include "support.h"
#include "utils.h"


extern gchar **environ;
static pid_t pid;
static GModule *module = NULL;
static struct vte_funcs *vf;


#define VTE_TERMINAL(obj) (GTK_CHECK_CAST((obj), VTE_TYPE_TERMINAL, VteTerminal))
#define VTE_TYPE_TERMINAL (vf->vte_terminal_get_type())


/* taken from anjuta, thanks */
gchar **vte_get_child_environment(GtkWidget *term)
{
	/* code from gnome-terminal, sort of. */
	gchar **p;
	gint i;
	gchar **retval;
#define EXTRA_ENV_VARS 5

	/* count env vars that are set */
	for (p = environ; *p; p++);

	i = p - environ;
	retval = g_new(gchar *, i + 1 + EXTRA_ENV_VARS);

	for (i = 0, p = environ; *p; p++)
	{
		/* Strip all these out, we'll replace some of them */
		if ((strncmp(*p, "COLUMNS=", 8) == 0) ||
		    (strncmp(*p, "LINES=", 6) == 0)   ||
		    (strncmp(*p, "TERM=", 5) == 0))
		{
			/* nothing: do not copy */
		}
		else
		{
			retval[i] = g_strdup(*p);
			++i;
		}
	}

	retval[i] = g_strdup ("TERM=xterm");
	++i;

	retval[i] = NULL;

	return retval;
}


void vte_init(void)
{

	GtkWidget *vte, *scrollbar, *hbox, *frame;
	module = g_module_open("libvte.so.4", G_MODULE_BIND_LAZY);

	if (module == NULL || app->have_vte == FALSE)
	{
		app->have_vte = FALSE;
		geany_debug("Could(or should) not load libvte.so.4, terminal support disabled");
		return;
	}
	else
	{
		app->have_vte = TRUE;
		vf = g_new(struct vte_funcs, 1);
		vc = g_new(struct vte_conf, 1);
		vte_register_symbols(module);
	}

	vte = vf->vte_terminal_new();
	vc->vte = vte;
	scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(VTE_TERMINAL(vte)->adjustment));
	GTK_WIDGET_UNSET_FLAGS(scrollbar, GTK_CAN_FOCUS);

	frame = gtk_frame_new(NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, TRUE, 0);

	vte_get_settings();

	vf->vte_terminal_set_size(VTE_TERMINAL(vte), 50, 1);
	vf->vte_terminal_set_encoding(VTE_TERMINAL(vte), "UTF-8");
	vf->vte_terminal_set_mouse_autohide(VTE_TERMINAL(vte), TRUE);
	vf->vte_terminal_set_word_chars(VTE_TERMINAL(vte), GEANY_WORDCHARS);
	vte_apply_user_settings();

	g_signal_connect(G_OBJECT(vte), "child-exited", G_CALLBACK(vte_start), NULL);
	g_signal_connect(G_OBJECT(vte), "button-press-event", G_CALLBACK(vte_button_pressed), NULL);
	g_signal_connect(G_OBJECT(vte), "event", G_CALLBACK(vte_keypress), NULL);

	vte_start(vte, NULL);

	gtk_widget_show_all(frame);
	gtk_notebook_insert_page(GTK_NOTEBOOK(msgwindow.notebook), frame, gtk_label_new(_("Terminal")), MSG_VTE);
}


void vte_close(void)
{
	g_module_close(module);
	g_free(vf);
	g_free(vc->font);
	g_free(vc->emulation);
	g_free(vc->color_back);
	g_free(vc->color_fore);
	g_free(vc);
}


gboolean vte_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	/// FIXME: GDK_KEY_PRESS doesn't seem to be called for our keys
	if (event->type != GDK_KEY_RELEASE)
		return FALSE;

	if (event->keyval == GDK_c ||
		event->keyval == GDK_d ||
		event->keyval == GDK_C ||
		event->keyval == GDK_D)
	{
		if (event->state & GDK_CONTROL_MASK)
		{
			kill(pid, SIGINT);
			pid = 0;
			vte_start(widget, NULL);
			return TRUE;
		}
	}
	return FALSE;
}


void vte_start(GtkWidget *widget, gpointer data)
{
	VteTerminal *vte = VTE_TERMINAL(widget);
	struct passwd *pw;
	const gchar *shell;
	const gchar *dir;
	gchar **env;

	pw = getpwuid(getuid());
	if (pw)
	{
		shell = pw->pw_shell;
		dir = pw->pw_dir;
	}
	else
	{
		shell = "/bin/sh";
		dir = "/";
	}

	env = vte_get_child_environment(app->window);
	pid = vf->vte_terminal_fork_command(VTE_TERMINAL(vte), shell, NULL, env, dir, TRUE, TRUE, TRUE);
	g_strfreev(env);
}


gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button == 2)
	{
		/* this behaviour(pasting at mouse button 2) seems to be default, but don't know
		 * if it is always the same, so I implement it by myself to be sure and
		 * return TRUE to prevent the default behaviour */
		vf->vte_terminal_paste_clipboard(VTE_TERMINAL(widget));
		return TRUE;
	}
	else if (event->button == 3)
	{
		if (vf->vte_terminal_get_has_selection(VTE_TERMINAL(widget)))
		{
			vf->vte_terminal_copy_clipboard(VTE_TERMINAL(widget));
			return TRUE;
		}
	}

	return FALSE;
}


void vte_register_symbols(GModule *mod)
{
	g_module_symbol(mod, "vte_terminal_new", (void*)&vf->vte_terminal_new);
	g_module_symbol(mod, "vte_terminal_set_size", (void*)&vf->vte_terminal_set_size);
	g_module_symbol(mod, "vte_terminal_fork_command", (void*)&vf->vte_terminal_fork_command);
	g_module_symbol(mod, "vte_terminal_set_word_chars", (void*)&vf->vte_terminal_set_word_chars);
	g_module_symbol(mod, "vte_terminal_set_mouse_autohide", (void*)&vf->vte_terminal_set_mouse_autohide);
	g_module_symbol(mod, "vte_terminal_set_encoding", (void*)&vf->vte_terminal_set_encoding);
	g_module_symbol(mod, "vte_terminal_reset", (void*)&vf->vte_terminal_reset);
	g_module_symbol(mod, "vte_terminal_set_cursor_blinks", (void*)&vf->vte_terminal_set_cursor_blinks);
	g_module_symbol(mod, "vte_terminal_get_type", (void*)&vf->vte_terminal_get_type);
	g_module_symbol(mod, "vte_terminal_set_scroll_on_output", (void*)&vf->vte_terminal_set_scroll_on_output);
	g_module_symbol(mod, "vte_terminal_set_scroll_on_keystroke", (void*)&vf->vte_terminal_set_scroll_on_keystroke);
	g_module_symbol(mod, "vte_terminal_set_font_from_string", (void*)&vf->vte_terminal_set_font_from_string);
	g_module_symbol(mod, "vte_terminal_set_scrollback_lines", (void*)&vf->vte_terminal_set_scrollback_lines);
	g_module_symbol(mod, "vte_terminal_get_has_selection", (void*)&vf->vte_terminal_get_has_selection);
	g_module_symbol(mod, "vte_terminal_copy_clipboard", (void*)&vf->vte_terminal_copy_clipboard);
	g_module_symbol(mod, "vte_terminal_paste_clipboard", (void*)&vf->vte_terminal_paste_clipboard);
	g_module_symbol(mod, "vte_terminal_set_emulation", (void*)&vf->vte_terminal_set_emulation);
	g_module_symbol(mod, "vte_terminal_set_color_foreground", (void*)&vf->vte_terminal_set_color_foreground);
	g_module_symbol(mod, "vte_terminal_set_color_background", (void*)&vf->vte_terminal_set_color_background);
}


void vte_apply_user_settings(void)
{
	vf->vte_terminal_set_scrollback_lines(VTE_TERMINAL(vc->vte), vc->scrollback_lines);
	vf->vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(vc->vte), vc->scroll_on_key);
	vf->vte_terminal_set_scroll_on_output(VTE_TERMINAL(vc->vte), vc->scroll_on_out);
	vf->vte_terminal_set_emulation(VTE_TERMINAL(vc->vte), vc->emulation);
	vf->vte_terminal_set_font_from_string(VTE_TERMINAL(vc->vte), vc->font);

	vf->vte_terminal_reset(VTE_TERMINAL(vc->vte), TRUE, FALSE);
}


void vte_get_settings(void)
{
	gchar **values = g_strsplit(app->terminal_settings, ";", 7);

	if (g_strv_length(values) != 7) return;
	vc->font = g_strdup(values[0]);
	//vc->color_fore = utils_get_color_from_bint(strtod(values[1], NULL));
	//vc->color_back = utils_get_color_from_bint(strtod(values[2], NULL));
	vc->color_fore = g_new(GdkColor, 1);
	vc->color_back = g_new(GdkColor, 1);
	gdk_color_parse(values[1], vc->color_fore);
	gdk_color_parse(values[2], vc->color_back);

	vc->scrollback_lines = strtod(values[3], NULL);
	if ((vc->scrollback_lines < 0) || (vc->scrollback_lines > 5000)) vc->scrollback_lines = 500;

	vc->emulation = g_strdup(values[4]);

	vc->scroll_on_key = utils_atob(values[5]);
	vc->scroll_on_out = utils_atob(values[6]);

	g_strfreev(values);
}


#endif
