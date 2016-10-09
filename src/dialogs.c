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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * File related dialogs, miscellaneous dialogs, font dialog.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs.h"

#include "app.h"
#include "build.h"
#include "document.h"
#include "encodings.h"
#include "encodingsprivate.h"
#include "filetypes.h"
#include "main.h"
#include "settings.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "win32.h"

#include "gtkcompat.h"

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
}
filesel_state = {
	{
		0,
		GEANY_ENCODINGS_MAX, /* default encoding is detect from file */
		-1, /* default filetype is detect from extension */
		FALSE,
		FALSE
	}
};


static gint filetype_combo_box_get_active_filetype(GtkComboBox *combo);


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


static gboolean open_file_dialog_handle_response(GtkWidget *dialog, gint response)
{
	gboolean ret = TRUE;

	if (response == GTK_RESPONSE_ACCEPT || response == GEANY_RESPONSE_VIEW)
	{
		GSList *filelist;
		GeanyFiletype *ft = NULL;
		const gchar *charset = NULL;
		GtkWidget *expander = ui_lookup_widget(dialog, "more_options_expander");
		GtkWidget *filetype_combo = ui_lookup_widget(dialog, "filetype_combo");
		GtkWidget *encoding_combo = ui_lookup_widget(dialog, "encoding_combo");
		gboolean ro = (response == GEANY_RESPONSE_VIEW);	/* View clicked */

		filesel_state.open.more_options_visible = gtk_expander_get_expanded(GTK_EXPANDER(expander));
		filesel_state.open.filter_idx = file_chooser_get_filter_idx(GTK_FILE_CHOOSER(dialog));
		filesel_state.open.filetype_idx = filetype_combo_box_get_active_filetype(GTK_COMBO_BOX(filetype_combo));

		/* ignore detect from file item */
		if (filesel_state.open.filetype_idx >= 0)
			ft = filetypes_index(filesel_state.open.filetype_idx);

		filesel_state.open.encoding_idx = ui_encodings_combo_box_get_active_encoding(GTK_COMBO_BOX(encoding_combo));
		if (filesel_state.open.encoding_idx >= 0 && filesel_state.open.encoding_idx < GEANY_ENCODINGS_MAX)
			charset = encodings[filesel_state.open.encoding_idx].charset;

		filelist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
		if (filelist != NULL)
		{
			const gchar *first = filelist->data;

			// When there's only one filename it may have been typed manually
			if (!filelist->next && !g_file_test(first, G_FILE_TEST_EXISTS))
			{
				dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("\"%s\" was not found."), first);
				ret = FALSE;
			}
			else
			{
				document_open_files(filelist, ro, ft, charset);
			}
			g_slist_foreach(filelist, (GFunc) g_free, NULL);	/* free filenames */
		}
		g_slist_free(filelist);
	}
	if (app->project && !EMPTY(app->project->base_path))
		gtk_file_chooser_remove_shortcut_folder(GTK_FILE_CHOOSER(dialog),
			app->project->base_path, NULL);
	return ret;
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


static void filetype_combo_cell_data_func(GtkCellLayout *cell_layout, GtkCellRenderer *cell,
		GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	gboolean sensitive = !gtk_tree_model_iter_has_child(tree_model, iter);
	gchar *text;

	gtk_tree_model_get(tree_model, iter, 1, &text, -1);
	g_object_set(cell, "sensitive", sensitive, "text", text, NULL);
	g_free(text);
}


static GtkWidget *create_filetype_combo_box(void)
{
	GtkTreeStore *store;
	GtkTreeIter iter_compiled, iter_script, iter_markup, iter_misc, iter_detect;
	GtkTreeIter *iter_parent;
	GtkWidget *combo;
	GtkCellRenderer *renderer;
	GSList *node;

	store = gtk_tree_store_new(2, G_TYPE_INT, G_TYPE_STRING);

	gtk_tree_store_insert_with_values(store, &iter_detect, NULL, -1,
			0, -1 /* auto-detect */, 1, _("Detect from file"), -1);

	gtk_tree_store_insert_with_values(store, &iter_compiled, NULL, -1,
			0, -1, 1, _("Programming Languages"), -1);
	gtk_tree_store_insert_with_values(store, &iter_script, NULL, -1,
			0, -1, 1, _("Scripting Languages"), -1);
	gtk_tree_store_insert_with_values(store, &iter_markup, NULL, -1,
			0, -1, 1, _("Markup Languages"), -1);
	gtk_tree_store_insert_with_values(store, &iter_misc, NULL, -1,
			0, -1, 1, _("Miscellaneous"), -1);

	foreach_slist (node, filetypes_by_title)
	{
		GeanyFiletype *ft = node->data;

		switch (ft->group)
		{
			case GEANY_FILETYPE_GROUP_COMPILED: iter_parent = &iter_compiled; break;
			case GEANY_FILETYPE_GROUP_SCRIPT: iter_parent = &iter_script; break;
			case GEANY_FILETYPE_GROUP_MARKUP: iter_parent = &iter_markup; break;
			case GEANY_FILETYPE_GROUP_MISC: iter_parent = &iter_misc; break;
			case GEANY_FILETYPE_GROUP_NONE:
			default: iter_parent = NULL;
		}
		gtk_tree_store_insert_with_values(store, NULL, iter_parent, -1,
				0, (gint) ft->id, 1, ft->title, -1);
	}

	combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo), &iter_detect);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(combo), renderer,
			filetype_combo_cell_data_func, NULL, NULL);

	g_object_unref(store);

	return combo;
}


/* the filetype, or -1 for auto-detect */
static gint filetype_combo_box_get_active_filetype(GtkComboBox *combo)
{
	gint id = -1;
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combo, &iter))
	{
		GtkTreeModel *model = gtk_combo_box_get_model(combo);
		gtk_tree_model_get(model, &iter, 0, &id, -1);
	}
	return id;
}


static gboolean filetype_combo_box_set_active_filetype(GtkComboBox *combo, const gint id)
{
	GtkTreeModel *model = gtk_combo_box_get_model(combo);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter_first(model, &iter))
	{
		do
		{
			gint row_id;
			gtk_tree_model_get(model, &iter, 0, &row_id, -1);
			if (id == row_id)
			{
				gtk_combo_box_set_active_iter(combo, &iter);
				return TRUE;
			}
		}
		while (ui_tree_model_iter_any_next(model, &iter, TRUE));
	}
	return FALSE;
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
	encoding_combo = ui_create_encodings_combo_box(TRUE, GEANY_ENCODINGS_MAX);
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
	filetype_combo = create_filetype_combo_box();
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
	GtkWidget *viewbtn;
	GSList *node;

	dialog = gtk_file_chooser_dialog_new(_("Open File"), GTK_WINDOW(main_widgets.window),
			GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);
	gtk_widget_set_name(dialog, "GeanyDialog");

	viewbtn = gtk_dialog_add_button(GTK_DIALOG(dialog), C_("Open dialog action", "_View"), GEANY_RESPONSE_VIEW);
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
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filetypes_create_file_filter(ft));
	}

	g_signal_connect(dialog, "notify::show-hidden",
		G_CALLBACK(on_file_open_show_hidden_notify), NULL);

	return dialog;
}


static void open_file_dialog_apply_settings(GtkWidget *dialog)
{
	static gboolean initialized = FALSE;
	GtkWidget *check_hidden = ui_lookup_widget(dialog, "check_hidden");
	GtkWidget *filetype_combo = ui_lookup_widget(dialog, "filetype_combo");
	GtkWidget *encoding_combo = ui_lookup_widget(dialog, "encoding_combo");
	GtkWidget *expander = ui_lookup_widget(dialog, "more_options_expander");

	/* we can't know the initial position of combo boxes, so retrieve it the first time */
	if (! initialized)
	{
		filesel_state.open.filter_idx = file_chooser_get_filter_idx(GTK_FILE_CHOOSER(dialog));

		initialized = TRUE;
	}
	else
	{
		file_chooser_set_filter_idx(GTK_FILE_CHOOSER(dialog), filesel_state.open.filter_idx);
	}
	gtk_expander_set_expanded(GTK_EXPANDER(expander), filesel_state.open.more_options_visible);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_hidden), filesel_state.open.show_hidden);
	ui_encodings_combo_box_set_active_encoding(GTK_COMBO_BOX(encoding_combo), filesel_state.open.encoding_idx);
	filetype_combo_box_set_active_filetype(GTK_COMBO_BOX(filetype_combo), filesel_state.open.filetype_idx);
}


/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file(void)
{
	gchar *initdir;

	/* set dialog directory to the current file's directory, if present */
	initdir = utils_get_current_file_dir_utf8();

	/* use project or default startup directory (if set) if no files are open */
	/** TODO should it only be used when initially open the dialog and not on every show? */
	if (! initdir)
		initdir = g_strdup(utils_get_default_dir_utf8());

	SETPTR(initdir, utils_get_locale_from_utf8(initdir));

#ifdef G_OS_WIN32
	if (settings_get_bool("use-native-windows-dialogs"))
		win32_show_document_open_dialog(GTK_WINDOW(main_widgets.window), _("Open File"), initdir);
	else
#endif
	{
		GtkWidget *dialog = create_open_file_dialog();

		open_file_dialog_apply_settings(dialog);

		if (initdir != NULL && g_path_is_absolute(initdir))
				gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), initdir);

		if (app->project && !EMPTY(app->project->base_path))
			gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(dialog),
					app->project->base_path, NULL);

		while (!open_file_dialog_handle_response(dialog,
			gtk_dialog_run(GTK_DIALOG(dialog))));
		gtk_widget_destroy(dialog);
	}
	g_free(initdir);
}


static gboolean handle_save_as(const gchar *utf8_filename, gboolean rename_file)
{
	GeanyDocument *doc = document_get_current();
	gboolean success = FALSE;

	g_return_val_if_fail(!EMPTY(utf8_filename), FALSE);

	if (doc->file_name != NULL)
	{
		if (rename_file)
		{
			document_rename_file(doc, utf8_filename);
		}
		if (doc->tm_file)
		{
			/* create a new tm_source_file object otherwise tagmanager won't work correctly */
			tm_workspace_remove_source_file(doc->tm_file);
			tm_source_file_free(doc->tm_file);
			doc->tm_file = NULL;
		}
	}
	success = document_save_file_as(doc, utf8_filename);

	build_menu_update(doc);
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
			if (G_UNLIKELY(EMPTY(new_filename)))
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
			gchar *utf8_filename;

			utf8_filename = utils_get_utf8_from_locale(new_filename);
			success = handle_save_as(utf8_filename, rename_file);
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


static GtkWidget *create_save_file_dialog(GeanyDocument *doc)
{
	GtkWidget *dialog, *rename_btn;
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
	/* disable rename unless file exists on disk */
	gtk_widget_set_sensitive(rename_btn, doc->real_path != NULL);

	gtk_dialog_add_buttons(GTK_DIALOG(dialog),
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

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
	return dialog;
}


static gboolean show_save_as_gtk(GeanyDocument *doc)
{
	GtkWidget *dialog;
	gint resp;

	g_return_val_if_fail(DOC_VALID(doc), FALSE);

	dialog = create_save_file_dialog(doc);

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

	if (app->project && !EMPTY(app->project->base_path))
		gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(dialog),
			app->project->base_path, NULL);

	/* Run the dialog synchronously, pausing this function call */
	do
	{
		resp = gtk_dialog_run(GTK_DIALOG(dialog));
	}
	while (! save_as_dialog_handle_response(dialog, resp));

	if (app->project && !EMPTY(app->project->base_path))
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
GEANY_API_SYMBOL
gboolean dialogs_show_save_as(void)
{
	GeanyDocument *doc = document_get_current();
	gboolean result = FALSE;

	g_return_val_if_fail(doc, FALSE);

#ifdef G_OS_WIN32
	if (settings_get_bool("use-native-windows-dialogs"))
	{
		gchar *utf8_name = win32_show_document_save_as_dialog(GTK_WINDOW(main_widgets.window),
						_("Save File"), doc);
		if (utf8_name != NULL)
			result = handle_save_as(utf8_name, FALSE);
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
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "geany");
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
GEANY_API_SYMBOL
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


/* Use GtkFontChooserDialog on GTK3.2 for consistency, and because
 * GtkFontSelectionDialog is somewhat broken on 3.4 */
#if GTK_CHECK_VERSION(3, 2, 0)
#	undef GTK_FONT_SELECTION_DIALOG
#	define GTK_FONT_SELECTION_DIALOG				GTK_FONT_CHOOSER_DIALOG

#	define gtk_font_selection_dialog_new(title) \
		gtk_font_chooser_dialog_new((title), NULL)
#	define gtk_font_selection_dialog_get_font_name(dlg) \
		gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dlg))
#	define gtk_font_selection_dialog_set_font_name(dlg, font) \
		gtk_font_chooser_set_font(GTK_FONT_CHOOSER(dlg), (font))
#endif

static void
on_font_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	gboolean close = TRUE;

	switch (response)
	{
		case GTK_RESPONSE_APPLY:
		case GTK_RESPONSE_OK:
		{
			gchar *fontname;

			fontname = gtk_font_selection_dialog_get_font_name(
				GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel));
			settings_set_string("editor-font", fontname);
			g_free(fontname);

			close = (response == GTK_RESPONSE_OK);
			break;
		}
	}

	if (close)
		gtk_widget_hide(ui_widgets.open_fontsel);
}


/* This shows the font selection dialog to choose a font. */
void dialogs_show_open_font(void)
{
	gchar *font_name;

#ifdef G_OS_WIN32
	if (settings_get_bool("use-native-windows-dialogs"))
	{
		win32_show_font_dialog();
		return;
	}
#endif

	if (ui_widgets.open_fontsel == NULL)
	{
		GtkWidget *apply_button;

		ui_widgets.open_fontsel = gtk_font_selection_dialog_new(_("Choose font"));;
		gtk_container_set_border_width(GTK_CONTAINER(ui_widgets.open_fontsel), 4);
		gtk_window_set_modal(GTK_WINDOW(ui_widgets.open_fontsel), TRUE);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(ui_widgets.open_fontsel), TRUE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(ui_widgets.open_fontsel), TRUE);
		gtk_window_set_type_hint(GTK_WINDOW(ui_widgets.open_fontsel), GDK_WINDOW_TYPE_HINT_DIALOG);
		gtk_widget_set_name(ui_widgets.open_fontsel, "GeanyDialog");

		apply_button = gtk_dialog_get_widget_for_response(GTK_DIALOG(ui_widgets.open_fontsel), GTK_RESPONSE_APPLY);

		if (apply_button)
			gtk_widget_show(apply_button);

		g_signal_connect(ui_widgets.open_fontsel,
					"delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
		g_signal_connect(ui_widgets.open_fontsel,
					"response", G_CALLBACK(on_font_dialog_response), NULL);

		gtk_window_set_transient_for(GTK_WINDOW(ui_widgets.open_fontsel), GTK_WINDOW(main_widgets.window));
	}
	font_name = settings_get_string("editor-font");
	gtk_font_selection_dialog_set_font_name(
		GTK_FONT_SELECTION_DIALOG(ui_widgets.open_fontsel), font_name);
	g_free(font_name);
	/* We make sure the dialog is visible. */
	gtk_window_present(GTK_WINDOW(ui_widgets.open_fontsel));
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


typedef struct
{
	GtkWidget *entry;
	GtkWidget *combo;

	GeanyInputCallback callback;
	gpointer data;
}
InputDialogData;


static void
on_input_dialog_response(GtkDialog *dialog, gint response, InputDialogData *data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		const gchar *str = gtk_entry_get_text(GTK_ENTRY(data->entry));

		if (data->combo != NULL)
			ui_combo_box_add_to_history(GTK_COMBO_BOX_TEXT(data->combo), str, 0);

		data->callback(str, data->data);
	}
	gtk_widget_hide(GTK_WIDGET(dialog));
}


/* Create and display an input dialog.
 * persistent: whether to remember previous entry text in a combo box;
 * 	in this case the dialog returned is not destroyed on a response,
 * 	and can be reshown.
 * Returns: the dialog widget. */
static GtkWidget *
dialogs_show_input_full(const gchar *title, GtkWindow *parent,
						const gchar *label_text, const gchar *default_text,
						gboolean persistent, GeanyInputCallback input_cb, gpointer input_cb_data,
						GCallback insert_text_cb, gpointer insert_text_cb_data)
{
	GtkWidget *dialog, *vbox;
	InputDialogData *data = g_malloc(sizeof *data);

	dialog = gtk_dialog_new_with_buttons(title, parent,
		GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	data->combo = NULL;
	data->entry = NULL;
	data->callback = input_cb;
	data->data = input_cb_data;

	if (label_text)
	{
		GtkWidget *label = gtk_label_new(label_text);
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_container_add(GTK_CONTAINER(vbox), label);
	}

	if (persistent)	/* remember previous entry text in a combo box */
	{
		data->combo = gtk_combo_box_text_new_with_entry();
		data->entry = gtk_bin_get_child(GTK_BIN(data->combo));
		ui_entry_add_clear_icon(GTK_ENTRY(data->entry));
		gtk_container_add(GTK_CONTAINER(vbox), data->combo);
	}
	else
	{
		data->entry = gtk_entry_new();
		ui_entry_add_clear_icon(GTK_ENTRY(data->entry));
		gtk_container_add(GTK_CONTAINER(vbox), data->entry);
	}

	if (default_text != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(data->entry), default_text);
	}
	gtk_entry_set_max_length(GTK_ENTRY(data->entry), 255);
	gtk_entry_set_width_chars(GTK_ENTRY(data->entry), 30);

	if (insert_text_cb != NULL)
		g_signal_connect(data->entry, "insert-text", insert_text_cb, insert_text_cb_data);
	g_signal_connect(data->entry, "activate", G_CALLBACK(on_input_entry_activate), dialog);
	g_signal_connect(dialog, "show", G_CALLBACK(on_input_dialog_show), data->entry);
	g_signal_connect_data(dialog, "response", G_CALLBACK(on_input_dialog_response), data, (GClosureNotify)g_free, 0);

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
		GeanyInputCallback input_cb, gpointer input_cb_data)
{
	return dialogs_show_input_full(title, parent, label_text, default_text, TRUE, input_cb, input_cb_data, NULL, NULL);
}


static void on_dialog_input(const gchar *str, gpointer data)
{
	gchar **dialog_input = data;
	*dialog_input = g_strdup(str);
}


/** Asks the user for text input.
 * @param title Dialog title.
 * @param parent @nullable The currently focused window, usually @c geany->main_widgets->window.
 * 	@c NULL can be used but is discouraged due to window manager effects.
 * @param label_text @nullable Label text, or @c NULL.
 * @param default_text @nullable Text to display in the input field, or @c NULL.
 * @return @nullable New copy of user input or @c NULL if cancelled.
 * @since 0.20. */
GEANY_API_SYMBOL
gchar *dialogs_show_input(const gchar *title, GtkWindow *parent, const gchar *label_text,
	const gchar *default_text)
{
	gchar *dialog_input = NULL;
	dialogs_show_input_full(title, parent, label_text, default_text, FALSE, on_dialog_input, &dialog_input, NULL, NULL);
	return dialog_input;
}


/* Note: could be changed to dialogs_show_validated_input with argument for callback. */
/* Returns: newly allocated copy of the entry text or NULL on cancel.
 * Specialised variant for Goto Line dialog. */
gchar *dialogs_show_input_goto_line(const gchar *title, GtkWindow *parent, const gchar *label_text,
	const gchar *default_text)
{
	gchar *dialog_input = NULL;
	dialogs_show_input_full(
		title, parent, label_text, default_text, FALSE, on_dialog_input, &dialog_input,
		G_CALLBACK(ui_editable_insert_text_callback), NULL);
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
GEANY_API_SYMBOL
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
	GtkWidget *dialog, *label, *image, *check;
	gchar *file_size, *title, *base_name, *time_changed, *time_modified, *time_accessed, *enctext;
	gchar *short_name;
#ifdef HAVE_SYS_TYPES_H
	GStatBuf st;
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

	g_return_if_fail(doc == NULL || doc->is_valid);

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
	title = g_strdup_printf(_("%s Properties"), short_name);
	dialog = ui_builder_get_object("properties_dialog");
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	g_free(short_name);
	g_free(title);
	gtk_widget_set_name(dialog, "GeanyDialog");

	label = ui_lookup_widget(dialog, "file_name_label");
	gtk_label_set_text(GTK_LABEL(label), base_name);

	image = ui_lookup_widget(dialog, "file_type_image");
	gtk_image_set_from_gicon(GTK_IMAGE(image), doc->file_type->icon,
			GTK_ICON_SIZE_BUTTON);

	label = ui_lookup_widget(dialog, "file_type_label");
	gtk_label_set_text(GTK_LABEL(label), doc->file_type->title);

	label = ui_lookup_widget(dialog, "file_size_label");
	file_size = utils_make_human_readable_str(filesize, 1, 0);
	gtk_label_set_text(GTK_LABEL(label), file_size);
	g_free(file_size);

	label = ui_lookup_widget(dialog, "file_location_label");
	gtk_label_set_text(GTK_LABEL(label), doc->file_name);

	check = ui_lookup_widget(dialog, "file_read_only_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), doc->readonly);

	label = ui_lookup_widget(dialog, "file_encoding_label");
	enctext = g_strdup_printf("%s %s",
		doc->encoding,
		(encodings_is_unicode_charset(doc->encoding)) ?
			((doc->has_bom) ? _("(with BOM)") : _("(without BOM)")) : "");
	gtk_label_set_text(GTK_LABEL(label), enctext);
	g_free(enctext);

	label = ui_lookup_widget(dialog, "file_modified_label");
	gtk_label_set_text(GTK_LABEL(label), time_modified);
	label = ui_lookup_widget(dialog, "file_changed_label");
	gtk_label_set_text(GTK_LABEL(label), time_changed);
	label = ui_lookup_widget(dialog, "file_accessed_label");
	gtk_label_set_text(GTK_LABEL(label), time_accessed);

	/* permissions */
	check = ui_lookup_widget(dialog, "file_perm_owner_r_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRUSR);
	check = ui_lookup_widget(dialog, "file_perm_owner_w_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWUSR);
	check = ui_lookup_widget(dialog, "file_perm_owner_x_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXUSR);
	check = ui_lookup_widget(dialog, "file_perm_group_r_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRGRP);
	check = ui_lookup_widget(dialog, "file_perm_group_w_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWGRP);
	check = ui_lookup_widget(dialog, "file_perm_group_x_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXGRP);
	check = ui_lookup_widget(dialog, "file_perm_other_r_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IROTH);
	check = ui_lookup_widget(dialog, "file_perm_other_w_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWOTH);
	check = ui_lookup_widget(dialog, "file_perm_other_x_check");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXOTH);

	g_free(base_name);
	g_free(time_changed);
	g_free(time_modified);
	g_free(time_accessed);

	gtk_widget_show(dialog);
}


/* extra_text can be NULL; otherwise it is displayed below main_text.
 * if parent is NULL, main_widgets.window will be used
 * btn_1, btn_2, btn_3 can be NULL.
 * returns response_1, response_2, response_3, or GTK_RESPONSE_DELETE_EVENT if the dialog was discarded */
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
	if (utils_str_equal(btn_3, GTK_STOCK_YES) &&
		utils_str_equal(btn_2, GTK_STOCK_NO) && btn_1 == NULL)
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
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "geany");

	/* question_text will be in bold if optional extra_text used */
	if (extra_text != NULL)
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s", extra_text);

	if (btn_1 != NULL)
		gtk_dialog_add_button(GTK_DIALOG(dialog), btn_1, response_1);

	btn = gtk_dialog_add_button(GTK_DIALOG(dialog), btn_2, response_2);
	/* we don't want a default, but we need to override the apply button as default */
	gtk_widget_grab_default(btn);
	gtk_dialog_add_button(GTK_DIALOG(dialog), btn_3, response_3);

	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

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
GEANY_API_SYMBOL
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
