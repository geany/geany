/*
 *      vte.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include "callbacks.h"


extern gchar **environ;
static pid_t pid;
static GModule *module = NULL;
static struct vte_funcs *vf;


#define VTE_TERMINAL(obj) (GTK_CHECK_CAST((obj), VTE_TYPE_TERMINAL, VteTerminal))
#define VTE_TYPE_TERMINAL (vf->vte_terminal_get_type())

static void vte_start(GtkWidget *widget);
static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean vte_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void vte_register_symbols(GModule *module);
static void vte_get_settings(void);
static void vte_popup_menu_clicked(GtkMenuItem *menuitem, gpointer user_data);
static GtkWidget *vte_create_popup_menu(void);


/* taken from anjuta, thanks */
static gchar **vte_get_child_environment(void)
{
	// code from gnome-terminal, sort of.
	gchar **p;
	gint i;
	gchar **retval;
#define EXTRA_ENV_VARS 5

	// count env vars that are set
	for (p = environ; *p; p++);

	i = p - environ;
	retval = g_new0(gchar *, i + 1 + EXTRA_ENV_VARS);

	for (i = 0, p = environ; *p; p++)
	{
		// Strip all these out, we'll replace some of them
		if ((strncmp(*p, "COLUMNS=", 8) == 0) ||
		    (strncmp(*p, "LINES=", 6) == 0)   ||
		    (strncmp(*p, "TERM=", 5) == 0))
		{
			// nothing: do not copy
		}
		else
		{
			retval[i] = g_strdup(*p);
			++i;
		}
	}

	retval[i] = g_strdup("TERM=xterm");
	++i;

	retval[i] = NULL;

	return retval;
}


void vte_init(void)
{

	GtkWidget *vte, *scrollbar, *hbox, *frame;

	if (app->have_vte == FALSE)
	{	// app->have_vte can be false, even if VTE is compiled in, think of command line option
		geany_debug("Disabling terminal support");
		return;
	}

	if (app->lib_vte && strlen(app->lib_vte))
	{
		module = g_module_open(app->lib_vte, G_MODULE_BIND_LAZY);
	}
	else
	{
		module = g_module_open("libvte.so", G_MODULE_BIND_LAZY);
		// try to fallback to libvte.so.4, if it is installed
		if (module == NULL) module = g_module_open("libvte.so.4", G_MODULE_BIND_LAZY);
	}

	if (module == NULL)
	{
		app->have_vte = FALSE;
		geany_debug("Could not load libvte.so.4, terminal support disabled");
		return;
	}
	else
	{
		app->have_vte = TRUE;
		vf = g_new0(struct vte_funcs, 1);
		vc = g_new0(struct vte_conf, 1);
		vte_register_symbols(module);
	}

	vte = vf->vte_terminal_new();
	vc->vte = vte;
	vc->menu = vte_create_popup_menu();
	scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(VTE_TERMINAL(vte)->adjustment));
	GTK_WIDGET_UNSET_FLAGS(scrollbar, GTK_CAN_FOCUS);

	frame = gtk_frame_new(NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, TRUE, 0);

	vte_get_settings();

	vf->vte_terminal_set_size(VTE_TERMINAL(vte), 30, 1);
	//vf->vte_terminal_set_encoding(VTE_TERMINAL(vte), "UTF-8");
	vf->vte_terminal_set_mouse_autohide(VTE_TERMINAL(vte), TRUE);
	vf->vte_terminal_set_word_chars(VTE_TERMINAL(vte), GEANY_WORDCHARS);

	g_signal_connect(G_OBJECT(vte), "child-exited", G_CALLBACK(vte_start), NULL);
	g_signal_connect(G_OBJECT(vte), "button-press-event", G_CALLBACK(vte_button_pressed), NULL);
	g_signal_connect(G_OBJECT(vte), "event", G_CALLBACK(vte_keypress), NULL);
	//g_signal_connect(G_OBJECT(vte), "drag-data-received", G_CALLBACK(vte_drag_data_received), NULL);
	//g_signal_connect(G_OBJECT(vte), "drag-drop", G_CALLBACK(vte_drag_drop), NULL);

	vte_start(vte);

	gtk_widget_show_all(frame);
	gtk_notebook_insert_page(GTK_NOTEBOOK(msgwindow.notebook), frame, gtk_label_new(_("Terminal")), MSG_VTE);

	// the vte widget has to be realised before color changes take effect
	//g_signal_connect(G_OBJECT(vte), "realize", G_CALLBACK(vte_apply_user_settings), NULL);

	//gtk_widget_realize(vte);
	vte_apply_user_settings();
}


void vte_close(void)
{
	g_free(vf);
	/* free the vte widget before unloading vte module
	 * this prevents a segfault on X close window if the message window is hidden
	 * (patch from Nick Treleaven, thanks) */
	gtk_widget_destroy(vc->vte);
	gtk_widget_destroy(vc->menu);
	g_free(vc->font);
	g_free(vc->emulation);
	g_free(vc->color_back);
	g_free(vc->color_fore);
	g_free(vc);
	g_module_close(module);
}


static gboolean vte_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->type != GDK_KEY_RELEASE)
		return FALSE;

	if ((event->keyval == GDK_c ||
		event->keyval == GDK_d ||
		event->keyval == GDK_C ||
		event->keyval == GDK_D) &&
		event->state & GDK_CONTROL_MASK)
	{
		kill(pid, SIGINT);
		pid = 0;
		vf->vte_terminal_reset(VTE_TERMINAL(widget), TRUE, TRUE);
		vte_start(widget);
		return TRUE;
	}

	return FALSE;
}


static void vte_start(GtkWidget *widget)
{
	VteTerminal *vte = VTE_TERMINAL(widget);
	struct passwd *pw;
	const gchar *shell;
	const gchar *dir = NULL;
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

	env = vte_get_child_environment();
	pid = vf->vte_terminal_fork_command(VTE_TERMINAL(vte), shell, NULL, env, dir, TRUE, TRUE, TRUE);
	g_strfreev(env);
}


static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_menu_popup(GTK_MENU(vc->menu), NULL, NULL, NULL, NULL, event->button, event->time);
	}

	return FALSE;
}


static void vte_register_symbols(GModule *mod)
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
	g_module_symbol(mod, "vte_terminal_feed_child", (void*)&vf->vte_terminal_feed_child);
}


void vte_apply_user_settings(void)
{
	if (! app->msgwindow_visible) return;
	//if (! GTK_WIDGET_REALIZED(vc->vte)) gtk_widget_realize(vc->vte);
	vf->vte_terminal_set_scrollback_lines(VTE_TERMINAL(vc->vte), vc->scrollback_lines);
	vf->vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(vc->vte), vc->scroll_on_key);
	vf->vte_terminal_set_scroll_on_output(VTE_TERMINAL(vc->vte), vc->scroll_on_out);
	vf->vte_terminal_set_emulation(VTE_TERMINAL(vc->vte), vc->emulation);
	vf->vte_terminal_set_font_from_string(VTE_TERMINAL(vc->vte), vc->font);
	vf->vte_terminal_set_color_foreground(VTE_TERMINAL(vc->vte), vc->color_fore);
	vf->vte_terminal_set_color_background(VTE_TERMINAL(vc->vte), vc->color_back);
}


static void vte_get_settings(void)
{
	gchar **values = g_strsplit(app->terminal_settings, ";", 8);

	if (g_strv_length(values) != 8)
	{
		app->terminal_settings = g_strdup_printf("Monospace 10;#FFFFFF;#000000;500;xterm;true;true;false");
		values = g_strsplit(app->terminal_settings, ";", 8);
	}
	vc->font = g_strdup(values[0]);
	vc->color_fore = g_new0(GdkColor, 1);
	vc->color_back = g_new0(GdkColor, 1);
	gdk_color_parse(values[1], vc->color_fore);
	gdk_color_parse(values[2], vc->color_back);

	vc->scrollback_lines = strtod(values[3], NULL);
	if ((vc->scrollback_lines < 0) || (vc->scrollback_lines > 5000)) vc->scrollback_lines = 500;

	vc->emulation = g_strdup(values[4]);

	vc->scroll_on_key = utils_atob(values[5]);
	vc->scroll_on_out = utils_atob(values[6]);
	vc->follow_path = utils_atob(values[7]);

	g_strfreev(values);
}


static void vte_popup_menu_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data))
	{
		case 0:
		{
			if (vf->vte_terminal_get_has_selection(VTE_TERMINAL(vc->vte)))
				vf->vte_terminal_copy_clipboard(VTE_TERMINAL(vc->vte));
			break;
		}
		case 1:
		{
			vf->vte_terminal_paste_clipboard(VTE_TERMINAL(vc->vte));
			break;
		}
		case 2:
		{
			on_preferences1_activate(menuitem, NULL);
			gtk_notebook_set_current_page(GTK_NOTEBOOK(lookup_widget(app->prefs_dialog, "notebook2")), 6);
			break;
		}
	}
}


static GtkWidget *vte_create_popup_menu(void)
{
	GtkWidget *menu, *item;

	menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock("gtk-copy", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(0));

	item = gtk_image_menu_item_new_from_stock("gtk-paste", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(1));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_from_stock("gtk-preferences", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(2));

	return menu;
}


void vte_send_cmd(const gchar *cmd)
{
	vf->vte_terminal_feed_child(VTE_TERMINAL(vc->vte), cmd, strlen(cmd));
}


/*
void vte_drag_data_received(GtkWidget *widget, GdkDragContext  *drag_context, gint x, gint y,
							GtkSelectionData *data, guint info, guint time)
{
	geany_debug("length: %d, format: %d, action: %d", data->length, data->format, drag_context->action);
	if ((data->length >= 0) && (data->format == 8))
	{
		if (drag_context->action == GDK_ACTION_ASK)
		{
			gint accept = TRUE;
			// should I check the incoming data?
			if (accept)
				drag_context->action = GDK_ACTION_COPY;
		}
		gtk_drag_finish(drag_context, TRUE, FALSE, time);
		return;
	}
	gtk_drag_finish(drag_context, FALSE, FALSE, time);
}


gboolean vte_drag_drop(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time,
					   gpointer user_data)
{

	GdkAtom *atom;
	gtk_drag_get_data(widget, drag_context, atom, time);
	geany_debug("%s", GDK_ATOM_TO_POINTER(atom));
	return TRUE;
}
*/

#endif
