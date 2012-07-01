/*
 *      log.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * Logging functions and the debug messages window.
 */

#include "geany.h"

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include "log.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"


static GString *log_buffer = NULL;
static GtkTextBuffer *dialog_textbuffer = NULL;

enum
{
	DIALOG_RESPONSE_CLEAR = 1
};


static void update_dialog(void)
{
	if (dialog_textbuffer != NULL)
	{
		GtkTextMark *mark;
		GtkTextView *textview = g_object_get_data(G_OBJECT(dialog_textbuffer), "textview");

		gtk_text_buffer_set_text(dialog_textbuffer, log_buffer->str, log_buffer->len);
		/* scroll to the end of the messages as this might be most interesting */
		mark = gtk_text_buffer_get_insert(dialog_textbuffer);
		gtk_text_view_scroll_to_mark(textview, mark, 0.0, FALSE, 0.0, 0.0);
	}
}


/* Geany's main debug/log function, declared in geany.h */
void geany_debug(gchar const *format, ...)
{
	va_list args;
	va_start(args, format);
	g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, format, args);
	va_end(args);
}


static void handler_print(const gchar *msg)
{
	printf("%s\n", msg);
	if (G_LIKELY(log_buffer != NULL))
	{
		g_string_append_printf(log_buffer, "%s\n", msg);
		update_dialog();
	}
}


static void handler_printerr(const gchar *msg)
{
	fprintf(stderr, "%s\n", msg);
	if (G_LIKELY(log_buffer != NULL))
	{
		g_string_append_printf(log_buffer, "%s\n", msg);
		update_dialog();
	}
}


static const gchar *get_log_prefix(GLogLevelFlags log_level)
{
	switch (log_level & G_LOG_LEVEL_MASK)
	{
		case G_LOG_LEVEL_ERROR:
			return "ERROR\t\t";
		case G_LOG_LEVEL_CRITICAL:
			return "CRITICAL\t";
		case G_LOG_LEVEL_WARNING:
			return "WARNING\t";
		case G_LOG_LEVEL_MESSAGE:
			return "MESSAGE\t";
		case G_LOG_LEVEL_INFO:
			return "INFO\t\t";
		case G_LOG_LEVEL_DEBUG:
			return "DEBUG\t";
	}
	return "LOG";
}


static void handler_log(const gchar *domain, GLogLevelFlags level, const gchar *msg, gpointer data)
{
	gchar *time_str;

	if (G_LIKELY(app != NULL && app->debug_mode) ||
		! ((G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO | G_LOG_LEVEL_MESSAGE) & level))
	{
#ifdef G_OS_WIN32
		/* On Windows g_log_default_handler() is not enough, we need to print it
		 * explicitly on stderr for the console window */
		/** TODO this can be removed if/when we remove the console window on Windows */
		if (domain != NULL)
			fprintf(stderr, "%s: %s\n", domain, msg);
		else
			fprintf(stderr, "%s\n", msg);
#else
		/* print the message as usual on stdout/stderr */
		g_log_default_handler(domain, level, msg, data);
#endif
	}

	time_str = utils_get_current_time_string();

	g_string_append_printf(log_buffer, "%s: %s %s: %s\n", time_str, domain,
		get_log_prefix(level), msg);

	g_free(time_str);

	update_dialog();
}


void log_handlers_init(void)
{
	log_buffer = g_string_sized_new(2048);

	g_set_print_handler(handler_print);
	g_set_printerr_handler(handler_printerr);
	g_log_set_default_handler(handler_log, NULL);
}


static void on_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == DIALOG_RESPONSE_CLEAR)
	{
		GtkTextIter start_iter, end_iter;

		gtk_text_buffer_get_start_iter(dialog_textbuffer, &start_iter);
		gtk_text_buffer_get_end_iter(dialog_textbuffer, &end_iter);
		gtk_text_buffer_delete(dialog_textbuffer, &start_iter, &end_iter);

		g_string_erase(log_buffer, 0, -1);
	}
	else
	{
		gtk_widget_destroy(GTK_WIDGET(dialog));
		dialog_textbuffer = NULL;
	}
}


void log_show_debug_messages_dialog(void)
{
	GtkWidget *dialog, *textview, *vbox, *swin;

	dialog = gtk_dialog_new_with_buttons(_("Debug Messages"), GTK_WINDOW(main_widgets.window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				_("Cl_ear"), DIALOG_RESPONSE_CLEAR,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_widget_set_name(dialog, "GeanyDialog");

	gtk_window_set_default_size(GTK_WINDOW(dialog), 550, 300);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	textview = gtk_text_view_new();
	dialog_textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	g_object_set_data(G_OBJECT(dialog_textbuffer), "textview", textview);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD_CHAR);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(swin), textview);

	gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

	g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), textview);
	gtk_widget_show_all(dialog);

	update_dialog(); /* set text after showing the window, to not scroll an unrealized textview */
}


void log_finalize(void)
{
	g_log_set_default_handler(g_log_default_handler, NULL);

	g_string_free(log_buffer, TRUE);
	log_buffer = NULL;
}
