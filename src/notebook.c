/*
 *      notebook.c - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Notebook tab Drag 'n' Drop reordering and tab management.
 */

#include "geany.h"

#include <gdk/gdkkeysyms.h>

#include "notebook.h"
#include "document.h"
#include "editor.h"
#include "documentprivate.h"
#include "ui_utils.h"
#include "sidebar.h"
#include "support.h"
#include "callbacks.h"
#include "utils.h"
#include "keybindings.h"
#include "main.h"

#define GEANY_DND_NOTEBOOK_TAB_TYPE	"geany_dnd_notebook_tab"

static const GtkTargetEntry drag_targets[] =
{
	{GEANY_DND_NOTEBOOK_TAB_TYPE, GTK_TARGET_SAME_APP | GTK_TARGET_SAME_WIDGET, 0}
};

static GtkTargetEntry files_drop_targets[] = {
	{ "STRING",			0, 0 },
	{ "UTF8_STRING",	0, 0 },
	{ "text/plain",		0, 0 },
	{ "text/uri-list",	0, 0 }
};

static const gsize MAX_MRU_DOCS = 20;
static GQueue *mru_docs = NULL;
static guint mru_pos = 0;

static gboolean switch_in_progress = FALSE;
static GtkWidget *switch_dialog = NULL;
static GtkWidget *switch_dialog_label = NULL;


static void
notebook_page_reordered_cb(GtkNotebook *notebook, GtkWidget *child, guint page_num,
		gpointer user_data);

static void
on_window_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, GtkSelectionData *data, guint target_type,
		guint event_time, gpointer user_data);

static void
notebook_tab_close_clicked_cb(GtkButton *button, gpointer user_data);

static void setup_tab_dnd(void);


static void update_mru_docs_head(GeanyDocument *doc)
{
	if (doc)
	{
		g_queue_remove(mru_docs, doc);
		g_queue_push_head(mru_docs, doc);

		if (g_queue_get_length(mru_docs) > MAX_MRU_DOCS)
			g_queue_pop_tail(mru_docs);
	}
}


/* before the tab changes, add the current document to the MRU list */
static void on_notebook_switch_page(GtkNotebook *notebook,
	GtkNotebookPage *page, guint page_num, gpointer user_data)
{
	GeanyDocument *new;

	new = document_get_from_page(page_num);

	/* insert the very first document (when adding the second document
	 * and switching to it) */
	if (g_queue_get_length(mru_docs) == 0 && gtk_notebook_get_n_pages(notebook) == 2)
		update_mru_docs_head(document_get_current());

	if (!switch_in_progress)
		update_mru_docs_head(new);
}


static void on_document_close(GObject *obj, GeanyDocument *doc)
{
	if (! main_status.quitting)
	{
		g_queue_remove(mru_docs, doc);
		/* this prevents the pop up window from showing when there's a single
		 * document */
		if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)) == 2)
			g_queue_clear(mru_docs);
	}
}


static GtkWidget *ui_minimal_dialog_new(GtkWindow *parent, const gchar *title)
{
	GtkWidget *dialog;

	dialog = gtk_window_new(GTK_WINDOW_POPUP);

	if (parent)
	{
		gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
		gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	}
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_set_name(dialog, "GeanyDialog");
	return dialog;
}


static gboolean is_modifier_key(guint keyval)
{
	switch (keyval)
	{
		case GDK_Shift_L:
		case GDK_Shift_R:
		case GDK_Control_L:
		case GDK_Control_R:
		case GDK_Meta_L:
		case GDK_Meta_R:
		case GDK_Alt_L:
		case GDK_Alt_R:
		case GDK_Super_L:
		case GDK_Super_R:
		case GDK_Hyper_L:
		case GDK_Hyper_R:
			return TRUE;
		default:
			return FALSE;
	}
}


static gboolean on_key_release_event(GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
{
	/* user may have rebound keybinding to a different modifier than Ctrl, so check all */
	if (switch_in_progress && is_modifier_key(ev->keyval))
	{
		GeanyDocument *doc;

		switch_in_progress = FALSE;

		if (switch_dialog)
		{
			gtk_widget_destroy(switch_dialog);
			switch_dialog = NULL;
		}

		doc = document_get_current();
		update_mru_docs_head(doc);
		mru_pos = 0;
		document_check_disk_status(doc, TRUE);
	}
	return FALSE;
}


static GtkWidget *create_switch_dialog(void)
{
	GtkWidget *dialog, *widget, *vbox;

	dialog = ui_minimal_dialog_new(GTK_WINDOW(main_widgets.window), _("Switch to Document"));
	gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 200, -1);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);

	widget = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_BUTTON);
	gtk_container_add(GTK_CONTAINER(vbox), widget);

	widget = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(widget), GTK_JUSTIFY_CENTER);
	gtk_container_add(GTK_CONTAINER(vbox), widget);
	switch_dialog_label = widget;

	g_signal_connect(dialog, "key-release-event", G_CALLBACK(on_key_release_event), NULL);
	return dialog;
}


static void update_filename_label(void)
{
	guint i;
	gchar *msg = NULL;
	guint queue_length;
	GeanyDocument *doc;

	if (!switch_dialog)
	{
		switch_dialog = create_switch_dialog();
		gtk_widget_show_all(switch_dialog);
	}

	queue_length = g_queue_get_length(mru_docs);
	for (i = mru_pos; (i <= mru_pos + 3) && (doc = g_queue_peek_nth(mru_docs, i % queue_length)); i++)
	{
		gchar *basename;

		basename = g_path_get_basename(DOC_FILENAME(doc));
		if (i == mru_pos)
			msg = g_markup_printf_escaped ("<b>%s</b>", basename);
		else if (i % queue_length == mru_pos)    /* && i != mru_pos */
		{
			/* We have wrapped around and got to the starting document again */
			g_free(basename);
			break;
		}
		else
		{
			SETPTR(basename, g_markup_printf_escaped ("\n%s", basename));
			SETPTR(msg, g_strconcat(msg, basename, NULL));
		}
		g_free(basename);
	}
	gtk_label_set_markup(GTK_LABEL(switch_dialog_label), msg);
	g_free(msg);
}


static gboolean on_switch_timeout(G_GNUC_UNUSED gpointer data)
{
	if (!switch_in_progress || switch_dialog)
	{
		return FALSE;
	}

	update_filename_label();
	return FALSE;
}


void notebook_switch_tablastused(void)
{
	GeanyDocument *last_doc;
	gboolean switch_start = !switch_in_progress;

	mru_pos += 1;
	last_doc = g_queue_peek_nth(mru_docs, mru_pos);

	if (! DOC_VALID(last_doc))
	{
		utils_beep();
		mru_pos = 0;
		last_doc = g_queue_peek_nth(mru_docs, mru_pos);
	}
	if (! DOC_VALID(last_doc))
		return;

	switch_in_progress = TRUE;
	document_show_tab(last_doc);

	/* if there's a modifier key, we can switch back in MRU order each time unless
	 * the key is released */
	if (switch_start)
		g_timeout_add(600, on_switch_timeout, NULL);
	else
		update_filename_label();
}


gboolean notebook_switch_in_progress(void)
{
	return switch_in_progress;
}


static gboolean focus_sci(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();

	if (doc != NULL && event->button == 1)
		gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));

	return FALSE;
}


static gboolean gtk_notebook_show_arrows(GtkNotebook *notebook)
{
	return notebook->scrollable;
#if 0
	/* To get this working we would need to define at least the first two fields of
	 * GtkNotebookPage since it is a private field. The better way would be to
	 * subclass GtkNotebook.
struct _FakeGtkNotebookPage
{
	GtkWidget *child;
	GtkWidget *tab_label;
};
 */
	gboolean show_arrow = FALSE;
	GList *children;

	if (! notebook->scrollable)
		return FALSE;

	children = notebook->children;
	while (children)
	{
		struct _FakeGtkNotebookPage *page = children->data;

		if (page->tab_label && ! gtk_widget_get_child_visible(page->tab_label))
			show_arrow = TRUE;

		children = children->next;
	}
	return show_arrow;
#endif
}


static gboolean is_position_on_tab_bar(GtkNotebook *notebook, GdkEventButton *event)
{
	GtkWidget *page;
	GtkWidget *tab;
	GtkWidget *nb;
	GtkPositionType tab_pos;
	gint scroll_arrow_hlength, scroll_arrow_vlength;
	gdouble x, y;

	page = gtk_notebook_get_nth_page(notebook, 0);
	g_return_val_if_fail(page != NULL, FALSE);

	tab = gtk_notebook_get_tab_label(notebook, page);
	g_return_val_if_fail(tab != NULL, FALSE);

	tab_pos = gtk_notebook_get_tab_pos(notebook);
	nb = GTK_WIDGET(notebook);

	gtk_widget_style_get(GTK_WIDGET(notebook), "scroll-arrow-hlength", &scroll_arrow_hlength,
		"scroll-arrow-vlength", &scroll_arrow_vlength, NULL);

	if (! gdk_event_get_coords((GdkEvent*) event, &x, &y))
	{
		x = event->x;
		y = event->y;
	}

	switch (tab_pos)
	{
		case GTK_POS_TOP:
		case GTK_POS_BOTTOM:
		{
			if (event->y >= 0 && event->y <= tab->allocation.height)
			{
				if (! gtk_notebook_show_arrows(notebook) || (
					x > scroll_arrow_hlength &&
					x < nb->allocation.width - scroll_arrow_hlength))
					return TRUE;
			}
			break;
		}
		case GTK_POS_LEFT:
		case GTK_POS_RIGHT:
		{
			if (event->x >= 0 && event->x <= tab->allocation.width)
			{
				if (! gtk_notebook_show_arrows(notebook) || (
					y > scroll_arrow_vlength &&
					y < nb->allocation.height - scroll_arrow_vlength))
					return TRUE;
			}
		}
	}

	return FALSE;
}


static void tab_bar_menu_activate_cb(GtkMenuItem *menuitem, gpointer data)
{
	GeanyDocument *doc = data;

	if (! DOC_VALID(doc))
		return;

	document_show_tab(doc);
}


static void on_open_in_new_window_activate(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *geany_path;
	GeanyDocument *doc = user_data;

	g_return_if_fail(doc->is_valid);

	geany_path = g_find_program_in_path("geany");

	if (geany_path)
	{
		gchar *doc_path = utils_get_locale_from_utf8(doc->file_name);
		gchar *argv[] = {geany_path, "-i", doc_path, NULL};
		GError *err = NULL;

		if (!utils_spawn_async(NULL, argv, NULL, 0, NULL, NULL, NULL, &err))
		{
			g_printerr("Unable to open new window: %s", err->message);
			g_error_free(err);
		}
		g_free(doc_path);
		g_free(geany_path);
	}
	else
		g_printerr("Unable to find 'geany'");
}


static void show_tab_bar_popup_menu(GdkEventButton *event, GtkWidget *page)
{
	GtkWidget *menu_item;
	static GtkWidget *menu = NULL;
	GeanyDocument *doc = NULL;
	gint page_num;

	if (menu == NULL)
		menu = gtk_menu_new();

	/* clear the old menu items */
	gtk_container_foreach(GTK_CONTAINER(menu), (GtkCallback) gtk_widget_destroy, NULL);

	ui_menu_add_document_items(GTK_MENU(menu), document_get_current(),
		G_CALLBACK(tab_bar_menu_activate_cb));

	menu_item = gtk_separator_menu_item_new();
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);

	if (page != NULL)
	{
		page_num = gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook), page);
		doc = document_get_from_page(page_num);
	}

	menu_item = ui_image_menu_item_new(GTK_STOCK_OPEN, "Open in New _Window");
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate",
		G_CALLBACK(on_open_in_new_window_activate), doc);
	/* disable if not on disk */
	if (doc == NULL || !doc->real_path)
		gtk_widget_set_sensitive(menu_item, FALSE);

	menu_item = gtk_separator_menu_item_new();
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);

	menu_item = gtk_image_menu_item_new_from_stock(GTK_STOCK_CLOSE, NULL);
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(notebook_tab_close_clicked_cb), page);
	gtk_widget_set_sensitive(GTK_WIDGET(menu_item), (page != NULL));

	menu_item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("Close Ot_her Documents"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(on_close_other_documents1_activate), page);
	gtk_widget_set_sensitive(GTK_WIDGET(menu_item), (page != NULL));

	menu_item = ui_image_menu_item_new(GTK_STOCK_CLOSE, _("C_lose All"));
	gtk_widget_show(menu_item);
	gtk_container_add(GTK_CONTAINER(menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(on_close_all1_activate), NULL);

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}


static gboolean notebook_tab_bar_click_cb(GtkWidget *widget, GdkEventButton *event,
										  gpointer user_data)
{
	if (event->type == GDK_2BUTTON_PRESS)
	{
		/* accessing ::event_window is a little hacky but we need to make sure the click
		 * was in the tab bar and not inside the child */
		if (event->window != GTK_NOTEBOOK(main_widgets.notebook)->event_window)
			return FALSE;

		if (is_position_on_tab_bar(GTK_NOTEBOOK(widget), event))
		{
			document_new_file(NULL, NULL, NULL);
			return TRUE;
		}
	}
	/* right-click is also handled here if it happened on the notebook tab bar but not
	 * on a tab directly */
	else if (event->button == 3)
	{
		show_tab_bar_popup_menu(event, NULL);
		return TRUE;
	}
	return FALSE;
}


void notebook_init()
{
	/* Individual style for the tab close buttons */
	gtk_rc_parse_string(
		"style \"geany-close-tab-button-style\" {\n"
		"	GtkWidget::focus-padding = 0\n"
		"	GtkWidget::focus-line-width = 0\n"
		"	xthickness = 0\n"
		"	ythickness = 0\n"
		"}\n"
		"widget \"*.geany-close-tab-button\" style \"geany-close-tab-button-style\""
	);

	g_signal_connect_after(main_widgets.notebook, "button-press-event",
		G_CALLBACK(notebook_tab_bar_click_cb), NULL);

	g_signal_connect(main_widgets.notebook, "drag-data-received",
		G_CALLBACK(on_window_drag_data_received), NULL);

	mru_docs = g_queue_new();
	g_signal_connect(main_widgets.notebook, "switch-page",
		G_CALLBACK(on_notebook_switch_page), NULL);
	g_signal_connect(geany_object, "document-close",
		G_CALLBACK(on_document_close), NULL);

	/* in case the switch dialog misses an event while drawing the dialog */
	g_signal_connect(main_widgets.window, "key-release-event", G_CALLBACK(on_key_release_event), NULL);

	setup_tab_dnd();
}


void notebook_free(void)
{
	g_queue_free(mru_docs);
}


static void setup_tab_dnd()
{
	GtkWidget *notebook = main_widgets.notebook;

	g_signal_connect(notebook, "page-reordered", G_CALLBACK(notebook_page_reordered_cb), NULL);
}


static void
notebook_page_reordered_cb(GtkNotebook *notebook, GtkWidget *child, guint page_num,
	gpointer user_data)
{
	/* Not necessary to update open files treeview if it's sorted.
	 * Note: if enabled, it's best to move the item instead of recreating all items. */
	/*sidebar_openfiles_update_all();*/
}


/* call this after the number of tabs in main_widgets.notebook changes. */
static void tab_count_changed(void)
{
	switch (gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook)))
	{
		case 0:
		/* Enables DnD for dropping files into the empty notebook widget */
		gtk_drag_dest_set(main_widgets.notebook, GTK_DEST_DEFAULT_ALL,
			files_drop_targets,	G_N_ELEMENTS(files_drop_targets),
			GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);
		break;

		case 1:
		/* Disables DnD for dropping files into the notebook widget and enables the DnD for moving file
		 * tabs. Files can still be dropped into the notebook widget because it will be handled by the
		 * active Scintilla Widget (only dropping to the tab bar is not possible but it should be ok) */
		gtk_drag_dest_set(main_widgets.notebook, GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			drag_targets, G_N_ELEMENTS(drag_targets), GDK_ACTION_MOVE);
		break;
	}
}


static gboolean notebook_tab_click(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	guint state;

	/* toggle additional widgets on double click */
	if (event->type == GDK_2BUTTON_PRESS)
	{
		if (interface_prefs.notebook_double_click_hides_widgets)
			on_menu_toggle_all_additional_widgets1_activate(NULL, NULL);

		return TRUE; /* stop other handlers like notebook_tab_bar_click_cb() */
	}
	/* close tab on middle click */
	if (event->button == 2)
	{
		document_remove_page(gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook),
			GTK_WIDGET(data)));
		return TRUE; /* stop other handlers like notebook_tab_bar_click_cb() */
	}
	/* switch last used tab on ctrl-click */
	state = event->state & gtk_accelerator_get_default_mod_mask();
	if (event->button == 1 && state == GDK_CONTROL_MASK)
	{
		keybindings_send_command(GEANY_KEY_GROUP_NOTEBOOK,
			GEANY_KEYS_NOTEBOOK_SWITCHTABLASTUSED);
		return TRUE;
	}
	/* right-click is first handled here if it happened on a notebook tab */
	if (event->button == 3)
	{
		show_tab_bar_popup_menu(event, data);
		return TRUE;
	}

	return FALSE;
}


static void notebook_tab_close_button_style_set(GtkWidget *btn, GtkRcStyle *prev_style,
												gpointer data)
{
	gint w, h;

	gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(btn), GTK_ICON_SIZE_MENU, &w, &h);
	gtk_widget_set_size_request(btn, w + 2, h + 2);
}


/* Returns page number of notebook page, or -1 on error */
gint notebook_new_tab(GeanyDocument *this)
{
	GtkWidget *hbox, *ebox;
	gint tabnum;
	GtkWidget *page;
	gint cur_page;

	g_return_val_if_fail(this != NULL, -1);

	page = GTK_WIDGET(this->editor->sci);

	this->priv->tab_label = gtk_label_new(NULL);

	/* get button press events for the tab label and the space between it and
	 * the close button, if any */
	ebox = gtk_event_box_new();
	GTK_WIDGET_SET_FLAGS(ebox, GTK_NO_WINDOW);
	g_signal_connect(ebox, "button-press-event", G_CALLBACK(notebook_tab_click), page);
	/* focus the current document after clicking on a tab */
	g_signal_connect_after(ebox, "button-release-event",
		G_CALLBACK(focus_sci), NULL);

	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), this->priv->tab_label, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(ebox), hbox);

	if (file_prefs.show_tab_cross)
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
		gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);

		g_signal_connect(btn, "clicked", G_CALLBACK(notebook_tab_close_clicked_cb), page);
		/* button overrides event box, so make middle click on button also close tab */
		g_signal_connect(btn, "button-press-event", G_CALLBACK(notebook_tab_click), page);
		/* handle style modification to keep button small as possible even when theme change */
		g_signal_connect(btn, "style-set", G_CALLBACK(notebook_tab_close_button_style_set), NULL);
	}

	gtk_widget_show_all(ebox);

	document_update_tab_label(this);

	if (file_prefs.tab_order_beside)
		cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.notebook));
	else
		cur_page = file_prefs.tab_order_ltr ? -2 /* hack: -2 + 1 = -1, last page */ : 0;
	if (file_prefs.tab_order_ltr)
		tabnum = gtk_notebook_insert_page_menu(GTK_NOTEBOOK(main_widgets.notebook), page,
			ebox, NULL, cur_page + 1);
	else
		tabnum = gtk_notebook_insert_page_menu(GTK_NOTEBOOK(main_widgets.notebook), page,
			ebox, NULL, cur_page);

	tab_count_changed();

	/* enable tab DnD */
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(main_widgets.notebook), page, TRUE);

	return tabnum;
}


static void
notebook_tab_close_clicked_cb(GtkButton *button, gpointer user_data)
{
	gint cur_page = gtk_notebook_page_num(GTK_NOTEBOOK(main_widgets.notebook),
		GTK_WIDGET(user_data));

	document_remove_page(cur_page);
}


/* Always use this instead of gtk_notebook_remove_page(). */
void notebook_remove_page(gint page_num)
{
	gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(main_widgets.notebook));

	if (page_num == page)
	{
		if (file_prefs.tab_order_ltr)
			page += 1;
		else if (page > 0) /* never go negative, it would select the last page */
			page -= 1;

		if (file_prefs.tab_close_switch_to_mru)
		{
			GeanyDocument *last_doc;

			last_doc = g_queue_peek_nth(mru_docs, 0);
			if (DOC_VALID(last_doc))
				page = document_get_notebook_page(last_doc);
		}

		gtk_notebook_set_current_page(GTK_NOTEBOOK(main_widgets.notebook), page);
	}

	/* now remove the page (so we don't temporarily switch to the previous page) */
	gtk_notebook_remove_page(GTK_NOTEBOOK(main_widgets.notebook), page_num);

	tab_count_changed();
}


static void
on_window_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, GtkSelectionData *data, guint target_type,
		guint event_time, gpointer user_data)
{
	gboolean success = FALSE;

	if (data->length > 0 && data->format == 8)
	{
		if (drag_context->action == GDK_ACTION_ASK)
		{
			drag_context->action = GDK_ACTION_COPY;
		}

		document_open_file_list((const gchar *)data->data, data->length);

		success = TRUE;
	}
	gtk_drag_finish(drag_context, success, FALSE, event_time);
}
