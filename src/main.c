/*
 *      main.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 * Program initialization and cleanup.
 */

#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "geany.h"

#if HAVE_LOCALE_H
# include <locale.h>
#endif

#include "main.h"
#include "prefs.h"
#include "interface.h"
#include "support.h"
#include "callbacks.h"

#include "ui_utils.h"
#include "utils.h"
#include "document.h"
#include "filetypes.h"
#include "keyfile.h"
#include "win32.h"
#include "msgwindow.h"
#include "dialogs.h"
#include "templates.h"
#include "encodings.h"
#include "treeviews.h"
#include "notebook.h"
#include "keybindings.h"
#include "editor.h"
#include "search.h"
#include "build.h"
#include "highlighting.h"
#include "symbols.h"
#include "project.h"
#include "tools.h"
#include "navqueue.h"
#include "plugins.h"

#ifdef HAVE_SOCKET
# include "socket.h"
#endif

#ifdef HAVE_VTE
# include "vte.h"
#endif

#ifndef N_
# define N_(String) (String)
#endif


GeanyApp *app;

GeanyStatus	 main_status;
CommandLineOptions cl_options;	// fields initialised in parse_command_line_options


static gboolean want_plugins;

// command-line options
static gboolean debug_mode = FALSE;
static gboolean ignore_global_tags = FALSE;
static gboolean no_msgwin = FALSE;
static gboolean show_version = FALSE;
static gchar *alternate_config = NULL;
#ifdef HAVE_VTE
static gboolean no_vte = FALSE;
static gchar *lib_vte = NULL;
#endif
#ifdef HAVE_SOCKET
static gboolean ignore_socket = FALSE;
#endif
static gboolean generate_datafiles = FALSE;
static gboolean generate_tags = FALSE;
static gboolean ft_names = FALSE;
static gboolean no_plugins = FALSE;

// in alphabetical order of short options
static GOptionEntry entries[] =
{
	{ "column", 0, 0, G_OPTION_ARG_INT, &cl_options.goto_column, N_("Set initial column number for the first opened file (useful in conjunction with --line)"), NULL },
	{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &alternate_config, N_("Use an alternate configuration directory"), NULL },
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug_mode, N_("Runs in debug mode (means being verbose)"), NULL },
	{ "ft-names", 0, 0, G_OPTION_ARG_NONE, &ft_names, N_("Print internal filetype names"), NULL },
	{ "generate-tags", 'g', 0, G_OPTION_ARG_NONE, &generate_tags, N_("Generate global tags file (see documentation)"), NULL },
	{ "generate-data-files", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &generate_datafiles, "", NULL },
#ifdef HAVE_SOCKET
	{ "new-instance", 'i', 0, G_OPTION_ARG_NONE, &ignore_socket, N_("Don't open files in a running instance, force opening a new instance"), NULL },
#endif
	{ "line", 'l', 0, G_OPTION_ARG_INT, &cl_options.goto_line, N_("Set initial line number for the first opened file"), NULL },
	{ "no-msgwin", 'm', 0, G_OPTION_ARG_NONE, &no_msgwin, N_("Don't show message window at startup"), NULL },
	{ "no-ctags", 'n', 0, G_OPTION_ARG_NONE, &ignore_global_tags, N_("Don't load auto completion data (see documentation)"), NULL },
	{ "no-plugins", 'p', 0, G_OPTION_ARG_NONE, &no_plugins, N_("Don't load plugins"), NULL },
	{ "no-session", 's', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &cl_options.load_session, N_("don't load the previous session's files"), NULL },
#ifdef HAVE_VTE
	{ "no-terminal", 't', 0, G_OPTION_ARG_NONE, &no_vte, N_("Don't load terminal support"), NULL },
	{ "vte-lib", 0, 0, G_OPTION_ARG_FILENAME, &lib_vte, N_("Filename of libvte.so"), NULL },
#endif
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, N_("Show version and exit"), NULL },
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};



/* Geany main debug function */
void geany_debug(gchar const *format, ...)
{
#ifndef GEANY_DEBUG
	if (app != NULL && app->debug_mode)
#endif
	{
		va_list args;
		va_start(args, format);
		g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, format, args);
		va_end(args);
	}
}

/* special things for the initial setup of the checkboxes and related stuff
 * an action on a setting is only performed if the setting is not equal to the program default
 * (all the following code is not perfect but it works for the moment) */
static void apply_settings(void)
{
	ui_update_fold_items();

	// toolbar, message window and sidebar are by default visible, so don't change it if it is true
	if (! prefs.toolbar_visible)
	{
		app->ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_toolbar1")), FALSE);
		gtk_widget_hide(app->toolbar);
		app->ignore_callback = FALSE;
	}
	if (! ui_prefs.msgwindow_visible)
	{
		app->ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_messages_window1")), FALSE);
		gtk_widget_hide(lookup_widget(app->window, "scrolledwindow1"));
		app->ignore_callback = FALSE;
	}
	if (! ui_prefs.sidebar_visible)
	{
		app->ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_sidebar1")), FALSE);
		app->ignore_callback = FALSE;
	}
	ui_treeviews_show_hide(TRUE);
	// sets the icon style of the toolbar
	switch (prefs.toolbar_icon_style)
	{
		case GTK_TOOLBAR_BOTH:
		{
			//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "images_and_text1")), TRUE);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(ui_widgets.toolbar_menu, "images_and_text2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_ICONS:
		{
			//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "images_only1")), TRUE);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(ui_widgets.toolbar_menu, "images_only2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_TEXT:
		{
			//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "text_only1")), TRUE);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(ui_widgets.toolbar_menu, "text_only2")), TRUE);
			break;
		}
	}
	gtk_toolbar_set_style(GTK_TOOLBAR(app->toolbar), prefs.toolbar_icon_style);

	// sets the icon size of the toolbar, use user preferences (.gtkrc) if not set
	if (prefs.toolbar_icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR ||
		prefs.toolbar_icon_size == GTK_ICON_SIZE_LARGE_TOOLBAR)
	{
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(app->toolbar), prefs.toolbar_icon_size);
	}
	ui_update_toolbar_icons(prefs.toolbar_icon_size);

	// line number and markers margin are by default enabled
	if (! editor_prefs.show_markers_margin)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_markers_margin1")), FALSE);
		editor_prefs.show_markers_margin = FALSE;
	}
	if (! editor_prefs.show_linenumber_margin)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_linenumber_margin1")), FALSE);
		editor_prefs.show_linenumber_margin = FALSE;
	}

	// interprets the saved window geometry
	if (prefs.save_winpos && ui_prefs.geometry[0] != -1)
	{
		gtk_window_move(GTK_WINDOW(app->window), ui_prefs.geometry[0], ui_prefs.geometry[1]);
		gtk_window_set_default_size(GTK_WINDOW(app->window), ui_prefs.geometry[2], ui_prefs.geometry[3]);
		if (ui_prefs.geometry[4] == 1)
			gtk_window_maximize(GTK_WINDOW(app->window));
	}

	// hide statusbar if desired
	if (! prefs.statusbar_visible)
	{
		gtk_widget_hide(app->statusbar);
	}

	app->ignore_callback = TRUE;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
				lookup_widget(app->window, "menu_line_breaking1")), editor_prefs.line_wrapping);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
				lookup_widget(app->window, "menu_use_auto_indentation1")),
				(editor_prefs.indent_mode != INDENT_NONE));
	app->ignore_callback = FALSE;

	// connect the toolbar dropdown menu for the new button
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(
			lookup_widget(app->window, "menutoolbutton1")), ui_widgets.new_file_menu);

	// set the tab placements of the notebooks
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->notebook), prefs.tab_pos_editor);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(msgwindow.notebook), prefs.tab_pos_msgwin);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->treeview_notebook), prefs.tab_pos_sidebar);

	ui_update_toolbar_items();

	// whether to show notebook tabs or not
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(app->notebook), prefs.show_notebook_tabs);
}


static void main_init(void)
{
	// inits
	app->window				= NULL;
	app->project			= NULL;
	ui_widgets.open_fontsel		= NULL;
	ui_widgets.open_colorsel		= NULL;
	ui_widgets.open_filesel		= NULL;
	ui_widgets.save_filesel		= NULL;
	ui_widgets.prefs_dialog		= NULL;
	tv.default_tag_tree	= NULL;
	main_status.main_window_realized= FALSE;
	prefs.tab_order_ltr		= FALSE;
	main_status.quitting			= FALSE;
	app->ignore_callback	= FALSE;
	app->tm_workspace				= tm_get_workspace();
	ui_prefs.recent_queue				= g_queue_new();
	main_status.opening_session_files		= FALSE;

	app->window = create_window1();
	ui_widgets.new_file_menu = gtk_menu_new();
	ui_widgets.recent_files_toolbar = gtk_menu_new();
	ui_widgets.recent_files_menuitem = lookup_widget(app->window, "recent_files1");
	ui_widgets.recent_files_menubar = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(ui_widgets.recent_files_menuitem),
							ui_widgets.recent_files_menubar);

	// store important pointers in the GeanyApp structure
	app->toolbar = lookup_widget(app->window, "toolbar1");
	app->treeview_notebook = lookup_widget(app->window, "notebook3");
	app->notebook = lookup_widget(app->window, "notebook1");
	app->statusbar = lookup_widget(app->window, "statusbar");
	app->popup_menu = create_edit_menu1();
	ui_widgets.toolbar_menu = create_toolbar_popup_menu1();
	ui_widgets.popup_goto_items[0] = lookup_widget(app->popup_menu, "goto_tag_definition1");
	ui_widgets.popup_goto_items[1] = lookup_widget(app->popup_menu, "goto_tag_declaration1");
	ui_widgets.popup_goto_items[2] = lookup_widget(app->popup_menu, "find_usage1");
	ui_widgets.popup_items[0] = lookup_widget(app->popup_menu, "cut1");
	ui_widgets.popup_items[1] = lookup_widget(app->popup_menu, "copy1");
	ui_widgets.popup_items[2] = lookup_widget(app->popup_menu, "delete1");
	ui_widgets.popup_items[3] = lookup_widget(app->popup_menu, "to_lower_case1");
	ui_widgets.popup_items[4] = lookup_widget(app->popup_menu, "to_upper_case1");
	ui_widgets.menu_copy_items[0] = lookup_widget(app->window, "menu_cut1");
	ui_widgets.menu_copy_items[1] = lookup_widget(app->window, "menu_copy1");
	ui_widgets.menu_copy_items[2] = lookup_widget(app->window, "menu_delete1");
	ui_widgets.menu_copy_items[3] = lookup_widget(app->window, "menu_to_lower_case2");
	ui_widgets.menu_copy_items[4] = lookup_widget(app->window, "menu_to_upper_case2");
	ui_widgets.menu_insert_include_items[0] = lookup_widget(app->popup_menu, "insert_include1");
	ui_widgets.menu_insert_include_items[1] = lookup_widget(app->window, "insert_include2");
	ui_widgets.save_buttons[0] = lookup_widget(app->window, "menu_save1");
	ui_widgets.save_buttons[1] = lookup_widget(app->window, "toolbutton10");
	ui_widgets.save_buttons[2] = lookup_widget(app->window, "menu_save_all1");
	ui_widgets.save_buttons[3] = lookup_widget(app->window, "toolbutton22");
	ui_widgets.redo_items[0] = lookup_widget(app->popup_menu, "redo1");
	ui_widgets.redo_items[1] = lookup_widget(app->window, "menu_redo2");
	ui_widgets.redo_items[2] = lookup_widget(app->window, "toolbutton_redo");
	ui_widgets.undo_items[0] = lookup_widget(app->popup_menu, "undo1");
	ui_widgets.undo_items[1] = lookup_widget(app->window, "menu_undo2");
	ui_widgets.undo_items[2] = lookup_widget(app->window, "toolbutton_undo");

	ui_init();

	// set widget names for matching with .gtkrc-2.0
	gtk_widget_set_name(app->window, "GeanyMainWindow");
	gtk_widget_set_name(ui_widgets.toolbar_menu, "GeanyToolbarMenu");
	gtk_widget_set_name(app->popup_menu, "GeanyEditMenu");
}


/* get the full file path of a command-line argument
 * N.B. the result should be freed and may contain '/../' or '/./ ' */
gchar *get_argv_filename(const gchar *filename)
{
	gchar *result;

	if (g_path_is_absolute(filename))
		result = g_strdup(filename);
	else
	{
		// use current dir
		gchar *cur_dir = g_get_current_dir();

		result = g_strjoin(
			G_DIR_SEPARATOR_S, cur_dir, filename, NULL);
		g_free(cur_dir);
	}
	return result;
}


static void setup_paths()
{
	gchar *data_dir;
	gchar *doc_dir;

	// set paths
#ifdef G_OS_WIN32
	// take the installation directory(the one where geany.exe is located) as the base for the
	// documentation and data files
	gchar *install_dir = g_win32_get_package_installation_directory("geany", NULL);

	data_dir = g_strconcat(install_dir, "\\data", NULL); // e.g. C:\Program Files\geany\data
	doc_dir = g_strconcat(install_dir, "\\doc", NULL);

	g_free(install_dir);
#else
	data_dir = g_strdup(PACKAGE_DATA_DIR "/" PACKAGE "/"); // e.g. /usr/share/geany
	doc_dir = g_strdup(PACKAGE_DATA_DIR "/doc/" PACKAGE "/html/");
#endif

	// convert path names to locale encoding
	app->datadir = utils_get_locale_from_utf8(data_dir);
	app->docdir = utils_get_locale_from_utf8(doc_dir);

	g_free(data_dir);
	g_free(doc_dir);
}


static void locale_init()
{
#ifdef ENABLE_NLS
	gchar *locale_dir = NULL;

#if HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif

#ifdef G_OS_WIN32
	gchar *install_dir = g_win32_get_package_installation_directory("geany", NULL);
	// e.g. C:\Program Files\geany\lib\locale
	locale_dir = g_strconcat(install_dir, "\\lib\\locale", NULL);
	g_free(install_dir);
#else
	locale_dir = g_strdup(PACKAGE_LOCALE_DIR);
#endif

	bindtextdomain(GETTEXT_PACKAGE, locale_dir);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	g_free(locale_dir);
#endif
}


static void parse_command_line_options(gint *argc, gchar ***argv)
{
	GOptionContext *context;
	GError *error = NULL;

	// first initialise cl_options fields with default values
	cl_options.load_session = TRUE;
	cl_options.goto_line = -1;
	cl_options.goto_column = -1;

	context = g_option_context_new(_("[FILES...]"));
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(g_option_context_get_main_group(context), GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_parse(context, argc, argv, &error);
	g_option_context_free(context);

	if (show_version)
	{
		printf(PACKAGE " " VERSION " ");
		printf(_("(built on %s with GTK %d.%d.%d, GLib %d.%d.%d)"),
				__DATE__, GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
				GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
		printf("\n");

		exit(0);
	}

#ifdef GEANY_DEBUG
	app->debug_mode = TRUE;
	geany_debug("debug mode built in (can't be disabled)");
#else
	app->debug_mode = debug_mode;
#endif

	if (alternate_config)
	{
		geany_debug("alternate config: %s", alternate_config);
		app->configdir = alternate_config;
	}
	else
		app->configdir = g_strconcat(GEANY_HOME_DIR, G_DIR_SEPARATOR_S, ".", PACKAGE, NULL);

#ifdef GEANY_DEBUG
	if (generate_datafiles)
	{
		filetypes_init_types();
		configuration_generate_data_files();	// currently only filetype_extensions.conf
		exit(0);
	}
#endif
	if (generate_tags)
	{
		gboolean ret;

		filetypes_init_types();
		configuration_read_filetype_extensions();	// needed for *.lang.tags filetype matching
		ret = symbols_generate_global_tags(*argc, *argv);
		exit(ret);
	}

	if (ft_names)
	{
		int i;

		printf("Geany's internal filetype names:\n");
		filetypes_init_types();
		for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
		{
			printf("%s\n", filetypes[i]->name);
		}
		filetypes_free_types();
		exit(0);
	}

#ifdef HAVE_SOCKET
	socket_info.ignore_socket = ignore_socket;
	if (ignore_socket)
		cl_options.load_session = FALSE;
#endif
#ifdef HAVE_VTE
	vte_info.lib_vte = lib_vte;
#endif
	cl_options.ignore_global_tags = ignore_global_tags;
}


// Returns 0 if config dir is OK.
static gint setup_config_dir()
{
	gint mkdir_result = 0;
	gchar *tmp = app->configdir;

	// convert configdir to locale encoding to avoid troubles
	app->configdir = utils_get_locale_from_utf8(app->configdir);
	g_free(tmp);

	mkdir_result = utils_make_settings_dir(app->configdir, app->datadir, app->docdir);
	if (mkdir_result != 0)
	{
		if (! dialogs_show_question(
			_("Configuration directory could not be created (%s).\nThere could be some problems "
			  "using Geany without a configuration directory.\nStart Geany anyway?"),
			  g_strerror(mkdir_result)))
		{
			exit(0);
		}
	}
	return mkdir_result;
}


static void signal_cb(gint sig)
{
	if (sig == SIGTERM)
	{
		on_exit_clicked(NULL, NULL);
	}
}


// open files from command line
static gboolean open_cl_files(gint argc, gchar **argv)
{
	gint i;

	if (argc <= 1) return FALSE;

	document_delay_colourise();

	for(i = 1; i < argc; i++)
	{
		gchar *filename = get_argv_filename(argv[i]);

		if (filename != NULL &&
			g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
		{
			gint idx;

			idx = document_open_file(-1, filename, 0, FALSE, NULL, NULL);
			// add recent file manually because opening_session_files is set
			if (DOC_IDX_VALID(idx))
				ui_add_recent_file(doc_list[idx].file_name);
		}
		else if (filename != NULL)
		{	// create new file if it doesn't exist
			gint idx;

			idx = document_new_file(filename, NULL, NULL);
			if (DOC_IDX_VALID(idx))
				ui_add_recent_file(doc_list[idx].file_name);
		}
		else
		{
			gchar *msg = _("Could not find file '%s'.");

			g_printerr(msg, filename);	// also print to the terminal
			g_printerr("\n");
			msgwin_status_add(msg, filename);
		}
		g_free(filename);
	}
	document_colourise_new();
	return TRUE;
}


static void load_project_file()
{
	gchar *locale_filename;

	g_return_if_fail(project_prefs.session_file != NULL);

	locale_filename = utils_get_locale_from_utf8(project_prefs.session_file);

	if (*locale_filename)
		project_load_file(locale_filename);

	g_free(locale_filename);
	g_free(project_prefs.session_file);	// no longer needed
}


static void load_settings()
{
	configuration_load();
	// let cmdline options overwrite configuration settings
#ifdef HAVE_VTE
	vte_info.have_vte = (no_vte) ? FALSE : vte_info.load_vte;
#endif
	if (no_msgwin) ui_prefs.msgwindow_visible = FALSE;

	want_plugins = prefs.load_plugins && !no_plugins;
}


gint main(gint argc, gchar **argv)
{
	gint idx;
	gint config_dir_result;

	app = g_new0(GeanyApp, 1);
	memset(&main_status, 0, sizeof(GeanyStatus));
	memset(&prefs, 0, sizeof(GeanyPrefs));
	memset(&ui_prefs, 0, sizeof(UIPrefs));
	memset(&ui_widgets, 0, sizeof(UIWidgets));

	setup_paths();
	locale_init();
	parse_command_line_options(&argc, &argv);

	gtk_set_locale();

	signal(SIGTERM, signal_cb);
#ifdef G_OS_UNIX
	// SIGQUIT is used to kill spawned children and we get also this signal, so ignore
	signal(SIGQUIT, SIG_IGN);
	// ignore SIGPIPE signal for preventing sudden death of program
	signal(SIGPIPE, SIG_IGN);
#endif

	config_dir_result = setup_config_dir();
#ifdef HAVE_SOCKET
    // check and create (unix domain) socket for remote operation
	if (! socket_info.ignore_socket)
	{
		socket_info.lock_socket = -1;
		socket_info.lock_socket_tag = 0;
		socket_info.lock_socket = socket_init(argc, argv);
		if (socket_info.lock_socket < 0)
		{
			// Socket exists
			if (argc > 1)	// filenames were sent to first instance, so quit
			{
				g_free(app->configdir);
				g_free(app->datadir);
				g_free(app->docdir);
				g_free(app);
				return 0;
			}
			// Start a new instance if no command line strings were passed
			socket_info.ignore_socket = TRUE;
			cl_options.load_session = FALSE;
		}
	}
#endif

	geany_debug("GTK+ %u.%u.%u, GLib %u.%u.%u",
		gtk_major_version, gtk_minor_version, gtk_micro_version,
		glib_major_version, glib_minor_version, glib_micro_version);
	gtk_init(&argc, &argv);

	// inits
	main_init();
	gtk_widget_set_size_request(app->window, GEANY_WINDOW_MINIMAL_WIDTH, GEANY_WINDOW_MINIMAL_HEIGHT);
	gtk_window_set_default_size(GTK_WINDOW(app->window), GEANY_WINDOW_DEFAULT_WIDTH, GEANY_WINDOW_DEFAULT_HEIGHT);
	encodings_init();

	load_settings();

	msgwin_init();
	build_init();
	search_init();
	ui_create_insert_menu_items();
	ui_create_insert_date_menu_items();
	keybindings_init();
	tools_create_insert_custom_command_menu_items();
	notebook_init();
	filetypes_init();
	templates_init();
	navqueue_init();
	document_init_doclist();
	treeviews_init();
	configuration_read_filetype_extensions();
	configuration_read_autocompletions();

	// set window icon
	{
		GdkPixbuf *pb;

		pb = ui_new_pixbuf_from_inline(GEANY_IMAGE_LOGO, FALSE);
		gtk_window_set_icon(GTK_WINDOW(app->window), pb);
		g_object_unref(pb);	// free our reference
	}

	// registering some basic events
	g_signal_connect(G_OBJECT(app->window), "delete_event", G_CALLBACK(on_exit_clicked), NULL);
	g_signal_connect(G_OBJECT(app->window), "key-press-event", G_CALLBACK(keybindings_got_event), NULL);
	g_signal_connect(G_OBJECT(app->toolbar), "button-press-event", G_CALLBACK(toolbar_popup_menu), NULL);
	g_signal_connect(G_OBJECT(lookup_widget(app->window, "textview_scribble")),
							"motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	g_signal_connect(G_OBJECT(lookup_widget(app->window, "entry1")),
							"motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	g_signal_connect(G_OBJECT(lookup_widget(app->window, "entry_goto_line")),
							"motion-notify-event", G_CALLBACK(on_motion_event), NULL);

#ifdef HAVE_VTE
	vte_init();
#endif
	ui_create_recent_menu();

	msgwin_status_add(_("This is Geany %s."), VERSION);
	if (config_dir_result != 0)
		msgwin_status_add(_("Configuration directory could not be created (%s)."),
			g_strerror(config_dir_result));

	// apply all configuration options
	apply_settings();

#ifdef HAVE_PLUGINS
	// load any enabled plugins before we open any documents
	if (want_plugins)
		plugins_init();
#endif

	// load any command line files or session files
	main_status.opening_session_files = TRUE;
	if (! open_cl_files(argc, argv))
	{
		if (prefs.load_session && cl_options.load_session)
		{
			load_project_file();

			// load session files
			if (! configuration_open_files())
			{
				ui_update_popup_copy_items(-1);
				ui_update_popup_reundo_items(-1);
			}
		}
	}
	main_status.opening_session_files = FALSE;

	// open a new file if no other file was opened
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) == 0)
		document_new_file(NULL, NULL, NULL);

	ui_document_buttons_update();
	ui_save_buttons_toggle(FALSE);

	idx = document_get_cur_idx();
	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
	treeviews_select_openfiles_item(idx);
	build_menu_update(idx);
	treeviews_update_tag_list(idx, FALSE);

	// finally realize the window to show the user what we have done
	gtk_widget_show(app->window);
	main_status.main_window_realized = TRUE;

	configuration_apply_settings();

#ifdef HAVE_SOCKET
	// register the callback of socket input
	if (! socket_info.ignore_socket && socket_info.lock_socket > 0)
	{
		socket_info.read_ioc = g_io_channel_unix_new(socket_info.lock_socket);
		socket_info.lock_socket_tag = g_io_add_watch(socket_info.read_ioc,
						G_IO_IN|G_IO_PRI|G_IO_ERR, socket_lock_input_cb, app->window);
	}
#endif

	//g_timeout_add(0, (GSourceFunc)destroyapp, NULL); // useful for start time tests
	gtk_main();
	return 0;
}


void main_quit()
{
	geany_debug("Quitting...");

#ifdef HAVE_SOCKET
	socket_finalize();
#endif

#ifdef HAVE_PLUGINS
	if (want_plugins)
		plugins_free();
#endif
	navqueue_free();
	keybindings_free();
	filetypes_save_commands();
	filetypes_free_types();
	styleset_free_styles();
	templates_free_templates();
	msgwin_finalize();
	search_finalize();
	build_finalize();
	document_finalize();
	symbols_finalize();
	editor_finalize();
	if (app->project != NULL) project_close();

	tm_workspace_free(TM_WORK_OBJECT(app->tm_workspace));
	g_free(app->configdir);
	g_free(app->datadir);
	g_free(app->docdir);
	g_free(prefs.default_open_path);
	g_free(ui_prefs.custom_date_format);
	g_free(prefs.editor_font);
	g_free(prefs.tagbar_font);
	g_free(prefs.msgwin_font);
	g_free(editor_prefs.long_line_color);
	g_free(prefs.context_action_cmd);
	g_free(prefs.template_developer);
	g_free(prefs.template_company);
	g_free(prefs.template_mail);
	g_free(prefs.template_initial);
	g_free(prefs.template_version);
	g_free(prefs.tools_make_cmd);
	g_free(prefs.tools_term_cmd);
	g_free(prefs.tools_browser_cmd);
	g_free(prefs.tools_print_cmd);
	g_free(prefs.tools_grep_cmd);
	g_strfreev(ui_prefs.custom_commands);
	while (! g_queue_is_empty(ui_prefs.recent_queue))
	{
		g_free(g_queue_pop_tail(ui_prefs.recent_queue));
	}
	g_queue_free(ui_prefs.recent_queue);

	if (ui_widgets.prefs_dialog && GTK_IS_WIDGET(ui_widgets.prefs_dialog)) gtk_widget_destroy(ui_widgets.prefs_dialog);
	if (ui_widgets.save_filesel && GTK_IS_WIDGET(ui_widgets.save_filesel)) gtk_widget_destroy(ui_widgets.save_filesel);
	if (ui_widgets.open_filesel && GTK_IS_WIDGET(ui_widgets.open_filesel)) gtk_widget_destroy(ui_widgets.open_filesel);
	if (ui_widgets.open_fontsel && GTK_IS_WIDGET(ui_widgets.open_fontsel)) gtk_widget_destroy(ui_widgets.open_fontsel);
	if (ui_widgets.open_colorsel && GTK_IS_WIDGET(ui_widgets.open_colorsel)) gtk_widget_destroy(ui_widgets.open_colorsel);
	if (tv.default_tag_tree && GTK_IS_WIDGET(tv.default_tag_tree))
	{
		g_object_unref(tv.default_tag_tree);
		gtk_widget_destroy(tv.default_tag_tree);
	}
#ifdef HAVE_VTE
	if (vte_info.have_vte) vte_close();
	g_free(vte_info.lib_vte);
	g_free(vte_info.dir);
#endif
	gtk_widget_destroy(app->window);

	// destroy popup menus
	if (app->popup_menu && GTK_IS_WIDGET(app->popup_menu))
					gtk_widget_destroy(app->popup_menu);
	if (ui_widgets.toolbar_menu && GTK_IS_WIDGET(ui_widgets.toolbar_menu))
					gtk_widget_destroy(ui_widgets.toolbar_menu);
	if (tv.popup_taglist && GTK_IS_WIDGET(tv.popup_taglist))
					gtk_widget_destroy(tv.popup_taglist);
	if (tv.popup_openfiles && GTK_IS_WIDGET(tv.popup_openfiles))
					gtk_widget_destroy(tv.popup_openfiles);
	if (msgwindow.popup_status_menu && GTK_IS_WIDGET(msgwindow.popup_status_menu))
					gtk_widget_destroy(msgwindow.popup_status_menu);
	if (msgwindow.popup_msg_menu && GTK_IS_WIDGET(msgwindow.popup_msg_menu))
					gtk_widget_destroy(msgwindow.popup_msg_menu);
	if (msgwindow.popup_compiler_menu && GTK_IS_WIDGET(msgwindow.popup_compiler_menu))
					gtk_widget_destroy(msgwindow.popup_compiler_menu);

	g_free(app);

	gtk_main_quit();
}


// malloc compatibility code
#undef malloc
void *malloc(size_t n);

// Allocate an N-byte block of memory from the heap. If N is zero, allocate a 1-byte block.
void *rpl_malloc(size_t n)
{
	if (n == 0)
		n = 1;
	return malloc(n);
}
