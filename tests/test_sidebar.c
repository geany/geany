
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "document.h"
#include "documentprivate.h"
#include "keyfile.h"
#include "main.h"
#include "sidebar.h"
#include "ui_utils.h"
#include "utils.h"

#define SIDEBAR_TEST_ADD(path, func) g_test_add_func("/sidebar/" path, func);

static void openfiles_add(const gchar **file_names)
{
	const gchar *file;

	while ((file = *file_names++))
	{
		GeanyDocument *doc = g_new0(GeanyDocument, 1);

		doc->priv = g_new0(GeanyDocumentPrivate, 1);
		doc->file_name = strdup(file);

		sidebar_openfiles_add(doc);
	}
}


static gboolean tree_count_cb(GtkTreeModel *model, GtkTreePath *path,
                              GtkTreeIter *iter, gpointer data_in)
{
	gint *c = (gint *) data_in;

	*c = *c + 1;
	return FALSE;
}


static gboolean tree_strings_cb(GtkTreeModel *model, GtkTreePath *path,
                                GtkTreeIter *iter, gpointer data_in)
{
	gchar **data = (gchar **) data_in;
	gchar *file;

	gtk_tree_model_get(model, iter, DOCUMENTS_SHORTNAME, &file, -1);
	data[g_strv_length(data)] = file;

	printf("%s\n", file);
	return FALSE;
}

void do_test_sidebar_openfiles(const gchar **test_data, const gchar **expected)
{
#ifdef HAVE_G_STRV_EQUAL
	int count = 0;
	GtkTreeStore *store;
	gchar **data;

	store = sidebar_create_store_openfiles();

	openfiles_add(test_data);

	gtk_tree_model_foreach(GTK_TREE_MODEL(store), tree_count_cb, (gpointer) &count);
	data = g_new0(gchar *, count + 1);
	gtk_tree_model_foreach(GTK_TREE_MODEL(store), tree_strings_cb, (gpointer) data);
	g_assert_true(g_strv_equal(expected, (const gchar * const *) data));
#else
	g_test_skip("Need g_strv_equal(), since GLib 2.60");
#endif
}

void test_sidebar_openfiles_none(void)
{
	const gchar *files[] = {
		"/tmp/x",
		"/tmp/b/a",
		"/tmp/b/b",
		NULL
	};
	const gchar *expected[] = {
		"a",
		"b",
		"x",
		NULL
	};

	interface_prefs.openfiles_path_mode = OPENFILES_PATHS_NONE;
	do_test_sidebar_openfiles(files, expected);
}


void test_sidebar_openfiles_path(void)
{
	const gchar *files[] = {
		"/tmp/x",
		"/tmp/b/a",
		"/tmp/b/b",
		NULL
	};
	const gchar *expected[] = {
		"/tmp",
		 "x",
		"/tmp/b",
		 "a",
		 "b",
		NULL
	};

	interface_prefs.openfiles_path_mode = OPENFILES_PATHS_LIST;
	do_test_sidebar_openfiles(files, expected);
}


void test_sidebar_openfiles_tree(void)
{
	const gchar *files[] = {
		"/tmp/x",
		"/tmp/b/a",
		"/tmp/b/b",
		NULL
	};
	const gchar *expected[] = {
		"/tmp",
		 "x",
		 "b",
		  "a",
		  "b",
		NULL
	};

	interface_prefs.openfiles_path_mode = OPENFILES_PATHS_TREE;
	do_test_sidebar_openfiles(files, expected);
}

int main(int argc, char **argv)
{
	/* In headless environments that do not provide a virtual display,
	 * GTK initialisation is expected to fail and gtk_init_check()
	 * will return FALSE. Calling any GTK function or instantiating
	 * any GTK type afterwards results in undefined behavior.
	 *
	 * By returning 77, we can tell test harnesses compatible with
	 * the GNU standard approach to regard the test as skipped.
	 */
	if (! gtk_init_check(&argc, &argv))
	{
		g_test_message("GTK initialization failed; skipping. Running inside a headless environment?");
		return 77;
	}

	g_test_init(&argc, &argv, NULL);

	main_init_headless();

	SIDEBAR_TEST_ADD("openfiles_none", test_sidebar_openfiles_none);
	SIDEBAR_TEST_ADD("openfiles_path", test_sidebar_openfiles_path);
	SIDEBAR_TEST_ADD("openfiles_tree", test_sidebar_openfiles_tree);

	return g_test_run();
}
