/*
 *      Copyright 2023 The Geany contributors
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

#include "encodingsprivate.h"
#include "main.h"


/* Asserts 2 bytes buffers are identical, trying to provide a somewhat useful
 * error if not. */
static void assert_cmpmem_eq_impl(const char *p1, const char *p2, gsize len,
		const char *domain, const char *file, int line, const char *func,
		const char *expr)
{
	gchar *msg;
	gsize i;

	for (i = 0; i < len && p1[i] == p2[i]; i++)
		;
	if (i == len)
		return;

	msg = g_strdup_printf("assertion failed (%s): bytes %#x and %#x differ at offset %lu (at \"%s\" and \"%s\")",
			expr, (guint) (guchar) p1[i], (guint) (guchar) p2[i], i, p1 + i, p2 + i);
	g_assertion_message(domain, file, line, func, msg);
	g_free(msg);
}

#define assert_cmpmem_eq_with_caller(p1, p2, len, domain, file, line, func) \
	assert_cmpmem_eq_impl(p1, p2, len, domain, file, line, func, #p1 " == " #p2)

#define assert_cmpmem_eq(p1, p2, len) assert_cmpmem_eq_impl(p1, p2, len, \
		G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, #p1 " == " #p2)

/*
 * @brief More convenient test API for encodings_convert_to_utf8_auto()
 * @param input Input buffer, NUL-terminated (well, at least there should be a
 *        trailing NUL).
 * @param input_size Actual size of @p input buffer, without the trailing NUL
 * @param disk_size Size on disk (as reported by e.g stat -- that may be 0 for
 *                  virtual files, otherwise should be input_size)
 * @param forced_enc Forced encoding, or NULL
 * @param expected_output Expected output data
 * @param expected_size Expected output size
 * @param expected_encoding Expected output encoding
 * @param expected_has_bom Whether the input contains a BOM
 * @param expected_partial Whether the output is expected to be truncated
 * @returns Whether the conversion succeeded and followed the parameters
 */
static gboolean assert_convert_to_utf8_auto_impl(
		const char *domain, const char *file, int line, const char *func,
		const gchar *input, gsize input_size,
		const gsize disk_size, const gchar *forced_enc,
		const gchar *expected_output, gsize expected_size, const gchar *expected_encoding,
		gboolean expected_has_bom, gboolean expected_partial)
{
	gchar *buf = g_memdup(input, input_size + 1);
	gsize size = disk_size;
	gchar *used_encoding = NULL;
	gboolean has_bom = FALSE;
	gboolean partial = FALSE;
	gboolean ret;
	GError *err = NULL;

	g_log(domain, G_LOG_LEVEL_INFO, "%s:%d:%s: converting %lu bytes", file, line, func, input_size);
	ret = encodings_convert_to_utf8_auto(&buf, &size, forced_enc, &used_encoding, &has_bom, &partial, &err);
	fflush(stdout);
	if (! ret)
	{
		g_log(domain, G_LOG_LEVEL_INFO, "%s:%d:%s: conversion failed: %s", file, line, func, err->message);
		g_error_free(err);
	}
	else
	{
		assert_cmpmem_eq_with_caller(buf, expected_output, MIN(size, expected_size),
				domain, file, line, func);
		g_assert_cmpuint(size, ==, expected_size);
		if (expected_encoding)
			g_assert_cmpstr(expected_encoding, ==, used_encoding);
		g_assert_cmpint(has_bom, ==, expected_has_bom);
		g_assert_cmpint(partial, ==, expected_partial);

		g_free(used_encoding);
	}

	g_free(buf);

	return ret;
}


#define assert_convert_to_utf8_auto(input, input_size, disk_size, forced_enc, \
		expected_output, expected_size, expected_encoding, expected_has_bom, expected_partial) \
	assert_convert_to_utf8_auto_impl(G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
			input, input_size, disk_size, forced_enc, \
			expected_output, expected_size, expected_encoding, expected_has_bom, expected_partial)


static void test_encodings_convert_ascii_to_utf8_auto(void)
{
#define TEST_ASCII(success, str, forced_enc) \
		g_assert(success == assert_convert_to_utf8_auto(str, G_N_ELEMENTS(str) - 1, G_N_ELEMENTS(str) - 1, \
				forced_enc, str, G_N_ELEMENTS(str) - 1, forced_enc, FALSE, \
				strlen(str) != G_N_ELEMENTS(str) - 1))

	TEST_ASCII(TRUE, "This is a very basic ASCII test", NULL);
	TEST_ASCII(TRUE, "This is a very basic ASCII test", "None");
	TEST_ASCII(TRUE, "This is a very basic ASCII test", "ASCII");
	TEST_ASCII(TRUE, "This is a very basic ASCII test", "UTF-8");
	TEST_ASCII(TRUE, "S\till ve\ry \b\asic", NULL);
	TEST_ASCII(FALSE, "With\0some\0NULs\0", NULL);
	TEST_ASCII(TRUE, "With\0some\0NULs\0", "None");
	TEST_ASCII(FALSE, "With\0some\0NULs\0", "UTF-8");

#undef TEST_ASCII
}


static void test_encodings_convert_utf8_to_utf8_auto(void)
{
#define UTF8_BOM "\xef\xbb\xbf"
#define TEST_UTF8(success, str, forced_enc)																	\
	G_STMT_START {																							\
		gboolean has_bom = strncmp(str, UTF8_BOM, 3) == 0;													\
		g_assert(success == assert_convert_to_utf8_auto(str, G_N_ELEMENTS(str) - 1, G_N_ELEMENTS(str) - 1,	\
				forced_enc, str + (has_bom ? 3 : 0), G_N_ELEMENTS(str) - 1 - (has_bom ? 3 : 0),				\
				forced_enc, has_bom, strlen(str) != G_N_ELEMENTS(str) - 1));								\
	} G_STMT_END

	TEST_UTF8(TRUE, "Thĩs îs å véry basìč ÅSÇǏÍ test", NULL);
	TEST_UTF8(TRUE, "Thĩs îs å véry basìč ÅSÇǏÍ test", "None");
	TEST_UTF8(TRUE, "Thĩs îs å véry basìč ÅSÇǏÍ test", "UTF-8");
	TEST_UTF8(FALSE, "Wíťh\0søme\0NÙLs\0", NULL);
	TEST_UTF8(FALSE, "Wíťh\0søme\0NÙLs\0", "UTF-8"); /* the NUL doesn't pass the UTF-8 check */
	TEST_UTF8(TRUE, "Wíťh\0søme\0NÙLs\0", "None"); /* with None we do no data validation, but report partial output */

	/* with the inline hint */
	TEST_UTF8(TRUE, "coding:utf-8 bãśïč", NULL);
	TEST_UTF8(FALSE, "coding:utf-8 Wíťh\0søme\0NÙLs", NULL);

	TEST_UTF8(TRUE, UTF8_BOM"With BOM", NULL);
	/* These won't pass the UTF-8 validation despite the BOM, so we fallback to
	 * testing other options, and it will succeed with UTF-16 so there's no real
	 * point in verifying this */
	/*TEST_UTF8(FALSE, UTF8_BOM"With BOM\0and NULs", NULL);*/
	/*TEST_UTF8(FALSE, UTF8_BOM"Wíth BØM\0añd NÙLs", NULL);*/

	/* non-UTF-8 */
	TEST_UTF8(FALSE, "Th\xec""s", "UTF-8");
	TEST_UTF8(FALSE, "Th\xec""s\0", "UTF-8");
	TEST_UTF8(FALSE, "\0Th\xec""s", "UTF-8");

#undef TEST_UTF8
#undef UTF8_BOM
}


static void test_encodings_convert_utf_other_to_utf8_auto(void)
{
#define UTF16_LE_BOM "\xff\xfe"
#define UTF16_BE_BOM "\xfe\xff"
#define UTF32_LE_BOM "\xff\xfe\x00\x00"
#define UTF32_BE_BOM "\x00\x00\xfe\xff"
#define TEST_ENC(success, input, output, has_bom, forced_enc, expected_encoding) \
		g_assert(success == assert_convert_to_utf8_auto(input, G_N_ELEMENTS(input) - 1, G_N_ELEMENTS(input) - 1, \
				forced_enc, output, G_N_ELEMENTS(output) - 1, expected_encoding, has_bom, \
				strlen(output) != G_N_ELEMENTS(output) - 1))
#define TEST(success, input, output, has_bom, forced_enc) \
		TEST_ENC(success, input, output, has_bom, forced_enc, forced_enc)

	TEST(TRUE, "N\000o\000 \000B\000O\000M\000", "No BOM", FALSE, NULL);
	TEST(TRUE, "N\000o\000 \000B\000\330\000M\000", "No BØM", FALSE, NULL);
	/* doesn't accept the NULs */
	TEST(FALSE, "N\000o\000 \000B\000O\000M\000\000\000a\000n\000d\000 \000N\000U\000L\000s\000", "No BOM\0and NULs", FALSE, NULL);
	TEST(FALSE, "N\000o\000 \000B\000\330\000M\000\000\000a\000\361\000d\000 \000N\000\331\000L\000s\000", "No BØM\0añd NÙLs", FALSE, NULL);

	TEST(TRUE, UTF16_LE_BOM"W\000i\000t\000h\000 \000B\000O\000M\000", "With BOM", TRUE, NULL);
	TEST(TRUE, UTF16_LE_BOM"W\000i\000t\000h\000 \000B\000\330\000M\000", "With BØM", TRUE, NULL);
	/* doesn't accept the NULs */
	TEST(FALSE, UTF16_LE_BOM"W\000i\000t\000h\000 \000B\000O\000M\000\000\000a\000n\000d\000 \000N\000U\000L\000s\000", "With BOM\0and NULs", TRUE, NULL);
	TEST(FALSE, UTF16_LE_BOM"W\000\355\000t\000h\000 \000B\000\330\000M\000\000\000a\000\361\000d\000 \000N\000\331\000L\000s\000", "Wíth BØM\0añd NÙLs", TRUE, NULL);

	/* We should actually be smarter in our selection of encoding introducing
	 * probability scores, because this loads as UTF-16LE but is "圀椀琀栀 䈀伀䴀"
	 * which doesn't seem to be real Chinese */
	TEST(TRUE, "\000N\000o\000 \000B\000O\000M", "No BOM", FALSE, "UTF-16BE");
	TEST(TRUE, "\000N\000o\000 \000B\000\330\000M", "No BØM", FALSE, NULL);
	/* doesn't accept the NULs -- and see above for the encoding choice */
	TEST(FALSE, "\000N\000o\000 \000B\000O\000M\000\000\000a\000n\000d\000 \000N\000U\000L\000s", "No BOM\0and NULs", FALSE, "UTF-16BE");
	TEST(FALSE, "\000N\000o\000 \000B\000\330\000M\000\000\000a\000\361\000d\000 \000N\000\331\000L\000s", "No BØM\0añd NÙLs", FALSE, NULL);

	TEST(TRUE, UTF16_BE_BOM"\000W\000i\000t\000h\000 \000B\000O\000M", "With BOM", TRUE, NULL);
	TEST(TRUE, UTF16_BE_BOM"\000W\000i\000t\000h\000 \000B\000\330\000M", "With BØM", TRUE, NULL);
	/* doesn't accept the NULs */
	TEST(FALSE, UTF16_BE_BOM"\000W\000i\000t\000h\000 \000B\000O\000M\000\000\000a\000n\000d\000 \000N\000U\000L\000s", "With BOM\0and NULs", TRUE, NULL);
	TEST(FALSE, UTF16_BE_BOM"\000W\000\355\000t\000h\000 \000B\000\330\000M\000\000\000a\000\361\000d\000 \000N\000\331\000L\000s", "Wíth BØM\0añd NÙLs", TRUE, NULL);

	TEST(TRUE, UTF32_LE_BOM"W\000\000\000i\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000O\000\000\000M\000\000\000", "With BOM", TRUE, NULL);
	TEST(TRUE, UTF32_LE_BOM"W\000\000\000i\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000\330\000\000\000M\000\000\000", "With BØM", TRUE, NULL);
	/* doesn't accept the NULs */
	TEST(FALSE, UTF32_LE_BOM"W\000\000\000i\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000O\000\000\000M\000\000\000\000\000\000\000a\000\000\000n\000\000\000d\000\000\000 \000\000\000N\000\000\000U\000\000\000L\000\000\000s\000\000\000", "With BOM\0and NULs", TRUE, NULL);
	TEST(FALSE, UTF32_LE_BOM"W\000\000\000\355\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000\330\000\000\000M\000\000\000\000\000\000\000a\000\000\000\361\000\000\000d\000\000\000 \000\000\000N\000\000\000\331\000\000\000L\000\000\000s\000\000\000", "Wíth BØM\0añd NÙLs", TRUE, NULL);

	TEST(TRUE, UTF32_BE_BOM"\000\000\000W\000\000\000i\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000O\000\000\000M", "With BOM", TRUE, NULL);
	TEST(TRUE, UTF32_BE_BOM"\000\000\000W\000\000\000i\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000\330\000\000\000M", "With BØM", TRUE, NULL);
	/* doesn't accept the NULs */
	TEST(FALSE, UTF32_BE_BOM"\000\000\000W\000\000\000i\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000O\000\000\000M\000\000\000\000\000\000\000a\000\000\000n\000\000\000d\000\000\000 \000\000\000N\000\000\000U\000\000\000L\000\000\000s", "With BOM\0and NULs", TRUE, NULL);
	TEST(FALSE, UTF32_BE_BOM"\000\000\000W\000\000\000\355\000\000\000t\000\000\000h\000\000\000 \000\000\000B\000\000\000\330\000\000\000M\000\000\000\000\000\000\000a\000\000\000\361\000\000\000d\000\000\000 \000\000\000N\000\000\000\331\000\000\000L\000\000\000s", "Wíth BØM\0añd NÙLs", TRUE, NULL);

	/* meh, UTF-7 */
	TEST(TRUE, "No B+ANg-M", "No BØM", FALSE, "UTF-7");
	TEST(TRUE, "+/v8-With B+ANg-M", "With BØM", TRUE, NULL);
	TEST(FALSE, "No B+ANg-M+AAA-but NULs", "No BØM\0but NULs", FALSE, "UTF-7");
	/* Fails to load as UTF-7 because of the NUL, and succeeds as UTF-8 but
	 * obviously doesn't match expectations */
	/*TEST(FALSE, "+/v8-With B+ANg-M+AAA-and NULs", "With BØM\0and NULs", TRUE, NULL);*/

	/* empty data with BOMs */
	TEST_ENC(TRUE, "+/v8-", "", TRUE, NULL, "UTF-7"); /* UTF-7 */
	TEST_ENC(TRUE, UTF16_BE_BOM, "", TRUE, NULL, "UTF-16BE");
	TEST_ENC(TRUE, UTF16_LE_BOM, "", TRUE, NULL, "UTF-16LE");
	TEST_ENC(TRUE, UTF32_BE_BOM, "", TRUE, NULL, "UTF-32BE");
	TEST_ENC(TRUE, UTF32_LE_BOM, "", TRUE, NULL, "UTF-32LE");

#undef TEST
#undef TEST_ENC
#undef UTF32_BE_BOM
#undef UTF32_LE_BOM
#undef UTF16_BE_BOM
#undef UTF16_LE_BOM
}


static void test_encodings_convert_iso8859_to_utf8_auto(void)
{
#define TEST(success, input, output, forced_enc) \
		g_assert(success == assert_convert_to_utf8_auto(input, G_N_ELEMENTS(input) - 1, G_N_ELEMENTS(input) - 1, \
				forced_enc, output, G_N_ELEMENTS(output) - 1, forced_enc, FALSE, \
				strlen(output) != G_N_ELEMENTS(output) - 1))

	TEST(TRUE, "Th\xec""s", "Thìs", NULL);
	TEST(TRUE, "Th\xec""s", "Thìs", "ISO-8859-1");
	TEST(TRUE, "Th\xec""s", "Thìs", "ISO-8859-15");
	TEST(TRUE, "\xa4""uro", "¤uro", "ISO-8859-1");
	TEST(TRUE, "\xa4""uro", "€uro", "ISO-8859-15");
	TEST(TRUE, "\xd8""ed", "Řed", "ISO-8859-2");
	/* make-believe UTF-8 BOM followed by non-UTF-8 data */
	TEST(TRUE, "\xef\xbb\xbf""not B\xd3M", "ï»¿not BÓM", NULL);
	TEST(TRUE, "coding:iso-8859-2 \xd8""ed", "coding:iso-8859-2 Řed", NULL);
	/* with NULs */
	TEST(FALSE, "W\xec""th\0z\xe9""r\xf8""s", "Wìth\0zérøs", "ISO-8859-1");
	TEST(FALSE, "W\xec""th\0z\xe9""r\xf8""s", "Wìth\0zérøs", "ISO-8859-15");
	/* This parses as UTF-16, but that's not really what we'd expect */
	/*TEST(FALSE, "W\xec""th\0z\xe9""r\xf8""s", "Wìth\0zérøs", NULL);*/

	/* UTF-8 BOM with non-UTF-8 data, we should fallback */
	TEST(TRUE, "\xef\xbb\xbfW\xec""th\xf8""ut BOM", "ï»¿Wìthøut BOM", NULL);

#undef TEST
}


int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	gtk_init_check(&argc, &argv);
	main_init_headless();

	g_test_add_func("/encodings/ascii/convert_to_utf8_auto", test_encodings_convert_ascii_to_utf8_auto);
	g_test_add_func("/encodings/utf8/convert_to_utf8_auto", test_encodings_convert_utf8_to_utf8_auto);
	g_test_add_func("/encodings/utf_other/convert_to_utf_other_auto", test_encodings_convert_utf_other_to_utf8_auto);
	g_test_add_func("/encodings/iso8859/convert_to_utf8_auto", test_encodings_convert_iso8859_to_utf8_auto);

	return g_test_run();
}
