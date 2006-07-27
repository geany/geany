/*
 *      main.c - this file is part of Geany, a fast and lightweight IDE
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


#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "geany.h"

#include "interface.h"
#include "support.h"
#include "callbacks.h"

#include "utils.h"
#include "document.h"
#include "keyfile.h"
#include "win32.h"
#include "msgwindow.h"
#include "dialogs.h"
#include "templates.h"
#include "encodings.h"
#include "treeviews.h"
#include "notebook.h"
#include "keybindings.h"
#include "sci_cb.h"

#ifdef HAVE_VTE
# include "vte.h"
#endif

#define N_(String) (String)

#ifdef HAVE_FIFO
static gchar fifo_name[512];
static gboolean ignore_fifo = FALSE;
#endif
static gboolean debug_mode = FALSE;
static gboolean ignore_global_tags = FALSE;
static gboolean no_msgwin = FALSE;
static gboolean show_version = FALSE;
static gchar *alternate_config = NULL;
#ifdef HAVE_VTE
static gboolean no_vte = FALSE;
static gchar *lib_vte = NULL;
#endif
static GOptionEntry entries[] =
{
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug_mode, N_("runs in debug mode (means being verbose)"), NULL },
	{ "no-ctags", 'n', 0, G_OPTION_ARG_NONE, &ignore_global_tags, N_("don't load auto completion data (see documentation)"), NULL },
#ifdef HAVE_FIFO
	{ "no-pipe", 'p', 0, G_OPTION_ARG_NONE, &ignore_fifo, N_("don't open files in a running instance, force opening a new instance"), NULL },
#endif
	{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &alternate_config, N_("use an alternate configuration directory"), NULL },
	{ "no-msgwin", 'm', 0, G_OPTION_ARG_NONE, &no_msgwin, N_("don't show message window at startup"), NULL },
#ifdef HAVE_VTE
	{ "no-terminal", 't', 0, G_OPTION_ARG_NONE, &no_vte, N_("don't load terminal support"), NULL },
	{ "vte-lib", 'l', 0, G_OPTION_ARG_FILENAME, &lib_vte, N_("filename of libvte.so"), NULL },
#endif
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, N_("show version and exit"), NULL },
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
	utils_update_fold_items();

	// toolbar, message window and sidebar are by default visible, so don't change it if it is true
	if (! app->toolbar_visible)
	{
		app->ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_toolbar1")), FALSE);
		gtk_widget_hide(app->toolbar);
		app->ignore_callback = FALSE;
	}
	if (! app->msgwindow_visible || no_msgwin)
	{
		app->ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_messages_window1")), FALSE);
		gtk_widget_hide(lookup_widget(app->window, "scrolledwindow1"));
		app->ignore_callback = FALSE;
	}
	if (! app->sidebar_visible)
	{
		app->ignore_callback = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_sidebar1")), FALSE);
		app->ignore_callback = FALSE;
	}
	utils_treeviews_showhide(TRUE);
	// sets the icon style of the toolbar
	switch (app->toolbar_icon_style)
	{
		case GTK_TOOLBAR_BOTH:
		{
			//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "images_and_text1")), TRUE);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->toolbar_menu, "images_and_text2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_ICONS:
		{
			//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "images_only1")), TRUE);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->toolbar_menu, "images_only2")), TRUE);
			break;
		}
		case GTK_TOOLBAR_TEXT:
		{
			//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "text_only1")), TRUE);
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->toolbar_menu, "text_only2")), TRUE);
			break;
		}
	}

	// sets the icon size of the toolbar, use user preferences (.gtkrc) if not set
	if (app->toolbar_icon_size == GTK_ICON_SIZE_SMALL_TOOLBAR ||
		app->toolbar_icon_size == GTK_ICON_SIZE_LARGE_TOOLBAR)
	{
		gtk_toolbar_set_icon_size(GTK_TOOLBAR(app->toolbar), app->toolbar_icon_size);
	}
	utils_update_toolbar_icons(app->toolbar_icon_size);

	// line number and markers margin are by default enabled
	if (! app->show_markers_margin)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_markers_margin1")), FALSE);
		app->show_markers_margin = FALSE;
	}
	if (! app->show_linenumber_margin)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_linenumber_margin1")), FALSE);
		app->show_linenumber_margin = FALSE;
	}

	// interprets the saved window geometry
	if (app->pref_main_save_winpos && app->geometry[0] != -1)
	{
		gtk_window_move(GTK_WINDOW(app->window), app->geometry[0], app->geometry[1]);
		gtk_window_set_default_size(GTK_WINDOW(app->window), app->geometry[2], app->geometry[3]);
	}

	app->ignore_callback = TRUE;
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
				lookup_widget(app->window, "menu_line_breaking1")), app->pref_editor_line_breaking);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
				lookup_widget(app->window, "menu_use_auto_indention1")), app->pref_editor_use_auto_indention);
	app->ignore_callback = FALSE;

	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(
			lookup_widget(app->window, "menutoolbutton1")), app->new_file_menu);

	// set the tab placements of the notebooks
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->notebook), app->tab_pos_editor);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(msgwindow.notebook), app->tab_pos_msgwin);
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->treeview_notebook), app->tab_pos_sidebar);

	utils_update_toolbar_items();
}


static void main_init(void)
{
	// inits
	app = g_new0(MyApp, 1);
#ifdef GEANY_DEBUG
	geany_debug("debug mode built in (can't be disabled)");
#endif
	app->debug_mode = debug_mode;
	if (alternate_config)
	{
		geany_debug("alternate config: %s", alternate_config);
		app->configdir = alternate_config;
	}
	else
		app->configdir = g_strconcat(GEANY_HOME_DIR, G_DIR_SEPARATOR_S, ".", PACKAGE, NULL);
	app->window				= NULL;
	app->search_text		= NULL;
	app->open_fontsel		= NULL;
	app->open_colorsel		= NULL;
	app->open_filesel		= NULL;
	app->save_filesel		= NULL;
	app->prefs_dialog		= NULL;
	app->find_dialog		= NULL;
	app->replace_dialog		= NULL;
	app->find_in_files_dialog = NULL;
	app->default_tag_tree	= NULL;
	app->main_window_realized= FALSE;
	app->tab_order_ltr		= FALSE;
	app->quitting			= FALSE;
	app->ignore_callback	= FALSE;
#ifdef HAVE_FIFO
	app->ignore_fifo		= ignore_fifo;
#endif
#ifdef HAVE_VTE
	app->lib_vte			= lib_vte;
#endif
	app->ignore_global_tags 		= ignore_global_tags;
	app->tm_workspace				= tm_get_workspace();
	app->recent_queue				= g_queue_new();
	app->opening_session_files		= FALSE;
	html_entities					= NULL;

	app->window = create_window1();
	app->new_file_menu = gtk_menu_new();

	// store important pointers in the MyApp structure
	app->toolbar = lookup_widget(app->window, "toolbar1");
	app->tagbar = lookup_widget(app->window, "scrolledwindow2");
	app->treeview_notebook = lookup_widget(app->window, "notebook3");
	app->notebook = lookup_widget(app->window, "notebook1");
	msgwindow.notebook = lookup_widget(app->window, "notebook_info");
	app->statusbar = lookup_widget(app->window, "statusbar");
	app->popup_menu = create_edit_menu1();
	app->toolbar_menu = create_toolbar_popup_menu1();
	app->compile_button = lookup_widget(app->window, "toolbutton13");
	app->run_button = lookup_widget(app->window, "toolbutton26");
	app->popup_goto_items[0] = lookup_widget(app->popup_menu, "goto_tag_definition1");
	app->popup_goto_items[1] = lookup_widget(app->popup_menu, "goto_tag_declaration1");
	app->popup_goto_items[2] = lookup_widget(app->popup_menu, "find_usage1");
	app->popup_items[0] = lookup_widget(app->popup_menu, "cut1");
	app->popup_items[1] = lookup_widget(app->popup_menu, "copy1");
	app->popup_items[2] = lookup_widget(app->popup_menu, "delete1");
	app->popup_items[3] = lookup_widget(app->popup_menu, "change_selection1");
	app->menu_copy_items[0] = lookup_widget(app->window, "menu_cut1");
	app->menu_copy_items[1] = lookup_widget(app->window, "menu_copy1");
	app->menu_copy_items[2] = lookup_widget(app->window, "menu_delete1");
	app->menu_copy_items[3] = lookup_widget(app->window, "menu_change_selection2");
	app->menu_insert_include_item[0] = lookup_widget(app->popup_menu, "insert_include1");
	app->menu_insert_include_item[1] = lookup_widget(app->window, "insert_include2");
	app->save_buttons[0] = lookup_widget(app->window, "menu_save1");
	app->save_buttons[1] = lookup_widget(app->window, "toolbutton10");
	app->sensitive_buttons[0] = lookup_widget(app->window, "menu_close1");
	app->sensitive_buttons[1] = lookup_widget(app->window, "toolbutton15");
	app->sensitive_buttons[2] = lookup_widget(app->window, "menu_change_font1");
	app->sensitive_buttons[3] = lookup_widget(app->window, "entry1");
	app->sensitive_buttons[4] = lookup_widget(app->window, "toolbutton18");
	app->sensitive_buttons[5] = lookup_widget(app->window, "toolbutton20");
	app->sensitive_buttons[6] = lookup_widget(app->window, "toolbutton21");
	app->sensitive_buttons[7] = lookup_widget(app->window, "menu_close_all1");
	app->sensitive_buttons[8] = lookup_widget(app->window, "menu_save_all1");
	app->sensitive_buttons[9] = lookup_widget(app->window, "toolbutton22");
	app->sensitive_buttons[10] = app->compile_button;
	app->sensitive_buttons[11] = lookup_widget(app->window, "menu_save_as1");
	app->sensitive_buttons[12] = lookup_widget(app->window, "toolbutton23");
	app->sensitive_buttons[13] = lookup_widget(app->window, "menu_count_words1");
	app->sensitive_buttons[14] = lookup_widget(app->window, "menu_build1");
	app->sensitive_buttons[15] = lookup_widget(app->window, "add_comments1");
	app->sensitive_buttons[16] = lookup_widget(app->window, "search1");
	app->sensitive_buttons[17] = lookup_widget(app->window, "menu_paste1");
	app->sensitive_buttons[18] = lookup_widget(app->window, "menu_undo2");
	app->sensitive_buttons[19] = lookup_widget(app->window, "preferences2");
	app->sensitive_buttons[20] = lookup_widget(app->window, "menu_reload1");
	app->sensitive_buttons[21] = lookup_widget(app->window, "menu_item4");
	app->sensitive_buttons[22] = lookup_widget(app->window, "menu_markers_margin1");
	app->sensitive_buttons[23] = lookup_widget(app->window, "menu_linenumber_margin1");
	app->sensitive_buttons[24] = lookup_widget(app->window, "menu_choose_color1");
	app->sensitive_buttons[25] = lookup_widget(app->window, "menu_zoom_in1");
	app->sensitive_buttons[26] = lookup_widget(app->window, "menu_zoom_out1");
	app->sensitive_buttons[27] = lookup_widget(app->window, "normal_size1");
	app->sensitive_buttons[28] = lookup_widget(app->window, "toolbutton24");
	app->sensitive_buttons[29] = lookup_widget(app->window, "toolbutton25");
	app->sensitive_buttons[30] = lookup_widget(app->window, "entry_goto_line");
	app->sensitive_buttons[31] = lookup_widget(app->window, "treeview6");
	app->sensitive_buttons[32] = lookup_widget(app->window, "print1");
	app->sensitive_buttons[33] = lookup_widget(app->window, "menu_reload_as1");
	app->redo_items[0] = lookup_widget(app->popup_menu, "redo1");
	app->redo_items[1] = lookup_widget(app->window, "menu_redo2");
	app->redo_items[2] = lookup_widget(app->window, "toolbutton_redo");
	app->undo_items[0] = lookup_widget(app->popup_menu, "undo1");
	app->undo_items[1] = lookup_widget(app->window, "menu_undo2");
	app->undo_items[2] = lookup_widget(app->window, "toolbutton_undo");
	msgwindow.tree_status = lookup_widget(app->window, "treeview3");
	msgwindow.tree_msg = lookup_widget(app->window, "treeview4");
	msgwindow.tree_compiler = lookup_widget(app->window, "treeview5");
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
		//use current dir
		gchar *cur_dir = g_get_current_dir();
		result = g_strjoin(
			G_DIR_SEPARATOR_S, cur_dir, filename, NULL);
		g_free(cur_dir);
	}
	return result;
}


#ifdef HAVE_FIFO
static gboolean read_fifo(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GIOStatus status = G_IO_STATUS_NORMAL;
	gchar *buffer = NULL;
	gsize len = 0;

	//status = g_io_channel_read_to_end(source, &buffer, &len, NULL);
	status = g_io_channel_read_line(source, &buffer, &len, NULL, NULL);

	// try to interpret the received data as a filename, otherwise do nothing
	if (g_file_test(buffer, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
	{
		document_open_file(-1, buffer, 0, FALSE, NULL, NULL);
		gtk_window_deiconify(GTK_WINDOW(app->window));
	}
	else
	{
		geany_debug("got data from named pipe, but it does not look like a filename");
	}
	g_free(buffer);

	if (status != G_IO_STATUS_NORMAL)
		return TRUE;
	else
	{
		g_io_channel_unref(app->fifo_ioc);
		g_io_channel_shutdown(app->fifo_ioc, FALSE, NULL);
		app->fifo_ioc = g_io_channel_unix_new(open(fifo_name, O_RDONLY | O_NONBLOCK));
		g_io_add_watch(app->fifo_ioc, G_IO_IN | G_IO_PRI, read_fifo, NULL);
		return FALSE;
	}
}


static void write_fifo(gint argc, gchar **argv)
{
	gint i;
	GIOChannel *ioc;

	for(i = 1; i < argc && argv[i] != NULL; i++)
	{
		gchar *filename = get_argv_filename(argv[i]);

		if (filename && g_file_test(filename, G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK))
		{
			ioc = g_io_channel_unix_new(open(fifo_name, O_WRONLY));
			g_io_channel_write_chars(ioc, filename, -1, NULL, NULL);
			g_io_channel_flush(ioc, NULL);
			g_io_channel_shutdown(ioc, TRUE, NULL);
		}
		g_free(filename);
	}
}


/* creates a named pipe in GEANY_FIFO_NAME, to communicate with new instances of Geany
 * to submit filenames and possibly other things */
static GIOChannel *create_fifo(gint argc, gchar **argv, const gchar *config_dir)
{
	struct stat st;
	gchar *tmp;
	GIOChannel *ioc;

	tmp = g_strconcat(config_dir, G_DIR_SEPARATOR_S, GEANY_FIFO_NAME, NULL);
	g_strlcpy(fifo_name, tmp, sizeof(fifo_name));
	g_free(tmp);

	if (stat(fifo_name, &st) == 0 && (! S_ISFIFO(st.st_mode)))
	{	// the FIFO file exists, but is not a FIFO
		unlink(fifo_name);
	}
	else if (stat(fifo_name, &st) == 0 && S_ISFIFO(st.st_mode))
	{	// FIFO exists, there should be a running instance of Geany
		if (argc > 1)
		{
			geany_debug("using running instance of Geany");
			write_fifo(argc, argv);
			exit(0);
		}
		else
		{
			if (dialogs_show_question(_("Geany is exiting because a named pipe was found. Mostly this means, Geany is already running. If you know Geany is not running, you can delete the file and start Geany anyway.\nDelete the named pipe and start Geany?")))
			{
				unlink(fifo_name);
			}
			else exit(0);
		}
	}

	// there is no Geany running, create fifo and start as usual, so we are a kind of server
	geany_debug("trying to create a new named pipe");

	if ((mkfifo(fifo_name, S_IRUSR | S_IWUSR)) == -1)
	{
		if (errno != EEXIST) geany_debug("creating of a named pipe for IPC failed! (%s)", g_strerror(errno));
		return NULL;
	}

	ioc = g_io_channel_unix_new(open(fifo_name, O_RDONLY | O_NONBLOCK));
	g_io_add_watch(ioc, G_IO_IN | G_IO_PRI, read_fifo, NULL);

	return ioc;
}
#endif


gint main(gint argc, gchar **argv)
{
	GError *error = NULL;
	GOptionContext *context;
#ifdef HAVE_FIFO
	GIOChannel *ioc = NULL;
#endif
	gint mkdir_result = 0;
	gint idx;
	gchar *config_dir;
	gchar *data_dir;
	gchar *doc_dir;
	gchar *locale_dir;

	// set paths
#ifdef G_OS_WIN32
	// take the installation directory(the one where geany.exe is located) as the base for the
	// language catalogs, documentation and data files
	gchar *install_dir = g_win32_get_package_installation_directory("geany", NULL);

	data_dir = g_strconcat(install_dir, "\\data", NULL); // e.g. C:\Program Files\geany\data
	doc_dir = g_strconcat(install_dir, "\\doc", NULL);
	locale_dir = g_strdup(data_dir);

	g_free(install_dir);
#else
	data_dir = g_strdup(PACKAGE_DATA_DIR "/" PACKAGE "/"); // e.g. /usr/share/geany
	doc_dir = g_strdup(PACKAGE_DATA_DIR "/doc/" PACKAGE "/html/");
	locale_dir = g_strdup(PACKAGE_LOCALE_DIR);
#endif

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, locale_dir);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif
	g_free(locale_dir);

	context = g_option_context_new(_(" - A fast and lightweight IDE"));
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(g_option_context_get_main_group(context), GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(TRUE));
	g_option_context_parse(context, &argc, &argv, &error);
	g_option_context_free(context);

	if (show_version)
	{
		printf(PACKAGE " " VERSION " ");
		printf(_("(built on %s with GTK %d.%d.%d, GLib %d.%d.%d)"),
				__DATE__, GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
				GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
		printf("\n");

		return (0);
	}

	gtk_set_locale();

	signal(SIGTERM, signal_cb);
#ifdef G_OS_UNIX
	/* ignore SIGPIPE signal for preventing sudden death of program */
	signal(SIGPIPE, SIG_IGN);
#endif

	if (alternate_config) config_dir = g_strdup(alternate_config);
	else config_dir = g_strconcat(GEANY_HOME_DIR, G_DIR_SEPARATOR_S, ".", PACKAGE, NULL);

	mkdir_result = utils_make_settings_dir(config_dir, data_dir, doc_dir);
	if (mkdir_result != 0)
	{
		if (! dialogs_show_question(
			_("Configuration directory could not be created (%s).\nThere could be some problems "
			  "using Geany without a configuration directory.\nStart Geany anyway?"),
			  g_strerror(mkdir_result)))
		{
			g_free(config_dir);
			return (0);
		}
	}

#ifdef HAVE_FIFO
	if (! ignore_fifo) ioc = create_fifo(argc, argv, config_dir);
#endif
	g_free(config_dir);

	gtk_init(&argc, &argv);

	// inits
	main_init();
	app->datadir = data_dir;
	app->docdir = doc_dir;
	gtk_widget_set_size_request(app->window, GEANY_WINDOW_MINIMAL_WIDTH, GEANY_WINDOW_MINIMAL_HEIGHT);
	gtk_window_set_default_size(GTK_WINDOW(app->window), GEANY_WINDOW_DEFAULT_WIDTH, GEANY_WINDOW_DEFAULT_HEIGHT);
	encodings_init();
	configuration_load();
	keybindings_init();
	notebook_init();
	templates_init();
	document_init_doclist();
	// do this here to let cmdline options overwrite configuration settings
#ifdef HAVE_VTE
	app->have_vte = app->load_vte;
	if (no_vte) app->have_vte = FALSE;
#endif
#ifdef HAVE_FIFO
	app->fifo_ioc = ioc;
#endif
	filetypes_init_types();
	configuration_read_filetype_extensions();

	gtk_window_set_icon(GTK_WINDOW(app->window), utils_new_pixbuf_from_inline(GEANY_IMAGE_LOGO, FALSE));

	// registering some basic events
	g_signal_connect(G_OBJECT(app->window), "delete_event", G_CALLBACK(on_exit_clicked), NULL);
	g_signal_connect(G_OBJECT(app->window), "configure-event", G_CALLBACK(on_window_configure_event), NULL);
	g_signal_connect(G_OBJECT(app->window), "key-press-event", G_CALLBACK(on_window_key_press_event), NULL);
	g_signal_connect(G_OBJECT(app->toolbar), "button-press-event", G_CALLBACK(toolbar_popup_menu), NULL);

	treeviews_prepare_openfiles();
	treeviews_create_taglist_popup_menu();
	treeviews_create_openfiles_popup_menu();
	msgwin_prepare_status_tree_view();
	msgwin_prepare_msg_tree_view();
	msgwin_prepare_compiler_tree_view();
	msgwindow.popup_status_menu = msgwin_create_message_popup_menu(3);
	msgwindow.popup_msg_menu = msgwin_create_message_popup_menu(4);
	msgwindow.popup_compiler_menu = msgwin_create_message_popup_menu(5);
#ifdef HAVE_VTE
	vte_init();
#endif
	dialogs_create_recent_menu();
	utils_create_insert_menu_items();

	msgwin_status_add(_("This is Geany %s."), VERSION);
	if (mkdir_result != 0)
		msgwin_status_add(_("Configuration directory could not be created (%s)."), g_strerror(mkdir_result));

	// apply all configuration options
	apply_settings();

	// open files from command line
	app->opening_session_files = TRUE;
	if (argc > 1)
	{
		gint i, opened = 0;
		for(i = 1; i < argc; i++)
		{
			if (argv[i] && g_file_test(argv[i], G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK))
			{
				if (opened < GEANY_MAX_OPEN_FILES)
				{
					gchar *filename = get_argv_filename(argv[i]);
					document_open_file(-1, filename, 0, FALSE, NULL, NULL);
					g_free(filename);
					opened++;
				}
				else
				{
					dialogs_show_error(
			_("You have opened too many files. There is a limit of %d concurrent open files."),
			GEANY_MAX_OPEN_FILES);
					break;
				}
			}
		}
	}
	else if (app->pref_main_load_session)
	{
		if (! configuration_open_files())
		{
			utils_update_popup_copy_items(-1);
			utils_update_popup_reundo_items(-1);
		}
	}
	app->opening_session_files = FALSE;

	// open a new file if no other file was opened
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) == 0)	document_new_file(NULL);

	utils_close_buttons_toggle();
	utils_save_buttons_toggle(FALSE);

	idx = document_get_cur_idx();
	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
	gtk_tree_model_foreach(GTK_TREE_MODEL(tv.store_openfiles), treeviews_find_node, GINT_TO_POINTER(idx));
	utils_build_show_hide(idx);
	utils_update_tag_list(idx, FALSE);

#ifdef G_OS_WIN32
	// hide "Build" menu item, at least until it is available for Windows
	gtk_widget_hide(lookup_widget(app->window, "menu_build1"));
	gtk_widget_hide(app->compile_button);
	gtk_widget_hide(app->run_button);
	gtk_widget_hide(lookup_widget(app->window, "separatortoolitem6"));
	{
		GtkWidget *compiler_tab;
		compiler_tab = gtk_notebook_get_tab_label(GTK_NOTEBOOK(msgwindow.notebook),
			gtk_notebook_get_nth_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_COMPILER));
		gtk_widget_set_sensitive(compiler_tab, FALSE);
	}
#endif

	// finally realize the window to show the user what we have done
	gtk_widget_show(app->window);
	app->main_window_realized = TRUE;

	configuration_apply_settings();

	//g_timeout_add(0, (GSourceFunc)destroyapp, NULL); // useful for start time tests
	gtk_main();
	return 0;
}




