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
		geany_debug("Could not load libvte.so.4, terminal support disabled");
		return;
	}
	else
	{
		app->have_vte = TRUE;
		vf = g_new(struct vte_funcs, 1);
		vte_register_symbols(module);
	}

	vte = vf->vte_terminal_new();
	scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(VTE_TERMINAL(vte)->adjustment));
	GTK_WIDGET_UNSET_FLAGS(scrollbar, GTK_CAN_FOCUS);

	frame = gtk_frame_new(NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, TRUE, 0);

	vf->vte_terminal_set_mouse_autohide(VTE_TERMINAL(vte), TRUE);
	vf->vte_terminal_set_scrollback_lines(VTE_TERMINAL(vte), 20);
	vf->vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(vte), TRUE);
	vf->vte_terminal_set_scroll_on_output(VTE_TERMINAL(vte), TRUE);
	vf->vte_terminal_set_word_chars(VTE_TERMINAL(vte), GEANY_WORDCHARS);
	vf->vte_terminal_set_size(VTE_TERMINAL(vte), 50, 1);
	vf->vte_terminal_set_encoding(VTE_TERMINAL(vte), "UTF-8");
	vf->vte_terminal_set_font_from_string(VTE_TERMINAL(vte), "Monospace 10");

	g_signal_connect(G_OBJECT(vte), "child-exited", G_CALLBACK(vte_start), NULL);
	g_signal_connect(G_OBJECT(vte), "event", G_CALLBACK(vte_keypress), NULL);

	vte_start(vte, NULL);

	gtk_widget_show_all(frame);
	gtk_notebook_insert_page(GTK_NOTEBOOK(msgwindow.notebook), frame, gtk_label_new(_("Terminal")), MSG_VTE);
}


void vte_close(void)
{
	g_module_close(module);
	g_free(vf);
}


gboolean vte_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	/* Fixme: GDK_KEY_PRESS doesn't seem to be called for our keys */
	if (event->type != GDK_KEY_RELEASE)
		return FALSE;

	/* ctrl-c or ctrl-d */
	if (event->keyval == GDK_c ||
		event->keyval == GDK_d ||
		event->keyval == GDK_C ||
		event->keyval == GDK_D)
	{
		/* Ctrl pressed */
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
}

#endif
