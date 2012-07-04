/*
 *      dialogs.c - this file is part of Geany, a fast and lightweight IDE
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
 * File related dialogs, miscellaneous dialogs, font dialog.
 */

#include "geany.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

/* gstdio.h also includes sys/stat.h */
#include <glib/gstdio.h>

#include "dialogs.h"

#include "callbacks.h"
#include "document.h"
#include "filetypes.h"
#include "win32.h"
#include "sciwrappers.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "keybindings.h"
#include "encodings.h"
#include "build.h"
#include "main.h"
#include "project.h"


enum
{
	GEANY_RESPONSE_RENAME,
	GEANY_RESPONSE_VIEW
};


static struct FileSelState
{
	struct
	{
		guint filter_idx;
		gint encoding_idx;
		gint filetype_idx;
		gboolean show_hidden;
		gboolean more_options_visible;
	} open;
	struct
	{
		gboolean open_in_new_tab;
	} save;
}
filesel_state = {
	{
		0,
		GEANY_ENCODINGS_MAX, /* default encoding is detect from file */
		0,
		FALSE,
		FALSE
	},
	{
		FALSE
	}
};


/* gets the ID of the current file filter */
static guint file_chooser_get_filter_idx(GtkFileChooser *chooser)
{
	guint idx = 0;
	GtkFileFilter *current;
	GSList *filters, *item;

	current = gtk_file_chooser_get_filter(chooser);
	filters = gtk_file_chooser_list_filters(chooser);
	foreach_slist(item, filters)
	{
		if (item->data == current)
			break;
		idx ++;
	}
	g_slist_free(filters);
	return idx;
}


/* sets the current file filter from its ID */
static void file_chooser_set_filter_idx(GtkFileChooser *chooser, guint idx)
{
	GtkFileFilter *current;
	GSList *filters;

	filters = gtk_file_chooser_list_filters(chooser);
	current = g_slist_nth_data(filters, idx);
	g_slist_free(filters);
	gtk_file_chooser_set_filter(chooser, current);
}


static void open_file_dialog_handle_response(GtkWidget *dialog, gint response)
{
	if (response == GTK_RESPONSE_ACCEPT || response == GEANY_RESPONSE_VIEW)
	{
		GSList *filelist;
		GtkTreeModel *encoding_model;
		GtkTreeIter encoding_iter;
		GeanyFiletype *ft = NULL;
		const gchar *charset = NULL;
		GtkWidget *expander = ui_lookup_widget(dialog, "more_options_expander");
		GtkWidget *filetype_combo = ui_lookup_widget(dialog, "filetype_combo");
		GtkWidget *encoding_combo = ui_lookup_widget(dialog, "encoding_combo");
		gboolean ro = (response == GEANY_RESPONSE_VIEW);	/* View clicked */

		filesel_state.open.more_options_visible = gtk_expander_get_expanded(GTK_EXPANDER(expander));
		filesel_state.open.filter_idx = file_chooser_get_filter_idx(GTK_FILE_CHOOSER(dialog));
		filesel_state.open.filetype_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(filetype_combo));

		/* ignore detect from file item */
		if (filesel_state.open.filetype_idx > 0)
			ft = g_slist_nth_data(filetypes_by_title, (guint) filesel_state.open.filetype_idx);

		encoding_model = gtk_combo_box_get_model(GTK_COMBO_BOX(encoding_combo));
		gtk_combo_box_get_active_iter(GTK_COMBO_BOX(encoding_combo), &encoding_iter);
		gtk_tree_model_get(encoding_model, &encoding_iter, 0, &filesel_state.open.encoding_idx, -1);
		if (filesel_state.open.encoding_idx >= 0 && filesel_state.open.encoding_idx < GEANY_ENCODINGS_MAX)
			charset = encodings[filesel_state.open.encoding_idx].charset;

		filelist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
		if (filelist != NULL)
		{
			document_open_files(filelist, ro, ft, charset);
			g_slist_foreach(filelist, (GFunc) g_free, NULL);	/* free filenames */
		}
		g_slist_free(filelist);
	}
	if (app->project && NZV(app->project->base_path))
		gtk_file_chooser_remove_shortcut_folder(GTK_FILE_CHOOSER(dialog),
			app->project->base_path, NULL);
}


static void on_file_open_show_hidden_notify(GObject *filechooser,
	GParamSpec *pspec, gpointer data)
{
	GtkWidget *toggle_button;

	toggle_button = ui_lookup_widget(GTK_WIDGET(filechooser), "check_hidden");

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button),
		gtk_file_chooser_get_show_hidden(GTK_FILE_CHOOSER(filechooser)));
}


static void
on_file_open_check_hidden_toggled(GtkToggleButton *togglebutton, GtkWidget *dialog)
{
	filesel_state.open.show_hidden = gtk_toggle_button_get_active(togglebutton);
	gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), filesel_state.open.show_hidden);
}


static gint encoding_combo_store_sort_func(GtkTreeModel *model,
										   GtkTreeIter *a,
										   GtkTreeIter *b,
										   gpointer data)
{
	gboolean a_has_child = gtk_tree_model_iter_has_child(model, a);
	gboolean b_has_child = gtk_tree_model_iter_has_child(model, b);
	gchar *a_string;
	gchar *b_string;
	gint cmp_res;

	if (a_has_child != b_has_child)
		return a_has_child ? -1 : 1;

	gtk_tree_model_get(model, a, 1, &a_string, -1);
	gtk_tree_model_get(model, b, 1, &b_string, -1);
	cmp_res = strcmp(a_string, b_string);
	g_free(a_string);
	g_free(b_string);
	return cmp_res;
}


static GtkTreeStore *create_encoding_combo_store(GtkTreeIter *iter_detect)
{
	GtkTreeStore *store;
	GtkTreeIter iter_current, iter_westeuro, iter_easteuro, iter_eastasian, iter_asian,
				iter_utf8, iter_middleeast;
	GtkTreeIter *iter_parent;
	gchar *encoding_string;
	gint i;

	store = gtk_tree_store_new(2, G_TYPE_INT, G_TYPE_STRING);

	gtk_tree_store_append(store, iter_detect, NULL);
	gtk_tree_store_set(store, iter_detect, 0, GEANY_ENCODINGS_MAX, 1, _("Detect from file"), -1);

	gtk_tree_store_append(store, &iter_westeuro, NULL);
	gtk_tree_store_set(store, &iter_westeuro, 0, -1, 1, _("West European"), -1);
	gtk_tree_store_append(store, &iter_easteuro, NULL);
	gtk_tree_store_set(store, &iter_easteuro, 0, -1, 1, _("East European"), -1);
	gtk_tree_store_append(store, &iter_eastasian, NULL);
	gtk_tree_store_set(store, &iter_eastasian, 0, -1, 1, _("East Asian"), -1);
	gtk_tree_store_append(store, &iter_asian, NULL);
	gtk_tree_store_set(store, &iter_asian, 0, -1, 1, _("SE & SW Asian"), -1);
	gtk_tree_store_append(store, &iter_middleeast, NULL);
	gtk_tree_store_set(store, &iter_middleeast, 0, -1, 1, _("Middle Eastern"), -1);
	gtk_tree_store_append(store, &iter_utf8, NULL);
	gtk_tree_store_set(store, &iter_utf8, 0, -1, 1, _("Unicode"), -1);

	for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
	{
		switch (encodings[i].group)
		{
			case WESTEUROPEAN: iter_parent = &iter_westeuro; break;
			case EASTEUROPEAN: iter_parent = &iter_easteuro; break;
			case EASTASIAN: iter_parent = &iter_eastasian; break;
			case ASIAN: iter_parent = &iter_asian; break;
			case MIDDLEEASTERN: iter_parent = &iter_middleeast; break;
			case UNICODE: iter_parent = &iter_utf8; break;
			case NONE:
			default: iter_parent = NULL;
		}
		gtk_tree_store_append(store, &iter_current, iter_parent);
		encoding_string = encodings_to_string(&encodings[i]);
		gtk_tree_store_set(store, &iter_current, 0, i, 1, encoding_string, -1);
		g_free(encoding_string);
		/* restore the saved state */
		if (i == filesel_state.open.encoding_idx)
			*iter_detect = iter_current;
	}

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 1, GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), 1, encoding_combo_store_sort_func, NULL, NULL);

	return store;
}


static void encoding_combo_cell_data_func(GtkCellLayout *cell_layout,
										  GtkCellRenderer *cell,
										  GtkTreeModel *tree_model,
										  GtkTreeIter *iter,
										  gpointer data)
{
	gboolean sensitive = !gtk_tree_model_iter_has_child(tree_model, iter);

	g_object_set(cell, "sensitive", sensitive, NULL);
}


static GtkWidget *add_file_open_extra_widget(GtkWidget *dialog)
{
	GtkWidget *expander, *vbox, *table, *check_hidden;
	GtkWidget *filetype_ebox, *filetype_label, *filetype_combo;
	GtkWidget *encoding_ebox, *encoding_label, *encoding_combo;

	expander = gtk_expander_new_with_mnemonic(_("_More Options"));
	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(expander), vbox);

	table = gtk_table_new(2, 4, FALSE);

	/* line 1 with checkbox and encoding combo */
	check_hidden = gtk_check_button_new_with_mnemonic(_("Show _hidden files"));
	gtk_widget_show(check_hidden);
	gtk_table_attach(GTK_TABLE(table), check_hidden, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
					(GtkAttachOptions) (0), 0, 5);

	/* spacing */
	gtk_table_attach(GTK_TABLE(table), gtk_label_new(""), 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 5, 5);

	encoding_label = gtk_label_new(_("Set encoding:"));
	gtk_misc_set_alignment(GTK_MISC(encoding_label), 1, 0);
	gtk_table_attach(GTK_TABLE(table), encoding_label, 2, 3, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 4, 5);
	/* the ebox is for the tooltip, because gtk_combo_box can't show tooltips */
	encoding_ebox = gtk_event_box_new();
	encoding_combo = gtk_combo_box_new();
	gtk_widget_set_tooltip_text(encoding_ebox,
		_("Explicitly defines an encoding for the file, if it would not be detected. This is useful when you know that the encoding of a file cannot be detected correctly by Geany.\nNote if you choose multiple files, they will all be opened with the chosen encoding."));
	gtk_container_add(GTK_CONTAINER(encoding_ebox), encoding_combo);
	gtk_table_attach(GTK_TABLE(table), encoding_ebox, 3, 4, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 5);

	/* line 2 with filetype combo */
	filetype_label = gtk_label_new(_("Set filetype:"));
	gtk_misc_set_alignment(GTK_MISC(filetype_label), 1, 0);
	gtk_table_attach(GTK_TABLE(table), filetype_label, 2, 3, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 4, 5);
	/* the ebox is for the tooltip, because gtk_combo_box can't show tooltips */
	filetype_ebox = gtk_event_box_new();
	filetype_combo = gtk_combo_box_new_text();
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(filetype_combo), 2);
	gtk_widget_set_tooltip_text(filetype_ebox,
		_("Explicitly defines a filetype for the file, if it would not be detected by filename extension.\nNote if you choose multiple files, they will all be opened with the chosen filetype."));
	gtk_container_add(GTK_CONTAINER(filetype_ebox), filetype_combo);
	gtk_table_attach(GTK_TABLE(table), filetype_ebox, 3, 4, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 5);

	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);

	g_signal_connect(check_hidden, "toggled", G_CALLBACK(on_file_open_check_hidden_toggled), dialog);

	ui_hookup_widget(dialog, expander, "more_options_expander");
	ui_hookup_widget(dialog, check_hidden, "check_hidden");
	ui_hookup_widget(dialog, filetype_combo, "filetype_combo");
	ui_hookup_widget(dialog, encoding_combo, "encoding_combo");

	return expander;
}


static GtkWidget *create_open_file_dialog(void)
{
	GtkWidget *dialog;
	GtkWidget *filetype_combo, *encoding_combo;
	GtkWidget *viewbtn;
	GtkCellRenderer *encoding_renderer;
	GtkTreeIter encoding_iter;
	GSList *node;

	dialog = gtk_file_chooser_dialog_new(_("Open File"), GTK_WINDOW(main_widgets.window),
			GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
	gtk_widget_set_name(dialog, "GeanyDialog");

	viewbtn = gtk_dialog_add_button(GTK_DIALOG(dialog), _("_View"), GEANY_RESPONSE_VIEW);
	gtk_widget_set_tooltip_text(viewbtn,
		_("Opens the file in read-only mode. If you choose more than one file to open, all files will be opened read-only."));

	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	gtk_widget_set_size_request(dialog, -1, 460);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_widgets.window));
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), FALSE);

	/* add checkboxes and filename entry */
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), add_file_open_extra_widget(dialog));
	filetype_combo = ui_lookup_widget(dialog, "filetype_combo");

	gtk_combo_box_append_text(GTK_COMBO_BOX(filetype_combo), _("Detect by file extension"));
	/* add FileFilters(start with "All Files") */
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),
				filetypes_create_file_filter(filetypes[GEANY_FILETYPES_NONE]));
	/* now create meta filter "All Source" */
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog),
				filetypes_create_file_filter_all_source());
	foreach_slist(node, filetypes_by_title)
	{
		GeanyFiletype *ft = node->data;

		if (G_UNLIKELY(ft->id == GEANY_FILETYPES_NONE))
			continue;
		gtk_combo_box_append_text(GTK_COMBO_BOX(filetype_combo), ft->title);
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filetypes_create_file_filter(ft));
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(filetype_combo), 0);

	/* fill encoding combo box */
	encoding_combo = ui_lookup_widget(dialog, "encoding_combo");
	gtk_combo_box_set_model(GTK_COMBO_BOX(encoding_combo), GTK_TREE_MODEL(
			create_encoding_combo_store(&encoding_iter)));
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(encoding_combo), &encoding_iter);
	encoding_renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(encoding_combo), encoding_renderer, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(encoding_combo), encoding_renderer, "text", 1);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(encoding_combo), encoding_renderer,
			encoding_combo_cell_data_func, NULL, NULL);

	g_signal_connect(dialog, "notify::show-hidden",
		G_CALLBACK(on_file_open_show_hidden_notify), NULL);

	return dialog;
}


static void open_file_dialog_apply_settings(GtkWidget *dialog)
{
	static gboolean initialized = FALSE;
	GtkWidget *check_hidden = ui_lookup_widget(dialog, "check_hidden");
	GtkWidget *filetype_combo = ui_lookup_widget(dialog, "filetype_combo");
	GtkWidget *expander = ui_lookup_widget(dialog, "more_options_expander");

	/* we can't know the initial position of combo boxes, so retreive it the first time */
	if (! initialized)
	{
		filesel_state.open.filter_idx = file_chooser_get_filter_idx(GTK_FILE_CHOOSER(dialog));
		filesel_state.open.filetype_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(filetype_combo));

		initialized = TRUE;
	}
	else
	{
		file_chooser_set_filter_idx(GTK_FILE_CHOOSER(dialog), filesel_state.open.filter_idx);
		gtk_combo_box_set_active(GTK_COMBO_BOX(filetype_combo), filesel_state.open.filetype_idx);
	}
	gtk_expander_set_expanded(GTK_EXPANDER(expander), filesel_state.open.more_options_visible);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_hidden), filesel_state.open.show_hidden);
	/* encoding combo is restored at creating time, see create_encoding_combo_store() */
}


/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file(void)
{
	gchar *initdir;

	/* set dialog directory to the current file's directory, if present */
	initdir = utils_get_current_file_dir_utf8();

	/* use project or default startup directory (if set) if no files are open */
	/** TODO should it only be used when initally open the dialog and not on every show? */
	if (! initdir)
		initdir = g_strdup(utils_get_default_dir_utf8());

	SETPTR(initdir, utils_get_locale_from_utf8(initdir));

#ifdef G_OS_WIN32
	if (interface_prefs.use_native_windows_dialogs)
		win32_show_document_open_dialog(GTK_WINDOW(main_widgets.window), _("Open File"), initdir);
	else
#endif
	{
		GtkWidget *dialog = create_open_file_dialog();
		gint response;

		open_file_dialog_apply_settings(dialog);

		if (initdir != NULL && g_path_is_absolute(initdir))
				gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), initdir);

		if (app->project && NZV(app->project->base_path))
			gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(dialog),
					app->project->base_path, NULL);

		response = gtk_dialog_run(GTK_DIALOG(dialog));
		open_file_dialog_handle_response(dialog, response);
		gtk_widget_destroy(dialog);
	}
	g_free(initdir);
}


static void on_save_as_new_tab_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(user_data), ! gtk_toggle_button_get_active(togglebutton));
}


static gboolean handle_save_as(const gchar *utf8_filename, gboolean open_new_tab, gboolean rename_file)
{
	GeanyDocument *doc = document_get_current();
	gboolean success = FALSE;

	g_return_val_if_fail(NZV(utf8_filename), FALSE);

	if (open_new_tab)
	{	/* "open" the saved file in a new tab and switch to it */
		doc = document_clone(doc, utf8_filename);
		success = document_save_file_as(doc, NULL);
	}
	else
	{
		if (doc->file_name != NULL)
		{
			if (rename_file)
			{
				document_rename_file(doc, utf8_filename);
			}
			/* create a new tm_source_file object otherwise tagmanager won't work correctly */
			tm_workspace_remove_object(doc->tm_file, TRUE, TRUE);
			doc->tm_file = NULL;
		}
		success = document_save_file_as(doc, utf8_filename);

		build_menu_update(doc);
	}
	return success;
}


static gboolean save_as_dialog_handle_response(GtkWidget *dialog, gint response)
{
	gboolean rename_file = FALSE;
	gboolean success = FALSE;
	gchar *new_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

	switch (response)
	{
		case GEANY_RESPONSE_RENAME:
			/* rename doesn't check for empty filename or overwriting */
			if (G_UNLIKELY(! NZV(new_filename)))
			{
				utils_beep();
				break;
			}
			if (g_file_test(new_filename, G_FILE_TEST_EXISTS) &&
				!dialogs_show_question_full(NULL, NULL, NULL,
					_("Overwrite?"),
					_("Filename already exists!")))
				break;
			rename_file = TRUE;
			/* fall through */
		case GTK_RESPONSE_ACCEPT:
		{
			gboolean open_new_tab = gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(ui_lookup_widget(dialog, "check_open_new_tab")));
			gchar *utf8_filename;

			utf8_filename = utils_get_utf8_from_locale(new_filename);
			success = handle_save_as(utf8_filename, open_new_tab, rename_file);

			if (success)
				filesel_state.save.open_in_new_tab = open_new_tab;

			g_free(utf8_filename);
			break;
		}
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			success = TRUE;
			break;
	}
	g_free(new_filename);

	return success;
}


static GtkWidget *create_save_file_dialog(void)
{
	GtkWidget *dialog, *vbox, *check_open_new_tab, *rename_btn;
	const gchar *initdir;

	dialog = gtk_file_chooser_dialog_new(_("Save File"), GTK_WINDOW(main_widgets.window),
				GTK_FILE_CHOOSER_ACTION_SAVE, NULL, NULL);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_widgets.window));
	gtk_widget_set_name(dialog, "GeanyDialog");

	rename_btn = gtk_dialog_add_button(GTK_DIALOG(dialog), _("R_ename"), GEANY_RESPONSE_RENAME);
	gtk_widget_set_tooltip_text(rename_btn, _("Save the file and rename it"));

	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	vbox = gtk_vbox_new(FALSE, 0);
	check_open_new_tab = gtk_check_button_new_with_mnemonic(_("_Open file in a new tab"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_open_new_tab), filesel_state.save.open_in_new_tab);
	gtk_widget_set_tooltip_text(check_open_new_tab,
		_("Keep the current unsaved document open"
		" and open the newly saved file in a new tab"));
	gtk_box_pack_start(GTK_BOX(vbox), check_open_new_tab, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);
	gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(dialog), vbox);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), FALSE);

	/* set the folder by default to the project base dir or the global pref for opening files */
	initdir = utils_get_default_dir_utf8();
	if (initdir)
	{
		gchar *linitdir = utils_get_locale_from_utf8(initdir);
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), linitdir);
		g_free(linitdir);
	}

	g_signal_connect(check_open_new_tab, "toggled",
				G_CALLBACK(on_save_as_new_tab_toggled), rename_btn);

	ui_hookup_widget(dialog, check_open_new_tab, "check_open_new_tab");

	return dialog;
}


static gboolean show_save_as_gtk(GeanyDocument *doc)
{
	GtkWidget *dialog;
	gint resp;

	g_return_val_if_fail(doc != NULL, FALSE);

	dialog = create_save_file_dialog();

	if (doc->file_name != NULL)
	{
		if (g_path_is_absolute(doc->file_name))
		{
			gchar *locale_filename = utils_get_locale_from_utf8(doc->file_name);
			gchar *locale_basename = g_path_get_basename(locale_filename);
			gchar *locale_dirname = g_path_get_dirname(locale_filename);

			gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), locale_dirname);
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), locale_basename);

			g_free(locale_filename);
			g_free(locale_basename);
			g_free(locale_dirname);
		}
		else
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), doc->file_name);
	}
	else
	{
		gchar *fname = NULL;

		if (doc->file_type != NULL && doc->file_type->extension != NULL)
			fname = g_strconcat(GEANY_STRING_UNTITLED, ".",
								doc->file_type->extension, NULL);
		else
			fname = g_strdup(GEANY_STRING_UNTITLED);

		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), fname);

		g_free(fname);
	}

	if (app->project && NZV(app->project->base_path))
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(dialog),
			app->project->base_path, NULL);

	/* Run the dialog synchronously, pausing this function call */
	do
	{
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
	}
	while (! save_as_dialog_handle_response(dialog, resp));

	if (app->project && NZV(app->project->base_path))
		gtk_file_chooser_remove_shortcut_folder(GTK_FILE_CHOOSER(dialog),
			app->project->base_path, NULL);

	gtk_widget_destroy(dialog);

	return (resp == GTK_RESPONSE_ACCEPT);
}


/**
 *  Shows the Save As dialog for the current notebook page.
 *
 *  @return @c TRUE if the file was saved, otherwise @c FALSE.
 **/
gboolean dialogs_show_save_as()
{
	GeanyDocument *doc = document_get_current();
	gboolean result = FALSE;

	g_return_val_if_fail(doc, FALSE);

#ifdef G_OS_WIN32
	if (interface_prefs.use_native_windows_dialogs)
	{
		gchar *utf8_name = win32_show_document_save_as_dialog(GTK_WINDOW(main_widgets.window),
						_("Save File"), DOC_FILENAME(doc));
		if (utf8_name != NULL)
			result = handle_save_as(utf8_name, FALSE, FALSE);
	}
	else
#endif
	result = show_save_as_gtk(doc);
	return result;
}


#ifndef G_OS_WIN32
static void show_msgbox_dialog(GtkWidget *dialog, GtkMessageType type, GtkWindow *parent)
{
	const gchar *title;
	switch (type)
	{
		case GTK_MESSAGE_ERROR:
			title = _("Error");
			break;
		case GTK_MESSAGE_QUESTION:
			title = _("Question");
			break;
		case GTK_MESSAGE_WARNING:
			title = _("Warning");
			break;
		default:
			title = _("Information");
			break;
	}
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	if (parent == NULL || GTK_IS_DIALOG(parent))
	{
		GdkPixbuf *pb = ui_new_pixbuf_from_inline(GEANY_IMAGE_LOGO);
		gtk_window_set_icon(GTK_WINDOW(dialog), pb);
		g_object_unref(pb);
	}
	gtk_widget_set_name(dialog, "GeanyDialog");

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}
#endif


/**
 *  Shows a message box of the type @a type with @a text.
 *  On Unix-like systems a GTK message dialog box is shown, on Win32 systems a native Windows
 *  message dialog box is shown.
 *
 *  @param type A @c GtkMessageType, e.g. @c GTK_MESSAGE_INFO, @c GTK_MESSAGE_WARNING,
 *              @c GTK_MESSAGE_QUESTION, @c GTK_MESSAGE_ERROR.
 *  @param text Printf()-style format string.
 *  @param ... Arguments for the @a text format string.
 **/
void dialogs_show_msgbox(GtkMessageType type, const gchar *text, ...)
{
#ifndef G_OS_WIN32
	GtkWidget *dialog;
#endif
	gchar *string;
	va_list args;
	GtkWindow *parent = (main_status.main_window_realized) ? GTK_WINDOW(main_widgets.window) : NULL;

	va_start(args, text);
	string = g_strdup_vprintf(text, args);
	va_end(args);

#ifdef G_OS_WIN32
	win32_message_dialog(GTK_WIDGET(parent), type, string);
#else
	dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
			type, GTK_BUTTONS_OK, "%s", string);
	show_msgbox_dialog(dialog, type, parent);
#endif
	g_free(string);
}


void dialogs_show_msgbox_with_secondary(GtkMessageType type, const gchar *text, const gchar *secondary)
{
	GtkWindow *parent = (main_status.main_window_realized) ? GTK_WINDOW(main_widgets.window) : NULL;
#ifdef G_OS_WIN32
	/* put the two strings together because Windows message boxes don't support secondary texts */
	gchar *string = g_strconcat(text, "\n", secondary, NULL);
	win32_message_dialog(GTK_WIDGET(parent), type, string);
	g_free(string);
#else
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
			type, GTK_BUTTONS_OK, "%s", text);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondary);
	show_msgbox_dialog(dialog, type, parent);
#endif
}


static gint run_unsaved_dialog(const gchar *msg, const gchar *msg2)
{
	GtkWidget *dialog, *button;
	gint ret;

	dialog = gtk_message_dialog_new(GTK_WINDOW(main_widgets.window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", msg);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", msg2);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = ui_button_new_with_image(GTK_STOCK_CLEAR, _("_Don't save"));
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_NO);
	gtk_widget_show(button);

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);

	return ret;
}


gboolean dialogs_show_unsaved_file(GeanyDocument *doc)
{
	gchar *msg, *short_fn = NULL;
	const gchar *msg2;
	gint response;
	gboolean old_quitting_state = main_status.quitting;

	/* display the file tab to remind the user of the document */
	main_status.quitting = FALSE;
	document_show_tab(doc);
	main_status.quitting = old_quitting_state;

	short_fn = document_get_basename_for_display(doc, -1);

	msg = g_strdup_printf(_("The file '%s' is not saved."), short_fn);
	msg2 = _("Do you want to save it before closing?");
	g_free(short_fn);

	response = run_unsaved_dialog(msg, msg2);
	g_free(msg);

	switch (response)
	{
		case GTK_RESPONSE_YES:
			/* document_save_file() returns the status if the file could be saved */
			return document_save_file(doc, FALSE);

		case GTK_RESPONSE_NO:
			return TRUE;

		case GTK_RESPONSE_CANCEL: /* fall through to default and leave the function */
		default:
			return FALSE;
	}
}


#ifndef G_OS_WIN32
static void
on_font_apply_button_clicked(GtkButton *button, gpointer user_data)
{
	gchar *fontname;

	fontname = gtk_font_selection_dialog_get_font_name(
		GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel));
	ui_set_editor_font(fontname);
	g_free(fontname);
}


static void
on_font_ok_button_clicked(GtkButton *button, gpointer user_data)
{
	/* We do the same thing as apply, but we close the dialog after. */
	on_font_apply_button_clicked(button, NULL);
	gtk_widget_hide(ui_widgets.open_fontsel);
}


static void
on_font_cancel_button_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(ui_widgets.open_fontsel);
}
#endif


/* This shows the font selection dialog to choose a font. */
void dialogs_show_open_font()
{
#ifdef G_OS_WIN32
	win32_show_font_dialog();
#else

	if (ui_widgets.open_fontsel == NULL)
	{
		ui_widgets.open_fontsel = gtk_font_selection_dialog_new(_("Choose font"));;
		gtk_container_set_border_width(GTK_CONTAINER(ui_widgets.open_fontsel), 4);
		gtk_window_set_modal(GTK_WINDOW(ui_widgets.open_fontsel), TRUE);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(ui_widgets.open_fontsel), TRUE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(ui_widgets.open_fontsel), TRUE);
		gtk_window_set_type_hint(GTK_WINDOW(ui_widgets.open_fontsel), GDK_WINDOW_TYPE_HINT_DIALOG);
		gtk_widget_set_name(ui_widgets.open_fontsel, "GeanyDialog");

		gtk_widget_show(GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel)->apply_button);

		g_signal_connect(ui_widgets.open_fontsel,
					"delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
		g_signal_connect(GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel)->ok_button,
					"clicked", G_CALLBACK(on_font_ok_button_clicked), NULL);
		g_signal_connect(GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel)->cancel_button,
					"clicked", G_CALLBACK(on_font_cancel_button_clicked), NULL);
		g_signal_connect(GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel)->apply_button,
					"clicked", G_CALLBACK(on_font_apply_button_clicked), NULL);

		gtk_window_set_transient_for(GTK_WINDOW(ui_widgets.open_fontsel), GTK_WINDOW(main_widgets.window));
	}
	gtk_font_selection_dialog_set_font_name(
		GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel), interface_prefs.editor_font);
	/* We make sure the dialog is visible. */
	gtk_window_present(GTK_WINDOW(ui_widgets.open_fontsel));
#endif
}


static void
on_input_dialog_show(GtkDialog *dialog, GtkWidget *entry)
{
	gtk_widget_grab_focus(entry);
}


static void
on_input_entry_activate(GtkEntry *entry, GtkDialog *dialog)
{
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}


static void
on_input_numeric_activate(GtkEntry *entry, GtkDialog *dialog)
{
	gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}


static void
on_input_dialog_response(GtkDialog *dialog, gint response, GtkWidget *entry)
{
	gboolean persistent = (gboolean) GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "has_combo"));

	if (response == GTK_RESPONSE_ACCEPT)
	{
		const gchar *str = gtk_entry_get_text(GTK_ENTRY(entry));
		GeanyInputCallback input_cb =
			(GeanyInputCallback) g_object_get_data(G_OBJECT(dialog), "input_cb");

		if (persistent)
		{
			GtkWidget *combo = (GtkWidget *) g_object_get_data(G_OBJECT(dialog), "combo");
			ui_combo_box_add_to_history(GTK_COMBO_BOX_ENTRY(combo), str, 0);
		}
		input_cb(str);
	}
	gtk_widget_hide(GTK_WIDGET(dialog));
}


static void add_input_widgets(GtkWidget *dialog, GtkWidget *vbox,
		const gchar *label_text, const gchar *default_text, gboolean persistent,
		GCallback insert_text_cb)
{
	GtkWidget *entry;

	if (label_text)
	{
		GtkWidget *label = gtk_label_new(label_text);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_container_add(GTK_CONTAINER(vbox), label);
	}

	if (persistent)	/* remember previous entry text in a combo box */
	{
		GtkWidget *combo = gtk_combo_box_entry_new_text();

		entry = gtk_bin_get_child(GTK_BIN(combo));
		ui_entry_add_clear_icon(GTK_ENTRY(entry));
		g_object_set_data(G_OBJECT(dialog), "combo", combo);
		gtk_container_add(GTK_CONTAINER(vbox), combo);
	}
	else
	{
		entry = gtk_entry_new();
		ui_entry_add_clear_icon(GTK_ENTRY(entry));
		gtk_container_add(GTK_CONTAINER(vbox), entry);
	}

	if (default_text != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), default_text);
	}
	gtk_entry_set_max_length(GTK_ENTRY(entry), 255);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);

	if (insert_text_cb != NULL)
		g_signal_connect(entry, "insert-text", insert_text_cb, NULL);
	g_signal_connect(entry, "activate", G_CALLBACK(on_input_entry_activate), dialog);
	g_signal_connect(dialog, "show", G_CALLBACK(on_input_dialog_show), entry);
	g_signal_connect(dialog, "response", G_CALLBACK(on_input_dialog_response), entry);
}


/* Create and display an input dialog.
 * persistent: whether to remember previous entry text in a combo box;
 * 	in this case the dialog returned is not destroyed on a response,
 * 	and can be reshown.
 * Returns: the dialog widget. */
static GtkWidget *
dialogs_show_input_full(const gchar *title, GtkWindow *parent,
						const gchar *label_text, const gchar *default_text,
						gboolean persistent, GeanyInputCallback input_cb, GCallback insert_text_cb)
{
	GtkWidget *dialog, *vbox;

	dialog = gtk_dialog_new_with_buttons(title, parent,
		GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	g_object_set_data(G_OBJECT(dialog), "has_combo", GINT_TO_POINTER(persistent));
	g_object_set_data(G_OBJECT(dialog), "input_cb", (gpointer) input_cb);

	add_input_widgets(dialog, vbox, label_text, default_text, persistent, insert_text_cb);

	if (persistent)
	{
		/* override default handler */
		g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
		gtk_widget_show_all(dialog);
		return dialog;
	}
	gtk_widget_show_all(dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return NULL;
}


/* Remember previous entry text in a combo box.
 * Returns: the dialog widget. */
GtkWidget *
dialogs_show_input_persistent(const gchar *title, GtkWindow *parent,
		const gchar *label_text, const gchar *default_text,
		GeanyInputCallback input_cb)
{
	return dialogs_show_input_full(title, parent, label_text, default_text, TRUE, input_cb, NULL);
}


/* ugly hack - user_data not supported for callback */
static gchar *dialog_input = NULL;

static void on_dialog_input(const gchar *str)
{
	dialog_input = g_strdup(str);
}


/** Asks the user for text input.
 * @param title Dialog title.
 * @param parent The currently focused window, usually @c geany->main_widgets->window.
 * 	@c NULL can be used but is discouraged due to window manager effects.
 * @param label_text Label text, or @c NULL.
 * @param default_text Text to display in the input field, or @c NULL.
 * @return New copy of user input or @c NULL if cancelled.
 * @since 0.20. */
gchar *dialogs_show_input(const gchar *title, GtkWindow *parent, const gchar *label_text,
	const gchar *default_text)
{
	dialog_input = NULL;
	dialogs_show_input_full(title, parent, label_text, default_text, FALSE, on_dialog_input, NULL);
	return dialog_input;
}


/* Note: could be changed to dialogs_show_validated_input with argument for callback. */
/* Returns: newly allocated copy of the entry text or NULL on cancel.
 * Specialised variant for Goto Line dialog. */
gchar *dialogs_show_input_goto_line(const gchar *title, GtkWindow *parent, const gchar *label_text,
	const gchar *default_text)
{
	dialog_input = NULL;
	dialogs_show_input_full(
		title, parent, label_text, default_text, FALSE, on_dialog_input,
		G_CALLBACK(ui_editable_insert_text_callback));
	return dialog_input;
}


/**
 *  Shows an input box to enter a numerical value using a GtkSpinButton.
 *  If the dialog is aborted, @a value remains untouched.
 *
 *  @param title The dialog title.
 *  @param label_text The shown dialog label.
 *  @param value The default value for the spin button and the return location of the entered value.
 * 				 Must be non-NULL.
 *  @param min Minimum allowable value (see documentation for @c gtk_spin_button_new_with_range()).
 *  @param max Maximum allowable value (see documentation for @c gtk_spin_button_new_with_range()).
 *  @param step Increment added or subtracted by spinning the widget
 * 				(see documentation for @c gtk_spin_button_new_with_range()).
 *
 *  @return @c TRUE if a value was entered and the dialog closed with 'OK'. @c FALSE otherwise.
 *
 *  @since 0.16
 **/
gboolean dialogs_show_input_numeric(const gchar *title, const gchar *label_text,
									gdouble *value, gdouble min, gdouble max, gdouble step)
{
	GtkWidget *dialog, *label, *spin, *vbox;
	gboolean res = FALSE;

	g_return_val_if_fail(title != NULL, FALSE);
	g_return_val_if_fail(label_text != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(main_widgets.window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");

	label = gtk_label_new(label_text);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	spin = gtk_spin_button_new_with_range(min, max, step);
	ui_entry_add_clear_icon(GTK_ENTRY(spin));
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), *value);
	g_signal_connect(spin, "activate", G_CALLBACK(on_input_numeric_activate), dialog);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), spin);
	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		*value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
		res = TRUE;
	}
	gtk_widget_destroy(dialog);

	return res;
}


void dialogs_show_file_properties(GeanyDocument *doc)
{
	GtkWidget *dialog, *label, *table, *hbox, *image, *perm_table, *check, *vbox;
	gchar *file_size, *title, *base_name, *time_changed, *time_modified, *time_accessed, *enctext;
	gchar *short_name;
	GdkPixbuf *pixbuf;
#ifdef HAVE_SYS_TYPES_H
	struct stat st;
	off_t filesize;
	mode_t mode;
	gchar *locale_filename;
#else
	gint filesize = 0;
	gint mode = 0;
#endif

/* define this ones, to avoid later trouble */
#ifndef S_IRUSR
# define S_IRUSR 0
# define S_IWUSR 0
# define S_IXUSR 0
#endif
#ifndef S_IRGRP
# define S_IRGRP 0
# define S_IWGRP 0
# define S_IXGRP 0
# define S_IROTH 0
# define S_IWOTH 0
# define S_IXOTH 0
#endif

	if (doc == NULL || doc->file_name == NULL)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
		_("An error occurred or file information could not be retrieved (e.g. from a new file)."));
		return;
	}


#ifdef HAVE_SYS_TYPES_H
	locale_filename = utils_get_locale_from_utf8(doc->file_name);
	if (g_stat(locale_filename, &st) == 0)
	{
		/* first copy the returned string and the trim it, to not modify the static glibc string
		 * g_strchomp() is used to remove trailing EOL chars, which are there for whatever reason */
		time_changed  = g_strchomp(g_strdup(ctime(&st.st_ctime)));
		time_modified = g_strchomp(g_strdup(ctime(&st.st_mtime)));
		time_accessed = g_strchomp(g_strdup(ctime(&st.st_atime)));
		filesize = st.st_size;
		mode = st.st_mode;
	}
	else
	{
		time_changed  = g_strdup(_("unknown"));
		time_modified = g_strdup(_("unknown"));
		time_accessed = g_strdup(_("unknown"));
		filesize = (off_t) 0;
		mode = (mode_t) 0;
	}
	g_free(locale_filename);
#else
	time_changed  = g_strdup(_("unknown"));
	time_modified = g_strdup(_("unknown"));
	time_accessed = g_strdup(_("unknown"));
#endif

	base_name = g_path_get_basename(doc->file_name);
	short_name = utils_str_middle_truncate(base_name, 30);
	title = g_strconcat(short_name, " ", _("Properties"), NULL);
	dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(main_widgets.window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	g_free(short_name);
	g_free(title);
	gtk_widget_set_name(dialog, "GeanyDialog");
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
	g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);

	label = ui_label_new_bold(base_name);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	pixbuf = ui_get_mime_icon(doc->file_type->mime_type, GTK_ICON_SIZE_BUTTON);
	image = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	gtk_misc_set_alignment(GTK_MISC(image), 1.0, 0.5);
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_container_add(GTK_CONTAINER(hbox), image);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	table = gtk_table_new(8, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("<b>Type:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(doc->file_type->title);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	label = gtk_label_new(_("<b>Size:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	file_size = utils_make_human_readable_str(filesize, 1, 0);
	label = gtk_label_new(file_size);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	g_free(file_size);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	label = gtk_label_new(_("<b>Location:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(doc->file_name);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	label = gtk_label_new(_("<b>Read-only:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	check = gtk_check_button_new_with_label(_("(only inside Geany)"));
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), doc->readonly);
	gtk_table_attach(GTK_TABLE(table), check, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.0, 0);

	label = gtk_label_new(_("<b>Encoding:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	enctext = g_strdup_printf("%s %s",
		doc->encoding,
		(encodings_is_unicode_charset(doc->encoding)) ?
			((doc->has_bom) ? _("(with BOM)") : _("(without BOM)")) : "");

	label = gtk_label_new(enctext);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	g_free(enctext);

	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	label = gtk_label_new(_("<b>Modified:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(time_modified);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	label = gtk_label_new(_("<b>Changed:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 6, 7,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(time_changed);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 6, 7,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	label = gtk_label_new(_("<b>Accessed:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 7, 8,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(time_accessed);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 7, 8,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	/* add table */
	gtk_container_add(GTK_CONTAINER(vbox), table);

	/* create table with the permissions */
	perm_table = gtk_table_new(5, 4, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(perm_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(perm_table), 5);

	label = gtk_label_new(_("<b>Permissions:</b>"));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 4, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	/* Header */
	label = gtk_label_new(_("Read:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	label = gtk_label_new(_("Write:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 2, 3, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	label = gtk_label_new(_("Execute:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 3, 4, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	/* Owner */
	label = gtk_label_new(_("Owner:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);


	/* Group */
	label = gtk_label_new(_("Group:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);


	/* Other */
	label = gtk_label_new(_("Other:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 4, 5,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IROTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 4, 5,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWOTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 4, 5,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXOTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 4, 5,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	gtk_container_add(GTK_CONTAINER(vbox), perm_table);

	g_free(base_name);
	g_free(time_changed);
	g_free(time_modified);
	g_free(time_accessed);

	gtk_widget_show_all(dialog);
}


/* extra_text can be NULL; otherwise it is displayed below main_text.
 * if parent is NULL, main_widgets.window will be used
 * btn_1, btn_2, btn_3 can be NULL.
 * returns response_1, response_2 or response_3 */
static gint show_prompt(GtkWidget *parent,
		const gchar *btn_1, GtkResponseType response_1,
		const gchar *btn_2, GtkResponseType response_2,
		const gchar *btn_3, GtkResponseType response_3,
		const gchar *question_text, const gchar *extra_text)
{
	gboolean ret = FALSE;
	GtkWidget *dialog;
	GtkWidget *btn;

	if (btn_2 == NULL)
	{
		btn_2 = GTK_STOCK_NO;
		response_2 = GTK_RESPONSE_NO;
	}
	if (btn_3 == NULL)
	{
		btn_3 = GTK_STOCK_YES;
		response_3 = GTK_RESPONSE_YES;
	}

#ifdef G_OS_WIN32
	/* our native dialog code doesn't support custom buttons */
	if (btn_3 == (gchar*)GTK_STOCK_YES && btn_2 == (gchar*)GTK_STOCK_NO && btn_1 == NULL)
	{
		gchar *string = (extra_text == NULL) ? g_strdup(question_text) :
			g_strconcat(question_text, "\n\n", extra_text, NULL);

		ret = win32_message_dialog(parent, GTK_MESSAGE_QUESTION, string);
		ret = ret ? response_3 : response_2;
		g_free(string);
		return ret;
	}
#endif
	if (parent == NULL && main_status.main_window_realized)
		parent = main_widgets.window;

	dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
		GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE, "%s", question_text);
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
	if (parent == NULL || GTK_IS_DIALOG(parent))
	{
		GdkPixbuf *pb = ui_new_pixbuf_from_inline(GEANY_IMAGE_LOGO);
		gtk_window_set_icon(GTK_WINDOW(dialog), pb);
		g_object_unref(pb);
	}

	/* question_text will be in bold if optional extra_text used */
	if (extra_text != NULL)
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s", extra_text);

	if (btn_1 != NULL)
		gtk_dialog_add_button(GTK_DIALOG(dialog), btn_1, response_1);

	/* For a cancel button, use cancel response so user can press escape to cancel */
	btn = gtk_dialog_add_button(GTK_DIALOG(dialog), btn_2,
		utils_str_equal(btn_2, GTK_STOCK_CANCEL) ? GTK_RESPONSE_CANCEL : response_2);
	/* we don't want a default, but we need to override the apply button as default */
	gtk_widget_grab_default(btn);
	gtk_dialog_add_button(GTK_DIALOG(dialog), btn_3, response_3);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	if (ret == GTK_RESPONSE_CANCEL)
		ret = response_2;
	return ret;
}


/**
 *  Shows a question message box with @a text and Yes/No buttons.
 *  On Unix-like systems a GTK message dialog box is shown, on Win32 systems a native Windows
 *  message dialog box is shown.
 *
 *  @param text Printf()-style format string.
 *  @param ... Arguments for the @a text format string.
 *
 *  @return @c TRUE if the user answered with Yes, otherwise @c FALSE.
 **/
gboolean dialogs_show_question(const gchar *text, ...)
{
	gchar *string;
	va_list args;
	GtkWidget *parent = (main_status.main_window_realized) ? main_widgets.window : NULL;
	gint result;

	va_start(args, text);
	string = g_strdup_vprintf(text, args);
	va_end(args);
	result = show_prompt(parent,
		NULL, GTK_RESPONSE_NONE,
		GTK_STOCK_NO, GTK_RESPONSE_NO,
		GTK_STOCK_YES, GTK_RESPONSE_YES,
		string, NULL);
	g_free(string);
	return (result == GTK_RESPONSE_YES);
}


/* extra_text can be NULL; otherwise it is displayed below main_text.
 * if parent is NULL, main_widgets.window will be used
 * yes_btn, no_btn can be NULL. */
gboolean dialogs_show_question_full(GtkWidget *parent, const gchar *yes_btn, const gchar *no_btn,
	const gchar *extra_text, const gchar *main_text, ...)
{
	gint result;
	gchar *string;
	va_list args;

	va_start(args, main_text);
	string = g_strdup_vprintf(main_text, args);
	va_end(args);
	result = show_prompt(parent,
		NULL, GTK_RESPONSE_NONE,
		no_btn, GTK_RESPONSE_NO,
		yes_btn, GTK_RESPONSE_YES,
		string, extra_text);
	g_free(string);
	return (result == GTK_RESPONSE_YES);
}


/* extra_text can be NULL; otherwise it is displayed below main_text.
 * if parent is NULL, main_widgets.window will be used
 * btn_1, btn_2, btn_3 can be NULL.
 * returns response_1, response_2 or response_3 */
gint dialogs_show_prompt(GtkWidget *parent,
		const gchar *btn_1, GtkResponseType response_1,
		const gchar *btn_2, GtkResponseType response_2,
		const gchar *btn_3, GtkResponseType response_3,
		const gchar *extra_text, const gchar *main_text, ...)
{
	gchar *string;
	va_list args;
	gint result;

	va_start(args, main_text);
	string = g_strdup_vprintf(main_text, args);
	va_end(args);
	result = show_prompt(parent, btn_1, response_1, btn_2, response_2, btn_3, response_3,
				string, extra_text);
	g_free(string);
	return result;
}
