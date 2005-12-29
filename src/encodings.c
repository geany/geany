/*
 *      encodings.c - this file is part of Geany, a fast and lightweight IDE
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
 */


/*
 * Modified by the gedit Team, 2002. See the gedit AUTHORS file for a
 * list of people on the gedit Team.
 * See the gedit ChangeLog files for a list of changes.
 */
 /* Stolen from anjuta */

#include <string.h>

#include "geany.h"
#include "utils.h"
#include "support.h"
#include "msgwindow.h"
#include "encodings.h"

struct _GeanyEncoding
{
  gint   idx;
  gchar *charset;
  gchar *name;
};


/*
 * The original versions of the following tables are taken from profterm
 *
 * Copyright (C) 2002 Red Hat, Inc.
 */

typedef enum
{

  GEANY_ENCODING_ISO_8859_1,
  GEANY_ENCODING_ISO_8859_2,
  GEANY_ENCODING_ISO_8859_3,
  GEANY_ENCODING_ISO_8859_4,
  GEANY_ENCODING_ISO_8859_5,
  GEANY_ENCODING_ISO_8859_6,
  GEANY_ENCODING_ISO_8859_7,
  GEANY_ENCODING_ISO_8859_8,
  GEANY_ENCODING_ISO_8859_8_I,
  GEANY_ENCODING_ISO_8859_9,
  GEANY_ENCODING_ISO_8859_10,
  GEANY_ENCODING_ISO_8859_13,
  GEANY_ENCODING_ISO_8859_14,
  GEANY_ENCODING_ISO_8859_15,
  GEANY_ENCODING_ISO_8859_16,

  GEANY_ENCODING_UTF_7,
  GEANY_ENCODING_UTF_16,
  GEANY_ENCODING_UCS_2,
  GEANY_ENCODING_UCS_4,

  GEANY_ENCODING_ARMSCII_8,
  GEANY_ENCODING_BIG5,
  GEANY_ENCODING_BIG5_HKSCS,
  GEANY_ENCODING_CP_866,

  GEANY_ENCODING_EUC_JP,
  GEANY_ENCODING_EUC_KR,
  GEANY_ENCODING_EUC_TW,

  GEANY_ENCODING_GB18030,
  GEANY_ENCODING_GB2312,
  GEANY_ENCODING_GBK,
  GEANY_ENCODING_GEOSTD8,
  GEANY_ENCODING_HZ,

  GEANY_ENCODING_IBM_850,
  GEANY_ENCODING_IBM_852,
  GEANY_ENCODING_IBM_855,
  GEANY_ENCODING_IBM_857,
  GEANY_ENCODING_IBM_862,
  GEANY_ENCODING_IBM_864,

  GEANY_ENCODING_ISO_2022_JP,
  GEANY_ENCODING_ISO_2022_KR,
  GEANY_ENCODING_ISO_IR_111,
  GEANY_ENCODING_JOHAB,
  GEANY_ENCODING_KOI8_R,
  GEANY_ENCODING_KOI8_U,

  GEANY_ENCODING_SHIFT_JIS,
  GEANY_ENCODING_TCVN,
  GEANY_ENCODING_TIS_620,
  GEANY_ENCODING_UHC,
  GEANY_ENCODING_VISCII,

  GEANY_ENCODING_WINDOWS_1250,
  GEANY_ENCODING_WINDOWS_1251,
  GEANY_ENCODING_WINDOWS_1252,
  GEANY_ENCODING_WINDOWS_1253,
  GEANY_ENCODING_WINDOWS_1254,
  GEANY_ENCODING_WINDOWS_1255,
  GEANY_ENCODING_WINDOWS_1256,
  GEANY_ENCODING_WINDOWS_1257,
  GEANY_ENCODING_WINDOWS_1258,

  GEANY_ENCODING_LAST

} GeanyEncodingIndex;

#undef _
#define _(String) String
static GeanyEncoding encodings [] = {

  { GEANY_ENCODING_ISO_8859_1,
    "ISO-8859-1", "Western" },
  { GEANY_ENCODING_ISO_8859_2,
   "ISO-8859-2", "Central European" },
  { GEANY_ENCODING_ISO_8859_3,
    "ISO-8859-3", "South European" },
  { GEANY_ENCODING_ISO_8859_4,
    "ISO-8859-4", "Baltic" },
  { GEANY_ENCODING_ISO_8859_5,
    "ISO-8859-5", "Cyrillic" },
  { GEANY_ENCODING_ISO_8859_6,
    "ISO-8859-6", "Arabic" },
  { GEANY_ENCODING_ISO_8859_7,
    "ISO-8859-7", "Greek" },
  { GEANY_ENCODING_ISO_8859_8,
    "ISO-8859-8", "Hebrew Visual" },
  { GEANY_ENCODING_ISO_8859_8_I,
    "ISO-8859-8-I", "Hebrew" },
  { GEANY_ENCODING_ISO_8859_9,
    "ISO-8859-9", "Turkish" },
  { GEANY_ENCODING_ISO_8859_10,
    "ISO-8859-10", "Nordic" },
  { GEANY_ENCODING_ISO_8859_13,
    "ISO-8859-13", "Baltic" },
  { GEANY_ENCODING_ISO_8859_14,
    "ISO-8859-14", "Celtic" },
  { GEANY_ENCODING_ISO_8859_15,
    "ISO-8859-15", "Western" },
  { GEANY_ENCODING_ISO_8859_16,
    "ISO-8859-16", "Romanian" },

  { GEANY_ENCODING_UTF_7,
    "UTF-7", "Unicode" },
  { GEANY_ENCODING_UTF_16,
    "UTF-16", "Unicode" },
  { GEANY_ENCODING_UCS_2,
    "UCS-2", "Unicode" },
  { GEANY_ENCODING_UCS_4,
    "UCS-4", "Unicode" },

  { GEANY_ENCODING_ARMSCII_8,
    "ARMSCII-8", "Armenian" },
  { GEANY_ENCODING_BIG5,
    "BIG5", "Chinese Traditional" },
  { GEANY_ENCODING_BIG5_HKSCS,
    "BIG5-HKSCS", "Chinese Traditional" },
  { GEANY_ENCODING_CP_866,
    "CP866", "Cyrillic/Russian" },

  { GEANY_ENCODING_EUC_JP,
    "EUC-JP", "Japanese" },
  { GEANY_ENCODING_EUC_KR,
    "EUC-KR", "Korean" },
  { GEANY_ENCODING_EUC_TW,
    "EUC-TW", "Chinese Traditional" },

  { GEANY_ENCODING_GB18030,
    "GB18030", "Chinese Simplified" },
  { GEANY_ENCODING_GB2312,
    "GB2312", "Chinese Simplified" },
  { GEANY_ENCODING_GBK,
    "GBK", "Chinese Simplified" },
  { GEANY_ENCODING_GEOSTD8,
    "GEORGIAN-ACADEMY", "Georgian" }, /* FIXME GEOSTD8 ? */
  { GEANY_ENCODING_HZ,
    "HZ", "Chinese Simplified" },

  { GEANY_ENCODING_IBM_850,
    "IBM850", "Western" },
  { GEANY_ENCODING_IBM_852,
    "IBM852", "Central European" },
  { GEANY_ENCODING_IBM_855,
    "IBM855", "Cyrillic" },
  { GEANY_ENCODING_IBM_857,
    "IBM857", "Turkish" },
  { GEANY_ENCODING_IBM_862,
    "IBM862", "Hebrew" },
  { GEANY_ENCODING_IBM_864,
    "IBM864", "Arabic" },

  { GEANY_ENCODING_ISO_2022_JP,
    "ISO-2022-JP", "Japanese" },
  { GEANY_ENCODING_ISO_2022_KR,
    "ISO-2022-KR", "Korean" },
  { GEANY_ENCODING_ISO_IR_111,
    "ISO-IR-111", "Cyrillic" },
  { GEANY_ENCODING_JOHAB,
    "JOHAB", "Korean" },
  { GEANY_ENCODING_KOI8_R,
    "KOI8R", "Cyrillic" },
  { GEANY_ENCODING_KOI8_U,
    "KOI8U", "Cyrillic/Ukrainian" },

  { GEANY_ENCODING_SHIFT_JIS,
    "SHIFT_JIS", "Japanese" },
  { GEANY_ENCODING_TCVN,
    "TCVN", "Vietnamese" },
  { GEANY_ENCODING_TIS_620,
    "TIS-620", "Thai" },
  { GEANY_ENCODING_UHC,
    "UHC", "Korean" },
  { GEANY_ENCODING_VISCII,
    "VISCII", "Vietnamese" },

  { GEANY_ENCODING_WINDOWS_1250,
    "WINDOWS-1250", "Central European" },
  { GEANY_ENCODING_WINDOWS_1251,
    "WINDOWS-1251", "Cyrillic" },
  { GEANY_ENCODING_WINDOWS_1252,
    "WINDOWS-1252", "Western" },
  { GEANY_ENCODING_WINDOWS_1253,
    "WINDOWS-1253", "Greek" },
  { GEANY_ENCODING_WINDOWS_1254,
    "WINDOWS-1254", "Turkish" },
  { GEANY_ENCODING_WINDOWS_1255,
    "WINDOWS-1255", "Hebrew" },
  { GEANY_ENCODING_WINDOWS_1256,
    "WINDOWS-1256", "Arabic" },
  { GEANY_ENCODING_WINDOWS_1257,
    "WINDOWS-1257", "Baltic" },
  { GEANY_ENCODING_WINDOWS_1258,
    "WINDOWS-1258", "Vietnamese" }
};


static void encoding_lazy_init(void)
{
	static gboolean initialized = FALSE;
	gint i;

	if (initialized)
		return;

	g_return_if_fail(G_N_ELEMENTS(encodings) == GEANY_ENCODING_LAST);

	i = 0;
	while (i < GEANY_ENCODING_LAST)
	{
		g_return_if_fail(encodings[i].idx == i);

		/* Translate the names */
		encodings[i].name = _(encodings[i].name);

		++i;
    }

	initialized = TRUE;
}


const GeanyEncoding *encoding_get_from_charset(const gchar *charset)
{
	gint i;

	encoding_lazy_init ();

	i = 0;
	while (i < GEANY_ENCODING_LAST)
	{
		if (strcmp(charset, encodings[i].charset) == 0)
			return &encodings[i];

		++i;
	}

	return NULL;
}


const GeanyEncoding *encoding_get_from_index(gint index)
{
	g_return_val_if_fail(index >= 0, NULL);

	if (index >= GEANY_ENCODING_LAST)
		return NULL;

	encoding_lazy_init();

	return &encodings[index];
}


gchar *encoding_to_string(const GeanyEncoding* enc)
{
	g_return_val_if_fail(enc != NULL, NULL);
	g_return_val_if_fail(enc->name != NULL, NULL);
	g_return_val_if_fail(enc->charset != NULL, NULL);

	encoding_lazy_init();

    return g_strdup_printf("%s (%s)", enc->name, enc->charset);
}


const gchar *encoding_get_charset(const GeanyEncoding* enc)
{
/*	g_return_val_if_fail(enc != NULL, NULL);
	g_return_val_if_fail(enc->charset != NULL, NULL);
*/
	if (enc == NULL) return NULL;

	encoding_lazy_init();

	return enc->charset;
}


/* Encodings */
GList *encoding_get_encodings(GList *encoding_strings)
{
	GList *res = NULL;

	if (encoding_strings != NULL)
	{
		GList *tmp;
		const GeanyEncoding *enc;

		tmp = encoding_strings;

		while (tmp)
		{
		      const char *charset = tmp->data;

		      if (strcmp(charset, "current") == 0)
			      g_get_charset(&charset);

		      g_return_val_if_fail(charset != NULL, NULL);
		      enc = encoding_get_from_charset(charset);

		      if (enc != NULL)
				res = g_list_append(res, (gpointer)enc);

		      tmp = g_list_next(tmp);
		}
	}
	return res;
}


typedef struct
{
	GtkWidget *add_button;
	GtkWidget *remove_button;
	GtkWidget *up_button;
	GtkWidget *down_button;
	GtkWidget *supported_treeview;
	GtkWidget *stock_treeview;
} GeanyEncodingsDialog;


//static GeanyEncodingsDialog *encodings_dialog = NULL;


enum
{
	COLUMN_ENCODING_NAME = 0,
	COLUMN_ENCODING_INDEX,
	ENCODING_NUM_COLS
};


enum
{
	COLUMN_SUPPORTED_ENCODING_NAME = 0,
	COLUMN_SUPPORTED_ENCODING,
	SUPPORTED_ENCODING_NUM_COLS
};


/*
static GtkTreeModel *create_encodings_treeview_model(void)
{
	GtkListStore *store;
	GtkTreeIter iter;
	gint i;
	const GeanyEncoding* enc;

	// create list store
	store = gtk_list_store_new(ENCODING_NUM_COLS, G_TYPE_STRING, G_TYPE_INT);

	i = 0;
	while ((enc = encoding_get_from_index(i)) != NULL)
	{
		gchar *name;
		enc = encoding_get_from_index(i);
		name = encoding_to_string(enc);
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, COLUMN_ENCODING_NAME, name,
				    		COLUMN_ENCODING_INDEX, i, -1);
		g_free(name);
		++i;
	}
	return GTK_TREE_MODEL(store);
}

static void on_add_encodings(GtkButton *button)
{
	GValue value = {0, };
	const GeanyEncoding* enc;
	GSList *encs = NULL;

	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;

	selection =
		gtk_tree_view_get_selection(GTK_TREE_VIEW
									 (encodings_dialog->stock_treeview));
	g_return_if_fail(selection != NULL);

	model =	gtk_tree_view_get_model(GTK_TREE_VIEW
							 (encodings_dialog->stock_treeview));
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	if (gtk_tree_selection_iter_is_selected(selection, &iter))
	{
		gtk_tree_model_get_value(model, &iter,
								  COLUMN_ENCODING_INDEX, &value);
		enc = encoding_get_from_index(g_value_get_int(&value));
		g_return_if_fail(enc != NULL);
		encs = g_slist_prepend(encs, (gpointer)enc);

		g_value_unset(&value);
	}

	while (gtk_tree_model_iter_next(model, &iter))
	{
		if (gtk_tree_selection_iter_is_selected(selection, &iter))
		{
			gtk_tree_model_get_value(model, &iter,
				    COLUMN_ENCODING_INDEX, &value);

			enc = encoding_get_from_index(g_value_get_int (&value));
			g_return_if_fail (enc != NULL);

			encs = g_slist_prepend(encs, (gpointer)enc);

			g_value_unset(&value);
		}
	}

	if (encs != NULL)
	{
		GSList *node;
		model =	gtk_tree_view_get_model(GTK_TREE_VIEW
							 (encodings_dialog->supported_treeview));
		encs = g_slist_reverse(encs);
		node = encs;
		while (node)
		{
			const GeanyEncoding *enc;
			gchar *name;
			GtkTreeIter iter;
			enc = (const GeanyEncoding *) node->data;

			name = encoding_to_string(enc);

			gtk_list_store_append(GTK_LIST_STORE(model), &iter);
			gtk_list_store_set(GTK_LIST_STORE(model), &iter,
						COLUMN_SUPPORTED_ENCODING_NAME, name,
						COLUMN_SUPPORTED_ENCODING, enc,
						-1);
			g_free(name);

			node = g_slist_next(node);
		}
		g_slist_free(encs);
	}
}

static void on_remove_encodings(GtkButton *button)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeView *treeview;

	treeview = GTK_TREE_VIEW(encodings_dialog->supported_treeview);
	selection = gtk_tree_view_get_selection(treeview);
	if (selection &&
		gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
	}
}

static void on_up_encoding(GtkButton *button)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeView *treeview;

	treeview = GTK_TREE_VIEW(encodings_dialog->supported_treeview);
	selection = gtk_tree_view_get_selection(treeview);
	if (selection &&
		gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		GtkTreePath *path;

		path = gtk_tree_model_get_path(model, &iter);
		if (gtk_tree_path_prev(path))
		{
			GtkTreeIter prev_iter;
			gtk_tree_model_get_iter(model, &prev_iter, path);
			gtk_list_store_swap(GTK_LIST_STORE(model), &prev_iter, &iter);
		}
		gtk_tree_path_free(path);
	}
}

static void on_down_encoding(GtkButton *button)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreeView *treeview;

	treeview = GTK_TREE_VIEW(encodings_dialog->supported_treeview);
	selection = gtk_tree_view_get_selection(treeview);
	if (selection &&
		gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		GtkTreeIter next_iter = iter;
		if (gtk_tree_model_iter_next(model, &next_iter))
		{
			gtk_list_store_swap(GTK_LIST_STORE(model), &iter, &next_iter);
		}
	}
}

static void on_stock_selection_changed(GtkTreeSelection *selection)
{
	if (gtk_tree_selection_count_selected_rows(selection) > 0)
		gtk_widget_set_sensitive(encodings_dialog->add_button, TRUE);
	else
		gtk_widget_set_sensitive(encodings_dialog->add_button, FALSE);
}

static void on_supported_selection_changed(GtkTreeSelection *selection)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected(selection, &model, &iter) > 0)
	{
		GtkTreePath *path;
		gtk_widget_set_sensitive(encodings_dialog->remove_button, TRUE);
		path = gtk_tree_model_get_path(model, &iter);
		if (gtk_tree_path_prev(path))
			gtk_widget_set_sensitive(encodings_dialog->up_button, TRUE);
		else
			gtk_widget_set_sensitive(encodings_dialog->up_button, FALSE);
		gtk_tree_path_free(path);

		if (gtk_tree_model_iter_next(model, &iter))
			gtk_widget_set_sensitive(encodings_dialog->down_button, TRUE);
		else
			gtk_widget_set_sensitive(encodings_dialog->down_button, FALSE);
	}
	else
	{
		gtk_widget_set_sensitive(encodings_dialog->remove_button, FALSE);
		gtk_widget_set_sensitive(encodings_dialog->up_button, FALSE);
		gtk_widget_set_sensitive(encodings_dialog->down_button, FALSE);
	}
}


static gchar *get_property(void)
{
	GtkTreeView *treeview;
	GString *str;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean valid;
	gchar *value;

	treeview = GTK_TREE_VIEW(app->tagbar);

	str = g_string_new("");

	model =	gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
	valid = gtk_tree_model_get_iter_first(model, &iter);
	while (valid)
	{
		GeanyEncoding *enc;
		gtk_tree_model_get(model, &iter, COLUMN_SUPPORTED_ENCODING, &enc, -1);
		g_assert(enc != NULL);
		g_assert(enc->charset != NULL);
		str = g_string_append(str, enc->charset);
		str = g_string_append(str, " ");
		valid = gtk_tree_model_iter_next(model, &iter);
	}
	value = g_string_free(str, FALSE);
	return value;
}


static void set_property(const gchar *value)
{
	GtkTreeView *treeview;
	GtkTreeModel *model;
	GList *list, *node;

	treeview = GTK_TREE_VIEW(app->tagbar);
	model = gtk_tree_view_get_model(treeview);
	gtk_list_store_clear(GTK_LIST_STORE(model));

	if (!value || strlen(value) <= 0)
		return;

	// Fill the model
	list = utils_glist_from_string(value);
	node = list;
	while (node)
	{
		const GeanyEncoding *enc;
		gchar *name;
		GtkTreeIter iter;

		enc = encoding_get_from_charset((gchar *) node->data);
		name = encoding_to_string(enc);

		gtk_list_store_append(GTK_LIST_STORE(model), &iter);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter,
				    COLUMN_SUPPORTED_ENCODING_NAME, name,
					COLUMN_SUPPORTED_ENCODING, enc,
				    -1);
		g_free(name);

		node = g_list_next(node);
	}
	utils_glist_strings_free(list);
}
*/


void encodings_init(void)
{
	encoding_lazy_init();
/*	GtkWidget *add_button;
	GtkWidget *remove_button;
	GtkWidget *up_button;
	GtkWidget *down_button;
	GtkWidget *supported_treeview;
	GtkWidget *stock_treeview = msgwindow.tree_msg;
	GtkTreeModel *model;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	g_return_if_fail(encodings_dialog == NULL);

	// Create the Encodings preferences page
	gxml = glade_xml_new(PACKAGE_DATA_DIR"/glade/anjuta.glade",
						  "preferences_dialog_encodings",
						  NULL);
	anjuta_preferences_add_page(pref, gxml,
								"Encodings",
								"preferences-encodings.png");
	supported_treeview = glade_xml_get_widget(gxml, "supported_treeview");
	stock_treeview =  glade_xml_get_widget(gxml, "stock_treeview");
	add_button = glade_xml_get_widget(gxml, "add_button");
	remove_button = glade_xml_get_widget(gxml, "remove_button");
	up_button = glade_xml_get_widget(gxml, "up_button");
	down_button = glade_xml_get_widget(gxml, "down_button");

	// Add the encoding column for stock treeview
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Stock Encodings"),
													  cell, "text",
													  COLUMN_ENCODING_NAME,
													  NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(msgwindow.tree_msg), column);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(stock_treeview),
									 COLUMN_ENCODING_NAME);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(stock_treeview));
	g_return_if_fail(selection != NULL);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(selection), "changed",
					  G_CALLBACK(on_stock_selection_changed), NULL);

	model = create_encodings_treeview_model();
	gtk_tree_view_set_model(GTK_TREE_VIEW(stock_treeview), model);
	g_object_unref(model);

	// Add the encoding column for supported treeview
	cell = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes(_("Supported Encodings"),
								cell, "text", COLUMN_ENCODING_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(supported_treeview), column);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(supported_treeview),
									 COLUMN_ENCODING_NAME);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(supported_treeview));
	g_return_if_fail(selection != NULL);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
	g_signal_connect(G_OBJECT(selection), "changed",
					  G_CALLBACK(on_supported_selection_changed), NULL);

	// create list store
	model = GTK_TREE_MODEL(gtk_list_store_new(SUPPORTED_ENCODING_NUM_COLS,
												G_TYPE_STRING, G_TYPE_POINTER));
	gtk_tree_view_set_model(GTK_TREE_VIEW(supported_treeview), model);
	g_object_unref(model);

	g_signal_connect(G_OBJECT(add_button), "clicked",
					  G_CALLBACK(on_add_encodings), NULL);
	g_signal_connect(G_OBJECT(remove_button), "clicked",
					  G_CALLBACK(on_remove_encodings), NULL);
	g_signal_connect(G_OBJECT(up_button), "clicked",
					  G_CALLBACK(on_up_encoding), NULL);
	g_signal_connect(G_OBJECT(down_button), "clicked",
					  G_CALLBACK(on_down_encoding), NULL);

	gtk_widget_set_sensitive(add_button, FALSE);
	gtk_widget_set_sensitive(remove_button, FALSE);
	gtk_widget_set_sensitive(up_button, FALSE);
	gtk_widget_set_sensitive(down_button, FALSE);

	encodings_dialog = g_new0(GeanyEncodingsDialog, 1);
	encodings_dialog->add_button = add_button;
	encodings_dialog->remove_button = remove_button;
	encodings_dialog->up_button = up_button;
	encodings_dialog->down_button = down_button;
	encodings_dialog->supported_treeview = supported_treeview;
	encodings_dialog->stock_treeview = stock_treeview;
*/
}
