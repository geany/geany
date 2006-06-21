/*
 *      encodings.c - this file is part of Geany, a fast and lightweight IDE
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
 */


/*
 * Modified by the gedit Team, 2002. See the gedit AUTHORS file for a
 * list of people on the gedit Team.
 * See the gedit ChangeLog files for a list of changes.
 */
 /* Stolen from anjuta */

#include <string.h>

#include "geany.h"
#include "utils.h"
#include "support.h"
#include "msgwindow.h"
#include "encodings.h"
#include "callbacks.h"



#define fill(x, y, z) encodings[x].idx = x;encodings[x].charset = y;encodings[x].name = z;
static void init_encodings(void)
{
	fill(GEANY_ENCODING_ISO_8859_1, "ISO-8859-1", "Western");
	fill(GEANY_ENCODING_ISO_8859_2, "ISO-8859-2", "Central European");
	fill(GEANY_ENCODING_ISO_8859_3, "ISO-8859-3", "South European");
	fill(GEANY_ENCODING_ISO_8859_4, "ISO-8859-4", "Baltic");
	fill(GEANY_ENCODING_ISO_8859_5, "ISO-8859-5", "Cyrillic");
	fill(GEANY_ENCODING_ISO_8859_6, "ISO-8859-6", "Arabic");
	fill(GEANY_ENCODING_ISO_8859_7, "ISO-8859-7", "Greek");
	fill(GEANY_ENCODING_ISO_8859_8, "ISO-8859-8", "Hebrew Visual");
	fill(GEANY_ENCODING_ISO_8859_8_I, "ISO-8859-8-I", "Hebrew");
	fill(GEANY_ENCODING_ISO_8859_9, "ISO-8859-9", "Turkish");
	fill(GEANY_ENCODING_ISO_8859_10, "ISO-8859-10", "Nordic");
	fill(GEANY_ENCODING_ISO_8859_13, "ISO-8859-13", "Baltic");
	fill(GEANY_ENCODING_ISO_8859_14, "ISO-8859-14", "Celtic");
	fill(GEANY_ENCODING_ISO_8859_15, "ISO-8859-15", "Western");
	fill(GEANY_ENCODING_ISO_8859_16, "ISO-8859-16", "Romanian");

	fill(GEANY_ENCODING_UTF_8, "UTF-8", "Unicode");
	fill(GEANY_ENCODING_UTF_16, "UTF-16", "Unicode");
	fill(GEANY_ENCODING_UCS_2, "UCS-2", "Unicode");
	fill(GEANY_ENCODING_UCS_4, "UCS-4", "Unicode");
	
	fill(GEANY_ENCODING_ARMSCII_8, "ARMSCII-8", "Armenian");
	fill(GEANY_ENCODING_BIG5, "BIG5", "Chinese Traditional");
	fill(GEANY_ENCODING_BIG5_HKSCS, "BIG5-HKSCS", "Chinese Traditional");
	fill(GEANY_ENCODING_CP_866, "CP866", "Cyrillic/Russian");
	
	fill(GEANY_ENCODING_EUC_JP, "EUC-JP", "Japanese");
	fill(GEANY_ENCODING_EUC_KR, "EUC-KR", "Korean");
	fill(GEANY_ENCODING_EUC_TW, "EUC-TW", "Chinese Traditional");

	fill(GEANY_ENCODING_GB18030, "GB18030", "Chinese Simplified");
	fill(GEANY_ENCODING_GB2312, "GB2312", "Chinese Simplified");
	fill(GEANY_ENCODING_GBK, "GBK", "Chinese Simplified");
	fill(GEANY_ENCODING_GEOSTD8, "GEORGIAN-ACADEMY", "Georgian");
	fill(GEANY_ENCODING_HZ, "HZ", "Chinese Simplified");
	
	fill(GEANY_ENCODING_IBM_850, "IBM850", "Western");
	fill(GEANY_ENCODING_IBM_852, "IBM852", "Central European");
	fill(GEANY_ENCODING_IBM_855, "IBM855", "Cyrillic");
	fill(GEANY_ENCODING_IBM_857, "IBM857", "Turkish");
	fill(GEANY_ENCODING_IBM_862, "IBM862", "Hebrew");
	fill(GEANY_ENCODING_IBM_864, "IBM864", "Arabic");
	
	fill(GEANY_ENCODING_ISO_2022_JP, "ISO-2022-JP", "Japanese");
	fill(GEANY_ENCODING_ISO_2022_KR, "ISO-2022-KR", "Korean");
	fill(GEANY_ENCODING_ISO_IR_111, "ISO-IR-111", "Cyrillic");
	fill(GEANY_ENCODING_JOHAB, "JOHAB", "Korean");
	fill(GEANY_ENCODING_KOI8_R, "KOI8R", "Cyrillic");
	fill(GEANY_ENCODING_KOI8_U, "KOI8U", "Cyrillic/Ukrainian");
	
	fill(GEANY_ENCODING_SHIFT_JIS, "SHIFT_JIS", "Japanese");
	fill(GEANY_ENCODING_TCVN, "TCVN", "Vietnamese");
	fill(GEANY_ENCODING_TIS_620, "TIS-620", "Thai");
	fill(GEANY_ENCODING_UHC, "UHC", "Korean");
	fill(GEANY_ENCODING_VISCII, "VISCII", "Vietnamese");

	fill(GEANY_ENCODING_WINDOWS_1250, "WINDOWS-1250", "Central European");
	fill(GEANY_ENCODING_WINDOWS_1251, "WINDOWS-1251", "Cyrillic");
	fill(GEANY_ENCODING_WINDOWS_1252, "WINDOWS-1252", "Western");
	fill(GEANY_ENCODING_WINDOWS_1253, "WINDOWS-1253", "Greek");
	fill(GEANY_ENCODING_WINDOWS_1254, "WINDOWS-1254", "Turkish");
	fill(GEANY_ENCODING_WINDOWS_1255, "WINDOWS-1255", "Hebrew");
	fill(GEANY_ENCODING_WINDOWS_1256, "WINDOWS-1256", "Arabic");
	fill(GEANY_ENCODING_WINDOWS_1257, "WINDOWS-1257", "Baltic");
	fill(GEANY_ENCODING_WINDOWS_1258, "WINDOWS-1258", "Vietnamese");
}


static void encodings_lazy_init(void)
{
	static gboolean initialized = FALSE;
	gint i;

	if (initialized)
		return;

	g_return_if_fail(G_N_ELEMENTS(encodings) == GEANY_ENCODINGS_MAX);

	i = 0;
	while (i < GEANY_ENCODINGS_MAX)
	{
		g_return_if_fail(encodings[i].idx == i);

		/* Translate the names */
		encodings[i].name = _(encodings[i].name);

		++i;
    }

	initialized = TRUE;
}


const GeanyEncoding *encodings_get_from_charset(const gchar *charset)
{
	gint i;

	encodings_lazy_init ();

	i = 0;
	while (i < GEANY_ENCODINGS_MAX)
	{
		if (strcmp(charset, encodings[i].charset) == 0)
			return &encodings[i];

		++i;
	}

	return NULL;
}


const GeanyEncoding *encodings_get_from_index(gint index)
{
	g_return_val_if_fail(index >= 0, NULL);

	if (index >= GEANY_ENCODINGS_MAX)
		return NULL;

	encodings_lazy_init();

	return &encodings[index];
}


gchar *encodings_to_string(const GeanyEncoding* enc)
{
	g_return_val_if_fail(enc != NULL, NULL);
	g_return_val_if_fail(enc->name != NULL, NULL);
	g_return_val_if_fail(enc->charset != NULL, NULL);

	encodings_lazy_init();

    return g_strdup_printf("%s (%s)", enc->name, enc->charset);
}


const gchar *encodings_get_charset(const GeanyEncoding* enc)
{
/*	g_return_val_if_fail(enc != NULL, NULL);
	g_return_val_if_fail(enc->charset != NULL, NULL);
*/
	if (enc == NULL) return NULL;

	encodings_lazy_init();

	return enc->charset;
}


/* Encodings */
GList *encodings_get_encodings(GList *encoding_strings)
{
	GList *res = NULL;

	if (encoding_strings != NULL)
	{
		GList *tmp;
		const GeanyEncoding *enc;

		tmp = encoding_strings;

		while (tmp)
		{
		      const char *charset = tmp->data;

		      if (strcmp(charset, "current") == 0)
			      g_get_charset(&charset);

		      g_return_val_if_fail(charset != NULL, NULL);
		      enc = encodings_get_from_charset(charset);

		      if (enc != NULL)
				res = g_list_append(res, (gpointer)enc);

		      tmp = g_list_next(tmp);
		}
	}
	return res;
}


void encodings_init(void)
{
	GtkWidget *item;
	GtkWidget *menu;
	gchar *label;
	guint i = 0;
	
	init_encodings();
	encodings_lazy_init();

	// create encodings submenu in document menu
	menu = lookup_widget(app->window, "set_encoding1_menu");
	while (i < GEANY_ENCODINGS_MAX)
	{
		if (encodings[i].idx != i) break;

		label = encodings_to_string(&encodings[i]);
		item = gtk_menu_item_new_with_label(label);
		gtk_widget_show(item);
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect((gpointer) item, "activate", G_CALLBACK(on_encoding_change),
													GINT_TO_POINTER(encodings[i].idx));
		g_free(label);
		i++;
    }
}
