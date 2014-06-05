/*
 *      geanypage.h - this file is part of Geany, a fast and lightweight IDE
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GEANY_PAGE_H
#define GEANY_PAGE_H 1

#include "gtkcompat.h"

G_BEGIN_DECLS

#define GEANY_TYPE_PAGE (geany_page_get_type ())
#define GEANY_PAGE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GEANY_TYPE_PAGE, GeanyPage))
#define GEANY_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GEANY_TYPE_PAGE, GeanyPageClass))
#define GEANY_IS_PAGE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GEANY_TYPE_PAGE))
#define GEANY_IS_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GEANY_TYPE_PAGE))
#define GEANY_PAGE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GEANY_TYPE_PAGE, GeanyPageClass))

/* For now, no private data because the type as a whole is not part of the plugin API */

typedef struct _GeanyPage      GeanyPage;
typedef struct _GeanyPageClass GeanyPageClass;

struct _GeanyPage
{
	GtkBox                parent_instance;

	gchar                 *label;
	GIcon                 *icon;
	GtkWidget             *tab_widget;
};

struct _GeanyPageClass
{
	GtkBoxClass           parent_class;
};

GType geany_page_get_type(void);

GtkWidget *geany_page_new_with_label(const gchar *label);
void geany_page_set_active(GeanyPage *self, gboolean is_active);

static inline
const gchar *geany_page_get_label(GeanyPage *self)
{
	return self->label;
}


static inline
GIcon *geany_page_get_icon(GeanyPage *self)
{
	return self->icon;
}

static inline
GtkWidget *geany_page_get_tab_widget(GeanyPage *self)
{
	return self->tab_widget;
}

gboolean geany_page_try_close(GeanyPage *self);

G_END_DECLS

#endif /* GEANY_PAGE_H */
