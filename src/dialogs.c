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
#include "keybindings.h"


static void dialogs_add_find_checkboxes(GtkDialog *dialog);


/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file ()
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) < GEANY_MAX_OPEN_FILES)
	{
#ifdef G_OS_WIN32
		win32_show_file_dialog(TRUE);
#else /* X11, not win32: use GTK_FILE_CHOOSER */

		/* We use the same file selection widget each time, so first
	   		of all we create it if it hasn't already been created. */
		if (app->open_filesel == NULL)
		{
			GtkWidget *combo;
			GtkWidget *viewbtn;
			GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));
			gint i;

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

			gtk_widget_set_size_request(app->open_filesel, 520, 460);
			gtk_window_set_modal(GTK_WINDOW(app->open_filesel), TRUE);
			gtk_window_set_destroy_with_parent(GTK_WINDOW(app->open_filesel), TRUE);
			gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->open_filesel), TRUE);
			gtk_window_set_type_hint(GTK_WINDOW(app->open_filesel), GDK_WINDOW_TYPE_HINT_DIALOG);
			gtk_window_set_transient_for(GTK_WINDOW(app->open_filesel), GTK_WINDOW(app->window));
			gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(app->open_filesel), TRUE);

			// add checkboxes and filename entry
			gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(app->open_filesel),
						dialogs_add_file_open_extra_widget());
			combo = lookup_widget(app->open_filesel, "filetype_combo");

			// add FileFilters(start with "All Files")
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->open_filesel),
						utils_create_file_filter(filetypes[GEANY_FILETYPES_ALL]));
			for (i = 0; i < GEANY_MAX_FILE_TYPES - 1; i++)
			{
				if (filetypes[i])
				{
					gtk_combo_box_append_text(GTK_COMBO_BOX(combo), filetypes[i]->title);
					gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->open_filesel),
						utils_create_file_filter(filetypes[i]));
				}
			}
			gtk_combo_box_append_text(GTK_COMBO_BOX(combo), _("Detect by file extension  "));
			gtk_combo_box_set_active(GTK_COMBO_BOX(combo), GEANY_MAX_FILE_TYPES - 1);

			g_signal_connect((gpointer) app->open_filesel, "selection-changed",
						G_CALLBACK(on_file_open_selection_changed), NULL);
			g_signal_connect ((gpointer) app->open_filesel, "delete_event",
						G_CALLBACK(gtk_widget_hide), NULL);
			g_signal_connect((gpointer) app->open_filesel, "response",
						G_CALLBACK(on_file_open_dialog_response), NULL);

 		}

		// set dialog directory to the current file's directory, if present
		{
			gchar *initdir = utils_get_current_file_dir();

			if (initdir != NULL)
			{
				gchar *locale_filename;

				locale_filename = g_locale_from_utf8(initdir, -1, NULL, NULL, NULL);
				if (locale_filename == NULL) locale_filename = g_strdup(initdir);

				if (g_path_is_absolute(locale_filename))
					gtk_file_chooser_set_current_folder(
						GTK_FILE_CHOOSER(app->open_filesel), locale_filename);

				g_free(initdir);
				g_free(locale_filename);
			}
		}

		gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->open_filesel));
		gtk_widget_show(app->open_filesel);
#endif
	}
	else
	{
		dialogs_show_error(
		_("You have opened too many files. There is a limit of %d concurrent open files."),
		GEANY_MAX_OPEN_FILES);
	}
}


/* This shows the file selection dialog to save a file. */
void dialogs_show_save_as()
{
#ifdef G_OS_WIN32
	win32_show_file_dialog(FALSE);
#else
	gint idx = document_get_cur_idx();

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
		gchar *locale_filename = g_locale_from_utf8(doc_list[idx].file_name, -1, NULL, NULL, NULL);
		if (locale_filename == NULL) locale_filename = g_strdup(doc_list[idx].file_name);

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
	gtk_dialog_run(GTK_DIALOG(app->save_filesel));
#endif
}


void dialogs_show_info(const gchar *text, ...)
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
	win32_message_dialog(GTK_MESSAGE_INFO, string);
#else

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", string);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
#endif
	g_free(string);
}


void dialogs_show_error(const gchar *text, ...)
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
	win32_message_dialog(GTK_MESSAGE_ERROR, string);
#else
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", string);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
#endif
	g_free(string);
}


gboolean dialogs_show_unsaved_file(gint idx)
{
#ifndef G_OS_WIN32
	GtkWidget *dialog, *button, *label, *image, *hbox, *align;
#endif
	gchar *msg;
	gint ret;

	if (doc_list[idx].file_name != NULL)
	{
		gchar *short_fn = g_path_get_basename(doc_list[idx].file_name);
		msg  = (gchar*) g_malloc(100 + strlen(short_fn));
		g_snprintf(msg, 100 + strlen(doc_list[idx].file_name),
				_("The file '%s' is not saved.\nDo you want to save it before closing?"), short_fn);
		g_free(short_fn);
	}
	else
	{
		msg  = g_strdup(_("The file is not saved.\nDo you want to save it before closing?"));
	}
#ifdef G_OS_WIN32
	ret = win32_message_dialog_unsaved(msg);
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", msg);

	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = gtk_button_new();
	label = gtk_label_new(_("Don't save"));
	image = gtk_image_new_from_stock(GTK_STOCK_NO, GTK_ICON_SIZE_BUTTON);
	hbox = gtk_hbox_new(FALSE, 2);

	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(button), align);
	gtk_container_add(GTK_CONTAINER(align), hbox);
	gtk_widget_show_all(align);
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
				dialogs_show_save_as();
			else
				document_save_file(idx, FALSE);
			ret = TRUE;
			break;
		}
		case GTK_RESPONSE_NO: ret = TRUE; break;
		case GTK_RESPONSE_CANCEL: /* fall through to default and leave the function */
		default: ret = FALSE; break;
	}

	return (gboolean) ret;
}


/* This shows the font selection dialog to choose a font. */
void dialogs_show_open_font(void)
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


void dialogs_show_word_count(void)
{
	GtkWidget *dialog, *label;
	gint idx;
	guint chars, lines, words;
	gchar *string, *text, *range;
	GtkRequisition *size;

	idx = document_get_cur_idx();
	if (idx == -1 || ! doc_list[idx].is_valid) return;

	size = g_new(GtkRequisition, 1);
	dialog = gtk_dialog_new_with_buttons(_("Word Count"), GTK_WINDOW(app->window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CLOSE, GTK_RESPONSE_NONE, NULL);
	if (sci_can_copy(doc_list[idx].sci))
	{
		text = g_malloc0(sci_get_selected_text_length(doc_list[idx].sci) + 1);
		sci_get_selected_text(doc_list[idx].sci, text);
		utils_wordcount(text, &chars, &lines, &words);
		g_free(text);
		range = _("selection");
	}
	else
	{
		text = g_malloc(sci_get_length(doc_list[idx].sci) + 1);
		sci_get_text(doc_list[idx].sci, sci_get_length(doc_list[idx].sci) + 1 , text);
		utils_wordcount(text, &chars, &lines, &words);
		g_free(text);
		range = _("whole document");
	}
	string = g_strdup_printf(_("Range:\t\t%s\n\nLines:\t\t%d\nWords:\t\t%d\nCharacters:\t%d\n"), range, lines, words, chars);
	label = gtk_label_new(string);
	g_free(string);

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
	g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_widget_destroy), dialog);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	gtk_widget_size_request(label, size);
	gtk_widget_set_size_request(dialog, size->width + 20, size->height + 50);

	g_free(size);
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


#define GEANY_ADD_WIDGET_ACCEL(gkey, menuitem) \
	if (keys[(gkey)]->key != 0) \
		gtk_widget_add_accelerator(menuitem, "activate", accel_group, \
			keys[(gkey)]->key, keys[(gkey)]->mods, GTK_ACCEL_VISIBLE)

GtkWidget *dialogs_create_build_menu_gen(gint idx)
{
	GtkWidget *menu, *item = NULL, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));
	filetype *ft = doc_list[idx].file_type;

	menu = gtk_menu_new();

	if (ft->menu_items->can_compile)
	{
		// compile the code
		item = gtk_image_menu_item_new_with_mnemonic(_("_Compile"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file"), NULL);
		GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_COMPILE, item);
		image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
		gtk_widget_show(image);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
		g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_compile_activate), NULL);
		ft->menu_items->item_compile = item;
	}

	if (ft->menu_items->can_link)
	{	// build the code
		item = gtk_image_menu_item_new_with_mnemonic(_("_Build"));
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		gtk_tooltips_set_tip(tooltips, item,
					_("Builds the current file (generate an executable file)"), NULL);
		GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_LINK, item);
		g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_build_activate), NULL);
		ft->menu_items->item_link = item;
	}

	if (item != NULL)
	{
		item = gtk_separator_menu_item_new();
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
	}

	// build the code with make all
	item = gtk_image_menu_item_new_with_mnemonic(_("_Make all"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKE, item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(0));

	// build the code with make
	item = gtk_image_menu_item_new_with_mnemonic(_("Make custom _target"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOWNTARGET, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(1));

	// build the code with make object
	item = gtk_image_menu_item_new_with_mnemonic(_("Make _object"));
	gtk_widget_show(item);
	GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_MAKEOBJECT, item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file using the "
										   "make tool"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(2));

	if (ft->menu_items->can_exec)
	{	// execute the code
		item = gtk_separator_menu_item_new();
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);

		item = gtk_image_menu_item_new_from_stock("gtk-execute", accel_group);
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		gtk_tooltips_set_tip(tooltips, item, _("Run or view the current file"), NULL);
		GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_RUN, item);
		g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_execute_activate), NULL);
		ft->menu_items->item_exec = item;
	}

	// arguments
	if (ft->menu_items->can_compile || ft->menu_items->can_link || ft->menu_items->can_exec)
	{
		// separator
		separator = gtk_separator_menu_item_new();
		gtk_widget_show(separator);
		gtk_container_add(GTK_CONTAINER(menu), separator);
		gtk_widget_set_sensitive(separator, FALSE);

		item = gtk_image_menu_item_new_with_mnemonic(_("_Set Includes and Arguments"));
		gtk_widget_show(item);
		GEANY_ADD_WIDGET_ACCEL(GEANY_KEYS_BUILD_OPTIONS, item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		gtk_tooltips_set_tip(tooltips, item,
					_("Sets the includes and library paths for the compiler and "
					  "the program arguments for execution"), NULL);
		image = gtk_image_new_from_stock("gtk-preferences", GTK_ICON_SIZE_MENU);
		gtk_widget_show(image);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
		g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_arguments_activate), NULL);
	}

	return menu;
}


GtkWidget *dialogs_create_build_menu_tex(gint idx)
{
	GtkWidget *menu, *item, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	menu = gtk_menu_new();

	// DVI
	item = gtk_image_menu_item_new_with_mnemonic(_("LaTeX -> DVI"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a DVI file"), NULL);
	if (keys[GEANY_KEYS_BUILD_COMPILE]->key)
		gtk_widget_add_accelerator(item, "activate", accel_group, keys[GEANY_KEYS_BUILD_COMPILE]->key,
			keys[GEANY_KEYS_BUILD_COMPILE]->mods, GTK_ACCEL_VISIBLE);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(0));

	// PDF
	item = gtk_image_menu_item_new_with_mnemonic(_("LaTeX -> PDF"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a PDF file"), NULL);
	if (keys[GEANY_KEYS_BUILD_LINK]->key)
		gtk_widget_add_accelerator(item, "activate", accel_group, keys[GEANY_KEYS_BUILD_LINK]->key,
			keys[GEANY_KEYS_BUILD_LINK]->mods, GTK_ACCEL_VISIBLE);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(1));

	// build the code with make all
	item = gtk_image_menu_item_new_with_mnemonic(_("Build with \"make\""));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
	if (keys[GEANY_KEYS_BUILD_MAKE]->key)
		gtk_widget_add_accelerator(item, "activate", accel_group, keys[GEANY_KEYS_BUILD_MAKE]->key,
			keys[GEANY_KEYS_BUILD_MAKE]->mods, GTK_ACCEL_VISIBLE);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(0));

	// build the code with make
	item = gtk_image_menu_item_new_with_mnemonic(_("Build with make (custom target)"));
	gtk_widget_show(item);
	if (keys[GEANY_KEYS_BUILD_MAKEOWNTARGET]->key)
		gtk_widget_add_accelerator(item, "activate", accel_group, keys[GEANY_KEYS_BUILD_MAKEOWNTARGET]->key,
			keys[GEANY_KEYS_BUILD_MAKEOWNTARGET]->mods, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(1));

	// DVI view
	item = gtk_image_menu_item_new_with_mnemonic(_("View DVI file"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	if (keys[GEANY_KEYS_BUILD_RUN]->key)
		gtk_widget_add_accelerator(item, "activate", accel_group, keys[GEANY_KEYS_BUILD_RUN]->key,
			keys[GEANY_KEYS_BUILD_RUN]->mods, GTK_ACCEL_VISIBLE);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles and view the current file"), NULL);
	image = gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(2));

	// PDF view
	item = gtk_image_menu_item_new_with_mnemonic(_("View PDF file"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	if (keys[GEANY_KEYS_BUILD_RUN2]->key)
		gtk_widget_add_accelerator(item, "activate", accel_group, keys[GEANY_KEYS_BUILD_RUN2]->key,
			keys[GEANY_KEYS_BUILD_RUN2]->mods, GTK_ACCEL_VISIBLE);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles and view the current file"), NULL);
	image = gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(3));

	// separator
	separator = gtk_separator_menu_item_new();
	gtk_widget_show(separator);
	gtk_container_add(GTK_CONTAINER(menu), separator);
	gtk_widget_set_sensitive(separator, FALSE);

	// arguments
	item = gtk_image_menu_item_new_with_mnemonic(_("Set Arguments"));
	gtk_widget_show(item);
	if (keys[GEANY_KEYS_BUILD_OPTIONS]->key)
		gtk_widget_add_accelerator(item, "activate", accel_group, keys[GEANY_KEYS_BUILD_OPTIONS]->key,
			keys[GEANY_KEYS_BUILD_OPTIONS]->mods, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Sets the program paths and arguments"), NULL);
	image = gtk_image_new_from_stock("gtk-preferences", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_arguments_activate), NULL);

	gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);

	return menu;
}


void dialogs_create_recent_menu(void)
{
	GtkWidget *recent_menu = lookup_widget(app->window, "recent_files1_menu");
	GtkWidget *tmp;
	guint i;
	gchar *filename;

	if (g_queue_get_length(app->recent_queue) == 0)
	{
		gtk_widget_set_sensitive(lookup_widget(app->window, "recent_files1"), FALSE);
		return;
	}

	for (i = 0; i < MIN(app->mru_length, g_queue_get_length(app->recent_queue));
		i++)
	{
		filename = g_queue_peek_nth(app->recent_queue, i);
		tmp = gtk_menu_item_new_with_label(filename);
		gtk_widget_show(tmp);
		gtk_menu_shell_append(GTK_MENU_SHELL(recent_menu), tmp);
		g_signal_connect((gpointer) tmp, "activate",
					G_CALLBACK(on_recent_file_activate), (gpointer) filename);
	}
}


void dialogs_show_make_target(void)
{
	GtkWidget *dialog, *label, *entry;

	dialog = gtk_dialog_new_with_buttons(_("Enter custom options for the make tool"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	label = gtk_label_new(_("Enter custom options here, all entered text is passed to the make command."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	entry = gtk_entry_new();
	if (app->build_make_custopt)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), app->build_make_custopt);
	}
	gtk_entry_set_max_length(GTK_ENTRY(entry), 248);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);

	g_signal_connect((gpointer) entry, "activate", G_CALLBACK(on_make_target_entry_activate), dialog);
	g_signal_connect((gpointer) dialog, "response", G_CALLBACK(on_make_target_dialog_response), entry);
	g_signal_connect((gpointer) dialog, "delete_event", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entry);
	gtk_widget_show_all(dialog);
}


void dialogs_show_goto_line(void)
{
	GtkWidget *dialog, *label, *entry;

	dialog = gtk_dialog_new_with_buttons(_("Go to line"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	label = gtk_label_new(_("Enter the line you want to go to"));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), 6);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);

	g_signal_connect((gpointer) entry, "activate", G_CALLBACK(on_goto_line_entry_activate), dialog);
	g_signal_connect((gpointer) dialog, "response", G_CALLBACK(on_goto_line_dialog_response), entry);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entry);
	gtk_widget_show_all(dialog);
}


static void dialogs_add_find_checkboxes(GtkDialog *dialog)
{
	GtkWidget *checkbox1, *checkbox2, *check_regexp, *checkbox4, *checkbox5,
			  *checkbox7, *hbox, *lbox, *rbox;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	checkbox1 = gtk_check_button_new_with_mnemonic(_("_Case sensitive"));
	g_object_set_data_full(G_OBJECT(dialog), "check_case",
					gtk_widget_ref(checkbox1), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);

	checkbox2 = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
	g_object_set_data_full(G_OBJECT(dialog), "check_word",
					gtk_widget_ref(checkbox2), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);

	checkbox5 = gtk_check_button_new_with_mnemonic(_("Match only word s_tart"));
	g_object_set_data_full(G_OBJECT(dialog), "check_wordstart",
					gtk_widget_ref(checkbox5), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox5), FALSE);

	lbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(lbox), checkbox1);
	gtk_container_add(GTK_CONTAINER(lbox), checkbox2);
	gtk_container_add(GTK_CONTAINER(lbox), checkbox5);

	check_regexp = gtk_check_button_new_with_mnemonic(_("_Use regular expressions"));
	g_object_set_data_full(G_OBJECT(dialog), "check_regexp",
					gtk_widget_ref(check_regexp), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(check_regexp), FALSE);
	gtk_tooltips_set_tip(tooltips, check_regexp, _("Use POSIX-like regular expressions. "
		"For detailed information about using regular expressions, please read the documentation."), NULL);
	g_signal_connect(G_OBJECT(check_regexp), "toggled",
		G_CALLBACK(on_find_replace_checkbutton_toggled), GTK_WIDGET(dialog));

	checkbox4 = gtk_check_button_new_with_mnemonic(_("_Search backwards"));
	g_object_set_data_full(G_OBJECT(dialog), "check_back",
					gtk_widget_ref(checkbox4), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox4), FALSE);

	checkbox7 = gtk_check_button_new_with_mnemonic(_("_Replace control characters"));
	g_object_set_data_full(G_OBJECT(dialog), "check_escape",
					gtk_widget_ref(checkbox7), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox7), FALSE);
	gtk_tooltips_set_tip(tooltips, checkbox7,
		_("Replace \\\\, \\t, \\n, \\r and \\uXXXX (Unicode chararacters) with the "
		  "corresponding control characters."), NULL);

	rbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(rbox), check_regexp);
	gtk_container_add(GTK_CONTAINER(rbox), checkbox7);
	gtk_container_add(GTK_CONTAINER(rbox), checkbox4);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(hbox), lbox);
	gtk_container_add(GTK_CONTAINER(hbox), rbox);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 6);
}


void dialogs_show_find(void)
{
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	if (sci_get_lines_selected(doc_list[idx].sci) == 1)
	{
		sel = g_malloc(sci_get_selected_text_length(doc_list[idx].sci));
		sci_get_selected_text(doc_list[idx].sci, sel);
	}

	if (app->find_dialog == NULL)
	{
		GtkWidget *label, *entry, *vbox;

		app->find_dialog = gtk_dialog_new_with_buttons(_("Find"),
			GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
			GTK_STOCK_FIND, GTK_RESPONSE_ACCEPT, NULL);

		label = gtk_label_new(_("Enter the search text here:"));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

		entry = gtk_combo_box_entry_new_text();
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 50);
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(entry)->child), sel);
		g_object_set_data_full(G_OBJECT(app->find_dialog), "entry",
						gtk_widget_ref(entry), (GDestroyNotify)gtk_widget_unref);

		g_signal_connect((gpointer) gtk_bin_get_child(GTK_BIN(entry)), "activate",
				G_CALLBACK(on_find_entry_activate), NULL);
		g_signal_connect((gpointer) app->find_dialog, "response",
				G_CALLBACK(on_find_dialog_response), entry);
		g_signal_connect((gpointer) app->find_dialog, "delete_event",
				G_CALLBACK(gtk_widget_hide), NULL);

		vbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox), label);
		gtk_container_add(GTK_CONTAINER(vbox), entry);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), vbox);

		dialogs_add_find_checkboxes(GTK_DIALOG(app->find_dialog));

		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(app->find_dialog)->vbox), 3);

		gtk_widget_show_all(app->find_dialog);
	}
	else
	{
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(
							lookup_widget(app->find_dialog, "entry"))->child), sel);
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(app->find_dialog, "entry"))->child));
		gtk_widget_show(app->find_dialog);
	}
	g_free(sel);
}


void dialogs_show_replace(void)
{
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	if (sci_get_lines_selected(doc_list[idx].sci) == 1)
	{
		sel = g_malloc(sci_get_selected_text_length(doc_list[idx].sci));
		sci_get_selected_text(doc_list[idx].sci, sel);
	}

	if (app->replace_dialog == NULL)
	{
		GtkWidget *label_find, *label_replace, *entry_find, *entry_replace,
			*checkbox6, *button, *align, *rbox, *fbox;
		GtkSizeGroup *size_group;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		app->replace_dialog = gtk_dialog_new_with_buttons(_("Replace"),
			GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);

		button = gtk_button_new_with_mnemonic(_("_In Selection"));
		gtk_tooltips_set_tip(tooltips, button,
			_("Replace all matches found in the currently selected text"), NULL);
		gtk_widget_show(button);
		gtk_dialog_add_action_widget(GTK_DIALOG(app->replace_dialog), button,
			GEANY_RESPONSE_REPLACE_SEL);
		button = gtk_button_new_with_mnemonic(_("Replace _All"));
		gtk_widget_show(button);
		gtk_dialog_add_action_widget(GTK_DIALOG(app->replace_dialog), button,
			GEANY_RESPONSE_REPLACE_ALL);
		button = gtk_button_new_from_stock(GTK_STOCK_FIND);
		gtk_widget_show(button);
		gtk_dialog_add_action_widget(GTK_DIALOG(app->replace_dialog), button,
			GEANY_RESPONSE_FIND);
		button = gtk_button_new_with_mnemonic(_("_Replace"));
		gtk_widget_show(button);
		gtk_dialog_add_action_widget(GTK_DIALOG(app->replace_dialog), button,
			GEANY_RESPONSE_REPLACE);

		label_find = gtk_label_new(_("Enter the search text here:"));
		gtk_misc_set_alignment(GTK_MISC(label_find), 0, 0.5);

		label_replace = gtk_label_new(_("Enter the replace text here:"));
		gtk_misc_set_alignment(GTK_MISC(label_replace), 0, 0.5);

		entry_find = gtk_combo_box_entry_new_text();
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 50);
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(entry_find)->child), sel);
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "entry_find",
						gtk_widget_ref(entry_find), (GDestroyNotify)gtk_widget_unref);

		entry_replace = gtk_combo_box_entry_new_text();
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))), 50);
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "entry_replace",
						gtk_widget_ref(entry_replace), (GDestroyNotify)gtk_widget_unref);

		g_signal_connect((gpointer) gtk_bin_get_child(GTK_BIN(entry_replace)), "activate",
				G_CALLBACK(on_replace_entry_activate), NULL);
		g_signal_connect((gpointer) app->replace_dialog, "response",
				G_CALLBACK(on_replace_dialog_response), entry_replace);
		g_signal_connect((gpointer) app->replace_dialog, "delete_event",
				G_CALLBACK(gtk_widget_hide), NULL);

		fbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(fbox), label_find);
		gtk_container_add(GTK_CONTAINER(fbox), entry_find);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(app->replace_dialog)->vbox), fbox,
			FALSE, FALSE, 6);

		rbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(rbox), label_replace);
		gtk_container_add(GTK_CONTAINER(rbox), entry_replace);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(app->replace_dialog)->vbox), rbox,
			FALSE, FALSE, 3);

		size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget(size_group, label_find);
		gtk_size_group_add_widget(size_group, label_replace);

		dialogs_add_find_checkboxes(GTK_DIALOG(app->replace_dialog));

		checkbox6 = gtk_check_button_new_with_mnemonic(_("Replace in all _open files"));
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "check_all_buffers",
						gtk_widget_ref(checkbox6), (GDestroyNotify)gtk_widget_unref);
		gtk_tooltips_set_tip(tooltips, checkbox6,
			_("Replaces the search text in all opened files. This option is only useful(and used) if you click on \"Replace All\"."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox6), FALSE);

		align = gtk_alignment_new(0, 0, 1, 1);
		gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 9, 0, 0);
		gtk_container_add(GTK_CONTAINER(align), checkbox6);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), align);

		gtk_widget_show_all(app->replace_dialog);
	}
	else
	{
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(
							lookup_widget(app->replace_dialog, "entry_find"))->child), sel);
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(app->replace_dialog, "entry_find"))->child));
		gtk_widget_show(app->replace_dialog);
	}
	g_free(sel);
}


void dialogs_show_find_in_files(void)
{
	static GtkWidget *combo = NULL;
	static GtkWidget *entry1;
	GtkWidget *entry2; // the child GtkEntry of combo
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;
	gchar *cur_dir;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	cur_dir = utils_get_current_file_dir();

	if (app->find_in_files_dialog == NULL)
	{
		GtkWidget *label, *label1, *checkbox1, *checkbox2, *checkbox3, *vbox2, *vbox1;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		app->find_in_files_dialog = gtk_dialog_new_with_buttons(
			_("Find in files"), GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

		gtk_dialog_add_button(GTK_DIALOG(app->find_in_files_dialog), "gtk-find", GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response(GTK_DIALOG(app->find_in_files_dialog),
			GTK_RESPONSE_ACCEPT);

		label1 = gtk_label_new("Directory to be searched:");
		gtk_misc_set_alignment(GTK_MISC(label1), 0, 0);

		entry1 = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(entry1), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(entry1), 50);
		g_object_set_data_full(G_OBJECT(app->find_in_files_dialog), "entry_dir",
						gtk_widget_ref(entry1), (GDestroyNotify)gtk_widget_unref);

		vbox1 = gtk_vbox_new(FALSE, 5);
		gtk_box_pack_start(GTK_BOX(vbox1), label1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox1), entry1, FALSE, FALSE, 0);

		label = gtk_label_new(_("Enter the search text here:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

		combo = gtk_combo_box_entry_new_text();
		entry2 = gtk_bin_get_child(GTK_BIN(combo));
		gtk_entry_set_max_length(GTK_ENTRY(entry2), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(entry2), 50);
		gtk_entry_set_activates_default(GTK_ENTRY(entry2), TRUE);

		checkbox1 = gtk_check_button_new_with_mnemonic(_("_Case sensitive"));
		g_object_set_data_full(G_OBJECT(app->find_in_files_dialog), "check_case",
						gtk_widget_ref(checkbox1), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);

		checkbox2 = gtk_check_button_new_with_mnemonic(_("Invert search results"));
		g_object_set_data_full(G_OBJECT(app->find_in_files_dialog), "check_invert",
						gtk_widget_ref(checkbox2), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);
		gtk_tooltips_set_tip(tooltips, checkbox2,
				_("Invert the sense of matching, to select non-matching lines."), NULL);

		checkbox3 = gtk_check_button_new_with_mnemonic(_("_Use extended regular expressions"));
		g_object_set_data_full(G_OBJECT(app->find_in_files_dialog), "check_eregexp",
						gtk_widget_ref(checkbox3), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox3), FALSE);
		gtk_tooltips_set_tip(tooltips, checkbox3,
							_("See grep's manual page for more information."), NULL);

		vbox2 = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox2), checkbox1);
		gtk_container_add(GTK_CONTAINER(vbox2), checkbox2);
		gtk_container_add(GTK_CONTAINER(vbox2), checkbox3);

		g_signal_connect((gpointer) app->find_in_files_dialog, "response",
				G_CALLBACK(on_find_in_files_dialog_response), combo);
		g_signal_connect((gpointer) app->find_in_files_dialog, "delete_event",
				G_CALLBACK(gtk_widget_hide), NULL);

		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(app->find_in_files_dialog)->vbox),
			vbox1, TRUE, TRUE, 6);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(app->find_in_files_dialog)->vbox),
			label, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(app->find_in_files_dialog)->vbox),
			combo, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(app->find_in_files_dialog)->vbox),
			vbox2, TRUE, TRUE, 6);

		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(app->find_in_files_dialog)->vbox), 6);

		gtk_widget_show_all(app->find_in_files_dialog);
	}

	if (sci_get_lines_selected(doc_list[idx].sci) == 1)
	{
		sel = g_malloc(sci_get_selected_text_length(doc_list[idx].sci));
		sci_get_selected_text(doc_list[idx].sci, sel);
	}

	entry2 = GTK_BIN(combo)->child;
	if (sel) gtk_entry_set_text(GTK_ENTRY(entry2), sel);
	g_free(sel);

	if (cur_dir) gtk_entry_set_text(GTK_ENTRY(entry1), cur_dir);
	g_free(cur_dir);

	// put the focus to the directory entry if it is empty
	if (utils_strcmp(gtk_entry_get_text(GTK_ENTRY(entry1)), ""))
		gtk_widget_grab_focus(entry1);
	else
		gtk_widget_grab_focus(entry2);

	gtk_widget_show(app->find_in_files_dialog);
}


void dialogs_show_includes_arguments_tex(void)
{
	GtkWidget *dialog, *label, *entries[4];
	gint idx = document_get_cur_idx();
	filetype *ft = doc_list[idx].file_type;

	dialog = gtk_dialog_new_with_buttons(_("Set Arguments"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	label = gtk_label_new(_("Set programs and options for compilation and viewing (La)TeX files.\nThe filename is appended automatically at the end.\n"));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

	// LaTeX -> DVI args
	if (ft->programs->compiler != NULL)
	{
		label = gtk_label_new(_("Enter here the (La)TeX command (for DVI creation) and some useful options."));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		entries[0] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
		if (ft->programs->compiler)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[0]), ft->programs->compiler);
		}
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[0]);

		// whitespace
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	}

	// LaTeX -> PDF args
	if (ft->programs->linker != NULL)
	{
		label = gtk_label_new(_("Enter here the (La)TeX command (for PDF creation) and some useful options."));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		entries[1] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
		if (ft->programs->linker)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[1]), ft->programs->linker);
		}
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[1]);

		// whitespace
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	}

	// View LaTeX -> DVI args
	if (ft->programs->run_cmd != NULL)
	{
		label = gtk_label_new(_("Enter here the (La)TeX command (for DVI preview) and some useful options."));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		entries[2] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
		if (ft->programs->run_cmd)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[2]), ft->programs->run_cmd);
		}
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[2]);

		// whitespace
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry3",
					gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);
	}

	// View LaTeX -> PDF args
	if (ft->programs->run_cmd2 != NULL)
	{
		label = gtk_label_new(_("Enter here the (La)TeX command (for PDF preview) and some useful options."));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		entries[3] = gtk_entry_new();
		gtk_entry_set_width_chars(GTK_ENTRY(entries[3]), 30);
		if (ft->programs->run_cmd2)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[3]), ft->programs->run_cmd2);
		}
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[3]);

		// whitespace
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
		g_object_set_data_full(G_OBJECT(dialog), "tex_entry4",
					gtk_widget_ref(entries[3]), (GDestroyNotify)gtk_widget_unref);
	}

	g_signal_connect((gpointer) dialog, "response",
				G_CALLBACK(on_includes_arguments_tex_dialog_response), GINT_TO_POINTER(idx));

	gtk_widget_show_all(dialog);
}


void dialogs_show_includes_arguments_gen(void)
{
	GtkWidget *dialog, *label, *entries[3];
	gint idx = document_get_cur_idx();
	filetype *ft = doc_list[idx].file_type;
	GtkTooltips *tooltips;
	dialog = gtk_dialog_new_with_buttons(_("Set Includes and Arguments"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	label = gtk_label_new(_("Sets the includes and library paths for the compiler and the program arguments for execution\n"));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	// include-args
	if (ft->menu_items->can_compile)
	{
		label = gtk_label_new(_("Enter here arguments to your compiler."));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		entries[0] = gtk_entry_new();
		gtk_tooltips_set_tip(tooltips, entries[0],
_("%f will be replaced by the complete filename\n%e will be replaced by filename without extension"
  "\nExample: test_file.c\n%f -> test_file.c\n%e -> test_file"), NULL);
		gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
		if (ft->programs->compiler)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[0]), ft->programs->compiler);
		}
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[0]);

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
		g_object_set_data_full(G_OBJECT(dialog), "includes_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	}

	// lib-args
	if (ft->menu_items->can_link)
	{
		label = gtk_label_new(_("Enter here arguments to your linker."));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		entries[1] = gtk_entry_new();
		gtk_tooltips_set_tip(tooltips, entries[1],
_("%f will be replaced by the complete filename\n%e will be replaced by filename without extension"
  "\nExample: test_file.c\n%f -> test_file.c\n%e -> test_file"), NULL);
		gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
		if (ft->programs->linker)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[1]), ft->programs->linker);
		}
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[1]);

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
		g_object_set_data_full(G_OBJECT(dialog), "includes_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	}

	// lib-args
	if (ft->menu_items->can_exec)
	{
		// program-args
		label = gtk_label_new(_("Enter here arguments to your program."));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
		entries[2] = gtk_entry_new();
		gtk_tooltips_set_tip(tooltips, entries[2],
_("%f will be replaced by the complete filename\n%e will be replaced by filename without extension"
  "\nExample: test_file.c\n%f -> test_file.c\n%e -> test_file"), NULL);
		gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
		if (ft->programs->run_cmd)
		{
			gtk_entry_set_text(GTK_ENTRY(entries[2]), ft->programs->run_cmd);
		}
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[2]);

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
		g_object_set_data_full(G_OBJECT(dialog), "includes_entry3",
						gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);
	}

	g_signal_connect((gpointer) dialog, "response",
				G_CALLBACK(on_includes_arguments_dialog_response), GINT_TO_POINTER(idx));

	gtk_widget_show_all(dialog);
}


GtkWidget *dialogs_add_file_open_extra_widget(void)
{
	GtkWidget *vbox;
	GtkWidget *lbox;
	GtkWidget *ebox;
	GtkWidget *hbox;
	GtkWidget *file_entry;
	GtkSizeGroup *sizegroup;
	GtkWidget *align;
	GtkWidget *check_hidden;
	GtkWidget *filetype_label;
	GtkWidget *filetype_combo;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	vbox = gtk_vbox_new(FALSE, 6);
	sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	align = gtk_alignment_new(1.0, 0.0, 0.0, 0.0);
	check_hidden = gtk_check_button_new_with_mnemonic(_("Show _hidden files"));
	gtk_widget_show(check_hidden);
	gtk_size_group_add_widget(GTK_SIZE_GROUP(sizegroup), check_hidden);
	gtk_container_add(GTK_CONTAINER(align), check_hidden);
	gtk_box_pack_start(GTK_BOX(vbox), align, FALSE, FALSE, 0);
	gtk_button_set_focus_on_click(GTK_BUTTON(check_hidden), FALSE);

	lbox = gtk_hbox_new(FALSE, 12);
	file_entry = gtk_entry_new();
	gtk_widget_show(file_entry);
	gtk_box_pack_start(GTK_BOX(lbox), file_entry, TRUE, TRUE, 0);
	//gtk_editable_set_editable(GTK_EDITABLE(file_entry), FALSE);
	gtk_entry_set_activates_default(GTK_ENTRY(file_entry), TRUE);

	// the ebox is for the tooltip, because gtk_combo_box doesn't show a tooltip for unknown reason
	ebox = gtk_event_box_new();
	hbox = gtk_hbox_new(FALSE, 6);
	filetype_label = gtk_label_new(_("Set filetype:"));
	filetype_combo = gtk_combo_box_new_text();
	gtk_tooltips_set_tip(tooltips, ebox,
		_("Explicitly defines a filetype for the file, if it would not be detected by filename extension.\nNote if you choose multiple files, they will all be opened with the chosen filetype."), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), filetype_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), filetype_combo, FALSE, FALSE, 0);
	gtk_size_group_add_widget(GTK_SIZE_GROUP(sizegroup), filetype_combo);
	gtk_container_add(GTK_CONTAINER(ebox), hbox);
	gtk_box_pack_start(GTK_BOX(lbox), ebox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), lbox, FALSE, FALSE, 0);
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

	return vbox;
}


void dialogs_show_file_properties(gint idx)
{
	GtkWidget *dialog, *label, *table, *hbox, *image, *perm_table, *check;
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
		dialogs_show_error(_("An error occurred or file information could not be retrieved(e.g. from a new file)."));
		return;
	}


#if defined(HAVE_SYS_STAT_H) && defined(TIME_WITH_SYS_TIME) && defined(HAVE_SYS_TYPES_H)
	locale_filename = g_locale_from_utf8(doc_list[idx].file_name, -1, NULL, NULL, NULL);
	if (locale_filename == NULL) locale_filename = g_strdup(doc_list[idx].file_name);
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
										 GTK_STOCK_CLOSE, GTK_RESPONSE_NONE, NULL);
	g_free(title);

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
	g_signal_connect(dialog, "delete_event", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);

	title = g_strdup_printf("<b>%s\n</b>\n", base_name);
	label = gtk_label_new(title);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 1.0);
	image = gtk_image_new_from_stock("gtk-file", GTK_ICON_SIZE_BUTTON);
	gtk_misc_set_alignment(GTK_MISC(image), 1.0, 0.0);
	hbox = gtk_hbox_new(FALSE, 10);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_container_add(GTK_CONTAINER(hbox), image);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
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
	(utils_is_unicode_charset(doc_list[idx].encoding)) ? ((doc_list[idx].has_bom) ? _("(with BOM)") : _("(without BOM)")) : "");

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
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));

	// create permission label and then table with the permissions
	label = gtk_label_new("<b>Permissions:</b>");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

	perm_table = gtk_table_new(4, 4, TRUE);
	gtk_table_set_row_spacings(GTK_TABLE(perm_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(perm_table), 5);

	// Header
	label = gtk_label_new(_("Read:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	label = gtk_label_new(_("Write:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 2, 3, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	label = gtk_label_new(_("Execute:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 3, 4, 0, 1,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0);

	// Owner
	label = gtk_label_new(_("Owner:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 1, 2,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);


	// Group
	label = gtk_label_new(_("Group:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 2, 3,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);


	// Other
	label = gtk_label_new(_("Other:"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IROTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWOTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_widget_set_sensitive(check, FALSE);
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXOTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 3, 4,
					(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), perm_table);

	g_free(base_name);
	g_free(time_changed);
	g_free(time_modified);
	g_free(time_accessed);

	gtk_widget_show_all(dialog);
}


gboolean dialogs_show_question(const gchar *text, ...)
{
#ifndef G_OS_WIN32
	GtkWidget *dialog;
#endif
	gchar *string = g_malloc(512);
	gboolean ret = FALSE;
	va_list args;

	va_start(args, text);
	g_vsnprintf(string, 511, text, args);
	va_end(args);

#ifdef G_OS_WIN32
	ret = win32_message_dialog(GTK_MESSAGE_QUESTION, string);
#else
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", string);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES)
		ret = TRUE;
	gtk_widget_destroy(dialog);
#endif
	g_free(string);

	return ret;
}


void dialogs_show_keyboard_shortcuts(void)
{
	GtkWidget *dialog, *hbox, *label1, *label2, *label3, *swin;
	GString *text_names = g_string_sized_new(600);
	GString *text_keys = g_string_sized_new(600);
	gchar *shortcut;
	guint i;
	gint height;

	dialog = gtk_dialog_new_with_buttons(_("Keyboard shortcuts"), GTK_WINDOW(app->window),
						GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

	height = GEANY_WINDOW_MINIMAL_HEIGHT;
	gtk_window_set_default_size(GTK_WINDOW(dialog), height * 0.8, height);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	label3 = gtk_label_new(_("The following keyboard shortcuts are defined:"));
	gtk_misc_set_padding(GTK_MISC(label3), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0);

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

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label3, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), swin, TRUE, TRUE, 0);

	g_signal_connect((gpointer) dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show_all(dialog);

	g_string_free(text_names, TRUE);
	g_string_free(text_keys, TRUE);
}

