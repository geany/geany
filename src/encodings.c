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
 *  $Id$
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



#define fill(v, w, x, y, z) \
		encodings[x].idx = x; \
		encodings[x].order = v; \
		encodings[x].group = w; \
		encodings[x].charset = y; \
		encodings[x].name = z;

static void init_encodings(void)
{
	fill(0, WESTEUROPEAN, GEANY_ENCODING_ISO_8859_14, "ISO-8859-14", _("Celtic"));
	fill(1, WESTEUROPEAN, GEANY_ENCODING_ISO_8859_7, "ISO-8859-7", _("Greek"));
	fill(2, WESTEUROPEAN, GEANY_ENCODING_WINDOWS_1253, "WINDOWS-1253", _("Greek"));
	fill(3, WESTEUROPEAN, GEANY_ENCODING_ISO_8859_10, "ISO-8859-10", _("Nordic"));
	fill(4, WESTEUROPEAN, GEANY_ENCODING_ISO_8859_3, "ISO-8859-3", _("South European"));
	fill(5, WESTEUROPEAN, GEANY_ENCODING_IBM_850, "IBM850", _("Western"));
	fill(6, WESTEUROPEAN, GEANY_ENCODING_ISO_8859_1, "ISO-8859-1", _("Western"));
	fill(7, WESTEUROPEAN, GEANY_ENCODING_ISO_8859_15, "ISO-8859-15", _("Western"));
	fill(8, WESTEUROPEAN, GEANY_ENCODING_WINDOWS_1252, "WINDOWS-1252", _("Western"));

	fill(0, EASTEUROPEAN, GEANY_ENCODING_ISO_8859_4, "ISO-8859-4", _("Baltic"));
	fill(1, EASTEUROPEAN, GEANY_ENCODING_ISO_8859_13, "ISO-8859-13", _("Baltic"));
	fill(2, EASTEUROPEAN, GEANY_ENCODING_WINDOWS_1257, "WINDOWS-1257", _("Baltic"));
	fill(3, EASTEUROPEAN, GEANY_ENCODING_IBM_852, "IBM852", _("Central European"));
	fill(4, EASTEUROPEAN, GEANY_ENCODING_ISO_8859_2, "ISO-8859-2", _("Central European"));
	fill(5, EASTEUROPEAN, GEANY_ENCODING_WINDOWS_1250, "WINDOWS-1250", _("Central European"));
	fill(6, EASTEUROPEAN, GEANY_ENCODING_IBM_855, "IBM855", _("Cyrillic"));
	fill(7, EASTEUROPEAN, GEANY_ENCODING_ISO_8859_5, "ISO-8859-5", _("Cyrillic"));
	fill(8, EASTEUROPEAN, GEANY_ENCODING_ISO_IR_111, "ISO-IR-111", _("Cyrillic"));
	fill(9, EASTEUROPEAN, GEANY_ENCODING_KOI8_R, "KOI8R", _("Cyrillic"));
	fill(10, EASTEUROPEAN, GEANY_ENCODING_WINDOWS_1251, "WINDOWS-1251", _("Cyrillic"));
	fill(11, EASTEUROPEAN, GEANY_ENCODING_CP_866, "CP866", _("Cyrillic/Russian"));
	fill(12, EASTEUROPEAN, GEANY_ENCODING_KOI8_U, "KOI8U", _("Cyrillic/Ukrainian"));
	fill(13, EASTEUROPEAN, GEANY_ENCODING_ISO_8859_16, "ISO-8859-16", _("Romanian"));

	fill(0, MIDDLEEASTERN, GEANY_ENCODING_IBM_864, "IBM864", _("Arabic"));
	fill(1, MIDDLEEASTERN, GEANY_ENCODING_ISO_8859_6, "ISO-8859-6", _("Arabic"));
	fill(2, MIDDLEEASTERN, GEANY_ENCODING_WINDOWS_1256, "WINDOWS-1256", _("Arabic"));
	fill(3, MIDDLEEASTERN, GEANY_ENCODING_IBM_862, "IBM862", _("Hebrew"));
	fill(4, MIDDLEEASTERN, GEANY_ENCODING_ISO_8859_8_I, "ISO-8859-8-I", _("Hebrew"));
	fill(5, MIDDLEEASTERN, GEANY_ENCODING_WINDOWS_1255, "WINDOWS-1255", _("Hebrew"));
	fill(6, MIDDLEEASTERN, GEANY_ENCODING_ISO_8859_8, "ISO-8859-8", _("Hebrew Visual"));

	fill(0, ASIAN, GEANY_ENCODING_ARMSCII_8, "ARMSCII-8", _("Armenian"));
	fill(1, ASIAN, GEANY_ENCODING_GEOSTD8, "GEORGIAN-ACADEMY", _("Georgian"));
	fill(2, ASIAN, GEANY_ENCODING_TIS_620, "TIS-620", _("Thai"));
	fill(3, ASIAN, GEANY_ENCODING_IBM_857, "IBM857", _("Turkish"));
	fill(4, ASIAN, GEANY_ENCODING_WINDOWS_1254, "WINDOWS-1254", _("Turkish"));
	fill(5, ASIAN, GEANY_ENCODING_ISO_8859_9, "ISO-8859-9", _("Turkish"));
	fill(6, ASIAN, GEANY_ENCODING_TCVN, "TCVN", _("Vietnamese"));
	fill(7, ASIAN, GEANY_ENCODING_VISCII, "VISCII", _("Vietnamese"));
	fill(8, ASIAN, GEANY_ENCODING_WINDOWS_1258, "WINDOWS-1258", _("Vietnamese"));

	fill(0, UNICODE, GEANY_ENCODING_UTF_7, "UTF-7", _("Unicode"));
	fill(1, UNICODE, GEANY_ENCODING_UTF_8, "UTF-8", _("Unicode"));
	fill(2, UNICODE, GEANY_ENCODING_UTF_16LE, "UTF-16LE", _("Unicode"));
	fill(3, UNICODE, GEANY_ENCODING_UTF_16BE, "UTF-16BE", _("Unicode"));
	fill(4, UNICODE, GEANY_ENCODING_UCS_2LE, "UCS-2LE", _("Unicode"));
	fill(5, UNICODE, GEANY_ENCODING_UCS_2BE, "UCS-2BE", _("Unicode"));
	fill(6, UNICODE, GEANY_ENCODING_UTF_32LE, "UTF-32LE", _("Unicode"));
	fill(7, UNICODE, GEANY_ENCODING_UTF_32BE, "UTF-32BE", _("Unicode"));

	fill(0, EASTASIAN, GEANY_ENCODING_GB18030, "GB18030", _("Chinese Simplified"));
	fill(1, EASTASIAN, GEANY_ENCODING_GB2312, "GB2312", _("Chinese Simplified"));
	fill(2, EASTASIAN, GEANY_ENCODING_GBK, "GBK", _("Chinese Simplified"));
	fill(3, EASTASIAN, GEANY_ENCODING_HZ, "HZ", _("Chinese Simplified"));
	fill(4, EASTASIAN, GEANY_ENCODING_BIG5, "BIG5", _("Chinese Traditional"));
	fill(5, EASTASIAN, GEANY_ENCODING_BIG5_HKSCS, "BIG5-HKSCS", _("Chinese Traditional"));
	fill(6, EASTASIAN, GEANY_ENCODING_EUC_TW, "EUC-TW", _("Chinese Traditional"));
	fill(7, EASTASIAN, GEANY_ENCODING_EUC_JP, "EUC-JP", _("Japanese"));
	fill(8, EASTASIAN, GEANY_ENCODING_ISO_2022_JP, "ISO-2022-JP", _("Japanese"));
	fill(9, EASTASIAN, GEANY_ENCODING_SHIFT_JIS, "SHIFT_JIS", _("Japanese"));
	fill(10, EASTASIAN, GEANY_ENCODING_EUC_KR, "EUC-KR", _("Korean"));
	fill(11, EASTASIAN, GEANY_ENCODING_ISO_2022_KR, "ISO-2022-KR", _("Korean"));
	fill(12, EASTASIAN, GEANY_ENCODING_JOHAB, "JOHAB", _("Korean"));
	fill(13, EASTASIAN, GEANY_ENCODING_UHC, "UHC", _("Korean"));
}


const GeanyEncoding *encodings_get_from_charset(const gchar *charset)
{
	gint i;

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
	g_return_val_if_fail(index < GEANY_ENCODINGS_MAX, NULL);

	return &encodings[index];
}


gchar *encodings_to_string(const GeanyEncoding* enc)
{
	g_return_val_if_fail(enc != NULL, NULL);
	g_return_val_if_fail(enc->name != NULL, NULL);
	g_return_val_if_fail(enc->charset != NULL, NULL);

    return g_strdup_printf("%s (%s)", enc->name, enc->charset);
}


const gchar *encodings_get_charset(const GeanyEncoding* enc)
{
	g_return_val_if_fail(enc != NULL, NULL);
	g_return_val_if_fail(enc->charset != NULL, NULL);

	return enc->charset;
}


void encodings_init(void)
{
	GtkWidget *item, *menu[2], *submenu, *menu_westeuro, *menu_easteuro, *menu_eastasian, *menu_asian,
			  *menu_utf8, *menu_middleeast, *item_westeuro, *item_easteuro, *item_eastasian,
			  *item_asian, *item_utf8, *item_middleeast;
	GCallback cb_func[2];
	gchar *label;
	guint i, j, k, order, group_size;

	init_encodings();

	// create encodings submenu in document menu
	menu[0] = lookup_widget(app->window, "set_encoding1_menu");
	menu[1] = lookup_widget(app->window, "menu_reload_as1_menu");
	cb_func[0] = G_CALLBACK(on_encoding_change);
	cb_func[1] = G_CALLBACK(on_reload_as_activate);

	for (k = 0; k < 2; k++)
	{
		menu_westeuro = gtk_menu_new();
		item_westeuro = gtk_menu_item_new_with_mnemonic(_("_West European"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_westeuro), menu_westeuro);
		gtk_container_add(GTK_CONTAINER(menu[k]), item_westeuro);
		gtk_widget_show_all(item_westeuro);

		menu_easteuro = gtk_menu_new();
		item_easteuro = gtk_menu_item_new_with_mnemonic(_("_East European"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_easteuro), menu_easteuro);
		gtk_container_add(GTK_CONTAINER(menu[k]), item_easteuro);
		gtk_widget_show_all(item_easteuro);

		menu_eastasian = gtk_menu_new();
		item_eastasian = gtk_menu_item_new_with_mnemonic(_("East _Asian"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_eastasian), menu_eastasian);
		gtk_container_add(GTK_CONTAINER(menu[k]), item_eastasian);
		gtk_widget_show_all(item_eastasian);

		menu_asian = gtk_menu_new();
		item_asian = gtk_menu_item_new_with_mnemonic(_("_SE & SW Asian"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_asian), menu_asian);
		gtk_container_add(GTK_CONTAINER(menu[k]), item_asian);
		gtk_widget_show_all(item_asian);

		menu_middleeast = gtk_menu_new();
		item_middleeast = gtk_menu_item_new_with_mnemonic(_("_Middle Eastern"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_middleeast), menu_middleeast);
		gtk_container_add(GTK_CONTAINER(menu[k]), item_middleeast);
		gtk_widget_show_all(item_middleeast);

		menu_utf8 = gtk_menu_new();
		item_utf8 = gtk_menu_item_new_with_mnemonic(_("_Unicode"));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_utf8), menu_utf8);
		gtk_container_add(GTK_CONTAINER(menu[k]), item_utf8);
		gtk_widget_show_all(item_utf8);

		/// TODO can it be optimized? ATM 3782 runs at line 239
		for (i = 0; i < GEANY_ENCODING_GROUPS_MAX; i++)
		{
			order = 0;
			switch (i)
			{
				case WESTEUROPEAN: submenu = menu_westeuro; group_size = 9; break;
				case EASTEUROPEAN: submenu = menu_easteuro; group_size = 14; break;
				case EASTASIAN: submenu = menu_eastasian; group_size = 14; break;
				case ASIAN: submenu = menu_asian; group_size = 9; break;
				case MIDDLEEASTERN: submenu = menu_middleeast; group_size = 7; break;
				case UNICODE: submenu = menu_utf8; group_size = 8; break;
				default: submenu = menu[k]; group_size = 0;
			}

			while (order < group_size)	// the biggest group has 13 elements
			{
				for (j = 0; j < GEANY_ENCODINGS_MAX; j++)
				{
					if (encodings[j].group == i && encodings[j].order == order)
					{
						label = encodings_to_string(&encodings[j]);
						item = gtk_menu_item_new_with_label(label);
						gtk_widget_show(item);
						gtk_container_add(GTK_CONTAINER(submenu), item);
						g_signal_connect((gpointer) item, "activate",
										cb_func[k], GINT_TO_POINTER(encodings[j].idx));
						g_free(label);
						break;
					}
				}
				order++;
			}
		}
	}
}
