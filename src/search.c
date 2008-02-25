/*
 *      search.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * Find, Replace, Find in Files dialog related functions.
 * Note that the basic text find functions are in document.c.
 */

#include <gdk/gdkkeysyms.h>

#include "geany.h"
#include "search.h"
#include "prefs.h"
#include "support.h"
#include "utils.h"
#include "msgwindow.h"
#include "document.h"
#include "sciwrappers.h"
#include "ui_utils.h"
#include "editor.h"

#include <unistd.h>
#include <string.h>

#ifdef G_OS_UNIX
# include <sys/types.h>
# include <sys/wait.h>
#endif


enum {
	GEANY_RESPONSE_FIND = 1,
	GEANY_RESPONSE_FIND_PREVIOUS,
	GEANY_RESPONSE_FIND_IN_FILE,
	GEANY_RESPONSE_FIND_IN_SESSION,
	GEANY_RESPONSE_MARK,
	GEANY_RESPONSE_REPLACE,
	GEANY_RESPONSE_REPLACE_AND_FIND,
	GEANY_RESPONSE_REPLACE_IN_SESSION,
	GEANY_RESPONSE_REPLACE_IN_FILE,
	GEANY_RESPONSE_REPLACE_IN_SEL
};


GeanySearchData search_data;

SearchPrefs search_prefs = {NULL};

static struct
{
	GtkWidget	*find_dialog;
	GtkWidget	*replace_dialog;
	GtkWidget	*find_in_files_dialog;
} widgets;


static gboolean search_read_io              (GIOChannel *source,
                                             GIOCondition condition,
                                             gpointer data);

static void search_close_pid(GPid child_pid, gint status, gpointer user_data);

static gchar **search_get_argv(const gchar **argv_prefix, const gchar *dir);


static void
on_find_replace_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data);

static void
on_find_dialog_response(GtkDialog *dialog, gint response, gpointer user_data);

static void
on_find_entry_activate(GtkEntry *entry, gpointer user_data);

static void
on_replace_dialog_response(GtkDialog *dialog, gint response, gpointer user_data);

static void
on_replace_entry_activate(GtkEntry *entry, gpointer user_data);

static gboolean
on_widget_key_pressed_set_focus(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

static void
on_find_in_files_dialog_response(GtkDialog *dialog, gint response, gpointer user_data);

static gboolean
search_find_in_files(const gchar *search_text, const gchar *dir, const gchar *opts);


void search_init(void)
{
	widgets.find_dialog		= NULL;
	widgets.replace_dialog		= NULL;
	widgets.find_in_files_dialog = NULL;
	search_data.text		= NULL;
}


#define FREE_WIDGET(wid) \
	if (wid && GTK_IS_WIDGET(wid)) gtk_widget_destroy(wid);

void search_finalize(void)
{
	FREE_WIDGET(widgets.find_dialog);
	FREE_WIDGET(widgets.replace_dialog);
	FREE_WIDGET(widgets.find_in_files_dialog);
	g_free(search_data.text);
}


static GtkWidget *add_find_checkboxes(GtkDialog *dialog)
{
	GtkWidget *checkbox1, *checkbox2, *check_regexp, *check_back, *checkbox5,
			  *checkbox7, *hbox, *fbox, *mbox;
	GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

	check_regexp = gtk_check_button_new_with_mnemonic(_("_Use regular expressions"));
	g_object_set_data_full(G_OBJECT(dialog), "check_regexp",
					gtk_widget_ref(check_regexp), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(check_regexp), FALSE);
	gtk_tooltips_set_tip(tooltips, check_regexp, _("Use POSIX-like regular expressions. "
		"For detailed information about using regular expressions, please read the documentation."), NULL);
	g_signal_connect(G_OBJECT(check_regexp), "toggled",
		G_CALLBACK(on_find_replace_checkbutton_toggled), GTK_WIDGET(dialog));

	if (dialog != GTK_DIALOG(widgets.find_dialog))
	{
		check_back = gtk_check_button_new_with_mnemonic(_("Search _backwards"));
		g_object_set_data_full(G_OBJECT(dialog), "check_back",
						gtk_widget_ref(check_back), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_back), FALSE);
	}
	else
	{	// align the two checkboxes at the top of the hbox
		GtkSizeGroup *label_size;
		check_back = gtk_label_new(NULL);
		label_size = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);
		gtk_size_group_add_widget(GTK_SIZE_GROUP(label_size), check_back);
		gtk_size_group_add_widget(GTK_SIZE_GROUP(label_size), check_regexp);
		g_object_unref(label_size);
	}
	checkbox7 = gtk_check_button_new_with_mnemonic(_("Use _escape sequences"));
	g_object_set_data_full(G_OBJECT(dialog), "check_escape",
					gtk_widget_ref(checkbox7), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox7), FALSE);
	gtk_tooltips_set_tip(tooltips, checkbox7,
		_("Replace \\\\, \\t, \\n, \\r and \\uXXXX (Unicode chararacters) with the "
		  "corresponding control characters."), NULL);

	// Search features
	fbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(fbox), check_regexp);
	gtk_container_add(GTK_CONTAINER(fbox), checkbox7);
	gtk_container_add(GTK_CONTAINER(fbox), check_back);

	checkbox1 = gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
	g_object_set_data_full(G_OBJECT(dialog), "check_case",
					gtk_widget_ref(checkbox1), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);

	checkbox2 = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
	g_object_set_data_full(G_OBJECT(dialog), "check_word",
					gtk_widget_ref(checkbox2), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);

	checkbox5 = gtk_check_button_new_with_mnemonic(_("Match from s_tart of word"));
	g_object_set_data_full(G_OBJECT(dialog), "check_wordstart",
					gtk_widget_ref(checkbox5), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox5), FALSE);

	// Matching options
	mbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(mbox), checkbox1);
	gtk_container_add(GTK_CONTAINER(mbox), checkbox2);
	gtk_container_add(GTK_CONTAINER(mbox), checkbox5);

	hbox = gtk_hbox_new(TRUE, 6);
	gtk_container_add(GTK_CONTAINER(hbox), fbox);
	gtk_container_add(GTK_CONTAINER(hbox), mbox);
	return hbox;
}


static void send_find_dialog_response(GtkButton *button, gpointer user_data)
{
	gtk_dialog_response(GTK_DIALOG(widgets.find_dialog), GPOINTER_TO_INT(user_data));
}


// store text, clear search flags so we can use Search->Find Next/Previous
static void setup_find_next(const gchar *text)
{
	g_free(search_data.text);
	search_data.text = g_strdup(text);
	search_data.flags = 0;
	search_data.backwards = FALSE;
	search_data.search_bar = FALSE;
}


/* Search for next match of the current "selection"
 * For X11 based systems, this will try to use the system-wide
 * x-selection first. If it doesn't find anything suitable in
 * the x-selection (or if we are on Win32) it will try to use
 * the scintilla selection or current token instead.
 * Search flags are always zero.
 */
void search_find_selection(gint idx, gboolean search_backwards)
{
	gchar *s = NULL;
#ifdef G_OS_UNIX
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
#endif

	g_return_if_fail(DOC_IDX_VALID(idx));

#ifdef G_OS_UNIX
	s=gtk_clipboard_wait_for_text(clipboard);
	if (s)
	{
		if (strchr(s,'\n') || strchr(s, '\r'))
		{
			g_free(s);
			s=NULL;
		};
	}
#endif

	if (!s)	{ s=editor_get_default_selection(idx, NULL); }
	if (s)
	{
		setup_find_next(s);	// allow find next/prev

		if (document_find_text(idx, s, 0, search_backwards, FALSE, NULL) > -1)
			editor_display_current_line(idx, 0.3F);
		g_free(s);
	}
}


void search_show_find_dialog(void)
{
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	g_return_if_fail(DOC_IDX_VALID(idx));

	sel = editor_get_default_selection(idx, NULL);

	if (widgets.find_dialog == NULL)
	{
		GtkWidget *label, *entry, *sbox, *vbox;
		GtkWidget *exp, *bbox, *button, *check_close;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		widgets.find_dialog = gtk_dialog_new_with_buttons(_("Find"),
			GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(widgets.find_dialog));
		gtk_widget_set_name(widgets.find_dialog, "GeanyDialogSearch");
		gtk_box_set_spacing(GTK_BOX(vbox), 9);

		button = ui_button_new_with_image(GTK_STOCK_GO_BACK, _("_Previous"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.find_dialog), button,
			GEANY_RESPONSE_FIND_PREVIOUS);
		g_object_set_data_full(G_OBJECT(widgets.find_dialog), "btn_previous",
						gtk_widget_ref(button), (GDestroyNotify)gtk_widget_unref);

		button = ui_button_new_with_image(GTK_STOCK_GO_FORWARD, _("_Next"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.find_dialog), button,
			GEANY_RESPONSE_FIND);

		label = gtk_label_new_with_mnemonic(_("_Search for:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		entry = gtk_combo_box_entry_new_text();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 50);
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(entry)->child), sel);
		g_object_set_data_full(G_OBJECT(widgets.find_dialog), "entry",
						gtk_widget_ref(entry), (GDestroyNotify)gtk_widget_unref);

		g_signal_connect((gpointer) gtk_bin_get_child(GTK_BIN(entry)), "activate",
				G_CALLBACK(on_find_entry_activate), NULL);
		g_signal_connect((gpointer) widgets.find_dialog, "response",
				G_CALLBACK(on_find_dialog_response), entry);
		g_signal_connect((gpointer) widgets.find_dialog, "delete_event",
				G_CALLBACK(gtk_widget_hide_on_delete), NULL);

		sbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(sbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(sbox), entry, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), sbox, TRUE, FALSE, 0);

		gtk_container_add(GTK_CONTAINER(vbox),
			add_find_checkboxes(GTK_DIALOG(widgets.find_dialog)));

		// Now add the multiple match options
		exp = gtk_expander_new_with_mnemonic(_("_Find All"));
		bbox = gtk_hbutton_box_new();

		button = gtk_button_new_with_mnemonic(_("_Mark"));
		gtk_tooltips_set_tip(tooltips, button,
				_("Mark all matches in the current document."), NULL);
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_find_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_MARK));

		button = gtk_button_new_with_mnemonic(_("In Sessi_on"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_find_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_FIND_IN_SESSION));

		button = gtk_button_new_with_mnemonic(_("_In Document"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_find_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_FIND_IN_FILE));

		// close window checkbox
		check_close = gtk_check_button_new_with_mnemonic(_("Close _dialog"));
		g_object_set_data_full(G_OBJECT(widgets.find_dialog), "check_close",
						gtk_widget_ref(check_close), (GDestroyNotify) gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_close), FALSE);
		gtk_tooltips_set_tip(tooltips, check_close,
				_("Disable this option to keep the dialog open."), NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_close), TRUE);
		gtk_container_add(GTK_CONTAINER(bbox), check_close);
		gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(bbox), check_close, TRUE);

		ui_hbutton_box_copy_layout(
			GTK_BUTTON_BOX(GTK_DIALOG(widgets.find_dialog)->action_area),
			GTK_BUTTON_BOX(bbox));
		gtk_container_add(GTK_CONTAINER(exp), bbox);
		gtk_container_add(GTK_CONTAINER(vbox), exp);

		gtk_widget_show_all(widgets.find_dialog);
	}
	else
	{
		// only set selection if the dialog is not already visible
		if (! GTK_WIDGET_VISIBLE(widgets.find_dialog) && sel)
			gtk_entry_set_text(GTK_ENTRY(GTK_BIN(
							lookup_widget(widgets.find_dialog, "entry"))->child), sel);
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(widgets.find_dialog, "entry"))->child));
		gtk_widget_show(widgets.find_dialog);
		// bring the dialog back in the foreground in case it is already open but the focus is away
		gtk_window_present(GTK_WINDOW(widgets.find_dialog));
	}
	g_free(sel);
}


static void send_replace_dialog_response(GtkButton *button, gpointer user_data)
{
	gtk_dialog_response(GTK_DIALOG(widgets.replace_dialog), GPOINTER_TO_INT(user_data));
}


void search_show_replace_dialog(void)
{
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	sel = editor_get_default_selection(idx, NULL);

	if (widgets.replace_dialog == NULL)
	{
		GtkWidget *label_find, *label_replace, *entry_find, *entry_replace,
			*check_close, *button, *rbox, *fbox, *vbox, *exp, *bbox;
		GtkSizeGroup *label_size;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		widgets.replace_dialog = gtk_dialog_new_with_buttons(_("Replace"),
			GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(widgets.replace_dialog));
		gtk_box_set_spacing(GTK_BOX(vbox), 9);
		gtk_widget_set_name(widgets.replace_dialog, "GeanyDialogSearch");

		button = gtk_button_new_from_stock(GTK_STOCK_FIND);
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.replace_dialog), button,
			GEANY_RESPONSE_FIND);
		button = gtk_button_new_with_mnemonic(_("_Replace"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.replace_dialog), button,
			GEANY_RESPONSE_REPLACE);
		button = gtk_button_new_with_mnemonic(_("Replace & Fi_nd"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.replace_dialog), button,
			GEANY_RESPONSE_REPLACE_AND_FIND);

		label_find = gtk_label_new_with_mnemonic(_("_Search for:"));
		gtk_misc_set_alignment(GTK_MISC(label_find), 0, 0.5);

		label_replace = gtk_label_new_with_mnemonic(_("Replace wit_h:"));
		gtk_misc_set_alignment(GTK_MISC(label_replace), 0, 0.5);

		entry_find = gtk_combo_box_entry_new_text();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label_find), entry_find);
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 50);
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(entry_find)->child), sel);
		g_object_set_data_full(G_OBJECT(widgets.replace_dialog), "entry_find",
						gtk_widget_ref(entry_find), (GDestroyNotify)gtk_widget_unref);

		entry_replace = gtk_combo_box_entry_new_text();
		gtk_label_set_mnemonic_widget(GTK_LABEL(label_replace), entry_replace);
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))), 50);
		g_object_set_data_full(G_OBJECT(widgets.replace_dialog), "entry_replace",
						gtk_widget_ref(entry_replace), (GDestroyNotify)gtk_widget_unref);

		g_signal_connect((gpointer) gtk_bin_get_child(GTK_BIN(entry_find)),
				"key-press-event", G_CALLBACK(on_widget_key_pressed_set_focus),
				gtk_bin_get_child(GTK_BIN(entry_replace)));
		g_signal_connect((gpointer) gtk_bin_get_child(GTK_BIN(entry_replace)), "activate",
				G_CALLBACK(on_replace_entry_activate), NULL);
		g_signal_connect((gpointer) widgets.replace_dialog, "response",
				G_CALLBACK(on_replace_dialog_response), entry_replace);
		g_signal_connect((gpointer) widgets.replace_dialog, "delete_event",
				G_CALLBACK(gtk_widget_hide_on_delete), NULL);

		fbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(fbox), label_find, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(fbox), entry_find, TRUE, TRUE, 0);

		rbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(rbox), label_replace, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(rbox), entry_replace, TRUE, TRUE, 0);

		label_size = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget(label_size, label_find);
		gtk_size_group_add_widget(label_size, label_replace);
		g_object_unref(G_OBJECT(label_size));	// auto destroy the size group

		gtk_box_pack_start(GTK_BOX(vbox), fbox, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), rbox, TRUE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox),
			add_find_checkboxes(GTK_DIALOG(widgets.replace_dialog)));

		// Now add the multiple replace options
		exp = gtk_expander_new_with_mnemonic(_("Re_place All"));
		bbox = gtk_hbutton_box_new();

		button = gtk_button_new_with_mnemonic(_("In Se_lection"));
		gtk_tooltips_set_tip(tooltips, button,
			_("Replace all matches found in the currently selected text"), NULL);
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_replace_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_REPLACE_IN_SEL));

		button = gtk_button_new_with_mnemonic(_("In Sessi_on"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_replace_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_REPLACE_IN_SESSION));

		button = gtk_button_new_with_mnemonic(_("_In Document"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_replace_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_REPLACE_IN_FILE));

		// close window checkbox
		check_close = gtk_check_button_new_with_mnemonic(_("Close _dialog"));
		g_object_set_data_full(G_OBJECT(widgets.replace_dialog), "check_close",
						gtk_widget_ref(check_close), (GDestroyNotify) gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_close), FALSE);
		gtk_tooltips_set_tip(tooltips, check_close,
				_("Disable this option to keep the dialog open."), NULL);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_close), TRUE);
		gtk_container_add(GTK_CONTAINER(bbox), check_close);
		gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(bbox), check_close, TRUE);

		ui_hbutton_box_copy_layout(
			GTK_BUTTON_BOX(GTK_DIALOG(widgets.replace_dialog)->action_area),
			GTK_BUTTON_BOX(bbox));
		gtk_container_add(GTK_CONTAINER(exp), bbox);
		gtk_container_add(GTK_CONTAINER(vbox), exp);

		gtk_widget_show_all(widgets.replace_dialog);
	}
	else
	{
		// only set selection if the dialog is not already visible
		if (! GTK_WIDGET_VISIBLE(widgets.replace_dialog) && sel)
			gtk_entry_set_text(GTK_ENTRY(GTK_BIN(
							lookup_widget(widgets.replace_dialog, "entry_find"))->child), sel);
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(widgets.replace_dialog, "entry_find"))->child));
		gtk_widget_show(widgets.replace_dialog);
		// bring the dialog back in the foreground in case it is already open but the focus is away
		gtk_window_present(GTK_WINDOW(widgets.replace_dialog));
	}
	g_free(sel);
}


static void on_extra_options_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	// disable extra option entry when checkbutton not checked
	gtk_widget_set_sensitive(GTK_WIDGET(user_data),
		gtk_toggle_button_get_active(togglebutton));
}


/* dir is the directory to search in (UTF-8 encoding), maybe NULL to determine it the usual way
 * by using the current file's path */
void search_show_find_in_files_dialog(const gchar *dir)
{
	static GtkWidget *combo = NULL;
	static GtkWidget *dir_combo;
	GtkWidget *entry; // the child GtkEntry of combo (or dir_combo)
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;
	gchar *cur_dir;

	if (widgets.find_in_files_dialog == NULL)
	{
		GtkWidget *label, *label1, *checkbox1, *checkbox2, *check_wholeword,
			*check_recursive, *check_extra, *entry_extra;
		GtkWidget *dbox, *sbox, *cbox, *rbox, *rbtn, *hbox, *vbox;
		GtkSizeGroup *size_group;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		widgets.find_in_files_dialog = gtk_dialog_new_with_buttons(
			_("Find in Files"), GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(widgets.find_in_files_dialog));
		gtk_box_set_spacing(GTK_BOX(vbox), 9);
		gtk_widget_set_name(widgets.find_in_files_dialog, "GeanyDialogSearch");

		gtk_dialog_add_button(GTK_DIALOG(widgets.find_in_files_dialog), "gtk-find", GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response(GTK_DIALOG(widgets.find_in_files_dialog),
			GTK_RESPONSE_ACCEPT);

		label1 = gtk_label_new_with_mnemonic(_("_Directory:"));
		gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);

		dir_combo = gtk_combo_box_entry_new_text();
		entry = gtk_bin_get_child(GTK_BIN(dir_combo));
		gtk_label_set_mnemonic_widget(GTK_LABEL(label1), entry);
		gtk_entry_set_max_length(GTK_ENTRY(entry), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 50);
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "dir_combo",
						gtk_widget_ref(dir_combo), (GDestroyNotify)gtk_widget_unref);

		dbox = ui_path_box_new(NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
			GTK_ENTRY(entry));
		gtk_box_pack_start(GTK_BOX(dbox), label1, FALSE, FALSE, 0);

		label = gtk_label_new_with_mnemonic(_("_Search for:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		combo = gtk_combo_box_entry_new_text();
		entry = gtk_bin_get_child(GTK_BIN(combo));
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
		gtk_entry_set_max_length(GTK_ENTRY(entry), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 50);
		gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);

		sbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(sbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(sbox), combo, TRUE, TRUE, 0);

		size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget(size_group, label1);
		gtk_size_group_add_widget(size_group, label);
		g_object_unref(G_OBJECT(size_group));	// auto destroy the size group

		rbox = gtk_vbox_new(FALSE, 0);
		rbtn = gtk_radio_button_new_with_mnemonic(NULL, _("Fixed s_trings"));
		// Make fixed strings the default to speed up searching all files in directory.
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rbtn), TRUE);
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "radio_fgrep",
						gtk_widget_ref(rbtn), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(rbtn), FALSE);
		gtk_container_add(GTK_CONTAINER(rbox), rbtn);

		rbtn = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(rbtn),
					_("_Grep regular expressions"));
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "radio_grep",
						gtk_widget_ref(rbtn), (GDestroyNotify)gtk_widget_unref);
		gtk_tooltips_set_tip(tooltips, rbtn,
					_("See grep's manual page for more information."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(rbtn), FALSE);
		gtk_container_add(GTK_CONTAINER(rbox), rbtn);

		rbtn = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(rbtn),
					_("_Extended regular expressions"));
		gtk_tooltips_set_tip(tooltips, rbtn,
					_("See grep's manual page for more information."), NULL);
		gtk_button_set_focus_on_click(GTK_BUTTON(rbtn), FALSE);
		gtk_container_add(GTK_CONTAINER(rbox), rbtn);

		check_recursive = gtk_check_button_new_with_mnemonic(_("_Recurse in subfolders"));
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "check_recursive",
						gtk_widget_ref(check_recursive), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_recursive), FALSE);

		checkbox1 = gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "check_case",
						gtk_widget_ref(checkbox1), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox1), TRUE);

		check_wholeword = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "check_wholeword",
						gtk_widget_ref(check_wholeword), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_wholeword), FALSE);

		checkbox2 = gtk_check_button_new_with_mnemonic(_("_Invert search results"));
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "check_invert",
						gtk_widget_ref(checkbox2), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);
		gtk_tooltips_set_tip(tooltips, checkbox2,
				_("Invert the sense of matching, to select non-matching lines."), NULL);

		cbox = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(cbox), checkbox1);
		gtk_container_add(GTK_CONTAINER(cbox), check_wholeword);
		gtk_container_add(GTK_CONTAINER(cbox), checkbox2);
		gtk_container_add(GTK_CONTAINER(cbox), check_recursive);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(hbox), rbox);
		gtk_container_add(GTK_CONTAINER(hbox), cbox);

		gtk_box_pack_start(GTK_BOX(vbox), dbox, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), sbox, TRUE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		check_extra = gtk_check_button_new_with_mnemonic(_("E_xtra options:"));
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "check_extra",
						gtk_widget_ref(check_extra), (GDestroyNotify)gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_extra), FALSE);

		entry_extra = gtk_entry_new();
		if (search_prefs.fif_extra_options)
			gtk_entry_set_text(GTK_ENTRY(entry_extra), search_prefs.fif_extra_options);
		gtk_widget_set_sensitive(entry_extra, FALSE);
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "entry_extra",
						gtk_widget_ref(entry_extra), (GDestroyNotify)gtk_widget_unref);
		gtk_tooltips_set_tip(tooltips, entry_extra,
				_("Other options to pass to Grep"), NULL);

		// enable entry_extra when check_extra is checked
		g_signal_connect(G_OBJECT(check_extra), "toggled",
			G_CALLBACK(on_extra_options_toggled), entry_extra);

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(hbox), check_extra, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), entry_extra, TRUE, TRUE, 0);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		g_signal_connect((gpointer) dir_combo, "key-press-event",
				G_CALLBACK(on_widget_key_pressed_set_focus), combo);
		g_signal_connect((gpointer) widgets.find_in_files_dialog, "response",
				G_CALLBACK(on_find_in_files_dialog_response), combo);
		g_signal_connect((gpointer) widgets.find_in_files_dialog, "delete_event",
				G_CALLBACK(gtk_widget_hide_on_delete), NULL);

		gtk_widget_show_all(widgets.find_in_files_dialog);
		sel = editor_get_default_selection(idx, NULL);
	}

	entry = GTK_BIN(combo)->child;
	// only set selection if the dialog is not already visible, or has just been created
	if (! sel && ! GTK_WIDGET_VISIBLE(widgets.find_in_files_dialog))
		sel = editor_get_default_selection(idx, NULL);
	if (sel)
		gtk_entry_set_text(GTK_ENTRY(entry), sel);
	g_free(sel);

	entry = GTK_BIN(dir_combo)->child;
	if (NZV(dir))
		cur_dir = g_strdup(dir);
	else
		cur_dir = utils_get_current_file_dir_utf8();
	if (cur_dir)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), cur_dir);
		g_free(cur_dir);
	}
	else
	{	// use default_open_path if no directory could be determined (e.g. when no files are open)
		gtk_entry_set_text(GTK_ENTRY(entry), prefs.default_open_path);
	}

	// put the focus to the directory entry if it is empty
	if (utils_str_equal(gtk_entry_get_text(GTK_ENTRY(entry)), ""))
		gtk_widget_grab_focus(dir_combo);
	else
		gtk_widget_grab_focus(combo);

	gtk_widget_show(widgets.find_in_files_dialog);
	// bring the dialog back in the foreground in case it is already open but the focus is away
	gtk_window_present(GTK_WINDOW(widgets.find_in_files_dialog));
}


static void
on_find_replace_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET(user_data);
	GtkToggleButton *chk_regexp = GTK_TOGGLE_BUTTON(
		lookup_widget(dialog, "check_regexp"));

	if (togglebutton == chk_regexp)
	{
		gboolean regex_set = gtk_toggle_button_get_active(chk_regexp);
		GtkWidget *check_word = lookup_widget(dialog, "check_word");
		GtkWidget *check_wordstart = lookup_widget(dialog, "check_wordstart");
		GtkToggleButton *check_case = GTK_TOGGLE_BUTTON(
			lookup_widget(dialog, "check_case"));
		static gboolean case_state = FALSE; // state before regex enabled

		// hide options that don't apply to regex searches
		if (dialog == widgets.find_dialog)
			gtk_widget_set_sensitive(lookup_widget(dialog, "btn_previous"), ! regex_set);
		else
			gtk_widget_set_sensitive(lookup_widget(dialog, "check_back"), ! regex_set);

		gtk_widget_set_sensitive(check_word, ! regex_set);
		gtk_widget_set_sensitive(check_wordstart, ! regex_set);

		if (regex_set)	// regex enabled
		{
			// Enable case sensitive but remember original case toggle state
			case_state = gtk_toggle_button_get_active(check_case);
			gtk_toggle_button_set_active(check_case, TRUE);
		}
		else	// regex disabled
		{
			// If case sensitive is still enabled, revert to what it was before we enabled it
			if (gtk_toggle_button_get_active(check_case) == TRUE)
				gtk_toggle_button_set_active(check_case, case_state);
		}
	}
}


static gint search_mark(gint idx, const gchar *search_text, gint flags)
{
	gint pos, line, count = 0;
	struct TextToFind ttf;

	g_return_val_if_fail(DOC_IDX_VALID(idx), 0);

	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(doc_list[idx].sci);
	ttf.lpstrText = (gchar *)search_text;
	while (1)
	{
		pos = sci_find_text(doc_list[idx].sci, flags, &ttf);
		if (pos == -1) break;

		line = sci_get_line_from_position(doc_list[idx].sci, pos);
		sci_set_marker_at_line(doc_list[idx].sci, line, TRUE, 1);

		ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
		count++;
	}
	return count;
}


static void
on_find_entry_activate(GtkEntry *entry, gpointer user_data)
{
	on_find_dialog_response(NULL, GEANY_RESPONSE_FIND,
				lookup_widget(GTK_WIDGET(entry), "entry"));
}


static gint get_search_flags(GtkWidget *dialog)
{
	gboolean fl1, fl2, fl3, fl4;

	fl1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(dialog, "check_case")));
	fl2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(dialog, "check_word")));
	fl3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(dialog, "check_regexp")));
	fl4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(dialog, "check_wordstart")));

	return (fl1 ? SCFIND_MATCHCASE : 0) |
		(fl2 ? SCFIND_WHOLEWORD : 0) |
		(fl3 ? SCFIND_REGEXP | SCFIND_POSIX : 0) |
		(fl4 ? SCFIND_WORDSTART : 0);
}


static void
on_find_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
		gtk_widget_hide(widgets.find_dialog);
	else
	{
		gint idx = document_get_cur_idx();
		gboolean search_replace_escape;
		gboolean check_close = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_close")));

		search_replace_escape = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_escape")));
		search_data.backwards = FALSE;
		search_data.search_bar = FALSE;

		if (! DOC_IDX_VALID(idx)) return;

		g_free(search_data.text);
		search_data.text = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(user_data)))));
		if (strlen(search_data.text) == 0 ||
			(search_replace_escape && ! utils_str_replace_escape(search_data.text)))
		{
			utils_beep();
			gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(widgets.find_dialog, "entry"))->child));
			return;
		}

		ui_combo_box_add_to_history(GTK_COMBO_BOX(user_data), search_data.text);

		search_data.flags = get_search_flags(widgets.find_dialog);

		switch (response)
		{
			case GEANY_RESPONSE_FIND:
			case GEANY_RESPONSE_FIND_PREVIOUS:
				document_find_text(idx, search_data.text, search_data.flags,
					(response == GEANY_RESPONSE_FIND_PREVIOUS), TRUE, GTK_WIDGET(widgets.find_dialog));
				check_close = FALSE;
				if (prefs.suppress_search_dialogs)
					check_close = TRUE;
				break;

			case GEANY_RESPONSE_FIND_IN_FILE:
				search_find_usage(search_data.text, search_data.flags, FALSE);
				break;

			case GEANY_RESPONSE_FIND_IN_SESSION:
				search_find_usage(search_data.text, search_data.flags, TRUE);
				break;

			case GEANY_RESPONSE_MARK:
				if (DOC_IDX_VALID(idx))
				{
					gint count = search_mark(idx, search_data.text, search_data.flags);

					if (count == 0)
						ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), search_data.text);
					else
						ui_set_statusbar(FALSE, _("Found %d matches for \"%s\"."), count,
							search_data.text);
				}
				break;
		}
		if (check_close)
			gtk_widget_hide(widgets.find_dialog);
	}
}


static void
on_replace_entry_activate(GtkEntry *entry, gpointer user_data)
{
	on_replace_dialog_response(NULL, GEANY_RESPONSE_REPLACE, NULL);
}


static void
on_replace_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	gint idx = document_get_cur_idx();
	GtkWidget *entry_find = lookup_widget(GTK_WIDGET(widgets.replace_dialog), "entry_find");
	GtkWidget *entry_replace = lookup_widget(GTK_WIDGET(widgets.replace_dialog), "entry_replace");
	gint search_flags_re;
	gboolean search_backwards_re, search_replace_escape_re;
	gboolean close_window;
	gchar *find, *replace;

	if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
	{
		gtk_widget_hide(widgets.replace_dialog);
		return;
	}

	close_window = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_close")));
	search_backwards_re = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_back")));
	search_replace_escape_re = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_escape")));
	find = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find)))));
	replace = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace)))));

	search_flags_re = get_search_flags(widgets.replace_dialog);

	if ((response != GEANY_RESPONSE_FIND) && (search_flags_re & SCFIND_MATCHCASE)
		&& (strcmp(find, replace) == 0))
	{
		utils_beep();
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(widgets.replace_dialog, "entry_find"))->child));
		return;
	}

	ui_combo_box_add_to_history(GTK_COMBO_BOX(entry_find), find);
	ui_combo_box_add_to_history(GTK_COMBO_BOX(entry_replace), replace);

	if (search_replace_escape_re &&
		(! utils_str_replace_escape(find) || ! utils_str_replace_escape(replace)))
	{
		utils_beep();
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(widgets.replace_dialog, "entry_find"))->child));
		return;
	}

	switch (response)
	{
		case GEANY_RESPONSE_REPLACE_AND_FIND:
		{
			gint rep = document_replace_text(idx, find, replace, search_flags_re,
				search_backwards_re);
			if (rep != -1)
				document_find_text(idx, find, search_flags_re, search_backwards_re,
					TRUE, NULL);
			break;
		}
		case GEANY_RESPONSE_REPLACE:
		{
			document_replace_text(idx, find, replace, search_flags_re,
				search_backwards_re);
			break;
		}
		case GEANY_RESPONSE_FIND:
		{
			document_find_text(idx, find, search_flags_re, search_backwards_re, TRUE,
							   GTK_WIDGET(dialog));
			break;
		}
		case GEANY_RESPONSE_REPLACE_IN_FILE:
		{
			if (! document_replace_all(idx, find, replace, search_flags_re,
				search_replace_escape_re))
			{
				utils_beep();
			}
			break;
		}
		case GEANY_RESPONSE_REPLACE_IN_SESSION:
		{
			guint n, count = 0;

			// replace in all documents following notebook tab order
			for (n = 0; (gint) n < gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)); n++)
			{
				gint ix = document_get_n_idx(n);

				if (! doc_list[ix].is_valid) continue;

				if (document_replace_all(ix, find, replace, search_flags_re,
					search_replace_escape_re)) count++;
			}
			if (count == 0)
				utils_beep();

			ui_set_statusbar(FALSE, _("Replaced text in %u files."), count);
			// show which docs had replacements:
			gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_STATUS);

			ui_save_buttons_toggle(doc_list[idx].changed);	// update save all
			break;
		}
		case GEANY_RESPONSE_REPLACE_IN_SEL:
		{
			document_replace_sel(idx, find, replace, search_flags_re, search_replace_escape_re);
			break;
		}
	}
	switch (response)
	{
		case GEANY_RESPONSE_REPLACE_IN_SEL:
		case GEANY_RESPONSE_REPLACE_IN_FILE:
		case GEANY_RESPONSE_REPLACE_IN_SESSION:
			if (close_window)
				gtk_widget_hide(widgets.replace_dialog);
	}
	g_free(find);
	g_free(replace);
}


static gboolean
on_widget_key_pressed_set_focus(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	// catch tabulator key to set the focus in the replace entry instead of
	// setting it to the combo box
	if (event->keyval == GDK_Tab)
	{
		gtk_widget_grab_focus(GTK_WIDGET(user_data));
		return TRUE;
	}
	return FALSE;
}


static GString *get_grep_options(void)
{
	gboolean fgrep = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					lookup_widget(widgets.find_in_files_dialog, "radio_fgrep")));
	gboolean grep = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					lookup_widget(widgets.find_in_files_dialog, "radio_grep")));
	gboolean invert = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					lookup_widget(widgets.find_in_files_dialog, "check_invert")));
	gboolean case_sens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					lookup_widget(widgets.find_in_files_dialog, "check_case")));
	gboolean whole_word = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					lookup_widget(widgets.find_in_files_dialog, "check_wholeword")));
	gboolean recursive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					lookup_widget(widgets.find_in_files_dialog, "check_recursive")));
	gboolean extra = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
					lookup_widget(widgets.find_in_files_dialog, "check_extra")));
	GString *gstr = g_string_new("-nHI");	// line numbers, filenames, ignore binaries

	if (invert)
		g_string_append_c(gstr, 'v');
	if (! case_sens)
		g_string_append_c(gstr, 'i');
	if (whole_word)
		g_string_append_c(gstr, 'w');
	if (recursive)
		g_string_append_c(gstr, 'r');

	if (fgrep)
		g_string_append_c(gstr, 'F');
	else if (! grep)
		g_string_append_c(gstr, 'E');

	if (extra)
	{
		gchar *text = g_strdup(gtk_entry_get_text(GTK_ENTRY(
					lookup_widget(widgets.find_in_files_dialog, "entry_extra"))));

		text = g_strstrip(text);
		if (*text != 0)
		{
			g_string_append_c(gstr, ' ');
			g_string_append(gstr, text);
		}
		g_free(text);
	}
	return gstr;
}


static void
on_find_in_files_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		const gchar *search_text =
			gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(user_data))));
		GtkWidget *dir_combo =
			lookup_widget(widgets.find_in_files_dialog, "dir_combo");
		const gchar *utf8_dir =
			gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(dir_combo))));

		// update extra options pref
		g_free(search_prefs.fif_extra_options);
		search_prefs.fif_extra_options = g_strdup(gtk_entry_get_text(GTK_ENTRY(
					lookup_widget(widgets.find_in_files_dialog, "entry_extra"))));

		if (utf8_dir == NULL || utils_str_equal(utf8_dir, ""))
			ui_set_statusbar(FALSE, _("Invalid directory for find in files."));
		else if (search_text && *search_text)
		{
			gchar *locale_dir;
			GString *opts = get_grep_options();

			locale_dir = utils_get_locale_from_utf8(utf8_dir);

			if (search_find_in_files(search_text, locale_dir, opts->str))
			{
				ui_combo_box_add_to_history(GTK_COMBO_BOX(user_data), search_text);
				ui_combo_box_add_to_history(GTK_COMBO_BOX(dir_combo), utf8_dir);
				gtk_widget_hide(widgets.find_in_files_dialog);
			}
			g_free(locale_dir);
			g_string_free(opts, TRUE);
		}
		else
			ui_set_statusbar(FALSE, _("No text to find."));
	}
	else
		gtk_widget_hide(widgets.find_in_files_dialog);
}


static gboolean
search_find_in_files(const gchar *search_text, const gchar *dir, const gchar *opts)
{
	gchar **argv_prefix, **argv, **opts_argv;
	guint opts_argv_len, i;
	GPid child_pid;
	gint stdout_fd;
	GError *error = NULL;
	gboolean ret = FALSE;

	if (! search_text || ! *search_text || ! dir) return TRUE;

	if (! g_file_test(prefs.tools_grep_cmd, G_FILE_TEST_IS_EXECUTABLE))
	{
		ui_set_statusbar(TRUE, _("Cannot execute grep tool '%s';"
			" check the path setting in Preferences."), prefs.tools_grep_cmd);
		return FALSE;
	}

	opts_argv = g_strsplit(opts, " ", -1);
	opts_argv_len = g_strv_length(opts_argv);

	// set grep command and options
	argv_prefix = g_new0(gchar*, 1 + opts_argv_len + 3 + 1);	// last +1 for recursive arg

	argv_prefix[0] = g_strdup(prefs.tools_grep_cmd);
	for (i = 0; i < opts_argv_len; i++)
	{
		argv_prefix[i + 1] = g_strdup(opts_argv[i]);
	}
	g_strfreev(opts_argv);

	i++;	// correct for prefs.tools_grep_cmd
	argv_prefix[i++] = g_strdup("--");
	argv_prefix[i++] = g_strdup(search_text);

	// finally add the arguments(files to be searched)
	if (strstr(argv_prefix[1], "r"))	// recursive option set
	{
		argv_prefix[i++] = g_strdup(".");
		argv_prefix[i++] = NULL;
		argv = argv_prefix;
	}
	else
	{
		argv_prefix[i++] = NULL;
		argv = search_get_argv((const gchar**)argv_prefix, dir);
		g_strfreev(argv_prefix);
	}

	if (argv == NULL)	// no files
	{
		g_strfreev(argv);
		return FALSE;
	}

	gtk_list_store_clear(msgwindow.store_msg);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_MESSAGE);

	if (! g_spawn_async_with_pipes(dir, (gchar**)argv, NULL,
		G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD,
		NULL, NULL, &child_pid,
		NULL, &stdout_fd, NULL, &error))
	{
		geany_debug("%s: g_spawn_async_with_pipes() failed: %s", __func__, error->message);
		ui_set_statusbar(TRUE, _("Process failed (%s)"), error->message);
		g_error_free(error);
		ret = FALSE;
	}
	else
	{
		gchar *str, *utf8_str;

		g_free(msgwindow.find_in_files_dir);
		msgwindow.find_in_files_dir = g_strdup(dir);
		utils_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
			TRUE, search_read_io, NULL);
		g_child_watch_add(child_pid, search_close_pid, NULL);

		str = g_strdup_printf(_("%s %s -- %s (in directory: %s)"),
			prefs.tools_grep_cmd, opts, search_text, dir);
		utf8_str = utils_get_utf8_from_locale(str);
		msgwin_msg_add(COLOR_BLUE, -1, -1, utf8_str);
		utils_free_pointers(str, utf8_str, NULL);
		ret = TRUE;
	}
	g_strfreev(argv);
	return ret;
}


/* Creates an argument vector of strings, copying argv_prefix[] values for
 * the first arguments, then followed by filenames found in dir.
 * Returns NULL if no files were found, otherwise returned vector should be fully freed. */
static gchar **search_get_argv(const gchar **argv_prefix, const gchar *dir)
{
	guint prefix_len, list_len, i, j;
	gchar **argv;
	GSList *list, *item;
	GError *error = NULL;

	g_return_val_if_fail(dir != NULL, NULL);

	prefix_len = g_strv_length((gchar**)argv_prefix);
	list = utils_get_file_list(dir, &list_len, &error);
	if (error)
	{
		ui_set_statusbar(TRUE, _("Could not open directory (%s)"), error->message);
		g_error_free(error);
		return NULL;
	}
	if (list == NULL) return NULL;

	argv = g_new(gchar*, prefix_len + list_len + 1);

	for (i = 0; i < prefix_len; i++)
		argv[i] = g_strdup(argv_prefix[i]);

	item = list;
	for (j = 0; j < list_len; j++)
	{
		argv[i++] = item->data;
		item = g_slist_next(item);
	}
	argv[i] = NULL;

	g_slist_free(list);
	return argv;
}


static gboolean search_read_io              (GIOChannel *source,
                                             GIOCondition condition,
                                             gpointer data)
{
	if (condition & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg;

		while (g_io_channel_read_line(source, &msg, NULL, NULL, NULL) && msg)
		{
			msgwin_msg_add(COLOR_BLACK, -1, -1, g_strstrip(msg));
			g_free(msg);
		}
	}
	if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		return FALSE;

	return TRUE;
}


static void search_close_pid(GPid child_pid, gint status, gpointer user_data)
{
#ifdef G_OS_UNIX
	const gchar *msg = _("Search failed.");
	gint color = COLOR_DARK_RED;

	if (WIFEXITED(status))
	{
		switch (WEXITSTATUS(status))
		{
			case 0:
			{
				gint count = gtk_tree_model_iter_n_children(
					GTK_TREE_MODEL(msgwindow.store_msg), NULL) - 1;

				msgwin_msg_add_fmt(COLOR_BLUE, -1, -1,
					_("Search completed with %d matches."), count);
				ui_set_statusbar(FALSE, _("Search completed with %d matches."), count);
				break;
			}
			case 1:
				msg = _("No matches found.");
				color = COLOR_BLUE;
			default:
				msgwin_msg_add(color, -1, -1, msg);
				ui_set_statusbar(FALSE, "%s", msg);
				break;
		}
	}
#endif

	utils_beep();
	g_spawn_close_pid(child_pid);
}


static gint find_document_usage(gint idx, const gchar *search_text, gint flags)
{
	gchar *buffer, *short_file_name;
	struct TextToFind ttf;
	gint count = 0;

	g_return_val_if_fail(DOC_IDX_VALID(idx), 0);

	short_file_name = g_path_get_basename(DOC_FILENAME(idx));

	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(doc_list[idx].sci);
	ttf.lpstrText = (gchar *)search_text;
	while (1)
	{
		gint pos, line, start, find_len;

		pos = sci_find_text(doc_list[idx].sci, flags, &ttf);
		if (pos == -1)
			break;	// no more matches
		find_len = ttf.chrgText.cpMax - ttf.chrgText.cpMin;
		if (find_len == 0)
			break;	// Ignore regex ^ or $

		count++;
		line = sci_get_line_from_position(doc_list[idx].sci, pos);
		buffer = sci_get_line(doc_list[idx].sci, line);
		msgwin_msg_add_fmt(COLOR_BLACK, line + 1, idx,
			"%s:%d : %s", short_file_name, line + 1, g_strstrip(buffer));
		g_free(buffer);

		start = ttf.chrgText.cpMax + 1;
		ttf.chrg.cpMin = start;
	}
	g_free(short_file_name);
	return count;
}


void search_find_usage(const gchar *search_text, gint flags, gboolean in_session)
{
	gint idx;
	gboolean found = FALSE;

	idx = document_get_cur_idx();
	g_return_if_fail(DOC_IDX_VALID(idx));

	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_MESSAGE);
	gtk_list_store_clear(msgwindow.store_msg);

	if (! in_session)
	{	// use current document
		found = (find_document_usage(idx, search_text, flags) > 0);
	}
	else
	{
		guint i;
		for (i = 0; i < doc_array->len; i++)
		{
			if (doc_list[i].is_valid)
				if (find_document_usage(i, search_text, flags) > 0) found = TRUE;
		}
	}

	if (! found) // no matches were found
	{
		ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), search_text);
		msgwin_msg_add_fmt(COLOR_BLUE, -1, -1, _("No matches found for \"%s\"."), search_text);
	}
	else
	{
		gint count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(msgwindow.store_msg), NULL);

		ui_set_statusbar(FALSE, _("Found %d matches for \"%s\"."), count, search_text);
		msgwin_msg_add_fmt(COLOR_BLUE, -1, -1, _("Found %d matches for \"%s\"."), count,
			search_text);
	}
}


