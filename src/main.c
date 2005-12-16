/*
 *      main.c - this file is part of Geany, a fast and lightweight IDE
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


#include <signal.h>
#include <time.h>


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
// include vte.h on non-Win32 systems, else define fake vte_init
#if defined(GEANY_WIN32) || ! defined(HAVE_VTE)
# define vte_init() ;
#else
# include "vte.h"
#endif


static gboolean debug_mode = FALSE;
static gboolean ignore_global_tags = FALSE;
static gboolean no_vte = FALSE;
static gboolean show_version = FALSE;
static gchar *alternate_config = NULL;
static GOptionEntry entries[] =
{
  { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug_mode, "runs in debug mode (means being verbose)", NULL },
  { "no-ctags", 'n', 0, G_OPTION_ARG_NONE, &ignore_global_tags, "don't load auto completion data (see documentation)", NULL },
  { "config", 'c', 0, G_OPTION_ARG_FILENAME, &alternate_config, "use an alternate configuration directory", NULL },
  { "no-terminal", 't', 0, G_OPTION_ARG_NONE, &no_vte, "don't load terminal support", NULL },
  { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "show version and exit", NULL },
  { NULL }
};


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
 * an action on a setting is only performed if the setting is unequal to the program default
 * (all the following code is not perfect but it works for the moment) */
void apply_settings(void)
{
	// toolbar, message window and tagbar are by default visible, so don't change it if it is true
	if (! app->toolbar_visible)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_toolbar1")), FALSE);
		gtk_widget_hide(app->toolbar);
		app->toolbar_visible = FALSE;
	}
	if (! app->treeview_symbol_visible)
	{
		//gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_symbol_window1")), FALSE);
		gtk_widget_hide(app->treeview_notebook);
		app->treeview_symbol_visible = FALSE;
	}
	if (! app->msgwindow_visible)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_messages_window1")), FALSE);
		gtk_container_remove(GTK_CONTAINER(lookup_widget(app->window, "vpaned1")), lookup_widget(app->window, "scrolledwindow1"));
		app->msgwindow_visible = FALSE;
	}

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

	// fullscreen mode, indention guides, line_endings and white spaces are disabled by default, so act only if they are true
	if (app->show_indent_guide)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_indention_guides1")), TRUE);
		app->show_indent_guide = TRUE;
	}
	if (app->show_white_space)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_white_space")), TRUE);
		app->show_white_space = TRUE;
	}
	if (app->show_line_endings)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_show_line_endings1")), TRUE);
		app->show_line_endings = TRUE;
	}
	if (app->fullscreen)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_fullscreen1")), TRUE);
		app->fullscreen = TRUE;
		utils_set_fullscreen();
	}
	// markers margin, construct and xml tag auto completion is by default enabled
	if (! app->show_markers_margin)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_markers_margin1")), FALSE);
		app->show_markers_margin = FALSE;
	}
	if (! app->auto_close_xml_tags)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_xml_tag1")), FALSE);
		app->auto_close_xml_tags = FALSE;
	}
	if (! app->auto_complete_constructs)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_construct_completion1")), FALSE);
		app->auto_complete_constructs = FALSE;
	}
	if (! app->use_auto_indention)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_use_auto_indention1")), FALSE);
		app->use_auto_indention = FALSE;
	}
/*	if (! app->line_breaking)
	{
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(app->window, "menu_line_breaking1")), FALSE);
		app->line_breaking = FALSE;
	}
*/
	// interprets the saved window geometry
	if (app->pref_main_save_winpos && app->geometry[0] != -1)
	{
		gtk_window_move(GTK_WINDOW(app->window), app->geometry[0], app->geometry[1]);
		gtk_window_set_default_size(GTK_WINDOW(app->window), app->geometry[2], app->geometry[3]);
	}

	if (! app->pref_main_show_search)
	{
		gtk_widget_hide(lookup_widget(app->window, "entry1"));
		gtk_widget_hide(lookup_widget(app->window, "toolbutton18"));
		gtk_widget_hide(lookup_widget(app->window, "separatortoolitem4"));
	}
}



gint main(gint argc, gchar **argv)
{
	GError *error = NULL;
	GOptionContext *context;
	gint mkdir_result, idx;
	time_t seconds = time((time_t *) 0);
	struct tm *tm = localtime(&seconds);
	this_year = tm->tm_year + 1900;
	this_month = tm->tm_mon + 1;
	this_day = tm->tm_mday;


#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
#endif
	gtk_set_locale();

	signal(SIGTERM, signal_cb);

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

	gtk_init(&argc, &argv);

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
	app->search_text		= NULL;
	app->open_fontsel		= NULL;
	app->open_filesel		= NULL;
	app->save_filesel		= NULL;
	app->prefs_dialog		= NULL;
	app->find_dialog		= NULL;
	app->main_window_realized= FALSE;
#ifdef HAVE_VTE
	app->have_vte 			= ! no_vte;
#else
	app->have_vte 			= FALSE;
#endif
	app->ignore_global_tags = ignore_global_tags;
	app->tm_workspace		= tm_get_workspace();
	app->recent_queue		= g_queue_new();
	mkdir_result = utils_make_settings_dir();
	if (mkdir_result != 0)
		if (! dialogs_show_mkcfgdir_error(mkdir_result)) destroyapp_early();
	configuration_load();
	templates_init();
	encodings_init();

	app->window = create_window1();
	app->new_file_menu = gtk_menu_new();
	filetypes_init_types();

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
	app->sensitive_buttons[15] = lookup_widget(app->window, "menu_add_changelog_entry1");
	app->redo_items[0] = lookup_widget(app->popup_menu, "redo1");
	app->redo_items[1] = lookup_widget(app->window, "menu_redo2");
	app->undo_items[0] = lookup_widget(app->popup_menu, "undo1");
	app->undo_items[1] = lookup_widget(app->window, "menu_undo2");
	msgwindow.tree_status = lookup_widget(app->window, "treeview3");
	msgwindow.tree_msg = lookup_widget(app->window, "treeview4");
	msgwindow.tree_compiler = lookup_widget(app->window, "treeview5");

	gtk_window_set_icon(GTK_WINDOW(app->window), utils_new_pixbuf_from_inline(GEANY_IMAGE_LOGO, FALSE));

	// registering some basic events
	g_signal_connect(G_OBJECT(app->window), "delete_event", G_CALLBACK(on_exit_clicked), NULL);
	g_signal_connect(G_OBJECT(app->window), "configure-event", G_CALLBACK(on_window_configure_event), NULL);
	g_signal_connect(G_OBJECT(app->window), "key-press-event", G_CALLBACK(on_window_key_press_event), NULL);
	g_signal_connect(G_OBJECT(app->toolbar), "button-press-event", G_CALLBACK(toolbar_popup_menu), NULL);

	treeviews_prepare_taglist();
	treeviews_prepare_openfiles();
	treeviews_create_openfiles_popup_menu();
	msgwin_prepare_status_tree_view();
	msgwin_prepare_msg_tree_view();
	msgwin_prepare_compiler_tree_view();
	msgwindow.popup_status_menu = msgwin_create_message_popup_menu(3);
	msgwindow.popup_msg_menu = msgwin_create_message_popup_menu(4);
	msgwindow.popup_compiler_menu = msgwin_create_message_popup_menu(5);
	vte_init();
	dialogs_create_build_menu();
	dialogs_create_recent_menu();
	utils_create_insert_menu_items();

	msgwin_status_add(_("This is %s %s by Enrico Troeger."), PACKAGE, VERSION);
	if (mkdir_result != 0)
		msgwin_status_add(_("Configuration directory could not be created (%s)."), g_strerror(mkdir_result));

	apply_settings();

	// open files from command line
	if (argc > 1)
	{
		gint i;
		for(i = 1; i < argc; i++)
		{
			if (argv[i] && g_file_test(argv[i], G_FILE_TEST_IS_REGULAR || G_FILE_TEST_IS_SYMLINK))
			{
				document_open_file(-1, argv[i], 0, FALSE);
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
	utils_close_buttons_toggle();
	utils_save_buttons_toggle(FALSE);

	// this option is currently disabled, until the document menu item is reordered
	gtk_widget_hide(lookup_widget(app->window, "set_file_readonly1"));

	// open a new file if no other file was opened
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) == 0)
		document_new_file(filetypes[GEANY_FILETYPES_ALL]);

	idx = document_get_cur_idx();
	gtk_widget_grab_focus(GTK_WIDGET(doc_list[idx].sci));
	gtk_tree_model_foreach(GTK_TREE_MODEL(tv.store_openfiles), treeviews_find_node, GINT_TO_POINTER(idx));
	utils_build_show_hide(idx);

	gtk_widget_show(app->window);

	app->main_window_realized = TRUE;

	//g_timeout_add(0, (GSourceFunc)destroyapp, NULL); // useful for start time tests
	gtk_main();
	return 0;
}


