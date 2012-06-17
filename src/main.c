/*
 *      main.c - this file is part of Geany, a fast and lightweight IDE
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
#include "sidebar.h"
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


static const gchar geany_lib_versions[] = "GTK %u.%u.%u, GLib %u.%u.%u";

static gboolean want_plugins;

/* command-line options */
static gboolean verbose_mode = FALSE;
static gboolean ignore_global_tags = FALSE;
static gboolean no_msgwin = FALSE;
static gboolean show_version = FALSE;
static gchar *alternate_config = NULL;
#ifdef HAVE_VTE
static gboolean no_vte = FALSE;
static gchar *lib_vte = NULL;
#endif
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
	{ "ft-names", 0, 0, G_OPTION_ARG_NONE, &ft_names, N_("Print internal filetype names"), NULL },
	{ "generate-tags", 'g', 0, G_OPTION_ARG_NONE, &generate_tags, N_("Generate global tags file (see documentation)"), NULL },
	{ "no-preprocessing", 'P', 0, G_OPTION_ARG_NONE, &no_preprocessing, N_("Don't preprocess C/C++ files when generating tags"), NULL },
#ifdef HAVE_SOCKET
	{ "new-instance", 'i', 0, G_OPTION_ARG_NONE, &cl_options.new_instance, N_("Don't open files in a running instance, force opening a new instance"), NULL },
	{ "socket-file", 0, 0, G_OPTION_ARG_FILENAME, &cl_options.socket_filename, N_("Use this socket filename for communication with a running Geany instance"), NULL },
	{ "list-documents", 0, 0, G_OPTION_ARG_NONE, &cl_options.list_documents, N_("Return a list of open documents in a running Geany instance"), NULL },
#endif
	{ "line", 'l', 0, G_OPTION_ARG_INT, &cl_options.goto_line, N_("Set initial line number for the first opened file"), NULL },
	{ "no-msgwin", 'm', 0, G_OPTION_ARG_NONE, &no_msgwin, N_("Don't show message window at startup"), NULL },
	{ "no-ctags", 'n', 0, G_OPTION_ARG_NONE, &ignore_global_tags, N_("Don't load auto completion data (see documentation)"), NULL },
#ifdef HAVE_PLUGINS
	{ "no-plugins", 'p', 0, G_OPTION_ARG_NONE, &no_plugins, N_("Don't load plugins"), NULL },
#endif
	{ "print-prefix", 0, 0, G_OPTION_ARG_NONE, &print_prefix, N_("Print Geany's installation prefix"), NULL },
	{ "read-only", 'r', 0, G_OPTION_ARG_NONE, &cl_options.readonly, N_("Open all FILES in read-only mode (see documention)"), NULL },
	{ "no-session", 's', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &cl_options.load_session, N_("Don't load the previous session's files"), NULL },
#ifdef HAVE_VTE
	{ "no-terminal", 't', 0, G_OPTION_ARG_NONE, &no_vte, N_("Don't load terminal support"), NULL },
	{ "vte-lib", 0, 0, G_OPTION_ARG_FILENAME, &lib_vte, N_("Filename of libvte.so"), NULL },
#endif
	{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose_mode, N_("Be verbose"), NULL },
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &show_version, N_("Show version and exit"), NULL },
	{ "dummy", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &dummy, NULL, NULL }, /* for +NNN line number arguments */
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};


static void setup_window_position(void)
{
	/* interprets the saved window geometry */
	if (!prefs.save_winpos)
		return;

	if (ui_prefs.geometry[0] != -1 && ui_prefs.geometry[1] != -1)
		gtk_window_move(GTK_WINDOW(main_widgets.window),
			ui_prefs.geometry[0], ui_prefs.geometry[1]);

	if (ui_prefs.geometry[2] != -1 && ui_prefs.geometry[3] != -1)
		gtk_window_set_default_size(GTK_WINDOW(main_widgets.window),
			ui_prefs.geometry[2], ui_prefs.geometry[3]);

	if (ui_prefs.geometry[4] == 1)
		gtk_window_maximize(GTK_WINDOW(main_widgets.window));
}


/* special things for the initial setup of the checkboxes and related stuff
 * an action on a setting is only performed if the setting is not equal to the program default
 * (all the following code is not perfect but it works for the moment) */
static void apply_settings(void)
{
	ui_update_fold_items();

	/* toolbar, message window and sidebar are by default visible, so don't change it if it is true */
	toolbar_show_hide();
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

	toolbar_apply_settings();
	toolbar_update_ui();

	ui_update_view_editor_menu_items();

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

#ifdef HAVE_VTE
	if (! vte_info.have_vte)
#endif
	{
		gtk_widget_set_sensitive(
			ui_lookup_widget(main_widgets.window, "send_selection_to_vte1"), FALSE);
	}

	if (interface_prefs.sidebar_pos != GTK_POS_LEFT)
		ui_swap_sidebar_pos();
}


static void main_init(void)
{
	/* inits */
	ui_init_builder();

	main_widgets.window				= NULL;
	app->project			= NULL;
	ui_widgets.open_fontsel		= NULL;
	ui_widgets.open_colorsel	= NULL;
	ui_widgets.prefs_dialog		= NULL;
	main_status.main_window_realized = FALSE;
	file_prefs.tab_order_ltr		= FALSE;
	file_prefs.tab_order_beside		= FALSE;
	main_status.quitting			= FALSE;
	ignore_callback	= FALSE;
	app->tm_workspace		= tm_get_workspace();
	ui_prefs.recent_queue				= g_queue_new();
	ui_prefs.recent_projects_queue		= g_queue_new();
	main_status.opening_session_files	= FALSE;

	ui_init_stock_items();

	main_widgets.window = create_window1();

	/* add recent projects to the Project menu */
	ui_widgets.recent_projects_menuitem = ui_lookup_widget(main_widgets.window, "recent_projects1");
	ui_widgets.recent_projects_menu_menubar = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(ui_widgets.recent_projects_menuitem),
							ui_widgets.recent_projects_menu_menubar);

	/* store important pointers for later reference */
	main_widgets.toolbar = toolbar_init();
	main_widgets.sidebar_notebook = ui_lookup_widget(main_widgets.window, "notebook3");
	main_widgets.notebook = ui_lookup_widget(main_widgets.window, "notebook1");
	main_widgets.editor_menu = create_edit_menu1();
	main_widgets.tools_menu = ui_lookup_widget(main_widgets.window, "tools1_menu");
	main_widgets.message_window_notebook = ui_lookup_widget(main_widgets.window, "notebook_info");
	main_widgets.project_menu = ui_lookup_widget(main_widgets.window, "menu_project1_menu");

	ui_widgets.toolbar_menu = create_toolbar_popup_menu1();
	ui_init();

	/* set widget names for matching with .gtkrc-2.0 */
	gtk_widget_set_name(main_widgets.window, "GeanyMainWindow");
	gtk_widget_set_name(ui_widgets.toolbar_menu, "GeanyToolbarMenu");
	gtk_widget_set_name(main_widgets.editor_menu, "GeanyEditMenu");
	gtk_widget_set_name(ui_lookup_widget(main_widgets.window, "menubar1"), "GeanyMenubar");
	gtk_widget_set_name(main_widgets.toolbar, "GeanyToolbar");

	gtk_window_set_default_size(GTK_WINDOW(main_widgets.window),
		GEANY_WINDOW_DEFAULT_WIDTH, GEANY_WINDOW_DEFAULT_HEIGHT);
}


const gchar *main_get_version_string(void)
{
	static gchar full[] = VERSION " (git >= " REVISION ")";

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

	if (g_path_is_absolute(filename) || utils_is_uri(filename))
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

	if (G_UNLIKELY(! NZV(filename)))
		return;

	/* allow to open files like "test:0" */
	if (g_file_test(filename, G_FILE_TEST_EXISTS))
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
	gchar *install_dir = win32_get_installation_dir();

	data_dir = g_strconcat(install_dir, "\\data", NULL); /* e.g. C:\Program Files\geany\data */
	doc_dir = g_strconcat(install_dir, "\\doc", NULL);

	g_free(install_dir);
#else
	data_dir = g_strconcat(GEANY_DATADIR, "/geany", NULL); /* e.g. /usr/share/geany */
	doc_dir = g_strconcat(GEANY_DOCDIR, "/html", NULL);
#endif

	/* convert path names to locale encoding */
	app->datadir = utils_get_locale_from_utf8(data_dir);
	app->docdir = utils_get_locale_from_utf8(doc_dir);

	g_free(data_dir);
	g_free(doc_dir);
}


/**
 *  Checks whether the main window has been realized.
 *  This is an easy indicator whether Geany is right now starting up (main window is not
 *  yet realized) or whether it has finished the startup process (main window is realized).
 *  This is because the main window is realized (i.e. actually drawn on the screen) at the
 *  end of the startup process.
 *
 *  @note Maybe you want to use the @link pluginsignals.c @c "geany-startup-complete" signal @endlink
 *        to get notified about the completed startup process.
 *
 *  @return @c TRUE if the Geany main window has been realized or @c FALSE otherwise.
 *
 *  @since 0.19
 **/
gboolean main_is_realized(void)
{
	return main_status.main_window_realized;
}


/**
 *  Initialises the gettext translation system.
 *  This is a convenience function to set up gettext for internationalisation support
 *  in external plugins. You should call this function early in @ref plugin_init().
 *  If the macro HAVE_LOCALE_H is defined, @c setlocale(LC_ALL, "") is called.
 *  The codeset for the message translations is set to UTF-8.
 *
 *  Note that this function only setups the gettext textdomain for you. You still have
 *  to adjust the build system of your plugin to get internationalisation support
 *  working properly.
 *
 *  If you have already used @ref PLUGIN_SET_TRANSLATABLE_INFO() you
 *  don't need to call main_locale_init() again as it has already been done.
 *
 *  @param locale_dir The location where the translation files should be searched. This is
 *                    usually the @c LOCALEDIR macro, defined by the build system.
 *                    E.g. @c $prefix/share/locale.
 *                    Only used on non-Windows systems. On Windows, the directory is determined
 *                    by @c g_win32_get_package_installation_directory().
 *  @param package The package name, usually this is the @c GETTEXT_PACKAGE macro,
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
	{
		gchar *install_dir = win32_get_installation_dir();
		/* e.g. C:\Program Files\geany\lib\locale */
		l_locale_dir = g_strconcat(install_dir, "\\share\\locale", NULL);
		g_free(install_dir);
	}
#else
	l_locale_dir = g_strdup(locale_dir);
#endif

	bindtextdomain(package, l_locale_dir);
	bind_textdomain_codeset(package, "UTF-8");
	g_free(l_locale_dir);
}


static void print_filetypes(void)
{
	const GSList *list, *node;

	filetypes_init_types();
	printf("Geany's filetype names:\n");

	list = filetypes_get_sorted_by_name();
	foreach_slist(node, list)
	{
		GeanyFiletype *ft = node->data;

		printf("%s\n", ft->name);
	}
	filetypes_free_types();
}


static void wait_for_input_on_windows(void)
{
#ifdef G_OS_WIN32
	if (verbose_mode)
	{
		geany_debug("Press any key to continue");
		getchar();
	}
#endif
}


static void parse_command_line_options(gint *argc, gchar ***argv)
{
	GError *error = NULL;
	GOptionContext *context;
	gint i;
	CommandLineOptions def_clo = {FALSE, NULL, TRUE, -1, -1, FALSE, FALSE, FALSE};

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

	app->debug_mode = verbose_mode;
	if (app->debug_mode)
	{
		/* Since GLib 2.32 messages logged with levels INFO and DEBUG aren't output by the
		 * default log handler unless the G_MESSAGES_DEBUG environment variable contains the
		 * domain of the message or is set to the special value "all" */
		g_setenv("G_MESSAGES_DEBUG", "all", FALSE);
	}

#ifdef G_OS_WIN32
	win32_init_debug_code();
#endif

	if (show_version)
	{
		printf(PACKAGE " %s (", main_get_version_string());
		/* note for translators: library versions are printed after this */
		printf(_("built on %s with "), __DATE__);
		printf(geany_lib_versions,
			GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
			GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
		printf(")\n");
		wait_for_input_on_windows();
		exit(0);
	}

	if (print_prefix)
	{
		printf("%s\n", GEANY_PREFIX);
		printf("%s\n", GEANY_DATADIR);
		printf("%s\n", GEANY_LIBDIR);
		printf("%s\n", GEANY_LOCALEDIR);
		wait_for_input_on_windows();
		exit(0);
	}

	if (alternate_config)
	{
		geany_debug("alternate config: %s", alternate_config);
		app->configdir = alternate_config;
	}
	else
	{
		app->configdir = g_build_filename(g_get_user_config_dir(), "geany", NULL);
	}

	if (generate_tags)
	{
		gboolean ret;

		filetypes_init_types();
		ret = symbols_generate_global_tags(*argc, *argv, ! no_preprocessing);
		filetypes_free_types();
		wait_for_input_on_windows();
		exit(ret);
	}

	if (ft_names)
	{
		print_filetypes();
		wait_for_input_on_windows();
		exit(0);
	}

#ifdef HAVE_SOCKET
	socket_info.ignore_socket = cl_options.new_instance;
	if (cl_options.socket_filename)
	{
		socket_info.file_name = cl_options.socket_filename;
	}
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


static gint create_config_dir(void)
{
	gint saved_errno = 0;
	gchar *conf_file = g_build_filename(app->configdir, "geany.conf", NULL);
	gchar *filedefs_dir = g_build_filename(app->configdir, GEANY_FILEDEFS_SUBDIR, NULL);

	gchar *templates_dir = g_build_filename(app->configdir, GEANY_TEMPLATES_SUBDIR, NULL);

	if (! g_file_test(app->configdir, G_FILE_TEST_EXISTS))
	{
#ifndef G_OS_WIN32
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

				if (! g_file_test(app->configdir, G_FILE_TEST_IS_DIR))
					utils_mkdir(app->configdir, TRUE);

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
#endif
		geany_debug("creating config directory %s", app->configdir);
		saved_errno = utils_mkdir(app->configdir, TRUE);
	}

	if (saved_errno == 0 && ! g_file_test(conf_file, G_FILE_TEST_EXISTS))
	{	/* check whether geany.conf can be written */
		saved_errno = utils_is_file_writable(app->configdir);
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
"the documentation (in ", app->docdir, G_DIR_SEPARATOR_S "index.html or visit " GEANY_HOMEPAGE ").", NULL);
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
For more information read the documentation (in ", app->docdir, G_DIR_SEPARATOR_S "index.html or visit " GEANY_HOMEPAGE ").",
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
	SETPTR(app->configdir, utils_get_locale_from_utf8(app->configdir));

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
	/* make configdir a real path */
	if (g_file_test(app->configdir, G_FILE_TEST_EXISTS))
		SETPTR(app->configdir, tm_get_real_path(app->configdir));

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
gboolean main_handle_filename(const gchar *locale_filename)
{
	GeanyDocument *doc;
	gint line = -1, column = -1;
	gchar *filename;

	g_return_val_if_fail(locale_filename, FALSE);

	/* check whether the passed filename is an URI */
	filename = utils_get_path_from_uri(locale_filename);
	if (filename == NULL)
		return FALSE;

	get_line_and_column_from_filename(filename, &line, &column);
	if (line >= 0)
		cl_options.goto_line = line;
	if (column >= 0)
		cl_options.goto_column = column;

	if (g_file_test(filename, G_FILE_TEST_IS_REGULAR))
	{
		doc = document_open_file(filename, cl_options.readonly, NULL, NULL);
		/* add recent file manually if opening_session_files is set */
		if (doc != NULL && main_status.opening_session_files)
			ui_add_recent_document(doc);
		g_free(filename);
		return TRUE;
	}
	else if (file_prefs.cmdline_new_files)
	{	/* create new file with the given filename */
		gchar *utf8_filename = utils_get_utf8_from_locale(filename);

		doc = document_new_file(utf8_filename, NULL, NULL);
		if (doc != NULL)
			ui_add_recent_document(doc);
		g_free(utf8_filename);
		g_free(filename);
		return TRUE;
	}
	g_free(filename);
	return FALSE;
}


/* open files from command line */
static void open_cl_files(gint argc, gchar **argv)
{
	gint i;

	for (i = 1; i < argc; i++)
	{
		gchar *filename = main_get_argv_filename(argv[i]);

		if (g_file_test(filename, G_FILE_TEST_IS_DIR))
		{
			g_free(filename);
			continue;
		}

#ifdef G_OS_WIN32
		/* It seems argv elements are encoded in CP1252 on a German Windows */
		SETPTR(filename, g_locale_to_utf8(filename, -1, NULL, NULL, NULL));
#endif
		if (filename && ! main_handle_filename(filename))
		{
			const gchar *msg = _("Could not find file '%s'.");

			g_printerr(msg, filename);	/* also print to the terminal */
			g_printerr("\n");
			ui_set_statusbar(TRUE, msg, filename);
		}
		g_free(filename);
	}
}


static void load_session_project_file(void)
{
	gchar *locale_filename;

	g_return_if_fail(project_prefs.session_file != NULL);

	locale_filename = utils_get_locale_from_utf8(project_prefs.session_file);

	if (G_LIKELY(NZV(locale_filename)))
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
	if (no_msgwin)
		ui_prefs.msgwindow_visible = FALSE;

#ifdef HAVE_PLUGINS
	want_plugins = prefs.load_plugins && !no_plugins;
#endif
}


void main_load_project_from_command_line(const gchar *locale_filename, gboolean use_session)
{
	gchar *pfile;

	pfile = utils_get_path_from_uri(locale_filename);
	if (pfile != NULL)
	{
		if (use_session)
			project_load_file_with_session(pfile);
		else
			project_load_file(pfile);
	}
	g_free(pfile);
}


static void load_startup_files(gint argc, gchar **argv)
{
	gboolean load_session = FALSE;

	if (argc > 1 && g_str_has_suffix(argv[1], ".geany"))
	{
		/* project file specified: load it, but decide the session later */
		main_load_project_from_command_line(argv[1], FALSE);
		argc--, argv++;
		/* force session load if using project-based session files */
		load_session = project_prefs.project_session;
	}

	/* Load the default session if:
	 * 1. "Load files from the last session" is active.
	 * 2. --no-session is not specified.
	 * 3. We are a primary instance.
	 * Has no effect if a CL project is loaded and using project-based session files. */
	if (prefs.load_session && cl_options.load_session && !cl_options.new_instance)
	{
		if (app->project == NULL)
			load_session_project_file();
		load_session = TRUE;
	}

	if (load_session)
	{
		/* load session files into tabs, as they are found in the session_files variable */
		configuration_open_files();

		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) == 0)
		{
			ui_update_popup_copy_items(NULL);
			ui_update_popup_reundo_items(NULL);
		}
	}

	open_cl_files(argc, argv);
}


static gboolean send_startup_complete(gpointer data)
{
	g_signal_emit_by_name(geany_object, "geany-startup-complete");
	return FALSE;
}


static const gchar *get_locale(void)
{
	const gchar *locale = "unknown";
#ifdef HAVE_LOCALE_H
	locale = setlocale(LC_CTYPE, NULL);
#endif
	return locale;
}


gint main(gint argc, gchar **argv)
{
	GeanyDocument *doc;
	gint config_dir_result;
	const gchar *locale;

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

	/* Initialize GLib's thread system in case any plugins want to use it or their
	 * dependencies (e.g. WebKit, Soup, ...) */
	if (!g_thread_supported())
		g_thread_init(NULL);

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
		/* Socket exists */
		if (socket_info.lock_socket == -2)
		{
			/* Quit if filenames were sent to first instance or the list of open
			 * documents has been sent */
			if (argc > 1 || cl_options.list_documents)
			{
				gdk_notify_startup_complete();
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

	locale = get_locale();
	geany_debug("Geany %s, %s",
		main_get_version_string(),
		locale);
	geany_debug(geany_lib_versions,
		gtk_major_version, gtk_minor_version, gtk_micro_version,
		glib_major_version, glib_minor_version, glib_micro_version);
	geany_debug("System data dir: %s", app->datadir);
	geany_debug("User config dir: %s", app->configdir);

	/* create the object so Geany signals can be connected in init() functions */
	geany_object = geany_object_new();

	/* inits */
	main_init();

	encodings_init();
	editor_init();

	/* init stash groups before loading keyfile */
	configuration_init();
	ui_init_prefs();
	search_init();
	project_init();
#ifdef HAVE_PLUGINS
	plugins_init();
#endif
	sidebar_init();
	load_settings();	/* load keyfile */

	msgwin_init();
	build_init();
	ui_create_insert_menu_items();
	ui_create_insert_date_menu_items();
	keybindings_init();
	notebook_init();
	filetypes_init();
	templates_init();
	navqueue_init();
	document_init_doclist();
	symbols_init();
	editor_snippets_init();

	/* set window icon */
	{
		GdkPixbuf *pb = ui_new_pixbuf_from_inline(GEANY_IMAGE_LOGO);
		gtk_window_set_icon(GTK_WINDOW(main_widgets.window), pb);
		g_object_unref(pb);	/* free our reference */
	}

	/* registering some basic events */
	g_signal_connect(main_widgets.window, "delete-event", G_CALLBACK(on_exit_clicked), NULL);
	g_signal_connect(main_widgets.window, "window-state-event", G_CALLBACK(on_window_state_event), NULL);

	g_signal_connect(msgwindow.scribble, "motion-notify-event", G_CALLBACK(on_motion_event), NULL);

#ifdef HAVE_VTE
	vte_init();
#endif
	ui_create_recent_menus();

	ui_set_statusbar(TRUE, _("This is Geany %s."), main_get_version_string());
	if (config_dir_result != 0)
		ui_set_statusbar(TRUE, _("Configuration directory could not be created (%s)."),
			g_strerror(config_dir_result));

	/* apply all configuration options */
	apply_settings();

#ifdef HAVE_PLUGINS
	/* load any enabled plugins before we open any documents */
	if (want_plugins)
		plugins_load_active();
#endif

	ui_sidebar_show_hide();

	/* set the active sidebar page after plugins have been loaded */
	gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.sidebar_notebook), ui_prefs.sidebar_page);

	/* load keybinding settings after plugins have added their groups */
	keybindings_load_keyfile();

	/* create the custom command menu after the keybindings have been loaded to have the proper
	 * accelerator shown for the menu items */
	tools_create_insert_custom_command_menu_items();

	/* load any command line files or session files */
	main_status.opening_session_files = TRUE;
	load_startup_files(argc, argv);
	main_status.opening_session_files = FALSE;

	/* open a new file if no other file was opened */
	document_new_file_if_non_open();

	ui_document_buttons_update();
	ui_save_buttons_toggle(FALSE);

	doc = document_get_current();
	sidebar_select_openfiles_item(doc);
	build_menu_update(doc);
	sidebar_update_tag_list(doc, FALSE);

#ifdef G_OS_WIN32
	/* Manually realise the main window to be able to set the position but don't show it.
	 * We don't set the position after showing the window to avoid flickering. */
	gtk_widget_realize(main_widgets.window);
#endif
	setup_window_position();

	/* finally show the window */
	document_grab_focus(doc);
	gtk_widget_show(main_widgets.window);
	main_status.main_window_realized = TRUE;

	configuration_apply_settings();

#ifdef HAVE_SOCKET
	/* register the callback of socket input */
	if (! socket_info.ignore_socket && socket_info.lock_socket > 0)
	{
		socket_info.read_ioc = g_io_channel_unix_new(socket_info.lock_socket);
		socket_info.lock_socket_tag = g_io_add_watch(socket_info.read_ioc,
						G_IO_IN | G_IO_PRI | G_IO_ERR, socket_lock_input_cb, main_widgets.window);
	}
#endif

#ifdef G_OS_WIN32
	{
		gchar *dir;
		/* On Windows, change the working directory to the Geany installation path to not lock
		 * the directory of a file passed as command line argument (see bug #2626124). */
		dir = win32_get_installation_dir();
		win32_set_working_directory(dir);
		g_free(dir);
	}
#endif

	/* when we are really done with setting everything up and the main event loop is running,
	 * tell other components, mainly plugins, that startup is complete */
	g_idle_add_full(G_PRIORITY_LOW, send_startup_complete, NULL, NULL);

	gtk_main();
	return 0;
}


static void queue_free(GQueue *queue)
{
	while (! g_queue_is_empty(queue))
	{
		g_free(g_queue_pop_tail(queue));
	}
	g_queue_free(queue);
}


void main_quit()
{
	geany_debug("Quitting...");

#ifdef HAVE_SOCKET
	socket_finalize();
#endif

#ifdef HAVE_PLUGINS
	plugins_finalize();
#endif

	navqueue_free();
	keybindings_free();
	notebook_free();
	highlighting_free_styles();
	templates_free_templates();
	msgwin_finalize();
	search_finalize();
	build_finalize();
	document_finalize();
	symbols_finalize();
	project_finalize();
	editor_finalize();
	editor_snippets_free();
	encodings_finalize();
	toolbar_finalize();
	sidebar_finalize();
	configuration_finalize();
	filetypes_free_types();
	ui_finalize();
	log_finalize();

	tm_workspace_free(TM_WORK_OBJECT(app->tm_workspace));
	g_free(app->configdir);
	g_free(app->datadir);
	g_free(app->docdir);
	g_free(prefs.default_open_path);
	g_free(prefs.custom_plugin_path);
	g_free(ui_prefs.custom_date_format);
	g_free(interface_prefs.editor_font);
	g_free(interface_prefs.tagbar_font);
	g_free(interface_prefs.msgwin_font);
	g_free(editor_prefs.long_line_color);
	g_free(editor_prefs.comment_toggle_mark);
	g_free(editor_prefs.color_scheme);
	g_free(tool_prefs.context_action_cmd);
	g_free(template_prefs.developer);
	g_free(template_prefs.company);
	g_free(template_prefs.mail);
	g_free(template_prefs.initials);
	g_free(template_prefs.version);
	g_free(tool_prefs.term_cmd);
	g_free(tool_prefs.browser_cmd);
	g_free(tool_prefs.grep_cmd);
	g_free(printing_prefs.external_print_cmd);
	g_free(printing_prefs.page_header_datefmt);
	g_strfreev(ui_prefs.custom_commands);
	g_strfreev(ui_prefs.custom_commands_labels);

	queue_free(ui_prefs.recent_queue);
	queue_free(ui_prefs.recent_projects_queue);

	if (ui_widgets.prefs_dialog && GTK_IS_WIDGET(ui_widgets.prefs_dialog)) gtk_widget_destroy(ui_widgets.prefs_dialog);
	if (ui_widgets.open_fontsel && GTK_IS_WIDGET(ui_widgets.open_fontsel)) gtk_widget_destroy(ui_widgets.open_fontsel);
	if (ui_widgets.open_colorsel && GTK_IS_WIDGET(ui_widgets.open_colorsel)) gtk_widget_destroy(ui_widgets.open_colorsel);
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
	if (msgwindow.popup_status_menu && GTK_IS_WIDGET(msgwindow.popup_status_menu))
					gtk_widget_destroy(msgwindow.popup_status_menu);
	if (msgwindow.popup_msg_menu && GTK_IS_WIDGET(msgwindow.popup_msg_menu))
					gtk_widget_destroy(msgwindow.popup_msg_menu);
	if (msgwindow.popup_compiler_menu && GTK_IS_WIDGET(msgwindow.popup_compiler_menu))
					gtk_widget_destroy(msgwindow.popup_compiler_menu);

	g_object_unref(geany_object);
	geany_object = NULL;

	g_free(app);

	ui_finalize_builder();

	gtk_main_quit();
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
	/* reload templates */
	templates_free_templates();
	templates_init();

	/* reload snippets */
	editor_snippets_free();
	editor_snippets_init();

	filetypes_reload_extensions();
	filetypes_reload();

	/* C tag names to ignore */
	symbols_reload_config_files();

	ui_set_statusbar(TRUE, _("Configuration files reloaded."));
}
