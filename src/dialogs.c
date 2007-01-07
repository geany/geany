/*
 *      dialogs.c - this file is part of Geany, a fast and lightweight IDE
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

#include "geany.h"

#include <gdk/gdkkeysyms.h>
#include <string.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
#include <time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "dialogs.h"

#include "callbacks.h"
#include "document.h"
#include "win32.h"
#include "sciwrappers.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "keybindings.h"
#include "encodings.h"


#if ! GEANY_USE_WIN32_DIALOG
static GtkWidget *add_file_open_extra_widget();
#endif


/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file ()
{
#if GEANY_USE_WIN32_DIALOG
	win32_show_file_dialog(TRUE);
#else /* X11, not win32: use GTK_FILE_CHOOSER */
	gchar *initdir;

	/* We use the same file selection widget each time, so first
		of all we create it if it hasn't already been created. */
	if (app->open_filesel == NULL)
	{
		GtkWidget *filetype_combo, *encoding_combo;
		GtkWidget *viewbtn;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));
		gint i;
		gchar *encoding_string;

		app->open_filesel = gtk_file_chooser_dialog_new(_("Open File"), GTK_WINDOW(app->window),
				GTK_FILE_CHOOSER_ACTION_OPEN, NULL, NULL);

		viewbtn = gtk_button_new_with_mnemonic(_("_View"));
		gtk_tooltips_set_tip(tooltips, viewbtn,
			_("Opens the file in read-only mode. If you choose more than one file to open, all files will be opened read-only."), NULL);
		gtk_widget_show(viewbtn);
		gtk_dialog_add_action_widget(GTK_DIALOG(app->open_filesel),
			viewbtn, GTK_RESPONSE_APPLY);
		gtk_dialog_add_buttons(GTK_DIALOG(app->open_filesel),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
		// set default Open, so pressing enter can open multiple files
		gtk_dialog_set_default_response(GTK_DIALOG(app->open_filesel),
			GTK_RESPONSE_ACCEPT);

		gtk_widget_set_size_request(app->open_filesel, -1, 460);
		gtk_window_set_modal(GTK_WINDOW(app->open_filesel), TRUE);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(app->open_filesel), TRUE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->open_filesel), TRUE);
		gtk_window_set_type_hint(GTK_WINDOW(app->open_filesel), GDK_WINDOW_TYPE_HINT_DIALOG);
		gtk_window_set_transient_for(GTK_WINDOW(app->open_filesel), GTK_WINDOW(app->window));
		gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(app->open_filesel), TRUE);

		// add checkboxes and filename entry
		gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(app->open_filesel),
			add_file_open_extra_widget());
		filetype_combo = lookup_widget(app->open_filesel, "filetype_combo");

		// add FileFilters(start with "All Files")
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->open_filesel),
					filetypes_create_file_filter(filetypes[GEANY_FILETYPES_ALL]));
		for (i = 0; i < GEANY_MAX_FILE_TYPES - 1; i++)
		{
			gtk_combo_box_append_text(GTK_COMBO_BOX(filetype_combo), filetypes[i]->title);
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->open_filesel),
					filetypes_create_file_filter(filetypes[i]));
		}
		gtk_combo_box_append_text(GTK_COMBO_BOX(filetype_combo), _("Detect by file extension"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(filetype_combo), GEANY_MAX_FILE_TYPES - 1);

		// fill encoding combo box
		encoding_combo = lookup_widget(app->open_filesel, "encoding_combo");
		for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
		{
			encoding_string = encodings_to_string(&encodings[i]);
			gtk_combo_box_append_text(GTK_COMBO_BOX(encoding_combo), encoding_string);
			g_free(encoding_string);
		}
		gtk_combo_box_append_text(GTK_COMBO_BOX(encoding_combo), _("Detect from file"));
		gtk_combo_box_set_active(GTK_COMBO_BOX(encoding_combo), GEANY_ENCODINGS_MAX);

		g_signal_connect((gpointer) app->open_filesel, "selection-changed",
					G_CALLBACK(on_file_open_selection_changed), NULL);
		g_signal_connect ((gpointer) app->open_filesel, "delete_event",
					G_CALLBACK(gtk_widget_hide), NULL);
		g_signal_connect((gpointer) app->open_filesel, "response",
					G_CALLBACK(on_file_open_dialog_response), NULL);
	}

	// set dialog directory to the current file's directory, if present
	initdir = utils_get_current_file_dir();
	if (initdir != NULL)
	{
		gchar *locale_filename;

		locale_filename = utils_get_locale_from_utf8(initdir);

		if (g_path_is_absolute(locale_filename))
			gtk_file_chooser_set_current_folder(
				GTK_FILE_CHOOSER(app->open_filesel), locale_filename);

		g_free(initdir);
		g_free(locale_filename);
	}

	gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->open_filesel));
	gtk_widget_show(app->open_filesel);
#endif
}


#ifndef USE_WIN32_DIALOG
static GtkWidget *add_file_open_extra_widget()
{
	GtkWidget *vbox, *table, *file_entry, *check_hidden;
	GtkWidget *filetype_ebox, *filetype_label, *filetype_combo;
	GtkWidget *encoding_ebox, *encoding_label, *encoding_combo;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	vbox = gtk_vbox_new(FALSE, 6);

	table = gtk_table_new(2, 4, FALSE);

	// line 1 with checkbox and encoding combo
	check_hidden = gtk_check_button_new_with_mnemonic(_("Show _hidden files"));
	gtk_button_set_focus_on_click(GTK_BUTTON(check_hidden), FALSE);
	gtk_widget_show(check_hidden);
	gtk_table_attach(GTK_TABLE(table), check_hidden, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
					(GtkAttachOptions) (0), 0, 5);

	// spacing
	gtk_table_attach(GTK_TABLE(table), gtk_label_new(""), 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 5, 5);

	encoding_label = gtk_label_new(_("Set encoding:"));
	gtk_misc_set_alignment(GTK_MISC(encoding_label), 1, 0);
	gtk_table_attach(GTK_TABLE(table), encoding_label, 2, 3, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 4, 5);
	// the ebox is for the tooltip, because gtk_combo_box can't show tooltips
	encoding_ebox = gtk_event_box_new();
	encoding_combo = gtk_combo_box_new_text();
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(encoding_combo), 3);
	gtk_tooltips_set_tip(tooltips, encoding_ebox,
		_("Explicitly defines an encoding for the file, if it would not be detected. This is useful when you know that the encoding of a file cannot be detected correctly by Geany.\nNote if you choose multiple files, they will all be opened with the chosen encoding."), NULL);
	gtk_container_add(GTK_CONTAINER(encoding_ebox), encoding_combo);
	gtk_table_attach(GTK_TABLE(table), encoding_ebox, 3, 4, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 5);

	// line 2 with filename entry and filetype combo
	file_entry = gtk_entry_new();
	gtk_widget_show(file_entry);
	//gtk_editable_set_editable(GTK_EDITABLE(file_entry), FALSE);
	gtk_entry_set_activates_default(GTK_ENTRY(file_entry), TRUE);
	gtk_table_attach(GTK_TABLE(table), file_entry, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
					(GtkAttachOptions) (0), 0, 5);

	// spacing
	gtk_table_attach(GTK_TABLE(table), gtk_label_new(""), 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 5, 5);

	filetype_label = gtk_label_new(_("Set filetype:"));
	gtk_misc_set_alignment(GTK_MISC(filetype_label), 1, 0);
	gtk_table_attach(GTK_TABLE(table), filetype_label, 2, 3, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 4, 5);
	// the ebox is for the tooltip, because gtk_combo_box can't show tooltips
	filetype_ebox = gtk_event_box_new();
	filetype_combo = gtk_combo_box_new_text();
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(filetype_combo), 2);
	gtk_tooltips_set_tip(tooltips, filetype_ebox,
		_("Explicitly defines a filetype for the file, if it would not be detected by filename extension.\nNote if you choose multiple files, they will all be opened with the chosen filetype."), NULL);
	gtk_container_add(GTK_CONTAINER(filetype_ebox), filetype_combo);
	gtk_table_attach(GTK_TABLE(table), filetype_ebox, 3, 4, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 5);

	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);

	g_signal_connect((gpointer) file_entry, "activate",
				G_CALLBACK(on_file_open_entry_activate), NULL);
	g_signal_connect((gpointer) check_hidden, "toggled",
				G_CALLBACK(on_file_open_check_hidden_toggled), NULL);

	g_object_set_data_full(G_OBJECT(app->open_filesel), "file_entry",
				gtk_widget_ref(file_entry), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(app->open_filesel), "check_hidden",
				gtk_widget_ref(check_hidden), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(app->open_filesel), "filetype_combo",
				gtk_widget_ref(filetype_combo), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(app->open_filesel), "encoding_combo",
				gtk_widget_ref(encoding_combo), (GDestroyNotify)gtk_widget_unref);

	return vbox;
}
#endif


/* This shows the file selection dialog to save a file, returning TRUE if
 * the file was saved. */
gboolean dialogs_show_save_as()
{
#ifdef G_OS_WIN32
	return win32_show_file_dialog(FALSE);
#else
	gint idx = document_get_cur_idx(), resp;

	if (app->save_filesel == NULL)
	{
		app->save_filesel = gtk_file_chooser_dialog_new(_("Save File"), GTK_WINDOW(app->window),
					GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
		gtk_window_set_modal(GTK_WINDOW(app->save_filesel), TRUE);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(app->save_filesel), TRUE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->save_filesel), TRUE);
		gtk_window_set_type_hint(GTK_WINDOW(app->save_filesel), GDK_WINDOW_TYPE_HINT_DIALOG);
		gtk_dialog_set_default_response(GTK_DIALOG(app->save_filesel), GTK_RESPONSE_ACCEPT);

		g_signal_connect((gpointer) app->save_filesel, "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
		g_signal_connect((gpointer) app->save_filesel, "response", G_CALLBACK(on_file_save_dialog_response), NULL);

		gtk_window_set_transient_for(GTK_WINDOW(app->save_filesel), GTK_WINDOW(app->window));
	}

	// If the current document has a filename we use that as the default.
	if (doc_list[idx].file_name != NULL)
	{
		gchar *locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);

		if (g_path_is_absolute(locale_filename))
			gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(app->save_filesel), locale_filename);
		else
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(app->save_filesel), locale_filename);
		g_free(locale_filename);
	}
	else
	{
		gchar *fname = NULL;

		if (doc_list[idx].file_type != NULL && doc_list[idx].file_type->id != GEANY_FILETYPES_ALL &&
			doc_list[idx].file_type->extension != NULL)
			fname = g_strconcat(GEANY_STRING_UNTITLED, ".",
								doc_list[idx].file_type->extension, NULL);
		else
			fname = g_strdup(GEANY_STRING_UNTITLED);

		gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->save_filesel));
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(app->save_filesel), fname);
		g_free(fname);
	}

	// Run the dialog synchronously, pausing this function call
	resp = gtk_dialog_run(GTK_DIALOG(app->save_filesel));
	return (resp == GTK_RESPONSE_ACCEPT);
#endif
}


void dialogs_show_msgbox(gint type, const gchar *text, ...)
{
#ifndef G_OS_WIN32
	GtkWidget *dialog;
#endif
	gchar *string = g_malloc(512);
	va_list args;

	va_start(args, text);
	g_vsnprintf(string, 511, text, args);
	va_end(args);

#ifdef G_OS_WIN32
	win32_message_dialog(type, string);
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  type, GTK_BUTTONS_OK, "%s", string);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
#endif
	g_free(string);
}


gboolean dialogs_show_unsaved_file(gint idx)
{
#ifndef G_OS_WIN32
	GtkWidget *dialog, *button;
#endif
	gchar *msg, *short_fn = NULL;
	gint ret;

	// display the file tab to remind the user of the document
	gtk_notebook_set_current_page(GTK_NOTEBOOK(app->notebook),
		document_find_by_sci(doc_list[idx].sci));

	if (doc_list[idx].file_name != NULL)
	{
		short_fn = g_path_get_basename(doc_list[idx].file_name);
	}

	msg = g_strdup_printf(_("The file '%s' is not saved."),
		(short_fn != NULL) ? short_fn : GEANY_STRING_UNTITLED);
	g_free(short_fn);

#ifdef G_OS_WIN32
	ret = win32_message_dialog_unsaved(msg);
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", msg);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
		"%s", _("Do you want to save it before closing?"));
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = ui_button_new_with_image(GTK_STOCK_CLEAR, _("_Don't save"));
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), button, GTK_RESPONSE_NO);
	gtk_widget_show(button);

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_SAVE, GTK_RESPONSE_YES);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
	ret = gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
#endif
	g_free(msg);

	switch(ret)
	{
		case GTK_RESPONSE_YES:
		{
			if (doc_list[idx].file_name == NULL)
			{
				ret = dialogs_show_save_as();
			}
			else
				// document_save_file() returns the status if the file could be saved
				ret = document_save_file(idx, FALSE);
			break;
		}
		case GTK_RESPONSE_NO: ret = TRUE; break;
		case GTK_RESPONSE_CANCEL: /* fall through to default and leave the function */
		default: ret = FALSE; break;
	}

	return (gboolean) ret;
}


/* This shows the font selection dialog to choose a font. */
void dialogs_show_open_font()
{
#ifdef G_OS_WIN32
	win32_show_font_dialog();
#else

	if (app->open_fontsel == NULL)
	{
		app->open_fontsel = gtk_font_selection_dialog_new(_("Choose font"));;
		gtk_container_set_border_width(GTK_CONTAINER(app->open_fontsel), 4);
		gtk_window_set_modal(GTK_WINDOW(app->open_fontsel), TRUE);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(app->open_fontsel), TRUE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->open_fontsel), TRUE);
		gtk_window_set_type_hint(GTK_WINDOW(app->open_fontsel), GDK_WINDOW_TYPE_HINT_DIALOG);

		gtk_widget_show(GTK_FONT_SELECTION_DIALOG(app->open_fontsel)->apply_button);

		g_signal_connect((gpointer) app->open_fontsel,
					"delete_event", G_CALLBACK(gtk_widget_hide), NULL);
		g_signal_connect((gpointer) GTK_FONT_SELECTION_DIALOG(app->open_fontsel)->ok_button,
					"clicked", G_CALLBACK(on_font_ok_button_clicked), NULL);
		g_signal_connect((gpointer) GTK_FONT_SELECTION_DIALOG(app->open_fontsel)->cancel_button,
					"clicked", G_CALLBACK(on_font_cancel_button_clicked), NULL);
		g_signal_connect((gpointer) GTK_FONT_SELECTION_DIALOG(app->open_fontsel)->apply_button,
					"clicked", G_CALLBACK(on_font_apply_button_clicked), NULL);

		gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(app->open_fontsel), app->editor_font);
		gtk_window_set_transient_for(GTK_WINDOW(app->open_fontsel), GTK_WINDOW(app->window));
	}
	/* We make sure the dialog is visible. */
	gtk_window_present(GTK_WINDOW(app->open_fontsel));
#endif
}


void dialogs_show_word_count()
{
	GtkWidget *dialog, *label, *vbox, *table;
	gint idx;
	guint chars, lines, words;
	gchar *text, *range;

	idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;

	dialog = gtk_dialog_new_with_buttons(_("Word Count"), GTK_WINDOW(app->window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));

	if (sci_can_copy(doc_list[idx].sci))
	{
		text = g_malloc0(sci_get_selected_text_length(doc_list[idx].sci) + 1);
		sci_get_selected_text(doc_list[idx].sci, text);
		range = _("selection");
	}
	else
	{
		text = g_malloc(sci_get_length(doc_list[idx].sci) + 1);
		sci_get_text(doc_list[idx].sci, sci_get_length(doc_list[idx].sci) + 1 , text);
		range = _("whole document");
	}
	utils_wordcount(text, &chars, &lines, &words);
	g_free(text);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("Range:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(range);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 20, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	label = gtk_label_new(_("Lines:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	text = g_strdup_printf("%d", lines);
	label = gtk_label_new(text);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 20, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	g_free(text);

	label = gtk_label_new(_("Words:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	text = g_strdup_printf("%d", words);
	label = gtk_label_new(text);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 20, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	g_free(text);

	label = gtk_label_new(_("Characters:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	text = g_strdup_printf("%d", chars);
	label = gtk_label_new(text);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 20, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	g_free(text);

	gtk_container_add(GTK_CONTAINER(vbox), table);

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
	g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_widget_destroy), dialog);

	gtk_widget_show_all(dialog);
}


/* This shows the color selection dialog to choose a color. */
void dialogs_show_color(gchar *colour)
{
#ifdef G_OS_WIN32
	win32_show_color_dialog(colour);
#else

	if (app->open_colorsel == NULL)
	{
		app->open_colorsel = gtk_color_selection_dialog_new(_("Color Chooser"));
		gtk_window_set_transient_for(GTK_WINDOW(app->open_colorsel), GTK_WINDOW(app->window));

		g_signal_connect(GTK_COLOR_SELECTION_DIALOG(app->open_colorsel)->cancel_button, "clicked",
						G_CALLBACK(on_color_cancel_button_clicked), NULL);
		g_signal_connect(GTK_COLOR_SELECTION_DIALOG(app->open_colorsel)->ok_button, "clicked",
						G_CALLBACK(on_color_ok_button_clicked), NULL);
		g_signal_connect(app->open_colorsel, "delete_event",
						G_CALLBACK(gtk_widget_hide), NULL);
	}
	// if colour is non-NULL set it in the dialog as preselected colour
	if (colour != NULL && (colour[0] == '0' || colour[0] == '#'))
	{
		GdkColor gc;

		if (colour[0] == '0' && colour[1] == 'x')
		{	// we have a string of the format "0x00ff00" and we need it to "#00ff00"
			colour[1] = '#';
			colour++;
		}
		gdk_color_parse(colour, &gc);
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(
							GTK_COLOR_SELECTION_DIALOG(app->open_colorsel)->colorsel), &gc);
	}

	// We make sure the dialog is visible.
	gtk_window_present(GTK_WINDOW(app->open_colorsel));
#endif
}


void dialogs_show_input(const gchar *title, const gchar *label_text, const gchar *default_text,
						GCallback cb_dialog, GCallback cb_entry)
{
	GtkWidget *dialog, *label, *entry, *vbox;

	dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(app->window),
						GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	label = gtk_label_new(label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	entry = gtk_entry_new();
	if (default_text != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), default_text);
	}
	gtk_entry_set_max_length(GTK_ENTRY(entry), 255);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);

	if (cb_entry != NULL) g_signal_connect((gpointer) entry, "activate", cb_entry, dialog);
	g_signal_connect((gpointer) dialog, "response", cb_dialog, entry);
	g_signal_connect((gpointer) dialog, "delete_event", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), entry);
	gtk_widget_show_all(dialog);
}


void dialogs_show_goto_line()
{
	GtkWidget *dialog, *label, *entry, *vbox;

	dialog = gtk_dialog_new_with_buttons(_("Go to line"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));

	label = gtk_label_new(_("Enter the line you want to go to:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), 6);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);

	g_signal_connect((gpointer) entry, "activate", G_CALLBACK(on_goto_line_entry_activate), dialog);
	g_signal_connect((gpointer) dialog, "response", G_CALLBACK(on_goto_line_dialog_response), entry);

	gtk_container_add(GTK_CONTAINER(vbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), entry);
	gtk_widget_show_all(dialog);
}


void dialogs_show_includes_arguments_tex()
{
	GtkWidget *dialog, *label, *entries[4], *vbox, *table;
	gint idx = document_get_cur_idx();
	gint response;
	filetype *ft = NULL;

	if (DOC_IDX_VALID(idx)) ft = doc_list[idx].file_type;
	g_return_if_fail(ft != NULL);

	dialog = gtk_dialog_new_with_buttons(_("Set Arguments"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));

	label = gtk_label_new(_("Set programs and options for compiling and viewing (La)TeX files."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 6);
	gtk_container_add(GTK_CONTAINER(vbox), table);

	// LaTeX -> DVI args
	if (ft->programs->compiler != NULL)
	{
		label = gtk_label_new(_("DVI creation:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[0] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
		if (ft->programs->compiler)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[0]), ft->programs->compiler);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[0], 1, 2, 0, 1);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	}

	// LaTeX -> PDF args
	if (ft->programs->linker != NULL)
	{
		label = gtk_label_new(_("PDF creation:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[1] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
		if (ft->programs->linker)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[1]), ft->programs->linker);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[1], 1, 2, 1, 2);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	}

	// View LaTeX -> DVI args
	if (ft->programs->run_cmd != NULL)
	{
		label = gtk_label_new(_("DVI preview:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[2] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
		if (ft->programs->run_cmd)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[2]), ft->programs->run_cmd);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[2], 1, 2, 2, 3);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry3",
					gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);
	}

	// View LaTeX -> PDF args
	if (ft->programs->run_cmd2 != NULL)
	{
		label = gtk_label_new(_("PDF preview:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[3] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[3]), 30);
		if (ft->programs->run_cmd2)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[3]), ft->programs->run_cmd2);
		}
		gtk_table_attach_defaults(GTK_TABLE(table), entries[3], 1, 2, 3, 4);
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry4",
					gtk_widget_ref(entries[3]), (GDestroyNotify)gtk_widget_unref);
	}

	label = gtk_label_new(_("%f will be replaced by the current filename, e.g. test_file.c\n"
							"%e will be replaced by the filename without extension, e.g. test_file"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	gtk_widget_show_all(dialog);
	// run modally to prevent user changing idx filetype
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	// call the callback manually
	on_includes_arguments_tex_dialog_response(GTK_DIALOG(dialog), response, ft);

	gtk_widget_destroy(dialog);
}


void dialogs_show_includes_arguments_gen()
{
	GtkWidget *dialog, *label, *entries[3], *vbox;
	GtkWidget *ft_table = NULL;
	gint row = 0;
	gint idx = document_get_cur_idx();
	gint response;
	filetype *ft = NULL;

	if (DOC_IDX_VALID(idx)) ft = doc_list[idx].file_type;
	g_return_if_fail(ft != NULL);

	dialog = gtk_dialog_new_with_buttons(_("Set Includes and Arguments"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));

	label = gtk_label_new(_("Set the commands for building and running programs."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	if (ft->actions->can_compile || ft->actions->can_link || ft->actions->can_exec)
	{
		GtkWidget *align, *frame;
		gchar *frame_title = g_strconcat(ft->title, _(" commands"), NULL);

		frame = ui_frame_new_with_alignment(frame_title, &align);
		gtk_container_add(GTK_CONTAINER(vbox), frame);
		g_free(frame_title);

		ft_table = gtk_table_new(3, 2, FALSE);
		gtk_table_set_row_spacings(GTK_TABLE(ft_table), 6);
		gtk_container_add(GTK_CONTAINER(align), ft_table);
		row = 0;
	}

	// include-args
	if (ft->actions->can_compile)
	{
		label = gtk_label_new(_("Compile:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(ft_table), label, 0, 1, row, row + 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[0] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
		if (ft->programs->compiler)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[0]), ft->programs->compiler);
		}
		gtk_table_attach_defaults(GTK_TABLE(ft_table), entries[0], 1, 2, row, row + 1);
		row++;

		g_object_set_data_full(G_OBJECT(dialog), "includes_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	}

	// lib-args
	if (ft->actions->can_link)
	{
		label = gtk_label_new(_("Build:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(ft_table), label, 0, 1, row, row + 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[1] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
		if (ft->programs->linker)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[1]), ft->programs->linker);
		}
		gtk_table_attach_defaults(GTK_TABLE(ft_table), entries[1], 1, 2, row, row + 1);
		row++;

		g_object_set_data_full(G_OBJECT(dialog), "includes_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	}

	// program-args
	if (ft->actions->can_exec)
	{
		label = gtk_label_new(_("Execute:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_table_attach(GTK_TABLE(ft_table), label, 0, 1, row, row + 1,
			GTK_FILL, GTK_FILL | GTK_EXPAND, 6, 0);

		entries[2] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
		if (ft->programs->run_cmd)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[2]), ft->programs->run_cmd);
		}
		gtk_table_attach_defaults(GTK_TABLE(ft_table), entries[2], 1, 2, row, row + 1);
		row++;

		g_object_set_data_full(G_OBJECT(dialog), "includes_entry3",
						gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);
	}

	label = gtk_label_new(_("%f will be replaced by the current filename, e.g. test_file.c\n"
							"%e will be replaced by the filename without extension, e.g. test_file"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	gtk_widget_show_all(dialog);
	// run modally to prevent user changing idx filetype
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	// call the callback manually
	on_includes_arguments_dialog_response(GTK_DIALOG(dialog), response, ft);

	gtk_widget_destroy(dialog);
}


void dialogs_show_file_properties(gint idx)
{
	GtkWidget *dialog, *label, *table, *hbox, *image, *perm_table, *check, *vbox;
	gchar *file_size, *title, *base_name, *time_changed, *time_modified, *time_accessed, *enctext;
#if defined(HAVE_SYS_STAT_H) && defined(HAVE_SYS_TYPES_H)
	struct stat st;
	off_t filesize;
	mode_t mode;
	gchar *locale_filename;
#else
	gint filesize = 0;
	gint mode = 0;
#endif

// define this ones, to avoid later trouble
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

	if (idx == -1 || ! doc_list[idx].is_valid || doc_list[idx].file_name == NULL)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
		_("An error occurred or file information could not be retrieved (e.g. from a new file)."));
		return;
	}


#if defined(HAVE_SYS_STAT_H) && defined(TIME_WITH_SYS_TIME) && defined(HAVE_SYS_TYPES_H)
	locale_filename = utils_get_locale_from_utf8(doc_list[idx].file_name);
	if (stat(locale_filename, &st) == 0)
	{
		// first copy the returned string and the trim it, to not modify the static glibc string
		// g_strchomp() is used to remove trailing EOL chars, which are there for whatever reason
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

	base_name = g_path_get_basename(doc_list[idx].file_name);
	title = g_strconcat(base_name, " ", _("Properties"), NULL);
	dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(app->window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	g_free(title);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
	g_signal_connect(dialog, "delete_event", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);

	title = g_strdup_printf("<b>%s</b>", base_name);
	label = gtk_label_new(title);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	image = gtk_image_new_from_stock("gtk-file", GTK_ICON_SIZE_BUTTON);
	gtk_misc_set_alignment(GTK_MISC(image), 1.0, 0.5);
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_container_add(GTK_CONTAINER(hbox), image);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);
	g_free(title);

	table = gtk_table_new(8, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	label = gtk_label_new(_("<b>Type:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(doc_list[idx].file_type->title);
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

	label = gtk_label_new(doc_list[idx].file_name);
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), doc_list[idx].readonly);
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
		doc_list[idx].encoding,
		(encodings_is_unicode_charset(doc_list[idx].encoding)) ?
			((doc_list[idx].has_bom) ? _("(with BOM)") : _("(without BOM)")) : "");

	label = gtk_label_new(enctext);
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
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 7, 8,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	// add table
	gtk_container_add(GTK_CONTAINER(vbox), table);

	// create table with the permissions
	perm_table = gtk_table_new(5, 4, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(perm_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(perm_table), 5);

	label = gtk_label_new(_("<b>Permissions:</b>"));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 4, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	// Header
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

	// Owner
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


	// Group
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


	// Other
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


static gboolean
show_question(const gchar *yes_btn, const gchar *no_btn, const gchar *question_text,
	const gchar *extra_text)
{
	gboolean ret = FALSE;
#ifdef G_OS_WIN32
	gchar *string = (extra_text == NULL) ? g_strdup(question_text) :
		g_strconcat(question_text, "\n\n", extra_text, NULL);

	ret = win32_message_dialog(GTK_MESSAGE_QUESTION, string);
	g_free(string);
#else
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
		GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
		GTK_BUTTONS_NONE, "%s", question_text);
	// question_text will be in bold if optional extra_text used
	if (extra_text != NULL)
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
			"%s", extra_text);

	// For a cancel button, use cancel reponse so user can press escape to cancel
	gtk_dialog_add_button(GTK_DIALOG(dialog), no_btn,
		utils_str_equal(no_btn, GTK_STOCK_CANCEL) ? GTK_RESPONSE_CANCEL : GTK_RESPONSE_NO);
	gtk_dialog_add_button(GTK_DIALOG(dialog), yes_btn, GTK_RESPONSE_YES);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
		ret = TRUE;
	gtk_widget_destroy(dialog);
#endif
	return ret;
}


gboolean dialogs_show_question(const gchar *text, ...)
{
	gboolean ret = FALSE;
	gchar *string = g_malloc(512);
	va_list args;

	va_start(args, text);
	g_vsnprintf(string, 511, text, args);
	va_end(args);
	ret = show_question(GTK_STOCK_YES, GTK_STOCK_NO, string, NULL);
	g_free(string);
	return ret;
}


/* extra_text can be NULL; otherwise it is displayed below main_text. */
gboolean dialogs_show_question_full(const gchar *yes_btn, const gchar *no_btn,
	const gchar *extra_text, const gchar *main_text, ...)
{
	gboolean ret = FALSE;
	gchar *string = g_malloc(512);
	va_list args;

	va_start(args, main_text);
	g_vsnprintf(string, 511, main_text, args);
	va_end(args);
	ret = show_question(yes_btn, no_btn, string, extra_text);
	g_free(string);
	return ret;
}


void dialogs_show_keyboard_shortcuts()
{
	GtkWidget *dialog, *hbox, *label1, *label2, *label3, *swin, *vbox;
	GString *text_names = g_string_sized_new(600);
	GString *text_keys = g_string_sized_new(600);
	gchar *shortcut;
	guint i;
	gint height;

	dialog = gtk_dialog_new_with_buttons(_("Keyboard shortcuts"), GTK_WINDOW(app->window),
				GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	height = GEANY_WINDOW_MINIMAL_HEIGHT;
	gtk_window_set_default_size(GTK_WINDOW(dialog), height * 0.8, height);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);

	label3 = gtk_label_new(_("The following keyboard shortcuts are defined:"));
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 6);

	label1 = gtk_label_new(NULL);

	label2 = gtk_label_new(NULL);

	for (i = 0; i < GEANY_MAX_KEYS; i++)
	{
		shortcut = gtk_accelerator_get_label(keys[i]->key, keys[i]->mods);
		g_string_append(text_names, keys[i]->label);
		g_string_append(text_names, "\n");
		g_string_append(text_keys, shortcut);
		g_string_append(text_keys, "\n");
		g_free(shortcut);
	}

	gtk_label_set_text(GTK_LABEL(label1), text_names->str);
	gtk_label_set_text(GTK_LABEL(label2), text_keys->str);

	gtk_container_add(GTK_CONTAINER(hbox), label1);
	gtk_container_add(GTK_CONTAINER(hbox), label2);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(swin), hbox);

	gtk_box_pack_start(GTK_BOX(vbox), label3, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

	g_signal_connect((gpointer) dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show_all(dialog);

	g_string_free(text_names, TRUE);
	g_string_free(text_keys, TRUE);
}

