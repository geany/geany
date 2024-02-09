/*
 *      geanyentryactionprivate.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2008 The Geany contributors
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


#ifndef GEANY_ENTRY_ACTION_PRIVATE_H
#define GEANY_ENTRY_ACTION_PRIVATE_H 1

G_BEGIN_DECLS

#define GEANY_ENTRY_ACTION_GET_PRIVATE(obj)	(GEANY_ENTRY_ACTION(obj)->priv)

struct _GeanyEntryActionPrivate
{
	GtkWidget	*entry;
	gboolean	 numeric;
	gboolean	 connected;
	gchar		*preedit;
	gboolean	 preedit_phase;
	gchar		*search_keyword;
};

G_END_DECLS

#endif /* GEANY_ENTRY_ACTION_PRIVATE_H */
