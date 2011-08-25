/*
 * BinReloc - a library for creating relocatable executables
 * Written by: Mike Hearn <mike@theoretic.com>
 *             Hongli Lai <h.lai@chello.nl>
 * http://autopackage.org/
 *
 * This source code is public domain. You can relicense this code
 * under whatever license you want.
 *
 * NOTE: if you're using C++ and are getting "undefined reference
 * to br_*", try renaming prefix.c to prefix.cpp
 */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif


/*
 * enrico - all the code below is only compiled and used if ENABLE_BINRELOC is set in config.h,
 *          this only happens if configure option --enable-binreloc was used
 */
#ifdef ENABLE_BINRELOC



#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <glib.h>
#include "prefix.h"


#undef NULL
#define NULL ((void *) 0)

#ifdef __GNUC__
	#define br_return_val_if_fail(expr,val) if (!(expr)) {fprintf (stderr, "** BinReloc (%s): assertion %s failed\n", __PRETTY_FUNCTION__, #expr); return val;}
#else
	#define br_return_val_if_fail(expr, val) if (!(expr)) return val
#endif /* __GNUC__ */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>


static char *br_last_value = (char*)NULL;


static void
br_free_last_value ()
{
	if (br_last_value)
		free (br_last_value);
}


/**
 * br_thread_local_store:
 * str: A dynamically allocated string.
 * Returns: str. This return value must not be freed.
 *
 * Store str in a thread-local variable and return str. The next
 * you run this function, that variable is freed too.
 * This function is created so you don't have to worry about freeing
 * strings.
 *
 * Example:
 * char *foo;
 * foo = thread_local_store (strdup ("hello")); --> foo == "hello"
 * foo = thread_local_store (strdup ("world")); --> foo == "world"; "hello" is now freed.
 */
const char *
br_thread_local_store (char *str)
{
	static int initialized = 0;


	if (!initialized)
	{
		atexit (br_free_last_value);
		initialized = 1;
	}

	if (br_last_value)
		free (br_last_value);
	br_last_value = str;

	return (const char *) str;
}


/**
 * br_locate:
 * symbol: A symbol that belongs to the app/library you want to locate.
 * Returns: A newly allocated string containing the full path of the
 *	    app/library that func belongs to, or NULL on error. This
 *	    string should be freed when not when no longer needed.
 *
 * Finds out to which application or library symbol belongs, then locate
 * the full path of that application or library.
 * Note that symbol cannot be a pointer to a function. That will not work.
 *
 * Example:
 * --> main.c
 * #include "prefix.h"
 * #include "libfoo.h"
 *
 * int main (int argc, char *argv[]) {
 *	printf ("Full path of this app: %s\n", br_locate (&argc));
 *	libfoo_start ();
 *	return 0;
 * }
 *
 * --> libfoo.c starts here
 * #include "prefix.h"
 *
 * void libfoo_start () {
 *	--> "" is a symbol that belongs to libfoo (because it's called
 *	--> from libfoo_start()); that's why this works.
 *	printf ("libfoo is located in: %s\n", br_locate (""));
 * }
 */
char *
br_locate (void *symbol)
{
	char line[5000];
	FILE *f;
	char *path;

	br_return_val_if_fail (symbol != NULL, NULL);

	f = fopen ("/proc/self/maps", "r");
	if (!f)
		return NULL;

	while (!feof (f))
	{
		unsigned long start, end;

		if (!fgets (line, sizeof (line), f))
			continue;
		if (!strstr (line, " r-xp ") || !strchr (line, '/'))
			continue;

		sscanf (line, "%lx-%lx ", &start, &end);
		if (symbol >= (void *) start && symbol < (void *) end)
		{
			char *tmp;
			gsize len;

			/* Extract the filename; it is always an absolute path */
			path = strchr (line, '/');

			/* Get rid of the newline */
			tmp = strrchr (path, '\n');
			if (tmp) *tmp = 0;

			/* Get rid of "(deleted)" */
			len = strlen (path);
			if (len > 10 && strcmp (path + len - 10, " (deleted)") == 0)
			{
				tmp = path + len - 10;
				*tmp = 0;
			}

			fclose(f);
			return strdup (path);
		}
	}

	fclose (f);
	return NULL;
}


/**
 * br_extract_prefix:
 * path: The full path of an executable or library.
 * Returns: The prefix, or NULL on error. This string should be freed when no longer needed.
 *
 * Extracts the prefix from path. This function assumes that your executable
 * or library is installed in an LSB-compatible directory structure.
 *
 * Example:
 * br_extract_prefix ("/usr/bin/gnome-panel");       --> Returns "/usr"
 * br_extract_prefix ("/usr/local/lib/libfoo.so");   --> Returns "/usr/local"
 * br_extract_prefix ("/usr/local/libfoo.so");       --> Returns "/usr"
 */
char *
br_extract_prefix (const char *path)
{
	char *end, *tmp, *result;

	br_return_val_if_fail (path != (char*)NULL, (char*)NULL);

	if (!*path) return strdup ("/");
	end = strrchr (path, '/');
	if (!end) return strdup (path);

	tmp = g_strndup ((char *) path, end - path);
	if (!*tmp)
	{
		g_free (tmp);
		return strdup ("/");
	}
	end = strrchr (tmp, '/');
	if (!end) return tmp;

	result = g_strndup (tmp, end - tmp);
	g_free (tmp);

	if (!*result)
	{
		g_free (result);
		result = strdup ("/");
	}

	return result;
}


/**
 * br_locate_prefix:
 * symbol: A symbol that belongs to the app/library you want to locate.
 * Returns: A prefix. This string should be freed when no longer needed.
 *
 * Locates the full path of the app/library that symbol belongs to, and return
 * the prefix of that path, or NULL on error.
 * Note that symbol cannot be a pointer to a function. That will not work.
 *
 * Example:
 * --> This application is located in /usr/bin/foo
 * br_locate_prefix (&argc);   --> returns: "/usr"
 */
char *
br_locate_prefix (void *symbol)
{
	char *path, *prefix;

	br_return_val_if_fail (symbol != NULL, NULL);

	path = br_locate (symbol);
	if (!path) return NULL;

	prefix = br_extract_prefix (path);
	free (path);
	return prefix;
}


/**
 * br_prepend_prefix:
 * symbol: A symbol that belongs to the app/library you want to locate.
 * path: The path that you want to prepend the prefix to.
 * Returns: The new path, or NULL on error. This string should be freed when no
 *	    longer needed.
 *
 * Gets the prefix of the app/library that symbol belongs to. Prepend that prefix to path.
 * Note that symbol cannot be a pointer to a function. That will not work.
 *
 * Example:
 * --> The application is /usr/bin/foo
 * br_prepend_prefix (&argc, "/share/foo/data.png");   --> Returns "/usr/share/foo/data.png"
 */
char *
br_prepend_prefix (void *symbol, char *path)
{
	char *tmp, *newpath;

	br_return_val_if_fail (symbol != NULL, NULL);
	br_return_val_if_fail (path != NULL, NULL);

	tmp = br_locate_prefix (symbol);
	if (!tmp) return NULL;

	if (strcmp (tmp, "/") == 0)
		newpath = strdup (path);
	else
		newpath = g_strconcat (tmp, path, NULL);

	free (tmp);
	return newpath;
}


#else /* ENABLE_BINRELOC */

typedef int iso_c_forbids_an_empty_source_file;

#endif /* ENABLE_BINRELOC */
