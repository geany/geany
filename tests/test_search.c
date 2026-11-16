/*
 *      test_search.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2026 The Geany contributors
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "search.h"

#define SEARCH_TEST_ADD(path, func) g_test_add_func("/search/" path, func);


static void test_replace_literal_case_sensitive(void)
{
	guint count = 0;
	gchar *result = search_replace_all_in_text("foo bar foo", "foo", "baz",
		GEANY_FIND_MATCHCASE, &count);

	g_assert_cmpstr(result, ==, "baz bar baz");
	g_assert_cmpuint(count, ==, 2);
	g_free(result);
}


static void test_replace_literal_case_insensitive(void)
{
	guint count = 0;
	gchar *result = search_replace_all_in_text("Foo foo FOO", "foo", "x", 0, &count);

	g_assert_cmpstr(result, ==, "x x x");
	g_assert_cmpuint(count, ==, 3);
	g_free(result);
}


static void test_replace_whole_word(void)
{
	guint count = 0;
	gchar *result = search_replace_all_in_text("foo foobar foo_bar", "foo", "x",
		GEANY_FIND_MATCHCASE | GEANY_FIND_WHOLEWORD, &count);

	g_assert_cmpstr(result, ==, "x foobar foo_bar");
	g_assert_cmpuint(count, ==, 1);
	g_free(result);
}


static void test_replace_regex_backreference(void)
{
	guint count = 0;
	gchar *result = search_replace_all_in_text("abc123 def456", "([a-z]+)([0-9]+)", "\\2-\\1",
		GEANY_FIND_MATCHCASE | GEANY_FIND_REGEXP, &count);

	g_assert_cmpstr(result, ==, "123-abc 456-def");
	g_assert_cmpuint(count, ==, 2);
	g_free(result);
}


static void test_replace_literal_special_chars(void)
{
	guint count = 0;
	gchar *result = search_replace_all_in_text("a.b a.b", "a.b", "$1",
		GEANY_FIND_MATCHCASE, &count);

	g_assert_cmpstr(result, ==, "$1 $1");
	g_assert_cmpuint(count, ==, 2);
	g_free(result);
}


static void test_replace_no_match(void)
{
	guint count = 99;
	gchar *result = search_replace_all_in_text("foo", "bar", "baz",
		GEANY_FIND_MATCHCASE, &count);

	g_assert_cmpstr(result, ==, "foo");
	g_assert_cmpuint(count, ==, 0);
	g_free(result);
}


static void test_replace_bad_regex(void)
{
	guint count = 99;
	gchar *result = search_replace_all_in_text("foo", "(", "bar",
		GEANY_FIND_MATCHCASE | GEANY_FIND_REGEXP, &count);

	g_assert_cmpstr(result, ==, "foo");
	g_assert_cmpuint(count, ==, 0);
	g_free(result);
}


static void test_replace_empty_find(void)
{
	guint count = 99;
	gchar *result = search_replace_all_in_text("foo", "", "bar", 0, &count);

	g_assert_cmpstr(result, ==, "foo");
	g_assert_cmpuint(count, ==, 0);
	g_free(result);
}


int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);

	SEARCH_TEST_ADD("replace/literal_case_sensitive", test_replace_literal_case_sensitive);
	SEARCH_TEST_ADD("replace/literal_case_insensitive", test_replace_literal_case_insensitive);
	SEARCH_TEST_ADD("replace/whole_word", test_replace_whole_word);
	SEARCH_TEST_ADD("replace/regex_backreference", test_replace_regex_backreference);
	SEARCH_TEST_ADD("replace/literal_special_chars", test_replace_literal_special_chars);
	SEARCH_TEST_ADD("replace/no_match", test_replace_no_match);
	SEARCH_TEST_ADD("replace/bad_regex", test_replace_bad_regex);
	SEARCH_TEST_ADD("replace/empty_find", test_replace_empty_find);

	return g_test_run();
}
