/*
 *      vte.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/*
 * Virtual Terminal Emulation setup and handling code, using the libvte plugin library.
 */

#include "geany.h"

#ifdef HAVE_VTE

#include <gdk/gdkkeysyms.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "vte.h"
#include "msgwindow.h"
#include "support.h"
#include "prefs.h"
#include "ui_utils.h"
#include "utils.h"
#include "document.h"
#include "callbacks.h"
#include "geanywraplabel.h"


VteInfo vte_info;
VteConfig *vc;

extern gchar **environ;
static pid_t pid;
static gboolean clean = TRUE;
static GModule *module = NULL;
static struct VteFunctions *vf;
static gboolean popup_menu_created = FALSE;
static gchar *gtk_menu_key_accel = NULL;
static gint vte_prefs_tab_num = -1;

/* use vte wordchars to select paths */
static const gchar VTE_WORDCHARS[] = "-A-Za-z0-9,./?%&#:_";


#define VTE_TERMINAL(obj) (GTK_CHECK_CAST((obj), VTE_TYPE_TERMINAL, VteTerminal))
#define VTE_TYPE_TERMINAL (vf->vte_terminal_get_type())

static void create_vte(void);
static void vte_start(GtkWidget *widget);
static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean vte_keyrelease(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean vte_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void vte_register_symbols(GModule *module);
static void vte_popup_menu_clicked(GtkMenuItem *menuitem, gpointer user_data);
static GtkWidget *vte_create_popup_menu(void);
void vte_commit(VteTerminal *vte, gchar *arg1, guint arg2, gpointer user_data);
void vte_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y,
							GtkSelectionData *data, guint info, guint ltime);


enum
{
	POPUP_COPY,
	POPUP_PASTE,
	POPUP_CHANGEPATH,
	POPUP_RESTARTTERMINAL,
	POPUP_PREFERENCES,
	TARGET_UTF8_STRING = 0,
	TARGET_TEXT,
	TARGET_COMPOUND_TEXT,
	TARGET_STRING,
	TARGET_TEXT_PLAIN
};

static const GtkTargetEntry dnd_targets[] =
{
  { "UTF8_STRING", 0, TARGET_UTF8_STRING },
  { "TEXT", 0, TARGET_TEXT },
  { "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
  { "STRING", 0, TARGET_STRING },
  { "text/plain", 0, TARGET_TEXT_PLAIN },
};


/* taken from anjuta, thanks */
static gchar **vte_get_child_environment(void)
{
	/* code from gnome-terminal, sort of. */
	gchar **p;
	gint i;
	gchar **retval;
#define EXTRA_ENV_VARS 5

	/* count env vars that are set */
	for (p = environ; *p; p++);

	i = p - environ;
	retval = g_new0(gchar *, i + 1 + EXTRA_ENV_VARS);

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

	retval[i] = g_strdup("TERM=xterm");
	++i;

	retval[i] = NULL;

	return retval;
}


static void override_menu_key(void)
{
	if (gtk_menu_key_accel == NULL) /* for restoring the default value */
		g_object_get(G_OBJECT(gtk_settings_get_default()), "gtk-menu-bar-accel",
																	&gtk_menu_key_accel, NULL);

	if (vc->ignore_menu_bar_accel)
		gtk_settings_set_string_property(gtk_settings_get_default(), "gtk-menu-bar-accel",
								"<Shift><Control><Mod1><Mod2><Mod3><Mod4><Mod5>F10", "Geany");
	else
		gtk_settings_set_string_property(gtk_settings_get_default(),
								"gtk-menu-bar-accel", gtk_menu_key_accel, "Geany");
}


void vte_init(void)
{
	if (vte_info.have_vte == FALSE)
	{	/* app->have_vte can be false, even if VTE is compiled in, think of command line option */
		geany_debug("Disabling terminal support");
		return;
	}

	if (vte_info.lib_vte && vte_info.lib_vte[0] != '\0')
	{
		module = g_module_open(vte_info.lib_vte, G_MODULE_BIND_LAZY);
	}
#ifdef VTE_MODULE_PATH
	else
	{
		module = g_module_open(VTE_MODULE_PATH, G_MODULE_BIND_LAZY);
	}
#endif

	if (module == NULL)
	{
		gint i;
		const gchar *sonames[] = {  "libvte.so", "libvte.so.4",
									"libvte.so.8", "libvte.so.9", NULL };

		for (i = 0; sonames[i] != NULL && module == NULL; i++ )
		{
			module = g_module_open(sonames[i], G_MODULE_BIND_LAZY);
		}
	}

	if (module == NULL)
	{
		vte_info.have_vte = FALSE;
		geany_debug("Could not load libvte.so, terminal support disabled");
		return;
	}
	else
	{
		vte_info.have_vte = TRUE;
		vf = g_new0(struct VteFunctions, 1);
		vte_register_symbols(module);
	}

	create_vte();

	/* setup the F10 menu override (so it works before the widget is first realised). */
	override_menu_key();
}


static void create_vte(void)
{
	GtkWidget *vte, *scrollbar, *hbox, *frame;

	vte = vf->vte_terminal_new();
	vc->vte = vte;
	scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(VTE_TERMINAL(vte)->adjustment));
	GTK_WIDGET_UNSET_FLAGS(scrollbar, GTK_CAN_FOCUS);

	frame = gtk_frame_new(NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_box_pack_start(GTK_BOX(hbox), vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

	/* set the default widget size first to prevent VTE expanding too much,
	 * sometimes causing the hscrollbar to be too big or out of view. */
	gtk_widget_set_size_request(GTK_WIDGET(vte), 10, 10);
	vf->vte_terminal_set_size(VTE_TERMINAL(vte), 30, 1);

	vf->vte_terminal_set_mouse_autohide(VTE_TERMINAL(vte), TRUE);
	vf->vte_terminal_set_word_chars(VTE_TERMINAL(vte), VTE_WORDCHARS);

    gtk_drag_dest_set(vte, GTK_DEST_DEFAULT_ALL,
		dnd_targets, G_N_ELEMENTS(dnd_targets), GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(vte), "child-exited", G_CALLBACK(vte_start), NULL);
	g_signal_connect(G_OBJECT(vte), "button-press-event", G_CALLBACK(vte_button_pressed), NULL);
	g_signal_connect(G_OBJECT(vte), "event", G_CALLBACK(vte_keypress), NULL);
	g_signal_connect(G_OBJECT(vte), "key-release-event", G_CALLBACK(vte_keyrelease), NULL);
	g_signal_connect(G_OBJECT(vte), "commit", G_CALLBACK(vte_commit), NULL);
	g_signal_connect(G_OBJECT(vte), "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	g_signal_connect(G_OBJECT(vte), "drag-data-received", G_CALLBACK(vte_drag_data_received), NULL);

	vte_start(vte);

	gtk_widget_show_all(frame);
	gtk_notebook_insert_page(GTK_NOTEBOOK(msgwindow.notebook), frame, gtk_label_new(_("Terminal")), MSG_VTE);

	/* the vte widget has to be realised before color changes take effect */
	g_signal_connect(G_OBJECT(vte), "realize", G_CALLBACK(vte_apply_user_settings), NULL);
}


void vte_close(void)
{
	g_free(vf);
	/* free the vte widget before unloading vte module
	 * this prevents a segfault on X close window if the message window is hidden */
	gtk_widget_destroy(vc->vte);
	if (popup_menu_created) gtk_widget_destroy(vc->menu);
	g_free(vc->emulation);
	g_free(vc->shell);
	g_free(vc->font);
	g_free(vc->colour_back);
	g_free(vc->colour_fore);
	g_free(vc);
	g_free(gtk_menu_key_accel);
	/* don't unload the module explicitly because it causes a segfault on FreeBSD */
	/** TODO is this still/really true? */
	/*g_module_close(module); */
}


static gboolean vte_keyrelease(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return ||
			 event->keyval == GDK_ISO_Enter ||
			 event->keyval == GDK_KP_Enter ||
			 ((event->keyval == GDK_c) && (event->state & GDK_CONTROL_MASK)))
	{
		clean = TRUE; /* assume any text on the prompt has been executed when pressing Enter/Return */
	}
	return FALSE;
}


static gboolean vte_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (vc->enable_bash_keys)
		return FALSE;	/* Ctrl-[CD] will be handled by the VTE itself */

	if (event->type != GDK_KEY_RELEASE)
		return FALSE;

	if ((event->keyval == GDK_c ||
		event->keyval == GDK_d ||
		event->keyval == GDK_C ||
		event->keyval == GDK_D) &&
		event->state & GDK_CONTROL_MASK &&
		! (event->state & GDK_SHIFT_MASK) && ! (event->state & GDK_MOD1_MASK))
	{
		vte_get_working_directory(); /* try to keep the working directory when restarting the VTE */

		if (pid > 0)
		{
			kill(pid, SIGINT);
			pid = 0;
		}
		vf->vte_terminal_reset(VTE_TERMINAL(widget), TRUE, TRUE);
		vte_start(widget);

		return TRUE;
	}
	return FALSE;
}


void vte_commit(VteTerminal *vte, gchar *arg1, guint arg2, gpointer user_data)
{
	clean = FALSE;
}


static void vte_start(GtkWidget *widget)
{
	VteTerminal *vte = VTE_TERMINAL(widget);
	gchar **env;
	gchar **argv;

	/* split the shell command line, so arguments will work too */
	argv = g_strsplit(vc->shell, " ", -1);

	if (argv != NULL)
	{
		env = vte_get_child_environment();
		pid = vf->vte_terminal_fork_command(VTE_TERMINAL(vte), argv[0], argv, env,
												vte_info.dir, TRUE, TRUE, TRUE);
		g_strfreev(env);
		g_strfreev(argv);
	}
	else
		pid = 0; /* use 0 as invalid pid */

	clean = TRUE;
}


static void vte_restart(GtkWidget *widget)
{
		vte_get_working_directory(); /* try to keep the working directory when restarting the VTE */
	 	if (pid > 0)
		{
			kill(pid, SIGINT);
			pid = 0;
		}
		vf->vte_terminal_reset(VTE_TERMINAL(widget), TRUE, TRUE);
		vte_start(widget);
}


static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button == 3)
	{
		if (! popup_menu_created)
		{
			vc->menu = vte_create_popup_menu();
			vf->vte_terminal_im_append_menuitems(VTE_TERMINAL(vc->vte), GTK_MENU_SHELL(vc->im_submenu));
			popup_menu_created = TRUE;
		}

		gtk_widget_grab_focus(vc->vte);
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
	g_module_symbol(mod, "vte_terminal_reset", (void*)&vf->vte_terminal_reset);
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
	g_module_symbol(mod, "vte_terminal_im_append_menuitems", (void*)&vf->vte_terminal_im_append_menuitems);
}


void vte_apply_user_settings(void)
{
	if (! ui_prefs.msgwindow_visible) return;
	/*if (! GTK_WIDGET_REALIZED(vc->vte)) gtk_widget_realize(vc->vte);*/
	vf->vte_terminal_set_scrollback_lines(VTE_TERMINAL(vc->vte), vc->scrollback_lines);
	vf->vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(vc->vte), vc->scroll_on_key);
	vf->vte_terminal_set_scroll_on_output(VTE_TERMINAL(vc->vte), vc->scroll_on_out);
	vf->vte_terminal_set_emulation(VTE_TERMINAL(vc->vte), vc->emulation);
	vf->vte_terminal_set_font_from_string(VTE_TERMINAL(vc->vte), vc->font);
	vf->vte_terminal_set_color_foreground(VTE_TERMINAL(vc->vte), vc->colour_fore);
	vf->vte_terminal_set_color_background(VTE_TERMINAL(vc->vte), vc->colour_back);

	override_menu_key();
}


static void vte_popup_menu_clicked(GtkMenuItem *menuitem, gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data))
	{
		case POPUP_COPY:
		{
			if (vf->vte_terminal_get_has_selection(VTE_TERMINAL(vc->vte)))
				vf->vte_terminal_copy_clipboard(VTE_TERMINAL(vc->vte));
			break;
		}
		case POPUP_PASTE:
		{
			vf->vte_terminal_paste_clipboard(VTE_TERMINAL(vc->vte));
			break;
		}
		case POPUP_CHANGEPATH:
		{
			gint idx = document_get_cur_idx();
			if (DOC_IDX_VALID(idx))
				vte_cwd(doc_list[idx].file_name, TRUE);
			break;
		}
		case POPUP_RESTARTTERMINAL:
		{
			vte_restart(vc->vte);
			break;
		}
		case POPUP_PREFERENCES:
		{
			GtkWidget *notebook;

			prefs_show_dialog();

			notebook = lookup_widget(ui_widgets.prefs_dialog, "notebook2");

			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), vte_prefs_tab_num);
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
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_COPY));

	item = gtk_image_menu_item_new_from_stock("gtk-paste", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_PASTE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Set Path From Document"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_CHANGEPATH));

	item = gtk_image_menu_item_new_with_mnemonic(_("_Restart Terminal"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_RESTARTTERMINAL));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	vc->im_submenu = gtk_menu_new();

	item = gtk_image_menu_item_new_with_mnemonic(_("_Input Methods"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), vc->im_submenu);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_from_stock("gtk-preferences", NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer)item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_PREFERENCES));

	msgwin_menu_add_common_items(GTK_MENU(menu));

	return menu;
}


/* if the command could be executed, TRUE is returned, FALSE otherwise (i.e. there was some text
 * on the prompt). */
gboolean vte_send_cmd(const gchar *cmd)
{
	if (clean)
	{
		vf->vte_terminal_feed_child(VTE_TERMINAL(vc->vte), cmd, strlen(cmd));
		clean = TRUE; /* vte_terminal_feed_child() also marks the vte as not clean */
		return TRUE;
	}
	else
		return FALSE;
}


/* Taken from Terminal by os-cillation: terminal_screen_get_working_directory, thanks.
 * Determines the working directory using various OS-specific mechanisms. */
const gchar* vte_get_working_directory(void)
{
	gchar  buffer[4096 + 1];
	gchar *file;
	gchar *cwd;
	gint   length;

	if (pid > 0)
	{
		file = g_strdup_printf("/proc/%d/cwd", pid);
		length = readlink(file, buffer, sizeof (buffer));

		if (length > 0 && *buffer == '/')
		{
			buffer[length] = '\0';
			g_free(vte_info.dir);
			vte_info.dir = g_strdup(buffer);
		}
		else if (length == 0)
		{
			cwd = g_get_current_dir();
			if (G_LIKELY(cwd != NULL))
			{
				if (chdir(file) == 0)
				{
					g_free(vte_info.dir);
					vte_info.dir = g_get_current_dir();
					if (chdir(cwd) != 0)
						geany_debug("%s: %s", __func__, g_strerror(errno));
				}
				g_free(cwd);
			}
		}
		g_free(file);
	}

	return vte_info.dir;
}


/* Changes the current working directory of the VTE to the path of the given filename.
 * filename is expected to be in UTF-8 encoding.
 * filename can also be a path, then it is used directly.
 * If force is set to TRUE, it will always change the cwd
 * */
void vte_cwd(const gchar *filename, gboolean force)
{
	if (vte_info.have_vte && (vc->follow_path || force) && filename != NULL)
	{
		gchar *path;

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
			path = g_strdup(filename);
		else
			path = g_path_get_dirname(filename);

		vte_get_working_directory(); /* refresh vte_info.dir */
		if (! utils_str_equal(path, vte_info.dir))
		{
			/* use g_shell_quote to avoid problems with spaces, '!' or something else in path */
			gchar *quoted_path = g_shell_quote(path);
			gchar *cmd = g_strconcat("cd ", quoted_path, "\n", NULL);
			if (! vte_send_cmd(cmd))
				ui_set_statusbar(FALSE,
		_("Could not change the directory in the VTE because it probably contains a command."));
			g_free(quoted_path);
			g_free(cmd);
		}
		g_free(path);
	}
}


void vte_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y,
							GtkSelectionData *data, guint info, guint ltime)
{
	if (info == TARGET_TEXT_PLAIN)
	{
		if (data->format == 8 && data->length > 0)
			vf->vte_terminal_feed_child(VTE_TERMINAL(widget),
				(const gchar*) data->data, data->length);
	}
	else
	{
		gchar *text = (gchar*) gtk_selection_data_get_text(data);
		if (NZV(text))
			vf->vte_terminal_feed_child(VTE_TERMINAL(widget), text, strlen(text));
		g_free (text);
	}
	gtk_drag_finish(drag_context, TRUE, FALSE, ltime);
}


static void check_run_in_vte_toggled(GtkToggleButton *togglebutton, GtkWidget *user_data)
{
	gtk_widget_set_sensitive(user_data, gtk_toggle_button_get_active(togglebutton));
}


void vte_append_preferences_tab(void)
{
	if (vte_info.have_vte)
	{
		GtkWidget *notebook, *vbox, *label, *alignment, *table, *frame, *box;
		GtkWidget *font_term, *color_fore, *color_back, *spin_scrollback, *entry_emulation;
		GtkWidget *check_scroll_key, *check_scroll_out, *check_follow_path;
		GtkWidget *check_enable_bash_keys, *check_ignore_menu_key;
		GtkWidget *check_run_in_vte, *check_skip_script, *entry_shell, *button_shell, *image_shell;
		GtkTooltips *tooltips;
		GtkObject *spin_scrollback_adj;

		tooltips = GTK_TOOLTIPS(lookup_widget(ui_widgets.prefs_dialog, "tooltips"));
		notebook = lookup_widget(ui_widgets.prefs_dialog, "notebook2");

		frame = ui_frame_new_with_alignment(_("Terminal plugin"), &alignment);
		vbox = gtk_vbox_new(FALSE, 12);
		gtk_container_add(GTK_CONTAINER(alignment), vbox);

		label = gtk_label_new(_("Terminal"));
		vte_prefs_tab_num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);

		label = geany_wrap_label_new(_("These settings for the virtual terminal emulator widget (VTE) only apply if the VTE library could be loaded."));
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 6);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_FILL);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		table = gtk_table_new(6, 2, FALSE);
		gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
		gtk_table_set_row_spacings(GTK_TABLE(table), 3);
		gtk_table_set_col_spacings(GTK_TABLE(table), 10);

		label = gtk_label_new(_("Terminal font:"));
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		font_term = gtk_font_button_new();
		gtk_table_attach(GTK_TABLE(table), font_term, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_tooltips_set_tip(tooltips, font_term, _("Sets the font for the terminal widget."), NULL);

		label = gtk_label_new(_("Foreground color:"));
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		label = gtk_label_new(_("Background color:"));
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		color_fore = gtk_color_button_new();
		gtk_table_attach(GTK_TABLE(table), color_fore, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_tooltips_set_tip(tooltips, color_fore, _("Sets the foreground color of the text in the terminal widget."), NULL);
		gtk_color_button_set_title(GTK_COLOR_BUTTON(color_fore), _("Color Chooser"));

		color_back = gtk_color_button_new();
		gtk_table_attach(GTK_TABLE(table), color_back, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_tooltips_set_tip(tooltips, color_back, _("Sets the background color of the text in the terminal widget."), NULL);
		gtk_color_button_set_title(GTK_COLOR_BUTTON(color_back), _("Color Chooser"));

		label = gtk_label_new(_("Scrollback lines:"));
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		spin_scrollback_adj = gtk_adjustment_new(500, 0, 5000, 1, 10, 10);
		spin_scrollback = gtk_spin_button_new(GTK_ADJUSTMENT(spin_scrollback_adj), 1, 0);
		gtk_table_attach(GTK_TABLE(table), spin_scrollback, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_tooltips_set_tip(tooltips, spin_scrollback, _("Specifies the history in lines, which you can scroll back in the terminal widget."), NULL);
		gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_scrollback), TRUE);
		gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin_scrollback), TRUE);

		label = gtk_label_new(_("Terminal emulation:"));
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		entry_emulation = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(table), entry_emulation, 1, 2, 4, 5,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_tooltips_set_tip(tooltips, entry_emulation, _("Controls how the terminal emulator should behave. Do not change this value unless you know exactly what you are doing."), NULL);

		label = gtk_label_new(_("Shell:"));
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		entry_shell = gtk_entry_new();
		gtk_tooltips_set_tip(tooltips, entry_shell, _("Sets the path to the shell which should be started inside the terminal emulation."), NULL);

		button_shell = gtk_button_new();
		gtk_widget_show(button_shell);

		box = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start_defaults(GTK_BOX(box), entry_shell);
		gtk_box_pack_start(GTK_BOX(box), button_shell, FALSE, FALSE, 0);
		gtk_table_attach(GTK_TABLE(table), box, 1, 2, 5, 6,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

		image_shell = gtk_image_new_from_stock("gtk-open", GTK_ICON_SIZE_BUTTON);
		gtk_widget_show(image_shell);
		gtk_container_add(GTK_CONTAINER(button_shell), image_shell);

		box = gtk_vbox_new(FALSE, 3);
		check_scroll_key = gtk_check_button_new_with_mnemonic(_("Scroll on keystroke"));
		gtk_tooltips_set_tip(tooltips, check_scroll_key, _("Whether to scroll to the bottom if a key was pressed."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_scroll_key), FALSE);
		gtk_container_add(GTK_CONTAINER(box), check_scroll_key);

		check_scroll_out = gtk_check_button_new_with_mnemonic(_("Scroll on output"));
		gtk_tooltips_set_tip(tooltips, check_scroll_out, _("Whether to scroll to the bottom when output is generated."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_scroll_out), FALSE);
		gtk_container_add(GTK_CONTAINER(box), check_scroll_out);

		check_enable_bash_keys = gtk_check_button_new_with_mnemonic(_("Override Geany keybindings"));
		gtk_tooltips_set_tip(tooltips, check_enable_bash_keys,
			_("Allows the VTE to receive keyboard shortcuts (apart from focus commands)."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_enable_bash_keys), FALSE);
		gtk_container_add(GTK_CONTAINER(box), check_enable_bash_keys);

		check_ignore_menu_key = gtk_check_button_new_with_mnemonic(_("Disable menu shortcut key (F10 by default)"));
		gtk_tooltips_set_tip(tooltips, check_ignore_menu_key, _("This option disables the keybinding to popup the menu bar (default is F10). Disabling it can be useful if you use, for example, Midnight Commander within the VTE."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_ignore_menu_key), FALSE);
		gtk_container_add(GTK_CONTAINER(box), check_ignore_menu_key);

		check_follow_path = gtk_check_button_new_with_mnemonic(_("Follow the path of the current file"));
		gtk_tooltips_set_tip(tooltips, check_follow_path, _("Whether to execute \"cd $path\" when you switch between opened files."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_follow_path), FALSE);
		gtk_container_add(GTK_CONTAINER(box), check_follow_path);

		/* create check_skip_script checkbox before the check_skip_script checkbox to be able to
		 * use the object for the toggled handler of check_skip_script checkbox */
		check_skip_script = gtk_check_button_new_with_mnemonic(_("Don't use run script"));
		gtk_tooltips_set_tip(tooltips, check_skip_script, _("Don't use the simple run script which is usually used to display the exit status of the executed program."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_skip_script), FALSE);
		gtk_widget_set_sensitive(check_skip_script, vc->run_in_vte);

		check_run_in_vte = gtk_check_button_new_with_mnemonic(_("Execute programs in VTE"));
		gtk_tooltips_set_tip(tooltips, check_run_in_vte, _("Run programs in VTE instead of opening a terminal emulation window. Please note, programs executed in VTE cannot be stopped."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_run_in_vte), FALSE);
		gtk_container_add(GTK_CONTAINER(box), check_run_in_vte);
		g_signal_connect((gpointer) check_run_in_vte, "toggled",
			G_CALLBACK(check_run_in_vte_toggled), check_skip_script);

		/* now add the check_skip_script checkbox after the check_run_in_vte checkbox */
		gtk_container_add(GTK_CONTAINER(box), check_skip_script);

		gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "font_term",
				gtk_widget_ref(font_term),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "color_fore",
				gtk_widget_ref(color_fore),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "color_back",
				gtk_widget_ref(color_back),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "spin_scrollback",
				gtk_widget_ref(spin_scrollback),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "entry_emulation",
				gtk_widget_ref(entry_emulation),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "entry_shell",
				gtk_widget_ref(entry_shell),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "check_scroll_key",
				gtk_widget_ref(check_scroll_key),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "check_scroll_out",
				gtk_widget_ref(check_scroll_out),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "check_enable_bash_keys",
				gtk_widget_ref(check_enable_bash_keys),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "check_ignore_menu_key",
				gtk_widget_ref(check_ignore_menu_key),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "check_follow_path",
				gtk_widget_ref(check_follow_path),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "check_run_in_vte",
				gtk_widget_ref(check_run_in_vte),	(GDestroyNotify) gtk_widget_unref);
		g_object_set_data_full(G_OBJECT(ui_widgets.prefs_dialog), "check_skip_script",
				gtk_widget_ref(check_skip_script),	(GDestroyNotify) gtk_widget_unref);

		gtk_widget_show_all(frame);

		g_signal_connect((gpointer) font_term, "font-set", G_CALLBACK(on_prefs_font_choosed),
														   GINT_TO_POINTER(4));
		g_signal_connect((gpointer) color_fore, "color-set", G_CALLBACK(on_prefs_color_choosed),
															 GINT_TO_POINTER(2));
		g_signal_connect((gpointer) color_back, "color-set", G_CALLBACK(on_prefs_color_choosed),
															 GINT_TO_POINTER(3));
		g_signal_connect((gpointer) button_shell, "clicked",
				G_CALLBACK(on_prefs_tools_button_clicked), entry_shell);
	}
}


#endif
