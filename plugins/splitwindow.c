/*
 *      splitview.c
 *
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "geany.h"
#include <glib/gi18n.h>
#include "Scintilla.h"
#include "ScintillaWidget.h"
#include "SciLexer.h"

#include "ui_utils.h"
#include "document.h"
#include "editor.h"
#include "plugindata.h"
#include "pluginmacros.h"


PLUGIN_VERSION_CHECK(76)
PLUGIN_SET_INFO(_("Split Window"), _("Splits the editor view into two windows."),
	"0.1", _("The Geany developer team"))


GeanyData *geany_data;
GeanyFunctions *geany_functions;

enum Command
{
	ITEM_SPLIT_HORIZONTAL,
	/* ITEM_SPLIT_VERTICAL, */
	ITEM_UNSPLIT,
	ITEM_COUNT
};

static GtkWidget *menu_items_array[ITEM_COUNT];
static GtkWidget **menu_items = menu_items_array;	/* avoid gcc warning when casting to (gpointer*) */
static enum Command plugin_state;
static GeanyEditor *our_editor = NULL;


static gint sci_get_value(ScintillaObject *sci, gint message_id, gint param)
{
	return p_sci->send_message(sci, message_id, param, 0);
}


static void set_styles(ScintillaObject *oldsci, ScintillaObject *newsci)
{
	gint style_id;

	for (style_id = 0; style_id <= 127; style_id++)
	{
		gint val;

		val = sci_get_value(oldsci, SCI_STYLEGETFORE, style_id);
		p_sci->send_message(newsci, SCI_STYLESETFORE, style_id, val);
		val = sci_get_value(oldsci, SCI_STYLEGETBACK, style_id);
		p_sci->send_message(newsci, SCI_STYLESETBACK, style_id, val);
		val = sci_get_value(oldsci, SCI_STYLEGETBOLD, style_id);
		p_sci->send_message(newsci, SCI_STYLESETBOLD, style_id, val);
		val = sci_get_value(oldsci, SCI_STYLEGETITALIC, style_id);
		p_sci->send_message(newsci, SCI_STYLESETITALIC, style_id, val);
	}
}


static void sci_set_font(ScintillaObject *sci, gint style, const gchar *font,
	G_GNUC_UNUSED gint size)
{
	p_sci->send_message(sci, SCI_STYLESETFONT, style, (sptr_t) font);
	p_sci->send_message(sci, SCI_STYLESETSIZE, style, size);
}


static void set_font(ScintillaObject *sci, const gchar *font_name, gint size)
{
	gint style;

	for (style = 0; style <= 127; style++)
		sci_set_font(sci, style, font_name, size);
	/* line number and braces */
	sci_set_font(sci, STYLE_LINENUMBER, font_name, size);
	sci_set_font(sci, STYLE_BRACELIGHT, font_name, size);
	sci_set_font(sci, STYLE_BRACEBAD, font_name, size);
}


/* TODO: maybe use SCI_STYLEGET(FONT|SIZE) instead */
static void update_font(ScintillaObject *sci, const gchar *font_name, gint size)
{
	gchar *fname;
	PangoFontDescription *font_desc;

	g_return_if_fail(font_name != NULL);

	font_desc = pango_font_description_from_string(font_name);

	fname = g_strdup_printf("!%s", pango_font_description_get_family(font_desc));
	size = pango_font_description_get_size(font_desc) / PANGO_SCALE;

	set_font(sci, fname, size);

	g_free(fname);
	pango_font_description_free(font_desc);
}


/* line numbers visibility */
static void set_line_numbers(ScintillaObject * sci, gboolean set, gint extra_width)
{
	if (set)
	{
		gchar tmp_str[15];
		gint len = p_sci->send_message(sci, SCI_GETLINECOUNT, 0, 0);
		gint width;
		g_snprintf(tmp_str, 15, "_%d%d", len, extra_width);
		width = p_sci->send_message(sci, SCI_TEXTWIDTH, STYLE_LINENUMBER, (sptr_t) tmp_str);
		p_sci->send_message(sci, SCI_SETMARGINWIDTHN, 0, width);
		p_sci->send_message(sci, SCI_SETMARGINSENSITIVEN, 0, FALSE); /* use default behaviour */
	}
	else
	{
		p_sci->send_message(sci, SCI_SETMARGINWIDTHN, 0, 0 );
	}
}


static void update_view(ScintillaObject *sci)
{
	ScintillaObject *current = p_document->get_current()->editor->sci;
	gint lexer;

	update_font(sci, geany->interface_prefs->editor_font, 0);

	lexer = p_sci->send_message(current, SCI_GETLEXER, 0, 0);
	p_sci->send_message(sci, SCI_SETLEXER, lexer, 0);
	set_styles(current, sci);

	set_line_numbers(sci, TRUE, 0);
	p_sci->send_message(sci, SCI_SETMARGINWIDTHN, 1, 0 ); /* hide marker margin */
}


static void set_state(enum Command id)
{
	gtk_widget_set_sensitive(menu_items[ITEM_SPLIT_HORIZONTAL],
		id != ITEM_SPLIT_HORIZONTAL);
	gtk_widget_set_sensitive(menu_items[ITEM_UNSPLIT],
		id != ITEM_UNSPLIT);
		
	plugin_state = id;
}


static void on_split_view(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *parent = gtk_widget_get_parent(notebook);
	GtkWidget *pane;
	GeanyDocument *doc = p_document->get_current();
	ScintillaObject *sci;
	gpointer sdoc;

	set_state(ITEM_SPLIT_HORIZONTAL);
	
	g_return_if_fail(doc);
	g_return_if_fail(our_editor == NULL);

	/* reparent document notebook */
	g_object_ref(notebook);
	gtk_container_remove(GTK_CONTAINER(parent), notebook);
	pane = gtk_hpaned_new();
	gtk_container_add(GTK_CONTAINER(parent), pane);
	gtk_paned_add1(GTK_PANED(pane), notebook);
	g_object_unref(notebook);

	sci = doc->editor->sci;
	sdoc = (gpointer) p_sci->send_message(sci, SCI_GETDOCPOINTER, 0, 0);
	our_editor = p_editor->create(doc);
	sci = our_editor->sci;
	p_sci->send_message(sci, SCI_SETDOCPOINTER, 0, GPOINTER_TO_INT(sdoc));
	update_view(sci);
	gtk_paned_add2(GTK_PANED(pane), GTK_WIDGET(sci));
	gtk_widget_show_all(pane);
}


static void on_unsplit(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *pane = gtk_widget_get_parent(notebook);
	GtkWidget *parent = gtk_widget_get_parent(pane);

	set_state(ITEM_UNSPLIT);
	
	g_return_if_fail(our_editor);

	/* reparent document notebook */
	g_object_ref(notebook);
	gtk_container_remove(GTK_CONTAINER(pane), notebook);
	gtk_widget_destroy(pane);
	p_editor->destroy(our_editor);
	our_editor = NULL;
	gtk_container_add(GTK_CONTAINER(parent), notebook);
	g_object_unref(notebook);
}


static void foreach_pointer(gpointer *items, gsize count, GFunc func, gpointer user_data)
{
	gsize i;
	
	for (i = 0; i < count; i++)
	{
		func(items[i], user_data);
	}
}


static void add_menu_item(gpointer data, gpointer user_data)
{
	GtkWidget *item = data;
	
	gtk_widget_show(item);
	gtk_menu_append(geany_data->main_widgets->tools_menu, item);
}


void plugin_init(GeanyData *data)
{
	GtkWidget *item;
	
	menu_items[ITEM_SPLIT_HORIZONTAL] = item =
		gtk_menu_item_new_with_mnemonic(_("Split _Horizontally"));
	g_signal_connect(item, "activate", G_CALLBACK(on_split_view), NULL);
	
	menu_items[ITEM_UNSPLIT] = item =
		gtk_menu_item_new_with_mnemonic(_("_Unsplit"));
	g_signal_connect(item, "activate", G_CALLBACK(on_unsplit), NULL);
	
	foreach_pointer((gpointer*)menu_items, ITEM_COUNT, add_menu_item, NULL);

	set_state(ITEM_UNSPLIT);
}


void plugin_cleanup()
{
	if (plugin_state != ITEM_UNSPLIT)
		on_unsplit(NULL, NULL);
	
	foreach_pointer((gpointer*)menu_items, ITEM_COUNT, (GFunc)gtk_widget_destroy, NULL);
}
