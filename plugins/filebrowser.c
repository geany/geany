/*
 *      filebrowser.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 *
 * $Id$
 */

/* Sidebar file browser plugin. */

#include "geany.h"

#include <gdk/gdkkeysyms.h>

#include "support.h"
#include "prefs.h"
#include "document.h"
#include "utils.h"
#include "keybindings.h"
#include "project.h"

#include "plugindata.h"
#include "pluginmacros.h"


PluginFields	*plugin_fields;
GeanyData		*geany_data;


VERSION_CHECK(26)

PLUGIN_INFO(_("File Browser"), _("Adds a file browser tab to the sidebar."), VERSION,
	_("The Geany developer team"))


enum
{
	FILEVIEW_COLUMN_ICON = 0,
	FILEVIEW_COLUMN_NAME,
	FILEVIEW_N_COLUMNS,
};

static gboolean show_hidden_files = FALSE;
static gboolean hide_object_files = TRUE;

static GtkWidget	*file_view_vbox;
static GtkWidget	*file_view;
static GtkListStore	*file_store;
static GtkTreeIter	*last_dir_iter = NULL;

static GtkWidget	*path_entry;
static gchar		*current_dir = NULL;	// in locale-encoding
static gchar		*open_cmd;				// in locale-encoding
static gchar		*config_file;


// Returns: whether name should be hidden.
static gboolean check_hidden(const gchar *base_name)
{
	gsize len;

	if (! NZV(base_name))
		return FALSE;

	if (base_name[0] == '.')
		return TRUE;

	len = strlen(base_name);
	if (base_name[len - 1] == '~')
		return TRUE;

	if (hide_object_files)
	{
		const gchar *exts[] = {".o", ".obj", ".so", ".dll", ".a", ".lib"};
		guint i;

		for (i = 0; i < G_N_ELEMENTS(exts); i++)
		{
			const gchar *ext = exts[i];

			if (utils->str_equal(&base_name[len - strlen(ext)], ext))
				return TRUE;
		}
	}
	return FALSE;
}


// name is in locale encoding
static void add_item(const gchar *name)
{
	GtkTreeIter iter;
	gchar *fname, *utf8_name;
	gboolean dir;

	if (! show_hidden_files && check_hidden(name))
		return;

	fname = g_strconcat(current_dir, G_DIR_SEPARATOR_S, name, NULL);
	dir = g_file_test(fname, G_FILE_TEST_IS_DIR);
	g_free(fname);

	if (dir)
	{
		if (last_dir_iter == NULL)
			gtk_list_store_prepend(file_store, &iter);
		else
		{
			gtk_list_store_insert_after(file_store, &iter, last_dir_iter);
			gtk_tree_iter_free(last_dir_iter);
		}
		last_dir_iter = gtk_tree_iter_copy(&iter);
	}
	else
		gtk_list_store_append(file_store, &iter);

	utf8_name = utils->get_utf8_from_locale(name);

	gtk_list_store_set(file_store, &iter,
		FILEVIEW_COLUMN_ICON, (dir) ? GTK_STOCK_DIRECTORY : GTK_STOCK_FILE,
		FILEVIEW_COLUMN_NAME, utf8_name, -1);
	g_free(utf8_name);
}


static void clear()
{
	gtk_list_store_clear(file_store);

	// reset the directory item pointer
	if (last_dir_iter != NULL)
		gtk_tree_iter_free(last_dir_iter);
	last_dir_iter = NULL;
}


// recreate the tree model from current_dir.
static void refresh()
{
	gchar *utf8_dir;
	GSList *list;

	// don't clear when the new path doesn't exist
	if (! g_file_test(current_dir, G_FILE_TEST_EXISTS))
		return;

	clear();

	utf8_dir = utils->get_utf8_from_locale(current_dir);
	gtk_entry_set_text(GTK_ENTRY(path_entry), utf8_dir);
	g_free(utf8_dir);

	list = utils->get_file_list(current_dir, NULL, NULL);
	if (list != NULL)
	{
		g_slist_foreach(list, (GFunc) add_item, NULL);
		g_slist_foreach(list, (GFunc) g_free, NULL);
		g_slist_free(list);
	}
}


static void on_go_home()
{
	setptr(current_dir, g_strdup(g_get_home_dir()));
	refresh();
}


static gchar *get_default_dir()
{
	const gchar *dir = NULL;

	if (project)
		dir = project->base_path;
	if (NZV(dir))
		return utils->get_locale_from_utf8(dir);

	return g_get_current_dir();
}


static void on_current_path()
{
	gchar *fname;
	gchar *dir;
	gint idx = documents->get_cur_idx();

	if (! DOC_IDX_VALID(idx) || doc_list[idx].file_name == NULL ||
		! g_path_is_absolute(doc_list[idx].file_name))
	{
		setptr(current_dir, get_default_dir());
		refresh();
		return;
	}
	fname = doc_list[idx].file_name;
	fname = utils->get_locale_from_utf8(fname);
	dir = g_path_get_dirname(fname);
	g_free(fname);

	setptr(current_dir, dir);
	refresh();
}


static void handle_selection(GList *list, GtkTreeSelection *treesel, gboolean external)
{
	GList *item;
	GtkTreeModel *model = GTK_TREE_MODEL(file_store);
	GtkTreePath *treepath;
	GtkTreeIter iter;
	gboolean dir_found = FALSE;

	for (item = list; item != NULL; item = g_list_next(item))
	{
		gchar *icon;

		treepath = (GtkTreePath*) item->data;
		gtk_tree_model_get_iter(model, &iter, treepath);
		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_ICON, &icon, -1);

		if (utils->str_equal(icon, GTK_STOCK_DIRECTORY))
		{
			dir_found = TRUE;
			g_free(icon);
			break;
		}
		g_free(icon);
	}

	if (dir_found && gtk_tree_selection_count_selected_rows(treesel) > 1)
	{
		ui->set_statusbar(FALSE, _("Too many items selected!"));
		return;
	}

	for (item = list; item != NULL; item = g_list_next(item))
	{
		gchar *name, *fname, *dir_sep = G_DIR_SEPARATOR_S;

		treepath = (GtkTreePath*) item->data;
		gtk_tree_model_get_iter(model, &iter, treepath);
		gtk_tree_model_get(model, &iter, FILEVIEW_COLUMN_NAME, &name, -1);

		if (utils->str_equal(current_dir, G_DIR_SEPARATOR_S)) /// TODO test this on Windows
			dir_sep = "";
		setptr(name, utils->get_locale_from_utf8(name));
		fname = g_strconcat(current_dir, dir_sep, name, NULL);
		g_free(name);

		if (external)
		{
			gchar *cmd;
			gchar *dir;
			GString *cmd_str = g_string_new(open_cmd);

			if (! dir_found)
				dir = g_path_get_dirname(fname);
			else
				dir = g_strdup(fname);

			utils->string_replace_all(cmd_str, "%f", fname);
			utils->string_replace_all(cmd_str, "%d", dir);

			cmd = g_string_free(cmd_str, FALSE);
			setptr(cmd, utils->get_locale_from_utf8(cmd));
			g_spawn_command_line_async(cmd, NULL);
			g_free(cmd);
			g_free(dir);
			g_free(fname);
		}
		else if (dir_found)
		{
			setptr(current_dir, fname);
			refresh();
			return;
		}
		else
		{
			documents->open_file(fname, FALSE, NULL, NULL);
			g_free(fname);
		}
	}
}


static void open_selected_files(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GList *list;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_view));

	list = gtk_tree_selection_get_selected_rows(treesel, &model);
	handle_selection(list, treesel, GPOINTER_TO_INT(user_data));

	g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free(list);
}


static void on_find_in_files(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkTreeSelection *treesel;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeIter iter;
	GList *list;
	GList *item;
	gchar *icon;
	gchar *dir;
	gchar *name;
	gboolean is_dir = FALSE;

	treesel = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_view));
	if (gtk_tree_selection_count_selected_rows(treesel) != 1)
		return;

	list = gtk_tree_selection_get_selected_rows(treesel, &model);
	for (item = list; item != NULL; item = g_list_next(item))
	{
		treepath = (GtkTreePath*) item->data;
		gtk_tree_model_get_iter(model, &iter, treepath);
		gtk_tree_model_get(model, &iter,
			FILEVIEW_COLUMN_ICON, &icon,
			FILEVIEW_COLUMN_NAME, &name, -1);

		if (utils->str_equal(icon, GTK_STOCK_DIRECTORY))
			is_dir = TRUE;
		g_free(icon);
	}

	g_list_foreach(list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free(list);

	if (is_dir)
		dir = g_strconcat(current_dir, G_DIR_SEPARATOR_S, name, NULL);
	else
		dir = g_strdup(current_dir);

	setptr(dir, utils->get_utf8_from_locale(dir));
	search->show_find_in_files_dialog(dir);
	g_free(dir);
}


static void on_hidden_files_clicked(GtkCheckMenuItem *item)
{
	show_hidden_files = gtk_check_menu_item_get_active(item);
	refresh();
}


static GtkWidget *create_popup_menu()
{
	GtkWidget *item, *menu, *image;

	menu = gtk_menu_new();

	item = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate",
		G_CALLBACK(open_selected_files), GINT_TO_POINTER(FALSE));


	image = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("Open _with ..."));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate",
		G_CALLBACK(open_selected_files), GINT_TO_POINTER(TRUE));

	image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	item = gtk_image_menu_item_new_with_mnemonic(_("_Find in Files"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_find_in_files), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_check_menu_item_new_with_mnemonic(_("Show _Hidden Files"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_hidden_files_clicked), NULL);

	item = gtk_separator_menu_item_new();
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);

	item = gtk_image_menu_item_new_with_mnemonic(_("H_ide Sidebar"));
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
		gtk_image_new_from_stock("gtk-close", GTK_ICON_SIZE_MENU));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_swapped((gpointer) item, "activate",
		G_CALLBACK(keybindings->send_command),
		GINT_TO_POINTER(GEANY_KEYS_MENU_SIDEBAR));

	return menu;
}


static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS)
		open_selected_files(NULL, NULL);
	else
	if (event->button == 3)
	{
		static GtkWidget *popup_menu = NULL;

		if (popup_menu == NULL)
			popup_menu = create_popup_menu();

		gtk_menu_popup(GTK_MENU(popup_menu), NULL, NULL, NULL, NULL,
			event->button, event->time);
		return TRUE;
	}
	return FALSE;
}


static void on_go_up()
{
	// remove the highest directory part (which becomes the basename of current_dir)
	setptr(current_dir, g_path_get_dirname(current_dir));
	refresh();
}


static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_Return
		|| event->keyval == GDK_ISO_Enter
		|| event->keyval == GDK_KP_Enter
		|| event->keyval == GDK_space)
		open_selected_files(NULL, NULL);

	if ((event->keyval == GDK_Up ||
		event->keyval == GDK_KP_Up) &&
		(event->state & GDK_MOD1_MASK))	// FIXME: Alt-Up doesn't seem to work!
		on_go_up();
	return FALSE;
}


static void on_path_entry_activate(GtkEntry *entry, gpointer user_data)
{
	gchar *new_dir = (gchar*) gtk_entry_get_text(entry);
	if (NZV(new_dir))
		new_dir = utils->get_locale_from_utf8(new_dir);
	else
		new_dir = g_strdup(g_get_home_dir());

	setptr(current_dir, new_dir);
	refresh();
}


static void prepare_file_view()
{
	GtkCellRenderer *text_renderer, *icon_renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	PangoFontDescription *pfd;

	file_store = gtk_list_store_new(FILEVIEW_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	gtk_tree_view_set_model(GTK_TREE_VIEW(file_view), GTK_TREE_MODEL(file_store));

	icon_renderer = gtk_cell_renderer_pixbuf_new();
	text_renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
  	gtk_tree_view_column_set_attributes(column, icon_renderer, "stock-id", FILEVIEW_COLUMN_ICON, NULL);
    gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
  	gtk_tree_view_column_set_attributes(column, text_renderer, "text", FILEVIEW_COLUMN_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(file_view), column);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(file_view), FALSE);

	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(file_view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(file_view), FILEVIEW_COLUMN_NAME);

	pfd = pango_font_description_from_string(prefs->tagbar_font);
	gtk_widget_modify_font(file_view, pfd);
	pango_font_description_free(pfd);

	// selection handling
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(file_view));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(file_view), "realize", G_CALLBACK(on_current_path), NULL);
	g_signal_connect(G_OBJECT(file_view), "button-press-event",
		G_CALLBACK(on_button_press), NULL);
	g_signal_connect(G_OBJECT(file_view), "key-press-event",
		G_CALLBACK(on_key_press), NULL);
}


static GtkWidget *make_toolbar()
{
	GtkWidget *wid, *toolbar;
	GtkTooltips *tooltips = GTK_TOOLTIPS(support->lookup_widget(
		app->window, "tooltips"));

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	wid = (GtkWidget *) gtk_tool_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(wid), tooltips,
		_("Up"), NULL);
	g_signal_connect(G_OBJECT(wid), "clicked", G_CALLBACK(on_go_up), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = (GtkWidget *) gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(wid), tooltips,
		_("Refresh"), NULL);
	g_signal_connect(G_OBJECT(wid), "clicked", G_CALLBACK(refresh), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = (GtkWidget *) gtk_tool_button_new_from_stock(GTK_STOCK_HOME);
	gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(wid), tooltips,
		_("Home"), NULL);
	g_signal_connect(G_OBJECT(wid), "clicked", G_CALLBACK(on_go_home), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	wid = (GtkWidget *) gtk_tool_button_new_from_stock(GTK_STOCK_JUMP_TO);
	gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(wid), tooltips,
		_("Set path from document"), NULL);
	g_signal_connect(G_OBJECT(wid), "clicked", G_CALLBACK(on_current_path), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);

	return toolbar;
}


#define CHECK_READ_SETTING(var, error, tmp) \
	if ((error) != NULL) \
	{ \
		g_error_free((error)); \
		(error) = NULL; \
	} \
	else \
		(var) = (tmp);

void init(GeanyData *data)
{
	GtkWidget *scrollwin, *toolbar;
	GKeyFile *config = g_key_file_new();
	GError *error = NULL;
	gboolean tmp;

	file_view_vbox = gtk_vbox_new(FALSE, 0);
	toolbar = make_toolbar();
	gtk_box_pack_start(GTK_BOX(file_view_vbox), toolbar, FALSE, FALSE, 0);

	path_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(file_view_vbox), path_entry, FALSE, FALSE, 2);
	g_signal_connect(G_OBJECT(path_entry), "activate", G_CALLBACK(on_path_entry_activate), NULL);

	file_view = gtk_tree_view_new();
	prepare_file_view();

	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(scrollwin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scrollwin), file_view);
	gtk_container_add(GTK_CONTAINER(file_view_vbox), scrollwin);

	gtk_widget_show_all(file_view_vbox);
	gtk_notebook_append_page(GTK_NOTEBOOK(app->treeview_notebook), file_view_vbox,
		gtk_label_new(_("Files")));

	config_file = g_strconcat(app->configdir, G_DIR_SEPARATOR_S, "plugins", G_DIR_SEPARATOR_S,
		"filebrowser", G_DIR_SEPARATOR_S, "filebrowser.conf", NULL);
	g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);
	open_cmd = g_key_file_get_string(config, "filebrowser", "open_command", &error);
	if (error != NULL)
	{
		open_cmd = g_strdup("nautilus \"%d\"");
		g_error_free(error);
		error = NULL;
	}
	tmp = g_key_file_get_boolean(config, "filebrowser", "show_hidden_files", &error);
	CHECK_READ_SETTING(show_hidden_files, error, tmp);
	tmp = g_key_file_get_boolean(config, "filebrowser", "hide_object_files", &error);
	CHECK_READ_SETTING(hide_object_files, error, tmp);

	g_key_file_free(config);
}


void configure(GtkWidget *parent)
{
	GtkWidget *dialog, *label, *entry, *checkbox_of, *checkbox_hf, *vbox;
	GtkTooltips *tooltips = gtk_tooltips_new();

	dialog = gtk_dialog_new_with_buttons(_("File Browser"),
		GTK_WINDOW(parent), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui->dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	label = gtk_label_new("External open command:");
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	entry = gtk_entry_new();
	gtk_widget_show(entry);
	if (open_cmd != NULL)
		gtk_entry_set_text(GTK_ENTRY(entry), open_cmd);
	gtk_tooltips_set_tip(tooltips, entry,
		_("The command to execute when using \"Open with\". You can use %f and %d wildcards.\n"
		  "%f will be replaced with the filename including full path\n"
		  "%d will be replaced with the path name of the selected file without the filename"),
		  NULL);
	gtk_container_add(GTK_CONTAINER(vbox), entry);

	checkbox_hf = gtk_check_button_new_with_label(_("Show hidden files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_hf), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_hf), show_hidden_files);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox_hf, FALSE, FALSE, 5);

	checkbox_of = gtk_check_button_new_with_label(_("Hide object files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox_of), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox_of), hide_object_files);
	gtk_tooltips_set_tip(tooltips, checkbox_of,
		_("Don't show generated object files in the file browser, this includes "
		  "*.o, *.obj. *.so, *.dll, *.a, *.lib"),
		  NULL);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox_of, FALSE, FALSE, 5);


	gtk_widget_show_all(vbox);

	// run the dialog and check for the response code
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		GKeyFile *config = g_key_file_new();
		gchar *data;
		gchar *config_dir = g_path_get_dirname(config_file);

		g_free(open_cmd);
		open_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
		show_hidden_files = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_hf));
		hide_object_files = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox_of));

		g_key_file_load_from_file(config, config_file, G_KEY_FILE_NONE, NULL);

		g_key_file_set_string(config, "filebrowser", "open_command", open_cmd);
		g_key_file_set_boolean(config, "filebrowser", "show_hidden_files", show_hidden_files);
		g_key_file_set_boolean(config, "filebrowser", "hide_object_files", hide_object_files);

		if (! g_file_test(config_dir, G_FILE_TEST_IS_DIR) && utils->mkdir(config_dir, TRUE) != 0)
		{
			dialogs->show_msgbox(GTK_MESSAGE_ERROR,
				_("Plugin configuration directory could not be created."));
		}
		else
		{
			// write config to file
			data = g_key_file_to_data(config, NULL, NULL);
			utils->write_file(config_file, data);
			g_free(data);
		}

		// apply the changes
		refresh();

		g_free(config_dir);
		g_key_file_free(config);
	}
	gtk_widget_destroy(dialog);
}


void cleanup()
{
	g_free(config_file);
	g_free(open_cmd);
	gtk_widget_destroy(file_view_vbox);
}
