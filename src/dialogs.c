/*
 *      dialogs.c - this file is part of Geany, a fast and lightweight IDE
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


#include <gdk/gdkkeysyms.h>

#include "dialogs.h"

#include "callbacks.h"
#include "document.h"
#include "win32.h"
#include "about.h"


/* This shows the file selection dialog to open a file. */
void dialogs_show_open_file ()
{
	if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) < GEANY_MAX_OPEN_FILES)
	{
#ifdef GEANY_WIN32
		win32_show_file_dialog(TRUE);
#else /* X11, not win32: use GTK_FILE_CHOOSER */

		/* We use the same file selection widget each time, so first
	   		of all we create it if it hasn't already been created. */
		if (app->open_filesel == NULL)
		{
			gint i;
			app->open_filesel = create_fileopendialog1();
			/* Make sure the dialog doesn't disappear behind the main window. */
			gtk_window_set_transient_for (GTK_WINDOW(app->open_filesel), GTK_WINDOW (app->window));

			// add FileFilters(start with "All Files")
			gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->open_filesel),
						utils_create_file_filter(filetypes[GEANY_FILETYPES_ALL]));
			for (i = 0; i < GEANY_MAX_FILE_TYPES - 1; i++)
			{
				if (filetypes[i])
				{
					gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(app->open_filesel),
						utils_create_file_filter(filetypes[i]));
				}
			}
			// add checkboxes and filename entry
			gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(app->open_filesel),
						dialogs_add_file_open_extra_widget());
			g_signal_connect((gpointer)app->open_filesel, "selection-changed",
						G_CALLBACK(on_file_open_selection_changed), NULL);
		}
		gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->open_filesel));
		/* We make sure the dialog is visible. */
		gtk_window_present(GTK_WINDOW(app->open_filesel));
#endif
	}
	else
	{
		dialogs_show_file_open_error();
	}
}


/* This shows the file selection dialog to save a file. */
void dialogs_show_save_as ()
{
#ifdef GEANY_WIN32
		win32_show_file_dialog(FALSE);
#else
	gint idx = document_get_cur_idx();

	if (app->save_filesel == NULL)
	{
		app->save_filesel = create_filesavedialog1();
		/* Make sure the dialog doesn't disappear behind the main window. */
		gtk_window_set_transient_for(GTK_WINDOW(app->save_filesel), GTK_WINDOW(app->window));
	}

	/* If the current document has a filename we use that as the default. */
	if (doc_list[idx].file_name)
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(app->save_filesel), doc_list[idx].file_name);
	else
	{
		gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->save_filesel));
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(app->save_filesel), GEANY_STRING_UNTITLED);
	}

	/* We make sure the dialog is visible. */
	gtk_window_present(GTK_WINDOW(app->save_filesel));
#endif
}



void dialogs_show_file_open_error(void)
{
	gchar *pri = g_strdup_printf(_("There is a limit of %d concurrent open tabs."), GEANY_MAX_OPEN_FILES);
	gchar *sec = _("error: too many open files");
#ifdef GEANY_WIN32
	MessageBox(NULL, pri, sec, MB_OK | MB_ICONERROR);
#else
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, sec);
	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), pri);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
#endif
	g_free(pri);
}


gboolean dialogs_show_not_found(const gchar *text)
{
	GtkWidget *dialog;
	gchar string[255];
	gint ret;

	snprintf(string, 255, _("The match \"%s\" was not found. Wrap search around the document?"), text);
#ifdef GEANY_WIN32
	dialog = NULL;
	ret = MessageBox(NULL, string, _("Question"), MB_YESNO | MB_ICONQUESTION);
	if (ret == IDYES) return TRUE;
	else return FALSE;
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, string);
	ret = gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
	if (ret == GTK_RESPONSE_YES) return TRUE;
	else return FALSE;
#endif
}


void dialogs_show_info(const gchar *text, ...)
{
#ifndef GEANY_WIN32
	GtkWidget *dialog;
#endif
	gchar *string = g_malloc(512);
	va_list args;

	va_start(args, text);
	g_vsnprintf(string, 511, text, args);
	va_end(args);

#ifdef GEANY_WIN32
	MessageBox(NULL, string, _("Information"), MB_OK | MB_ICONINFORMATION);
#else

	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_INFO, GTK_BUTTONS_OK, string);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
#endif
	g_free(string);
}


void dialogs_show_error(const gchar *text, ...)
{
#ifndef GEANY_WIN32
	GtkWidget *dialog;
#endif
	gchar *string = g_malloc(512);
	va_list args;

	va_start(args, text);
	g_vsnprintf(string, 511, text, args);
	va_end(args);

#ifdef GEANY_WIN32
	MessageBox(NULL, string, _("Error"), MB_OK | MB_ICONERROR);
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, string);
	gtk_dialog_run (GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
#endif
	g_free(string);
}


gboolean dialogs_show_unsaved_file(gint idx)
{
#ifndef GEANY_WIN32
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
#ifdef GEANY_WIN32
	ret = MessageBox(NULL, msg, _("Error"), MB_YESNOCANCEL | MB_ICONQUESTION);
	switch(ret)
	{
		case IDYES: ret = GTK_RESPONSE_YES; break;
		case IDNO: ret = GTK_RESPONSE_NO; break;
		case IDCANCEL: ret = GTK_RESPONSE_CANCEL; break;
	}
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, msg);

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
				document_save_file(idx);
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
#ifdef GEANY_WIN32
	win32_show_font_dialog();
#else /* X11, not win32: use GTK_FILE_CHOOSER */

	if (app->open_fontsel == NULL)
	{
		app->open_fontsel = create_fontselectiondialog1();
		/* Make sure the dialog doesn't disappear behind the main window. */
		gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(app->open_fontsel), app->editor_font);
		gtk_window_set_transient_for(GTK_WINDOW (app->open_fontsel), GTK_WINDOW(app->window));
	}
	/* We make sure the dialog is visible. */
	gtk_window_present(GTK_WINDOW(app->open_fontsel));
#endif
}


void dialogs_show_about(void)
{
	AboutInfo *info;
	GtkWidget *dialog;
	GdkPixbuf *icon;
	gchar *s;
	guint n;

	static const struct { gchar *name, *email, *language; } translators[] =
	{
		//{ "Dwayne Bailey", "dwayne@translate.org.za", "en_GB" },
		{ NULL, NULL, NULL },
	};

	icon = utils_new_pixbuf_from_inline(GEANY_IMAGE_LOGO, FALSE);

	info = about_info_new(PACKAGE, VERSION, _("A fast and lightweight IDE"),
						  ABOUT_COPYRIGHT_TEXT("2005", "Enrico Troeger"), GEANY_HOMEPAGE, GEANY_CODENAME);
	about_info_add_credit(info, "Enrico Troeger", "enrico.troeger@uvena.de", _("Maintainer"));

	for (n = 0; translators[n].name != NULL; ++n)
	{
		s = g_strdup_printf(_("Translator (%s)"), translators[n].language);
		about_info_add_credit(info, translators[n].name, translators[n].email, s);
		g_free(s);
	}

	dialog = about_dialog_new(GTK_WINDOW(app->window), info, icon);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	about_info_free(info);
}


gboolean dialogs_show_reload_warning(const gchar *text)
{
#ifdef GEANY_WIN32
	if (MessageBox(NULL, text, _("Question"), MB_YESNO | MB_ICONWARNING) == IDYES) return TRUE;
	else return FALSE;
#else
	gboolean ret = FALSE;
	GtkWidget *dlg;

	dlg = gtk_message_dialog_new(GTK_WINDOW(app->window),
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_WARNING,
										  GTK_BUTTONS_YES_NO, text);
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES)
	{
		ret = TRUE;
	}
	gtk_widget_destroy(dlg);

	return ret;
#endif
}


gboolean dialogs_show_confirm_exit(void)
{
	gchar *text = _("Do you really want to quit?");

#ifdef GEANY_WIN32
	if (MessageBox(NULL, text, _("Question"), MB_YESNO | MB_ICONWARNING) == IDYES) return TRUE;
	else return FALSE;
#else
	gboolean ret = FALSE;
	GtkWidget *dlg;

	dlg = gtk_message_dialog_new(GTK_WINDOW(app->window),
										  GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_QUESTION,
										  GTK_BUTTONS_YES_NO, text);
	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_YES)
	{
		ret = TRUE;
	}
	gtk_widget_destroy(dlg);

	return ret;
#endif
}


void dialogs_show_word_count(void)
{
	GtkWidget *dialog, *label;
	gint idx;
	guint chars, lines, words;
	gchar *string, *text, *range;
	GtkRequisition *size = g_new(GtkRequisition, 1);

	idx = document_get_cur_idx();
	if (idx == -1) return;

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

	g_signal_connect_swapped(dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	gtk_widget_size_request(label, size);
	gtk_widget_set_size_request(dialog, size->width + 20, size->height + 50);

	g_free(size);
	gtk_widget_show_all(dialog);
}


/* This shows the color selection dialog to choose a color. */
void dialogs_show_color(void)
{
#ifdef GEANY_WIN32
	win32_show_color_dialog();
#else /* X11, not win32: use GTK_COLOR_CHOOSER */

	if (app->open_colorsel == NULL)
	{
		app->open_colorsel = gtk_color_selection_dialog_new(_("Color Chooser"));
		gtk_window_set_transient_for(GTK_WINDOW(app->open_colorsel), GTK_WINDOW(app->window));

		g_signal_connect(GTK_COLOR_SELECTION_DIALOG(app->open_colorsel)->cancel_button, "clicked",
						G_CALLBACK(on_color_cancel_button_clicked), NULL);
		g_signal_connect(GTK_COLOR_SELECTION_DIALOG(app->open_colorsel)->ok_button, "clicked",
						G_CALLBACK(on_color_ok_button_clicked), NULL);
		g_signal_connect(app->open_colorsel, "delete_event",
						G_CALLBACK(on_color_delete_event), NULL);
	}
	/* We make sure the dialog is visible. */
	gtk_window_present (GTK_WINDOW(app->open_colorsel));
#endif
}


void dialogs_create_build_menu(void)
{
	GtkWidget *menu, *item, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(lookup_widget(app->window, "menu_build1")), menu);

	// add tearoff
	//item = gtk_tearoff_menu_item_new();
	//gtk_widget_show(item);
	//gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);

	// compile the code
	item = gtk_image_menu_item_new_with_label(_("Compile"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file"), NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F8, (GdkModifierType) 0, GTK_ACCEL_VISIBLE);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK (on_build_compile_activate), NULL);


	// build the code
	app->build_menu_item_link = gtk_menu_item_new_with_label(_("Build"));
	gtk_widget_show(app->build_menu_item_link);
	gtk_container_add(GTK_CONTAINER(menu), app->build_menu_item_link);
	gtk_tooltips_set_tip(tooltips, app->build_menu_item_link,
				_("Builds the current file (generate an executable file)"), NULL);
	gtk_widget_add_accelerator(app->build_menu_item_link, "activate", accel_group, GDK_F9,
				(GdkModifierType) 0, GTK_ACCEL_VISIBLE);
	g_signal_connect((gpointer) app->build_menu_item_link, "activate", G_CALLBACK (on_build_build_activate), NULL);


	// build the code with make all
	item = gtk_menu_item_new_with_label(_("Build with \"make\""));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F9,
				(GdkModifierType) GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK (on_build_make_activate), GINT_TO_POINTER(0));


	// build the code with make
	item = gtk_menu_item_new_with_label(_("Build with make (custom target)"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK (on_build_make_activate), GINT_TO_POINTER(1));


	// execute the code
	item = gtk_image_menu_item_new_from_stock("gtk-execute", accel_group);
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Execute the current file if it was already compiled"), NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F5,
				(GdkModifierType) 0, GTK_ACCEL_VISIBLE);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK (on_build_execute_activate), NULL);


	// separator
	separator = gtk_separator_menu_item_new();
	gtk_widget_show(separator);
	gtk_container_add(GTK_CONTAINER(menu), separator);
	gtk_widget_set_sensitive(separator, FALSE);


	// arguments
	item = gtk_image_menu_item_new_with_label(_("Set Includes and Arguments"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER (menu), item);
	gtk_tooltips_set_tip(tooltips, item,
				_("Sets the includes and library paths for the compiler and "
				  "the program arguments for execution"), NULL);
	image = gtk_image_new_from_stock("gtk-preferences", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK (on_build_arguments_activate), NULL);


	gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);
}


void dialogs_create_recent_menu(void)
{
	GtkWidget *recent_menu = lookup_widget(app->window, "recent_files1_menu");
	GtkWidget *tmp;
	gint i;
	gchar *filename;

	for (i = (MIN(GEANY_RECENT_MRU_LENGTH, g_queue_get_length(app->recent_queue)) - 1); i >= 0; i--)
	{
		filename = g_queue_peek_nth(app->recent_queue, i);
		tmp = gtk_menu_item_new_with_label(filename);
		gtk_widget_show(tmp);
		gtk_container_add(GTK_CONTAINER(recent_menu), tmp);
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

	g_signal_connect((gpointer) entry, "activate", G_CALLBACK (on_make_target_entry_activate), dialog);
	g_signal_connect((gpointer) dialog, "response", G_CALLBACK (on_make_target_dialog_response), entry);

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


void dialogs_show_find(void)
{
	if (app->find_dialog == NULL)
	{
		GtkWidget *label, *entry, *checkbox1, *checkbox2, *checkbox3, *checkbox4, *checkbox5;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		app->find_dialog = gtk_dialog_new_with_buttons(_("Find"), GTK_WINDOW(app->window),
						GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

		gtk_dialog_add_button(GTK_DIALOG(app->find_dialog), "gtk-find", GTK_RESPONSE_ACCEPT);

		label = gtk_label_new(_("Enter the search text here"));
		gtk_misc_set_padding(GTK_MISC(label), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

		entry = gtk_combo_box_entry_new_text();
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 50);
		g_object_set_data_full(G_OBJECT(app->find_dialog), "entry",
						gtk_widget_ref(entry), (GDestroyNotify)gtk_widget_unref);

		g_signal_connect((gpointer) gtk_bin_get_child(GTK_BIN(entry)), "activate",
				G_CALLBACK(on_find_entry_activate), NULL);
		g_signal_connect((gpointer) app->find_dialog, "response",
				G_CALLBACK(on_find_dialog_response), entry);

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), label);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), entry);

		checkbox1 = gtk_check_button_new_with_mnemonic(_("_Case sensitive"));
		g_object_set_data_full(G_OBJECT(app->find_dialog), "check_case",
						gtk_widget_ref(checkbox1), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);

		checkbox2 = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
		g_object_set_data_full(G_OBJECT(app->find_dialog), "check_word",
						gtk_widget_ref(checkbox2), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);

		checkbox3 = gtk_check_button_new_with_mnemonic(_("_Use regular expressions"));
		g_object_set_data_full(G_OBJECT(app->find_dialog), "check_regexp",
						gtk_widget_ref(checkbox3), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox3), FALSE);
		gtk_tooltips_set_tip(tooltips, checkbox3,
			_("For detailed information about using regular expressions, please read the documentation."), NULL);

		checkbox4 = gtk_check_button_new_with_mnemonic(_("_Search backwards"));
		g_object_set_data_full(G_OBJECT(app->find_dialog), "check_back",
						gtk_widget_ref(checkbox4), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox4), FALSE);

		checkbox5 = gtk_check_button_new_with_mnemonic(_("Match only word _start"));
		g_object_set_data_full(G_OBJECT(app->find_dialog), "check_wordstart",
						gtk_widget_ref(checkbox5), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox5), FALSE);

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), checkbox1);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), checkbox2);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), checkbox5);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), checkbox3);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), checkbox4);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->find_dialog)->vbox), gtk_label_new(""));

		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(app->find_dialog)->vbox), 3);

		gtk_widget_show_all(app->find_dialog);
	}
	else
	{
		gtk_widget_show(app->find_dialog);
	}
}


void dialogs_show_replace(void)
{
	if (app->replace_dialog == NULL)
	{
		GtkWidget *label_find, *label_replace, *entry_find, *entry_replace, *checkbox1, *checkbox2, *checkbox3, *checkbox5, *checkbox4;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		app->replace_dialog = gtk_dialog_new_with_buttons(_("Replace"), GTK_WINDOW(app->window),
						GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

		gtk_dialog_add_button(GTK_DIALOG(app->replace_dialog), _("Replace"), GEANY_RESPONSE_REPLACE);
		gtk_dialog_add_button(GTK_DIALOG(app->replace_dialog), _("Replace All"), GEANY_RESPONSE_REPLACE_ALL);
		gtk_dialog_add_button(GTK_DIALOG(app->replace_dialog), _("Replace in selection"), GEANY_RESPONSE_REPLACE_SEL);

		label_find = gtk_label_new(_("Enter the search text here"));
		gtk_misc_set_padding(GTK_MISC(label_find), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label_find), 0, 0);

		label_replace = gtk_label_new(_("Enter the replace text here"));
		gtk_misc_set_padding(GTK_MISC(label_replace), 0, 6);
		gtk_misc_set_alignment(GTK_MISC(label_replace), 0, 0);

		entry_find = gtk_combo_box_entry_new_text();
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 50);
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

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), label_find);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), entry_find);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), label_replace);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), entry_replace);

		checkbox1 = gtk_check_button_new_with_mnemonic(_("_Case sensitive"));
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "check_case",
						gtk_widget_ref(checkbox1), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);

		checkbox2 = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "check_word",
						gtk_widget_ref(checkbox2), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);

		checkbox3 = gtk_check_button_new_with_mnemonic(_("_Use regular expressions"));
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "check_regexp",
						gtk_widget_ref(checkbox3), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox3), FALSE);
		gtk_tooltips_set_tip(tooltips, checkbox3,
			_("For detailed information about using regular expressions, please read the documentation."), NULL);

		checkbox4 = gtk_check_button_new_with_mnemonic(_("_Search backwards"));
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "check_back",
						gtk_widget_ref(checkbox4), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox4), FALSE);

		checkbox5 = gtk_check_button_new_with_mnemonic(_("Match only word _start"));
		g_object_set_data_full(G_OBJECT(app->replace_dialog), "check_wordstart",
						gtk_widget_ref(checkbox5), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox5), FALSE);

		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), checkbox1);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), checkbox2);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), checkbox5);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), checkbox3);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), checkbox4);
		gtk_container_add(GTK_CONTAINER(GTK_DIALOG(app->replace_dialog)->vbox), gtk_label_new(""));

		gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(app->replace_dialog)->vbox), 3);

		gtk_widget_show_all(app->replace_dialog);
	}
	else
	{
		gtk_widget_show(app->replace_dialog);
	}
}


void dialogs_show_includes_arguments(void)
{
	GtkWidget *dialog, *label, *entries[3];

	dialog = gtk_dialog_new_with_buttons(_("Set Includes and Arguments"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	label = gtk_label_new(_("Sets the includes and library paths for the compiler and the program arguments for execution"));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

	// include-args
	label = gtk_label_new(_("Enter here arguments to your compiler."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	entries[0] = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
	if (app->build_args_inc)
	{
		gtk_entry_set_text(GTK_ENTRY(entries[0]), app->build_args_inc);
	}
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[0]);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));

	// lib-args
	label = gtk_label_new(_("Enter here arguments to your linker."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	entries[1] = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
	if (app->build_args_libs)
	{
		gtk_entry_set_text(GTK_ENTRY(entries[1]), app->build_args_libs);
	}
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[1]);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));

	// program-args
	label = gtk_label_new(_("Enter here arguments to your program."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	entries[2] = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
	if (app->build_args_prog)
	{
		gtk_entry_set_text(GTK_ENTRY(entries[2]), app->build_args_prog);
	}
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[2]);


	g_object_set_data_full(G_OBJECT(dialog), "includes_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(dialog), "includes_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(dialog), "includes_entry3",
					gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);


	g_signal_connect((gpointer) dialog, "response", G_CALLBACK(on_includes_arguments_dialog_response), NULL);

	gtk_widget_show_all(dialog);
}


GtkWidget *dialogs_add_file_open_extra_widget(void)
{
	GtkWidget *vbox;
	GtkWidget *file_entry;
	GtkWidget *check_readonly;
	GtkWidget *check_hidden;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	vbox = gtk_vbox_new(FALSE, 4);

	file_entry = gtk_entry_new();
	gtk_widget_show(file_entry);
	gtk_box_pack_start(GTK_BOX(vbox), file_entry, FALSE, FALSE, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(file_entry), TRUE);

	check_readonly = gtk_check_button_new_with_mnemonic(_("Open file read-only"));
	gtk_widget_show(check_readonly);
	gtk_box_pack_start(GTK_BOX(vbox), check_readonly, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, check_readonly,
		_("Opens the file in read-only mode. If you choose more than one file to open, all files will opened read-only."), NULL);
	gtk_button_set_focus_on_click(GTK_BUTTON(check_readonly), FALSE);

	check_hidden = gtk_check_button_new_with_mnemonic(_("Show hidden files"));
	gtk_widget_show(check_hidden);
	gtk_box_pack_start(GTK_BOX(vbox), check_hidden, FALSE, FALSE, 0);
	gtk_button_set_focus_on_click(GTK_BUTTON(check_hidden), FALSE);

	g_signal_connect((gpointer) file_entry, "activate",
				G_CALLBACK(on_file_open_entry_activate), NULL);
	g_signal_connect((gpointer) check_hidden, "toggled",
				G_CALLBACK(on_file_open_check_hidden_toggled), NULL);

	g_object_set_data_full(G_OBJECT(app->open_filesel), "file_entry",
				gtk_widget_ref(file_entry), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(app->open_filesel), "check_readonly",
				gtk_widget_ref(check_readonly), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(app->open_filesel), "check_hidden",
				gtk_widget_ref(check_hidden), (GDestroyNotify)gtk_widget_unref);

	return vbox;
}


gboolean dialogs_show_mkcfgdir_error(gint error_nr)
{
	GtkWidget *dialog;
	gchar string[255];
	gint ret;

	snprintf(string, 255, _("Configuration directory could not be created (%s).\nThere could be some problems using %s without a configuration directory.\nStart %s anyway?"),
									g_strerror(error_nr), PACKAGE, PACKAGE);
#ifdef GEANY_WIN32
	dialog = NULL;
	ret = MessageBox(NULL, string, _("Error"), MB_YESNO | MB_ICONERROR);
	if (ret == IDYES) return TRUE;
	else return FALSE;
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR, GTK_BUTTONS_YES_NO, string);
	ret = gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
	if (ret == GTK_RESPONSE_YES) return TRUE;
	else return FALSE;
#endif
}

