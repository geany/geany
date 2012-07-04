/*
 *      vte.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 */

/*
 * Virtual Terminal Emulation setup and handling code, using the libvte plugin library.
 */

#include "geany.h"

#ifdef HAVE_VTE

/* include stdlib.h AND unistd.h, because on GNU/Linux pid_t seems to be
 * in stdlib.h, on FreeBSD in unistd.h, sys/types.h is needed for C89 */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include "vte.h"
#include "support.h"
#include "prefs.h"
#include "ui_utils.h"
#include "utils.h"
#include "document.h"
#include "msgwindow.h"
#include "callbacks.h"
#include "geanywraplabel.h"
#include "editor.h"
#include "sciwrappers.h"


VteInfo vte_info;
VteConfig *vc;

static pid_t pid = 0;
static gboolean clean = TRUE;
static GModule *module = NULL;
static struct VteFunctions *vf;
static gchar *gtk_menu_key_accel = NULL;

/* use vte wordchars to select paths */
static const gchar VTE_WORDCHARS[] = "-A-Za-z0-9,./?%&#:_";


/* Incomplete VteTerminal struct from vte/vte.h. */
typedef struct _VteTerminal VteTerminal;
struct _VteTerminal
{
	GtkWidget widget;
	GtkAdjustment *adjustment;
};

#define VTE_TERMINAL(obj) (GTK_CHECK_CAST((obj), VTE_TYPE_TERMINAL, VteTerminal))
#define VTE_TYPE_TERMINAL (vf->vte_terminal_get_type())

typedef enum {
	VTE_CURSOR_BLINK_SYSTEM,
	VTE_CURSOR_BLINK_ON,
	VTE_CURSOR_BLINK_OFF
} VteTerminalCursorBlinkMode;


/* Holds function pointers we need to access the VTE API. */
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
	void (*vte_terminal_set_color_bold) (VteTerminal *terminal, const GdkColor *foreground);
	void (*vte_terminal_set_color_background) (VteTerminal *terminal, const GdkColor *background);
	void (*vte_terminal_feed_child) (VteTerminal *terminal, const char *data, glong length);
	void (*vte_terminal_im_append_menuitems) (VteTerminal *terminal, GtkMenuShell *menushell);
	void (*vte_terminal_set_cursor_blink_mode) (VteTerminal *terminal,
												VteTerminalCursorBlinkMode mode);
	void (*vte_terminal_set_cursor_blinks) (VteTerminal *terminal, gboolean blink);
	void (*vte_terminal_select_all) (VteTerminal *terminal);
	void (*vte_terminal_set_audible_bell) (VteTerminal *terminal, gboolean is_audible);
};


static void create_vte(void);
static void vte_start(GtkWidget *widget);
static void vte_restart(GtkWidget *widget);
static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean vte_keyrelease_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean vte_keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void vte_register_symbols(GModule *module);
static void vte_popup_menu_clicked(GtkMenuItem *menuitem, gpointer user_data);
static GtkWidget *vte_create_popup_menu(void);
static void vte_commit_cb(VteTerminal *vte, gchar *arg1, guint arg2, gpointer user_data);
static void vte_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
								   gint x, gint y, GtkSelectionData *data, guint info, guint ltime);


enum
{
	POPUP_COPY,
	POPUP_PASTE,
	POPUP_SELECTALL,
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


static gchar **vte_get_child_environment(void)
{
	const gchar *exclude_vars[] = {"COLUMNS", "LINES", "TERM", NULL};

	return utils_copy_environment(exclude_vars, "TERM", "xterm", NULL);
}


static void override_menu_key(void)
{
	if (gtk_menu_key_accel == NULL) /* for restoring the default value */
		g_object_get(G_OBJECT(gtk_settings_get_default()),
			"gtk-menu-bar-accel", &gtk_menu_key_accel, NULL);

	if (vc->ignore_menu_bar_accel)
		gtk_settings_set_string_property(gtk_settings_get_default(),
			"gtk-menu-bar-accel", "<Shift><Control><Mod1><Mod2><Mod3><Mod4><Mod5>F10", "Geany");
	else
		gtk_settings_set_string_property(gtk_settings_get_default(),
			"gtk-menu-bar-accel", gtk_menu_key_accel, "Geany");
}


void vte_init(void)
{
	if (vte_info.have_vte == FALSE)
	{	/* vte_info.have_vte can be false even if VTE is compiled in, think of command line option */
		geany_debug("Disabling terminal support");
		return;
	}

	if (NZV(vte_info.lib_vte))
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

		for (i = 0; sonames[i] != NULL && module == NULL; i++)
		{
			module = g_module_open(sonames[i], G_MODULE_BIND_LAZY);
		}
	}

	if (module == NULL)
	{
		vte_info.have_vte = FALSE;
		geany_debug("Could not load libvte.so, embedded terminal support disabled");
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


static void on_vte_realize(void)
{
	/* the vte widget has to be realised before color changes take effect */
	vte_apply_user_settings();

	vf->vte_terminal_im_append_menuitems(VTE_TERMINAL(vc->vte), GTK_MENU_SHELL(vc->im_submenu));
}


static void create_vte(void)
{
	GtkWidget *vte, *scrollbar, *hbox, *frame;

	vc->vte = vte = vf->vte_terminal_new();
	scrollbar = gtk_vscrollbar_new(GTK_ADJUSTMENT(VTE_TERMINAL(vte)->adjustment));
	GTK_WIDGET_UNSET_FLAGS(scrollbar, GTK_CAN_FOCUS);

	/* create menu now so copy/paste shortcuts work */
	vc->menu = vte_create_popup_menu();
	g_object_ref_sink(vc->menu);

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

	g_signal_connect(vte, "child-exited", G_CALLBACK(vte_start), NULL);
	g_signal_connect(vte, "button-press-event", G_CALLBACK(vte_button_pressed), NULL);
	g_signal_connect(vte, "event", G_CALLBACK(vte_keypress_cb), NULL);
	g_signal_connect(vte, "key-release-event", G_CALLBACK(vte_keyrelease_cb), NULL);
	g_signal_connect(vte, "commit", G_CALLBACK(vte_commit_cb), NULL);
	g_signal_connect(vte, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	g_signal_connect(vte, "drag-data-received", G_CALLBACK(vte_drag_data_received), NULL);

	vte_start(vte);

	gtk_widget_show_all(frame);
	gtk_notebook_insert_page(GTK_NOTEBOOK(msgwindow.notebook), frame, gtk_label_new(_("Terminal")), MSG_VTE);

	g_signal_connect_after(vte, "realize", G_CALLBACK(on_vte_realize), NULL);
}


void vte_close(void)
{
	g_free(vf);
	/* free the vte widget before unloading vte module
	 * this prevents a segfault on X close window if the message window is hidden */
	gtk_widget_destroy(vc->vte);
	gtk_widget_destroy(vc->menu);
	g_object_unref(vc->menu);
	g_free(vc->emulation);
	g_free(vc->shell);
	g_free(vc->font);
	g_free(vc->colour_back);
	g_free(vc->colour_fore);
	g_free(vc->send_cmd_prefix);
	g_free(vc);
	g_free(gtk_menu_key_accel);
	/* Don't unload the module explicitly because it causes a segfault on FreeBSD. The segfault
	 * happens when the app really exits, not directly on g_module_close(). This still needs to
	 * be investigated. */
	/*g_module_close(module); */
}


static gboolean vte_keyrelease_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (ui_is_keyval_enter_or_return(event->keyval) ||
		((event->keyval == GDK_c) && (event->state & GDK_CONTROL_MASK)))
	{
		/* assume any text on the prompt has been executed when pressing Enter/Return */
		clean = TRUE;
	}
	return FALSE;
}


static gboolean vte_keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
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
		vte_restart(widget);
		return TRUE;
	}
	return FALSE;
}


static void vte_commit_cb(VteTerminal *vte, gchar *arg1, guint arg2, gpointer user_data)
{
	clean = FALSE;
}


static void vte_start(GtkWidget *widget)
{
	gchar **env;
	gchar **argv;

	/* split the shell command line, so arguments will work too */
	argv = g_strsplit(vc->shell, " ", -1);

	if (argv != NULL)
	{
		env = vte_get_child_environment();
		pid = vf->vte_terminal_fork_command(VTE_TERMINAL(widget), argv[0], argv, env,
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
	clean = TRUE;
}


static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_widget_grab_focus(vc->vte);
		gtk_menu_popup(GTK_MENU(vc->menu), NULL, NULL, NULL, NULL, event->button, event->time);
	}
	return FALSE;
}


static void vte_set_cursor_blink_mode(void)
{
	if (vf->vte_terminal_set_cursor_blink_mode != NULL)
		/* vte >= 0.17.1 */
		vf->vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(vc->vte),
			(vc->cursor_blinks) ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);
	else
		/* vte < 0.17.1 */
		vf->vte_terminal_set_cursor_blinks(VTE_TERMINAL(vc->vte), vc->cursor_blinks);
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
	g_module_symbol(mod, "vte_terminal_set_color_bold", (void*)&vf->vte_terminal_set_color_bold);
	g_module_symbol(mod, "vte_terminal_set_color_background", (void*)&vf->vte_terminal_set_color_background);
	g_module_symbol(mod, "vte_terminal_feed_child", (void*)&vf->vte_terminal_feed_child);
	g_module_symbol(mod, "vte_terminal_im_append_menuitems", (void*)&vf->vte_terminal_im_append_menuitems);
	g_module_symbol(mod, "vte_terminal_set_cursor_blink_mode", (void*)&vf->vte_terminal_set_cursor_blink_mode);
	if (vf->vte_terminal_set_cursor_blink_mode == NULL)
		/* vte_terminal_set_cursor_blink_mode() is only available since 0.17.1, so if we don't find
		 * this symbol, we are probably on an older version and use the old API instead */
		g_module_symbol(mod, "vte_terminal_set_cursor_blinks", (void*)&vf->vte_terminal_set_cursor_blinks);
	g_module_symbol(mod, "vte_terminal_select_all", (void*)&vf->vte_terminal_select_all);
	g_module_symbol(mod, "vte_terminal_set_audible_bell", (void*)&vf->vte_terminal_set_audible_bell);
}


void vte_apply_user_settings(void)
{
	if (! ui_prefs.msgwindow_visible)
		return;

	vf->vte_terminal_set_scrollback_lines(VTE_TERMINAL(vc->vte), vc->scrollback_lines);
	vf->vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(vc->vte), vc->scroll_on_key);
	vf->vte_terminal_set_scroll_on_output(VTE_TERMINAL(vc->vte), vc->scroll_on_out);
	vf->vte_terminal_set_emulation(VTE_TERMINAL(vc->vte), vc->emulation);
	vf->vte_terminal_set_font_from_string(VTE_TERMINAL(vc->vte), vc->font);
	vf->vte_terminal_set_color_foreground(VTE_TERMINAL(vc->vte), vc->colour_fore);
	vf->vte_terminal_set_color_bold(VTE_TERMINAL(vc->vte), vc->colour_fore);
	vf->vte_terminal_set_color_background(VTE_TERMINAL(vc->vte), vc->colour_back);
	vf->vte_terminal_set_audible_bell(VTE_TERMINAL(vc->vte), prefs.beep_on_errors);
	vte_set_cursor_blink_mode();

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
		case POPUP_SELECTALL:
		{
			vte_select_all();
			break;
		}
		case POPUP_CHANGEPATH:
		{
			GeanyDocument *doc = document_get_current();
			if (doc != NULL)
				vte_cwd(doc->file_name, TRUE);
			break;
		}
		case POPUP_RESTARTTERMINAL:
		{
			vte_restart(vc->vte);
			break;
		}
		case POPUP_PREFERENCES:
		{
			GtkWidget *notebook, *tab_page;

			prefs_show_dialog();

			notebook = ui_lookup_widget(ui_widgets.prefs_dialog, "notebook2");
			tab_page = ui_lookup_widget(ui_widgets.prefs_dialog, "frame_term");

			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
				gtk_notebook_page_num(GTK_NOTEBOOK(notebook), GTK_WIDGET(tab_page)));

			break;
		}
	}
}


static GtkWidget *vte_create_popup_menu(void)
{
	GtkWidget *menu, *item;
	GtkAccelGroup *accel_group;

	menu = gtk_menu_new();

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(main_widgets.window), accel_group);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group,
		GDK_c, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_COPY));

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group,
		GDK_v, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_PASTE));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_SELECT_ALL, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_SELECTALL));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("_Set Path From Document"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_CHANGEPATH));

	item = gtk_image_menu_item_new_with_mnemonic(_("_Restart Terminal"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_RESTARTTERMINAL));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_PREFERENCES));

	msgwin_menu_add_common_items(GTK_MENU(menu));

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	/* the IM submenu should always be the last item to be consistent with other GTK popup menus */
	vc->im_submenu = gtk_menu_new();

	item = gtk_image_menu_item_new_with_mnemonic(_("_Input Methods"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), vc->im_submenu);
	/* submenu populated after vte realized */
	return menu;
}


/* If the command could be executed, TRUE is returned, FALSE otherwise (i.e. there was some text
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
 * Determines the working directory using various OS-specific mechanisms and stores the determined
 * directory in vte_info.dir. Note: vte_info.dir contains the real path. */
const gchar *vte_get_working_directory(void)
{
	gchar  buffer[4096 + 1];
	gchar *file;
	gchar *cwd;
	gint   length;

	if (pid > 0)
	{
		file = g_strdup_printf("/proc/%d/cwd", pid);
		length = readlink(file, buffer, sizeof(buffer));

		if (length > 0 && *buffer == '/')
		{
			buffer[length] = '\0';
			g_free(vte_info.dir);
			vte_info.dir = g_strdup(buffer);
		}
		else if (length == 0)
		{
			cwd = g_get_current_dir();
			if (cwd != NULL)
			{
				if (chdir(file) == 0)
				{
					g_free(vte_info.dir);
					vte_info.dir = g_get_current_dir();
					if (chdir(cwd) != 0)
						geany_debug("%s: %s", G_STRFUNC, g_strerror(errno));
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
 */
void vte_cwd(const gchar *filename, gboolean force)
{
	if (vte_info.have_vte && (vc->follow_path || force) &&
		filename != NULL && g_path_is_absolute(filename))
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
			gchar *cmd = g_strconcat(vc->send_cmd_prefix, "cd ", quoted_path, "\n", NULL);
			if (! vte_send_cmd(cmd))
			{
				ui_set_statusbar(FALSE,
		_("Could not change the directory in the VTE because it probably contains a command."));
				geany_debug(
		"Could not change the directory in the VTE because it probably contains a command.");
			}
			g_free(quoted_path);
			g_free(cmd);
		}
		g_free(path);
	}
}


static void vte_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
								   gint x, gint y, GtkSelectionData *data, guint info, guint ltime)
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
		g_free(text);
	}
	gtk_drag_finish(drag_context, TRUE, FALSE, ltime);
}


G_MODULE_EXPORT void on_check_run_in_vte_toggled(GtkToggleButton *togglebutton, GtkWidget *user_data)
{
	g_return_if_fail(GTK_IS_WIDGET(user_data));
	gtk_widget_set_sensitive(user_data, gtk_toggle_button_get_active(togglebutton));
}


G_MODULE_EXPORT void on_term_font_set(GtkFontButton *widget, gpointer user_data)
{
	const gchar *fontbtn = gtk_font_button_get_font_name(widget);

	if (! utils_str_equal(fontbtn, vc->font))
	{
		SETPTR(vc->font, g_strdup(gtk_font_button_get_font_name(widget)));
		vte_apply_user_settings();
	}
}


G_MODULE_EXPORT void on_term_fg_color_set(GtkColorButton *widget, gpointer user_data)
{
	g_free(vc->colour_fore);
	vc->colour_fore = g_new0(GdkColor, 1);
	gtk_color_button_get_color(widget, vc->colour_fore);
}


G_MODULE_EXPORT void on_term_bg_color_set(GtkColorButton *widget, gpointer user_data)
{
	g_free(vc->colour_back);
	vc->colour_back = g_new0(GdkColor, 1);
	gtk_color_button_get_color(widget, vc->colour_back);
}


void vte_append_preferences_tab(void)
{
	if (vte_info.have_vte)
	{
		GtkWidget *frame_term, *button_shell, *entry_shell;
		GtkWidget *check_run_in_vte, *check_skip_script;
		GtkWidget *font_button, *fg_color_button, *bg_color_button;

		button_shell = GTK_WIDGET(ui_lookup_widget(ui_widgets.prefs_dialog, "button_term_shell"));
		entry_shell = GTK_WIDGET(ui_lookup_widget(ui_widgets.prefs_dialog, "entry_shell"));
		ui_setup_open_button_callback(button_shell, NULL,
			GTK_FILE_CHOOSER_ACTION_OPEN, GTK_ENTRY(entry_shell));

		check_skip_script = GTK_WIDGET(ui_lookup_widget(ui_widgets.prefs_dialog, "check_skip_script"));
		gtk_widget_set_sensitive(check_skip_script, vc->run_in_vte);

		check_run_in_vte = GTK_WIDGET(ui_lookup_widget(ui_widgets.prefs_dialog, "check_run_in_vte"));
		g_signal_connect(G_OBJECT(check_run_in_vte), "toggled",
			G_CALLBACK(on_check_run_in_vte_toggled), check_skip_script);

		font_button = ui_lookup_widget(ui_widgets.prefs_dialog, "font_term");
		g_signal_connect(font_button, "font-set",  G_CALLBACK(on_term_font_set), NULL);

		fg_color_button = ui_lookup_widget(ui_widgets.prefs_dialog, "color_fore");
		g_signal_connect(fg_color_button, "color-set", G_CALLBACK(on_term_fg_color_set), NULL);

		bg_color_button = ui_lookup_widget(ui_widgets.prefs_dialog, "color_back");
		g_signal_connect(bg_color_button, "color-set", G_CALLBACK(on_term_bg_color_set), NULL);

		frame_term = ui_lookup_widget(ui_widgets.prefs_dialog, "frame_term");
		gtk_widget_show_all(frame_term);
	}
}


void vte_select_all(void)
{
	if (vf->vte_terminal_select_all != NULL)
		vf->vte_terminal_select_all(VTE_TERMINAL(vc->vte));
}


void vte_send_selection_to_vte(void)
{
	GeanyDocument *doc;
	gchar *text;
	gsize len;

	doc = document_get_current();
	g_return_if_fail(doc != NULL);

	if (sci_has_selection(doc->editor->sci))
	{
		text = g_malloc0(sci_get_selected_text_length(doc->editor->sci) + 1);
		sci_get_selected_text(doc->editor->sci, text);
	}
	else
	{	/* Get the current line */
		gint line_num = sci_get_current_line(doc->editor->sci);
		text = sci_get_line(doc->editor->sci, line_num);
	}

	len = strlen(text);

	if (vc->send_selection_unsafe)
	{	/* Explicitly append a trailing newline character to get the command executed,
		   this is disabled by default as it could cause all sorts of damage. */
		if (text[len-1] != '\n' && text[len-1] != '\r')
		{
			SETPTR(text, g_strconcat(text, "\n", NULL));
			len++;
		}
	}
	else
	{	/* Make sure there is no newline character at the end to prevent unwanted execution */
		while (text[len-1] == '\n' || text[len-1] == '\r')
		{
			text[len-1] = '\0';
			len--;
		}
	}

	vf->vte_terminal_feed_child(VTE_TERMINAL(vc->vte), text, len);

	/* show the VTE */
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_VTE);
	gtk_widget_grab_focus(vc->vte);
	msgwin_show_hide(TRUE);

	g_free(text);
}


#endif
