/*
 *      about.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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

#include "about.h"
#include "geany.h"
#include "utils.h"
#include "ui_utils.h"
#include "support.h"

static GtkWidget *gb_window = NULL;
#include "gb.c"


#define HEADER "<span size=\"larger\" weight=\"bold\">Geany " VERSION "%s</span>"
#define INFO "<span size=\"larger\" weight=\"bold\">%s</span>"
#define CODENAME "<span weight=\"bold\">\"" GEANY_CODENAME "\"</span>"
#define BUILDDATE "<span size=\"smaller\">%s</span>"
#define COPYRIGHT "Copyright (c)  2005-2006 \n Enrico Tröger \nAll rights reserved."
#define CREDITS \
"<span size=\"larger\" weight=\"bold\">%s</span>\n\t\
Enrico Tröger - %s\n\t\
&lt;enrico.troeger@uvena.de&gt;\n\n\t\
Nick Treleaven - %s\n\t\
&lt;nick.treleaven@btinternet.com&gt;\n\n\t\
Frank Lanitz - %s\n\t\
&lt;frank@frank.uvena.de&gt;\n\n\t\
\n<span size=\"larger\" weight=\"bold\">%s</span>\n\t\
\
Yura Semashko - <language> be_BY\n\t\
&lt;yurand2@gmail.com&gt;\n\n\t\
Toni Garcia-Navarro - <language> ca_ES\n\t\
&lt;topi@elpiset.net&gt;\n\n\t\
Petr Messner - <language> cs_CZ \n\t\
&lt;messa@messa.cz&gt;\n\n\t\
Frank Lanitz - <language> de_DE\n\t\
&lt;frank@frank.uvena.de&gt;\n\n\t\
Damián Viano - <language> es\n\t\
&lt;debian@damianv.com.ar&gt;\n\n\t\
Kurt De Bree - <language> nl\n\t\
&lt;kdebree@telenet.be&gt;\n\n\t\
Jacek Wolszczak - <language> pl_PL\n\t\
&lt;shutdownrunner@o2.pl&gt;\n\n\t\
Alexandre Moreira - <language> pt_BR\n\t\
&lt;alexandream@gmail.com&gt;\n\n\t\
brahmann_ - <language> ru_RU\n\t\
&lt;brahmann@mthr.net.ru&gt;\n\n\t\
Clytie Siddall - <language> vi_VN\n\t\
&lt;clytie@riverland.net.au&gt;"

static void header_eventbox_style_set(GtkWidget *widget);
static void header_label_style_set(GtkWidget *widget);
static void homepage_clicked(GtkButton *button, gpointer data);


static GtkWidget *create_dialog(void)
{
	GtkWidget *dialog;
	GtkWidget *header_image;
	GtkWidget *header_label;
	GtkWidget *label_info;
	GtkWidget *codename_label;
	GtkWidget *builddate_label;
	GtkWidget *url_button;
	GtkWidget *cop_label;
	GtkWidget *label;
	GtkWidget *credits_label;
	GtkWidget *license_textview;
	GtkWidget *notebook;
	GtkWidget *box;
	GtkWidget *credits_scrollwin;
	GtkWidget *license_scrollwin;
	GtkWidget *info_box;
	GtkWidget *button;
	GtkWidget *header_hbox;
	GtkWidget *header_eventbox;
	GdkPixbuf *icon;
	GtkTextBuffer* tb;
	gchar *license_text = NULL;
	gchar buffer[512];
	gchar buffer2[128];
	gchar *credits;

	dialog = gtk_dialog_new();

	//configure dialog
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(app->window));
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_title(GTK_WINDOW(dialog), _("About Geany"));
	button = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	g_signal_connect(G_OBJECT(dialog), "key-press-event", G_CALLBACK(gb_on_key_pressed), NULL);

	// create header
	header_eventbox = gtk_event_box_new();
	gtk_widget_show(header_eventbox);
	header_hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(header_hbox), 4);
	gtk_widget_show(header_hbox);
	gtk_container_add(GTK_CONTAINER(header_eventbox), header_hbox);
	header_image = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(header_hbox), header_image, FALSE,FALSE,0);
	header_label = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(header_label), TRUE);
	// print the subversion revision if it is available
	g_snprintf(buffer, sizeof(buffer), HEADER, (utils_strcmp(REVISION, "-1")) ? "" : " (" REVISION ")");
	gtk_label_set_markup(GTK_LABEL(header_label), buffer);
	gtk_widget_show(header_label);
	gtk_box_pack_start(GTK_BOX(header_hbox), header_label, FALSE,FALSE,0);
	header_eventbox_style_set(header_eventbox);
	header_label_style_set(header_label);
	g_signal_connect_after(G_OBJECT(header_eventbox), "style_set", G_CALLBACK(header_eventbox_style_set), NULL);
	g_signal_connect_after(G_OBJECT(header_label), "style_set", G_CALLBACK(header_label_style_set), NULL);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), header_eventbox, FALSE, FALSE, 0);

	// set image
	icon = ui_new_pixbuf_from_inline(GEANY_IMAGE_LOGO, FALSE);
	gtk_image_set_from_pixbuf(GTK_IMAGE(header_image), icon);
	gtk_window_set_icon(GTK_WINDOW(dialog), icon);

	// create notebook
	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 2);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, TRUE, TRUE, 0);

	// create "Info" tab
	info_box = gtk_vbox_new(FALSE, 0);
	//gtk_container_set_border_width(GTK_CONTAINER(info_box), 6);
	gtk_widget_show(info_box);

	label_info = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(label_info), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(label_info), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(label_info), TRUE);
	g_snprintf(buffer, sizeof(buffer), INFO, _("A fast and lightweight IDE"));
	gtk_label_set_markup(GTK_LABEL(label_info), buffer);
	gtk_misc_set_padding(GTK_MISC(label_info), 2, 11);
	gtk_widget_show(label_info);
	gtk_box_pack_start(GTK_BOX(info_box), label_info, FALSE, FALSE, 0);

	// Codename label
	codename_label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(codename_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(codename_label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(codename_label), TRUE);
	gtk_label_set_markup(GTK_LABEL(codename_label), CODENAME);
	gtk_misc_set_padding(GTK_MISC(codename_label), 2, 8);
	gtk_widget_show(codename_label);
	gtk_box_pack_start(GTK_BOX(info_box), codename_label, FALSE, FALSE, 0);

	// build date label
	builddate_label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(builddate_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(builddate_label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(builddate_label), TRUE);
	g_snprintf(buffer2, sizeof(buffer2), _("(built on %s)"), __DATE__);
	g_snprintf(buffer, sizeof(buffer), BUILDDATE, buffer2);
	gtk_label_set_markup(GTK_LABEL(builddate_label), buffer);
	gtk_misc_set_padding(GTK_MISC(builddate_label), 2, 2);
	gtk_widget_show(builddate_label);
	gtk_box_pack_start(GTK_BOX(info_box), builddate_label, FALSE, FALSE, 0);

	box = gtk_hbutton_box_new();
	url_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(url_button), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(url_button), "clicked", G_CALLBACK(homepage_clicked), GEANY_HOMEPAGE);
	label = gtk_label_new(NULL);
	gtk_label_set_text(GTK_LABEL(label), GEANY_HOMEPAGE);
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(url_button), label);
	gtk_widget_show(url_button);
	gtk_box_pack_start(GTK_BOX(box), url_button, FALSE, FALSE, 0);
	gtk_widget_show(box);
	gtk_box_pack_start(GTK_BOX(info_box), box, FALSE, FALSE, 10);

	// copyright label
	cop_label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(cop_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(cop_label), FALSE);
	gtk_label_set_use_markup(GTK_LABEL(cop_label), TRUE);
	gtk_label_set_markup(GTK_LABEL(cop_label), COPYRIGHT);
	gtk_misc_set_padding(GTK_MISC(cop_label), 2, 10);
	gtk_widget_show(cop_label);
	gtk_box_pack_start(GTK_BOX(info_box), cop_label, FALSE, FALSE, 0);
	//gtk_container_add(GTK_CONTAINER(info_box), cop_label);

	label = gtk_label_new(_("Info"));
	gtk_widget_show(label);
	gtk_widget_show_all(info_box);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), info_box, label);

	// create "Credits" tab
	credits_scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(credits_scrollwin), 6);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(credits_scrollwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	credits_label = gtk_label_new(NULL);
	gtk_label_set_selectable(GTK_LABEL(credits_label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(credits_label), TRUE);
	credits = g_strdup_printf(CREDITS, _("Developers"), _("Maintainer"), _("developer"),
							 _("translation maintainer"), _("Translators"));
	credits = utils_str_replace(credits, "<language>", _("language"));
	gtk_label_set_markup(GTK_LABEL(credits_label), credits);
	gtk_misc_set_alignment(GTK_MISC(credits_label), 0.0, 0.0);
	gtk_widget_show(credits_label);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(credits_scrollwin), credits_label);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(gtk_widget_get_parent(credits_label)), GTK_SHADOW_NONE);
	label = gtk_label_new(_("Credits"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), credits_scrollwin, label);

	// create "License" tab
	license_scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(license_scrollwin), 6);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(license_scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	license_textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(license_textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(license_textview), FALSE);
	gtk_widget_show(license_textview);
	gtk_container_add(GTK_CONTAINER(license_scrollwin), license_textview);
	label = gtk_label_new(_("License"));
	gtk_widget_show(label);

	g_snprintf(buffer, sizeof(buffer), "%s" G_DIR_SEPARATOR_S "GPL-2", app->datadir);

	g_file_get_contents(buffer, &license_text, NULL, NULL);
	if (license_text == NULL)
	{
		license_text = g_strdup("License text could not be found, please google for GPLv2");
	}
	tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(license_textview));
	gtk_text_buffer_set_text(tb, license_text, strlen(license_text));

	g_free(credits);
	g_free(license_text);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), license_scrollwin, label);

	gtk_widget_show_all(dialog);
	return dialog;
}


void about_dialog_show(void)
{
	GtkWidget *dialog;

	dialog = create_dialog();

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


static void header_eventbox_style_set(GtkWidget *widget)
{
	static gint recursive = 0;
	GtkStyle *style;

	if (recursive > 0)
		return;

	++recursive;
	style = gtk_widget_get_style(widget);
	gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &style->bg[GTK_STATE_SELECTED]);
	--recursive;
}


static void header_label_style_set(GtkWidget *widget)
{
	static gint recursive = 0;
	GtkStyle *style;

	if (recursive > 0)
		return;

	++recursive;
	style = gtk_widget_get_style(widget);
	gtk_widget_modify_fg(widget, GTK_STATE_NORMAL, &style->fg[GTK_STATE_SELECTED]);
	--recursive;
}


static void homepage_clicked(GtkButton *button, gpointer data)
{
	utils_start_browser(data);
}

