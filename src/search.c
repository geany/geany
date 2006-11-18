/*
 *      search.c - this file is part of Geany, a fast and lightweight IDE
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

#include <gdk/gdkkeysyms.h>

#include "geany.h"
#include "search.h"
#include "support.h"
#include "utils.h"
#include "msgwindow.h"
#include "document.h"
#include "sciwrappers.h"
#include "ui_utils.h"
#include "sci_cb.h"

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


typedef enum
{
	FIF_CASE_SENSITIVE	= 1 << 0,
	FIF_WHOLE_WORD		= 1 << 1,
	FIF_INVERT_MATCH	= 1 << 2
} fif_options;

typedef enum
{
	FIF_FGREP,
	FIF_GREP,
	FIF_EGREP
} fif_match_type;


GeanySearchData search_data;

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

static GSList *search_get_file_list(const gchar *path, guint *length);


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
search_find_in_files(const gchar *search_text, const gchar *dir, fif_match_type regex_opt,
		fif_options opts);

static void on_open_dir_dialog_clicked(GtkButton *button, gpointer user_data);


void search_init()
{
	widgets.find_dialog		= NULL;
	widgets.replace_dialog		= NULL;
	widgets.find_in_files_dialog = NULL;
	search_data.text		= NULL;
}


#define FREE_WIDGET(wid) \
	if (wid && GTK_IS_WIDGET(wid)) gtk_widget_destroy(wid);

void search_finalize()
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
		check_back = gtk_check_button_new_with_mnemonic(_("_Search backwards"));
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

	checkbox1 = gtk_check_button_new_with_mnemonic(_("_Case sensitive"));
	g_object_set_data_full(G_OBJECT(dialog), "check_case",
					gtk_widget_ref(checkbox1), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);

	checkbox2 = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
	g_object_set_data_full(G_OBJECT(dialog), "check_word",
					gtk_widget_ref(checkbox2), (GDestroyNotify)gtk_widget_unref);
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);

	checkbox5 = gtk_check_button_new_with_mnemonic(_("Match only word s_tart"));
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


static gchar *get_default_text(gint idx)
{
	gchar *s = NULL;
	
	if (sci_get_lines_selected(doc_list[idx].sci) == 1)
	{
		s = g_malloc(sci_get_selected_text_length(doc_list[idx].sci));
		sci_get_selected_text(doc_list[idx].sci, s);
	}
	else if (sci_get_lines_selected(doc_list[idx].sci) == 0)
	{	// use the word at current cursor position
		static gchar word[GEANY_MAX_WORD_LENGTH];
		sci_cb_find_current_word(doc_list[idx].sci, -1, word, sizeof(word));
		if (word[0] != '\0') s = g_strdup(word);
	}
	
	return s;
}


void search_show_find_dialog()
{
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	sel = get_default_text(idx);

	if (widgets.find_dialog == NULL)
	{
		GtkWidget *label, *entry, *sbox, *vbox;
		GtkWidget *exp, *bbox, *button, *check_close;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		widgets.find_dialog = gtk_dialog_new_with_buttons(_("Find"),
			GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(widgets.find_dialog));
		gtk_box_set_spacing(GTK_BOX(vbox), 9);

		button = ui_button_new_with_image(GTK_STOCK_GO_BACK, _("_Previous"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.find_dialog), button,
			GEANY_RESPONSE_FIND_PREVIOUS);
		g_object_set_data_full(G_OBJECT(widgets.find_dialog), "btn_previous",
						gtk_widget_ref(button), (GDestroyNotify)gtk_widget_unref);

		button = ui_button_new_with_image(GTK_STOCK_GO_FORWARD, _("_Next"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.find_dialog), button,
			GEANY_RESPONSE_FIND);

		label = gtk_label_new(_("Search for:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		entry = gtk_combo_box_entry_new_text();
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
				G_CALLBACK(gtk_widget_hide), NULL);

		sbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(sbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(sbox), entry, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), sbox, TRUE, FALSE, 0);

		gtk_container_add(GTK_CONTAINER(vbox),
			add_find_checkboxes(GTK_DIALOG(widgets.find_dialog)));

		// Now add the multiple match options
		exp = gtk_expander_new(_("Find All"));
		bbox = gtk_hbutton_box_new();

#if 0
		button = gtk_button_new_with_mnemonic(_("_Mark"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_find_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_MARK));
#endif	// not implemented yet

		button = gtk_button_new_with_mnemonic(_("In Sessi_on"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_find_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_FIND_IN_SESSION));

		button = gtk_button_new_with_mnemonic(_("In F_ile"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_find_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_FIND_IN_FILE));

		// close window checkbox
		check_close = gtk_check_button_new_with_mnemonic(_("Close _dialog"));
		g_object_set_data_full(G_OBJECT(widgets.find_dialog), "check_close",
						gtk_widget_ref(check_close), (GDestroyNotify) gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_close), FALSE);
		gtk_tooltips_set_tip(tooltips, check_close,
				_("Disable this option to do not close this dialog."), NULL);
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
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(
							lookup_widget(widgets.find_dialog, "entry"))->child), sel);
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(widgets.find_dialog, "entry"))->child));
		gtk_widget_show(widgets.find_dialog);
	}
	g_free(sel);
}


static void send_replace_dialog_response(GtkButton *button, gpointer user_data)
{
	gtk_dialog_response(GTK_DIALOG(widgets.replace_dialog), GPOINTER_TO_INT(user_data));
}


void search_show_replace_dialog()
{
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	sel = get_default_text(idx);

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

		button = gtk_button_new_from_stock(GTK_STOCK_FIND);
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.replace_dialog), button,
			GEANY_RESPONSE_FIND);
		button = gtk_button_new_with_mnemonic(_("_Replace"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.replace_dialog), button,
			GEANY_RESPONSE_REPLACE);
		button = gtk_button_new_with_mnemonic(_("Re_place & Find"));
		gtk_dialog_add_action_widget(GTK_DIALOG(widgets.replace_dialog), button,
			GEANY_RESPONSE_REPLACE_AND_FIND);

		label_find = gtk_label_new(_("Search for:"));
		gtk_misc_set_alignment(GTK_MISC(label_find), 0, 0.5);

		label_replace = gtk_label_new(_("Replace with:"));
		gtk_misc_set_alignment(GTK_MISC(label_replace), 0, 0.5);

		entry_find = gtk_combo_box_entry_new_text();
		gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 50);
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(entry_find)->child), sel);
		g_object_set_data_full(G_OBJECT(widgets.replace_dialog), "entry_find",
						gtk_widget_ref(entry_find), (GDestroyNotify)gtk_widget_unref);

		entry_replace = gtk_combo_box_entry_new_text();
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
				G_CALLBACK(gtk_widget_hide), NULL);

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
		exp = gtk_expander_new(_("Replace All"));
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

		button = gtk_button_new_with_mnemonic(_("In F_ile"));
		gtk_container_add(GTK_CONTAINER(bbox), button);
		g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(send_replace_dialog_response),
			GINT_TO_POINTER(GEANY_RESPONSE_REPLACE_IN_FILE));

		// close window checkbox
		check_close = gtk_check_button_new_with_mnemonic(_("Close _dialog"));
		g_object_set_data_full(G_OBJECT(widgets.replace_dialog), "check_close",
						gtk_widget_ref(check_close), (GDestroyNotify) gtk_widget_unref);
		gtk_button_set_focus_on_click(GTK_BUTTON(check_close), FALSE);
		gtk_tooltips_set_tip(tooltips, check_close,
				_("Disable this option to do not close this dialog."), NULL);
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
		if (sel) gtk_entry_set_text(GTK_ENTRY(GTK_BIN(
							lookup_widget(widgets.replace_dialog, "entry_find"))->child), sel);
		gtk_widget_grab_focus(GTK_WIDGET(GTK_BIN(lookup_widget(widgets.replace_dialog, "entry_find"))->child));
		gtk_widget_show(widgets.replace_dialog);
	}
	g_free(sel);
}


void search_show_find_in_files_dialog()
{
	static GtkWidget *combo = NULL;
	static GtkWidget *dir_combo;
	GtkWidget *entry; // the child GtkEntry of combo (or dir_combo)
	GtkWidget *dirbtn, *openimg;
	gint idx = document_get_cur_idx();
	gchar *sel = NULL;
	gchar *cur_dir;

	if (idx == -1 || ! doc_list[idx].is_valid) return;

	if (widgets.find_in_files_dialog == NULL)
	{
		GtkWidget *label, *label1, *checkbox1, *checkbox2, *check_wholeword;
		GtkWidget *dbox, *sbox, *cbox, *rbox, *rbtn, *hbox, *vbox;
		GtkSizeGroup *size_group;
		GtkTooltips *tooltips = GTK_TOOLTIPS(lookup_widget(app->window, "tooltips"));

		widgets.find_in_files_dialog = gtk_dialog_new_with_buttons(
			_("Find in files"), GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
		vbox = ui_dialog_vbox_new(GTK_DIALOG(widgets.find_in_files_dialog));
		gtk_box_set_spacing(GTK_BOX(vbox), 9);

		gtk_dialog_add_button(GTK_DIALOG(widgets.find_in_files_dialog), "gtk-find", GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_default_response(GTK_DIALOG(widgets.find_in_files_dialog),
			GTK_RESPONSE_ACCEPT);

		label1 = gtk_label_new(_("Directory:"));
		gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);

		dir_combo = gtk_combo_box_entry_new_text();
		entry = gtk_bin_get_child(GTK_BIN(dir_combo));
		gtk_entry_set_max_length(GTK_ENTRY(entry), 248);
		gtk_entry_set_width_chars(GTK_ENTRY(entry), 50);
		g_object_set_data_full(G_OBJECT(widgets.find_in_files_dialog), "dir_combo",
						gtk_widget_ref(dir_combo), (GDestroyNotify)gtk_widget_unref);

		dirbtn = gtk_button_new();
		openimg = gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(dirbtn), openimg);
		g_signal_connect(G_OBJECT(dirbtn), "clicked", G_CALLBACK(on_open_dir_dialog_clicked),
			NULL);

		dbox = gtk_hbox_new(FALSE, 6);
		gtk_box_pack_start(GTK_BOX(dbox), label1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(dbox), dir_combo, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(dbox), dirbtn, FALSE, FALSE, 0);

		label = gtk_label_new(_("Search for:"));
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

		combo = gtk_combo_box_entry_new_text();
		entry = gtk_bin_get_child(GTK_BIN(combo));
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
		rbtn = gtk_radio_button_new_with_mnemonic(NULL, _("_Fixed strings"));
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

		checkbox1 = gtk_check_button_new_with_mnemonic(_("_Case sensitive"));
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

		hbox = gtk_hbox_new(FALSE, 6);
		gtk_container_add(GTK_CONTAINER(hbox), rbox);
		gtk_container_add(GTK_CONTAINER(hbox), cbox);

		gtk_box_pack_start(GTK_BOX(vbox), dbox, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), sbox, TRUE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(vbox), hbox);

		g_signal_connect((gpointer) dir_combo, "key-press-event",
				G_CALLBACK(on_widget_key_pressed_set_focus), combo);
		g_signal_connect((gpointer) widgets.find_in_files_dialog, "response",
				G_CALLBACK(on_find_in_files_dialog_response), combo);
		g_signal_connect((gpointer) widgets.find_in_files_dialog, "delete_event",
				G_CALLBACK(gtk_widget_hide), NULL);

		gtk_widget_show_all(widgets.find_in_files_dialog);
	}

	sel = get_default_text(idx);

	entry = GTK_BIN(combo)->child;
	if (sel) gtk_entry_set_text(GTK_ENTRY(entry), sel);
	g_free(sel);

	entry = GTK_BIN(dir_combo)->child;
	cur_dir = utils_get_current_file_dir();
	if (cur_dir)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), cur_dir);
		g_free(cur_dir);
	}

	// put the focus to the directory entry if it is empty
	if (utils_str_equal(gtk_entry_get_text(GTK_ENTRY(entry)), ""))
		gtk_widget_grab_focus(dir_combo);
	else
		gtk_widget_grab_focus(combo);

	gtk_widget_show(widgets.find_in_files_dialog);
}


static void on_open_dir_dialog_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog = gtk_file_chooser_dialog_new(_("Select folder"),
		GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_OK, NULL);
	GtkWidget *dir_combo =
		lookup_widget(widgets.find_in_files_dialog, "dir_combo");
	GtkWidget *entry_dir = gtk_bin_get_child(GTK_BIN(dir_combo));
	gchar *dir_locale;
	const gchar *entry_text;

	entry_text = gtk_entry_get_text(GTK_ENTRY(entry_dir));
	dir_locale = utils_get_locale_from_utf8(entry_text);
	if (g_file_test(dir_locale, G_FILE_TEST_IS_DIR))
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir_locale);
	g_free(dir_locale);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
	{
		gchar *dir_utf8;
		dir_locale = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog));
		dir_utf8 = utils_get_utf8_from_locale(dir_locale);
		g_free(dir_locale);
		gtk_entry_set_text(GTK_ENTRY(entry_dir), dir_utf8);
		g_free(dir_utf8);
	}
	gtk_widget_destroy(dialog);
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


static void
on_find_entry_activate(GtkEntry *entry, gpointer user_data)
{
	on_find_dialog_response(NULL, GEANY_RESPONSE_FIND,
				lookup_widget(GTK_WIDGET(entry), "entry"));
}


static void
on_find_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	if (response == GTK_RESPONSE_CANCEL)
		gtk_widget_hide(widgets.find_dialog);
	else
	{
		gint idx = document_get_cur_idx();
		gboolean search_replace_escape;
		gboolean
			fl1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_case"))),
			fl2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_word"))),
			fl3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_regexp"))),
			fl4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_wordstart"))),
			check_close = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_close")));
		search_replace_escape = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						lookup_widget(GTK_WIDGET(widgets.find_dialog), "check_escape")));
		search_data.backwards = FALSE;

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
		search_data.flags = (fl1 ? SCFIND_MATCHCASE : 0) |
				(fl2 ? SCFIND_WHOLEWORD : 0) |
				(fl3 ? SCFIND_REGEXP | SCFIND_POSIX: 0) |
				(fl4 ? SCFIND_WORDSTART : 0);
		
		switch (response)
		{
			case GEANY_RESPONSE_FIND:
			case GEANY_RESPONSE_FIND_PREVIOUS:
			document_find_text(idx, search_data.text, search_data.flags,
				(response == GEANY_RESPONSE_FIND_PREVIOUS), TRUE);
			check_close = FALSE;
			if (app->pref_main_suppress_search_dialogs)
				check_close = TRUE;
			break;

			case GEANY_RESPONSE_FIND_IN_FILE:
			search_find_usage(search_data.text, search_data.flags, FALSE);
			break;

			case GEANY_RESPONSE_FIND_IN_SESSION:
			search_find_usage(search_data.text, search_data.flags, TRUE);
			break;

			case GEANY_RESPONSE_MARK:
			// TODO
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
	gboolean fl1, fl2, fl3, fl4;
	gboolean close_window;
	gchar *find, *replace;

	if (response == GTK_RESPONSE_CANCEL)
	{
		gtk_widget_hide(widgets.replace_dialog);
		return;
	}

	fl1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_case")));
	fl2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_word")));
	fl3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_regexp")));
	fl4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_wordstart")));
	close_window = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_close")));
	search_backwards_re = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_back")));
	search_replace_escape_re = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				lookup_widget(GTK_WIDGET(widgets.replace_dialog), "check_escape")));
	find = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find)))));
	replace = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace)))));

	if ((response != GEANY_RESPONSE_FIND) && (! fl1) && (strcasecmp(find, replace) == 0))
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

	search_flags_re = (fl1 ? SCFIND_MATCHCASE : 0) |
					  (fl2 ? SCFIND_WHOLEWORD : 0) |
					  (fl3 ? SCFIND_REGEXP | SCFIND_POSIX : 0) |
					  (fl4 ? SCFIND_WORDSTART : 0);

	switch (response)
	{
		case GEANY_RESPONSE_REPLACE_AND_FIND:
		{
			gint rep = document_replace_text(idx, find, replace, search_flags_re,
				search_backwards_re);
			if (rep != -1)
				document_find_text(idx, find, search_flags_re, search_backwards_re,
					TRUE);
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
			document_find_text(idx, find, search_flags_re, search_backwards_re, TRUE);
			break;
		}
		case GEANY_RESPONSE_REPLACE_IN_FILE:
		{
			document_replace_all(idx, find, replace, search_flags_re, search_replace_escape_re);
			if (close_window) gtk_widget_hide(widgets.replace_dialog);
			break;
		}
		case GEANY_RESPONSE_REPLACE_IN_SESSION:
		{
			guint i;
			for (i = 0; i < doc_array->len; i++)
			{
				if (! doc_list[i].is_valid) continue;

				document_replace_all(i, find, replace, search_flags_re, search_replace_escape_re);
			}
			ui_save_buttons_toggle(doc_list[idx].changed);	// update save all
			if (close_window) gtk_widget_hide(widgets.replace_dialog);
			break;
		}
		case GEANY_RESPONSE_REPLACE_IN_SEL:
		{
			document_replace_sel(idx, find, replace, search_flags_re, search_replace_escape_re);
			if (close_window) gtk_widget_hide(widgets.replace_dialog);
			break;
		}
		default:
		geany_debug("Missing reponse in %s.", G_GNUC_FUNCTION);
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

		if (utf8_dir == NULL || utils_str_equal(utf8_dir, ""))
			msgwin_status_add(_("Invalid directory for find in files."));
		else if (search_text && *search_text)
		{
			gchar *locale_dir;
			gboolean invert = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
							lookup_widget(widgets.find_in_files_dialog, "check_invert")));
			gboolean case_sens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
							lookup_widget(widgets.find_in_files_dialog, "check_case")));
			gboolean whole_word = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
							lookup_widget(widgets.find_in_files_dialog, "check_wholeword")));
			gint opts = (invert ? FIF_INVERT_MATCH : 0) |
						(case_sens ? FIF_CASE_SENSITIVE : 0) |
						(whole_word ? FIF_WHOLE_WORD : 0);
			gboolean fgrep = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
							lookup_widget(widgets.find_in_files_dialog, "radio_fgrep")));
			gboolean grep = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
							lookup_widget(widgets.find_in_files_dialog, "radio_grep")));
			gint regex_opt = fgrep ? FIF_FGREP : (grep ? FIF_GREP : FIF_EGREP);

			locale_dir = utils_get_locale_from_utf8(utf8_dir);

			if (search_find_in_files(search_text, locale_dir, regex_opt, opts))
			{
				ui_combo_box_add_to_history(GTK_COMBO_BOX(user_data), search_text);
				ui_combo_box_add_to_history(GTK_COMBO_BOX(dir_combo), utf8_dir);
				gtk_widget_hide(widgets.find_in_files_dialog);
			}
			g_free(locale_dir);
		}
		else
			msgwin_status_add(_("No text to find."));
	}
	else
		gtk_widget_hide(widgets.find_in_files_dialog);
}


static gboolean
search_find_in_files(const gchar *search_text, const gchar *dir, fif_match_type regex_opt,
		fif_options opts)
{
	gchar **argv_prefix;
	gchar **grep_cmd_argv;
	gchar **argv;
	gchar grep_opts[] = "-nHI    ";
	GPid child_pid;
	gint stdout_fd, stdin_fd, grep_cmd_argv_len, i, grep_opts_len = 4;
	GError *error = NULL;
	gboolean ret = FALSE;

	if (! search_text || ! *search_text || ! dir) return TRUE;

	// first process the grep command (need to split it in a argv because it can be "grep -I")
	grep_cmd_argv = g_strsplit(app->tools_grep_cmd, " ", -1);
	grep_cmd_argv_len = g_strv_length(grep_cmd_argv);

	if (! g_file_test(grep_cmd_argv[0], G_FILE_TEST_IS_EXECUTABLE))
	{
		msgwin_status_add(_("Cannot execute grep tool '%s';"
			" check the path setting in Preferences."), app->tools_grep_cmd);
		return FALSE;
	}

	gtk_list_store_clear(msgwindow.store_msg);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_MESSAGE);

	switch (regex_opt)
	{
		default:
		case FIF_FGREP:	grep_opts[grep_opts_len++] = 'F'; break;
		case FIF_GREP:	break;
		case FIF_EGREP:	grep_opts[grep_opts_len++] = 'E'; break;
	}

	if (! (opts & FIF_CASE_SENSITIVE))
		grep_opts[grep_opts_len++] = 'i';
	if (opts & FIF_WHOLE_WORD)
		grep_opts[grep_opts_len++] = 'w';
	if (opts & FIF_INVERT_MATCH)
		grep_opts[grep_opts_len++] = 'v';
	grep_opts[grep_opts_len] = '\0';

	// set grep command and options
	argv_prefix = g_new0(gchar*, grep_cmd_argv_len + 4);
	for (i = 0; i < grep_cmd_argv_len; i++)
	{
		argv_prefix[i] = g_strdup(grep_cmd_argv[i]);
	}
	argv_prefix[grep_cmd_argv_len] = g_strdup(grep_opts);
	argv_prefix[grep_cmd_argv_len + 1] = g_strdup("--");
	argv_prefix[grep_cmd_argv_len + 2] = g_strdup(search_text);
	argv_prefix[grep_cmd_argv_len + 3] = NULL;
	g_strfreev(grep_cmd_argv);

	// finally add the arguments(files to be searched)
	argv = search_get_argv((const gchar**)argv_prefix, dir);
	g_strfreev(argv_prefix);

	if (argv == NULL) return FALSE;

	if (! g_spawn_async_with_pipes(dir, (gchar**)argv, NULL,
		G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD,
		NULL, NULL, &child_pid,
		&stdin_fd, &stdout_fd, NULL, &error))
	{
		geany_debug("%s: g_spawn_async_with_pipes() failed: %s", __func__, error->message);
		msgwin_status_add(_("Process failed (%s)"), error->message);
		g_error_free(error);
		ret = FALSE;
	}
	else
	{
		g_free(msgwindow.find_in_files_dir);
		msgwindow.find_in_files_dir = g_strdup(dir);
		utils_set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
			search_read_io, NULL);
		g_child_watch_add(child_pid, search_close_pid, NULL);
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

	g_return_val_if_fail(dir != NULL, NULL);

	prefix_len = g_strv_length((gchar**)argv_prefix);
	list = search_get_file_list(dir, &list_len);
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


/* Gets a sorted list of files in the current directory.
 * The list and the data in the list should be freed after use.
 * Returns: The list or NULL if no files found.
 * *length is set to the number of non-NULL data items in the list. */
static GSList *search_get_file_list(const gchar *path, guint *length)
{
	GError *error = NULL;
	GSList *list = NULL;
	guint len = 0;
	GDir *dir;

	g_return_val_if_fail(path != NULL, NULL);

	dir = g_dir_open(path, 0, &error);
	if (error)
	{
		msgwin_status_add(_("Could not open directory (%s)"), error->message);
		g_error_free(error);
		*length = 0;
		return NULL;
	}

	while (1)
	{
		const gchar *filename = g_dir_read_name(dir);
		if (filename == NULL) break;

		list = g_slist_insert_sorted(list, g_strdup(filename), (GCompareFunc) strcmp);
		len++;
	}
	g_dir_close(dir);

	*length = len;
	return list;
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
			msgwin_msg_add(-1, -1, g_strstrip(msg));
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
	gchar *msg = _("Search failed.");

	if (WIFEXITED(status))
	{
		switch (WEXITSTATUS(status))
		{
			case 0: msg = _("Search completed."); break;
			case 1: msg = _("No matches found."); break;
			default: break;
		}
	}

	msgwin_msg_add(-1, -1, msg);
#endif
	utils_beep();
	g_spawn_close_pid(child_pid);
}


static gint find_document_usage(gint idx, const gchar *search_text, gint flags)
{
	gchar *buffer, *short_file_name, *string;
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
		buffer = sci_get_line(doc_list[idx].sci, line);

		if (doc_list[idx].file_name == NULL)
			short_file_name = g_strdup(GEANY_STRING_UNTITLED);
		else
			short_file_name = g_path_get_basename(doc_list[idx].file_name);
		string = g_strdup_printf("%s:%d : %s", short_file_name, line + 1, g_strstrip(buffer));
		msgwin_msg_add(line + 1, idx, string);

		g_free(buffer);
		g_free(short_file_name);
		g_free(string);
		ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
		count++;
	}
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
		for(i = 0; i < doc_array->len; i++)
		{
			if (doc_list[i].is_valid)
				if (find_document_usage(i, search_text, flags) > 0) found = TRUE;
		}
	}

	if (! found) // no matches were found
	{
		gchar *text = g_strdup_printf(_("No matches found for '%s'."), search_text);
		ui_set_statusbar(text, FALSE);
		msgwin_msg_add(-1, -1, text);
		g_free(text);
	}
}
