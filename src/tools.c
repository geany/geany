/*
 *      tools.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
 *      Copyright 2006 Nick Treleaven <nick.treleaven@btinternet.com>
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

#include "support.h"
#include "document.h"
#include "sciwrappers.h"
#include "ui_utils.h"

enum
{
	COLUMN_CHARACTER,
	COLUMN_HTML_NAME,
	N_COLUMNS
};

static GtkWidget *special_characters_dialog = NULL;
static GtkTreeStore *special_characters_store = NULL;
static GtkTreeView *special_characters_tree = NULL;

static void on_tools_show_dialog_insert_special_chars_response
		(GtkDialog *dialog, gint response, gpointer user_data);
static void on_special_characters_tree_row_activated
		(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data);
static void special_characters_fill_store(GtkTreeStore *store);
static gboolean special_characters_insert(GtkTreeModel *model, GtkTreeIter *iter);



void tools_show_dialog_insert_special_chars()
{
	if (special_characters_dialog == NULL)
	{
		gint height;
		GtkCellRenderer *renderer;
		GtkTreeViewColumn *column;
		GtkWidget *swin, *vbox, *label;

		special_characters_dialog = gtk_dialog_new_with_buttons(
					_("Special characters"), GTK_WINDOW(app->window),
					GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					_("_Insert"), GTK_RESPONSE_OK, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(special_characters_dialog));
		gtk_box_set_spacing(GTK_BOX(vbox), 6);

		height = GEANY_WINDOW_MINIMAL_HEIGHT;
		gtk_window_set_default_size(GTK_WINDOW(special_characters_dialog), height * 0.8, height);
		gtk_dialog_set_default_response(GTK_DIALOG(special_characters_dialog), GTK_RESPONSE_CANCEL);

		label = gtk_label_new(_("Choose a special character from the list below and double click on it or use the button to insert it at the current cursor position."));
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

		special_characters_tree = GTK_TREE_VIEW(gtk_tree_view_new());
		//g_object_set(tree, "vertical-separator", 6, NULL);

		special_characters_store = gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(special_characters_tree),
								GTK_TREE_MODEL(special_characters_store));

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
								_("Character"), renderer, "text", COLUMN_CHARACTER, NULL);
		gtk_tree_view_column_set_resizable(column, TRUE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(special_characters_tree), column);

		renderer = gtk_cell_renderer_text_new();
		column = gtk_tree_view_column_new_with_attributes(
								_("HTML (name)"), renderer, "text", COLUMN_HTML_NAME, NULL);
		gtk_tree_view_column_set_resizable(column, TRUE);
		gtk_tree_view_append_column(GTK_TREE_VIEW(special_characters_tree), column);

		swin = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin), GTK_POLICY_AUTOMATIC,
			GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport(
					GTK_SCROLLED_WINDOW(swin), GTK_WIDGET(special_characters_tree));

		gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

		g_signal_connect((gpointer) special_characters_tree, "row-activated",
					G_CALLBACK(on_special_characters_tree_row_activated), NULL);

		g_signal_connect((gpointer) special_characters_dialog, "response",
					G_CALLBACK(on_tools_show_dialog_insert_special_chars_response), NULL);

		special_characters_fill_store(special_characters_store);

		gtk_tree_view_expand_all(special_characters_tree);
	}
	gtk_widget_show_all(special_characters_dialog);
}


// fill the tree model with data
static void special_characters_fill_store(GtkTreeStore *store)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	guint i;

	gchar *chars[][2] =
		{
			{ N_("HTML characters"), NULL },
			{ "\"", "&quot;" },
			{ "&", "&amp;" },
			{ "<", "&lt;" },
			{ ">", "&gt;" },

			{ N_("ISO 8859-1 characters"), NULL },
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
			/// TODO add the symbols from http://de.selfhtml.org/html/referenz/zeichen.htm
			{ N_("Greek characters"), NULL },
			{ N_("Mathematical characters"), NULL },
			{ N_("Technical characters"), NULL },
			{ N_("Arrow characters"), NULL },
			{ N_("Punctuation characters"), NULL },
			{ N_("Miscellaneous characters"), NULL }
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
static gboolean special_characters_insert(GtkTreeModel *model, GtkTreeIter *iter)
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


static void on_tools_show_dialog_insert_special_chars_response(GtkDialog *dialog, gint response,
														gpointer user_data)
{
	if (response == GTK_RESPONSE_OK)
	{
		GtkTreeSelection *selection;
		GtkTreeModel *model;
		GtkTreeIter iter;

		selection = gtk_tree_view_get_selection(special_characters_tree);

		if (gtk_tree_selection_get_selected(selection, &model, &iter))
		{
			// only hide dialog if selection was not a category
			if (special_characters_insert(model, &iter))
				gtk_widget_hide(GTK_WIDGET(dialog));
		}
	}
	else
		gtk_widget_hide(GTK_WIDGET(dialog));
}


static void on_special_characters_tree_row_activated(GtkTreeView *treeview, GtkTreePath *path,
											  GtkTreeViewColumn *col, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model = GTK_TREE_MODEL(special_characters_store);

	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		// only hide dialog if selection was not a category
		if (special_characters_insert(model, &iter))
			gtk_widget_hide(special_characters_dialog);
		else
		{ // double click on a category to toggle the expand or collapse it
			/// TODO
		}

	}
}
