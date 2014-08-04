/*
 *      splitwindow.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Split Window plugin. */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#include "geanyplugin.h"
#include "gtkcompat.h"
#include <string.h>


PLUGIN_VERSION_CHECK(GEANY_API_VERSION)
PLUGIN_SET_INFO(_("Split Window"), _("Splits the editor view into two windows."),
	VERSION, _("The Geany developer team"))


GeanyData		*geany_data;
GeanyFunctions	*geany_functions;
GeanyPlugin		*geany_plugin;


/* Keybinding(s) */
enum
{
	KB_SPLIT_HORIZONTAL,
	KB_SPLIT_VERTICAL,
	KB_SPLIT_UNSPLIT,
	KB_SPLIT_SWITCH,
	KB_COUNT
};

enum State
{
	STATE_SPLIT_HORIZONTAL,
	STATE_SPLIT_VERTICAL,
	STATE_UNSPLIT,
	STATE_COUNT
};

static struct
{
	GtkWidget *main;
	GtkWidget *horizontal;
	GtkWidget *vertical;
	GtkWidget *unsplit;
}
menu_items;

static enum State plugin_state;


typedef struct EditWindow
{
	GeanyEditor		*editor;	/* original editor for split view */
	ScintillaObject	*sci;		/* new editor widget */
	GtkWidget		*notebook;
	GtkWidget		*name_label;
}
EditWindow;

static EditWindow edit_window = {NULL, NULL, NULL, NULL};


static void on_unsplit(GtkMenuItem *menuitem, gpointer user_data);
static void set_editor(EditWindow *editwin, GeanyEditor *editor);


/* line numbers visibility */
static void set_line_numbers(ScintillaObject * sci, gboolean set)
{
	if (set)
	{
		gchar tmp_str[15];
		gint len = scintilla_send_message(sci, SCI_GETLINECOUNT, 0, 0);
		gint width;

		g_snprintf(tmp_str, 15, "_%d", len);
		width = scintilla_send_message(sci, SCI_TEXTWIDTH, STYLE_LINENUMBER, (sptr_t) tmp_str);
		scintilla_send_message(sci, SCI_SETMARGINWIDTHN, 0, width);
		scintilla_send_message(sci, SCI_SETMARGINSENSITIVEN, 0, FALSE); /* use default behaviour */
	}
	else
	{
		scintilla_send_message(sci, SCI_SETMARGINWIDTHN, 0, 0);
	}
}


static void on_sci_notify(ScintillaObject *sci, gint param,
		SCNotification *nt, gpointer data)
{
	gint line;

	switch (nt->nmhdr.code)
	{
		/* adapted from editor.c: on_margin_click() */
		case SCN_MARGINCLICK:
			/* left click to marker margin toggles marker */
			if (nt->margin == 1)
			{
				gboolean set;
				gint marker = 1;

				line = sci_get_line_from_position(sci, nt->position);
				set = sci_is_marker_set_at_line(sci, line, marker);
				if (!set)
					sci_set_marker_at_line(sci, line, marker);
				else
					sci_delete_marker_at_line(sci, line, marker);
			}
			/* left click on the folding margin to toggle folding state of current line */
			if (nt->margin == 2)
			{
				line = sci_get_line_from_position(sci, nt->position);
				scintilla_send_message(sci, SCI_TOGGLEFOLD, line, 0);
			}
			break;

		default: break;
	}
}


static void sync_to_current(ScintillaObject *sci, ScintillaObject *current)
{
	gpointer sdoc;
	gint pos;

	/* set the new sci widget to view the existing Scintilla document */
	sdoc = (gpointer) scintilla_send_message(current, SCI_GETDOCPOINTER, 0, 0);
	scintilla_send_message(sci, SCI_SETDOCPOINTER, 0, (sptr_t) sdoc);

	highlighting_set_styles(sci, edit_window.editor->document->file_type);
	pos = sci_get_current_position(current);
	sci_set_current_position(sci, pos, TRUE);

	/* override some defaults */
	set_line_numbers(sci, geany->editor_prefs->show_linenumber_margin);
	/* marker margin */
	scintilla_send_message(sci, SCI_SETMARGINWIDTHN, 1,
		scintilla_send_message(current, SCI_GETMARGINWIDTHN, 1, 0));
	if (!geany->editor_prefs->folding)
		scintilla_send_message(sci, SCI_SETMARGINWIDTHN, 2, 0);
}


struct SciState
{
	struct
	{
		gint line, column;
	} scrolling;
	struct
	{
		GArray *points;
	} folding;
};


static void get_sci_scrolling(ScintillaObject *sci, gint *column, gint *line)
{
	gint pos;
	gint x = 0, y = 0;
	gint i;

	x += (gint) scintilla_send_message(sci, SCI_GETMARGINLEFT, 0, 0);
	for (i = 0; i < 5 /* scintilla has 5 margins */; i++)
		x += (gint) scintilla_send_message(sci, SCI_GETMARGINWIDTHN, (uptr_t) i, 0);

	pos = (gint) scintilla_send_message(sci, SCI_POSITIONFROMPOINT, (uptr_t) x, y);
	*line = sci_get_line_from_position(sci, pos);
	*column = sci_get_col_from_position(sci, pos);
}


static void set_sci_scrolling(ScintillaObject *sci, gint column, gint line)
{
	gint cur_line, cur_col;

	get_sci_scrolling(sci, &cur_col, &cur_line);
	line = (gint) scintilla_send_message(sci, SCI_VISIBLEFROMDOCLINE, (uptr_t) line, 0);
	cur_line = (gint) scintilla_send_message(sci, SCI_VISIBLEFROMDOCLINE, (uptr_t) cur_line, 0);
	scintilla_send_message(sci, SCI_LINESCROLL, (uptr_t) (column - cur_col), line - cur_line);
}


static GArray *get_sci_folding(ScintillaObject *sci)
{
	GArray *array;
	gint n_lines, i;

	n_lines = sci_get_line_count(sci);
	array = g_array_sized_new(FALSE, FALSE, 1, (guint) n_lines);

	for (i = 0; i < n_lines; i++)
	{
		gchar visible = (gchar) scintilla_send_message(sci, SCI_GETLINEVISIBLE, (uptr_t) i, 0);

		g_array_append_val(array, visible);
	}

	return array;
}


static void set_sci_folding(ScintillaObject *sci, GArray *folding)
{
	guint i;

	for (i = 0; i < folding->len; i++)
	{
		gchar visible = (gchar) scintilla_send_message(sci, SCI_GETLINEVISIBLE, i, 0);

		if (visible != g_array_index(folding, gchar, i))
			scintilla_send_message(sci, SCI_TOGGLEFOLD, i, 0);
	}
}


static void sci_state_init(struct SciState *state)
{
	state->scrolling.line = 0;
	state->scrolling.column = 0;
	state->folding.points = NULL;
}


static void sci_state_unset(struct SciState *state)
{
	g_array_free(state->folding.points, TRUE);
}


/* FIXME: include selection? */
static void save_sci_state(ScintillaObject *sci, struct SciState *state)
{
	/* scrolling */
	get_sci_scrolling(sci, &state->scrolling.column, &state->scrolling.line);

	/* folding */
	state->folding.points = get_sci_folding(sci);
}


static void apply_sci_state(ScintillaObject *sci, struct SciState *state)
{
	/* folding */
	set_sci_folding(sci, state->folding.points);

	/* scrolling */
	set_sci_scrolling(sci, state->scrolling.column, state->scrolling.line);
}


static void swap_panes(void)
{
	GtkWidget *pane = gtk_widget_get_parent(geany_data->main_widgets->notebook);
	GtkWidget *child1, *child2;
	GeanyDocument *real_doc = document_get_current();

	g_return_if_fail(edit_window.editor);
	g_return_if_fail(real_doc != NULL);

	child1 = gtk_paned_get_child1(GTK_PANED(pane));
	child2 = gtk_paned_get_child2(GTK_PANED(pane));

	g_object_ref(child1);
	g_object_ref(child2);
	gtk_container_remove(GTK_CONTAINER(pane), child1);
	gtk_container_remove(GTK_CONTAINER(pane), child2);

	/* now, swap displayed documents */
	if (real_doc)
	{
		GeanyDocument *view_doc = edit_window.editor->document;
		struct SciState real_state, view_state;

		sci_state_init(&real_state);
		sci_state_init(&view_state);

		save_sci_state(edit_window.sci, &view_state);
		save_sci_state(real_doc->editor->sci, &real_state);

		set_editor(&edit_window, real_doc->editor);
		apply_sci_state(edit_window.sci, &real_state);

		gtk_notebook_set_current_page(GTK_NOTEBOOK(geany_data->main_widgets->notebook),
				document_get_notebook_page(view_doc));
		apply_sci_state(view_doc->editor->sci, &view_state);

		sci_state_unset(&real_state);
		sci_state_unset(&view_state);
	}

	gtk_paned_add1(GTK_PANED(pane), child2);
	gtk_paned_add2(GTK_PANED(pane), child1);
	g_object_unref(child1);
	g_object_unref(child2);

	real_doc = document_get_current();
	if (DOC_VALID(real_doc))
		gtk_widget_grab_focus(GTK_WIDGET(real_doc->editor->sci));
}


static gboolean on_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	switch (event->type)
	{
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
		case GDK_KEY_PRESS:
		case GDK_KEY_RELEASE:
		{
			GeanyDocument *doc;

			swap_panes();

			doc = document_get_current();
			if (DOC_VALID(doc))
			{
				/* Swap the event's window with the other sci view */
				GdkEventAny *any_event = (GdkEventAny*) event;

				g_object_unref(any_event->window);
				any_event->window = gtk_widget_get_window(GTK_WIDGET(doc->editor->sci));
				g_object_ref(any_event->window);

				gtk_main_do_event(event);
			}

			return TRUE;
		}

		default: break;
	}

	return FALSE;
}


static void on_tab_close_button_clicked(GtkButton *button, GeanyDocument *doc)
{
	g_return_if_fail(doc && doc->is_valid);

	document_close(doc);
}


static void set_editor(EditWindow *editwin, GeanyEditor *editor)
{
	GtkWidget *box;
	gchar *display_name = document_get_basename_for_display(editor->document, -1);

	editwin->editor = editor;

	/* first destroy any widget, otherwise its signals will have an
	 * invalid document as user_data */
	if (editwin->sci != NULL)
		gtk_widget_destroy(GTK_WIDGET(editwin->sci));

	editwin->sci = editor_create_widget(editor);
	gtk_widget_show(GTK_WIDGET(editwin->sci));

	box = gtk_hbox_new(FALSE, 2);
	edit_window.name_label = gtk_label_new(display_name);
	gtk_box_pack_start(GTK_BOX(box), edit_window.name_label, FALSE, FALSE, 0);

	if (geany->file_prefs->show_tab_cross)
	{
		GtkWidget *image, *btn, *align;

		btn = gtk_button_new();
		gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
		gtk_button_set_focus_on_click(GTK_BUTTON(btn), FALSE);
		gtk_widget_set_name(btn, "geany-close-tab-button");

		image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
		gtk_container_add(GTK_CONTAINER(btn), image);

		align = gtk_alignment_new(1.0, 0.5, 0.0, 0.0);
		gtk_container_add(GTK_CONTAINER(align), btn);
		gtk_box_pack_start(GTK_BOX(box), align, TRUE, TRUE, 0);

		g_signal_connect(btn, "clicked", G_CALLBACK(on_tab_close_button_clicked), editor->document);
	}

	gtk_widget_show_all(box);
	gtk_notebook_append_page(GTK_NOTEBOOK(editwin->notebook), GTK_WIDGET(editwin->sci), box);

	sync_to_current(editwin->sci, editor->sci);

	/* for margin events */
	g_signal_connect(editwin->sci, "sci-notify",
			G_CALLBACK(on_sci_notify), NULL);
	g_signal_connect(editwin->sci, "event",
			G_CALLBACK(on_event), NULL);

	g_free(display_name);
}


static void set_state(enum State id)
{
	gtk_widget_set_sensitive(menu_items.horizontal,
		(id != STATE_SPLIT_HORIZONTAL) && (id != STATE_SPLIT_VERTICAL));
	gtk_widget_set_sensitive(menu_items.vertical,
		(id != STATE_SPLIT_HORIZONTAL) && (id != STATE_SPLIT_VERTICAL));
	gtk_widget_set_sensitive(menu_items.unsplit,
		id != STATE_UNSPLIT);

	plugin_state = id;
}


/* Create a GtkToolButton with stock icon, label and tooltip.
 * @param label can be NULL to use stock label text. @a label can contain underscores,
 * which will be removed.
 * @param tooltip can be NULL to use label text (useful for GTK_TOOLBAR_ICONS). */
static GtkWidget *ui_tool_button_new(const gchar *stock_id, const gchar *label, const gchar *tooltip)
{
	GtkToolItem *item;
	gchar *dupl = NULL;

	if (stock_id && !label)
	{
		label = ui_lookup_stock_label(stock_id);
	}
	dupl = utils_str_remove_chars(g_strdup(label), "_");
	label = dupl;

	item = gtk_tool_button_new(NULL, label);
	if (stock_id)
		gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(item), stock_id);

	if (!tooltip)
		tooltip = label;
	if (tooltip)
		gtk_widget_set_tooltip_text(GTK_WIDGET(item), tooltip);

	g_free(dupl);
	return GTK_WIDGET(item);
}


static void on_refresh(void)
{
	GeanyDocument *doc = document_get_current();

	g_return_if_fail(doc);
	g_return_if_fail(edit_window.sci);

	set_editor(&edit_window, doc->editor);
}


static void on_doc_menu_item_clicked(gpointer item, GeanyDocument *doc)
{
	if (doc->is_valid)
		set_editor(&edit_window, doc->editor);
}


static void on_doc_menu_show(GtkMenu *menu)
{
	/* clear the old menu items */
	gtk_container_foreach(GTK_CONTAINER(menu), (GtkCallback) gtk_widget_destroy, NULL);

	ui_menu_add_document_items(menu, edit_window.editor->document,
		G_CALLBACK(on_doc_menu_item_clicked));
}


static gboolean on_notebook_button_press_event(GtkWidget *widget, GdkEventButton *event,
	gpointer user_data)
{
	if (event->button == 3)
	{
		GtkWidget *item;
		GtkWidget *menu = gtk_menu_new();

		ui_menu_add_document_items(GTK_MENU(menu), edit_window.editor->document,
			G_CALLBACK(on_doc_menu_item_clicked));

		item = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		item = ui_image_menu_item_new(GTK_STOCK_JUMP_TO, _("Show the current document"));
		g_signal_connect(item, "activate", G_CALLBACK(on_refresh), NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		item = gtk_separator_menu_item_new();
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("_Unsplit"));
		g_signal_connect(item, "activate", G_CALLBACK(on_unsplit), NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

		gtk_widget_show_all(menu);
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

		return TRUE;
	}
	return FALSE;
}


static GtkWidget *create_toolbar(void)
{
	GtkWidget *toolbar, *item;
	GtkToolItem *tool_item;

	toolbar = gtk_toolbar_new();
	gtk_widget_set_name(toolbar, "geany-splitwindow-toolbar");
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), FALSE); /* not to shrink */

	tool_item = gtk_menu_tool_button_new(NULL, NULL);
	gtk_tool_button_set_stock_id(GTK_TOOL_BUTTON(tool_item), GTK_STOCK_JUMP_TO);
	item = (GtkWidget*)tool_item;
	gtk_widget_set_tooltip_text(item, _("Show the current document"));
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	g_signal_connect(item, "clicked", G_CALLBACK(on_refresh), NULL);

	item = gtk_menu_new();
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(tool_item), item);
	g_signal_connect(item, "show", G_CALLBACK(on_doc_menu_show), NULL);

	tool_item = gtk_tool_item_new();
	gtk_tool_item_set_expand(tool_item, TRUE);
	gtk_container_add(GTK_CONTAINER(toolbar), GTK_WIDGET(tool_item));

	item = ui_tool_button_new(GTK_STOCK_CLOSE, _("_Unsplit"), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	g_signal_connect(item, "clicked", G_CALLBACK(on_unsplit), NULL);

	return toolbar;
}


static void split_view(gboolean horizontal)
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *parent = gtk_widget_get_parent(notebook);
	GtkWidget *pane, *toolbar;
	GeanyDocument *doc = document_get_current();
	gint width = gtk_widget_get_allocated_width(notebook) / 2;
	gint height = gtk_widget_get_allocated_height(notebook) / 2;

	g_return_if_fail(doc);
	g_return_if_fail(edit_window.editor == NULL);

	set_state(horizontal ? STATE_SPLIT_HORIZONTAL : STATE_SPLIT_VERTICAL);

	g_object_ref(notebook);
	gtk_container_remove(GTK_CONTAINER(parent), notebook);

	pane = horizontal ? gtk_hpaned_new() : gtk_vpaned_new();
	gtk_container_add(GTK_CONTAINER(parent), pane);

	gtk_container_add(GTK_CONTAINER(pane), notebook);
	g_object_unref(notebook);

	edit_window.notebook = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(edit_window.notebook),
		geany->interface_prefs->tab_pos_editor);
	g_signal_connect_after(edit_window.notebook, "button-press-event",
		G_CALLBACK(on_notebook_button_press_event), NULL);

	toolbar = create_toolbar();
	gtk_widget_show_all(toolbar);
	/* FIXME: notebook's action widgets is a 2.20 feature... */
	gtk_notebook_set_action_widget(GTK_NOTEBOOK(edit_window.notebook), toolbar, GTK_PACK_END);
	gtk_container_add(GTK_CONTAINER(pane), edit_window.notebook);

	set_editor(&edit_window, doc->editor);

	if (horizontal)
	{
		gtk_paned_set_position(GTK_PANED(pane), width);
	}
	else
	{
		gtk_paned_set_position(GTK_PANED(pane), height);
	}
	gtk_widget_show_all(pane);
}


static void on_split_horizontally(GtkMenuItem *menuitem, gpointer user_data)
{
	split_view(TRUE);
}


static void on_split_vertically(GtkMenuItem *menuitem, gpointer user_data)
{
	split_view(FALSE);
}


static void on_unsplit(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *notebook = geany_data->main_widgets->notebook;
	GtkWidget *pane = gtk_widget_get_parent(notebook);
	GtkWidget *parent = gtk_widget_get_parent(pane);

	set_state(STATE_UNSPLIT);

	g_return_if_fail(edit_window.editor);

	g_object_ref(notebook);
	gtk_container_remove(GTK_CONTAINER(pane), notebook);

	gtk_widget_destroy(pane);
	edit_window.editor = NULL;
	edit_window.sci = NULL;

	gtk_container_add(GTK_CONTAINER(parent), notebook);
	g_object_unref(notebook);
}


static void kb_activate(guint key_id)
{
	switch (key_id)
	{
		case KB_SPLIT_HORIZONTAL:
			if (plugin_state == STATE_UNSPLIT)
				split_view(TRUE);
			break;
		case KB_SPLIT_VERTICAL:
			if (plugin_state == STATE_UNSPLIT)
				split_view(FALSE);
			break;
		case KB_SPLIT_UNSPLIT:
			if (plugin_state != STATE_UNSPLIT)
				on_unsplit(NULL, NULL);
			break;
		case KB_SPLIT_SWITCH:
			if (plugin_state != STATE_UNSPLIT)
				swap_panes();
			break;
	}
}


void plugin_init(GeanyData *data)
{
	GtkWidget *item, *menu;
	GeanyKeyGroup *key_group;

	/* Individual style for the toolbar */
#if GTK_CHECK_VERSION(3, 0, 0)
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(provider,
		"#geany-splitwindow-toolbar {\n"
		"	-GtkWidget-focus-padding: 0;\n"
		"	-GtkWidget-focus-line-width: 0;\n"
		"	-GtkToolbar-internal-padding: 0;\n"
		"	-GtkToolbar-shadow-type: none;\n"
		"	padding: 0;\n"
		"}\n",
		-1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
		GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(provider);
#else
	gtk_rc_parse_string(
		"style \"geany-splitwindow-toolbar-style\" {\n"
		"	GtkWidget::focus-padding = 0\n"
		"	GtkWidget::focus-line-width = 0\n"
		"	xthickness = 0\n"
		"	ythickness = 0\n"
		"	GtkToolbar::internal-padding = 0\n"
		"	GtkToolbar::shadow-type = none\n"
		"}\n"
		"widget \"*.geany-splitwindow-toolbar\" style \"geany-splitwindow-toolbar-style\""
	);
#endif

	menu_items.main = item = gtk_menu_item_new_with_mnemonic(_("_Split Window"));
	gtk_menu_shell_append(GTK_MENU_SHELL(geany_data->main_widgets->tools_menu), item);
	ui_add_document_sensitive(item);

	menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_items.main), menu);

	menu_items.horizontal = item =
		gtk_menu_item_new_with_mnemonic(_("_Side by Side"));
	g_signal_connect(item, "activate", G_CALLBACK(on_split_horizontally), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	menu_items.vertical = item =
		gtk_menu_item_new_with_mnemonic(_("_Top and Bottom"));
	g_signal_connect(item, "activate", G_CALLBACK(on_split_vertically), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	menu_items.unsplit = item =
		gtk_menu_item_new_with_mnemonic(_("_Unsplit"));
	g_signal_connect(item, "activate", G_CALLBACK(on_unsplit), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	gtk_widget_show_all(menu_items.main);

	set_state(STATE_UNSPLIT);

	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "split_window", KB_COUNT, NULL);
	keybindings_set_item(key_group, KB_SPLIT_HORIZONTAL, kb_activate,
		0, 0, "split_horizontal", _("Side by Side"), menu_items.horizontal);
	keybindings_set_item(key_group, KB_SPLIT_VERTICAL, kb_activate,
		0, 0, "split_vertical", _("Top and Bottom"), menu_items.vertical);
	keybindings_set_item(key_group, KB_SPLIT_UNSPLIT, kb_activate,
		0, 0, "split_unsplit", _("_Unsplit"), menu_items.unsplit);
	keybindings_set_item(key_group, KB_SPLIT_SWITCH, kb_activate,
		0, 0, "split_switch", _("_Switch Panes Focus"), NULL);
}


static gboolean do_select_current(gpointer data)
{
	GeanyDocument *doc;

	/* guard out for the unlikely case we get called after another unsplitting */
	if (plugin_state == STATE_UNSPLIT)
		return FALSE;

	doc = document_get_current();
	if (doc)
		set_editor(&edit_window, doc->editor);
	else
		on_unsplit(NULL, NULL);

	return FALSE;
}


static void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	if (doc->editor == edit_window.editor)
	{
		/* select current or unsplit in IDLE time, so the tab has changed */
		plugin_idle_add(geany_plugin, do_select_current, NULL);
	}
}


static void on_document_save(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
	/* update filename */
	if (doc->editor == edit_window.editor)
	{
		gchar *filename = document_get_basename_for_display(doc, -1);

		gtk_label_set_text(GTK_LABEL(edit_window.name_label), filename);
		g_free(filename);
	}
}


static void on_document_filetype_set(GObject *obj, GeanyDocument *doc,
	GeanyFiletype *filetype_old, gpointer user_data)
{
	/* update styles */
	if (edit_window.editor == doc->editor)
		sync_to_current(edit_window.sci, doc->editor->sci);
}


PluginCallback plugin_callbacks[] =
{
	{ "document-close", (GCallback) &on_document_close, FALSE, NULL },
	{ "document-save", (GCallback) &on_document_save, FALSE, NULL },
	{ "document-filetype-set", (GCallback) &on_document_filetype_set, FALSE, NULL },
	{ NULL, NULL, FALSE, NULL }
};


void plugin_cleanup(void)
{
	if (plugin_state != STATE_UNSPLIT)
		on_unsplit(NULL, NULL);

	gtk_widget_destroy(menu_items.main);
}
