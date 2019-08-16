
#include "utils.h"

#include "gtkcompat.h"

#define UTIL_TEST_ADD(path, func) g_test_add_func("/utils/" path, func);


static void test_utils_strv_new(void)
{
	gchar **data;

	data = utils_strv_new("1", NULL);
	g_assert_nonnull(data);
	g_assert_cmpint(g_strv_length(data), ==, 1);
	g_assert_cmpstr(data[0], ==, "1");
	g_strfreev(data);

	data = utils_strv_new("1", "2", "3", NULL);
	g_assert_nonnull(data);
	g_assert_cmpint(g_strv_length(data), ==, 3);
	g_assert_cmpstr(data[0], ==, "1");
	g_assert_cmpstr(data[1], ==, "2");
	g_assert_cmpstr(data[2], ==, "3");
	g_strfreev(data);

	data = utils_strv_new("1", "", "", "4", NULL);
	g_assert_nonnull(data);
	g_assert_cmpint(g_strv_length(data), ==, 4);
	g_assert_cmpstr(data[0], ==, "1");
	g_assert_cmpstr(data[1], ==, "");
	g_assert_cmpstr(data[2], ==, "");
	g_assert_cmpstr(data[3], ==, "4");
	g_strfreev(data);
}

static void test_utils_strv_find_common_prefix(void)
{
	gchar **data, *s;

	s = utils_strv_find_common_prefix(NULL, 0);
	g_assert_null(s);

	data = utils_strv_new("", NULL);
	s = utils_strv_find_common_prefix(data, -1);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("1", "2", "3", NULL);
	s = utils_strv_find_common_prefix(data, -1);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("abc", "amn", "axy", NULL);
	s = utils_strv_find_common_prefix(data, -1);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "a");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("abc", "", "axy", NULL);
	s = utils_strv_find_common_prefix(data, -1);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("22", "23", "33", NULL);
	s = utils_strv_find_common_prefix(data, 1);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "22");
	g_free(s);
	s = utils_strv_find_common_prefix(data, 2);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "2");
	g_free(s);
	s = utils_strv_find_common_prefix(data, 3);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("/home/user/src/geany/src/stash.c", 
	                      "/home/user/src/geany/src/sidebar.c", 
	                      "/home/user/src/geany/src/sidebar.h", 
	                      "/home/user/src/geany/src/sidebar.h",
	                      "/home/user/src/geany/src/main.c",
	                      "/home/user/src/geany-plugins/addons/src/addons.c",
	                      NULL);
	s = utils_strv_find_common_prefix(data, 4);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany/src/s");
	g_free(s);
	s = utils_strv_find_common_prefix(data, 5);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany/src/");
	g_free(s);
	s = utils_strv_find_common_prefix(data, -1);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany");
	g_free(s);
	g_strfreev(data);
}

#define DIR_SEP "\\/"
void test_utils_strv_find_lcs(void)
{
	gchar **data, *s;

	s = utils_strv_find_lcs(NULL, 0, "");
	g_assert_null(s);

	data = utils_strv_new("", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("1", "2", "3", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("abc", "amn", "axy", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "a");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("bca", "mna", "xya", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "a");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("abc", "", "axy", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("a123b", "b123c", "c123d", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "123");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("22", "23", "33", NULL);
	s = utils_strv_find_lcs(data, 1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "22");
	g_free(s);
	s = utils_strv_find_lcs(data, 2, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "2");
	g_free(s);
	s = utils_strv_find_lcs(data, 3, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("/home/user/src/geany/src/stash.c", 
	                      "/home/user/src/geany/src/sidebar.c", 
	                      "/home/user/src/geany/src/sidebar.h", 
	                      "/home/user/src/geany/src/sidebar.h",
	                      "/home/user/src/geany/src/main.c",
	                      "/home/user/src/geany-plugins/addons/src/addons.c",
	                      NULL);
	s = utils_strv_find_lcs(data, 4, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany/src/s");
	g_free(s);
	s = utils_strv_find_lcs(data, 4, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany/src/");
	g_free(s);
	s = utils_strv_find_lcs(data, 5, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany/src/");
	g_free(s);
	s = utils_strv_find_lcs(data, 5, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany/src/");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/geany");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/home/user/src/");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("/src/a/app-1.2.3/src/lib/module/source.c",
	                      "/src/b/app-2.2.3/src/module/source.c", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/module/source.c");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/module/");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("/src/a/app-1.2.3/src/lib/module\\source.c",
	                      "/src/b/app-2.2.3/src/module\\source.c", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/module\\source.c");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/module\\");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("/src/a/app-1.2.3/src/lib/module/",
	                      "/src/b/app-2.2.3/src/module/", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, ".2.3/src/");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/module/");
	g_free(s);
	g_strfreev(data);

	data = utils_strv_new("/usr/local/bin/geany", "/usr/bin/geany", "/home/user/src/geany/src/geany", NULL);
	s = utils_strv_find_lcs(data, -1, "");
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "/geany");
	g_free(s);
	s = utils_strv_find_lcs(data, -1, DIR_SEP);
	g_assert_nonnull(s);
	g_assert_cmpstr(s, ==, "");
	g_free(s);
	g_strfreev(data);
}


/* g_strv_equal is too recent */
static gboolean strv_eq(gchar **strv1, gchar **strv2)
{
	while(1) {
		gchar *s1 = *strv1++;
		gchar *s2 = *strv2++;

		if (!s1 && !s2)
			return TRUE;
		else if (s1 && !s2)
			return FALSE;
		else if (s2 && !s1)
			return FALSE;
		else if (!g_str_equal(s1, s2))
			return FALSE;
	}
}

void test_utils_strv_shorten_file_list(void)
{
	gchar **data, **expected, **result;
	gchar *empty[] = { NULL };

	result = utils_strv_shorten_file_list(NULL, 0);
	expected = empty;
	g_assert_true(strv_eq(result, expected));
	g_strfreev(result);

	data = utils_strv_new("", NULL);
	result = utils_strv_shorten_file_list(data, -1);
	expected = data;
	g_assert_true(strv_eq(result, expected));
	g_strfreev(result);
	g_strfreev(data);

	data = utils_strv_new("1", "2", "3", NULL);
	result = utils_strv_shorten_file_list(data, -1);
	expected = data;
	g_assert_true(strv_eq(result, expected));
	g_strfreev(result);
	g_strfreev(data);

	data = utils_strv_new("abc", "amn", "axy", NULL);
	result = utils_strv_shorten_file_list(data, -1);
	expected = data;
	g_assert_true(strv_eq(result, expected));
	g_strfreev(result);
	g_strfreev(data);

	data = utils_strv_new("abc", "", "axy", NULL);
	result = utils_strv_shorten_file_list(data, -1);
	expected = data;
	g_assert_true(strv_eq(result, expected));
	g_strfreev(result);
	g_strfreev(data);

	data = utils_strv_new("22", "23", "33", NULL);
	result = utils_strv_shorten_file_list(data, 1);
	expected = utils_strv_new("22", NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	g_strfreev(result);
	result = utils_strv_shorten_file_list(data, 2);
	expected = utils_strv_new("22", "23", NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	g_strfreev(result);
	result = utils_strv_shorten_file_list(data, 3);
	expected = utils_strv_new("22", "23", "33", NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	g_strfreev(result);
	g_strfreev(data);

	data = utils_strv_new("/home/user/src/geany/src/stash.c", 
	                      "/home/user/src/geany/src/sidebar.c", 
	                      "/home/user/src/geany/src/sidebar.h", 
	                      "/home/user/src/geany/src/sidebar.h",
	                      "/home/user/src/geany/src/main.c",
	                      "/home/user/src/geany-plugins/addons/src/addons.c",
	                      NULL);
	result = utils_strv_shorten_file_list(data, 4);
	expected = utils_strv_new("stash.c", "sidebar.c", "sidebar.h", "sidebar.h", NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	result = utils_strv_shorten_file_list(data, 5);
	expected = utils_strv_new("stash.c", "sidebar.c", "sidebar.h", "sidebar.h", "main.c", NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	result = utils_strv_shorten_file_list(data, -1);
	expected = utils_strv_new("geany/src/stash.c", "geany/src/sidebar.c",
	                          "geany/src/sidebar.h", "geany/src/sidebar.h", "geany/src/main.c",
	                          "geany-plugins/addons/src/addons.c",
	                          NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	g_strfreev(data);

	data = utils_strv_new("/home/user1/src/geany/src/stash.c", 
	                      "/home/user2/src/geany/src/sidebar.c", 
	                      "/home/user3/src/geany/src/sidebar.h", 
	                      "/home/user4/src/geany/src/sidebar.h",
	                      "/home/user5/src/geany/src/main.c",
	                      NULL);
	result = utils_strv_shorten_file_list(data, -1);
	expected = utils_strv_new("user1/.../stash.c",
	                          "user2/.../sidebar.c",
	                          "user3/.../sidebar.h",
	                          "user4/.../sidebar.h",
	                          "user5/.../main.c",
	                          NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	g_strfreev(result);
	g_strfreev(data);

	data = utils_strv_new("/aaa/bbb/cc/ccccc/ddddd", "/aaa/bbb/xxx/yyy/cc/ccccc/ddddd", NULL);
	result = utils_strv_shorten_file_list(data, -1);
	expected = utils_strv_new("cc/.../ddddd", "xxx/yyy/cc/.../ddddd", NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	g_strfreev(result);
	g_strfreev(data);

	data = utils_strv_new("/src/a/app-1.2.3/src/lib/module/source.c",
	                      "/src/b/app-2.2.3/src/module/source.c", NULL);
	result = utils_strv_shorten_file_list(data, -1);
	expected = utils_strv_new("a/app-1.2.3/src/lib/.../source.c",
	                          "b/app-2.2.3/src/.../source.c", NULL);
	g_assert_true(strv_eq(result, expected));
	g_strfreev(expected);
	g_strfreev(result);
	g_strfreev(data);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);

	UTIL_TEST_ADD("strv_join", test_utils_strv_new);
	UTIL_TEST_ADD("strv_find_common_prefix", test_utils_strv_find_common_prefix);
	UTIL_TEST_ADD("strv_find_lcs", test_utils_strv_find_lcs);
	UTIL_TEST_ADD("strv_shorten_file_list", test_utils_strv_shorten_file_list);

	return g_test_run();
}
