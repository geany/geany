/*
 *      main.c - this file is part of Geany, a fast and lightweight IDE
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

/**
 * @file: main.h
 * Main program-related commands.
 * Handles program initialization and cleanup.
 */

#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "geany.h"
#include <glib/gstdio.h>

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "main.h"
#include "prefix.h"
#include "prefs.h"
#include "interface.h"
#include "support.h"
#include "callbacks.h"
#include "log.h"
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
#include "printing.h"
#include "toolbar.h"
#include "geanyobject.h"

#ifdef HAVE_SOCKET
# include "socket.h"
#endif

#ifdef HAVE_VTE
# include "vte.h"
#endif

#ifndef N_
# define N_(String) (String)
#endif


GeanyApp	*app;
gboolean	ignore_callback;	/* hack workaround for GTK+ toggle button callback problem */

GeanyStatus	 main_status;
CommandLineOptions cl_options;	/* fields initialised in parse_command_line_options */


static gboolean want_plugins;

/* command-line options */
static gboolean debug_mode = FALSE;
static gboolean ignore_global_tags = FALSE;
static gboolean no_msgwin = FALSE;
static gboolean show_version = FALSE;
static gchar *alternate_config = NULL;
#ifdef HAVE_VTE
static gboolean no_vte = FALSE;
static gchar *lib_vte = NULL;
#endif
static gboolean generate_datafiles = FALSE;
static gboolean generate_tags = FALSE;
static gboolean no_preprocessing = FALSE;
static gboolean ft_names = FALSE;
static gboolean print_prefix = FALSE;
#ifdef HAVE_PLUGINS
static gboolean no_plugins = FALSE;
#endif
static gboolean dummy = FALSE;

/* in alphabetical order of short options */
static GOptionEntry entries[] =
{
	{ "column", 0, 0, G_OPTION_ARG_INT, &cl_options.goto_column, N_("Set initial column number for the first opened file (useful in conjunction with --line)"), NULL },
	{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &alternate_config, N_("Use an alternate configuration directory"), NULL },
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug_mode, N_("Runs in debug mode (means being verbose)"), NULL },
	{ "ft-names", 0, 0, G_OPTION_ARG_NONE, &ft_names, N_("Print internal filetype names"), NULL },
	{ "generate-tags", 'g', 0, G_OPTION_ARG_NONE, &generate_tags, N_("Generate global tags file (see documentation)"), NULL },
	{ "no-preprocessing", 'P', 0, G_OPTION_ARG_NONE, &no_preprocessing, N_("Don't preprocess C/C++ files when generating tags"), NULL },
	{ "generate-data-files", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &generate_datafiles, NULL, NULL },
#ifdef HAVE_SOCKET
	{ "new-instance", 'i', 0, G_OPTION_ARG_NONE, &cl_options.new_instance, N_("Don't open files in a running instance, force opening a new instance"), NULL },
#endif
	{ "line", 'l', 0, G_OPTION_ARG_INT, &cl_options.goto_line, N_("Set initial line number for the first opened file"), NULL },
	{ "no-msgwin", 'm', 0, G_OPTION_ARG_NONE, &no_msgwin, N_("Don't show message window at startup"), NULL },
	{ "no-ctags", 'n', 0, G_OPTION_ARG_NONE, &ignore_global_tags, N_("Don't load auto completion data (see documentation)"), NULL },
#ifdef HAVE_PLUGINS
	{ "no-plugins", 'p', 0, G_OPTION_ARG_NONE, &no_plugins, N_("Don't load plugins"), NULL },
#endif
	{ "print-prefix", 0, 0, G_OPTION_ARG_NONE, &print_prefix, N_("Print Geany's installation prefix"), NULL },
	{ "no-session", 's', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &cl_options.load_session, N_("don't load the previous session's files"), NULL },
#ifdef HAVE_VTE
	{ "no-terminal", 't', 0, G_OPTION_ARG_NONE, &no_vte, N_("Don't load terminal support"), NULL },
	{ "vte-lib", 0, 0, G_OPTION_ARG_FILENAME, &lib_vte, N_("Filename of libvte.so"), NULL },
#endif
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, N_("Show version and exit"), NULL },
	{ "dummy", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &dummy, NULL, NULL }, /* for +NNN line number arguments */
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};



/* special things for the initial setup of the checkboxes and related stuff
 * an action on a setting is only performed if the setting is not equal to the program default
 * (all the following code is not perfect but it works for the moment) */
static void apply_settings(void)
{
	ui_update_fold_items();

	/* toolbar, message window and sidebar are by default visible, so don't change it if it is true */
	if (! toolbar_prefs.visible)
	{
		ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_show_toolbar1")), FALSE);
		gtk_widget_hide(main_widgets.toolbar);
		ignore_callback = FALSE;
	}
	if (! ui_prefs.msgwindow_visible)
	{
		ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_show_messages_window1")), FALSE);
		gtk_widget_hide(ui_lookup_widget(main_widgets.window, "scrolledwindow1"));
		ignore_callback = FALSE;
	}
	if (! ui_prefs.sidebar_visible)
	{
		ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "menu_show_sidebar1")), FALSE);
		ignore_callback = FALSE;
	}
	ui_sidebar_show_hide();
	/* sets the icon style of the toolbar */
	switch (toolbar_prefs.icon_style)
	{
		case GTK_TOOLBAR_BOTH:
		{
			/*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "images_and_text1")), TRUE);*/
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(ui_widgets.toolbar_menu, "images_and_text2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_ICONS:
		{
			/*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "images_only1")), TRUE);*/
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(ui_widgets.toolbar_menu, "images_only2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_TEXT:
		{
			/*gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(main_widgets.window, "text_only1")), TRUE);*/
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(ui_lookup_widget(ui_widgets.toolbar_menu, "text_only2")), TRUE);
			break;
		}
	}
	gtk_toolbar_set_style(GTK_TOOLBAR(main_widgets.toolbar), toolbar_prefs.icon_style);

	/* sets the icon size of the toolbar, use user preferences (.gtkrc) if not set */
	if (toolbar_prefs.icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR ||
		toolbar_prefs.icon_size == GTK_ICON_SIZE_LARGE_TOOLBAR)
	{
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(main_widgets.toolbar), toolbar_prefs.icon_size);
	}
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(main_widgets.toolbar), toolbar_prefs.icon_size);

	ui_update_view_editor_menu_items();

	/* interprets the saved window geometry */
	if (prefs.save_winpos && ui_prefs.geometry[0] != -1)
	{
		gtk_window_move(GTK_WINDOW(main_widgets.window), ui_prefs.geometry[0], ui_prefs.geometry[1]);
		gtk_window_set_default_size(GTK_WINDOW(main_widgets.window), ui_prefs.geometry[2], ui_prefs.geometry[3]);
		if (ui_prefs.geometry[4] == 1)
			gtk_window_maximize(GTK_WINDOW(main_widgets.window));
	}

	/* hide statusbar if desired */
	if (! interface_prefs.statusbar_visible)
	{
		gtk_widget_hide(ui_widgets.statusbar);
	}

	/* set the tab placements of the notebooks */
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(main_widgets.notebook), interface_prefs.tab_pos_editor);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(msgwindow.notebook), interface_prefs.tab_pos_msgwin);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(main_widgets.sidebar_notebook), interface_prefs.tab_pos_sidebar);

	/* whether to show notebook tabs or not */
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(main_widgets.notebook), interface_prefs.show_notebook_tabs);
}


static void main_init(void)
{
	/* inits */
	main_widgets.window				= NULL;
	app->project			= NULL;
	ui_widgets.open_fontsel		= NULL;
	ui_widgets.open_colorsel	= NULL;
	ui_widgets.open_filesel		= NULL;
	ui_widgets.save_filesel		= NULL;
	ui_widgets.prefs_dialog		= NULL;
	tv.default_tag_tree	= NULL;
	main_status.main_window_realized= FALSE;
	file_prefs.tab_order_ltr		= FALSE;
	main_status.quitting			= FALSE;
	ignore_callback	= FALSE;
	app->tm_workspace		= tm_get_workspace();
	ui_prefs.recent_queue				= g_queue_new();
	main_status.opening_session_files		= FALSE;

	main_widgets.window = create_window1();
	ui_widgets.recent_files_menuitem = ui_lookup_widget(main_widgets.window, "recent_files1");
	ui_widgets.recent_files_menu_menubar = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(ui_widgets.recent_files_menuitem),
							ui_widgets.recent_files_menu_menubar);

	/* store important pointers for later reference */
	main_widgets.toolbar = toolbar_init();
	main_widgets.sidebar_notebook = ui_lookup_widget(main_widgets.window, "notebook3");
	main_widgets.notebook = ui_lookup_widget(main_widgets.window, "notebook1");
	main_widgets.editor_menu = create_edit_menu1();
	main_widgets.tools_menu = ui_lookup_widget(main_widgets.window, "tools1_menu");

	ui_widgets.toolbar_menu = create_toolbar_popup_menu1();
	ui_init();

	/* set widget names for matching with .gtkrc-2.0 */
	gtk_widget_set_name(main_widgets.window, "GeanyMainWindow");
	gtk_widget_set_name(ui_widgets.toolbar_menu, "GeanyToolbarMenu");
	gtk_widget_set_name(main_widgets.editor_menu, "GeanyEditMenu");

#if ! GTK_CHECK_VERSION(2, 10, 0)
	/* hide Page setup menu item, it isn't supported with non-GTK printing */
	gtk_widget_hide(ui_widgets.print_page_setup);
#endif
}


const gchar *main_get_version_string(void)
{
	static gchar full[] = VERSION " (svn >= " REVISION ")";

	if (utils_str_equal(REVISION, "-1"))
		return VERSION;
	else
		return full;
}


/* get the full file path of a command-line argument
 * N.B. the result should be freed and may contain '/../' or '/./ ' */
gchar *main_get_argv_filename(const gchar *filename)
{
	gchar *result;

	if (g_path_is_absolute(filename))
		result = g_strdup(filename);
	else
	{
		/* use current dir */
		gchar *cur_dir = g_get_current_dir();

		result = g_strjoin(
			G_DIR_SEPARATOR_S, cur_dir, filename, NULL);
		g_free(cur_dir);
	}
	return result;
}


/* get a :line:column specifier from the end of a filename (if present),
 * return the line/column values, and remove the specifier from the string
 * (Note that *line and *column must both be set to -1 initially) */
static void get_line_and_column_from_filename(gchar *filename, gint *line, gint *column)
{
	gsize i;
	gint colon_count = 0;
	gboolean have_number = FALSE;
	gsize len;

	g_assert(*line == -1 && *column == -1);

	if (! NZV(filename))
		return;

	len = strlen(filename);
	for (i = len - 1; i >= 1; i--)
	{
		gboolean is_colon = filename[i] == ':';
		gboolean is_digit = g_ascii_isdigit(filename[i]);

		if (! is_colon && ! is_digit)
			break;

		if (is_colon)
		{
			if (++colon_count > 1)
				break;	/* bail on 2+ colons in a row */
		}
		else
			colon_count = 0;

		if (is_digit)
			have_number = TRUE;

		if (is_colon && have_number)
		{
			gint number = atoi(&filename[i + 1]);

			filename[i] = '\0';
			have_number = FALSE;

			*column = *line;
			*line = number;
		}

		if (*column >= 0)
			break;	/* line and column are set, so we're done */
	}
}


static void setup_paths(void)
{
	gchar *data_dir;
	gchar *doc_dir;

	/* set paths */
#ifdef G_OS_WIN32
	/* use the installation directory(the one where geany.exe is located) as the base for the
	 * documentation and data files */
	gchar *install_dir = g_win32_get_package_installation_directory(NULL, NULL);

	data_dir = g_strconcat(install_dir, "\\data", NULL); /* e.g. C:\Program Files\geany\data */
	doc_dir = g_strconcat(install_dir, "\\doc", NULL);

	g_free(install_dir);
#else
	data_dir = g_strconcat(GEANY_DATADIR, "/geany/", NULL); /* e.g. /usr/share/geany */
	doc_dir = g_strconcat(GEANY_DOCDIR, "/html/", NULL);
#endif

	/* convert path names to locale encoding */
	app->datadir = utils_get_locale_from_utf8(data_dir);
	app->docdir = utils_get_locale_from_utf8(doc_dir);

	g_free(data_dir);
	g_free(doc_dir);
}


/**
 *  Initialises the gettext translation system.
 *  This is a convenience function to set up gettext for internationalisation support
 *  in external plugins. You should call this function early in @ref plugin_init().
 *  If the macro HAVE_LOCALE_H is defined, @a setlocale(LC_ALL, "") is called.
 *  The codeset for the mesaage translations is set to UTF-8.
 *
 *  Note that this function only setup the gettext textdomain for you. You still have
 *  to adjust the build system of your plugin to get internationalisation support
 *  working properly.
 *
 *  @param locale_dir The location where the translation files should be searched. This is
 *                    usually the @a LOCALEDIR macro, defined by the build system.
 *                    E.g. $prefix/share/locale.
 *                    Only used on non-Windows systems. On Windows, the directory is determined
 *                    by @c g_win32_get_package_installation_directory().
 *  @param package The package name, usually this is the @a GETTEXT_PACKAGE macro,
 *                 defined by the build system.
 *
 *  @since 0.16
 **/
void main_locale_init(const gchar *locale_dir, const gchar *package)
{
	gchar *l_locale_dir = NULL;

#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
#endif

#ifdef G_OS_WIN32
	gchar *install_dir = g_win32_get_package_installation_directory(NULL, NULL);
	/* e.g. C:\Program Files\geany\lib\locale */
	l_locale_dir = g_strconcat(install_dir, "\\share\\locale", NULL);
	g_free(install_dir);
#else
	l_locale_dir = g_strdup(locale_dir);
#endif

	bindtextdomain(package, l_locale_dir);
	bind_textdomain_codeset(package, "UTF-8");
	textdomain(package);
	g_free(l_locale_dir);
}


static void print_filetypes(void)
{
	guint i;

	filetypes_init_types();
	printf("Geany's internal filetype names:\n");

	for (i = 0; i < filetypes_array->len; i++)
	{
		printf("%s\n", filetypes[i]->name);
	}
	filetypes_free_types();
}


static void parse_command_line_options(gint *argc, gchar ***argv)
{
	GError *error = NULL;
	GOptionContext *context;
	gint i;
	CommandLineOptions def_clo = {FALSE, TRUE, -1, -1, FALSE};

	/* first initialise cl_options fields with default values */
	cl_options = def_clo;

	/* the GLib option parser can't handle the +NNN (line number) option,
	 * so we grab that here and replace it with a no-op */
	for (i = 1; i < (*argc); i++)
	{
		if ((*argv)[i][0] != '+')
			continue;

		cl_options.goto_line = atoi((*argv)[i] + 1);
		(*argv)[i] = "--dummy";
	}

	context = g_option_context_new(_("[FILES...]"));
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(g_option_context_get_main_group(context), GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(FALSE));
	g_option_context_parse(context, argc, argv, &error);
	g_option_context_free(context);

	if (error != NULL)
	{
		g_printerr("Geany: %s\n", error->message);
		g_error_free(error);
		exit(1);
	}

	if (show_version)
	{
		printf(PACKAGE " %s ", main_get_version_string());
		printf(_("(built on %s with GTK %d.%d.%d, GLib %d.%d.%d)"),
				__DATE__, GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
				GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
		printf("\n");

		exit(0);
	}

	if (print_prefix)
	{
		printf("%s\n", GEANY_PREFIX);
		printf("%s\n", GEANY_DATADIR);
		printf("%s\n", GEANY_LIBDIR);
		printf("%s\n", GEANY_LOCALEDIR);
		exit(0);
	}

	app->debug_mode = debug_mode;

#ifdef G_OS_WIN32
	win32_init_debug_code();
#endif

	if (alternate_config)
	{
		geany_debug("alternate config: %s", alternate_config);
		app->configdir = alternate_config;
	}
	else
	{
		app->configdir = g_build_filename(g_get_user_config_dir(), "geany", NULL);
	}
	setptr(app->configdir, tm_get_real_path(app->configdir));

#ifdef GEANY_DEBUG
	if (generate_datafiles)
	{
		filetypes_init_types();
		configuration_generate_data_files();	/* currently only filetype_extensions.conf */
		exit(0);
	}
#endif
	if (generate_tags)
	{
		gboolean ret;

		filetypes_init_types();
		filetypes_read_extensions();	/* needed for *.lang.tags filetype matching */
		ret = symbols_generate_global_tags(*argc, *argv, ! no_preprocessing);
		exit(ret);
	}

	if (ft_names)
	{
		print_filetypes();
		exit(0);
	}

#ifdef HAVE_SOCKET
	socket_info.ignore_socket = cl_options.new_instance;
#endif
#ifdef HAVE_VTE
	vte_info.lib_vte = lib_vte;
#endif
	cl_options.ignore_global_tags = ignore_global_tags;

	if (! gtk_init_check(NULL, NULL))
	{	/* check whether we have a valid X display and exit if not */
		g_printerr("Geany: cannot open display\n");
		exit(1);
	}
}


#ifdef G_OS_WIN32
# define DIR_SEP "\\" /* on Windows we need an additional dir separator */
#else
# define DIR_SEP ""
#endif

static gint create_config_dir(void)
{
	gint saved_errno = 0;
	gchar *conf_file = g_build_filename(app->configdir, "geany.conf", NULL);
	gchar *filedefs_dir = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, NULL);

	gchar *templates_dir = g_build_filename(app->configdir, GEANY_TEMPLATES_SUBDIR, NULL);

	if (! g_file_test(app->configdir, G_FILE_TEST_EXISTS))
	{
		/* if we are *not* using an alternate config directory, we check whether the old one
		 * in ~/.geany still exists and try to move it */
		if (alternate_config == NULL)
		{
			gchar *old_dir = g_build_filename(g_get_home_dir(), ".geany", NULL);
			/* move the old config dir if it exists */
			if (g_file_test(old_dir, G_FILE_TEST_EXISTS))
			{
				if (! dialogs_show_question_full(main_widgets.window,
					GTK_STOCK_YES, GTK_STOCK_QUIT, _("Move it now?"),
					"%s",
					_("Geany needs to move your old configuration directory before starting.")))
					exit(0);

				if (g_rename(old_dir, app->configdir) == 0)
				{
					dialogs_show_msgbox(GTK_MESSAGE_INFO,
						_("Your configuration directory has been successfully moved from \"%s\" to \"%s\"."),
						old_dir, app->configdir);
					g_free(old_dir);
					return 0;
				}
				else
				{
					dialogs_show_msgbox(GTK_MESSAGE_WARNING,
						/* for translators: the third %s in brackets is the error message which
						 * describes why moving the dir didn't work */
						_("Your old configuration directory \"%s\" could not be moved to \"%s\" (%s). "
						  "Please move manually the directory to the new location."),
						old_dir, app->configdir, g_strerror(errno));
				}
			}
			g_free(old_dir);
		}
		geany_debug("creating config directory %s", app->configdir);
		saved_errno = utils_mkdir(app->configdir, FALSE);
	}

	if (saved_errno == 0 && ! g_file_test(conf_file, G_FILE_TEST_EXISTS))
	{	/* check whether geany.conf can be written */
		saved_errno = utils_is_file_writeable(app->configdir);
	}

	/* make subdir for filetype definitions */
	if (saved_errno == 0)
	{
		gchar *filedefs_readme = g_build_filename(app->configdir,
					GEANY_FILEDEFS_SUBDIR, "filetypes.README", NULL);

		if (! g_file_test(filedefs_dir, G_FILE_TEST_EXISTS))
		{
			saved_errno = utils_mkdir(filedefs_dir, FALSE);
		}
		if (saved_errno == 0 && ! g_file_test(filedefs_readme, G_FILE_TEST_EXISTS))
		{
			gchar *text = g_strconcat(
"Copy files from ", app->datadir, " to this directory to overwrite "
"them. To use the defaults, just delete the file in this directory.\nFor more information read "
"the documentation (in ", app->docdir, DIR_SEP "index.html or visit " GEANY_HOMEPAGE ").", NULL);
			utils_write_file(filedefs_readme, text);
			g_free(text);
		}
		g_free(filedefs_readme);
	}

	/* make subdir for template files */
	if (saved_errno == 0)
	{
		gchar *templates_readme = g_build_filename(app->configdir, GEANY_TEMPLATES_SUBDIR,
						"templates.README", NULL);

		if (! g_file_test(templates_dir, G_FILE_TEST_EXISTS))
		{
			saved_errno = utils_mkdir(templates_dir, FALSE);
		}
		if (saved_errno == 0 && ! g_file_test(templates_readme, G_FILE_TEST_EXISTS))
		{
			gchar *text = g_strconcat(
"There are several template files in this directory. For these templates you can use wildcards.\n\
For more information read the documentation (in ", app->docdir, DIR_SEP "index.html or visit " GEANY_HOMEPAGE ").",
					NULL);
			utils_write_file(templates_readme, text);
			g_free(text);
		}
		g_free(templates_readme);
	}

	g_free(filedefs_dir);
	g_free(templates_dir);
	g_free(conf_file);

	return saved_errno;
}


/* Returns 0 if config dir is OK. */
static gint setup_config_dir(void)
{
	gint mkdir_result = 0;

	/* convert configdir to locale encoding to avoid troubles */
	setptr(app->configdir, utils_get_locale_from_utf8(app->configdir));

	mkdir_result = create_config_dir();
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


/* Used for command-line arguments at startup or from socket.
 * this will strip any :line:col filename suffix from locale_filename */
gboolean main_handle_filename(gchar *locale_filename)
{
	GeanyDocument *doc;
	gint line = -1, column = -1;

	g_return_val_if_fail(locale_filename, FALSE);

	get_line_and_column_from_filename(locale_filename, &line, &column);
	if (line >= 0)
		cl_options.goto_line = line;
	if (column >= 0)
		cl_options.goto_column = column;

	if (g_file_test(locale_filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
	{
		doc = document_open_file(locale_filename, FALSE, NULL, NULL);
		/* add recent file manually if opening_session_files is set */
		if (doc != NULL && main_status.opening_session_files)
			ui_add_recent_file(doc->file_name);
		return TRUE;
	}
	else if (file_prefs.cmdline_new_files)
	{	/* create new file with the given filename */
		gchar *utf8_filename = utils_get_utf8_from_locale(locale_filename);

		doc = document_new_file(utf8_filename, NULL, NULL);
		if (doc != NULL)
			ui_add_recent_file(doc->file_name);
		g_free(utf8_filename);
		return TRUE;
	}
	return FALSE;
}


/* open files from command line */
static gboolean open_cl_files(gint argc, gchar **argv)
{
	gint i;

	if (argc <= 1) return FALSE;

	for (i = 1; i < argc; i++)
	{
		gchar *filename = main_get_argv_filename(argv[i]);
#ifdef G_OS_WIN32
		/* It seems argv elements are encoded in CP1252 on a German Windows */
		setptr(filename, g_locale_to_utf8(filename, -1, NULL, NULL, NULL));
#endif
		if (filename && !main_handle_filename(filename))
		{
			const gchar *msg = _("Could not find file '%s'.");

			g_printerr(msg, filename);	/* also print to the terminal */
			g_printerr("\n");
			ui_set_statusbar(TRUE, msg, filename);
		}
		g_free(filename);
	}
	return TRUE;
}


static void load_session_project_file(void)
{
	gchar *locale_filename;

	g_return_if_fail(project_prefs.session_file != NULL);

	locale_filename = utils_get_locale_from_utf8(project_prefs.session_file);

	if (*locale_filename)
		project_load_file(locale_filename);

	g_free(locale_filename);
	g_free(project_prefs.session_file);	/* no longer needed */
}


static void load_settings(void)
{
	configuration_load();
	/* let cmdline options overwrite configuration settings */
#ifdef HAVE_VTE
	vte_info.have_vte = (no_vte) ? FALSE : vte_info.load_vte;
#endif
	if (no_msgwin) ui_prefs.msgwindow_visible = FALSE;

#ifdef HAVE_PLUGINS
	want_plugins = prefs.load_plugins && !no_plugins;
#endif
}


static void load_startup_files(gint argc, gchar **argv)
{
	gboolean load_project_from_cl = FALSE;

	/* ATM when opening a project file any other filenames are ignored */
	load_project_from_cl = (argc > 1) && g_str_has_suffix(argv[1], ".geany");
	if (load_project_from_cl && argc > 2)
		g_print("Ignoring extra filenames after %s", argv[1]);

	if (load_project_from_cl || ! open_cl_files(argc, argv))
	{
		if (prefs.load_session)
		{
			if (load_project_from_cl)
				project_load_file(argv[1]);
			else
			if (cl_options.load_session && !cl_options.new_instance)
				load_session_project_file();

			/* when we want a new instance, we still load project session files unless -s
			 * was passed */
			if (!cl_options.load_session || (!load_project_from_cl && cl_options.new_instance))
				return;

			/* load session files into tabs, as they are found in the session_files variable */
			configuration_open_files();

			if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) == 0)
			{
				ui_update_popup_copy_items(NULL);
				ui_update_popup_reundo_items(NULL);
			}
		}
	}
}


gint main(gint argc, gchar **argv)
{
	GeanyDocument *doc;
	gint config_dir_result;

	log_handlers_init();

	app = g_new0(GeanyApp, 1);
	memset(&main_status, 0, sizeof(GeanyStatus));
	memset(&prefs, 0, sizeof(GeanyPrefs));
	memset(&interface_prefs, 0, sizeof(GeanyInterfacePrefs));
	memset(&toolbar_prefs, 0, sizeof(GeanyToolbarPrefs));
	memset(&file_prefs, 0, sizeof(GeanyFilePrefs));
	memset(&search_prefs, 0, sizeof(GeanySearchPrefs));
	memset(&tool_prefs, 0, sizeof(GeanyToolPrefs));
	memset(&template_prefs, 0, sizeof(GeanyTemplatePrefs));
	memset(&ui_prefs, 0, sizeof(UIPrefs));
	memset(&ui_widgets, 0, sizeof(UIWidgets));

	setup_paths();
#ifdef ENABLE_NLS
	main_locale_init(GEANY_LOCALEDIR, GETTEXT_PACKAGE);
#endif
	parse_command_line_options(&argc, &argv);

	signal(SIGTERM, signal_cb);
#ifdef G_OS_UNIX
	/* SIGQUIT is used to kill spawned children and we get also this signal, so ignore */
	signal(SIGQUIT, SIG_IGN);
	/* ignore SIGPIPE signal for preventing sudden death of program */
	signal(SIGPIPE, SIG_IGN);
#endif

	config_dir_result = setup_config_dir();
#ifdef HAVE_SOCKET
    /* check and create (unix domain) socket for remote operation */
	if (! socket_info.ignore_socket)
	{
		socket_info.lock_socket = -1;
		socket_info.lock_socket_tag = 0;
		socket_info.lock_socket = socket_init(argc, argv);
		if (socket_info.lock_socket == -2)
		{
			/* Socket exists */
			if (argc > 1)	/* filenames were sent to first instance, so quit */
			{
				g_free(app->configdir);
				g_free(app->datadir);
				g_free(app->docdir);
				g_free(app);
				return 0;
			}
			/* Start a new instance if no command line strings were passed */
			socket_info.ignore_socket = TRUE;
			cl_options.new_instance = TRUE;
		}
	}
#endif

	geany_debug("Geany %s, GTK+ %u.%u.%u, GLib %u.%u.%u",
		main_get_version_string(),
		gtk_major_version, gtk_minor_version, gtk_micro_version,
		glib_major_version, glib_minor_version, glib_micro_version);

	/* create the object so Geany signals can be connected in init() functions */
	geany_object = geany_object_new();

	/* inits */
	main_init();
	gtk_widget_set_size_request(main_widgets.window, GEANY_WINDOW_MINIMAL_WIDTH, GEANY_WINDOW_MINIMAL_HEIGHT);
	gtk_window_set_default_size(GTK_WINDOW(main_widgets.window), GEANY_WINDOW_DEFAULT_WIDTH, GEANY_WINDOW_DEFAULT_HEIGHT);

	encodings_init();
	editor_init();

	configuration_init();
	search_init();
	load_settings();

	msgwin_init();
	build_init();
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
	symbols_init();
	filetypes_read_extensions();
	editor_snippets_init();

	/* set window icon */
	{
		GdkPixbuf *pb;

		pb = ui_new_pixbuf_from_inline(GEANY_IMAGE_LOGO);
		gtk_window_set_icon(GTK_WINDOW(main_widgets.window), pb);
		g_object_unref(pb);	/* free our reference */
	}

	/* registering some basic events */
	{
		GtkWidget *entry;

		g_signal_connect(main_widgets.window, "delete-event", G_CALLBACK(on_exit_clicked), NULL);
		g_signal_connect(main_widgets.window, "key-press-event", G_CALLBACK(keybindings_got_event), NULL);
		g_signal_connect(main_widgets.toolbar, "button-press-event", G_CALLBACK(toolbar_popup_menu), NULL);

		g_signal_connect(ui_lookup_widget(main_widgets.window, "textview_scribble"),
								"motion-notify-event", G_CALLBACK(on_motion_event), NULL);
		entry = toolbar_get_widget_child_by_name("SearchEntry");
		if (entry != NULL)
			g_signal_connect(entry, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
		entry = toolbar_get_widget_child_by_name("GotoEntry");
		if (entry != NULL)
			g_signal_connect(entry, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
	}
#ifdef HAVE_VTE
	vte_init();
#endif
	ui_create_recent_menu();

	ui_set_statusbar(TRUE, _("This is Geany %s."), main_get_version_string());
	if (config_dir_result != 0)
		ui_set_statusbar(TRUE, _("Configuration directory could not be created (%s)."),
			g_strerror(config_dir_result));

	/* apply all configuration options */
	apply_settings();

#ifdef HAVE_PLUGINS
	/* load any enabled plugins before we open any documents */
	if (want_plugins)
		plugins_init();
#endif

	/* load keybinding settings after plugins have added their groups */
	keybindings_load_keyfile();

	/* load any command line files or session files */
	main_status.opening_session_files = TRUE;
	load_startup_files(argc, argv);
	main_status.opening_session_files = FALSE;

	/* open a new file if no other file was opened */
	document_new_file_if_non_open();

	ui_document_buttons_update();
	ui_save_buttons_toggle(FALSE);

	doc = document_get_current();
	gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
	treeviews_select_openfiles_item(doc);
	build_menu_update(doc);
	treeviews_update_tag_list(doc, FALSE);

	/* finally realize the window to show the user what we have done */
	gtk_widget_show(main_widgets.window);
	main_status.main_window_realized = TRUE;

	configuration_apply_settings();

#ifdef HAVE_SOCKET
	/* register the callback of socket input */
	if (! socket_info.ignore_socket && socket_info.lock_socket > 0)
	{
		socket_info.read_ioc = g_io_channel_unix_new(socket_info.lock_socket);
		socket_info.lock_socket_tag = g_io_add_watch(socket_info.read_ioc,
						G_IO_IN|G_IO_PRI|G_IO_ERR, socket_lock_input_cb, main_widgets.window);
	}
#endif

	/*g_timeout_add(0, (GSourceFunc)destroyapp, NULL);*/ /* useful for start time tests*/
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
	highlighting_free_styles();
	templates_free_templates();
	msgwin_finalize();
	search_finalize();
	build_finalize();
	document_finalize();
	symbols_finalize();
	editor_finalize();
	editor_snippets_free();
	encodings_finalize();
	toolbar_finalize();
	configuration_finalize();
	/* must be last */
	log_finalize();

	tm_workspace_free(TM_WORK_OBJECT(app->tm_workspace));
	g_free(app->configdir);
	g_free(app->datadir);
	g_free(app->docdir);
	g_free(prefs.default_open_path);
	g_free(ui_prefs.custom_date_format);
	g_free(interface_prefs.editor_font);
	g_free(interface_prefs.tagbar_font);
	g_free(interface_prefs.msgwin_font);
	g_free(editor_prefs.long_line_color);
	g_free(editor_prefs.comment_toggle_mark);
	g_free(tool_prefs.context_action_cmd);
	g_free(template_prefs.developer);
	g_free(template_prefs.company);
	g_free(template_prefs.mail);
	g_free(template_prefs.initials);
	g_free(template_prefs.version);
	g_free(tool_prefs.make_cmd);
	g_free(tool_prefs.term_cmd);
	g_free(tool_prefs.browser_cmd);
	g_free(tool_prefs.grep_cmd);
	g_free(printing_prefs.external_print_cmd);
	g_free(printing_prefs.page_header_datefmt);
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
	gtk_widget_destroy(main_widgets.window);

	/* destroy popup menus */
	if (main_widgets.editor_menu && GTK_IS_WIDGET(main_widgets.editor_menu))
					gtk_widget_destroy(main_widgets.editor_menu);
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

	g_object_unref(geany_object);
	geany_object = NULL;

	g_free(app);

	gtk_main_quit();
}


/* malloc compatibility code */
#undef malloc
void *malloc(size_t n);

/* Allocate an N-byte block of memory from the heap. If N is zero, allocate a 1-byte block. */
void *rpl_malloc(size_t n)
{
	if (n == 0)
		n = 1;
	return malloc(n);
}


/**
 *  Reloads most of Geany's configuration files without restarting. Currently the following
 *  files are reloaded: all template files, also new file templates and the 'New (with template)'
 *  menus will be updated, Snippets (snippets.conf), filetype extensions (filetype_extensions.conf),
 *  and 'settings' and 'build_settings' sections of the filetype definition files.
 *
 *  Plugins may call this function if they changed any of these files (e.g. a configuration file
 *  editor plugin).
 *
 *  @since 0.15
 **/
void main_reload_configuration(void)
{
	guint i;

	/* reload templates */
	templates_free_templates();
	templates_init();

	/* reload snippets */
	editor_snippets_free();
	editor_snippets_init();

	/* reload filetype extensions */
	filetypes_read_extensions();

	/* save possibly changed commands before re-reading them */
	filetypes_save_commands();

	/* reload filetype configs */
	for (i = 0; i < filetypes_array->len; i++)
	{
		/* filetypes_load_config() will skip not loaded filetypes */
		filetypes_load_config(i, TRUE);
	}

	/* C tag names to ignore */
	symbols_reload_config_files();

	ui_set_statusbar(TRUE, _("Configuration files reloaded."));
}
