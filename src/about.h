/*
 *      about.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_ABOUT_DIALOG_H
#define GEANY_ABOUT_DIALOG_H

//#include <gtk/gtkdialog.h>

#define ABOUT_COPYRIGHT_TEXT(years, owner) ("Copyright (c) " years "\n" owner "\nAll rights reserved.")

typedef struct _AboutInfo AboutInfo;

AboutInfo* about_info_new(const gchar *program, const gchar *version, const gchar *description,
						  const gchar *copyright, const gchar *homepage, const gchar *codename);

void about_info_free(AboutInfo *info);

void about_info_add_credit(AboutInfo *info, const gchar *name, const gchar *mail, const gchar *task);

#define TYPE_ABOUT_DIALOG about_dialog_get_type()
#define ABOUT_DIALOG(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, TYPE_ABOUT_DIALOG, AboutDialog)
//#define XFCE_ABOUT_DIALOG_CLASS(klass) G_TYPE_CHECK_CLASS_CAST(klass, XFCE_TYPE_ABOUT_DIALOG, XfceAboutDialogClass)
#define IS_ABOUT_DIALOG(obj) G_TYPE_CHECK_INSTANCE_TYPE(obj, TYPE_ABOUT_DIALOG)

typedef struct _AboutDialogClass AboutDialogClass;

typedef struct _AboutDialog AboutDialog;

struct _AboutDialogClass
{
	GtkDialogClass parent_class;
};

struct _AboutDialog
{
	GtkDialog parent;
	struct _AboutDialogPrivate *priv;
};

GType about_dialog_get_type(void);

GtkWidget* about_dialog_new(GtkWindow *parent, const AboutInfo *info, GdkPixbuf *icon);


#endif
