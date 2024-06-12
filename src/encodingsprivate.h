/*
 *      encodingsprivate.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 The Geany contributors
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

#ifndef ENCODINGSPRIVATE_H
#define ENCODINGSPRIVATE_H

#include "encodings.h"

/* Groups of encodings */
typedef enum
{
	NONE = 0,
	WESTEUROPEAN,
	EASTEUROPEAN,
	EASTASIAN,
	ASIAN,
	MIDDLEEASTERN,
	UNICODE,

	GEANY_ENCODING_GROUPS_MAX
}
GeanyEncodingGroup;

/* Structure to represent an encoding to be used in Geany. */
typedef struct GeanyEncoding
{
	GeanyEncodingIndex      idx; /* The index of the encoding inside globa encodes array.*/
	gint                    order; /* Internally used member for grouping */
	GeanyEncodingGroup      group; /* Internally used member for grouping */
	const gchar            *charset; /* String representation of the encoding, e.g. "ISO-8859-3" */
	const gchar            *name; /* Translatable and descriptive name of the encoding, e.g. "South European" */
	gboolean                supported; /* Whether this encoding is supported on the system */
}
GeanyEncoding;

const GeanyEncoding* encodings_get_from_charset(const gchar *charset);
const GeanyEncoding* encodings_get_from_index(gint idx);

gchar* encodings_to_string(const GeanyEncoding* enc);
const gchar* encodings_get_charset(const GeanyEncoding* enc);

void encodings_select_radio_item(const gchar *charset);

void encodings_init_headless(void);
void encodings_init(void);
void encodings_finalize(void);

GtkTreeStore *encodings_encoding_store_new(gboolean has_detect);

gint encodings_encoding_store_get_encoding(GtkTreeStore *store, GtkTreeIter *iter);

gboolean encodings_encoding_store_get_iter(GtkTreeStore *store, GtkTreeIter *iter, gint enc);

void encodings_encoding_store_cell_data_func(GtkCellLayout *cell_layout, GtkCellRenderer *cell,
                                             GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data);

gboolean encodings_is_unicode_charset(const gchar *string);

gboolean encodings_convert_to_utf8_auto(gchar **buf, gsize *size, const gchar *forced_enc,
                                        gchar **used_encoding, gboolean *has_bom, gboolean *has_nuls,
                                        GError **error);

GeanyEncodingIndex encodings_scan_unicode_bom(const gchar *string, gsize len, guint *bom_len);

GeanyEncodingIndex encodings_get_idx_from_charset(const gchar *charset);

extern GeanyEncoding encodings[GEANY_ENCODINGS_MAX];

#endif /* ENCODINGSPRIVATE_H */
