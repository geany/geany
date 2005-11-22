/*
 *      about.c - this file is part of Geany, a fast and lightweight IDE
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


#include "geany.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>

#include "about.h"
#include "support.h"
#include "utils.h"


typedef struct _AboutPerson AboutPerson;
struct _AboutPerson
{
	gchar *name;
	gchar *mail;
	gchar *task;
};

struct _AboutInfo
{
	gchar *program;
	gchar *version;
	gchar *description;
	gchar *copyright;
	GList *credits;
	gchar *license;
	gchar *homepage;
	gchar *codename;
};


AboutPerson *about_person_new(const gchar *name, const gchar *mail, const gchar *task)
{
	AboutPerson *person;

	g_return_val_if_fail(name != NULL, NULL);

	person = g_new0(AboutPerson, 1);
	person->name = g_strdup(name);
	if (mail != NULL) person->mail = g_strdup(mail);
	if (task != NULL) person->task = g_strdup(task);

	return(person);
}

void about_person_free(AboutPerson *person)
{
	g_return_if_fail(person != NULL);

	if (person->name != NULL) g_free(person->name);
	if (person->mail != NULL) g_free(person->mail);
	if (person->task != NULL) g_free(person->task);

	g_free(person);
}


AboutInfo *about_info_new(const gchar *program, const gchar *version,
				const gchar *description, const gchar *copyright, const gchar *homepage, const gchar *codename)
{
	AboutInfo *info;

	// validate parameters
	g_return_val_if_fail(program != NULL, NULL);
	g_return_val_if_fail(version != NULL, NULL);
	g_return_val_if_fail(description != NULL, NULL);

	// create info object
	if ((info = g_new0(AboutInfo, 1)) == NULL) return(NULL);

	// install settings
	info->program = g_strdup(program);
	info->version = g_strdup(version);
	info->description = g_strdup(description);
	info->copyright = g_strdup(copyright);
	g_file_get_contents(GEANY_DATA_DIR "/GPL-2", &(info->license), NULL, NULL);
	if (info->license == NULL)
	{
		info->license = g_strdup("License text could not be found, please google for GPLv2");
	}
	info->homepage = g_strdup(homepage);
	info->codename = g_strdup(codename);
	return(info);
}


void about_info_free(AboutInfo *info)
{
	// validate parameters
	g_return_if_fail (info != NULL);

	/* free strings */
	if (info->program != NULL) g_free (info->program);
	if (info->version != NULL) g_free (info->version);
	if (info->description != NULL) g_free (info->description);
	if (info->copyright != NULL) g_free (info->copyright);
	if (info->license != NULL) g_free (info->license);
	if (info->homepage != NULL) g_free (info->homepage);
	if (info->codename != NULL) g_free (info->codename);

	// free credits
	g_list_foreach(info->credits, (GFunc) about_person_free, NULL);
	g_list_free(info->credits);

	// free info itself
	g_free (info);
}


void about_info_set_homepage(AboutInfo *info, const gchar *homepage)
{
	// validate parameters
	g_return_if_fail (info != NULL);
	g_return_if_fail (homepage != NULL);

	// set new homepage
	if (info->homepage != NULL)
		g_free (info->homepage);
	info->homepage = g_strdup (homepage);
}


void about_info_add_credit(AboutInfo *info, const gchar *name, const gchar *mail, const gchar *task)
{
	// validate parameters
	g_return_if_fail(info != NULL);
	g_return_if_fail(name != NULL);

	// add person to credits list
	info->credits = g_list_append(info->credits, about_person_new(name, mail, task));
}


typedef struct _AboutDialogPrivate AboutDialogPrivate;
struct _AboutDialogPrivate
{
	// homepage url if any
	gchar *homepage;

	// tooltips help
	GtkTooltips *tooltips;

	// header widgets
	GtkWidget *header_image;
	GtkWidget *header_label;

	// widgets in the "Info" tab
	GtkWidget *info_des_label;
	GtkWidget *info_codename_label;
	GtkWidget *info_builddate_label;
	GtkWidget *info_url_button;
	GtkWidget *info_cop_label;

	// widgets in the "Credits" tab
	GtkWidget *credits_scrollwin;
	GtkWidget *credits_label;

	// widgets in the "License" tab
	GtkWidget *license_scrollwin;
	GtkWidget *license_textview;
};


static void about_dialog_class_init(AboutDialogClass *klass);
static void about_dialog_init(AboutDialog *dialog);
static void about_dialog_finalize(GObject *object);

static GObjectClass *parent_class;


GType about_dialog_get_type(void)
{
	static GType about_dialog_type = 0;

	if (!about_dialog_type) {
		static const GTypeInfo about_dialog_info =
		{
			sizeof(AboutDialogClass), NULL, NULL, (GClassInitFunc)about_dialog_class_init,
			NULL, NULL, sizeof(AboutDialog), 0, (GInstanceInitFunc)about_dialog_init
		};

		about_dialog_type = g_type_register_static(GTK_TYPE_DIALOG, "AboutDialog", &about_dialog_info, 0);
	}

	return(about_dialog_type);
}


static void about_dialog_class_init(AboutDialogClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	// be sure to initialize the libraries i18n support first
	//_xfce_i18n_init();

	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = about_dialog_finalize;
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


static void homepage_clicked(GtkButton *button, AboutDialogPrivate *priv)
{
	utils_start_browser(priv->homepage);
}


static void about_dialog_init(AboutDialog *dialog)
{
	AboutDialogPrivate* priv;
	GtkWidget *header_eventbox;
	GtkWidget *header_hbox;
	GtkWidget *info_box;
	GtkWidget *notebook;
	GtkWidget *label;
	GtkWidget *align;
	GtkWidget *tmp;
	GtkWidget *box;
	GtkWidget *button;

	// allocate private data storage
	priv = g_new0(AboutDialogPrivate, 1);
	dialog->priv = priv;

	//configure dialog
	button = gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	// allocate tooltips
	priv->tooltips = gtk_tooltips_new();
	g_object_ref(G_OBJECT(priv->tooltips));
	gtk_object_sink(GTK_OBJECT(priv->tooltips));

	// create header
	header_eventbox = gtk_event_box_new();
	gtk_widget_show(header_eventbox);
	header_hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(header_hbox), 4);
	gtk_widget_show(header_hbox);
	gtk_container_add(GTK_CONTAINER(header_eventbox), header_hbox);
	priv->header_image = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(header_hbox), priv->header_image, FALSE,FALSE,0);
	priv->header_label = gtk_label_new(NULL);
	gtk_label_set_use_markup(GTK_LABEL(priv->header_label), TRUE);
	gtk_widget_show(priv->header_label);
	gtk_box_pack_start(GTK_BOX(header_hbox), priv->header_label, FALSE,FALSE,0);
	header_eventbox_style_set(header_eventbox);
	header_label_style_set(priv->header_label);
	g_signal_connect_after(G_OBJECT(header_eventbox), "style_set", G_CALLBACK(header_eventbox_style_set), NULL);
	g_signal_connect_after(G_OBJECT(priv->header_label), "style_set", G_CALLBACK(header_label_style_set), NULL);
	gtk_box_pack_start(GTK_BOX(dialog->parent.vbox), header_eventbox, FALSE, FALSE, 0);

	// create notebook
	box = gtk_event_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(box), 2);
	gtk_widget_show(box);
	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_container_add(GTK_CONTAINER(box), notebook);
	gtk_box_pack_start(GTK_BOX(dialog->parent.vbox), box, TRUE, TRUE, 0);

	// create "Info" tab
	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_widget_show(align);
	info_box = gtk_vbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(info_box), 6);
	gtk_widget_show(info_box);
	priv->info_des_label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(priv->info_des_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(priv->info_des_label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(priv->info_des_label), TRUE);
	gtk_misc_set_padding(GTK_MISC(priv->info_des_label), 2, 10);
	gtk_widget_show(priv->info_des_label);
	gtk_box_pack_start(GTK_BOX(info_box), priv->info_des_label, FALSE, FALSE, 0);

	// Codename label
	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_widget_show(align);
	priv->info_codename_label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(priv->info_codename_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(priv->info_codename_label), FALSE);
	gtk_label_set_use_markup(GTK_LABEL(priv->info_codename_label), TRUE);
	gtk_misc_set_padding(GTK_MISC(priv->info_codename_label), 2, 8);
	gtk_widget_show(priv->info_codename_label);
	gtk_box_pack_start(GTK_BOX(info_box), priv->info_codename_label, FALSE, FALSE, 0);

	// build date label
	priv->info_builddate_label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(priv->info_builddate_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(priv->info_builddate_label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(priv->info_builddate_label), TRUE);
	gtk_misc_set_padding(GTK_MISC(priv->info_builddate_label), 2, 0);
	gtk_widget_show(priv->info_builddate_label);
	gtk_box_pack_start(GTK_BOX(info_box), priv->info_builddate_label, FALSE, FALSE, 0);

	// make some space between build date and homepage button
	tmp = gtk_label_new("");
	gtk_widget_show(tmp);
	gtk_box_pack_start(GTK_BOX(info_box), tmp, FALSE, FALSE, 0);

	box = gtk_hbutton_box_new();
	priv->info_url_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(priv->info_url_button), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(priv->info_url_button), "clicked", G_CALLBACK(homepage_clicked), priv);
	gtk_box_pack_start(GTK_BOX(box), priv->info_url_button, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(priv->tooltips, priv->info_url_button, _("Visit homepage"), NULL);
	gtk_widget_show(box);
	gtk_box_pack_start(GTK_BOX(info_box), box, FALSE, FALSE, 0);

	priv->info_cop_label = gtk_label_new(NULL);
	gtk_label_set_justify(GTK_LABEL(priv->info_cop_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable(GTK_LABEL(priv->info_cop_label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(priv->info_cop_label), TRUE);
	gtk_misc_set_padding(GTK_MISC(priv->info_cop_label), 2, 10);
	gtk_widget_show(priv->info_cop_label);
	gtk_box_pack_start(GTK_BOX(info_box), priv->info_cop_label, FALSE, FALSE, 0);

	label = gtk_label_new(_("Info"));
	gtk_widget_show(label);
	gtk_container_add(GTK_CONTAINER(align), info_box);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), align, label);

	// create "Credits" tab
	priv->credits_scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(priv->credits_scrollwin), 6);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->credits_scrollwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	priv->credits_label = gtk_label_new(NULL);
	gtk_label_set_selectable(GTK_LABEL(priv->credits_label), TRUE);
	gtk_label_set_use_markup(GTK_LABEL(priv->credits_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(priv->credits_label), 0.0, 0.0);
	gtk_widget_show(priv->credits_label);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(priv->credits_scrollwin), priv->credits_label);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(gtk_widget_get_parent(priv->credits_label)), GTK_SHADOW_NONE);
	label = gtk_label_new(_("Credits"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), priv->credits_scrollwin, label);

	// create "License" tab
	priv->license_scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(priv->license_scrollwin), 6);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->license_scrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	priv->license_textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(priv->license_textview), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(priv->license_textview), FALSE);
	gtk_widget_show(priv->license_textview);
	gtk_container_add(GTK_CONTAINER(priv->license_scrollwin), priv->license_textview);
	label = gtk_label_new(_("License"));
	gtk_widget_show(label);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), priv->license_scrollwin, label);
}


static void about_dialog_finalize(GObject *object)
{
	AboutDialogPrivate *priv;
	AboutDialog *dialog;

	g_return_if_fail(IS_ABOUT_DIALOG(object));

	dialog = ABOUT_DIALOG(object);
	priv = dialog->priv;

	// free tooltips
	g_object_unref(G_OBJECT(priv->tooltips));

	if (priv->homepage != NULL) g_free(priv->homepage);
	g_free(priv);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}


GtkWidget *about_dialog_new(GtkWindow *parent, const AboutInfo *info, GdkPixbuf *icon)
{
	AboutDialogPrivate *priv;
	AboutDialog *dialog;
	GtkTextBuffer* tb;
	GtkWidget *label;
	gchar buffer[1024];
	gchar *text, *p;
	GList *credit;

	// validate parameters
	g_return_val_if_fail(info != NULL, NULL);

	// allocate dialog
	dialog = ABOUT_DIALOG(g_object_new(TYPE_ABOUT_DIALOG, NULL));
	priv = dialog->priv;

	// make transient for parent
	if (parent != NULL)
	{
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	}
	else
	{
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);
	}

	// put the icon inplace if any
	if (icon != NULL)
	{
		gtk_window_set_icon(GTK_WINDOW(dialog), icon);
		gtk_image_set_from_pixbuf(GTK_IMAGE(priv->header_image), icon);
		gtk_widget_show(priv->header_image);
	}

	// set window title
	g_snprintf(buffer, sizeof(buffer), _("About %s..."), info->program);
	gtk_window_set_title(GTK_WINDOW(dialog), buffer);

	// set header label
	g_snprintf(buffer, sizeof(buffer), "<span size=\"larger\" weight=\"bold\">"
		"%s %s</span>", info->program, info->version);
	gtk_label_set_markup(GTK_LABEL(priv->header_label), buffer);

	// set "Info" description text
	g_snprintf(buffer, sizeof(buffer), "<span size=\"larger\" weight=\"bold\">%s</span>", info->description);
	gtk_label_set_markup(GTK_LABEL(priv->info_des_label), buffer);

	// set "Codename" description text
	g_snprintf(buffer, sizeof(buffer), "<span weight=\"bold\">\"%s\"</span>", info->codename);
	gtk_label_set_markup(GTK_LABEL(priv->info_codename_label), buffer);

	// set build date
	g_snprintf(buffer, sizeof(buffer), "<span size=\"smaller\">(built on %s)</span>", __DATE__);
	gtk_label_set_markup(GTK_LABEL(priv->info_builddate_label), buffer);

	// set "Info" homepage if given
	if (info->homepage != NULL)
	{
		priv->homepage = g_strdup(info->homepage);
		g_snprintf(buffer, sizeof(buffer), "<tt>%s</tt>", priv->homepage);
		label = gtk_label_new(NULL);
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_label_set_markup(GTK_LABEL(label), buffer);
		gtk_widget_show(label);
		gtk_container_add(GTK_CONTAINER(priv->info_url_button), label);
		gtk_widget_show(priv->info_url_button);
	}

	// set "Info" copyright text
	gtk_label_set_text(GTK_LABEL(priv->info_cop_label), info->copyright);

	// display "Credits"
	if (info->credits != NULL)
	{
		text = g_strdup("");
		for (credit = info->credits; credit != NULL; credit = credit->next)
		{
			const AboutPerson *person = (const AboutPerson *)credit->data;

			g_snprintf(buffer, sizeof(buffer), "<big><i>%s</i></big>", person->name);

			if (person->mail != NULL)
			{
				g_strlcat(buffer, "\n\t", sizeof(buffer));
				g_strlcat(buffer, person->mail, sizeof(buffer));
			}

			if (person->task != NULL)
			{
				g_strlcat(buffer, "\n\t", sizeof(buffer));
				g_strlcat(buffer, person->task, sizeof(buffer));
			}

			p = g_strconcat(text, buffer, "\n\n", NULL);
			g_free(text);
			text = p;
		}
		gtk_label_set_markup(GTK_LABEL(priv->credits_label), text);
		gtk_widget_show(priv->credits_scrollwin);
		g_free(text);
	}

	// display "License"
	if (info->license != NULL)
	{
		tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->license_textview));
		gtk_text_buffer_set_text(tb, info->license, strlen(info->license));
		gtk_widget_show(priv->license_scrollwin);
	}

	return(GTK_WIDGET(dialog));
}

