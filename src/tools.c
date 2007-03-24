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


enum
{
	COLUMN_CHARACTER,
	COLUMN_HTML_NAME,
	N_COLUMNS
};

static GtkWidget *sc_dialog = NULL;
static GtkTreeStore *sc_store = NULL;
static GtkTreeView *sc_tree = NULL;

static void sc_on_tools_show_dialog_insert_special_chars_response
		(GtkDialog *dialog, gint response, gpointer user_data);
static void sc_on_tree_row_activated
		(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data);
static void sc_fill_store(GtkTreeStore *store);
static gboolean sc_insert(GtkTreeModel *model, GtkTreeIter *iter);



void tools_show_dialog_insert_special_chars()
{
	if (sc_dialog == NULL)
	{
		gint height;
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;
		GtkWidget *swin, *vbox, *label;

		sc_dialog = gtk_dialog_new_with_buttons(
					_("Special characters"), GTK_WINDOW(app->window),
					GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					_("_Insert"), GTK_RESPONSE_OK, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(sc_dialog));
		gtk_box_set_spacing(GTK_BOX(vbox), 6);

		height = GEANY_WINDOW_MINIMAL_HEIGHT;
		gtk_window_set_default_size(GTK_WINDOW(sc_dialog), height * 0.8, height);
		gtk_dialog_set_default_response(GTK_DIALOG(sc_dialog), GTK_RESPONSE_CANCEL);

		label = gtk_label_new(_("Choose a special character from the list below and double click on it or use the button to insert it at the current cursor position."));
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		sc_tree = GTK_TREE_VIEW(gtk_tree_view_new());

		sc_store = gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(sc_tree),
								GTK_TREE_MODEL(sc_store));

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
								_("Character"), renderer, "text", COLUMN_CHARACTER, NULL);
		gtk_tree_view_column_set_resizable(column, TRUE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(sc_tree), column);

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
								_("HTML (name)"), renderer, "text", COLUMN_HTML_NAME, NULL);
		gtk_tree_view_column_set_resizable(column, TRUE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(sc_tree), column);

		swin = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport(
					GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(sc_tree));

		gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

		g_signal_connect((gpointer) sc_tree, "row-activated",
					G_CALLBACK(sc_on_tree_row_activated), NULL);

		g_signal_connect((gpointer) sc_dialog, "response",
					G_CALLBACK(sc_on_tools_show_dialog_insert_special_chars_response), NULL);

		sc_fill_store(sc_store);

		//gtk_tree_view_expand_all(special_characters_tree);
		gtk_tree_view_set_search_column(sc_tree, COLUMN_HTML_NAME);
	}
	gtk_widget_show_all(sc_dialog);
}


// fill the tree model with data
/// TODO move this in a file and make it extendable for more data types
static void sc_fill_store(GtkTreeStore *store)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	guint i;

	gchar *chars[][2] =
		{
			{ _("HTML characters"), NULL },
			{ "\"", "&quot;" },
			{ "&", "&amp;" },
			{ "<", "&lt;" },
			{ ">", "&gt;" },

			{ _("ISO 8859-1 characters"), NULL },
			{ " ", "&nbsp;" },
			{ "¡", "&iexcl;" },
			{ "¢", "&cent;" },
			{ "£", "&pound;" },
			{ "¤", "&curren;" },
			{ "¥", "&yen;" },
			{ "¦", "&brvbar;" },
			{ "§", "&sect;" },
			{ "¨", "&uml;" },
			{ "©", "&copy;" },
			{ "®", "&reg;" },
			{ "«", "&laquo;" },
			{ "»", "&raquo;" },
			{ "¬", "&not;" },
			{ " ", "&shy;" },
			{ "¯", "&macr;" },
			{ "°", "&deg;" },
			{ "±", "&plusmn;" },
			{ "¹", "&sup1;" },
			{ "²", "&sup2;" },
			{ "³", "&sup3;" },
			{ "¼", "&frac14;" },
			{ "½", "&frac12;" },
			{ "¾", "&frac34;" },
			{ "×", "&times;" },
			{ "÷", "&divide;" },
			{ "´", "&acute;" },
			{ "µ", "&micro;" },
			{ "¶", "&para;" },
			{ "·", "&middot;" },
			{ "¸", "&cedil;" },
			{ "ª", "&ordf;" },
			{ "º", "&ordm;" },
			{ "¿", "&iquest;" },
			{ "À", "&Agrave;" },
			{ "Á", "&Aacute;" },
			{ "Â", "&Acirc;" },
			{ "Ã", "&Atilde;" },
			{ "Ä", "&Auml;" },
			{ "Å", "&Aring;" },
			{ "Æ", "&AElig;" },
			{ "Ç", "&Ccedil;" },
			{ "È", "&Egrave;" },
			{ "É", "&Eacute;" },
			{ "Ê", "&Ecirc;" },
			{ "Ë", "&Euml;" },
			{ "Ì", "&Igrave;" },
			{ "Í", "&Iacute;" },
			{ "Î", "&Icirc;" },
			{ "Ï", "&Iuml;" },
			{ "Ð", "&ETH;" },
			{ "Ñ", "&Ntilde;" },
			{ "Ò", "&Ograve;" },
			{ "Ó", "&Oacute;" },
			{ "Ô", "&Ocirc;" },
			{ "Õ", "&Otilde;" },
			{ "Ö", "&Ouml;" },
			{ "Ø", "&Oslash;" },
			{ "Ù", "&Ugrave;" },
			{ "Ú", "&Uacute;" },
			{ "Û", "&Ucirc;" },
			{ "Ü", "&Uuml;" },
			{ "Ý", "&Yacute;" },
			{ "Þ", "&THORN;" },
			{ "ß", "&szlig;" },
			{ "à", "&agrave;" },
			{ "á", "&aacute;" },
			{ "â", "&acirc;" },
			{ "ã", "&atilde;" },
			{ "ä", "&auml;" },
			{ "å", "&aring;" },
			{ "æ", "&aelig;" },
			{ "ç", "&ccedil;" },
			{ "è", "&egrave;" },
			{ "é", "&eacute;" },
			{ "ê", "&ecirc;" },
			{ "ë", "&euml;" },
			{ "ì", "&igrave;" },
			{ "í", "&iacute;" },
			{ "î", "&icirc;" },
			{ "ï", "&iuml;" },
			{ "ð", "&eth;" },
			{ "ñ", "&ntilde;" },
			{ "ò", "&ograve;" },
			{ "ó", "&oacute;" },
			{ "ô", "&ocirc;" },
			{ "õ", "&otilde;" },
			{ "ö", "&ouml;" },
			{ "ø", "&oslash;" },
			{ "ù", "&ugrave;" },
			{ "ú", "&uacute;" },
			{ "û", "&ucirc;" },
			{ "ü", "&uuml;" },
			{ "ý", "&yacute;" },
			{ "þ", "&thorn;" },
			{ "ÿ", "&yuml;" },

			{ _("Greek characters"), NULL },
			{ "Α", "&Alpha;" },
			{ "α", "&alpha;" },
			{ "Β", "&Beta;" },
			{ "β", "&beta;" },
			{ "Γ", "&Gamma;" },
			{ "γ", "&gamma;" },
			{ "Δ", "&Delta;" },
			{ "δ", "&Delta;" },
			{ "δ", "&delta;" },
			{ "Ε", "&Epsilon;" },
			{ "ε", "&epsilon;" },
			{ "Ζ", "&Zeta;" },
			{ "ζ", "&zeta;" },
			{ "Η", "&Eta;" },
			{ "η", "&eta;" },
			{ "Θ", "&Theta;" },
			{ "θ", "&theta;" },
			{ "Ι", "&Iota;" },
			{ "ι", "&iota;" },
			{ "Κ", "&Kappa;" },
			{ "κ", "&kappa;" },
			{ "Λ", "&Lambda;" },
			{ "λ", "&lambda;" },
			{ "Μ", "&Mu;" },
			{ "μ", "&mu;" },
			{ "Ν", "&Nu;" },
			{ "ν", "&nu;" },
			{ "Ξ", "&Xi;" },
			{ "ξ", "&xi;" },
			{ "Ο", "&Omicron;" },
			{ "ο", "&omicron;" },
			{ "Π", "&Pi;" },
			{ "π", "&pi;" },
			{ "Ρ", "&Rho;" },
			{ "ρ", "&rho;" },
			{ "Σ", "&Sigma;" },
			{ "ς", "&sigmaf;" },
			{ "σ", "&sigma;" },
			{ "Τ", "&Tau;" },
			{ "τ", "&tau;" },
			{ "Υ", "&Upsilon;" },
			{ "υ", "&upsilon;" },
			{ "Φ", "&Phi;" },
			{ "φ", "&phi;" },
			{ "Χ", "&Chi;" },
			{ "χ", "&chi;" },
			{ "Ψ", "&Psi;" },
			{ "ψ", "&psi;" },
			{ "Ω", "&Omega;" },
			{ "ω", "&omega;" },
			{ "ϑ", "&thetasym;" },
			{ "ϒ", "&upsih;" },
			{ "ϖ", "&piv;" },

			{ _("Mathematical characters"), NULL },
			{ "∀", "&forall;" },
			{ "∂", "&part;" },
			{ "∃", "&exist;" },
			{ "∅", "&empty;" },
			{ "∇", "&nabla;" },
			{ "∈", "&isin;" },
			{ "∉", "&notin;" },
			{ "∋", "&ni;" },
			{ "∏", "&prod;" },
			{ "∑", "&sum;" },
			{ "−", "&minus;" },
			{ "∗", "&lowast;" },
			{ "√", "&radic;" },
			{ "∝", "&prop;" },
			{ "∞", "&infin;" },
			{ "∠", "&ang;" },
			{ "∧", "&and;" },
			{ "∨", "&or;" },
			{ "∩", "&cap;" },
			{ "∪", "&cup;" },
			{ "∫", "&int;" },
			{ "∴", "&there4;" },
			{ "∼", "&sim;" },
			{ "≅", "&cong;" },
			{ "≈", "&asymp;" },
			{ "≠", "&ne;" },
			{ "≡", "&equiv;" },
			{ "≤", "&le;" },
			{ "≥", "&ge;" },
			{ "⊂", "&sub;" },
			{ "⊃", "&sup;" },
			{ "⊄", "&nsub;" },
			{ "⊆", "&sube;" },
			{ "⊇", "&supe;" },
			{ "⊕", "&oplus;" },
			{ "⊗", "&otimes;" },
			{ "⊥", "&perp;" },
			{ "⋅", "&sdot;" },
			{ "◊", "&loz;" },

			{ _("Technical characters"), NULL },
			{ "⌈", "&lceil;" },
			{ "⌉", "&rceil;" },
			{ "⌊", "&lfloor;" },
			{ "⌋", "&rfloor;" },
			{ "〈", "&lang;" },
			{ "〉", "&rang;" },

			{ _("Arrow characters"), NULL },
			{ "←", "&larr;" },
			{ "↑", "&uarr;" },
			{ "→", "&rarr;" },
			{ "↓", "&darr;" },
			{ "↔", "&harr;" },
			{ "↵", "&crarr;" },
			{ "⇐", "&lArr;" },
			{ "⇑", "&uArr;" },
			{ "⇒", "&rArr;" },
			{ "⇓", "&dArr;" },
			{ "⇔", "&hArr;" },

			{ _("Punctuation characters"), NULL },
			{ "–", "&ndash;" },
			{ "—", "&mdash;" },
			{ "‘", "&lsquo;" },
			{ "’", "&rsquo;" },
			{ "‚", "&sbquo;" },
			{ "“", "&ldquo;" },
			{ "”", "&rdquo;" },
			{ "„", "&bdquo;" },
			{ "†", "&dagger;" },
			{ "‡", "&Dagger;" },
			{ "…", "&hellip;" },
			{ "‰", "&permil;" },
			{ "‹", "&lsaquo;" },
			{ "›", "&rsaquo;" },

			{ _("Miscellaneous characters"), NULL },
			{ "•", "&bull;" },
			{ "′", "&prime;" },
			{ "″", "&Prime;" },
			{ "‾", "&oline;" },
			{ "⁄", "&frasl;" },
			{ "℘", "&weierp;" },
			{ "ℑ", "&image;" },
			{ "ℜ", "&real;" },
			{ "™", "&trade;" },
			{ "€", "&euro;" },
			{ "ℵ", "&alefsym;" },
			{ "♠", "&spades;" },
			{ "♣", "&clubs;" },
			{ "♥", "&hearts;" },
			{ "♦", "&diams;" },
			{ "Œ", "&OElig;" },
			{ "œ", "&oelig;" },
			{ "Š", "&Scaron;" },
			{ "š", "&scaron;" },
			{ "Ÿ", "&Yuml;" },
			{ "ƒ", "&fnof;" },
		};

	for (i = 0; i < G_N_ELEMENTS(chars); i++)
	{
		if (chars[i][1] == NULL)
		{	// add a category
			gtk_tree_store_append(store, &iter, NULL);
			gtk_tree_store_set(store, &iter, COLUMN_CHARACTER, chars[i][0], -1);
			if (parent_iter != NULL) gtk_tree_iter_free(parent_iter);
			parent_iter = gtk_tree_iter_copy(&iter);
		}
		else
		{	// add child to parent_iter
			gtk_tree_store_append(store, &iter, parent_iter);
			gtk_tree_store_set(store, &iter, COLUMN_CHARACTER, chars[i][0],
											 COLUMN_HTML_NAME, chars[i][1], -1);
		}
	}
}


/* just inserts the HTML_NAME coloumn of the selected row at current position
 * returns only TRUE if a valid selection(i.e. no category) could be found */
static gboolean sc_insert(GtkTreeModel *model, GtkTreeIter *iter)
{
	gint idx = document_get_cur_idx();
	gboolean result = FALSE;

	if (DOC_IDX_VALID(idx))
	{
		gchar *str;
		gint pos = sci_get_current_position(doc_list[idx].sci);

		gtk_tree_model_get(model, iter, COLUMN_HTML_NAME, &str, -1);
		if (str && *str)
		{
			sci_insert_text(doc_list[idx].sci, pos, str);
			g_free(str);
			result = TRUE;
		}
	}
	return result;
}


static void sc_on_tools_show_dialog_insert_special_chars_response(GtkDialog *dialog, gint response,
														gpointer user_data)
{
	if (response == GTK_RESPONSE_OK)
	{
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection(sc_tree);

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			// only hide dialog if selection was not a category
			if (sc_insert(model, &iter))
				gtk_widget_hide(GTK_WIDGET(dialog));
		}
	}
	else
		gtk_widget_hide(GTK_WIDGET(dialog));
}


static void sc_on_tree_row_activated(GtkTreeView *treeview, GtkTreePath *path,
											  GtkTreeViewColumn *col, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(sc_store);

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		// only hide dialog if selection was not a category
		if (sc_insert(model, &iter))
			gtk_widget_hide(sc_dialog);
		else
		{	// double click on a category to toggle the expand or collapse it
			if (gtk_tree_view_row_expanded(sc_tree, path))
				gtk_tree_view_collapse_row(sc_tree, path);
			else
				gtk_tree_view_expand_row(sc_tree, path, FALSE);
		}
	}
}


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



