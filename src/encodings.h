/*
 *      encodings.h - this file is part of Geany, a fast and lightweight IDE
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


/*
 * Modified by the gedit Team, 2002. See the gedit AUTHORS file for a
 * list of people on the gedit Team.
 * See the gedit ChangeLog files for a list of changes.
 */

 /* Stolen from anjuta */

#ifndef GEANY_ENCODINGS_H
#define GEANY_ENCODINGS_H

#include <glib.h>

typedef struct _GeanyEncoding GeanyEncoding;

const GeanyEncoding* encoding_get_from_charset(const gchar *charset);
const GeanyEncoding* encoding_get_from_index(gint index);

gchar* encoding_to_string(const GeanyEncoding* enc);
const gchar* encoding_get_charset(const GeanyEncoding* enc);

GList* encoding_get_encodings(GList *encoding_strings);

void encodings_init(void);

#endif
