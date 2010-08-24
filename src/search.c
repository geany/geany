/*
 *      search.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2010 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
#include "document.h"
#include "msgwindow.h"
#include "sciwrappers.h"
#include "ui_utils.h"
#include "editor.h"
#include "encodings.h"
#include "project.h"
#include "keyfile.h"
#include "stash.h"

#include <unistd.h>
#include <string.h>
#include <ctype.h>

#ifdef G_OS_UNIX
# include <sys/types.h>
# include <sys/wait.h>
#endif

#ifdef HAVE_REGEX_H
# include <regex.h>
#else
# include "gnuregex.h"
#endif


enum
{
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
GeanySearchPrefs search_prefs;


static struct
{
	gboolean fif_regexp;
	gboolean fif_case_sensitive;
	gboolean fif_match_whole_word;
	gboolean fif_invert_results;
	gboolean fif_recursive;
	gboolean fif_use_extra_options;
	gchar *fif_extra_options;
	gboolean fif_use_files;
	gchar *fif_files;
}
settings = {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, NULL, FALSE, NULL};

static StashGroup *fif_prefs = NULL;


static struct
{
	GtkWidget	*dialog;
	GtkWidget	*entry;
	gboolean	all_expanded;
	gint		position[2]; /* x, y */
}
find_dlg = {NULL, NULL, FALSE, {0, 0}};

static struct
{
	GtkWidget	*dialog;
	GtkWidget	*find_entry;
	GtkWidget	*replace_entry;
	gboolean	all_expanded;
	gint		position[2]; /* x, y */
}
replace_dlg = {NULL, NULL, NULL, FALSE, {0, 0}};

static struct
{
	GtkWidget	*dialog;
	GtkWidget	*dir_combo;
	GtkWidget	*files_combo;
	GtkWidget	*search_combo;
	GtkWidget	*encoding_combo;
	gint		position[2]; /* x, y */
}
fif_dlg = {NULL, NULL, NULL, NULL, NULL, {0, 0}};


static gboolean search_read_io(GIOChannel *source, GIOCondition condition, gpointer data);
static gboolean search_read_io_stderr(GIOChannel *source, GIOCondition condition, gpointer data);

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

static void
on_find_in_files_dialog_response(GtkDialog *dialog, gint response, gpointer user_data);

static gboolean
search_find_in_files(const gchar *utf8_search_text, const gchar *dir, const gchar *opts,
	const gchar *enc);


static void init_prefs(void)
{
	StashGroup *group;

	group = stash_group_new("search");
	configuration_add_pref_group(group, TRUE);
	stash_group_add_toggle_button(group, &search_prefs.use_current_file_dir,
		"pref_search_current_file_dir", TRUE, "check_fif_current_dir");
	stash_group_add_boolean(group, &find_dlg.all_expanded, "find_all_expanded", FALSE);
	stash_group_add_boolean(group, &replace_dlg.all_expanded, "replace_all_expanded", FALSE);
	/* dialog positions */
	stash_group_add_integer(group, &find_dlg.position[0], "position_find_x", -1);
	stash_group_add_integer(group, &find_dlg.position[1], "position_find_y", -1);
	stash_group_add_integer(group, &replace_dlg.position[0], "position_replace_x", -1);
	stash_group_add_integer(group, &replace_dlg.position[1], "position_replace_y", -1);
	stash_group_add_integer(group, &fif_dlg.position[0], "position_fif_x", -1);
	stash_group_add_integer(group, &fif_dlg.position[1], "position_fif_y", -1);

	group = stash_group_new("search");
	fif_prefs = group;
	configuration_add_pref_group(group, FALSE);
	stash_group_add_toggle_button(group, &settings.fif_regexp,
		"fif_regexp", FALSE, "check_regexp");
	stash_group_add_toggle_button(group, &settings.fif_case_sensitive,
		"fif_case_sensitive", TRUE, "check_case");
	stash_group_add_toggle_button(group, &settings.fif_match_whole_word,
		"fif_match_whole_word", FALSE, "check_wholeword");
	stash_group_add_toggle_button(group, &settings.fif_invert_results,
		"fif_invert_results", FALSE, "check_invert");
	stash_group_add_toggle_button(group, &settings.fif_recursive,
		"fif_recursive", FALSE, "check_recursive");
	stash_group_add_entry(group, &settings.fif_extra_options,
		"fif_extra_options", "", "entry_extra");
	stash_group_add_toggle_button(group, &settings.fif_use_extra_options,
		"fif_use_extra_options", FALSE, "check_extra");
	stash_group_add_entry(group, &settings.fif_files,
		"fif_files", "", "entry_files");
	stash_group_add_toggle_button(group, &settings.fif_use_files,
		"fif_use_files", FALSE, "check_files");
}


void search_init(void)
{
	search_data.text = NULL;
	init_prefs();
}


#define FREE_WIDGET(wid) \
	if (wid && GTK_IS_WIDGET(wid)) gtk_widget_destroy(wid);

void search_finalize(void)
{
	FREE_WIDGET(find_dlg.dialog);
	FREE_WIDGET(replace_dlg.dialog);
	FREE_WIDGET(fif_dlg.dialog);
	g_free(search_data.text);
}


static GtkWidget *add_find_checkboxes(GtkDialog *dialog)
{
	GtkWidget *checkbox1, *checkbox2, *check_regexp, *check_back, *checkbox5,
			  *checkbox7, *hbox, *fbox, *mbox;

	check_regexp = gtk_check_button_new_with_mnemonic(_("_Use regular expressions"));
	ui_hookup_widget(dialog, check_regexp, "check_regexp");
	gtk_button_set_focus_on_click(GTK_BUTTON(check_regexp), FALSE);
	ui_widget_set_tooltip_text(check_regexp, _("Use POSIX-like regular expressions. "
		"For detailed information about using regular expressions, please read the documentation."));
	g_signal_connect(check_regexp, "toggled",
		G_CALLBACK(on_find_replace_checkbutton_toggled), GTK_WIDGET(dialog));

	if (dialog != GTK_DIALOG(find_dlg.dialog))
	{
		check_back = gtk_check_button_new_with_mnemonic(_("Search _backwards"));
		ui_hookup_widget(dialog, check_back, "check_back");
		gtk_button_set_focus_on_click(GTK_BUTTON(check_back), FALSE);
	}
	else
	{	/* align the two checkboxes at the top of the hbox */
		GtkSizeGroup *label_size;
		check_back = gtk_label_new(NULL);
		label_size = gtk_size_group_new(GTK_SIZE_GROUP_VERTICAL);
		gtk_size_group_add_widget(GTK_SIZE_GROUP(label_size), check_back);
		gtk_size_group_add_widget(GTK_SIZE_GROUP(label_size), check_regexp);
		g_object_unref(label_size);
	}
	checkbox7 = gtk_check_button_new_with_mnemonic(_("Use _escape sequences"));
	ui_hookup_widget(dialog, checkbox7, "check_escape");
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox7), FALSE);
	ui_widget_set_tooltip_text(checkbox7,
		_("Replace \\\\, \\t, \\n, \\r and \\uXXXX (Unicode chararacters) with the "
		  "corresponding control characters"));

	/* Search features */
	fbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(fbox), check_regexp);
	gtk_container_add(GTK_CONTAINER(fbox), checkbox7);
	gtk_container_add(GTK_CONTAINER(fbox), check_back);

	checkbox1 = gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
	ui_hookup_widget(dialog, checkbox1, "check_case");
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);

	checkbox2 = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
	ui_hookup_widget(dialog, checkbox2, "check_word");
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);

	checkbox5 = gtk_check_button_new_with_mnemonic(_("Match from s_tart of word"));
	ui_hookup_widget(dialog, checkbox5, "check_wordstart");
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox5), FALSE);

	/* Matching options */
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
	gtk_dialog_response(GTK_DIALOG(find_dlg.dialog), GPOINTER_TO_INT(user_data));
}


/* store text, clear search flags so we can use Search->Find Next/Previous */
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
void search_find_selection(GeanyDocument *doc, gboolean search_backwards)
{
	gchar *s = NULL;
#ifdef G_OS_UNIX
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
#endif

	g_return_if_fail(doc != NULL);

#ifdef G_OS_UNIX
	s = gtk_clipboard_wait_for_text(clipboard);
	if (s)
	{
		if (strchr(s,'\n') || strchr(s, '\r'))
		{
			g_free(s);
			s = NULL;
		};
	}
#endif

	if (!s)
		s = editor_get_default_selection(doc->editor, TRUE, NULL);

	if (s)
	{
		setup_find_next(s);	/* allow find next/prev */

		if (document_find_text(doc, s, 0, search_backwards, FALSE, NULL) > -1)
			editor_display_current_line(doc->editor, 0.3F);
		g_free(s);
	}
}


/* this will load a GTK rc style to set a monospace font for text fields(GtkEntry) in all
 * search dialogs. This needs to be done only once.
 * The monospace font should increase readibility of regular expressions containing spaces, points,
 * commas and similar (#1907117). */
static void load_monospace_style(void)
{
	static const gchar *rcstyle =
		"style \"geany-monospace\"\n" \
		"{\n" \
		"    font_name=\"Monospace\"\n" \
		"}\n" \
		"widget \"GeanyDialogSearch.*.GtkEntry\" style \"geany-monospace\"";
	static gboolean load = TRUE;

	if (load)
	{
		gtk_rc_parse_string(rcstyle);
		load = FALSE;
	}
}


static void on_expander_activated(GtkExpander *exp, gpointer data)
{
	gboolean *setting = data;

	*setting = gtk_expander_get_expanded(exp);
}


static void create_find_dialog(void)
{
	GtkWidget *label, *entry, *sbox, *vbox;
	GtkWidget *exp, *bbox, *button, *check_close;

	load_monospace_style();

	find_dlg.dialog = gtk_dialog_new_with_buttons(_("Find"),
		GTK_WINDOW(main_widgets.window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(find_dlg.dialog));
	gtk_widget_set_name(find_dlg.dialog, "GeanyDialogSearch");
	gtk_box_set_spacing(GTK_BOX(vbox), 9);

	button = ui_button_new_with_image(GTK_STOCK_GO_BACK, _("_Previous"));
	gtk_dialog_add_action_widget(GTK_DIALOG(find_dlg.dialog), button,
		GEANY_RESPONSE_FIND_PREVIOUS);
	ui_hookup_widget(find_dlg.dialog, button, "btn_previous");

	button = ui_button_new_with_image(GTK_STOCK_GO_FORWARD, _("_Next"));
	gtk_dialog_add_action_widget(GTK_DIALOG(find_dlg.dialog), button,
		GEANY_RESPONSE_FIND);

	label = gtk_label_new_with_mnemonic(_("_Search for:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	entry = gtk_combo_box_entry_new_text();
	ui_entry_add_clear_icon(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 248);
	gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))), 50);
	find_dlg.entry = GTK_BIN(entry)->child;
	ui_hookup_widget(find_dlg.dialog, entry, "entry");

	g_signal_connect(gtk_bin_get_child(GTK_BIN(entry)), "activate",
			G_CALLBACK(on_find_entry_activate), NULL);
	g_signal_connect(find_dlg.dialog, "response",
			G_CALLBACK(on_find_dialog_response), entry);
	g_signal_connect(find_dlg.dialog, "delete-event",
			G_CALLBACK(gtk_widget_hide_on_delete), NULL);

	sbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(sbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sbox), entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), sbox, TRUE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(vbox),
		add_find_checkboxes(GTK_DIALOG(find_dlg.dialog)));

	/* Now add the multiple match options */
	exp = gtk_expander_new_with_mnemonic(_("_Find All"));
	gtk_expander_set_expanded(GTK_EXPANDER(exp), find_dlg.all_expanded);
	g_signal_connect_after(exp, "activate",
		G_CALLBACK(on_expander_activated), &find_dlg.all_expanded);

	bbox = gtk_hbutton_box_new();

	button = gtk_button_new_with_mnemonic(_("_Mark"));
	ui_widget_set_tooltip_text(button,
			_("Mark all matches in the current document"));
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(send_find_dialog_response),
		GINT_TO_POINTER(GEANY_RESPONSE_MARK));

	button = gtk_button_new_with_mnemonic(_("In Sessi_on"));
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(send_find_dialog_response),
		GINT_TO_POINTER(GEANY_RESPONSE_FIND_IN_SESSION));

	button = gtk_button_new_with_mnemonic(_("_In Document"));
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(send_find_dialog_response),
		GINT_TO_POINTER(GEANY_RESPONSE_FIND_IN_FILE));

	/* close window checkbox */
	check_close = gtk_check_button_new_with_mnemonic(_("Close _dialog"));
	ui_hookup_widget(find_dlg.dialog, check_close, "check_close");
	gtk_button_set_focus_on_click(GTK_BUTTON(check_close), FALSE);
	ui_widget_set_tooltip_text(check_close,
			_("Disable this option to keep the dialog open"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_close), TRUE);
	gtk_container_add(GTK_CONTAINER(bbox), check_close);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(bbox), check_close, TRUE);

	ui_hbutton_box_copy_layout(
		GTK_BUTTON_BOX(GTK_DIALOG(find_dlg.dialog)->action_area),
		GTK_BUTTON_BOX(bbox));
	gtk_container_add(GTK_CONTAINER(exp), bbox);
	gtk_container_add(GTK_CONTAINER(vbox), exp);
}


static void set_dialog_position(GtkWidget *dialog, gint *position)
{
	if (position[0] >= 0)
		gtk_window_move(GTK_WINDOW(dialog), position[0], position[1]);
}


void search_show_find_dialog(void)
{
	GeanyDocument *doc = document_get_current();
	gchar *sel = NULL;

	g_return_if_fail(doc != NULL);

	sel = editor_get_default_selection(doc->editor, search_prefs.use_current_word, NULL);

	if (find_dlg.dialog == NULL)
	{
		create_find_dialog();
		if (sel)
			gtk_entry_set_text(GTK_ENTRY(find_dlg.entry), sel);

		set_dialog_position(find_dlg.dialog, find_dlg.position);
		gtk_widget_show_all(find_dlg.dialog);
	}
	else
	{
		/* only set selection if the dialog is not already visible */
		if (! GTK_WIDGET_VISIBLE(find_dlg.dialog) && sel)
			gtk_entry_set_text(GTK_ENTRY(find_dlg.entry), sel);
		gtk_widget_grab_focus(find_dlg.entry);
		set_dialog_position(find_dlg.dialog, find_dlg.position);
		gtk_widget_show(find_dlg.dialog);
		if (sel != NULL) /* when we have a selection, reset the entry widget's background colour */
			ui_set_search_entry_background(find_dlg.entry, TRUE);
		/* bring the dialog back in the foreground in case it is already open but the focus is away */
		gtk_window_present(GTK_WINDOW(find_dlg.dialog));
	}

	g_free(sel);
}


static void send_replace_dialog_response(GtkButton *button, gpointer user_data)
{
	gtk_dialog_response(GTK_DIALOG(replace_dlg.dialog), GPOINTER_TO_INT(user_data));
}


static gboolean
on_widget_key_pressed_set_focus(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_Tab)
	{
		gtk_widget_grab_focus(GTK_WIDGET(user_data));
		return TRUE;
	}
	return FALSE;
}


static void create_replace_dialog(void)
{
	GtkWidget *label_find, *label_replace, *entry_find, *entry_replace,
		*check_close, *button, *rbox, *fbox, *vbox, *exp, *bbox;
	GtkSizeGroup *label_size;

	load_monospace_style();

	replace_dlg.dialog = gtk_dialog_new_with_buttons(_("Replace"),
		GTK_WINDOW(main_widgets.window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(replace_dlg.dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 9);
	gtk_widget_set_name(replace_dlg.dialog, "GeanyDialogSearch");

	button = gtk_button_new_from_stock(GTK_STOCK_FIND);
	gtk_dialog_add_action_widget(GTK_DIALOG(replace_dlg.dialog), button,
		GEANY_RESPONSE_FIND);
	button = gtk_button_new_with_mnemonic(_("_Replace"));
	gtk_button_set_image(GTK_BUTTON(button),
		gtk_image_new_from_stock(GTK_STOCK_FIND_AND_REPLACE, GTK_ICON_SIZE_BUTTON));
	gtk_dialog_add_action_widget(GTK_DIALOG(replace_dlg.dialog), button,
		GEANY_RESPONSE_REPLACE);
	button = gtk_button_new_with_mnemonic(_("Replace & Fi_nd"));
	gtk_button_set_image(GTK_BUTTON(button),
		gtk_image_new_from_stock(GTK_STOCK_FIND_AND_REPLACE, GTK_ICON_SIZE_BUTTON));
	gtk_dialog_add_action_widget(GTK_DIALOG(replace_dlg.dialog), button,
		GEANY_RESPONSE_REPLACE_AND_FIND);

	label_find = gtk_label_new_with_mnemonic(_("_Search for:"));
	gtk_misc_set_alignment(GTK_MISC(label_find), 0, 0.5);

	label_replace = gtk_label_new_with_mnemonic(_("Replace wit_h:"));
	gtk_misc_set_alignment(GTK_MISC(label_replace), 0, 0.5);

	entry_find = gtk_combo_box_entry_new_text();
	ui_entry_add_clear_icon(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_find), entry_find);
	gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 248);
	gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_find))), 50);
	ui_hookup_widget(replace_dlg.dialog, entry_find, "entry_find");
	replace_dlg.find_entry = GTK_BIN(entry_find)->child;

	entry_replace = gtk_combo_box_entry_new_text();
	ui_entry_add_clear_icon(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_replace), entry_replace);
	gtk_entry_set_max_length(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))), 248);
	gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry_replace))), 50);
	ui_hookup_widget(replace_dlg.dialog, entry_replace, "entry_replace");
	replace_dlg.replace_entry = GTK_BIN(entry_replace)->child;

	/* tab from find to the replace entry */
	g_signal_connect(gtk_bin_get_child(GTK_BIN(entry_find)),
			"key-press-event", G_CALLBACK(on_widget_key_pressed_set_focus),
			gtk_bin_get_child(GTK_BIN(entry_replace)));
	g_signal_connect(gtk_bin_get_child(GTK_BIN(entry_replace)), "activate",
			G_CALLBACK(on_replace_entry_activate), NULL);
	g_signal_connect(replace_dlg.dialog, "response",
			G_CALLBACK(on_replace_dialog_response), entry_replace);
	g_signal_connect(replace_dlg.dialog, "delete-event",
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
	g_object_unref(G_OBJECT(label_size));	/* auto destroy the size group */

	gtk_box_pack_start(GTK_BOX(vbox), fbox, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), rbox, TRUE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(vbox),
		add_find_checkboxes(GTK_DIALOG(replace_dlg.dialog)));

	/* Now add the multiple replace options */
	exp = gtk_expander_new_with_mnemonic(_("Re_place All"));
	gtk_expander_set_expanded(GTK_EXPANDER(exp), replace_dlg.all_expanded);
	g_signal_connect_after(exp, "activate",
		G_CALLBACK(on_expander_activated), &replace_dlg.all_expanded);

	bbox = gtk_hbutton_box_new();

	button = gtk_button_new_with_mnemonic(_("In Sessi_on"));
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(send_replace_dialog_response),
		GINT_TO_POINTER(GEANY_RESPONSE_REPLACE_IN_SESSION));

	button = gtk_button_new_with_mnemonic(_("_In Document"));
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(send_replace_dialog_response),
		GINT_TO_POINTER(GEANY_RESPONSE_REPLACE_IN_FILE));

	button = gtk_button_new_with_mnemonic(_("In Se_lection"));
	ui_widget_set_tooltip_text(button,
		_("Replace all matches found in the currently selected text"));
	gtk_container_add(GTK_CONTAINER(bbox), button);
	g_signal_connect(button, "clicked", G_CALLBACK(send_replace_dialog_response),
		GINT_TO_POINTER(GEANY_RESPONSE_REPLACE_IN_SEL));

	/* close window checkbox */
	check_close = gtk_check_button_new_with_mnemonic(_("Close _dialog"));
	ui_hookup_widget(replace_dlg.dialog, check_close, "check_close");
	gtk_button_set_focus_on_click(GTK_BUTTON(check_close), FALSE);
	ui_widget_set_tooltip_text(check_close,
			_("Disable this option to keep the dialog open"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_close), TRUE);
	gtk_container_add(GTK_CONTAINER(bbox), check_close);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX(bbox), check_close, TRUE);

	ui_hbutton_box_copy_layout(
		GTK_BUTTON_BOX(GTK_DIALOG(replace_dlg.dialog)->action_area),
		GTK_BUTTON_BOX(bbox));
	gtk_container_add(GTK_CONTAINER(exp), bbox);
	gtk_container_add(GTK_CONTAINER(vbox), exp);
}


void search_show_replace_dialog(void)
{
	GeanyDocument *doc = document_get_current();
	gchar *sel = NULL;

	if (doc == NULL)
		return;

	sel = editor_get_default_selection(doc->editor, search_prefs.use_current_word, NULL);

	if (replace_dlg.dialog == NULL)
	{
		create_replace_dialog();
		if (sel)
			gtk_entry_set_text(GTK_ENTRY(replace_dlg.find_entry), sel);

		set_dialog_position(replace_dlg.dialog, replace_dlg.position);
		gtk_widget_show_all(replace_dlg.dialog);
	}
	else
	{
		/* only set selection if the dialog is not already visible */
		if (! GTK_WIDGET_VISIBLE(replace_dlg.dialog) && sel)
			gtk_entry_set_text(GTK_ENTRY(replace_dlg.find_entry), sel);
		if (sel != NULL) /* when we have a selection, reset the entry widget's background colour */
			ui_set_search_entry_background(replace_dlg.find_entry, TRUE);
		gtk_widget_grab_focus(replace_dlg.find_entry);
		set_dialog_position(replace_dlg.dialog, replace_dlg.position);
		gtk_widget_show(replace_dlg.dialog);
		/* bring the dialog back in the foreground in case it is already open but the focus is away */
		gtk_window_present(GTK_WINDOW(replace_dlg.dialog));
	}
	g_free(sel);
}


static void on_widget_toggled_set_sensitive(GtkToggleButton *togglebutton, gpointer user_data)
{
	/* disable extra option entry when checkbutton not checked */
	gtk_widget_set_sensitive(GTK_WIDGET(user_data),
		gtk_toggle_button_get_active(togglebutton));
}


static void create_fif_dialog(void)
{
	GtkWidget *dir_combo, *combo, *fcombo, *e_combo, *entry;
	GtkWidget *label, *label1, *label2, *checkbox1, *checkbox2, *check_wholeword,
		*check_recursive, *check_extra, *entry_extra, *check_regexp, *check;
	GtkWidget *dbox, *sbox, *lbox, *rbox, *hbox, *vbox, *ebox;
	GtkSizeGroup *size_group;
	gchar *encoding_string;
	guint i;

	load_monospace_style();

	fif_dlg.dialog = gtk_dialog_new_with_buttons(
		_("Find in Files"), GTK_WINDOW(main_widgets.window), GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	vbox = ui_dialog_vbox_new(GTK_DIALOG(fif_dlg.dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 9);
	gtk_widget_set_name(fif_dlg.dialog, "GeanyDialogSearch");

	gtk_dialog_add_button(GTK_DIALOG(fif_dlg.dialog), "gtk-find", GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(fif_dlg.dialog),
		GTK_RESPONSE_ACCEPT);

	label = gtk_label_new_with_mnemonic(_("_Search for:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	combo = gtk_combo_box_entry_new_text();
	entry = gtk_bin_get_child(GTK_BIN(combo));
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	gtk_entry_set_max_length(GTK_ENTRY(entry), 248);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 50);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	fif_dlg.search_combo = combo;

	sbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(sbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(sbox), combo, TRUE, TRUE, 0);

	/* make labels same width */
	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget(size_group, label);

	check = gtk_check_button_new_with_mnemonic(_("Fi_les:"));
	ui_hookup_widget(fif_dlg.dialog, check, "check_files");
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
	gtk_size_group_add_widget(size_group, check);

	/* tab from search to the files checkbox */
	g_signal_connect(entry, "key-press-event",
		G_CALLBACK(on_widget_key_pressed_set_focus), check);

	fcombo = gtk_combo_box_entry_new_text();
	gtk_widget_set_sensitive(fcombo, FALSE);
	entry = gtk_bin_get_child(GTK_BIN(fcombo));
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	ui_widget_set_tooltip_text(entry, _("File patterns, e.g. *.c *.h"));
	ui_hookup_widget(fif_dlg.dialog, entry, "entry_files");
	fif_dlg.files_combo = fcombo;

	/* enable entry when check is checked */
	g_signal_connect(check, "toggled",
		G_CALLBACK(on_widget_toggled_set_sensitive), fcombo);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), fcombo, TRUE, TRUE, 0);

	label1 = gtk_label_new_with_mnemonic(_("_Directory:"));
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);

	dir_combo = gtk_combo_box_entry_new_text();
	entry = gtk_bin_get_child(GTK_BIN(dir_combo));
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label1), entry);
	gtk_entry_set_max_length(GTK_ENTRY(entry), 248);
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 50);
	fif_dlg.dir_combo = dir_combo;

	/* tab from files to the dir entry */
	g_signal_connect(gtk_bin_get_child(GTK_BIN(fcombo)), "key-press-event",
		G_CALLBACK(on_widget_key_pressed_set_focus), entry);

	dbox = ui_path_box_new(NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
		GTK_ENTRY(entry));
	gtk_box_pack_start(GTK_BOX(dbox), label1, FALSE, FALSE, 0);

	label2 = gtk_label_new_with_mnemonic(_("E_ncoding:"));
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);

	e_combo = gtk_combo_box_new_text();
	for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
	{
		encoding_string = encodings_to_string(&encodings[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(e_combo), encoding_string);
		g_free(encoding_string);
	}
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(e_combo), 3);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label2), e_combo);
	fif_dlg.encoding_combo = e_combo;

	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), e_combo, TRUE, TRUE, 0);

	gtk_size_group_add_widget(size_group, label1);
	gtk_size_group_add_widget(size_group, label2);
	g_object_unref(G_OBJECT(size_group));	/* auto destroy the size group */

	gtk_box_pack_start(GTK_BOX(vbox), sbox, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), dbox, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);

	check_regexp = gtk_check_button_new_with_mnemonic(_("_Use regular expressions"));
	ui_hookup_widget(fif_dlg.dialog, check_regexp, "check_regexp");
	gtk_button_set_focus_on_click(GTK_BUTTON(check_regexp), FALSE);
	ui_widget_set_tooltip_text(check_regexp, _("See grep's manual page for more information"));

	check_recursive = gtk_check_button_new_with_mnemonic(_("_Recurse in subfolders"));
	ui_hookup_widget(fif_dlg.dialog, check_recursive, "check_recursive");
	gtk_button_set_focus_on_click(GTK_BUTTON(check_recursive), FALSE);

	checkbox1 = gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
	ui_hookup_widget(fif_dlg.dialog, checkbox1, "check_case");
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox1), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox1), TRUE);

	check_wholeword = gtk_check_button_new_with_mnemonic(_("Match only a _whole word"));
	ui_hookup_widget(fif_dlg.dialog, check_wholeword, "check_wholeword");
	gtk_button_set_focus_on_click(GTK_BUTTON(check_wholeword), FALSE);

	checkbox2 = gtk_check_button_new_with_mnemonic(_("_Invert search results"));
	ui_hookup_widget(fif_dlg.dialog, checkbox2, "check_invert");
	gtk_button_set_focus_on_click(GTK_BUTTON(checkbox2), FALSE);
	ui_widget_set_tooltip_text(checkbox2,
			_("Invert the sense of matching, to select non-matching lines"));

	lbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(lbox), check_regexp);
	gtk_container_add(GTK_CONTAINER(lbox), checkbox2);
	gtk_container_add(GTK_CONTAINER(lbox), check_recursive);

	rbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(rbox), checkbox1);
	gtk_container_add(GTK_CONTAINER(rbox), check_wholeword);
	gtk_container_add(GTK_CONTAINER(rbox), gtk_label_new(NULL));

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(hbox), lbox);
	gtk_container_add(GTK_CONTAINER(hbox), rbox);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	check_extra = gtk_check_button_new_with_mnemonic(_("E_xtra options:"));
	ui_hookup_widget(fif_dlg.dialog, check_extra, "check_extra");
	gtk_button_set_focus_on_click(GTK_BUTTON(check_extra), FALSE);

	entry_extra = gtk_entry_new();
	ui_entry_add_clear_icon(GTK_ENTRY(entry_extra));
	gtk_widget_set_sensitive(entry_extra, FALSE);
	ui_widget_set_tooltip_text(entry_extra, _("Other options to pass to Grep"));
	ui_hookup_widget(fif_dlg.dialog, entry_extra, "entry_extra");

	/* enable entry_extra when check_extra is checked */
	g_signal_connect(check_extra, "toggled",
		G_CALLBACK(on_widget_toggled_set_sensitive), entry_extra);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox), check_extra, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), entry_extra, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(vbox), hbox);

	g_signal_connect(fif_dlg.dialog, "response",
			G_CALLBACK(on_find_in_files_dialog_response), NULL);
	g_signal_connect(fif_dlg.dialog, "delete-event",
			G_CALLBACK(gtk_widget_hide_on_delete), NULL);
}


/* dir is the directory to search in (UTF-8 encoding), maybe NULL to determine it the usual way
 * by using the current file's path */
void search_show_find_in_files_dialog(const gchar *dir)
{
	GtkWidget *entry; /* for child GtkEntry of a GtkComboBoxEntry */
	GeanyDocument *doc = document_get_current();
	gchar *sel = NULL;
	gchar *cur_dir = NULL;
	GeanyEncodingIndex enc_idx = GEANY_ENCODING_UTF_8;

	if (fif_dlg.dialog == NULL)
	{
		create_fif_dialog();
		gtk_widget_show_all(fif_dlg.dialog);
		if (doc)
			sel = editor_get_default_selection(doc->editor, search_prefs.use_current_word, NULL);
	}
	stash_group_display(fif_prefs, fif_dlg.dialog);

	/* only set selection if the dialog is not already visible, or has just been created */
	if (doc && ! sel && ! GTK_WIDGET_VISIBLE(fif_dlg.dialog))
		sel = editor_get_default_selection(doc->editor, search_prefs.use_current_word, NULL);

	entry = GTK_BIN(fif_dlg.search_combo)->child;
	if (sel)
		gtk_entry_set_text(GTK_ENTRY(entry), sel);
	g_free(sel);

	/* add project's base path directory to the dir list, we do this here once
	 * (in create_fif_dialog() it would fail if a project is opened after dialog creation) */
	if (app->project != NULL && NZV(app->project->base_path))
	{
		ui_combo_box_prepend_text_once(GTK_COMBO_BOX(fif_dlg.dir_combo),
			app->project->base_path);
	}

	entry = GTK_BIN(fif_dlg.dir_combo)->child;
	if (NZV(dir))
		cur_dir = g_strdup(dir);	/* custom directory argument passed */
	else
	{
		gboolean entry_empty = ! NZV(gtk_entry_get_text(GTK_ENTRY(entry)));

		if (search_prefs.use_current_file_dir || entry_empty)
		{
			cur_dir = utils_get_current_file_dir_utf8();

			/* use default_open_path if no directory could be determined
			 * (e.g. when no files are open) */
			if (!cur_dir)
				cur_dir = g_strdup(utils_get_default_dir_utf8());
			if (!cur_dir)
				cur_dir = g_get_current_dir();
		}
	}
	if (cur_dir)
	{
		gtk_entry_set_text(GTK_ENTRY(entry), cur_dir);
		g_free(cur_dir);
	}

	/* set the encoding of the current file */
	if (doc != NULL)
		enc_idx = encodings_get_idx_from_charset(doc->encoding);
	gtk_combo_box_set_active(GTK_COMBO_BOX(fif_dlg.encoding_combo), enc_idx);

	/* put the focus to the directory entry if it is empty */
	if (utils_str_equal(gtk_entry_get_text(GTK_ENTRY(entry)), ""))
		gtk_widget_grab_focus(fif_dlg.dir_combo);
	else
		gtk_widget_grab_focus(fif_dlg.search_combo);

	/* set dialog window position */
	set_dialog_position(fif_dlg.dialog, fif_dlg.position);

	gtk_widget_show(fif_dlg.dialog);
	/* bring the dialog back in the foreground in case it is already open but the focus is away */
	gtk_window_present(GTK_WINDOW(fif_dlg.dialog));
}


static void
on_find_replace_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET(user_data);
	GtkToggleButton *chk_regexp = GTK_TOGGLE_BUTTON(
		ui_lookup_widget(dialog, "check_regexp"));

	if (togglebutton == chk_regexp)
	{
		gboolean regex_set = gtk_toggle_button_get_active(chk_regexp);
		GtkWidget *check_word = ui_lookup_widget(dialog, "check_word");
		GtkWidget *check_wordstart = ui_lookup_widget(dialog, "check_wordstart");
		GtkToggleButton *check_case = GTK_TOGGLE_BUTTON(
			ui_lookup_widget(dialog, "check_case"));
		GtkWidget *check_escape = ui_lookup_widget(dialog, "check_escape");
		static gboolean case_state = FALSE; /* state before regex enabled */

		/* hide options that don't apply to regex searches */
		gtk_widget_set_sensitive(check_escape, ! regex_set);

		if (dialog == find_dlg.dialog)
			gtk_widget_set_sensitive(ui_lookup_widget(dialog, "btn_previous"), ! regex_set);
		else
			gtk_widget_set_sensitive(ui_lookup_widget(dialog, "check_back"), ! regex_set);

		gtk_widget_set_sensitive(check_word, ! regex_set);
		gtk_widget_set_sensitive(check_wordstart, ! regex_set);

		if (regex_set)	/* regex enabled */
		{
			/* Enable case sensitive but remember original case toggle state */
			case_state = gtk_toggle_button_get_active(check_case);
			gtk_toggle_button_set_active(check_case, TRUE);
		}
		else	/* regex disabled */
		{
			/* If case sensitive is still enabled, revert to what it was before we enabled it */
			if (gtk_toggle_button_get_active(check_case) == TRUE)
				gtk_toggle_button_set_active(check_case, case_state);
		}
	}
}


/* Clears markers if text is null/empty.
 * @return Number of matches marked. */
gint search_mark_all(GeanyDocument *doc, const gchar *search_text, gint flags)
{
	gint pos, count = 0;
	gsize len;
	struct Sci_TextToFind ttf;

	g_return_val_if_fail(doc != NULL, 0);

	/* clear previous search indicators */
	editor_indicator_clear(doc->editor, GEANY_INDICATOR_SEARCH);

	if (!NZV(search_text))
		return 0;

	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(doc->editor->sci);
	ttf.lpstrText = (gchar *)search_text;
	while (TRUE)
	{
		pos = sci_find_text(doc->editor->sci, flags, &ttf);
		if (pos == -1) break;

		len = ttf.chrgText.cpMax - ttf.chrgText.cpMin;
		if (len)
			editor_indicator_set_on_range(doc->editor, GEANY_INDICATOR_SEARCH, pos, pos + len);

		ttf.chrg.cpMin = ttf.chrgText.cpMax + 1;
		count++;
	}
	return count;
}


static void
on_find_entry_activate(GtkEntry *entry, gpointer user_data)
{
	on_find_dialog_response(NULL, GEANY_RESPONSE_FIND,
				ui_lookup_widget(GTK_WIDGET(entry), "entry"));
}


static gint get_search_flags(GtkWidget *dialog)
{
	gboolean fl1, fl2, fl3, fl4;

	fl1 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				ui_lookup_widget(dialog, "check_case")));
	fl2 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				ui_lookup_widget(dialog, "check_word")));
	fl3 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				ui_lookup_widget(dialog, "check_regexp")));
	fl4 = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				ui_lookup_widget(dialog, "check_wordstart")));

	return (fl1 ? SCFIND_MATCHCASE : 0) |
		(fl2 ? SCFIND_WHOLEWORD : 0) |
		(fl3 ? SCFIND_REGEXP | SCFIND_POSIX : 0) |
		(fl4 ? SCFIND_WORDSTART : 0);
}


static void
on_find_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	gtk_window_get_position(GTK_WINDOW(find_dlg.dialog),
		&find_dlg.position[0], &find_dlg.position[1]);

	if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
		gtk_widget_hide(find_dlg.dialog);
	else
	{
		GeanyDocument *doc = document_get_current();
		gboolean search_replace_escape;
		gboolean check_close = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						ui_lookup_widget(GTK_WIDGET(find_dlg.dialog), "check_close")));

		if (doc == NULL)
			return;

		search_replace_escape = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
						ui_lookup_widget(GTK_WIDGET(find_dlg.dialog), "check_escape")));
		search_data.backwards = FALSE;
		search_data.search_bar = FALSE;

		g_free(search_data.text);
		search_data.text = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(user_data)))));
		search_data.flags = get_search_flags(find_dlg.dialog);

		if (strlen(search_data.text) == 0)
		{
			fail:
			utils_beep();
			gtk_widget_grab_focus(find_dlg.entry);
			return;
		}
		if (search_replace_escape || search_data.flags & SCFIND_REGEXP)
		{
			if (! utils_str_replace_escape(search_data.text, search_data.flags & SCFIND_REGEXP))
				goto fail;
		}
		ui_combo_box_add_to_history(GTK_COMBO_BOX_ENTRY(user_data), search_data.text, 0);

		switch (response)
		{
			case GEANY_RESPONSE_FIND:
			case GEANY_RESPONSE_FIND_PREVIOUS:
			{
				gint result = document_find_text(doc, search_data.text, search_data.flags,
					(response == GEANY_RESPONSE_FIND_PREVIOUS), TRUE, GTK_WIDGET(find_dlg.dialog));
				ui_set_search_entry_background(find_dlg.entry, (result > -1));
				check_close = FALSE;
				if (search_prefs.suppress_dialogs)
					check_close = TRUE;
				break;
			}
			case GEANY_RESPONSE_FIND_IN_FILE:
				search_find_usage(search_data.text, search_data.flags, FALSE);
				break;

			case GEANY_RESPONSE_FIND_IN_SESSION:
				search_find_usage(search_data.text, search_data.flags, TRUE);
				break;

			case GEANY_RESPONSE_MARK:
			{
				gint count = search_mark_all(doc, search_data.text, search_data.flags);

				if (count == 0)
					ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), search_data.text);
				else
					ui_set_statusbar(FALSE,
						ngettext("Found %d match for \"%s\".",
								 "Found %d matches for \"%s\".", count),
						count, search_data.text);
			}
			break;
		}
		if (check_close)
			gtk_widget_hide(find_dlg.dialog);
	}
}


static void
on_replace_entry_activate(GtkEntry *entry, gpointer user_data)
{
	on_replace_dialog_response(NULL, GEANY_RESPONSE_REPLACE, NULL);
}


static void replace_in_session(GeanyDocument *doc,
		gint search_flags_re, gboolean search_replace_escape_re,
		const gchar *find, const gchar *replace)
{
	guint n, page_count, rep_count = 0, file_count = 0;

	/* replace in all documents following notebook tab order */
	page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(main_widgets.notebook));
	for (n = 0; n < page_count; n++)
	{
		GeanyDocument *tmp_doc = document_get_from_page(n);
		gint reps = 0;

		reps = document_replace_all(tmp_doc, find, replace, search_flags_re,
			search_replace_escape_re);
		rep_count += reps;
		if (reps)
			file_count++;
	}
	if (file_count == 0)
	{
		utils_beep();
		ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), find);
		return;
	}
	/* if only one file was changed, don't override that document's status message
	 * so we don't have to translate 4 messages for ngettext */
	if (file_count > 1)
		ui_set_statusbar(FALSE, _("Replaced %u matches in %u documents."),
			rep_count, file_count);

	/* show which docs had replacements: */
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_STATUS);

	ui_save_buttons_toggle(doc->changed);	/* update save all */
}


static void
on_replace_dialog_response(GtkDialog *dialog, gint response, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	gint search_flags_re;
	gboolean search_backwards_re, search_replace_escape_re;
	gboolean close_window;
	gchar *find, *replace;

	gtk_window_get_position(GTK_WINDOW(replace_dlg.dialog),
		&replace_dlg.position[0], &replace_dlg.position[1]);

	if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
	{
		gtk_widget_hide(replace_dlg.dialog);
		return;
	}

	close_window = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				ui_lookup_widget(GTK_WIDGET(replace_dlg.dialog), "check_close")));
	search_backwards_re = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				ui_lookup_widget(GTK_WIDGET(replace_dlg.dialog), "check_back")));
	search_replace_escape_re = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
				ui_lookup_widget(GTK_WIDGET(replace_dlg.dialog), "check_escape")));
	find = g_strdup(gtk_entry_get_text(GTK_ENTRY(replace_dlg.find_entry)));
	replace = g_strdup(gtk_entry_get_text(GTK_ENTRY(replace_dlg.replace_entry)));

	search_flags_re = get_search_flags(replace_dlg.dialog);

	if ((response != GEANY_RESPONSE_FIND) && (search_flags_re & SCFIND_MATCHCASE)
		&& (strcmp(find, replace) == 0))
	{
		fail:
		utils_beep();
		gtk_widget_grab_focus(replace_dlg.find_entry);
		return;
	}
	if (search_flags_re & SCFIND_REGEXP)
	{
		if (! utils_str_replace_escape(find, TRUE) ||
			! utils_str_replace_escape(replace, TRUE))
			goto fail;
	}
	else if (search_replace_escape_re)
	{
		if (! utils_str_replace_escape(find, FALSE) ||
			! utils_str_replace_escape(replace, FALSE))
			goto fail;
	}

	ui_combo_box_add_to_history(GTK_COMBO_BOX_ENTRY(
		gtk_widget_get_parent(replace_dlg.find_entry)), find, 0);
	ui_combo_box_add_to_history(GTK_COMBO_BOX_ENTRY(
		gtk_widget_get_parent(replace_dlg.replace_entry)), replace, 0);

	switch (response)
	{
		case GEANY_RESPONSE_REPLACE_AND_FIND:
		{
			gint rep = document_replace_text(doc, find, replace, search_flags_re,
				search_backwards_re);
			if (rep != -1)
				document_find_text(doc, find, search_flags_re, search_backwards_re,
					TRUE, NULL);
			break;
		}
		case GEANY_RESPONSE_REPLACE:
			document_replace_text(doc, find, replace, search_flags_re,
				search_backwards_re);
			break;

		case GEANY_RESPONSE_FIND:
		{
			gint result = document_find_text(doc, find, search_flags_re,
								search_backwards_re, TRUE, GTK_WIDGET(dialog));
			ui_set_search_entry_background(replace_dlg.find_entry, (result > -1));
			break;
		}
		case GEANY_RESPONSE_REPLACE_IN_FILE:
			if (! document_replace_all(doc, find, replace, search_flags_re,
				search_replace_escape_re))
				utils_beep();
			break;

		case GEANY_RESPONSE_REPLACE_IN_SESSION:
			replace_in_session(doc, search_flags_re, search_replace_escape_re, find, replace);
			break;

		case GEANY_RESPONSE_REPLACE_IN_SEL:
			document_replace_sel(doc, find, replace, search_flags_re, search_replace_escape_re);
			break;
	}
	switch (response)
	{
		case GEANY_RESPONSE_REPLACE_IN_SEL:
		case GEANY_RESPONSE_REPLACE_IN_FILE:
		case GEANY_RESPONSE_REPLACE_IN_SESSION:
			if (close_window)
				gtk_widget_hide(replace_dlg.dialog);
	}
	g_free(find);
	g_free(replace);
}


static GString *get_grep_options(void)
{
	GString *gstr = g_string_new("-nHI");	/* line numbers, filenames, ignore binaries */

	if (settings.fif_invert_results)
		g_string_append_c(gstr, 'v');
	if (! settings.fif_case_sensitive)
		g_string_append_c(gstr, 'i');
	if (settings.fif_match_whole_word)
		g_string_append_c(gstr, 'w');
	if (settings.fif_recursive)
		g_string_append_c(gstr, 'r');

	if (!settings.fif_regexp)
		g_string_append_c(gstr, 'F');
	else
		g_string_append_c(gstr, 'E');

	if (settings.fif_use_extra_options)
	{
		g_strstrip(settings.fif_extra_options);

		if (*settings.fif_extra_options != 0)
		{
			g_string_append_c(gstr, ' ');
			g_string_append(gstr, settings.fif_extra_options);
		}
	}
	g_strstrip(settings.fif_files);
	if (settings.fif_use_files && *settings.fif_files)
	{
		GString *tmp;

		/* put --include= before each pattern */
		tmp = g_string_new(settings.fif_files);
		do {} while (utils_string_replace_all(tmp, "  ", " "));
		g_string_prepend_c(tmp, ' ');
		utils_string_replace_all(tmp, " ", " --include=");
		g_string_append(gstr, tmp->str);
		g_string_free(tmp, TRUE);
	}
	return gstr;
}


static void
on_find_in_files_dialog_response(GtkDialog *dialog, gint response,
		G_GNUC_UNUSED gpointer user_data)
{
	gtk_window_get_position(GTK_WINDOW(fif_dlg.dialog), &fif_dlg.position[0], &fif_dlg.position[1]);

	stash_group_update(fif_prefs, fif_dlg.dialog);

	if (response == GTK_RESPONSE_ACCEPT)
	{
		GtkWidget *search_combo = fif_dlg.search_combo;
		const gchar *search_text =
			gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(search_combo))));
		GtkWidget *dir_combo = fif_dlg.dir_combo;
		const gchar *utf8_dir =
			gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(dir_combo))));
		GeanyEncodingIndex enc_idx = gtk_combo_box_get_active(
			GTK_COMBO_BOX(fif_dlg.encoding_combo));

		if (!NZV(utf8_dir))
			ui_set_statusbar(FALSE, _("Invalid directory for find in files."));
		else if (NZV(search_text))
		{
			gchar *locale_dir;
			GString *opts = get_grep_options();
			const gchar *enc = (enc_idx == GEANY_ENCODING_UTF_8) ? NULL :
				encodings_get_charset_from_index(enc_idx);

			locale_dir = utils_get_locale_from_utf8(utf8_dir);

			if (search_find_in_files(search_text, locale_dir, opts->str, enc))
			{
				ui_combo_box_add_to_history(GTK_COMBO_BOX_ENTRY(search_combo), search_text, 0);
				ui_combo_box_add_to_history(GTK_COMBO_BOX_ENTRY(fif_dlg.files_combo), NULL, 0);
				ui_combo_box_add_to_history(GTK_COMBO_BOX_ENTRY(dir_combo), utf8_dir, 0);
				gtk_widget_hide(fif_dlg.dialog);
			}
			g_free(locale_dir);
			g_string_free(opts, TRUE);
		}
		else
			ui_set_statusbar(FALSE, _("No text to find."));
	}
	else
		gtk_widget_hide(fif_dlg.dialog);
}


static gboolean
search_find_in_files(const gchar *utf8_search_text, const gchar *dir, const gchar *opts,
	const gchar *enc)
{
	gchar **argv_prefix, **argv, **opts_argv;
	gchar *command_grep;
	gchar *search_text = NULL;
	guint opts_argv_len, i;
	GPid child_pid;
	gint stdout_fd;
	gint stderr_fd;
	GError *error = NULL;
	gboolean ret = FALSE;
	gssize utf8_text_len;

	if (! NZV(utf8_search_text) || ! dir) return TRUE;

	command_grep = g_find_program_in_path(tool_prefs.grep_cmd);
	if (command_grep == NULL)
	{
		ui_set_statusbar(TRUE, _("Cannot execute grep tool '%s';"
			" check the path setting in Preferences."), tool_prefs.grep_cmd);
		return FALSE;
	}

	/* convert the search text in the preferred encoding (if the text is not valid UTF-8. assume
	 * it is already in the preferred encoding) */
	utf8_text_len = strlen(utf8_search_text);
	if (enc != NULL && g_utf8_validate(utf8_search_text, utf8_text_len, NULL))
	{
		search_text = g_convert(utf8_search_text, utf8_text_len, enc, "UTF-8", NULL, NULL, NULL);
	}
	if (search_text == NULL)
		search_text = g_strdup(utf8_search_text);

	opts_argv = g_strsplit(opts, " ", -1);
	opts_argv_len = g_strv_length(opts_argv);

	/* set grep command and options */
	argv_prefix = g_new0(gchar*, 1 + opts_argv_len + 3 + 1);	/* last +1 for recursive arg */

	argv_prefix[0] = command_grep;
	for (i = 0; i < opts_argv_len; i++)
	{
		argv_prefix[i + 1] = g_strdup(opts_argv[i]);
	}
	g_strfreev(opts_argv);

	i++;	/* correct for tool_prefs.grep_cmd */
	argv_prefix[i++] = g_strdup("--");
	argv_prefix[i++] = search_text;

	/* finally add the arguments(files to be searched) */
	if (strstr(argv_prefix[1], "r"))	/* recursive option set */
	{
		/* Use '.' so we get relative paths in the output */
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

	if (argv == NULL)	/* no files */
	{
		return FALSE;
	}

	gtk_list_store_clear(msgwindow.store_msg);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_MESSAGE);

	if (! g_spawn_async_with_pipes(dir, (gchar**)argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
		NULL, NULL, &child_pid,
		NULL, &stdout_fd, &stderr_fd, &error))
	{
		geany_debug("%s: g_spawn_async_with_pipes() failed: %s", G_STRFUNC, error->message);
		ui_set_statusbar(TRUE, _("Process failed (%s)"), error->message);
		g_error_free(error);
		ret = FALSE;
	}
	else
	{
		gchar *str, *utf8_str;

		ui_progress_bar_start(_("Searching..."));

		g_free(msgwindow.find_in_files_dir);
		msgwindow.find_in_files_dir = g_strdup(dir);
		/* we can pass 'enc' without strdup'ing it here because it's a global const string and
		 * always exits longer than the lifetime of this function */
		utils_set_up_io_channel(stdout_fd, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			TRUE, search_read_io, (gpointer) enc);
		utils_set_up_io_channel(stderr_fd, G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
			TRUE, search_read_io_stderr, (gpointer) enc);
		g_child_watch_add(child_pid, search_close_pid, NULL);

		str = g_strdup_printf(_("%s %s -- %s (in directory: %s)"),
			tool_prefs.grep_cmd, opts, utf8_search_text, dir);
		utf8_str = utils_get_utf8_from_locale(str);
		msgwin_msg_add_string(COLOR_BLUE, -1, NULL, utf8_str);
		utils_free_pointers(2, str, utf8_str, NULL);
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


static gboolean read_fif_io(GIOChannel *source, GIOCondition condition, gchar *enc, gint msg_color)
{
	if (condition & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg, *utf8_msg;

		while (g_io_channel_read_line(source, &msg, NULL, NULL, NULL) && msg)
		{
			utf8_msg = NULL;

			g_strstrip(msg);
			/* enc is NULL when encoding is set to UTF-8, so we can skip any conversion */
			if (enc != NULL)
			{
				if (! g_utf8_validate(msg, -1, NULL))
				{
					utf8_msg = g_convert(msg, -1, "UTF-8", enc, NULL, NULL, NULL);
				}
				if (utf8_msg == NULL)
					utf8_msg = msg;
			}
			else
				utf8_msg = msg;

			msgwin_msg_add_string(msg_color, -1, NULL, utf8_msg);

			if (utf8_msg != msg)
				g_free(utf8_msg);
			g_free(msg);
		}
	}
	if (condition & (G_IO_ERR | G_IO_HUP | G_IO_NVAL))
		return FALSE;

	return TRUE;
}


static gboolean search_read_io(GIOChannel *source, GIOCondition condition, gpointer data)
{
	return read_fif_io(source, condition, data, COLOR_BLACK);
}


static gboolean search_read_io_stderr(GIOChannel *source, GIOCondition condition, gpointer data)
{
	return read_fif_io(source, condition, data, COLOR_DARK_RED);
}


static void search_close_pid(GPid child_pid, gint status, gpointer user_data)
{
	/* TODO: port this also to Windows API */
#ifdef G_OS_UNIX
	const gchar *msg = _("Search failed.");
	gint exit_status = 1;

	if (WIFEXITED(status))
	{
		exit_status = WEXITSTATUS(status);
	}
	else if (WIFSIGNALED(status))
	{
		exit_status = -1;
		g_warning("Find in Files: The command failed unexpectedly (signal received).");
	}

	switch (exit_status)
	{
		case 0:
		{
			gint count = gtk_tree_model_iter_n_children(
				GTK_TREE_MODEL(msgwindow.store_msg), NULL) - 1;
			gchar *text = ngettext(
						"Search completed with %d match.",
						"Search completed with %d matches.", count);

			msgwin_msg_add(COLOR_BLUE, -1, NULL, text, count);
			ui_set_statusbar(FALSE, text, count);
			break;
		}
		case 1:
			msg = _("No matches found.");
		default:
			msgwin_msg_add_string(COLOR_BLUE, -1, NULL, msg);
			ui_set_statusbar(FALSE, "%s", msg);
			break;
	}
#endif

	utils_beep();
	g_spawn_close_pid(child_pid);
	ui_progress_bar_stop();
}


static gboolean compile_regex(regex_t *regex, const gchar *str, gint sflags)
{
	gint err;
	gint rflags = REG_EXTENDED | REG_NEWLINE;

	if (~sflags & SCFIND_MATCHCASE)
		rflags |= REG_ICASE;
	if (sflags & (SCFIND_WHOLEWORD | SCFIND_WORDSTART))
	{
		geany_debug("%s: Unsupported regex flags found!", G_STRFUNC);
	}

	err = regcomp(regex, str, rflags);
	if (err != 0)
	{
		gchar buf[256];

		regerror(err, regex, buf, sizeof buf);
		ui_set_statusbar(FALSE, _("Bad regex: %s"), buf);
		return FALSE;
	}
	return TRUE;
}


/* groups that don't exist are handled OK as len = end - start = (-1) - (-1) = 0 */
static gchar *get_regex_match_string(const gchar *text, regmatch_t *pmatch, gint match_idx)
{
	return g_strndup(&text[pmatch[match_idx].rm_so],
		pmatch[match_idx].rm_eo - pmatch[match_idx].rm_so);
}


static regmatch_t regex_matches[10];
/* All matching text from regex_matches[0].rm_so to regex_matches[0].rm_eo */
static gchar *regex_match_text = NULL;

static gint find_regex(ScintillaObject *sci, guint pos, regex_t *regex)
{
	const gchar *text;
	gint flags = 0;

	g_return_val_if_fail(pos <= (guint)sci_get_length(sci), FALSE);

	if (sci_get_col_from_position(sci, pos) != 0)
		flags = REG_NOTBOL;
	/* Warning: any SCI calls will invalidate 'text' after calling SCI_GETCHARACTERPOINTER */
	text = (void*)scintilla_send_message(sci, SCI_GETCHARACTERPOINTER, 0, 0);
	text += pos;

	if (regexec(regex, text, G_N_ELEMENTS(regex_matches), regex_matches, flags) == 0)
	{
		setptr(regex_match_text, get_regex_match_string(text, regex_matches, 0));
		return regex_matches[0].rm_so + pos;
	}
	setptr(regex_match_text, NULL);
	return -1;
}


gint search_find_next(ScintillaObject *sci, const gchar *str, gint flags)
{
	regex_t regex;
	gint ret = -1;
	gint pos;

	if (~flags & SCFIND_REGEXP)
		return sci_search_next(sci, flags, str);

	if (!compile_regex(&regex, str, flags))
		return -1;

	pos = sci_get_current_position(sci);
	ret = find_regex(sci, pos, &regex);
	if (ret >= 0)
		sci_set_selection(sci, ret, regex_matches[0].rm_eo + pos);

	regfree(&regex);
	return ret;
}


gint search_replace_target(ScintillaObject *sci, const gchar *replace_text,
	gboolean regex)
{
	GString *str;
	gint ret = 0;
	gint i = 0;

	if (!regex)
		return sci_replace_target(sci, replace_text, FALSE);

	str = g_string_new(replace_text);
	while (str->str[i])
	{
		gchar *ptr = &str->str[i];
		gchar *grp;
		gchar c;

		if (ptr[0] != '\\')
		{
			i++;
			continue;
		}
		c = ptr[1];
		/* backslash or unnecessary escape */
		if (c == '\\' || !isdigit(c))
		{
			g_string_erase(str, i, 1);
			i++;
			continue;
		}
		/* digit escape */
		g_string_erase(str, i, 2);
		/* fix match offsets by subtracting index of whole match start from the string */
		grp = get_regex_match_string(regex_match_text - regex_matches[0].rm_so,
			regex_matches, c - '0');
		g_string_insert(str, i, grp);
		i += strlen(grp);
		g_free(grp);
	}
	ret = sci_replace_target(sci, str->str, FALSE);
	g_string_free(str, TRUE);
	return ret;
}


gint search_find_text(ScintillaObject *sci, gint flags, struct Sci_TextToFind *ttf)
{
	regex_t regex;
	gint pos;
	gint ret;

	if (~flags & SCFIND_REGEXP)
		return sci_find_text(sci, flags, ttf);

	if (!compile_regex(&regex, ttf->lpstrText, flags))
		return -1;

	pos = ttf->chrg.cpMin;
	ret = find_regex(sci, pos, &regex);
	regfree(&regex);

	if (ret >= 0 && ret < ttf->chrg.cpMax)
	{
		ttf->chrgText.cpMin = regex_matches[0].rm_so + pos;
		ttf->chrgText.cpMax = regex_matches[0].rm_eo + pos;
		return ret;
	}
	return -1;
}


static gint find_document_usage(GeanyDocument *doc, const gchar *search_text, gint flags)
{
	gchar *buffer, *short_file_name;
	struct Sci_TextToFind ttf;
	gint count = 0;
	gint prev_line = -1;

	g_return_val_if_fail(doc != NULL, 0);

	short_file_name = g_path_get_basename(DOC_FILENAME(doc));

	ttf.chrg.cpMin = 0;
	ttf.chrg.cpMax = sci_get_length(doc->editor->sci);
	ttf.lpstrText = (gchar *)search_text;
	while (1)
	{
		gint pos, line, start, find_len;

		pos = search_find_text(doc->editor->sci, flags, &ttf);
		if (pos == -1)
			break;	/* no more matches */
		find_len = ttf.chrgText.cpMax - ttf.chrgText.cpMin;
		if (find_len == 0)
			break;	/* Ignore regex ^ or $ */

		count++;
		line = sci_get_line_from_position(doc->editor->sci, pos);
		if (line != prev_line)
		{
			buffer = sci_get_line(doc->editor->sci, line);
			msgwin_msg_add(COLOR_BLACK, line + 1, doc,
				"%s:%d: %s", short_file_name, line + 1, g_strstrip(buffer));
			g_free(buffer);
			prev_line = line;
		}

		start = ttf.chrgText.cpMax + 1;
		ttf.chrg.cpMin = start;
	}
	g_free(short_file_name);
	return count;
}


void search_find_usage(const gchar *search_text, gint flags, gboolean in_session)
{
	GeanyDocument *doc;
	gint count = 0;

	doc = document_get_current();
	g_return_if_fail(doc != NULL);

	if (!NZV(search_text))
	{
		utils_beep();
		return;
	}

	gtk_notebook_set_current_page(GTK_NOTEBOOK(msgwindow.notebook), MSG_MESSAGE);
	gtk_list_store_clear(msgwindow.store_msg);

	if (! in_session)
	{	/* use current document */
		count = find_document_usage(doc, search_text, flags);
	}
	else
	{
		guint i;
		for (i = 0; i < documents_array->len; i++)
		{
			if (documents[i]->is_valid)
			{
				count += find_document_usage(documents[i], search_text, flags);
			}
		}
	}

	if (count == 0) /* no matches were found */
	{
		ui_set_statusbar(FALSE, _("No matches found for \"%s\"."), search_text);
		msgwin_msg_add(COLOR_BLUE, -1, NULL, _("No matches found for \"%s\"."), search_text);
	}
	else
	{
		ui_set_statusbar(FALSE, ngettext(
			"Found %d match for \"%s\".", "Found %d matches for \"%s\".", count),
			count, search_text);
		msgwin_msg_add(COLOR_BLUE, -1, NULL, ngettext(
			"Found %d match for \"%s\".", "Found %d matches for \"%s\".", count),
			count, search_text);
	}
}


/* ttf is updated to include the last match position (ttf->chrg.cpMin) and
 * the new search range end (ttf->chrg.cpMax).
 * Note: Normally you would call sci_start/end_undo_action() around this call. */
/* Warning: Scintilla recommends caching replacements to do all at once to avoid
 * performance issues with SCI_GETCHARACTERPOINTER. */
guint search_replace_range(ScintillaObject *sci, struct Sci_TextToFind *ttf,
		gint flags, const gchar *replace_text)
{
	gint count = 0;
	const gchar *find_text = ttf->lpstrText;
	gint start = ttf->chrg.cpMin;
	gint end = ttf->chrg.cpMax;

	g_return_val_if_fail(sci != NULL && find_text != NULL && replace_text != NULL, 0);
	if (! *find_text)
		return 0;

	while (TRUE)
	{
		gint search_pos;
		gint find_len = 0, replace_len = 0;

		search_pos = search_find_text(sci, flags, ttf);
		find_len = ttf->chrgText.cpMax - ttf->chrgText.cpMin;
		if (search_pos == -1)
			break;	/* no more matches */
		if (find_len == 0 && ! NZV(replace_text))
			break;	/* nothing to do */

		if (search_pos + find_len > end)
			break;	/* found text is partly out of range */
		else
		{
			gint movepastEOL = 0;

			sci_set_target_start(sci, search_pos);
			sci_set_target_end(sci, search_pos + find_len);

			if (find_len <= 0)
			{
				gchar chNext = sci_get_char_at(sci, sci_get_target_end(sci));

				if (chNext == '\r' || chNext == '\n')
					movepastEOL = 1;
			}
			replace_len = search_replace_target(sci, replace_text,
				flags & SCFIND_REGEXP);
			count++;
			if (search_pos == end)
				break;	/* Prevent hang when replacing regex $ */

			/* make the next search start after the replaced text */
			start = search_pos + replace_len + movepastEOL;
			if (find_len == 0)
				start = sci_get_position_after(sci, start);	/* prevent '[ ]*' regex rematching part of replaced text */
			ttf->chrg.cpMin = start;
			end += replace_len - find_len;	/* update end of range now text has changed */
			ttf->chrg.cpMax = end;
		}
	}
	return count;
}


