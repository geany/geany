/*
 *      tools.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


enum
{
	CC_COLUMN_ID,
	CC_COLUMN_STATUS,
	CC_COLUMN_TOOLTIP,
	CC_COLUMN_CMD,
	CC_COLUMN_LABEL,
	CC_COLUMN_COUNT
};

/* custom commands code*/
struct cc_dialog
{
	guint count;
	GtkWidget *view;
	GtkTreeViewColumn *edit_column;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GtkWidget *button_add;
	GtkWidget *button_remove;
	GtkWidget *button_up;
	GtkWidget *button_down;
};

static gboolean cc_error_occurred = FALSE;
static gboolean cc_reading_finished = FALSE;
static GString *cc_buffer;


static gboolean cc_exists_command(const gchar *command)
{
	gchar *path = g_find_program_in_path(command);

	g_free(path);

	return path != NULL;
}


/* update STATUS and TOOLTIP columns according to cmd */
static void cc_dialog_update_row_status(GtkListStore *store, GtkTreeIter *iter, const gchar *cmd)
{
	GError *err = NULL;
	const gchar *stock_id = GTK_STOCK_NO;
	gchar *tooltip = NULL;
	gint argc;
	gchar **argv;

	if (! NZV(cmd))
		stock_id = GTK_STOCK_YES;
	else if (g_shell_parse_argv(cmd, &argc, &argv, &err))
	{
		if (argc > 0 && cc_exists_command(argv[0]))
			stock_id = GTK_STOCK_YES;
		else
			tooltip = g_strdup_printf(_("Invalid command: %s"), _("Command not found"));
		g_strfreev(argv);
	}
	else
	{
		tooltip = g_strdup_printf(_("Invalid command: %s"), err->message);
		g_error_free(err);
	}

	gtk_list_store_set(store, iter, CC_COLUMN_STATUS, stock_id, CC_COLUMN_TOOLTIP, tooltip, -1);
	g_free(tooltip);
}


/* adds a new row for custom command @p idx, or an new empty one if < 0 */
static void cc_dialog_add_command(struct cc_dialog *cc, gint idx, gboolean start_editing)
{
	GtkTreeIter iter;
	const gchar *cmd = NULL;
	const gchar *label = NULL;
	guint id = cc->count;

	if (idx >= 0)
	{
		cmd = ui_prefs.custom_commands[idx];
		label = ui_prefs.custom_commands_labels[idx];
	}

	cc->count++;
	gtk_list_store_append(cc->store, &iter);
	gtk_list_store_set(cc->store, &iter, CC_COLUMN_ID, id, CC_COLUMN_CMD, cmd, CC_COLUMN_LABEL, label, -1);
	cc_dialog_update_row_status(cc->store, &iter, cmd);

	if (start_editing)
	{
		GtkTreePath *path;

		gtk_widget_grab_focus(cc->view);
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(cc->store), &iter);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(cc->view), path, cc->edit_column, TRUE);
		gtk_tree_path_free(path);
	}
}


static void cc_on_dialog_add_clicked(GtkButton *button, struct cc_dialog *cc)
{
	cc_dialog_add_command(cc, -1, TRUE);
}


static void cc_on_dialog_remove_clicked(GtkButton *button, struct cc_dialog *cc)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(cc->selection, NULL, &iter))
		gtk_list_store_remove(cc->store, &iter);
}


static void cc_on_dialog_move_up_clicked(GtkButton *button, struct cc_dialog *cc)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(cc->selection, NULL, &iter))
	{
		GtkTreePath *path;
		GtkTreeIter prev;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(cc->store), &iter);
		if (gtk_tree_path_prev(path) &&
			gtk_tree_model_get_iter(GTK_TREE_MODEL(cc->store), &prev, path))
		{
			gtk_list_store_move_before(cc->store, &iter, &prev);
		}
		gtk_tree_path_free(path);
	}
}


static void cc_on_dialog_move_down_clicked(GtkButton *button, struct cc_dialog *cc)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(cc->selection, NULL, &iter))
	{
		GtkTreeIter next = iter;

		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(cc->store), &next))
			gtk_list_store_move_after(cc->store, &iter, &next);
	}
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
		editor_select_lines(doc->editor, FALSE);

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

		if (pid != 0)
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


static void cc_dialog_on_command_edited(GtkCellRendererText *renderer, gchar *path, gchar *text,
		struct cc_dialog *cc)
{
	GtkTreeIter iter;

	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(cc->store), &iter, path);
	gtk_list_store_set(cc->store, &iter, CC_COLUMN_CMD, text, -1);
	cc_dialog_update_row_status(cc->store, &iter, text);
}


static void cc_dialog_on_label_edited(GtkCellRendererText *renderer, gchar *path, gchar *text,
		struct cc_dialog *cc)
{
	GtkTreeIter iter;

	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(cc->store), &iter, path);
	gtk_list_store_set(cc->store, &iter, CC_COLUMN_LABEL, text, -1);
}


/* re-compute IDs to reflect the current store state */
static void cc_dialog_update_ids(struct cc_dialog *cc)
{
	GtkTreeIter iter;

	cc->count = 1;
	if (! gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cc->store), &iter))
		return;

	do
	{
		gtk_list_store_set(cc->store, &iter, CC_COLUMN_ID, cc->count, -1);
		cc->count++;
	}
	while (gtk_tree_model_iter_next(GTK_TREE_MODEL(cc->store), &iter));
}


/* update sensitiveness of the buttons according to the selection */
static void cc_dialog_update_sensitive(struct cc_dialog *cc)
{
	GtkTreeIter iter;
	gboolean has_selection = FALSE;
	gboolean first_selected = FALSE;
	gboolean last_selected = FALSE;

	if ((has_selection = gtk_tree_selection_get_selected(cc->selection, NULL, &iter)))
	{
		GtkTreePath *path;
		GtkTreePath *copy;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(cc->store), &iter);
		copy = gtk_tree_path_copy(path);
		first_selected = ! gtk_tree_path_prev(copy);
		gtk_tree_path_free(copy);
		gtk_tree_path_next(path);
		last_selected = ! gtk_tree_model_get_iter(GTK_TREE_MODEL(cc->store), &iter, path);
		gtk_tree_path_free(path);
	}

	gtk_widget_set_sensitive(cc->button_remove, has_selection);
	gtk_widget_set_sensitive(cc->button_up, has_selection && ! first_selected);
	gtk_widget_set_sensitive(cc->button_down, has_selection && ! last_selected);
}


static void cc_dialog_on_tree_selection_changed(GtkTreeSelection *selection, struct cc_dialog *cc)
{
	cc_dialog_update_sensitive(cc);
}


static void cc_dialog_on_row_inserted(GtkTreeModel *model, GtkTreePath  *path, GtkTreeIter *iter,
		struct cc_dialog *cc)
{
	cc_dialog_update_ids(cc);
	cc_dialog_update_sensitive(cc);
}


static void cc_dialog_on_row_deleted(GtkTreeModel *model, GtkTreePath  *path, struct cc_dialog *cc)
{
	cc_dialog_update_ids(cc);
	cc_dialog_update_sensitive(cc);
}


static void cc_dialog_on_rows_reordered(GtkTreeModel *model, GtkTreePath  *path, GtkTreeIter *iter,
		gpointer new_order, struct cc_dialog *cc)
{
	cc_dialog_update_ids(cc);
	cc_dialog_update_sensitive(cc);
}


static void cc_show_dialog_custom_commands(void)
{
	GtkWidget *dialog, *label, *vbox, *scroll, *buttonbox;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	guint i;
	struct cc_dialog cc;

	dialog = gtk_dialog_new_with_buttons(_("Set Custom Commands"), GTK_WINDOW(main_widgets.window),
						GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 300); /* give a reasonable minimal default size */
	vbox = ui_dialog_vbox_new(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	gtk_widget_set_name(dialog, "GeanyDialog");

	label = gtk_label_new(_("You can send the current selection to any of these commands and the output of the command replaces the current selection."));
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	cc.count = 1;
	cc.store = gtk_list_store_new(CC_COLUMN_COUNT, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_STRING);
	cc.view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(cc.store));
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(cc.view), CC_COLUMN_TOOLTIP);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(cc.view), TRUE);
	cc.selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(cc.view));
	/* ID column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("ID"), renderer, "text", CC_COLUMN_ID, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cc.view), column);
	/* command column, holding status and command display */
	column = g_object_new(GTK_TYPE_TREE_VIEW_COLUMN, "title", _("Command"), "expand", TRUE, "resizable", TRUE, NULL);
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer, "stock-id", CC_COLUMN_STATUS, NULL);
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	g_signal_connect(renderer, "edited", G_CALLBACK(cc_dialog_on_command_edited), &cc);
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_set_attributes(column, renderer, "text", CC_COLUMN_CMD, NULL);
	cc.edit_column = column;
	gtk_tree_view_append_column(GTK_TREE_VIEW(cc.view), column);
	/* label column */
	renderer = gtk_cell_renderer_text_new();
	g_object_set(renderer, "editable", TRUE, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	g_signal_connect(renderer, "edited", G_CALLBACK(cc_dialog_on_label_edited), &cc);
	column = gtk_tree_view_column_new_with_attributes(_("Label"), renderer, "text", CC_COLUMN_LABEL, NULL);
	g_object_set(column, "expand", TRUE, "resizable", TRUE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(cc.view), column);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(scroll), cc.view);
	gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

	if (ui_prefs.custom_commands != NULL)
	{
		GtkTreeIter iter;
		guint len = g_strv_length(ui_prefs.custom_commands);

		for (i = 0; i < len; i++)
		{
			if (! NZV(ui_prefs.custom_commands[i]))
				continue; /* skip empty fields */

			cc_dialog_add_command(&cc, i, FALSE);
		}

		/* focus the first row if any */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cc.store), &iter))
		{
			GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(cc.store), &iter);

			gtk_tree_view_set_cursor(GTK_TREE_VIEW(cc.view), path, cc.edit_column, FALSE);
			gtk_tree_path_free(path);
		}
	}

	buttonbox = gtk_hbutton_box_new();
	gtk_box_set_spacing(GTK_BOX(buttonbox), 6);
	gtk_box_pack_start(GTK_BOX(vbox), buttonbox, FALSE, FALSE, 0);
	cc.button_add = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(cc.button_add, "clicked", G_CALLBACK(cc_on_dialog_add_clicked), &cc);
	gtk_container_add(GTK_CONTAINER(buttonbox), cc.button_add);
	cc.button_remove = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(cc.button_remove, "clicked", G_CALLBACK(cc_on_dialog_remove_clicked), &cc);
	gtk_container_add(GTK_CONTAINER(buttonbox), cc.button_remove);
	cc.button_up = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	g_signal_connect(cc.button_up, "clicked", G_CALLBACK(cc_on_dialog_move_up_clicked), &cc);
	gtk_container_add(GTK_CONTAINER(buttonbox), cc.button_up);
	cc.button_down = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	g_signal_connect(cc.button_down, "clicked", G_CALLBACK(cc_on_dialog_move_down_clicked), &cc);
	gtk_container_add(GTK_CONTAINER(buttonbox), cc.button_down);

	cc_dialog_update_sensitive(&cc);

	/* only connect the selection signal when all other cc_dialog fields are set */
	g_signal_connect(cc.selection, "changed", G_CALLBACK(cc_dialog_on_tree_selection_changed), &cc);
	g_signal_connect(cc.store, "row-inserted", G_CALLBACK(cc_dialog_on_row_inserted), &cc);
	g_signal_connect(cc.store, "row-deleted", G_CALLBACK(cc_dialog_on_row_deleted), &cc);
	g_signal_connect(cc.store, "rows-reordered", G_CALLBACK(cc_dialog_on_rows_reordered), &cc);

	gtk_widget_show_all(vbox);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		GSList *cmd_list = NULL;
		GSList *lbl_list = NULL;
		gint len = 0;
		gchar **commands = NULL;
		gchar **labels = NULL;
		GtkTreeIter iter;

		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cc.store), &iter))
		{
			do
			{
				gchar *cmd;
				gchar *lbl;

				gtk_tree_model_get(GTK_TREE_MODEL(cc.store), &iter, CC_COLUMN_CMD, &cmd, CC_COLUMN_LABEL, &lbl, -1);
				if (NZV(cmd))
				{
					cmd_list = g_slist_prepend(cmd_list, cmd);
					lbl_list = g_slist_prepend(lbl_list, lbl);
					len++;
				}
				else
				{
					g_free(cmd);
					g_free(lbl);
				}
			}
			while (gtk_tree_model_iter_next(GTK_TREE_MODEL(cc.store), &iter));
		}
		cmd_list = g_slist_reverse(cmd_list);
		lbl_list = g_slist_reverse(lbl_list);
		/* create a new null-terminated array but only if there is any commands defined */
		if (len > 0)
		{
			gint j = 0;
			GSList *cmd_node, *lbl_node;

			commands = g_new(gchar*, len + 1);
			labels = g_new(gchar*, len + 1);
			/* walk commands and labels lists */
			for (cmd_node = cmd_list, lbl_node = lbl_list; cmd_node != NULL; cmd_node = cmd_node->next, lbl_node = lbl_node->next)
			{
				commands[j] = (gchar*) cmd_node->data;
				labels[j] = (gchar*) lbl_node->data;
				j++;
			}
			/* null-terminate the arrays */
			commands[j] = NULL;
			labels[j] = NULL;
		}
		/* set the new arrays */
		g_strfreev(ui_prefs.custom_commands);
		ui_prefs.custom_commands = commands;
		g_strfreev(ui_prefs.custom_commands_labels);
		ui_prefs.custom_commands_labels = labels;
		/* rebuild the menu items */
		tools_create_insert_custom_command_menu_items();

		g_slist_free(cmd_list);
		g_slist_free(lbl_list);
	}
	gtk_widget_destroy(dialog);
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


static void cc_insert_custom_command_items(GtkMenu *me, const gchar *label, const gchar *tooltip, gint idx)
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

	item = gtk_menu_item_new_with_label(label);
	gtk_widget_set_tooltip_text(item, tooltip);
	if (key_idx != -1)
	{
		kb = keybindings_lookup_item(GEANY_KEY_GROUP_FORMAT, key_idx);
		gtk_widget_add_accelerator(item, "activate", gtk_accel_group_new(),
			kb->key, kb->mods, GTK_ACCEL_VISIBLE);
	}
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
			const gchar *label = ui_prefs.custom_commands_labels[i];

			if (! NZV(label))
				label = ui_prefs.custom_commands[i];
			if (NZV(label)) /* skip empty items */
			{
				cc_insert_custom_command_items(menu_edit, label, ui_prefs.custom_commands[i], idx);
				idx++;
			}
		}
	}

	/* separator and Set menu item */
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu_edit), item);
	gtk_widget_show(item);

	cc_insert_custom_command_items(menu_edit, _("Set Custom Commands"), NULL, -1);
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
	gchar *text;
	const gchar *range;

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
on_color_cancel_button_clicked(GtkButton *button, gpointer user_data)
{
	gtk_widget_hide(ui_widgets.open_colorsel);
}


static void
on_color_ok_button_clicked(GtkButton *button, gpointer user_data)
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
