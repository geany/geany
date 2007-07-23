/*
 *      tools.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 * Miscellaneous code for the Tools menu items.
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
#include "sciwrappers.h"
#include "utils.h"
#include "ui_utils.h"
#include "msgwindow.h"
#include "keybindings.h"
#include "templates.h"


/* custom commands code*/
struct cc_dialog
{
	gint count;
	GtkWidget *box;
};


static void cc_add_command(struct cc_dialog *cc, gint idx)
{
	GtkWidget *label, *entry, *hbox;
	gchar str[6];

	hbox = gtk_hbox_new(FALSE, 5);
	g_snprintf(str, 5, "%d:", cc->count);
	label = gtk_label_new(str);

	entry = gtk_entry_new();
	if (idx >= 0)
		gtk_entry_set_text(GTK_ENTRY(entry), app->custom_commands[idx]);
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
		gint idx = GPOINTER_TO_INT(data);
		gchar *msg = NULL;
		GString *str = g_string_sized_new(256);
		GIOStatus rv;
		GError *err = NULL;

		do
		{
			rv = g_io_channel_read_line(ioc, &msg, NULL, NULL, &err);
			if (msg != NULL)
			{
				g_string_append(str, msg);
				g_free(msg);
			}
			if (err != NULL)
			{
				geany_debug("%s: %s", __func__, err->message);
				g_error_free(err);
				err = NULL;
			}
		} while (rv == G_IO_STATUS_NORMAL || rv == G_IO_STATUS_AGAIN);

		if (rv == G_IO_STATUS_EOF)
		{	// Command completed successfully
			sci_replace_sel(doc_list[idx].sci, str->str);
		}
		else
		{	// Something went wrong?
			g_warning("%s: %s\n", __func__, "Incomplete command output");
		}
		g_string_free(str, TRUE);
	}
	return FALSE;
}


static gboolean cc_iofunc_err(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg = NULL;

		while (g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL) && msg != NULL)
		{
			g_warning("%s: %s", (const gchar*) data, g_strstrip(msg));
			g_free(msg);
		}
		return TRUE;
	}

	return FALSE;
}


/* Executes command (which should include all necessary command line args) and passes the current
 * selection through the standard input of command. The whole output of command replaces the
 * current selection. */
void tools_execute_custom_command(gint idx, const gchar *command)
{
	GError *error = NULL;
	GPid pid;
	gchar **argv;
	gint stdin_fd;
	gint stdout_fd;
	gint stderr_fd;

	g_return_if_fail(DOC_IDX_VALID(idx) && command != NULL);

	if (! sci_can_copy(doc_list[idx].sci))
		return;

	argv = g_strsplit(command, " ", -1);
	msgwin_status_add(_("Passing data and executing custom command: %s"), command);

	if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
						NULL, NULL, &pid, &stdin_fd, &stdout_fd, &stderr_fd, &error))
	{
		gchar *sel;
		gint len, remaining, wrote;

		// use GIOChannel to monitor stdout
		utils_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
				FALSE, cc_iofunc, GINT_TO_POINTER(idx));
		// copy program's stderr to Geany's stdout to help error tracking
		utils_set_up_io_channel(stderr_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
				FALSE, cc_iofunc_err, (gpointer)command);

		// get selection
		len = sci_get_selected_text_length(doc_list[idx].sci);
		sel = g_malloc0(len);
		sci_get_selected_text(doc_list[idx].sci, sel);

		// write data to the command
		remaining = len - 1;
		do
		{
			wrote = write(stdin_fd, sel, remaining);
			if (wrote < 0)
			{
				g_warning("%s: %s: %s\n", __func__, "Failed sending data to command",
										strerror(errno));
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
		g_error_free(error);
	}

	g_strfreev(argv);
}


static void cc_show_dialog_custom_commands()
{
	GtkWidget *dialog, *label, *vbox, *button;
	guint i;
	struct cc_dialog cc;

	dialog = gtk_dialog_new_with_buttons(_("Set Custom Commands"), GTK_WINDOW(app->window),
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

	if (app->custom_commands == NULL || g_strv_length(app->custom_commands) == 0)
	{
		cc_add_command(&cc, -1);
	}
	else
	{
		for (i = 0; i < g_strv_length(app->custom_commands); i++)
		{
			if (app->custom_commands[i][0] == '\0')
				continue; // skip empty fields

			cc_add_command(&cc, i);
		}
	}

	button = gtk_button_new_from_stock("gtk-add");
	g_signal_connect((gpointer) button, "clicked",
			G_CALLBACK(cc_on_custom_commands_dlg_add_clicked), &cc);
	gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		// get all hboxes which contain a label and an entry element
		GList *children = gtk_container_get_children(GTK_CONTAINER(cc.box));
		GList *tmp;
		GSList *result_list = NULL;
		gint j = 0;
		gint len = 0;
		gchar **result = NULL;
		const gchar *text;

		while (children != NULL)
		{
			// get the contents of each hbox
			tmp = gtk_container_get_children(GTK_CONTAINER(children->data));

			// first element of the list is the label, so skip it and get the entry element
			tmp = tmp->next;

			text = gtk_entry_get_text(GTK_ENTRY(tmp->data));

			// if the content of the entry is non-empty, add it to the result array
			if (text[0] != '\0')
			{
				result_list = g_slist_append(result_list, g_strdup(text));
				len++;
			}
			children = children->next;
		}
		// create a new null-terminated array but only if there any commands defined
		if (len > 0)
		{
			result = g_new(gchar*, len + 1);
			while (result_list != NULL)
			{
				result[j] = (gchar*) result_list->data;

				result_list = result_list->next;
				j++;
			}
			result[len] = NULL; // null-terminate the array
		}
		// set the new array
		g_strfreev(app->custom_commands);
		app->custom_commands = result;
		// rebuild the menu items
		tools_create_insert_custom_command_menu_items();

		g_slist_free(result_list);
		g_list_free(children);
	}
	gtk_widget_destroy(dialog);
}


/* enable or disable all custom command menu items when the sub menu is opened */
static void cc_on_custom_command_menu_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gint idx = document_get_cur_idx();
	gint i, len;
	gboolean enable;
	GList *children;

	if (! DOC_IDX_VALID(idx)) return;

	enable = sci_can_copy(doc_list[idx].sci) && (app->custom_commands != NULL);

	children = gtk_container_get_children(GTK_CONTAINER(user_data));
	len = g_list_length(children);
	i = 0;
	while (children != NULL)
	{
		if (i == (len - 2))
			break; // stop before the last two elements (the seperator and the set entry)

		gtk_widget_set_sensitive(GTK_WIDGET(children->data), enable);
		children = children->next;
		i++;
	}
}


static void cc_on_custom_command_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gint idx = document_get_cur_idx();
	gint command_idx;

	if (! DOC_IDX_VALID(idx)) return;

	command_idx = GPOINTER_TO_INT(user_data);

	if (app->custom_commands == NULL ||
		command_idx < 0 || command_idx > (gint) g_strv_length(app->custom_commands))
	{
		cc_show_dialog_custom_commands();
		return;
	}

	// send it through the command and when the command returned the output the current selection
	// will be replaced
	tools_execute_custom_command(idx, app->custom_commands[command_idx]);
}


static void cc_insert_custom_command_items(GtkMenu *me, GtkMenu *mp, gchar *label, gint idx)
{
	GtkWidget *item;
	gint key_idx = -1;

	switch (idx)
	{
		case 0: key_idx = GEANY_KEYS_EDIT_SENDTOCMD1; break;
		case 1: key_idx = GEANY_KEYS_EDIT_SENDTOCMD2; break;
		case 2: key_idx = GEANY_KEYS_EDIT_SENDTOCMD3; break;
	}

	item = gtk_menu_item_new_with_label(label);
	if (key_idx != -1)
		gtk_widget_add_accelerator(item, "activate", gtk_accel_group_new(),
			keys[key_idx]->key, keys[key_idx]->mods, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(me), item);
	gtk_widget_show(item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(cc_on_custom_command_activate),
		GINT_TO_POINTER(idx));

	item = gtk_menu_item_new_with_label(label);
	if (key_idx != -1)
		gtk_widget_add_accelerator(item, "activate", gtk_accel_group_new(),
			keys[key_idx]->key, keys[key_idx]->mods, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(mp), item);
	gtk_widget_show(item);
	g_signal_connect((gpointer) item, "activate", G_CALLBACK(cc_on_custom_command_activate),
		GINT_TO_POINTER(idx));
}


void tools_create_insert_custom_command_menu_items()
{
	GtkMenu *menu_edit = GTK_MENU(lookup_widget(app->window, "send_selection_to2_menu"));
	GtkMenu *menu_popup = GTK_MENU(lookup_widget(app->popup_menu, "send_selection_to1_menu"));
	GtkWidget *item;
	GList *me_children;
	GList *mp_children;
	static gboolean signal_set = FALSE;

	// first clean the menus to be able to rebuild them
	me_children = gtk_container_get_children(GTK_CONTAINER(menu_edit));
	mp_children = gtk_container_get_children(GTK_CONTAINER(menu_popup));
	while (me_children != NULL)
	{
		gtk_widget_destroy(GTK_WIDGET(me_children->data));
		gtk_widget_destroy(GTK_WIDGET(mp_children->data));

		me_children = me_children->next;
		mp_children = mp_children->next;
	}


	if (app->custom_commands == NULL || g_strv_length(app->custom_commands) == 0)
	{
		item = gtk_menu_item_new_with_label(_("No custom commands defined."));
		gtk_container_add(GTK_CONTAINER(menu_edit), item);
		gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
		item = gtk_menu_item_new_with_label(_("No custom commands defined."));
		gtk_container_add(GTK_CONTAINER(menu_popup), item);
		gtk_widget_set_sensitive(item, FALSE);
		gtk_widget_show(item);
	}
	else
	{
		guint i;
		gint idx = 0;
		for (i = 0; i < g_strv_length(app->custom_commands); i++)
		{
			if (app->custom_commands[i][0] != '\0') // skip empty fields
			{
				cc_insert_custom_command_items(menu_edit, menu_popup, app->custom_commands[i], idx);
				idx++;
			}
		}
	}

	// separator and Set menu item
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_edit), item);
	gtk_widget_show(item);
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_popup), item);
	gtk_widget_show(item);

	cc_insert_custom_command_items(menu_edit, menu_popup, _("Set Custom Commands"), -1);

	if (! signal_set)
	{
		g_signal_connect((gpointer) lookup_widget(app->popup_menu, "send_selection_to1"),
					"activate", G_CALLBACK(cc_on_custom_command_menu_activate), menu_popup);
		g_signal_connect((gpointer) lookup_widget(app->window, "send_selection_to2"),
					"activate", G_CALLBACK(cc_on_custom_command_menu_activate), menu_edit);
		signal_set = TRUE;
	}
}

