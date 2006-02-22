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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#include "about.h"
#include "sciwrappers.h"
#include "support.h"
#include "interface.h"
#include "utils.h"



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
			GtkWidget *combo;
			gint i;

			app->open_filesel = gtk_file_chooser_dialog_new(_("Open File"), NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

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
				gtk_file_chooser_set_current_folder(
					GTK_FILE_CHOOSER(app->open_filesel), initdir);
				g_free(initdir);
			}
		}

		gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(app->open_filesel));
		gtk_widget_show(app->open_filesel);
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
		app->save_filesel = gtk_file_chooser_dialog_new(_("Save File"), NULL,
					GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
		gtk_window_set_modal(GTK_WINDOW(app->save_filesel), TRUE);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(app->save_filesel), TRUE);
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(app->save_filesel), TRUE);
		gtk_window_set_type_hint(GTK_WINDOW(app->save_filesel), GDK_WINDOW_TYPE_HINT_DIALOG);

		g_signal_connect((gpointer) app->save_filesel, "delete_event", G_CALLBACK(gtk_widget_hide), NULL);
		g_signal_connect((gpointer) app->save_filesel, "response", G_CALLBACK(on_file_save_dialog_response), NULL);

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
	gchar *string;
	gint ret;

	string = g_strdup_printf(_("The match \"%s\" was not found. Wrap search around the document?"), text);
#ifdef GEANY_WIN32
	dialog = NULL;
	ret = MessageBox(NULL, string, _("Question"), MB_YESNO | MB_ICONQUESTION);
	g_free(string);
	if (ret == IDYES) return TRUE;
	else return FALSE;
#else
	dialog = gtk_message_dialog_new(GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", string);
	g_free(string);
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


gboolean dialogs_show_fifo_error(const gchar *text, ...)
{
	GtkWidget *dialog;
	gchar string[300];
	gint ret;
	va_list args;

	va_start(args, text);
	g_vsnprintf(string, 299, text, args);
	va_end(args);

	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
										  GTK_MESSAGE_ERROR, GTK_BUTTONS_YES_NO, string);
	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	
	if (ret == GTK_RESPONSE_YES) return TRUE;
	else return FALSE;
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
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, string);
	gtk_dialog_run(GTK_DIALOG(dialog));
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
						  ABOUT_COPYRIGHT_TEXT("2006", "Enrico Troeger"), GEANY_HOMEPAGE, GEANY_CODENAME);
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
						G_CALLBACK(gtk_widget_hide), NULL);
	}
	/* We make sure the dialog is visible. */
	gtk_window_present (GTK_WINDOW(app->open_colorsel));
#endif
}


GtkWidget *dialogs_create_build_menu_gen(gboolean link, gboolean execute)
{
	GtkWidget *menu, *item, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	menu = gtk_menu_new();

	// compile the code
	item = gtk_image_menu_item_new_with_label(_("Compile"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file"), NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F8, (GdkModifierType) 0, GTK_ACCEL_VISIBLE);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_compile_activate), NULL);

	if (link)
	{	// build the code
		app->build_menu_item_link = gtk_menu_item_new_with_label(_("Build"));
		gtk_widget_show(app->build_menu_item_link);
		gtk_container_add(GTK_CONTAINER(menu), app->build_menu_item_link);
		gtk_tooltips_set_tip(tooltips, app->build_menu_item_link,
					_("Builds the current file (generate an executable file)"), NULL);
		gtk_widget_add_accelerator(app->build_menu_item_link, "activate", accel_group, GDK_F9,
					(GdkModifierType) 0, GTK_ACCEL_VISIBLE);
		g_signal_connect((gpointer) app->build_menu_item_link, "activate", G_CALLBACK(on_build_build_activate), NULL);
	}

	// build the code with make all
	item = gtk_menu_item_new_with_label(_("Build with \"make\""));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F9,
				(GdkModifierType) GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(0));

	// build the code with make
	item = gtk_menu_item_new_with_label(_("Build with make (custom target)"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(1));

	if (execute)
	{	// execute the code
		item = gtk_image_menu_item_new_from_stock("gtk-execute", accel_group);
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		gtk_tooltips_set_tip(tooltips, item, _("Execute the current file if it was already compiled"), NULL);
		gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F5,
					(GdkModifierType) 0, GTK_ACCEL_VISIBLE);
		g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_execute_activate), NULL);
	}

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
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_arguments_activate), GINT_TO_POINTER(link));

	gtk_window_add_accel_group(GTK_WINDOW(app->window), accel_group);

	return menu;
}


GtkWidget *dialogs_create_build_menu_tex(void)
{
	GtkWidget *menu, *item, *image, *separator;
	GtkAccelGroup *accel_group = gtk_accel_group_new();
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	menu = gtk_menu_new();

	// DVI
	item = gtk_image_menu_item_new_with_label(_("LaTeX -> DVI"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a DVI file"), NULL);
//	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F8, (GdkModifierType) 0, GTK_ACCEL_VISIBLE);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(0));

	// PDF
	item = gtk_image_menu_item_new_with_label(_("LaTeX -> PDF"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles the current file into a PDF file"), NULL);
//	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F9, (GdkModifierType) 0, GTK_ACCEL_VISIBLE);
	image = gtk_image_new_from_stock("gtk-convert", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(1));

	// build the code with make all
	item = gtk_menu_item_new_with_label(_("Build with \"make\""));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the default target"), NULL);
/*	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F9,
				(GdkModifierType) GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
*/	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(0));

	// build the code with make
	item = gtk_menu_item_new_with_label(_("Build with make (custom target)"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_tooltips_set_tip(tooltips, item, _("Builds the current file with the "
										   "make tool and the specified target"), NULL);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_make_activate), GINT_TO_POINTER(1));

	// DVI view
	item = gtk_image_menu_item_new_with_label(_("View DVI file"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
//	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F5, (GdkModifierType) 0, GTK_ACCEL_VISIBLE);
	gtk_tooltips_set_tip(tooltips, item, _("Compiles and view the current file"), NULL);
	image = gtk_image_new_from_stock("gtk-find", GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_build_tex_activate), GINT_TO_POINTER(2));

	// PDF view
	item = gtk_image_menu_item_new_with_label(_("View PDF file"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER(menu), item);
/*	gtk_widget_add_accelerator(item, "activate", accel_group, GDK_F5,
				(GdkModifierType) GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
*/	gtk_tooltips_set_tip(tooltips, item, _("Compiles and view the current file"), NULL);
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
	item = gtk_image_menu_item_new_with_label(_("Set Arguments"));
	gtk_widget_show(item);
	gtk_container_add(GTK_CONTAINER (menu), item);
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
	gint i;
	gchar *filename;

	for (i = (MIN(app->mru_length, g_queue_get_length(app->recent_queue)) - 1); i >= 0; i--)
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
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	if (sci_can_copy(doc_list[idx].sci))
	{
		sel = g_malloc(sci_get_selected_text_length(doc_list[idx].sci));
		sci_get_selected_text(doc_list[idx].sci, sel);
	}

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
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(entry)->child), sel);
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

	if (sci_can_copy(doc_list[idx].sci))
	{
		sel = g_malloc(sci_get_selected_text_length(doc_list[idx].sci));
		sci_get_selected_text(doc_list[idx].sci, sel);
	}

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
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(
							lookup_widget(app->replace_dialog, "entry_find"))->child), sel);
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(app->replace_dialog, "entry_find"))->child));
		gtk_widget_show(app->replace_dialog);
	}
	g_free(sel);
}

void dialogs_show_includes_arguments_tex(void)
{
	GtkWidget *dialog, *label, *entries[3];

	dialog = gtk_dialog_new_with_buttons(_("Set Arguments"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	label = gtk_label_new(_("Set programs and options for compilation and viewing (La)TeX files.\nThe filename is appended automatically at the end.\n"));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);

	// LaTeX -> DVI args
	label = gtk_label_new(_("Enter here the (La)TeX command (for DVI creation) and some useful options."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	entries[0] = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entries[0]), 30);
	if (app->build_tex_dvi_cmd)
		gtk_entry_set_text(GTK_ENTRY(entries[0]), app->build_tex_dvi_cmd);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[0]);

	// whitespace
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));

	// LaTeX -> PDF args
	label = gtk_label_new(_("Enter here the (La)TeX command (for PDF creation) and some useful options."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	entries[1] = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entries[1]), 30);
	if (app->build_tex_pdf_cmd)
		gtk_entry_set_text(GTK_ENTRY(entries[1]), app->build_tex_pdf_cmd);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[1]);

	// whitespace
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));

	// View LaTeX -> DVI args
	label = gtk_label_new(_("Enter here the (La)TeX command (for DVI preview) and some useful options."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	entries[2] = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entries[2]), 30);
	if (app->build_tex_view_dvi_cmd)
		gtk_entry_set_text(GTK_ENTRY(entries[2]), app->build_tex_view_dvi_cmd);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[2]);

	// whitespace
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));

	// View LaTeX -> PDF args
	label = gtk_label_new(_("Enter here the (La)TeX command (for PDF preview) and some useful options."));
	gtk_misc_set_padding(GTK_MISC(label), 0, 6);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	entries[3] = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entries[3]), 30);
	if (app->build_tex_view_pdf_cmd)
		gtk_entry_set_text(GTK_ENTRY(entries[3]), app->build_tex_view_pdf_cmd);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), entries[3]);

	// whitespace
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));

	g_object_set_data_full(G_OBJECT(dialog), "tex_entry1",
					gtk_widget_ref(entries[0]), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(dialog), "tex_entry2",
					gtk_widget_ref(entries[1]), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(dialog), "tex_entry3",
					gtk_widget_ref(entries[2]), (GDestroyNotify)gtk_widget_unref);
	g_object_set_data_full(G_OBJECT(dialog), "tex_entry4",
					gtk_widget_ref(entries[3]), (GDestroyNotify)gtk_widget_unref);


	g_signal_connect((gpointer) dialog, "response", G_CALLBACK(on_includes_arguments_tex_dialog_response), NULL);

	gtk_widget_show_all(dialog);
}


void dialogs_show_includes_arguments_gen(gboolean link)
{
	GtkWidget *dialog, *label, *entries[3];

	dialog = gtk_dialog_new_with_buttons(_("Set Includes and Arguments"), GTK_WINDOW(app->window),
										GTK_DIALOG_DESTROY_WITH_PARENT,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

	label = gtk_label_new(_("Sets the includes and library paths for the compiler and the program arguments for execution\n"));
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

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), gtk_label_new(""));


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
	GtkWidget *ebox;
	GtkWidget *hbox;
	GtkWidget *file_entry;
	GtkWidget *check_readonly;
	GtkWidget *check_hidden;
	GtkWidget *filetype_label;
	GtkWidget *filetype_combo;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	vbox = gtk_vbox_new(FALSE, 4);

	file_entry = gtk_entry_new();
	gtk_widget_show(file_entry);
	gtk_box_pack_start(GTK_BOX(vbox), file_entry, FALSE, FALSE, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(file_entry), TRUE);

	check_readonly = gtk_check_button_new_with_mnemonic(_("Open file _read-only"));
	gtk_widget_show(check_readonly);
	gtk_box_pack_start(GTK_BOX(vbox), check_readonly, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, check_readonly,
		_("Opens the file in read-only mode. If you choose more than one file to open, all files will opened read-only."), NULL);
	gtk_button_set_focus_on_click(GTK_BUTTON(check_readonly), FALSE);

	check_hidden = gtk_check_button_new_with_mnemonic(_("Show hidden files"));
	gtk_widget_show(check_hidden);
	gtk_box_pack_start(GTK_BOX(vbox), check_hidden, FALSE, FALSE, 0);
	gtk_button_set_focus_on_click(GTK_BUTTON(check_hidden), FALSE);

	// the ebox is for the tooltip, because gtk_combo_box doesn't show a tooltip for unknown reason
	ebox = gtk_event_box_new();
	hbox = gtk_hbox_new(FALSE, 4);
	filetype_label = gtk_label_new(_("Set filetype: "));
	filetype_combo = gtk_combo_box_new_text();
	gtk_tooltips_set_tip(tooltips, ebox,
		_("Explicitly defines a filetype for the file, if it would not detected by filename extension.\nNote if you choose multiple files, they will all opened with the chossed filetype."), NULL);
	gtk_box_pack_start(GTK_BOX(hbox), filetype_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), filetype_combo, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(ebox), hbox);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox);

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
	g_object_set_data_full(G_OBJECT(app->open_filesel), "filetype_combo",
				gtk_widget_ref(filetype_combo), (GDestroyNotify)gtk_widget_unref);

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
	dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_ERROR, GTK_BUTTONS_YES_NO, string);
	ret = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	if (ret == GTK_RESPONSE_YES) return TRUE;
	else return FALSE;
#endif
}


void dialogs_show_file_properties(gint idx)
{
	GtkWidget *dialog, *label, *table, *hbox, *image, *perm_table, *check;
	gchar *file_size, *title, *base_name, *locale_filename, *time_changed, *time_modified, *time_accessed;
	GtkRequisition *size;
#if defined(HAVE_SYS_STAT_H) && defined(HAVE_SYS_TYPES_H)
	struct stat st;
	off_t filesize;	
	mode_t mode;
#else
	gint filesize = 0;
	gint mode = 0;
#endif

	if (idx == -1 || ! doc_list[idx].is_valid) return;

#if defined(HAVE_SYS_STAT_H) && defined(TIME_WITH_SYS_TIME) && defined(HAVE_SYS_TYPES_H)
	locale_filename = g_locale_from_utf8(doc_list[idx].file_name, -1, NULL, NULL, NULL);
	if (locale_filename == NULL) locale_filename = g_strdup(doc_list[idx].file_name);
	if (stat(locale_filename, &st) == 0)
	{
		time_changed  = g_strdup(ctime(&st.st_ctime));
		time_modified = g_strdup(ctime(&st.st_mtime));
		time_accessed = g_strdup(ctime(&st.st_atime));
		filesize = st.st_size;
		mode = st.st_mode;
	}
	else 
	{
		time_changed  = g_strdup(_("unknown"));
		time_modified = g_strdup(_("unknown"));
		time_accessed = g_strdup(_("unknown"));
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

	g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);

	title = g_strdup_printf("<b>%s\n</b>", base_name);
	label = gtk_label_new(title);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	image = gtk_image_new_from_stock("gtk-file", GTK_ICON_SIZE_BUTTON);
	gtk_misc_set_alignment(GTK_MISC(image), 0.8, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_container_add(GTK_CONTAINER(hbox), image);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), hbox);
	g_free(title);
	/// add doc_list members
	table = gtk_table_new(6, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);

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
	//file_size = utils_make_human_readable_str(1048576/1024, 1, 0);
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

	label = gtk_label_new(_("<b>Modified:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(time_modified);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	label = gtk_label_new(_("<b>Changed:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(time_changed);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	label = gtk_label_new(_("<b>Accessed:</b>"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 5, 6,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0);

	label = gtk_label_new(time_accessed);
	gtk_table_attach(GTK_TABLE(table), label, 1, 2, 5, 6,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	// add table
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

	// create permission label and then table with the permissions
	label = gtk_label_new("<b>Permissions:</b>");
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), label);
	
	perm_table = gtk_table_new(4, 4, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(perm_table), 5);
	gtk_table_set_col_spacings(GTK_TABLE(perm_table), 5);

	// Header
	label = gtk_label_new(_("<b>Read:</b>"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	label = gtk_label_new(_("<b>Write:</b>"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 2, 3, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	label = gtk_label_new(_("<b>Execute:</b>"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 3, 4, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	// Owner
	label = gtk_label_new(_("<b>Owner:</b>"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXUSR);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);


	// Group
	label = gtk_label_new(_("<b>Group:</b>"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IRGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXGRP);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);


	// Other
	label = gtk_label_new(_("<b>Other:</b>"));
	gtk_table_attach(GTK_TABLE(perm_table), label, 0, 1, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IROTH); 
	gtk_table_attach(GTK_TABLE(perm_table), check, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IWOTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 2, 3, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);

	check = gtk_check_button_new();
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mode & S_IXOTH);
	gtk_table_attach(GTK_TABLE(perm_table), check, 3, 4, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_button_set_alignment(GTK_BUTTON(check), 0.5, 0);




	
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), perm_table);

	// resize the dialog
	/// FIXME do we really need this?
	size = g_new(GtkRequisition, 1);
	gtk_widget_size_request(GTK_DIALOG(dialog)->vbox, size);
	//gtk_widget_set_size_request(dialog, size->width + 20, size->height + 50);
	g_free(size);
	g_free(base_name);
	g_free(time_changed);
	g_free(time_modified);
	g_free(time_accessed);

	gtk_widget_show_all(dialog);
}
