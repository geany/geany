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
	fill(GEANY_ENCODING_ISO_8859_1, "ISO-8859-1", _("Western"));
	fill(GEANY_ENCODING_ISO_8859_2, "ISO-8859-2", _("Central European"));
	fill(GEANY_ENCODING_ISO_8859_3, "ISO-8859-3", _("South European"));
	fill(GEANY_ENCODING_ISO_8859_4, "ISO-8859-4", _("Baltic"));
	fill(GEANY_ENCODING_ISO_8859_5, "ISO-8859-5", _("Cyrillic"));
	fill(GEANY_ENCODING_ISO_8859_6, "ISO-8859-6", _("Arabic"));
	fill(GEANY_ENCODING_ISO_8859_7, "ISO-8859-7", _("Greek"));
	fill(GEANY_ENCODING_ISO_8859_8, "ISO-8859-8", _("Hebrew Visual"));
	fill(GEANY_ENCODING_ISO_8859_8_I, "ISO-8859-8-I", _("Hebrew"));
	fill(GEANY_ENCODING_ISO_8859_9, "ISO-8859-9", _("Turkish"));
	fill(GEANY_ENCODING_ISO_8859_10, "ISO-8859-10", _("Nordic"));
	fill(GEANY_ENCODING_ISO_8859_13, "ISO-8859-13", _("Baltic"));
	fill(GEANY_ENCODING_ISO_8859_14, "ISO-8859-14", _("Celtic"));
	fill(GEANY_ENCODING_ISO_8859_15, "ISO-8859-15", _("Western"));
	fill(GEANY_ENCODING_ISO_8859_16, "ISO-8859-16", _("Romanian"));

	fill(GEANY_ENCODING_UTF_8, "UTF-8", _("Unicode"));
	fill(GEANY_ENCODING_UTF_16, "UTF-16", _("Unicode"));
	fill(GEANY_ENCODING_UCS_2, "UCS-2", _("Unicode"));
	fill(GEANY_ENCODING_UCS_4, "UCS-4", _("Unicode"));

	fill(GEANY_ENCODING_ARMSCII_8, "ARMSCII-8", _("Armenian"));
	fill(GEANY_ENCODING_BIG5, "BIG5", _("Chinese Traditional"));
	fill(GEANY_ENCODING_BIG5_HKSCS, "BIG5-HKSCS", _("Chinese Traditional"));
	fill(GEANY_ENCODING_CP_866, "CP866", _("Cyrillic/Russian"));

	fill(GEANY_ENCODING_EUC_JP, "EUC-JP", _("Japanese"));
	fill(GEANY_ENCODING_EUC_KR, "EUC-KR", _("Korean"));
	fill(GEANY_ENCODING_EUC_TW, "EUC-TW", _("Chinese Traditional"));

	fill(GEANY_ENCODING_GB18030, "GB18030", _("Chinese Simplified"));
	fill(GEANY_ENCODING_GB2312, "GB2312", _("Chinese Simplified"));
	fill(GEANY_ENCODING_GBK, "GBK", _("Chinese Simplified"));
	fill(GEANY_ENCODING_GEOSTD8, "GEORGIAN-ACADEMY", _("Georgian"));
	fill(GEANY_ENCODING_HZ, "HZ", _("Chinese Simplified"));

	fill(GEANY_ENCODING_IBM_850, "IBM850", _("Western"));
	fill(GEANY_ENCODING_IBM_852, "IBM852", _("Central European"));
	fill(GEANY_ENCODING_IBM_855, "IBM855", _("Cyrillic"));
	fill(GEANY_ENCODING_IBM_857, "IBM857", _("Turkish"));
	fill(GEANY_ENCODING_IBM_862, "IBM862", _("Hebrew"));
	fill(GEANY_ENCODING_IBM_864, "IBM864", _("Arabic"));

	fill(GEANY_ENCODING_ISO_2022_JP, "ISO-2022-JP", _("Japanese"));
	fill(GEANY_ENCODING_ISO_2022_KR, "ISO-2022-KR", _("Korean"));
	fill(GEANY_ENCODING_ISO_IR_111, "ISO-IR-111", _("Cyrillic"));
	fill(GEANY_ENCODING_JOHAB, "JOHAB", _("Korean"));
	fill(GEANY_ENCODING_KOI8_R, "KOI8R", _("Cyrillic"));
	fill(GEANY_ENCODING_KOI8_U, "KOI8U", _("Cyrillic/Ukrainian"));

	fill(GEANY_ENCODING_SHIFT_JIS, "SHIFT_JIS", _("Japanese"));
	fill(GEANY_ENCODING_TCVN, "TCVN", _("Vietnamese"));
	fill(GEANY_ENCODING_TIS_620, "TIS-620", _("Thai"));
	fill(GEANY_ENCODING_UHC, "UHC", _("Korean"));
	fill(GEANY_ENCODING_VISCII, "VISCII", _("Vietnamese"));

	fill(GEANY_ENCODING_WINDOWS_1250, "WINDOWS-1250", _("Central European"));
	fill(GEANY_ENCODING_WINDOWS_1251, "WINDOWS-1251", _("Cyrillic"));
	fill(GEANY_ENCODING_WINDOWS_1252, "WINDOWS-1252", _("Western"));
	fill(GEANY_ENCODING_WINDOWS_1253, "WINDOWS-1253", _("Greek"));
	fill(GEANY_ENCODING_WINDOWS_1254, "WINDOWS-1254", _("Turkish"));
	fill(GEANY_ENCODING_WINDOWS_1255, "WINDOWS-1255", _("Hebrew"));
	fill(GEANY_ENCODING_WINDOWS_1256, "WINDOWS-1256", _("Arabic"));
	fill(GEANY_ENCODING_WINDOWS_1257, "WINDOWS-1257", _("Baltic"));
	fill(GEANY_ENCODING_WINDOWS_1258, "WINDOWS-1258", _("Vietnamese"));
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
GList *encodings_get_encodings(void)
{
	GList *res = NULL;
	gint i;

	for (i = 0; i < GEANY_ENCODINGS_MAX; i++)
	{
		if (&encodings[i] != NULL)
			res = g_list_append(res, (gpointer)&encodings[i]);
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
