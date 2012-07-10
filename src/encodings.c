/*
 *      encodings.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * Encoding conversion and Byte Order Mark (BOM) handling.
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
#include "document.h"
#include "documentprivate.h"
#include "msgwindow.h"
#include "encodings.h"
#include "callbacks.h"
#include "ui_utils.h"

/* <meta http-equiv="content-type" content="text/html; charset=UTF-8" /> */
#define PATTERN_HTMLMETA "<meta\\s+http-equiv\\s*=\\s*\"?content-type\"?\\s+content\\s*=\\s*\"text/x?html;\\s*charset=([a-z0-9_-]+)\"\\s*/?>"
/* " geany_encoding=utf-8 " or " coding: utf-8 " */
#define PATTERN_CODING "coding[\t ]*[:=][\t ]*\"?([a-z0-9-]+)\"?[\t ]*"

/* precompiled regexps */
static GRegex *pregs[2];
static gboolean pregs_loaded = FALSE;


GeanyEncoding encodings[GEANY_ENCODINGS_MAX];


#define fill(Order, Group, Idx, Charset, Name) \
		encodings[Idx].idx = Idx; \
		encodings[Idx].order = Order; \
		encodings[Idx].group = Group; \
		encodings[Idx].charset = Charset; \
		encodings[Idx].name = Name;

static void init_encodings(void)
{
	fill(0,		WESTEUROPEAN,	GEANY_ENCODING_ISO_8859_14,		"ISO-8859-14",		_("Celtic"));
	fill(1,		WESTEUROPEAN,	GEANY_ENCODING_ISO_8859_7,		"ISO-8859-7",		_("Greek"));
	fill(2,		WESTEUROPEAN,	GEANY_ENCODING_WINDOWS_1253,	"WINDOWS-1253",		_("Greek"));
	fill(3,		WESTEUROPEAN,	GEANY_ENCODING_ISO_8859_10,		"ISO-8859-10",		_("Nordic"));
	fill(4,		WESTEUROPEAN,	GEANY_ENCODING_ISO_8859_3,		"ISO-8859-3",		_("South European"));
	fill(5,		WESTEUROPEAN,	GEANY_ENCODING_IBM_850,			"IBM850",			_("Western"));
	fill(6,		WESTEUROPEAN,	GEANY_ENCODING_ISO_8859_1,		"ISO-8859-1",		_("Western"));
	fill(7,		WESTEUROPEAN,	GEANY_ENCODING_ISO_8859_15,		"ISO-8859-15",		_("Western"));
	fill(8,		WESTEUROPEAN,	GEANY_ENCODING_WINDOWS_1252,	"WINDOWS-1252",		_("Western"));

	fill(0,		EASTEUROPEAN,	GEANY_ENCODING_ISO_8859_4,		"ISO-8859-4",		_("Baltic"));
	fill(1,		EASTEUROPEAN,	GEANY_ENCODING_ISO_8859_13,		"ISO-8859-13",		_("Baltic"));
	fill(2,		EASTEUROPEAN,	GEANY_ENCODING_WINDOWS_1257,	"WINDOWS-1257",		_("Baltic"));
	fill(3,		EASTEUROPEAN,	GEANY_ENCODING_IBM_852,			"IBM852",			_("Central European"));
	fill(4,		EASTEUROPEAN,	GEANY_ENCODING_ISO_8859_2,		"ISO-8859-2",		_("Central European"));
	fill(5,		EASTEUROPEAN,	GEANY_ENCODING_WINDOWS_1250,	"WINDOWS-1250",		_("Central European"));
	fill(6,		EASTEUROPEAN,	GEANY_ENCODING_IBM_855,			"IBM855",			_("Cyrillic"));
	fill(7,		EASTEUROPEAN,	GEANY_ENCODING_ISO_8859_5,		"ISO-8859-5",		_("Cyrillic"));
	/* ISO-IR-111 not available on Windows */
	fill(8,		EASTEUROPEAN,	GEANY_ENCODING_ISO_IR_111,		"ISO-IR-111",		_("Cyrillic"));
	fill(9,		EASTEUROPEAN,	GEANY_ENCODING_KOI8_R,			"KOI8-R",			_("Cyrillic"));
	fill(10,	EASTEUROPEAN,	GEANY_ENCODING_WINDOWS_1251,	"WINDOWS-1251",		_("Cyrillic"));
	fill(11,	EASTEUROPEAN,	GEANY_ENCODING_CP_866,			"CP866",			_("Cyrillic/Russian"));
	fill(12,	EASTEUROPEAN,	GEANY_ENCODING_KOI8_U,			"KOI8-U",			_("Cyrillic/Ukrainian"));
	fill(13,	EASTEUROPEAN,	GEANY_ENCODING_ISO_8859_16,		"ISO-8859-16",		_("Romanian"));

	fill(0,		MIDDLEEASTERN,	GEANY_ENCODING_IBM_864,			"IBM864",			_("Arabic"));
	fill(1,		MIDDLEEASTERN,	GEANY_ENCODING_ISO_8859_6,		"ISO-8859-6",		_("Arabic"));
	fill(2,		MIDDLEEASTERN,	GEANY_ENCODING_WINDOWS_1256,	"WINDOWS-1256",		_("Arabic"));
	fill(3,		MIDDLEEASTERN,	GEANY_ENCODING_IBM_862,			"IBM862",			_("Hebrew"));
	/* not available at all, ? */
	fill(4,		MIDDLEEASTERN,	GEANY_ENCODING_ISO_8859_8_I,	"ISO-8859-8-I",		_("Hebrew"));
	fill(5,		MIDDLEEASTERN,	GEANY_ENCODING_WINDOWS_1255,	"WINDOWS-1255",		_("Hebrew"));
	fill(6,		MIDDLEEASTERN,	GEANY_ENCODING_ISO_8859_8,		"ISO-8859-8",		_("Hebrew Visual"));

	fill(0,		ASIAN,			GEANY_ENCODING_ARMSCII_8,		"ARMSCII-8",		_("Armenian"));
	fill(1,		ASIAN,			GEANY_ENCODING_GEOSTD8,			"GEORGIAN-ACADEMY",	_("Georgian"));
	fill(2,		ASIAN,			GEANY_ENCODING_TIS_620,			"TIS-620",			_("Thai"));
	fill(3,		ASIAN,			GEANY_ENCODING_IBM_857,			"IBM857",			_("Turkish"));
	fill(4,		ASIAN,			GEANY_ENCODING_WINDOWS_1254,	"WINDOWS-1254",		_("Turkish"));
	fill(5,		ASIAN,			GEANY_ENCODING_ISO_8859_9,		"ISO-8859-9",		_("Turkish"));
	fill(6,		ASIAN,			GEANY_ENCODING_TCVN,			"TCVN",				_("Vietnamese"));
	fill(7,		ASIAN,			GEANY_ENCODING_VISCII,			"VISCII",			_("Vietnamese"));
	fill(8,		ASIAN,			GEANY_ENCODING_WINDOWS_1258,	"WINDOWS-1258",		_("Vietnamese"));

	fill(0,		UNICODE,		GEANY_ENCODING_UTF_7,			"UTF-7",			_("Unicode"));
	fill(1,		UNICODE,		GEANY_ENCODING_UTF_8,			"UTF-8",			_("Unicode"));
	fill(2,		UNICODE,		GEANY_ENCODING_UTF_16LE,		"UTF-16LE",			_("Unicode"));
	fill(3,		UNICODE,		GEANY_ENCODING_UTF_16BE,		"UTF-16BE",			_("Unicode"));
	fill(4,		UNICODE,		GEANY_ENCODING_UCS_2LE,			"UCS-2LE",			_("Unicode"));
	fill(5,		UNICODE,		GEANY_ENCODING_UCS_2BE,			"UCS-2BE",			_("Unicode"));
	fill(6,		UNICODE,		GEANY_ENCODING_UTF_32LE,		"UTF-32LE",			_("Unicode"));
	fill(7,		UNICODE,		GEANY_ENCODING_UTF_32BE,		"UTF-32BE",			_("Unicode"));

	fill(0,		EASTASIAN,		GEANY_ENCODING_GB18030,			"GB18030",			_("Chinese Simplified"));
	fill(1,		EASTASIAN,		GEANY_ENCODING_GB2312,			"GB2312",			_("Chinese Simplified"));
	fill(2,		EASTASIAN,		GEANY_ENCODING_GBK,				"GBK",				_("Chinese Simplified"));
	/* maybe not available on Linux */
	fill(3,		EASTASIAN,		GEANY_ENCODING_HZ,				"HZ",				_("Chinese Simplified"));
	fill(4,		EASTASIAN,		GEANY_ENCODING_BIG5,			"BIG5",				_("Chinese Traditional"));
	fill(5,		EASTASIAN,		GEANY_ENCODING_BIG5_HKSCS,		"BIG5-HKSCS",		_("Chinese Traditional"));
	fill(6,		EASTASIAN,		GEANY_ENCODING_EUC_TW,			"EUC-TW",			_("Chinese Traditional"));
	fill(7,		EASTASIAN,		GEANY_ENCODING_EUC_JP,			"EUC-JP",			_("Japanese"));
	fill(8,		EASTASIAN,		GEANY_ENCODING_ISO_2022_JP,		"ISO-2022-JP",		_("Japanese"));
	fill(9,		EASTASIAN,		GEANY_ENCODING_SHIFT_JIS,		"SHIFT_JIS",		_("Japanese"));
	fill(10,	EASTASIAN,		GEANY_ENCODING_CP_932,			"CP932",			_("Japanese"));
	fill(11,	EASTASIAN,		GEANY_ENCODING_EUC_KR,			"EUC-KR",			_("Korean"));
	fill(12,	EASTASIAN,		GEANY_ENCODING_ISO_2022_KR,		"ISO-2022-KR",		_("Korean"));
	fill(13,	EASTASIAN,		GEANY_ENCODING_JOHAB,			"JOHAB",			_("Korean"));
	fill(14,	EASTASIAN,		GEANY_ENCODING_UHC,				"UHC",				_("Korean"));

	fill(0,		NONE,			GEANY_ENCODING_NONE,			"None",				_("Without encoding"));
}


/* compares two encoding names in a permissive fashion.
 * e.g. "utf8" matches "UTF-8", "iso8859_1" matches "ISO-8859-1", etc. */
static gboolean encodings_charset_equals(const gchar *a, const gchar *b)
{
	gboolean was_alpha = FALSE; /* whether last character of previous word was a letter */
	gboolean need_sep = FALSE; /* whether we're expecting an implicit separator */

	while (*a && *b)
	{
		gboolean is_alpha;

		if (g_ascii_toupper(*a) == g_ascii_toupper(*b) &&
			((is_alpha = g_ascii_isalpha(*a)) || g_ascii_isdigit(*a)))
		{
			/* either there was a real separator, or we need a implicit one (a chage from alpha to
			 * numeric or so) */
			if (! need_sep || (was_alpha != is_alpha))
			{
				a++;
				b++;
				was_alpha = is_alpha;
				need_sep = FALSE;
			}
			else
				return FALSE;
		}
		else
		{
			guint n_sep = 0;

			if (! g_ascii_isalnum(*a))
			{
				a++;
				n_sep++;
			}
			if (! g_ascii_isalnum(*b))
			{
				b++;
				n_sep++;
			}
			if (n_sep < 1)
				return FALSE;
			else if (n_sep < 2)
				need_sep = TRUE;
		}
	}
	return *a == *b;
}


GeanyEncodingIndex encodings_get_idx_from_charset(const gchar *charset)
{
	gint i;

	if (charset == NULL)
		return GEANY_ENCODING_UTF_8;

	i = 0;
	while (i < GEANY_ENCODINGS_MAX)
	{
		if (encodings_charset_equals(charset, encodings[i].charset))
			return i;

		++i;
	}
	return GEANY_ENCODING_UTF_8;
}


const GeanyEncoding *encodings_get_from_charset(const gchar *charset)
{
	gint i;

	if (charset == NULL)
		return &encodings[GEANY_ENCODING_UTF_8];

	i = 0;
	while (i < GEANY_ENCODINGS_MAX)
	{
		if (encodings_charset_equals(charset, encodings[i].charset))
			return &encodings[i];

		++i;
	}

	return NULL;
}


static const gchar *encodings_normalize_charset(const gchar *charset)
{
	const GeanyEncoding *encoding;

	encoding = encodings_get_from_charset(charset);
	if (encoding != NULL)
		return encoding->charset;

	return NULL;
}


const GeanyEncoding *encodings_get_from_index(gint idx)
{
	g_return_val_if_fail(idx >= 0 && idx < GEANY_ENCODINGS_MAX, NULL);

	return &encodings[idx];
}


/**
 *  Gets the character set name of the specified index e.g. for use with
 *  @ref document_set_encoding().
 *
 *  @param idx @ref GeanyEncodingIndex to retrieve the corresponding character set.
 *
 *
 *  @return The charset according to idx, or @c NULL if the index is invalid.
 *
 *  @since 0.13
 **/
const gchar* encodings_get_charset_from_index(gint idx)
{
	g_return_val_if_fail(idx >= 0 && idx < GEANY_ENCODINGS_MAX, NULL);

	return encodings[idx].charset;
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


static GtkWidget *radio_items[GEANY_ENCODINGS_MAX];


void encodings_select_radio_item(const gchar *charset)
{
	gint i;

	g_return_if_fail(charset != NULL);

	i = 0;
	while (i < GEANY_ENCODINGS_MAX)
	{
		if (utils_str_equal(charset, encodings[i].charset))
			break;
		i++;
	}
	if (i == GEANY_ENCODINGS_MAX)
		i = GEANY_ENCODING_UTF_8; /* fallback to UTF-8 */

	/* ignore_callback has to be set by the caller */
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(radio_items[i]), TRUE);
}


/* Regexp detection of file encoding declared in the file itself.
 * Idea and parts of code taken from Bluefish, thanks.
 * regex_compile() is used to compile regular expressions on program init and keep it in memory
 * for faster access when opening a file. Pre-compiled regexps will be freed on program exit.
 */
static GRegex *regex_compile(const gchar *pattern)
{
	GError *error = NULL;
	GRegex *regex = g_regex_new(pattern, G_REGEX_CASELESS, 0, &error);

	if (!regex)
	{
		geany_debug("Failed to compile encoding regex (%s)", error->message);
		g_error_free(error);
	}
	return regex;
}


static gchar *regex_match(GRegex *preg, const gchar *buffer, gsize size)
{
	gchar *encoding = NULL;
	GMatchInfo *minfo;

	if (G_UNLIKELY(! pregs_loaded || buffer == NULL))
		return NULL;

	/* scan only the first 512 characters in the buffer */
	size = MIN(size, 512);

	if (g_regex_match_full(preg, buffer, size, 0, 0, &minfo, NULL) &&
		g_match_info_get_match_count(minfo) >= 2)
	{
		encoding = g_match_info_fetch(minfo, 1);
		geany_debug("Detected encoding by regex search: %s", encoding);

		SETPTR(encoding, g_utf8_strup(encoding, -1));
	}
	g_match_info_free(minfo);
	return encoding;
}


static void encodings_radio_item_change_cb(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	GeanyDocument *doc = document_get_current();
	guint i = GPOINTER_TO_INT(user_data);

	if (ignore_callback || doc == NULL || encodings[i].charset == NULL ||
		! gtk_check_menu_item_get_active(menuitem) ||
		utils_str_equal(encodings[i].charset, doc->encoding))
		return;

	if (doc->readonly)
	{
		utils_beep();
		return;
	}
	document_undo_add(doc, UNDO_ENCODING, g_strdup(doc->encoding));

	document_set_encoding(doc, encodings[i].charset);
}


void encodings_finalize(void)
{
	if (pregs_loaded)
	{
		guint i, len;
		len = G_N_ELEMENTS(pregs);
		for (i = 0; i < len; i++)
		{
			g_regex_unref(pregs[i]);
		}
	}
}


void encodings_init(void)
{
	GtkWidget *item, *menu[2], *submenu, *menu_westeuro, *menu_easteuro, *menu_eastasian, *menu_asian,
			  *menu_utf8, *menu_middleeast, *item_westeuro, *item_easteuro, *item_eastasian,
			  *item_asian, *item_utf8, *item_middleeast;
	GCallback cb_func[2];
	GSList *group = NULL;
	gchar *label;
	gint order, group_size;
	guint i, j, k;

	init_encodings();

	if (! pregs_loaded)
	{
		pregs[0] = regex_compile(PATTERN_HTMLMETA);
		pregs[1] = regex_compile(PATTERN_CODING);
		pregs_loaded = TRUE;
	}

	/* create encodings submenu in document menu */
	menu[0] = ui_lookup_widget(main_widgets.window, "set_encoding1_menu");
	menu[1] = ui_lookup_widget(main_widgets.window, "menu_reload_as1_menu");
	cb_func[0] = G_CALLBACK(encodings_radio_item_change_cb);
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

		/** TODO can it be optimized? ATM 3782 runs at line "if (encodings[j].group ...)" */
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
				default: submenu = menu[k]; group_size = 1;
			}

			while (order < group_size)	/* the biggest group has 13 elements */
			{
				for (j = 0; j < GEANY_ENCODINGS_MAX; j++)
				{
					if (encodings[j].group == i && encodings[j].order == order)
					{
						label = encodings_to_string(&encodings[j]);
						if (k == 0)
						{
							item = gtk_radio_menu_item_new_with_label(group, label);
							group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
							radio_items[j] = item;
						}
						else
							item = gtk_menu_item_new_with_label(label);
						gtk_widget_show(item);
						gtk_container_add(GTK_CONTAINER(submenu), item);
						g_signal_connect(item, "activate",
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


/**
 *  Tries to convert @a buffer into UTF-8 encoding from the encoding specified with @a charset.
 *  If @a fast is not set, additional checks to validate the converted string are performed.
 *
 *  @param buffer The input string to convert.
 *  @param size The length of the string, or -1 if the string is nul-terminated.
 *  @param charset The charset to be used for conversion.
 *  @param fast @c TRUE to only convert the input and skip extended checks on the converted string.
 *
 *  @return If the conversion was successful, a newly allocated nul-terminated string,
 *    which must be freed with @c g_free(). Otherwise @c NULL.
 **/
gchar *encodings_convert_to_utf8_from_charset(const gchar *buffer, gssize size,
											  const gchar *charset, gboolean fast)
{
	gchar *utf8_content = NULL;
	GError *conv_error = NULL;
	gchar* converted_contents = NULL;
	gsize bytes_written;

	g_return_val_if_fail(buffer != NULL, NULL);
	g_return_val_if_fail(charset != NULL, NULL);

	converted_contents = g_convert(buffer, size, "UTF-8", charset, NULL,
								   &bytes_written, &conv_error);

	if (fast)
	{
		utf8_content = converted_contents;
		if (conv_error != NULL) g_error_free(conv_error);
	}
	else if (conv_error != NULL || ! g_utf8_validate(converted_contents, bytes_written, NULL))
	{
		if (conv_error != NULL)
		{
			geany_debug("Couldn't convert from %s to UTF-8 (%s).", charset, conv_error->message);
			g_error_free(conv_error);
			conv_error = NULL;
		}
		else
			geany_debug("Couldn't convert from %s to UTF-8.", charset);

		utf8_content = NULL;
		g_free(converted_contents);
	}
	else
	{
		geany_debug("Converted from %s to UTF-8.", charset);
		utf8_content = converted_contents;
	}

	return utf8_content;
}


static gchar *encodings_check_regexes(const gchar *buffer, gsize size)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(pregs); i++)
	{
		gchar *charset;

		if ((charset = regex_match(pregs[i], buffer, size)) != NULL)
			return charset;
	}
	return NULL;
}


static gchar *encodings_convert_to_utf8_with_suggestion(const gchar *buffer, gssize size,
		const gchar *suggested_charset, gchar **used_encoding)
{
	const gchar *locale_charset = NULL;
	const gchar *charset;
	gchar *utf8_content;
	gboolean check_suggestion = suggested_charset != NULL;
	gboolean check_locale = FALSE;
	gint i, preferred_charset;

	if (size == -1)
	{
		size = strlen(buffer);
	}

	/* current locale is not UTF-8, we have to check this charset */
	check_locale = ! g_get_charset(&locale_charset);

	/* First check for preferred charset, if specified */
	preferred_charset = file_prefs.default_open_encoding;

	if (preferred_charset == encodings[GEANY_ENCODING_NONE].idx ||
		preferred_charset < 0 ||
		preferred_charset >= GEANY_ENCODINGS_MAX)
	{
		preferred_charset = -1;
	}

	/* -1 means "Preferred charset" */
	for (i = -1; i < GEANY_ENCODINGS_MAX; i++)
	{
		if (G_UNLIKELY(i == encodings[GEANY_ENCODING_NONE].idx))
			continue;

		if (check_suggestion)
		{
			check_suggestion = FALSE;
			charset = encodings_normalize_charset(suggested_charset);
			if (charset == NULL) /* we failed at normalizing suggested encoding, try it as is */
				charset = suggested_charset;
			i = -2; /* keep i below the start value to have it again at -1 on the next loop run */
		}
		else if (check_locale)
		{
			check_locale = FALSE;
			charset = locale_charset;
			i = -2; /* keep i below the start value to have it again at -1 on the next loop run */
		}
		else if (i == -1)
		{
			if (preferred_charset >= 0)
			{
				charset = encodings[preferred_charset].charset;
				geany_debug("Using preferred charset: %s", charset);
			}
			else
				continue;
		}
		else if (i >= 0)
			charset = encodings[i].charset;
		else /* in this case we have i == -2, continue to increase i and go ahead */
			continue;

		if (G_UNLIKELY(charset == NULL))
			continue;

		geany_debug("Trying to convert %" G_GSIZE_FORMAT " bytes of data from %s into UTF-8.",
			size, charset);
		utf8_content = encodings_convert_to_utf8_from_charset(buffer, size, charset, FALSE);

		if (G_LIKELY(utf8_content != NULL))
		{
			if (used_encoding != NULL)
			{
				if (G_UNLIKELY(*used_encoding != NULL))
				{
					geany_debug("%s:%d", __FILE__, __LINE__);
					g_free(*used_encoding);
				}
				*used_encoding = g_strdup(charset);
			}
			return utf8_content;
		}
	}

	return NULL;
}


/**
 *  Tries to convert @a buffer into UTF-8 encoding and store the detected original encoding in
 *  @a used_encoding.
 *
 *  @param buffer the input string to convert.
 *  @param size the length of the string, or -1 if the string is nul-terminated.
 *  @param used_encoding return location of the detected encoding of the input string, or @c NULL.
 *
 *  @return If the conversion was successful, a newly allocated nul-terminated string,
 *    which must be freed with @c g_free(). Otherwise @c NULL.
 **/
gchar *encodings_convert_to_utf8(const gchar *buffer, gssize size, gchar **used_encoding)
{
	gchar *regex_charset;
	gchar *utf8;

	/* first try to read the encoding from the file content */
	regex_charset = encodings_check_regexes(buffer, size);
	utf8 = encodings_convert_to_utf8_with_suggestion(buffer, size, regex_charset, used_encoding);
	g_free(regex_charset);

	return utf8;
}


/* If there's a BOM, return a corresponding GEANY_ENCODING_UTF_* index,
 * otherwise GEANY_ENCODING_NONE.
 * */
GeanyEncodingIndex encodings_scan_unicode_bom(const gchar *string, gsize len, guint *bom_len)
{
	if (len >= 3)
	{
		if (bom_len)
			*bom_len = 3;

		if ((guchar)string[0] == 0xef && (guchar)string[1] == 0xbb &&
			(guchar)string[2] == 0xbf)
		{
			return GEANY_ENCODING_UTF_8;
		}
	}
	if (len >= 4)
	{
		if (bom_len)
			*bom_len = 4;

		if ((guchar)string[0] == 0x00 && (guchar)string[1] == 0x00 &&
				 (guchar)string[2] == 0xfe && (guchar)string[3] == 0xff)
		{
			return GEANY_ENCODING_UTF_32BE; /* Big endian */
		}
		if ((guchar)string[0] == 0xff && (guchar)string[1] == 0xfe &&
				 (guchar)string[2] == 0x00 && (guchar)string[3] == 0x00)
		{
			return GEANY_ENCODING_UTF_32LE; /* Little endian */
		}
		if ((string[0] == 0x2b && string[1] == 0x2f && string[2] == 0x76) &&
				 (string[3] == 0x38 || string[3] == 0x39 || string[3] == 0x2b || string[3] == 0x2f))
		{
			 return GEANY_ENCODING_UTF_7;
		}
	}
	if (len >= 2)
	{
		if (bom_len)
			*bom_len = 2;

		if ((guchar)string[0] == 0xfe && (guchar)string[1] == 0xff)
		{
			return GEANY_ENCODING_UTF_16BE; /* Big endian */
		}
		if ((guchar)string[0] == 0xff && (guchar)string[1] == 0xfe)
		{
			return GEANY_ENCODING_UTF_16LE; /* Little endian */
		}
	}
	if (bom_len)
		*bom_len = 0;
	return GEANY_ENCODING_NONE;
}


gboolean encodings_is_unicode_charset(const gchar *string)
{
	if (string != NULL &&
		(strncmp(string, "UTF", 3) == 0 || strncmp(string, "UCS", 3) == 0))
	{
		return TRUE;
	}
	return FALSE;
}


typedef struct
{
	gchar		*data;	/* null-terminated data */
	gsize		 size;	/* actual data size */
	gsize		 len;	/* string length of data */
	gchar		*enc;
	gboolean	 bom;
	gboolean	 partial;
} BufferData;


/* convert data with the specified encoding */
static gboolean
handle_forced_encoding(BufferData *buffer, const gchar *forced_enc)
{
	GeanyEncodingIndex enc_idx;

	if (utils_str_equal(forced_enc, "UTF-8"))
	{
		if (! g_utf8_validate(buffer->data, buffer->len, NULL))
		{
			return FALSE;
		}
	}
	else
	{
		gchar *converted_text = encodings_convert_to_utf8_from_charset(
										buffer->data, buffer->size, forced_enc, FALSE);
		if (converted_text == NULL)
		{
			return FALSE;
		}
		else
		{
			SETPTR(buffer->data, converted_text);
			buffer->len = strlen(converted_text);
		}
	}
	enc_idx = encodings_scan_unicode_bom(buffer->data, buffer->size, NULL);
	buffer->bom = (enc_idx == GEANY_ENCODING_UTF_8);
	buffer->enc = g_strdup(forced_enc);
	return TRUE;
}


/* detect encoding and convert to UTF-8 if necessary */
static gboolean
handle_encoding(BufferData *buffer, GeanyEncodingIndex enc_idx)
{
	g_return_val_if_fail(buffer->enc == NULL, FALSE);
	g_return_val_if_fail(buffer->bom == FALSE, FALSE);

	if (buffer->size == 0)
	{
		/* we have no data so assume UTF-8, buffer->len can be 0 even we have an empty
		 * e.g. UTF32 file with a BOM(so size is 4, len is 0) */
		buffer->enc = g_strdup("UTF-8");
	}
	else
	{
		/* first check for a BOM */
		if (enc_idx != GEANY_ENCODING_NONE)
		{
			buffer->enc = g_strdup(encodings[enc_idx].charset);
			buffer->bom = TRUE;

			if (enc_idx != GEANY_ENCODING_UTF_8) /* the BOM indicated something else than UTF-8 */
			{
				gchar *converted_text = encodings_convert_to_utf8_from_charset(
										buffer->data, buffer->size, buffer->enc, FALSE);
				if (converted_text != NULL)
				{
					SETPTR(buffer->data, converted_text);
					buffer->len = strlen(converted_text);
				}
				else
				{
					/* there was a problem converting data from BOM encoding type */
					SETPTR(buffer->enc, NULL);
					buffer->bom = FALSE;
				}
			}
		}

		if (buffer->enc == NULL)	/* either there was no BOM or the BOM encoding failed */
		{
			/* first try to read the encoding from the file content */
			gchar *regex_charset = encodings_check_regexes(buffer->data, buffer->size);

			/* try UTF-8 first */
			if (encodings_get_idx_from_charset(regex_charset) == GEANY_ENCODING_UTF_8 &&
				(buffer->size == buffer->len) && g_utf8_validate(buffer->data, buffer->len, NULL))
			{
				buffer->enc = g_strdup("UTF-8");
			}
			else
			{
				/* detect the encoding */
				gchar *converted_text = encodings_convert_to_utf8_with_suggestion(buffer->data,
					buffer->size, regex_charset, &buffer->enc);

				if (converted_text == NULL)
				{
					g_free(regex_charset);
					return FALSE;
				}
				SETPTR(buffer->data, converted_text);
				buffer->len = strlen(converted_text);
			}
			g_free(regex_charset);
		}
	}
	return TRUE;
}


static void
handle_bom(BufferData *buffer)
{
	guint bom_len;

	encodings_scan_unicode_bom(buffer->data, buffer->size, &bom_len);
	g_return_if_fail(bom_len != 0);

	/* use filedata->len here because the contents are already converted into UTF-8 */
	buffer->len -= bom_len;
	/* overwrite the BOM with the remainder of the file contents, plus the NULL terminator. */
	g_memmove(buffer->data, buffer->data + bom_len, buffer->len + 1);
	buffer->data = g_realloc(buffer->data, buffer->len + 1);
}


/* loads textfile data, verifies and converts to forced_enc or UTF-8. Also handles BOM. */
static gboolean handle_buffer(BufferData *buffer, const gchar *forced_enc)
{
	GeanyEncodingIndex tmp_enc_idx;

	/* temporarily retrieve the encoding idx based on the BOM to suppress the following warning
	 * if we have a BOM */
	tmp_enc_idx = encodings_scan_unicode_bom(buffer->data, buffer->size, NULL);

	/* check whether the size of the loaded data is equal to the size of the file in the
	 * filesystem file size may be 0 to allow opening files in /proc/ which have typically a
	 * file size of 0 bytes */
	if (buffer->len != buffer->size && buffer->size != 0 && (
		tmp_enc_idx == GEANY_ENCODING_UTF_8 || /* tmp_enc_idx can be UTF-7/8/16/32, UCS and None */
		tmp_enc_idx == GEANY_ENCODING_UTF_7))  /* filter UTF-7/8 where no NULL bytes are allowed */
	{
		buffer->partial = TRUE;
	}

	/* Determine character encoding and convert to UTF-8 */
	if (forced_enc != NULL)
	{
		/* the encoding should be ignored(requested by user), so open the file "as it is" */
		if (utils_str_equal(forced_enc, encodings[GEANY_ENCODING_NONE].charset))
		{
			buffer->bom = FALSE;
			buffer->enc = g_strdup(encodings[GEANY_ENCODING_NONE].charset);
		}
		else if (! handle_forced_encoding(buffer, forced_enc))
		{
			return FALSE;
		}
	}
	else if (! handle_encoding(buffer, tmp_enc_idx))
	{
		return FALSE;
	}

	if (buffer->bom)
		handle_bom(buffer);
	return TRUE;
}


/*
 * Tries to convert @a buffer into UTF-8 encoding. Unlike encodings_convert_to_utf8()
 * and encodings_convert_to_utf8_from_charset() it handles the possible BOM in the data.
 *
 * @param buf a pointer to modifiable null-terminated buffer to convert.
 *   It may or may not be modified, and should be freed whatever happens.
 * @param size a pointer to the size of the buffer (expected to be e.g. the on-disk
 *   file size). It will be updated to the new size.
 * @param forced_enc forced encoding to use, or @c NULL
 * @param used_encoding return location for the actually used encoding, or @c NULL
 * @param has_bom return location to store whether the data had a BOM, or @c NULL
 * @param partial return location to store whether the conversion may be partial, or @c NULL
 *
 * @return @C TRUE if the conversion succeeded, @c FALSE otherwise.
 */
gboolean encodings_convert_to_utf8_auto(gchar **buf, gsize *size, const gchar *forced_enc,
		gchar **used_encoding, gboolean *has_bom, gboolean *partial)
{
	BufferData buffer;

	buffer.data = *buf;
	buffer.size = *size;
	/* use strlen to check for null chars */
	buffer.len = strlen(buffer.data);
	buffer.enc = NULL;
	buffer.bom = FALSE;
	buffer.partial = FALSE;

	if (! handle_buffer(&buffer, forced_enc))
		return FALSE;

	*size = buffer.len;
	if (used_encoding)
		*used_encoding = buffer.enc;
	else
		g_free(buffer.enc);
	if (has_bom)
		*has_bom = buffer.bom;
	if (partial)
		*partial = buffer.partial;

	*buf = buffer.data;
	return TRUE;
}
