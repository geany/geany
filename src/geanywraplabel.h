/*
 *      geanywraplabel.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2009-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2009-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 */

#ifndef GEANY_WRAP_LABEL_H
#define GEANY_WRAP_LABEL_H

G_BEGIN_DECLS


#define GEANY_WRAP_LABEL_TYPE				(geany_wrap_label_get_type())
#define GEANY_WRAP_LABEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), \
	GEANY_WRAP_LABEL_TYPE, GeanyWrapLabel))
#define GEANY_WRAP_LABEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), \
	GEANY_WRAP_LABEL_TYPE, GeanyWrapLabelClass))
#define IS_GEANY_WRAP_LABEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
	GEANY_WRAP_LABEL_TYPE))
#define IS_GEANY_WRAP_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
	GEANY_WRAP_LABEL_TYPE))


typedef struct _GeanyWrapLabel       GeanyWrapLabel;
typedef struct _GeanyWrapLabelClass  GeanyWrapLabelClass;

GType			geany_wrap_label_get_type			(void);
GtkWidget*		geany_wrap_label_new				(const gchar *text);


G_END_DECLS

#endif /* GEANY_WRAP_LABEL_H */
