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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Virtual Terminal Emulation setup and handling code, using the libvte plugin library.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_VTE

#include "vte.h"

#include "callbacks.h"
#include "document.h"
#include "geanyobject.h"
#include "msgwindow.h"
#include "prefs.h"
#include "sciwrappers.h"
#include "support.h"
#include "ui_utils.h"
#include "utils.h"
#include "keybindings.h"

#include "gtkcompat.h"

/* include stdlib.h AND unistd.h, because on GNU/Linux pid_t seems to be
 * in stdlib.h, on FreeBSD in unistd.h, sys/types.h is needed for C89 */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <gdk/gdkkeysyms.h>
#include <signal.h>
#include <string.h>
#include <errno.h>


VteInfo vte_info = { FALSE, FALSE, FALSE, NULL, NULL };
VteConfig *vc;

static GPid pid = 0;
static gboolean clean = TRUE;
static GModule *module = NULL;
static struct VteFunctions *vf;
static gchar *gtk_menu_key_accel = NULL;
static GtkWidget *terminal_label = NULL;
static guint terminal_label_update_source = 0;

/* use vte wordchars to select paths */
static const gchar VTE_WORDCHARS[] = "-A-Za-z0-9,./?%&#:_";
static const gchar VTE_ADDITIONAL_WORDCHARS[] = "-,./?%&#:_";


/* Incomplete VteTerminal struct from vte/vte.h. */
typedef struct _VteTerminal VteTerminal;
struct _VteTerminal
{
	GtkWidget widget;
	GtkAdjustment *adjustment;
};

#define VTE_TERMINAL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), VTE_TYPE_TERMINAL, VteTerminal))
#define VTE_TYPE_TERMINAL (vf->vte_terminal_get_type())

typedef enum {
	VTE_CURSOR_BLINK_SYSTEM,
	VTE_CURSOR_BLINK_ON,
	VTE_CURSOR_BLINK_OFF
} VteTerminalCursorBlinkMode;

typedef enum {
	/* we don't care for the other possible values */
	VTE_PTY_DEFAULT = 0
} VtePtyFlags;


/* Holds function pointers we need to access the VTE API. */
struct VteFunctions
{
	guint (*vte_get_major_version) (void);
	guint (*vte_get_minor_version) (void);
	GtkWidget* (*vte_terminal_new) (void);
	pid_t (*vte_terminal_fork_command) (VteTerminal *terminal, const char *command, char **argv,
										char **envv, const char *directory, gboolean lastlog,
										gboolean utmp, gboolean wtmp);
	gboolean (*vte_terminal_spawn_sync) (VteTerminal *terminal, VtePtyFlags pty_flags,
										 const char *working_directory, char **argv, char **envv,
										 GSpawnFlags spawn_flags, GSpawnChildSetupFunc child_setup,
										 gpointer child_setup_data, GPid *child_pid,
										 GCancellable *cancellable, GError **error);
	void (*vte_terminal_set_size) (VteTerminal *terminal, glong columns, glong rows);
	void (*vte_terminal_set_word_chars) (VteTerminal *terminal, const char *spec);
	void (*vte_terminal_set_word_char_exceptions) (VteTerminal *terminal, const char *exceptions);
	void (*vte_terminal_set_mouse_autohide) (VteTerminal *terminal, gboolean setting);
	void (*vte_terminal_reset) (VteTerminal *terminal, gboolean full, gboolean clear_history);
	GType (*vte_terminal_get_type) (void);
	void (*vte_terminal_set_scroll_on_output) (VteTerminal *terminal, gboolean scroll);
	void (*vte_terminal_set_scroll_on_keystroke) (VteTerminal *terminal, gboolean scroll);
	void (*vte_terminal_set_font) (VteTerminal *terminal, const PangoFontDescription *font_desc);
	void (*vte_terminal_set_scrollback_lines) (VteTerminal *terminal, glong lines);
	gboolean (*vte_terminal_get_has_selection) (VteTerminal *terminal);
	void (*vte_terminal_copy_clipboard) (VteTerminal *terminal);
	void (*vte_terminal_paste_clipboard) (VteTerminal *terminal);
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
	GtkAdjustment* (*vte_terminal_get_adjustment) (VteTerminal *terminal);
#if GTK_CHECK_VERSION(3, 0, 0)
	/* hack for the VTE 2.91 API using GdkRGBA: we wrap the API to keep using GdkColor on our side */
	void (*vte_terminal_set_color_foreground_rgba) (VteTerminal *terminal, const GdkRGBA *foreground);
	void (*vte_terminal_set_color_bold_rgba) (VteTerminal *terminal, const GdkRGBA *foreground);
	void (*vte_terminal_set_color_background_rgba) (VteTerminal *terminal, const GdkRGBA *background);
#endif
};


static void create_vte(void);
static void vte_start(GtkWidget *widget);
static void vte_restart(GtkWidget *widget);
static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean vte_keyrelease_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean vte_keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean vte_register_symbols(GModule *module);
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


/* replacement for vte_terminal_get_adjustment() when it's not available */
static GtkAdjustment *default_vte_terminal_get_adjustment(VteTerminal *vte)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	if (GTK_IS_SCROLLABLE(vte))
		return gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(vte));
#endif
	/* this is only valid in < 0.38, 0.38 broke ABI */
	return vte->adjustment;
}


#if GTK_CHECK_VERSION(3, 0, 0)
/* Wrap VTE 2.91 API using GdkRGBA with GdkColor so we use a single API on our side */

static void rgba_from_color(GdkRGBA *rgba, const GdkColor *color)
{
	rgba->red = color->red / 65535.0;
	rgba->green = color->green / 65535.0;
	rgba->blue = color->blue / 65535.0;
	rgba->alpha = 1.0;
}

#	define WRAP_RGBA_SETTER(name) \
	static void wrap_##name(VteTerminal *terminal, const GdkColor *color) \
	{ \
		GdkRGBA rgba; \
		rgba_from_color(&rgba, color); \
		vf->name##_rgba(terminal, &rgba); \
	}

WRAP_RGBA_SETTER(vte_terminal_set_color_background)
WRAP_RGBA_SETTER(vte_terminal_set_color_bold)
WRAP_RGBA_SETTER(vte_terminal_set_color_foreground)

#	undef WRAP_RGBA_SETTER
#endif


static gchar **vte_get_child_environment(void)
{
	const gchar *exclude_vars[] = {"COLUMNS", "LINES", "TERM", "TERM_PROGRAM", NULL};

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


static void on_startup_complete(G_GNUC_UNUSED GObject *dummy)
{
	GeanyDocument *doc = document_get_current();

	if (doc)
		vte_cwd((doc->real_path != NULL) ? doc->real_path : doc->file_name, FALSE);
}


void vte_init(void)
{
	if (vte_info.have_vte == FALSE)
	{	/* vte_info.have_vte can be false even if VTE is compiled in, think of command line option */
		geany_debug("Disabling terminal support");
		return;
	}

	if (!EMPTY(vte_info.lib_vte))
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
		const gchar *sonames[] = {
#if GTK_CHECK_VERSION(3, 0, 0)
# ifdef __APPLE__
			"libvte-2.91.0.dylib", "libvte-2.91.dylib",
			"libvte2_90.9.dylib", "libvte2_90.dylib",
# endif
			"libvte-2.91.so", "libvte-2.91.so.0",
			"libvte2_90.so", "libvte2_90.so.9",
#else /* GTK 2 */
# ifdef __APPLE__
			"libvte.9.dylib", "libvte.dylib",
# endif
			"libvte.so", "libvte.so.9", "libvte.so.8", "libvte.so.4",
#endif
			NULL
		};

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
		geany_debug("Loaded libvte from %s", g_module_name(module));
		vf = g_new0(struct VteFunctions, 1);
		if (vte_register_symbols(module))
			vte_info.have_vte = TRUE;
		else
		{
			vte_info.have_vte = FALSE;
			g_free(vf);
			/* FIXME: is closing the module safe? see vte_close() and test on FreeBSD */
			/*g_module_close(module);*/
			module = NULL;
			return;
		}
	}

	create_vte();

	/* setup the F10 menu override (so it works before the widget is first realised). */
	override_menu_key();

	g_signal_connect(geany_object, "geany-startup-complete", G_CALLBACK(on_startup_complete), NULL);
}


static void on_vte_realize(void)
{
	/* the vte widget has to be realised before color changes take effect */
	vte_apply_user_settings();

	if (vf->vte_terminal_im_append_menuitems && vc->im_submenu)
		vf->vte_terminal_im_append_menuitems(VTE_TERMINAL(vc->vte), GTK_MENU_SHELL(vc->im_submenu));
}


static gboolean vte_start_idle(G_GNUC_UNUSED gpointer user_data)
{
	vte_start(vc->vte);
	return FALSE;
}


static void create_vte(void)
{
	GtkWidget *vte, *scrollbar, *hbox;

	vc->vte = vte = vf->vte_terminal_new();
	scrollbar = gtk_vscrollbar_new(vf->vte_terminal_get_adjustment(VTE_TERMINAL(vte)));
	gtk_widget_set_can_focus(scrollbar, FALSE);

	/* create menu now so copy/paste shortcuts work */
	vc->menu = vte_create_popup_menu();
	g_object_ref_sink(vc->menu);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vte, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

	/* set the default widget size first to prevent VTE expanding too much,
	 * sometimes causing the hscrollbar to be too big or out of view. */
	gtk_widget_set_size_request(GTK_WIDGET(vte), 10, 10);
	vf->vte_terminal_set_size(VTE_TERMINAL(vte), 30, 1);

	vf->vte_terminal_set_mouse_autohide(VTE_TERMINAL(vte), TRUE);
	if (vf->vte_terminal_set_word_chars)
		vf->vte_terminal_set_word_chars(VTE_TERMINAL(vte), VTE_WORDCHARS);
	else if (vf->vte_terminal_set_word_char_exceptions)
		vf->vte_terminal_set_word_char_exceptions(VTE_TERMINAL(vte), VTE_ADDITIONAL_WORDCHARS);

	gtk_drag_dest_set(vte, GTK_DEST_DEFAULT_ALL,
		dnd_targets, G_N_ELEMENTS(dnd_targets), GDK_ACTION_COPY);

	g_signal_connect(vte, "child-exited", G_CALLBACK(vte_start), NULL);
	g_signal_connect(vte, "button-press-event", G_CALLBACK(vte_button_pressed), NULL);
	g_signal_connect(vte, "event", G_CALLBACK(vte_keypress_cb), NULL);
	g_signal_connect(vte, "key-release-event", G_CALLBACK(vte_keyrelease_cb), NULL);
	g_signal_connect(vte, "commit", G_CALLBACK(vte_commit_cb), NULL);
	g_signal_connect(vte, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	g_signal_connect(vte, "drag-data-received", G_CALLBACK(vte_drag_data_received), NULL);

	/* start shell on idle otherwise the initial prompt can get corrupted */
	g_idle_add(vte_start_idle, NULL);

	gtk_widget_show_all(hbox);
	terminal_label = gtk_label_new(_("Terminal"));
	gtk_notebook_insert_page(GTK_NOTEBOOK(msgwindow.notebook), hbox, terminal_label, MSG_VTE);

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
	g_free(vc->shell);
	g_free(vc->font);
	g_free(vc->send_cmd_prefix);
	g_free(vc);
	g_free(gtk_menu_key_accel);
	/* Don't unload the module explicitly because it causes a segfault on FreeBSD. The segfault
	 * happens when the app really exits, not directly on g_module_close(). This still needs to
	 * be investigated. */
	/*g_module_close(module); */
}


static gboolean set_dirty_idle(gpointer user_data)
{
	gtk_widget_set_name(terminal_label, "geany-terminal-dirty");
	terminal_label_update_source = 0;
	return FALSE;
}


static void set_clean(gboolean value)
{
	if (clean != value)
	{
		if (terminal_label)
		{
			if (terminal_label_update_source > 0)
			{
				g_source_remove(terminal_label_update_source);
				terminal_label_update_source = 0;
			}
			if (value)
				gtk_widget_set_name(terminal_label, NULL);
			else
				terminal_label_update_source = g_timeout_add(150, set_dirty_idle, NULL);
		}
		clean = value;
	}
}


static gboolean vte_keyrelease_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (ui_is_keyval_enter_or_return(event->keyval) ||
		((event->keyval == GDK_c) && (event->state & GDK_CONTROL_MASK)))
	{
		/* assume any text on the prompt has been executed when pressing Enter/Return */
		set_clean(TRUE);
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
	set_clean(FALSE);
}


static void vte_start(GtkWidget *widget)
{
	/* split the shell command line, so arguments will work too */
	gchar **argv = g_strsplit(vc->shell, " ", -1);

	if (argv != NULL)
	{
		gchar **env = vte_get_child_environment();

		if (vf->vte_terminal_spawn_sync)
		{
			if (! vf->vte_terminal_spawn_sync(VTE_TERMINAL(widget), VTE_PTY_DEFAULT,
											  vte_info.dir, argv, env, 0, NULL, NULL,
											  &pid, NULL, NULL))
			{
				pid = -1;
			}
		}
		else
		{
			pid = vf->vte_terminal_fork_command(VTE_TERMINAL(widget), argv[0], argv, env,
												vte_info.dir, TRUE, TRUE, TRUE);
		}
		g_strfreev(env);
		g_strfreev(argv);
	}
	else
		pid = 0; /* use 0 as invalid pid */

	set_clean(TRUE);
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
	set_clean(TRUE);
}


static gboolean vte_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button == 3)
	{
		gtk_widget_grab_focus(vc->vte);
		gtk_menu_popup(GTK_MENU(vc->menu), NULL, NULL, NULL, NULL, event->button, event->time);
		return TRUE;
	}
	else if (event->button == 2)
	{
		gtk_widget_grab_focus(widget);
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


#if GTK_CHECK_VERSION(3, 0, 0)
static gboolean vte_is_2_91(void)
{
	guint major = vf->vte_get_major_version ? vf->vte_get_major_version() : 0;
	guint minor = vf->vte_get_minor_version ? vf->vte_get_minor_version() : 0;

	/* 2.91 API started at 0.38 */
	return ((major > 0 || (major == 0 && minor >= 38)) ||
	        /* 0.38 doesn't have runtime version checks, so check a symbol that didn't exist before */
	        vf->vte_terminal_spawn_sync != NULL);
}
#endif


static gboolean vte_register_symbols(GModule *mod)
{
	#define BIND_SYMBOL_FULL(name, dest) \
		g_module_symbol(mod, name, (void*)(dest))
	#define BIND_SYMBOL(field) \
		BIND_SYMBOL_FULL(#field, &vf->field)
	#define BIND_REQUIRED_SYMBOL_FULL(name, dest) \
		G_STMT_START { \
			if (! BIND_SYMBOL_FULL(name, dest)) \
			{ \
				g_critical(_("invalid VTE library \"%s\": missing symbol \"%s\""), \
						g_module_name(mod), name); \
				return FALSE; \
			} \
		} G_STMT_END
	#define BIND_REQUIRED_SYMBOL(field) \
		BIND_REQUIRED_SYMBOL_FULL(#field, &vf->field)
	#define BIND_REQUIRED_SYMBOL_RGBA_WRAPPED(field) \
		G_STMT_START { \
			BIND_REQUIRED_SYMBOL_FULL(#field, &vf->field##_rgba); \
			vf->field = wrap_##field; \
		} G_STMT_END

	BIND_SYMBOL(vte_get_major_version);
	BIND_SYMBOL(vte_get_minor_version);
	BIND_REQUIRED_SYMBOL(vte_terminal_new);
	BIND_REQUIRED_SYMBOL(vte_terminal_set_size);
	if (! BIND_SYMBOL(vte_terminal_spawn_sync))
		/* vte_terminal_spawn_sync() is available only in 0.38 */
		BIND_REQUIRED_SYMBOL(vte_terminal_fork_command);
	/* 0.38 removed vte_terminal_set_word_chars() */
	BIND_SYMBOL(vte_terminal_set_word_chars);
	/* 0.40 introduced it under a different API */
	BIND_SYMBOL(vte_terminal_set_word_char_exceptions);
	BIND_REQUIRED_SYMBOL(vte_terminal_set_mouse_autohide);
	BIND_REQUIRED_SYMBOL(vte_terminal_reset);
	BIND_REQUIRED_SYMBOL(vte_terminal_get_type);
	BIND_REQUIRED_SYMBOL(vte_terminal_set_scroll_on_output);
	BIND_REQUIRED_SYMBOL(vte_terminal_set_scroll_on_keystroke);
	BIND_REQUIRED_SYMBOL(vte_terminal_set_font);
	BIND_REQUIRED_SYMBOL(vte_terminal_set_scrollback_lines);
	BIND_REQUIRED_SYMBOL(vte_terminal_get_has_selection);
	BIND_REQUIRED_SYMBOL(vte_terminal_copy_clipboard);
	BIND_REQUIRED_SYMBOL(vte_terminal_paste_clipboard);
#if GTK_CHECK_VERSION(3, 0, 0)
	if (vte_is_2_91())
	{
		BIND_REQUIRED_SYMBOL_RGBA_WRAPPED(vte_terminal_set_color_foreground);
		BIND_REQUIRED_SYMBOL_RGBA_WRAPPED(vte_terminal_set_color_bold);
		BIND_REQUIRED_SYMBOL_RGBA_WRAPPED(vte_terminal_set_color_background);
	}
	else
#endif
	{
		BIND_REQUIRED_SYMBOL(vte_terminal_set_color_foreground);
		BIND_REQUIRED_SYMBOL(vte_terminal_set_color_bold);
		BIND_REQUIRED_SYMBOL(vte_terminal_set_color_background);
	}
	BIND_REQUIRED_SYMBOL(vte_terminal_feed_child);
	BIND_SYMBOL(vte_terminal_im_append_menuitems);
	if (! BIND_SYMBOL(vte_terminal_set_cursor_blink_mode))
		/* vte_terminal_set_cursor_blink_mode() is only available since 0.17.1, so if we don't find
		 * this symbol, we are probably on an older version and use the old API instead */
		BIND_REQUIRED_SYMBOL(vte_terminal_set_cursor_blinks);
	BIND_REQUIRED_SYMBOL(vte_terminal_select_all);
	BIND_REQUIRED_SYMBOL(vte_terminal_set_audible_bell);
	if (! BIND_SYMBOL(vte_terminal_get_adjustment))
		/* vte_terminal_get_adjustment() is available since 0.9 and removed in 0.38 */
		vf->vte_terminal_get_adjustment = default_vte_terminal_get_adjustment;

	#undef BIND_REQUIRED_SYMBOL_RGBA_WRAPPED
	#undef BIND_REQUIRED_SYMBOL
	#undef BIND_REQUIRED_SYMBOL_FULL
	#undef BIND_SYMBOL
	#undef BIND_SYMBOL_FULL

	return TRUE;
}


void vte_apply_user_settings(void)
{
	PangoFontDescription *font_desc;

	if (! ui_prefs.msgwindow_visible)
		return;

	vf->vte_terminal_set_scrollback_lines(VTE_TERMINAL(vc->vte), vc->scrollback_lines);
	vf->vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(vc->vte), vc->scroll_on_key);
	vf->vte_terminal_set_scroll_on_output(VTE_TERMINAL(vc->vte), vc->scroll_on_out);
	font_desc = pango_font_description_from_string(vc->font);
	vf->vte_terminal_set_font(VTE_TERMINAL(vc->vte), font_desc);
	pango_font_description_free(font_desc);
	vf->vte_terminal_set_color_foreground(VTE_TERMINAL(vc->vte), &vc->colour_fore);
	vf->vte_terminal_set_color_bold(VTE_TERMINAL(vc->vte), &vc->colour_fore);
	vf->vte_terminal_set_color_background(VTE_TERMINAL(vc->vte), &vc->colour_back);
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
	gboolean show_im_menu = TRUE;

	menu = gtk_menu_new();

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(main_widgets.window), accel_group);

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_COPY, NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group,
		GDK_c, GEANY_PRIMARY_MOD_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(vte_popup_menu_clicked), GINT_TO_POINTER(POPUP_COPY));

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PASTE, NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group,
		GDK_v, GEANY_PRIMARY_MOD_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
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

	/* VTE 2.91 doesn't have IM context items, and GTK >= 3.10 doesn't show them anyway */
	if (! vf->vte_terminal_im_append_menuitems || gtk_check_version(3, 10, 0) == NULL)
		show_im_menu = FALSE;
	else /* otherwise, query the setting */
		g_object_get(gtk_settings_get_default(), "gtk-show-input-method-menu", &show_im_menu, NULL);

	if (! show_im_menu)
		vc->im_submenu = NULL;
	else
	{
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
	}

	return menu;
}


/* If the command could be executed, TRUE is returned, FALSE otherwise (i.e. there was some text
 * on the prompt). */
gboolean vte_send_cmd(const gchar *cmd)
{
	if (clean)
	{
		vf->vte_terminal_feed_child(VTE_TERMINAL(vc->vte), cmd, strlen(cmd));
		set_clean(TRUE); /* vte_terminal_feed_child() also marks the vte as not clean */
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
	if (pid > 0)
	{
		gchar  buffer[4096 + 1];
		gchar *file = g_strdup_printf("/proc/%d/cwd", pid);
		gint   length = readlink(file, buffer, sizeof(buffer));

		if (length > 0 && *buffer == '/')
		{
			buffer[length] = '\0';
			g_free(vte_info.dir);
			vte_info.dir = g_strdup(buffer);
		}
		else if (length == 0)
		{
			gchar *cwd = g_get_current_dir();

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
				const gchar *msg = _("Directory not changed because the terminal may contain some input (press Ctrl+C or Enter to clear it).");
				ui_set_statusbar(FALSE, "%s", msg);
				geany_debug("%s", msg);
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
		if (gtk_selection_data_get_format(data) == 8 && gtk_selection_data_get_length(data) > 0)
			vf->vte_terminal_feed_child(VTE_TERMINAL(widget),
				(const gchar*) gtk_selection_data_get_data(data),
				gtk_selection_data_get_length(data));
	}
	else
	{
		gchar *text = (gchar*) gtk_selection_data_get_text(data);
		if (!EMPTY(text))
			vf->vte_terminal_feed_child(VTE_TERMINAL(widget), text, strlen(text));
		g_free(text);
	}
	gtk_drag_finish(drag_context, TRUE, FALSE, ltime);
}


static void on_check_run_in_vte_toggled(GtkToggleButton *togglebutton, GtkWidget *user_data)
{
	g_return_if_fail(GTK_IS_WIDGET(user_data));
	gtk_widget_set_sensitive(user_data, gtk_toggle_button_get_active(togglebutton));
}


static void on_term_font_set(GtkFontButton *widget, gpointer user_data)
{
	const gchar *fontbtn = gtk_font_button_get_font_name(widget);

	if (! utils_str_equal(fontbtn, vc->font))
	{
		SETPTR(vc->font, g_strdup(gtk_font_button_get_font_name(widget)));
		vte_apply_user_settings();
	}
}


static void on_term_fg_color_set(GtkColorButton *widget, gpointer user_data)
{
	gtk_color_button_get_color(widget, &vc->colour_fore);
}


static void on_term_bg_color_set(GtkColorButton *widget, gpointer user_data)
{
	gtk_color_button_get_color(widget, &vc->colour_back);
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
		text = sci_get_selection_contents(doc->editor->sci);
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
