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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#ifdef HAVE_VTE
# include "vte.h"
#endif


#ifdef HAVE_FIFO
static gchar fifo_name[512];
static gboolean ignore_fifo = FALSE;
#endif
static gboolean debug_mode = FALSE;
static gboolean ignore_global_tags = FALSE;
static gboolean no_msgwin = FALSE;
static gboolean no_vte = FALSE;
static gboolean show_version = FALSE;
static gchar *alternate_config = NULL;
#ifdef HAVE_VTE
static gchar *lib_vte = NULL;
#endif
static GOptionEntry entries[] =
{
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug_mode, "runs in debug mode (means being verbose)", NULL },
	{ "no-ctags", 'n', 0, G_OPTION_ARG_NONE, &ignore_global_tags, "don't load auto completion data (see documentation)", NULL },
#ifdef HAVE_FIFO
	{ "no-pipe", 'p', 0, G_OPTION_ARG_NONE, &ignore_fifo, "don't open files in a running instance, force opening a new instance", NULL },
#endif
	{ "config", 'c', 0, G_OPTION_ARG_FILENAME, &alternate_config, "use an alternate configuration directory", NULL },
	{ "no-msgwin", 'm', 0, G_OPTION_ARG_NONE, &no_msgwin, "don't show message window at startup", NULL },
	{ "no-terminal", 't', 0, G_OPTION_ARG_NONE, &no_vte, "don't load terminal support", NULL },
#ifdef HAVE_VTE
	{ "vte-lib", 'l', 0, G_OPTION_ARG_FILENAME, &lib_vte, "filename of libvte.so", NULL },
#endif
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "show version and exit", NULL },
	{ NULL }
};



/* Geany main debug function */
void geany_debug(gchar const *format, ...)
{
#ifndef GEANY_DEBUG
	if (app->debug_mode)
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

	// toolbar, message window and tagbar are by default visible, so don't change it if it is true
	if (! app->toolbar_visible)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_toolbar1")), FALSE);
		gtk_widget_hide(app->toolbar);
		app->toolbar_visible = FALSE;
	}
	if (! app->msgwindow_visible || no_msgwin)
	{
		// I know this is a bit confusing, but it works
		app->msgwindow_visible = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_messages_window1")), FALSE);
		app->msgwindow_visible = FALSE;
	}
	utils_treeviews_showhide();
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

	// fullscreen mode is disabled by default, so act only if it is true
	if (app->fullscreen)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_fullscreen1")), TRUE);
		app->fullscreen = TRUE;
		utils_set_fullscreen();
	}
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

	if (! app->pref_main_show_goto)
	{
		gtk_widget_hide(lookup_widget(app->window, "entry_goto_line"));
		gtk_widget_hide(lookup_widget(app->window, "toolbutton25"));
		gtk_widget_hide(lookup_widget(app->window, "separatortoolitem5"));
	}

	if (! app->pref_main_show_search)
	{
		gtk_widget_hide(lookup_widget(app->window, "entry1"));
		gtk_widget_hide(lookup_widget(app->window, "toolbutton18"));
		gtk_widget_hide(lookup_widget(app->window, "separatortoolitem4"));
	}

	g_object_set(G_OBJECT(lookup_widget(app->window, "menu_line_breaking1")), "active",
				GINT_TO_POINTER(app->pref_editor_line_breaking), NULL);
	g_object_set(G_OBJECT(lookup_widget(app->window, "menu_use_auto_indention1")), "active",
				GINT_TO_POINTER(app->pref_editor_use_auto_indention), NULL);

	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(
			lookup_widget(app->window, "menutoolbutton1")), app->new_file_menu);
}


static void main_init(void)
{
	// inits
	app = g_new(MyApp, 1);
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
#ifdef HAVE_VTE
	app->lib_vte			= lib_vte;
#endif
	app->window				= NULL;
	app->search_text		= NULL;
	app->build_args_inc		= NULL;
	app->build_args_libs	= NULL;
	app->build_args_prog	= NULL;
	app->open_fontsel		= NULL;
	app->open_colorsel		= NULL;
	app->open_filesel		= NULL;
	app->save_filesel		= NULL;
	app->prefs_dialog		= NULL;
	app->find_dialog		= NULL;
	app->replace_dialog		= NULL;
	app->default_tag_tree	= NULL;
	app->main_window_realized= FALSE;
	app->quitting			= FALSE;
#ifdef HAVE_FIFO
	app->ignore_fifo		= ignore_fifo;
#endif
#ifdef HAVE_VTE
	app->have_vte 			= ! no_vte;
#else
	app->have_vte 			= FALSE;
#endif
	app->ignore_global_tags 					= ignore_global_tags;
	app->tm_workspace							= tm_get_workspace();
	app->recent_queue							= g_queue_new();
	app->opening_session_files					= FALSE;
	dialogs_build_menus.menu_c.menu				= NULL;
	dialogs_build_menus.menu_c.item_compile		= NULL;
	dialogs_build_menus.menu_c.item_exec		= NULL;
	dialogs_build_menus.menu_c.item_link		= NULL;
	dialogs_build_menus.menu_tex.menu			= NULL;
	dialogs_build_menus.menu_tex.item_compile	= NULL;
	dialogs_build_menus.menu_tex.item_exec		= NULL;
	dialogs_build_menus.menu_tex.item_link		= NULL;
	dialogs_build_menus.menu_misc.menu			= NULL;
	dialogs_build_menus.menu_misc.item_compile	= NULL;
	dialogs_build_menus.menu_misc.item_exec		= NULL;
	dialogs_build_menus.menu_misc.item_link		= NULL;

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
	app->sensitive_buttons[16] = lookup_widget(app->window, "find1");
	app->sensitive_buttons[17] = lookup_widget(app->window, "find_next1");
	app->sensitive_buttons[18] = lookup_widget(app->window, "replace1");
	app->sensitive_buttons[19] = lookup_widget(app->window, "menu_paste1");
	app->sensitive_buttons[20] = lookup_widget(app->window, "menu_undo2");
	app->sensitive_buttons[21] = lookup_widget(app->window, "preferences2");
	app->sensitive_buttons[22] = lookup_widget(app->window, "revert1");
	app->sensitive_buttons[23] = lookup_widget(app->window, "menu_item4");
	app->sensitive_buttons[24] = lookup_widget(app->window, "menu_markers_margin1");
	app->sensitive_buttons[25] = lookup_widget(app->window, "menu_linenumber_margin1");
	app->sensitive_buttons[26] = lookup_widget(app->window, "menu_choose_color1");
	app->sensitive_buttons[27] = lookup_widget(app->window, "menu_zoom_in1");
	app->sensitive_buttons[28] = lookup_widget(app->window, "menu_zoom_out1");
	app->sensitive_buttons[29] = lookup_widget(app->window, "normal_size1");
	app->sensitive_buttons[30] = lookup_widget(app->window, "toolbutton24");
	app->redo_items[0] = lookup_widget(app->popup_menu, "redo1");
	app->redo_items[1] = lookup_widget(app->window, "menu_redo2");
	app->undo_items[0] = lookup_widget(app->popup_menu, "undo1");
	app->undo_items[1] = lookup_widget(app->window, "menu_undo2");
	msgwindow.tree_status = lookup_widget(app->window, "treeview3");
	msgwindow.tree_msg = lookup_widget(app->window, "treeview4");
	msgwindow.tree_compiler = lookup_widget(app->window, "treeview5");
}


#ifdef HAVE_FIFO
static gboolean read_fifo(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GIOStatus status;
	gchar *read_data = NULL;
	gsize len = 0;

	status = g_io_channel_read_to_end(source, &read_data, &len, NULL);

	// try to interpret the received data as a filename, otherwise do nothing
	if (g_file_test(read_data, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
	{
		document_open_file(-1, read_data, 0, FALSE, NULL);
	}
	else
	{
		geany_debug("got data from named pipe, but it does not look like a filename");
	}
	gtk_widget_grab_focus(app->window);

	g_free(read_data);

	return TRUE;
}


static void write_fifo(gint argc, gchar **argv)
{
	GIOChannel *ioc = g_io_channel_unix_new(open(fifo_name, O_WRONLY));
	gint i;
	for(i = 1; i < argc; i++)
	{
		if (argv[i] && g_file_test(argv[i], G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK))
		{
			g_io_channel_write_chars(ioc, argv[i], -1, NULL, NULL);
			//g_io_channel_flush(ioc, NULL);
		}
	}
	g_io_channel_shutdown(ioc, TRUE, NULL);
}


/* creates a named pipe in GEANY_FIFO_NAME, to communicate with new instances of Geany
 * to submit filenames and possibly other things */
static void create_fifo(gint argc, gchar **argv, const gchar *config_dir)
{
	struct stat st;
	gchar *tmp;
	GIOChannel *ioc;

	tmp = g_strconcat(config_dir, G_DIR_SEPARATOR_S, GEANY_FIFO_NAME, NULL);
	strncpy(fifo_name, tmp, MAX(strlen(tmp), sizeof(fifo_name)));
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
			if (dialogs_show_fifo_error(_("Geany is exiting because a named pipe was found. Mostly this means, Geany is already running. If you know Geany is not running, you can delete the file and start Geany anyway.\nDelete the named pipe and start Geany?")))
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
		return;
	}

	ioc = g_io_channel_unix_new(open(fifo_name, O_RDONLY | O_NONBLOCK));
	g_io_add_watch(ioc, G_IO_IN | G_IO_PRI, read_fifo, NULL);
}
#endif


gint main(gint argc, gchar **argv)
{
	GError *error = NULL;
	GOptionContext *context;
	gint mkdir_result = 0;
	gint idx;
	time_t seconds = time((time_t *) 0);
	struct tm *tm = localtime(&seconds);
	this_year = tm->tm_year + 1900;
	this_month = tm->tm_mon + 1;
	this_day = tm->tm_mday;
	gchar *config_dir;

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif
	gtk_set_locale();

	context = g_option_context_new(_(" - A fast and lightweight IDE"));
	g_option_context_add_main_entries(context, entries, GETTEXT_PACKAGE);
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

	signal(SIGTERM, signal_cb);

	if (alternate_config) config_dir = g_strdup(alternate_config);
	else config_dir = g_strconcat(GEANY_HOME_DIR, G_DIR_SEPARATOR_S, ".", PACKAGE, NULL);

	mkdir_result = utils_make_settings_dir(config_dir);
	if (mkdir_result != 0 && ! dialogs_show_mkcfgdir_error(mkdir_result))
	{
		g_free(config_dir);
		return (0);
	}

#ifdef HAVE_FIFO
	if (! ignore_fifo) create_fifo(argc, argv, config_dir);
#endif
	g_free(config_dir);

	gtk_init(&argc, &argv);

	main_init();
	gtk_widget_set_size_request(app->window, GEANY_WINDOW_MINIMAL_WIDTH, GEANY_WINDOW_MINIMAL_HEIGHT);
	gtk_window_set_default_size(GTK_WINDOW(app->window), GEANY_WINDOW_DEFAULT_WIDTH, GEANY_WINDOW_DEFAULT_HEIGHT);
	configuration_load();
	templates_init();
	encodings_init();
	document_init_doclist();

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

	msgwin_status_add(_("This is %s %s by Enrico Troeger."), PACKAGE, VERSION);
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
					document_open_file(-1, argv[i], 0, FALSE, NULL);
					opened++;
				}
				else
				{
					dialogs_show_file_open_error();
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
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) == 0)
		document_new_file(filetypes[GEANY_FILETYPES_ALL]);

	utils_close_buttons_toggle();
	utils_save_buttons_toggle(FALSE);

	idx = document_get_cur_idx();
	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
	gtk_tree_model_foreach(GTK_TREE_MODEL(tv.store_openfiles), treeviews_find_node, GINT_TO_POINTER(idx));
	utils_build_show_hide(idx);
	utils_update_tag_list(idx, FALSE);

	// finally realize the window to show the user what we have done
	gtk_widget_show(app->window);
	app->main_window_realized = TRUE;

	//g_timeout_add(0, (GSourceFunc)destroyapp, NULL); // useful for start time tests
	gtk_main();
	return 0;
}




