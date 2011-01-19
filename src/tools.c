/*
 *      tools.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2011 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2011 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/*
 * Miscellaneous code for the built-in Tools menu items, and custom command code.
 * For Plugins code see plugins.c.
 */

#include "geany.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifdef G_OS_UNIX
# include <sys/types.h>
# include <sys/wait.h>
# include <signal.h>
#endif

#include "tools.h"
#include "support.h"
#include "document.h"
#include "editor.h"
#include "sciwrappers.h"
#include "utils.h"
#include "ui_utils.h"
#include "msgwindow.h"
#include "keybindings.h"
#include "templates.h"
#include "win32.h"
#include "dialogs.h"


/* custom commands code*/
struct cc_dialog
{
	gint count;
	GtkWidget *box;
};

static gboolean cc_error_occurred = FALSE;
static gboolean cc_reading_finished = FALSE;
static GString *cc_buffer;

static void cc_add_command(struct cc_dialog *cc, gint idx)
{
	GtkWidget *label, *entry, *hbox;
	gchar str[6];

	hbox = gtk_hbox_new(FALSE, 5);
	g_snprintf(str, 5, "%d:", cc->count);
	label = gtk_label_new(str);

	entry = gtk_entry_new();
	if (idx >= 0)
		gtk_entry_set_text(GTK_ENTRY(entry), ui_prefs.custom_commands[idx]);
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_max_length(GTK_ENTRY(entry), 255);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 30);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
	gtk_widget_show_all(hbox);
	gtk_container_add(GTK_CONTAINER(cc->box), hbox);
	cc->count++;
}


static void cc_on_custom_commands_dlg_add_clicked(GtkToolButton *toolbutton, struct cc_dialog *cc)
{
	cc_add_command(cc, -1);
}


static gboolean cc_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg = NULL;
		GIOStatus rv;
		GError *err = NULL;

		cc_buffer = g_string_sized_new(256);

		do
		{
			rv = g_io_channel_read_line(ioc, &msg, NULL, NULL, &err);
			if (msg != NULL)
			{
				g_string_append(cc_buffer, msg);
				g_free(msg);
			}
			if (G_UNLIKELY(err != NULL))
			{
				geany_debug("%s: %s", G_STRFUNC, err->message);
				g_error_free(err);
				err = NULL;
			}
		} while (rv == G_IO_STATUS_NORMAL || rv == G_IO_STATUS_AGAIN);

		if (G_UNLIKELY(rv != G_IO_STATUS_EOF))
		{	/* Something went wrong? */
			g_warning("%s: %s\n", G_STRFUNC, "Incomplete command output");
		}
	}
	return FALSE;
}


static gboolean cc_iofunc_err(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg = NULL;
		GString *str = g_string_sized_new(256);
		GIOStatus rv;

		do
		{
			rv = g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL);
			if (msg != NULL)
			{
				g_string_append(str, msg);
				g_free(msg);
			}
		} while (rv == G_IO_STATUS_NORMAL || rv == G_IO_STATUS_AGAIN);

		if (NZV(str->str))
		{
			g_warning("%s: %s\n", (const gchar *) data, str->str);
			ui_set_statusbar(TRUE,
				_("The executed custom command returned an error. "
				"Your selection was not changed. Error message: %s"),
				str->str);
			cc_error_occurred = TRUE;

		}
		g_string_free(str, TRUE);
	}
	cc_reading_finished = TRUE;
	return FALSE;
}


static gboolean cc_replace_sel_cb(gpointer user_data)
{
	GeanyDocument *doc = user_data;

	if (! cc_reading_finished)
	{	/* keep this function in the main loop until cc_iofunc_err() has finished */
		return TRUE;
	}

	if (! cc_error_occurred && cc_buffer != NULL)
	{	/* Command completed successfully */
		sci_replace_sel(doc->editor->sci, cc_buffer->str);
		g_string_free(cc_buffer, TRUE);
		cc_buffer = NULL;
	}

	cc_error_occurred = FALSE;
	cc_reading_finished = FALSE;

	return FALSE;
}


/* check whether the executed command failed and if so do nothing.
 * If it returned with a sucessful exit code, replace the selection. */
static void cc_exit_cb(GPid child_pid, gint status, gpointer user_data)
{
	/* if there was already an error, skip further checks */
	if (! cc_error_occurred)
	{
#ifdef G_OS_UNIX
		if (WIFEXITED(status))
		{
			if (WEXITSTATUS(status) != EXIT_SUCCESS)
				cc_error_occurred = TRUE;
		}
		else if (WIFSIGNALED(status))
		{	/* the terminating signal: WTERMSIG (status)); */
			cc_error_occurred = TRUE;
		}
		else
		{	/* any other failure occured */
			cc_error_occurred = TRUE;
		}
#else
		cc_error_occurred = ! win32_get_exit_status(child_pid);
#endif

		if (cc_error_occurred)
		{	/* here we are sure cc_error_occurred was set due to an unsuccessful exit code
			 * and so we add an error message */
			/* TODO maybe include the exit code in the error message */
			ui_set_statusbar(TRUE,
				_("The executed custom command exited with an unsuccessful exit code."));
		}
	}

	g_idle_add(cc_replace_sel_cb, user_data);
	g_spawn_close_pid(child_pid);
}


/* Executes command (which should include all necessary command line args) and passes the current
 * selection through the standard input of command. The whole output of command replaces the
 * current selection. */
void tools_execute_custom_command(GeanyDocument *doc, const gchar *command)
{
	GError *error = NULL;
	GPid pid;
	gchar **argv;
	gint stdin_fd;
	gint stdout_fd;
	gint stderr_fd;

	g_return_if_fail(doc != NULL && command != NULL);

	if (! sci_has_selection(doc->editor->sci))
		return;

	if (!g_shell_parse_argv(command, NULL, &argv, &error))
	{
		ui_set_statusbar(TRUE, _("Custom command failed: %s"), error->message);
		g_error_free(error);
		return;
	}
	ui_set_statusbar(TRUE, _("Passing data and executing custom command: %s"), command);

	cc_error_occurred = FALSE;

	if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
						NULL, NULL, &pid, &stdin_fd, &stdout_fd, &stderr_fd, &error))
	{
		gchar *sel;
		gint len, remaining, wrote;

		if (pid > 0)
			g_child_watch_add(pid, (GChildWatchFunc) cc_exit_cb, doc);

		/* use GIOChannel to monitor stdout */
		utils_set_up_io_channel(stdout_fd, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
				FALSE, cc_iofunc, NULL);
		/* copy program's stderr to Geany's stdout to help error tracking */
		utils_set_up_io_channel(stderr_fd, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
				FALSE, cc_iofunc_err, (gpointer)command);

		/* get selection */
		len = sci_get_selected_text_length(doc->editor->sci);
		sel = g_malloc0(len + 1);
		sci_get_selected_text(doc->editor->sci, sel);

		/* write data to the command */
		remaining = len - 1;
		do
		{
			wrote = write(stdin_fd, sel, remaining);
			if (G_UNLIKELY(wrote < 0))
			{
				g_warning("%s: %s: %s\n", G_STRFUNC, "Failed sending data to command",
										g_strerror(errno));
				break;
			}
			remaining -= wrote;
		} while (remaining > 0);
		close(stdin_fd);
		g_free(sel);
	}
	else
	{
		geany_debug("g_spawn_async_with_pipes() failed: %s", error->message);
		ui_set_statusbar(TRUE, _("Custom command failed: %s"), error->message);
		g_error_free(error);
	}

	g_strfreev(argv);
}


static void cc_show_dialog_custom_commands(void)
{
	GtkWidget *dialog, *label, *vbox, *button;
	guint i;
	struct cc_dialog cc;

	dialog = gtk_dialog_new_with_buttons(_("Set Custom Commands"), GTK_WINDOW(main_widgets.window),
						GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_widget_set_name(dialog, "GeanyDialog");

	label = gtk_label_new(_("You can send the current selection to any of these commands and the output of the command replaces the current selection."));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_container_add(GTK_CONTAINER(vbox), label);

	cc.count = 1;
	cc.box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), cc.box);

	if (ui_prefs.custom_commands == NULL || g_strv_length(ui_prefs.custom_commands) == 0)
	{
		cc_add_command(&cc, -1);
	}
	else
	{
		guint len = g_strv_length(ui_prefs.custom_commands);
		for (i = 0; i < len; i++)
		{
			if (ui_prefs.custom_commands[i][0] == '\0')
				continue; /* skip empty fields */

			cc_add_command(&cc, i);
		}
	}

	button = gtk_button_new_from_stock("gtk-add");
	g_signal_connect(button, "clicked", G_CALLBACK(cc_on_custom_commands_dlg_add_clicked), &cc);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		/* get all hboxes which contain a label and an entry element */
		GList *children = gtk_container_get_children(GTK_CONTAINER(cc.box));
		GList *node, *list;
		GSList *result_list = NULL;
		gint j = 0;
		gint len = 0;
		gchar **result = NULL;
		const gchar *text;

		foreach_list(node, children)
		{
			/* get the contents of each hbox */
			list = gtk_container_get_children(GTK_CONTAINER(node->data));

			/* first element of the list is the label, so skip it and get the entry element */
			text = gtk_entry_get_text(GTK_ENTRY(list->next->data));

			/* if the content of the entry is non-empty, add it to the result array */
			if (text[0] != '\0')
			{
				result_list = g_slist_append(result_list, g_strdup(text));
				len++;
			}
			g_list_free(list);
		}
		/* create a new null-terminated array but only if there any commands defined */
		if (len > 0)
		{
			result = g_new(gchar*, len + 1);
			while (result_list != NULL)
			{
				result[j] = (gchar*) result_list->data;

				result_list = result_list->next;
				j++;
			}
			result[len] = NULL; /* null-terminate the array */
		}
		/* set the new array */
		g_strfreev(ui_prefs.custom_commands);
		ui_prefs.custom_commands = result;
		/* rebuild the menu items */
		tools_create_insert_custom_command_menu_items();

		g_slist_free(result_list);
		g_list_free(children);
	}
	gtk_widget_destroy(dialog);
}


/* enable or disable all custom command menu items when the sub menu is opened */
static void cc_on_custom_command_menu_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gint i, len;
	gboolean enable;
	GList *children, *node;

	g_return_if_fail(doc != NULL);

	enable = sci_has_selection(doc->editor->sci) && (ui_prefs.custom_commands != NULL);

	children = gtk_container_get_children(GTK_CONTAINER(user_data));
	len = g_list_length(children);
	i = 0;
	foreach_list(node, children)
	{
		if (i == (len - 2))
			break; /* stop before the last two elements (the seperator and the set entry) */

		gtk_widget_set_sensitive(GTK_WIDGET(node->data), enable);
		i++;
	}
	g_list_free(children);
}


static void cc_on_custom_command_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gint command_idx;

	g_return_if_fail(doc != NULL);

	command_idx = GPOINTER_TO_INT(user_data);

	if (ui_prefs.custom_commands == NULL ||
		command_idx < 0 || command_idx > (gint) g_strv_length(ui_prefs.custom_commands))
	{
		cc_show_dialog_custom_commands();
		return;
	}

	/* send it through the command and when the command returned the output the current selection
	 * will be replaced */
	tools_execute_custom_command(doc, ui_prefs.custom_commands[command_idx]);
}


static void cc_insert_custom_command_items(GtkMenu *me, gchar *label, gint idx)
{
	GtkWidget *item;
	gint key_idx = -1;
	GeanyKeyBinding *kb = NULL;

	switch (idx)
	{
		case 0: key_idx = GEANY_KEYS_FORMAT_SENDTOCMD1; break;
		case 1: key_idx = GEANY_KEYS_FORMAT_SENDTOCMD2; break;
		case 2: key_idx = GEANY_KEYS_FORMAT_SENDTOCMD3; break;
	}

	if (key_idx != -1)
		kb = keybindings_lookup_item(GEANY_KEY_GROUP_FORMAT, key_idx);

	item = gtk_menu_item_new_with_label(label);
	if (key_idx != -1)
		gtk_widget_add_accelerator(item, "activate", gtk_accel_group_new(),
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(me), item);
	gtk_widget_show(item);
	g_signal_connect(item, "activate", G_CALLBACK(cc_on_custom_command_activate),
		GINT_TO_POINTER(idx));
}


void tools_create_insert_custom_command_menu_items(void)
{
	GtkMenu *menu_edit = GTK_MENU(ui_lookup_widget(main_widgets.window, "send_selection_to2_menu"));
	GtkWidget *item;
	GList *me_children, *node;
	static gboolean signal_set = FALSE;

	/* first clean the menus to be able to rebuild them */
	me_children = gtk_container_get_children(GTK_CONTAINER(menu_edit));
	foreach_list(node, me_children)
		gtk_widget_destroy(GTK_WIDGET(node->data));
	g_list_free(me_children);

	if (ui_prefs.custom_commands == NULL || g_strv_length(ui_prefs.custom_commands) == 0)
	{
		item = gtk_menu_item_new_with_label(_("No custom commands defined."));
		gtk_container_add(GTK_CONTAINER(menu_edit), item);
		gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
	}
	else
	{
		guint i, len;
		gint idx = 0;
		len = g_strv_length(ui_prefs.custom_commands);
		for (i = 0; i < len; i++)
		{
			if (ui_prefs.custom_commands[i][0] != '\0') /* skip empty fields */
			{
				cc_insert_custom_command_items(menu_edit, ui_prefs.custom_commands[i], idx);
				idx++;
			}
		}
	}

	/* separator and Set menu item */
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_edit), item);
	gtk_widget_show(item);

	cc_insert_custom_command_items(menu_edit, _("Set Custom Commands"), -1);

	if (! signal_set)
	{
		g_signal_connect(ui_lookup_widget(main_widgets.window, "send_selection_to2"),
					"activate", G_CALLBACK(cc_on_custom_command_menu_activate), menu_edit);
		signal_set = TRUE;
	}
}


/* (stolen from bluefish, thanks)
 * Returns number of characters, lines and words in the supplied gchar*.
 * Handles UTF-8 correctly. Input must be properly encoded UTF-8.
 * Words are defined as any characters grouped, separated with spaces. */
static void word_count(gchar *text, guint *chars, guint *lines, guint *words)
{
	guint in_word = 0;
	gunichar utext;

	if (! text)
		return; /* politely refuse to operate on NULL */

	*chars = *words = *lines = 0;
	while (*text != '\0')
	{
		(*chars)++;

		switch (*text)
		{
			case '\n':
				(*lines)++;
			case '\r':
			case '\f':
			case '\t':
			case ' ':
			case '\v':
				mb_word_separator:
				if (in_word)
				{
					in_word = 0;
					(*words)++;
				}
				break;
			default:
				utext = g_utf8_get_char_validated(text, 2); /* This might be an utf-8 char */
				if (g_unichar_isspace(utext)) /* Unicode encoded space? */
					goto mb_word_separator;
				if (g_unichar_isgraph(utext)) /* Is this something printable? */
					in_word = 1;
				break;
		}
		/* Even if the current char is 2 bytes, this will iterate correctly. */
		text = g_utf8_next_char(text);
	}

	/* Capture last word, if there's no whitespace at the end of the file. */
	if (in_word)
		(*words)++;
	/* We start counting line numbers from 1 */
	if (*chars > 0)
		(*lines)++;
}


void tools_word_count(void)
{
	GtkWidget *dialog, *label, *vbox, *table;
	GeanyDocument *doc;
	guint chars = 0, lines = 0, words = 0;
	gchar *text, *range;

	doc = document_get_current();
	g_return_if_fail(doc != NULL);

	dialog = gtk_dialog_new_with_buttons(_("Word Count"), GTK_WINDOW(main_widgets.window),
										 GTK_DIALOG_DESTROY_WITH_PARENT,
										 GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_widget_set_name(dialog, "GeanyDialog");

	if (sci_has_selection(doc->editor->sci))
	{
		text = g_malloc0(sci_get_selected_text_length(doc->editor->sci) + 1);
		sci_get_selected_text(doc->editor->sci, text);
		range = _("selection");
	}
	else
	{
		text = g_malloc(sci_get_length(doc->editor->sci) + 1);
		sci_get_text(doc->editor->sci, sci_get_length(doc->editor->sci) + 1 , text);
		range = _("whole document");
	}
	word_count(text, &chars, &lines, &words);
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


/*
 * color dialog callbacks
 */
#ifndef G_OS_WIN32
static void
on_color_cancel_button_clicked         (GtkButton       *button,
                                        gpointer         user_data)
{
	gtk_widget_hide(ui_widgets.open_colorsel);
}


static void
on_color_ok_button_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
	GdkColor color;
	GeanyDocument *doc = document_get_current();
	gchar *hex;

	gtk_widget_hide(ui_widgets.open_colorsel);
	g_return_if_fail(doc != NULL);

	gtk_color_selection_get_current_color(
			GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(ui_widgets.open_colorsel)->colorsel), &color);

	hex = utils_get_hex_from_color(&color);
	editor_insert_color(doc->editor, hex);
	g_free(hex);
}
#endif


/* This shows the color selection dialog to choose a color. */
void tools_color_chooser(const gchar *color)
{
#ifdef G_OS_WIN32
	win32_show_color_dialog(color);
#else
	gchar *c = (gchar*) color;

	if (ui_widgets.open_colorsel == NULL)
	{
		ui_widgets.open_colorsel = gtk_color_selection_dialog_new(_("Color Chooser"));
		gtk_widget_set_name(ui_widgets.open_colorsel, "GeanyDialog");
		gtk_window_set_transient_for(GTK_WINDOW(ui_widgets.open_colorsel), GTK_WINDOW(main_widgets.window));
		gtk_color_selection_set_has_palette(
			GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(ui_widgets.open_colorsel)->colorsel), TRUE);

		g_signal_connect(GTK_COLOR_SELECTION_DIALOG(ui_widgets.open_colorsel)->cancel_button, "clicked",
						G_CALLBACK(on_color_cancel_button_clicked), NULL);
		g_signal_connect(GTK_COLOR_SELECTION_DIALOG(ui_widgets.open_colorsel)->ok_button, "clicked",
						G_CALLBACK(on_color_ok_button_clicked), NULL);
		g_signal_connect(ui_widgets.open_colorsel, "delete-event",
						G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	}
	/* if color is non-NULL set it in the dialog as preselected color */
	if (c != NULL && (c[0] == '0' || c[0] == '#'))
	{
		GdkColor gc;

		if (c[0] == '0' && c[1] == 'x')
		{	/* we have a string of the format "0x00ff00" and we need it to "#00ff00" */
			c[1] = '#';
			c++;
		}
		gdk_color_parse(c, &gc);
		gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(
							GTK_COLOR_SELECTION_DIALOG(ui_widgets.open_colorsel)->colorsel), &gc);
		gtk_color_selection_set_previous_color(GTK_COLOR_SELECTION(
							GTK_COLOR_SELECTION_DIALOG(ui_widgets.open_colorsel)->colorsel), &gc);
	}

	/* We make sure the dialog is visible. */
	gtk_window_present(GTK_WINDOW(ui_widgets.open_colorsel));
#endif
}


