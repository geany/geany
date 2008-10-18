/*
 *      splitwindow.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *
 * $Id$
 */

/* Split Window plugin. */

#include "geany.h"
#include "support.h"
#include "Scintilla.h"
#include "ScintillaWidget.h"
#include "SciLexer.h"

#include "ui_utils.h"
#include "document.h"
#include "editor.h"
#include "plugindata.h"
#include "pluginmacros.h"


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)
PLUGIN_SET_INFO(_("Split Window"), _("Splits the editor view into two windows."),
	"0.1", _("The Geany developer team"))


GeanyData		*geany_data;
GeanyFunctions	*geany_functions;


enum State
{
	STATE_SPLIT_HORIZONTAL,
	/* STATE_SPLIT_VERTICAL, */
	STATE_UNSPLIT,
	STATE_COUNT
};

static struct
{
	GtkWidget *main;
	GtkWidget *horizontal;
	GtkWidget *unsplit;
}
menu_items;

static enum State plugin_state;


typedef struct EditWindow
{
	GeanyEditor		*editor;	/* original editor for split view */
	ScintillaObject	*sci;		/* new editor widget */
	GtkWidget		*vbox;
	GtkWidget		*name_label;
}
EditWindow;

static EditWindow edit_window = {NULL, NULL, NULL, NULL};


static void on_unsplit(GtkMenuItem *menuitem, gpointer user_data);


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
	gint size)
{
	p_sci->send_message(sci, SCI_STYLESETFONT, style, (sptr_t) font);
	p_sci->send_message(sci, SCI_STYLESETSIZE, style, size);
}


static void update_font(ScintillaObject *current, ScintillaObject *sci)
{
	gint style_id;
	gint size;
	gchar font_name[1024]; /* should be big enough */

	p_sci->send_message(current, SCI_STYLEGETFONT, 0, (sptr_t)font_name);
	size = sci_get_value(current, SCI_STYLEGETSIZE, 0);

	for (style_id = 0; style_id <= 127; style_id++)
	{
		sci_set_font(sci, style_id, font_name, size);
	}

	/* line number and braces */
	sci_set_font(sci, STYLE_LINENUMBER, font_name, size);
	sci_set_font(sci, STYLE_BRACELIGHT, font_name, size);
	sci_set_font(sci, STYLE_BRACEBAD, font_name, size);
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


static void sync_to_current(ScintillaObject *sci, ScintillaObject *current)
{
	gpointer sdoc;
	gint lexer;
	gint pos;

	/* set the new sci widget to view the existing Scintilla document */
	sdoc = (gpointer) p_sci->send_message(current, SCI_GETDOCPOINTER, 0, 0);
	p_sci->send_message(sci, SCI_SETDOCPOINTER, 0, GPOINTER_TO_INT(sdoc));

	update_font(current, sci);
	lexer = p_sci->send_message(current, SCI_GETLEXER, 0, 0);
	p_sci->send_message(sci, SCI_SETLEXER, lexer, 0);
	set_styles(current, sci);

	pos = p_sci->get_current_position(current);
	p_sci->set_current_position(sci, pos, TRUE);

	/* override some defaults */
	set_line_numbers(sci, TRUE, 0);
	p_sci->send_message(sci, SCI_SETMARGINWIDTHN, 1, 0 ); /* hide marker margin */
}


static void set_editor(EditWindow *editwin, GeanyEditor *editor)
{
	editwin->editor = editor;

	/* first destroy any widget, otherwise its signals will have an
	 * invalid document as user_data */
	if (editwin->sci != NULL)
		gtk_widget_destroy(GTK_WIDGET(editwin->sci));

	editwin->sci = p_editor->create_widget(editor);
	gtk_widget_show(GTK_WIDGET(editwin->sci));
	gtk_container_add(GTK_CONTAINER(editwin->vbox), GTK_WIDGET(editwin->sci));

	sync_to_current(editwin->sci, editor->sci);

	gtk_label_set_text(GTK_LABEL(editwin->name_label), DOC_FILENAME(editor->document));
}


static void set_state(enum State id)
{
	gtk_widget_set_sensitive(menu_items.horizontal,
		id != STATE_SPLIT_HORIZONTAL);
	gtk_widget_set_sensitive(menu_items.unsplit,
		id != STATE_UNSPLIT);

	plugin_state = id;
}


static GtkWidget *create_tool_button(const gchar *label, const gchar *stock_id)
{
	GtkToolItem *item;
	GtkTooltips *tooltips = GTK_TOOLTIPS(p_support->lookup_widget(
		geany->main_widgets->window, "tooltips"));

	item = gtk_tool_button_new(NULL, label);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(item), stock_id);
	gtk_tool_item_set_tooltip(item, tooltips, label, NULL);

	return GTK_WIDGET(item);
}


static void on_refresh(void)
{
	GeanyDocument *doc = p_document->get_current();

	g_return_if_fail(doc);
	g_return_if_fail(edit_window.sci);

	set_editor(&edit_window, doc->editor);
}


/* avoid adding new strings which are the same but without a leading underscore */
static const gchar *after_underscore(const gchar *str)
{
	const gchar *u = g_strstr_len(str, -1, "_");

	if (u)
		return ++u;
	else
		return str;
}


static GtkWidget *create_toolbar(void)
{
	GtkWidget *toolbar, *item;
	GtkToolItem *tool_item;

	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	item = (GtkWidget*)gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	g_signal_connect(item, "clicked", G_CALLBACK(on_refresh), NULL);

	tool_item = gtk_tool_item_new();
	gtk_tool_item_set_expand(tool_item, TRUE);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(tool_item));

	item = gtk_label_new(NULL);
	gtk_label_set_ellipsize(GTK_LABEL(item), PANGO_ELLIPSIZE_START);
	gtk_container_add(GTK_CONTAINER(tool_item), item);
	edit_window.name_label = item;

	item = create_tool_button(after_underscore(_("_Unsplit")), GTK_STOCK_CLOSE);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	g_signal_connect(item, "clicked", G_CALLBACK(on_unsplit), NULL);

	return toolbar;
}


static void on_split_view(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *parent = gtk_widget_get_parent(notebook);
	GtkWidget *pane, *toolbar, *box;
	GeanyDocument *doc = p_document->get_current();
	gint width = notebook->allocation.width / 2;

	set_state(STATE_SPLIT_HORIZONTAL);

	g_return_if_fail(doc);
	g_return_if_fail(edit_window.editor == NULL);

	/* temporarily put document notebook in main vbox (scintilla widgets must stay
	 * in a visible parent window, otherwise there are X selection and scrollbar issues) */
	gtk_widget_reparent(notebook,
		p_support->lookup_widget(geany->main_widgets->window, "vbox1"));

	pane = gtk_hpaned_new();
	gtk_container_add(GTK_CONTAINER(parent), pane);
	gtk_widget_reparent(notebook, pane);

	box = gtk_vbox_new(FALSE, 0);
	toolbar = create_toolbar();
	gtk_box_pack_start(GTK_BOX(box), toolbar, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(pane), box);
	edit_window.vbox = box;

	set_editor(&edit_window, doc->editor);

	gtk_paned_set_position(GTK_PANED(pane), width);
	gtk_widget_show_all(pane);
}


static void on_unsplit(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *pane = gtk_widget_get_parent(notebook);
	GtkWidget *parent = gtk_widget_get_parent(pane);

	set_state(STATE_UNSPLIT);

	g_return_if_fail(edit_window.editor);

	/* temporarily put document notebook in main vbox (scintilla widgets must stay
	 * in a visible parent window, otherwise there are X selection and scrollbar issues) */
	gtk_widget_reparent(notebook,
		p_support->lookup_widget(geany->main_widgets->window, "vbox1"));

	gtk_widget_destroy(pane);
	edit_window.editor = NULL;
	edit_window.sci = NULL;
	gtk_widget_reparent(notebook, parent);
}


void plugin_init(GeanyData *data)
{
	GtkWidget *item, *menu;

	menu_items.main = item = gtk_menu_item_new_with_mnemonic(_("_Split Window"));
	gtk_menu_append(geany_data->main_widgets->tools_menu, item);
	p_ui->add_document_sensitive(item);

	menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items.main), menu);

	menu_items.horizontal = item =
		gtk_menu_item_new_with_mnemonic(_("_Horizontally"));
	g_signal_connect(item, "activate", G_CALLBACK(on_split_view), NULL);
	gtk_menu_append(menu, item);

	menu_items.unsplit = item =
		gtk_menu_item_new_with_mnemonic(_("_Unsplit"));
	g_signal_connect(item, "activate", G_CALLBACK(on_unsplit), NULL);
	gtk_menu_append(menu, item);

	gtk_widget_show_all(menu_items.main);

	set_state(STATE_UNSPLIT);
}


static void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
    /* remove the split window because the document won't exist anymore */
	if (doc->editor == edit_window.editor)
		on_unsplit(NULL, NULL);
}


static void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	/* update filename */
	if (doc->editor == edit_window.editor)
		gtk_label_set_text(GTK_LABEL(edit_window.name_label), DOC_FILENAME(doc));
}


PluginCallback plugin_callbacks[] =
{
    { "document-close", (GCallback) &on_document_close, FALSE, NULL },
    { "document-save", (GCallback) &on_document_save, FALSE, NULL },
    { NULL, NULL, FALSE, NULL }
};


void plugin_cleanup()
{
	if (plugin_state != STATE_UNSPLIT)
		on_unsplit(NULL, NULL);

	gtk_widget_destroy(menu_items.main);
}
