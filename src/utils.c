/*
 *      utils.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 * $Id$
 */

/*
 * General utility functions, non-GTK related.
 */

#include "geany.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <glib/gstdio.h>

#include "prefs.h"
#include "support.h"
#include "document.h"
#include "filetypes.h"
#include "dialogs.h"
#include "win32.h"
#include "project.h"

#include "utils.h"


void utils_start_browser(const gchar *uri)
{
#ifdef G_OS_WIN32
	win32_open_browser(uri);
#else
	gchar *cmdline = g_strconcat(tool_prefs.browser_cmd, " ", uri, NULL);

	if (! g_spawn_command_line_async(cmdline, NULL))
	{
		const gchar *argv[3];

		argv[0] = "xdg-open";
		argv[1] = uri;
		argv[2] = NULL;
		if (! g_spawn_async(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH,
				NULL, NULL, NULL, NULL))
		{
			argv[0] = "firefox";
			if (! g_spawn_async(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH,
					NULL, NULL, NULL, NULL))
			{
				argv[0] = "mozilla";
				if (! g_spawn_async(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH, NULL,
						NULL, NULL, NULL))
				{
					argv[0] = "opera";
					if (! g_spawn_async(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH,
							NULL, NULL, NULL, NULL))
					{
						argv[0] = "konqueror";
						if (! g_spawn_async(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH,
								NULL, NULL, NULL, NULL))
						{
							argv[0] = "netscape";
							g_spawn_async(NULL, (gchar**)argv, NULL, G_SPAWN_SEARCH_PATH,
								NULL, NULL, NULL, NULL);
						}
					}
				}
			}
		}
	}
	g_free(cmdline);
#endif
}


/* taken from anjuta, to determine the EOL mode of the file */
gint utils_get_line_endings(const gchar* buffer, glong size)
{
	gint i;
	guint cr, lf, crlf, max_mode;
	gint mode;

	cr = lf = crlf = 0;

	for ( i = 0; i < size ; i++ )
	{
		if ( buffer[i] == 0x0a )
		{
			/* LF */
			lf++;
		}
		else if ( buffer[i] == 0x0d )
		{
			if (i >= (size-1))
			{
				/* Last char, CR */
				cr++;
			}
			else
			{
				if (buffer[i+1] != 0x0a)
				{
					/* CR */
					cr++;
				}
				else
				{
					/* CRLF */
					crlf++;
				}
				i++;
			}
		}
	}

	/* Vote for the maximum */
	mode = SC_EOL_LF;
	max_mode = lf;
	if (crlf > max_mode)
	{
		mode = SC_EOL_CRLF;
		max_mode = crlf;
	}
	if (cr > max_mode)
	{
		mode = SC_EOL_CR;
		max_mode = cr;
	}

	return mode;
}


gboolean utils_isbrace(gchar c, gboolean include_angles)
{
	switch (c)
	{
		case '<':
		case '>':
		return include_angles;

		case '(':
		case ')':
		case '{':
		case '}':
		case '[':
		case ']': return TRUE;
		default:  return FALSE;
	}
}


gboolean utils_is_opening_brace(gchar c, gboolean include_angles)
{
	switch (c)
	{
		case '<':
		return include_angles;

		case '(':
		case '{':
		case '[':  return TRUE;
		default:  return FALSE;
	}
}


/**
 *  Write the given @c text into a file with @c filename.
 *  If the file doesn't exist, it will be created.
 *  If it already exists, it will be overwritten.
 *
 *  @param filename The filename of the file to write, in locale encoding.
 *  @param text The text to write into the file.
 *
 *  @return 0 if the directory was successfully created, otherwise the @c errno of the
 *          failed operation is returned.
 **/
gint utils_write_file(const gchar *filename, const gchar *text)
{
	FILE *fp;
	gint bytes_written, len;

	if (filename == NULL)
	{
		return ENOENT;
	}

	len = strlen(text);

	fp = g_fopen(filename, "w");
	if (fp != NULL)
	{
		bytes_written = fwrite(text, sizeof (gchar), len, fp);
		fclose(fp);

		if (len != bytes_written)
		{
			geany_debug("utils_write_file(): written only %d bytes, had to write %d bytes to %s",
								bytes_written, len, filename);
			return EIO;
		}
	}
	else
	{
		geany_debug("utils_write_file(): could not write to file %s (%s)",
			filename, g_strerror(errno));
		return errno;
	}
	return 0;
}


/*
 * (stolen from anjuta and modified)
 * Search backward through size bytes looking for a '<', then return the tag, if any.
 * @return The tag name
 */
gchar *utils_find_open_xml_tag(const gchar sel[], gint size, gboolean check_tag)
{
	const gchar *begin, *cur;

	if (size < 3)
	{	/* Smallest tag is "<p>" which is 3 characters */
		return NULL;
	}
	begin = &sel[0];
	if (check_tag)
		cur = &sel[size - 3];
	else
		cur = &sel[size - 1];

	cur--; /* Skip past the > */
	while (cur > begin)
	{
		if (*cur == '<') break;
		else if (! check_tag && *cur == '>') break;
		--cur;
	}

	if (*cur == '<')
	{
		GString *result = g_string_sized_new(64);

		cur++;
		while (strchr(":_-.", *cur) || isalnum(*cur))
		{
			g_string_append_c(result, *cur);
			cur++;
		}
		return g_string_free(result, FALSE);
	}

	return NULL;
}


const gchar *utils_get_eol_name(gint eol_mode)
{
	switch (eol_mode)
	{
		case SC_EOL_CRLF: return _("Win (CRLF)"); break;
		case SC_EOL_CR: return _("Mac (CR)"); break;
		default: return _("Unix (LF)"); break;
	}
}


gboolean utils_atob(const gchar *str)
{
	if (str == NULL)
		return FALSE;
	else if (strcmp(str, "TRUE") == 0 || strcmp(str, "true") == 0)
		return TRUE;
	return FALSE;
}


/* NULL-safe version of g_path_is_absolute(). */
gboolean utils_is_absolute_path(const gchar *path)
{
	if (! path || *path == '\0')
		return FALSE;

	return g_path_is_absolute(path);
}


gdouble utils_scale_round(gdouble val, gdouble factor)
{
	/*val = floor(val * factor + 0.5);*/
	val = floor(val);
	val = MAX(val, 0);
	val = MIN(val, factor);

	return val;
}


/**
 *  A replacement function for g_strncasecmp() to compare strings case-insensitive.
 *  It converts both strings into lowercase using g_utf8_strdown() and then compare
 *  both strings using strcmp().
 *  This is not completely accurate regarding locale-specific case sorting rules
 *  but seems to be a good compromise between correctness and performance.
 *
 *  The input strings should be in UTF-8 or locale encoding.
 *
 *  @param s1 Pointer to first string or @a NULL.
 *  @param s2 Pointer to second string or @a NULL.
 *
 *  @return an integer less than, equal to, or greater than zero if @a s1 is found, respectively,
 *          to be less than, to match, or to be greater than @a s2.
 **/
gint utils_str_casecmp(const gchar *s1, const gchar *s2)
{
	gchar *tmp1, *tmp2;
	gint result;

	g_return_val_if_fail(s1 != NULL, 1);
	g_return_val_if_fail(s2 != NULL, -1);

	tmp1 = g_strdup(s1);
	tmp2 = g_strdup(s2);

	/* first ensure strings are UTF-8 */
	if (! g_utf8_validate(s1, -1, NULL))
		setptr(tmp1, g_locale_to_utf8(s1, -1, NULL, NULL, NULL));
	if (! g_utf8_validate(s2, -1, NULL))
		setptr(tmp2, g_locale_to_utf8(s2, -1, NULL, NULL, NULL));

	if (tmp1 == NULL)
	{
		g_free(tmp2);
		return 1;
	}
	if (tmp2 == NULL)
	{
		g_free(tmp1);
		return -1;
	}

	/* then convert the strings into a case-insensitive form */
	setptr(tmp1, g_utf8_strdown(tmp1, -1));
	setptr(tmp2, g_utf8_strdown(tmp2, -1));

	/* compare */
	result = strcmp(tmp1, tmp2);

	g_free(tmp1);
	g_free(tmp2);
	return result;
}


/**
 *  @a NULL-safe string comparison. Returns @a TRUE if both @c a and @c b are @a NULL
 *  or if @c a and @c b refer to valid strings which are equal.
 *
 *  @param a Pointer to first string or @a NULL.
 *  @param b Pointer to second string or @a NULL.
 *
 *  @return @a TRUE if @c a equals @c b, else @a FALSE.
 **/
gboolean utils_str_equal(const gchar *a, const gchar *b)
{
	/* (taken from libexo from os-cillation) */
	if (a == NULL && b == NULL) return TRUE;
	else if (a == NULL || b == NULL) return FALSE;

	while (*a == *b++)
		if (*a++ == '\0')
			return TRUE;

	return FALSE;
}


/**
 *  Remove the extension from @c filename and return the result in a newly allocated string.
 *
 *  @param filename The filename to operate on.
 *
 *  @return A newly-allocated string, should be freed when no longer needed.
 **/
gchar *utils_remove_ext_from_filename(const gchar *filename)
{
	gchar *last_dot = strrchr(filename, '.');
	gchar *result;
	gint i;

	if (filename == NULL) return NULL;

	if (! last_dot) return g_strdup(filename);

	/* assumes extension is small, so extra bytes don't matter */
	result = g_malloc(strlen(filename));
	i = 0;
	while ((filename + i) != last_dot)
	{
		result[i] = filename[i];
		i++;
	}
	result[i] = 0;
	return result;
}


gchar utils_brace_opposite(gchar ch)
{
	switch (ch)
	{
		case '(': return ')';
		case ')': return '(';
		case '[': return ']';
		case ']': return '[';
		case '{': return '}';
		case '}': return '{';
		case '<': return '>';
		case '>': return '<';
		default: return '\0';
	}
}


gchar *utils_get_hostname(void)
{
#ifdef G_OS_WIN32
	return win32_get_hostname();
#elif defined(HAVE_GETHOSTNAME)
	gchar hostname[100];
	if (gethostname(hostname, sizeof(hostname)) == 0)
		return g_strdup(hostname);
#endif
	return g_strdup("localhost");
}


/* Checks whether the given file can be written. locale_filename is expected in locale encoding.
 * Returns 0 if it can be written, otherwise it returns errno */
gint utils_is_file_writeable(const gchar *locale_filename)
{
	gchar *file;
	gint ret;

	if (! g_file_test(locale_filename, G_FILE_TEST_EXISTS) &&
		! g_file_test(locale_filename, G_FILE_TEST_IS_DIR))
		/* get the file's directory to check for write permission if it doesn't yet exist */
		file = g_path_get_dirname(locale_filename);
	else
		file = g_strdup(locale_filename);

#ifdef G_OS_WIN32
	/* use _waccess on Windows, access() doesn't accept special characters */
	ret = win32_check_write_permission(file);
#else

	/* access set also errno to "FILE NOT FOUND" even if locale_filename is writeable, so use
	 * errno only when access() explicitly returns an error */
	if (access(file, R_OK | W_OK) != 0)
		ret = errno;
	else
		ret = 0;
#endif
	g_free(file);
	return ret;
}


/* Replaces all occurrences of needle in haystack with replacement.
 * Warning: haystack will be freed.
 * New code should use utils_string_replace_all() instead (freeing arguments
 * is unusual behaviour).
 * All strings have to be NULL-terminated.
 * See utils_string_replace_all() for details. */
gchar *utils_str_replace(gchar *haystack, const gchar *needle, const gchar *replacement)
{
	GString *str;

	if (haystack == NULL)
		return NULL;

	str = g_string_new(haystack);

	g_free(haystack);
	utils_string_replace_all(str, needle, replacement);

	return g_string_free(str, FALSE);
}


gint utils_strpos(const gchar *haystack, const gchar *needle)
{
	gint haystack_length = strlen(haystack);
	gint needle_length = strlen(needle);
	gint i, j, pos = -1;

	if (needle_length > haystack_length)
	{
		return -1;
	}
	else
	{
		for (i = 0; (i < haystack_length) && pos == -1; i++)
		{
			if (haystack[i] == needle[0] && needle_length == 1)	return i;
			else if (haystack[i] == needle[0])
			{
				for (j = 1; (j < needle_length); j++)
				{
					if (haystack[i+j] == needle[j])
					{
						if (pos == -1) pos = i;
					}
					else
					{
						pos = -1;
						break;
					}
				}
			}
		}
		return pos;
	}
}


/**
 *  This is a convenience function to retrieve a formatted date/time string from strftime().
 *  This function should be preferred to directly calling strftime() since this function
 *  works on UTF-8 encoded strings.
 *
 *  @param format The format string to pass to strftime(3). See the strftime(3)
 *                documentation for details, in UTF-8 encoding.
 *  @param time_to_use The date/time to use, in time_t format or NULL to use the current time.
 *
 *  @return A newly-allocated string, should be freed when no longer needed.
 **/
gchar *utils_get_date_time(const gchar *format, time_t *time_to_use)
{
	const struct tm *tm;
	static gchar date[1024];
	gchar *locale_format;
	gsize len;

	g_return_val_if_fail(format != NULL, NULL);

	locale_format = g_locale_from_utf8(format, -1, NULL, NULL, NULL);
	if (locale_format == NULL)
		return NULL;

	if (time_to_use != NULL)
		tm = localtime(time_to_use);
	else
	{
		time_t tp = time(NULL);
		tm = localtime(&tp);
	}

	len = strftime(date, 1024, locale_format, tm);
	g_free(locale_format);
	if (len == 0)
		return NULL;

	if (! g_utf8_validate(date, len, NULL))
		return g_locale_to_utf8(date, len, NULL, NULL, NULL);
	else
		return g_strdup(date);
}


gchar *utils_get_initials(const gchar *name)
{
	gint i = 1, j = 1;
	gchar *initials = g_malloc0(5);

	initials[0] = name[0];
	while (name[i] != '\0' && j < 4)
	{
		if (name[i] == ' ' && name[i + 1] != ' ')
		{
			initials[j++] = name[i + 1];
		}
		i++;
	}
	return initials;
}


/**
 *  Convenience function for g_key_file_get_integer() to add a default value argument.
 *
 *  @param config A GKeyFile object.
 *  @param section The group name to look in for the key.
 *  @param key The key to find.
 *  @param default_value The default value which will be returned when @c section or @c key
 *         don't exist.
 *
 *  @return The value associated with c key as an integer, or the given default value if the value
 *          could not be retrieved.
 **/
gint utils_get_setting_integer(GKeyFile *config, const gchar *section, const gchar *key,
							   const gint default_value)
{
	gint tmp;
	GError *error = NULL;

	if (config == NULL) return default_value;

	tmp = g_key_file_get_integer(config, section, key, &error);
	if (error)
	{
		g_error_free(error);
		return default_value;
	}
	return tmp;
}


/**
 *  Convenience function for g_key_file_get_boolean() to add a default value argument.
 *
 *  @param config A GKeyFile object.
 *  @param section The group name to look in for the key.
 *  @param key The key to find.
 *  @param default_value The default value which will be returned when @c section or @c key
 *         don't exist.
 *
 *  @return The value associated with c key as a boolean, or the given default value if the value
 *          could not be retrieved.
 **/
gboolean utils_get_setting_boolean(GKeyFile *config, const gchar *section, const gchar *key,
								   const gboolean default_value)
{
	gboolean tmp;
	GError *error = NULL;

	if (config == NULL) return default_value;

	tmp = g_key_file_get_boolean(config, section, key, &error);
	if (error)
	{
		g_error_free(error);
		return default_value;
	}
	return tmp;
}


/**
 *  Convenience function for g_key_file_get_string() to add a default value argument.
 *
 *  @param config A GKeyFile object.
 *  @param section The group name to look in for the key.
 *  @param key The key to find.
 *  @param default_value The default value which will be returned when @c section or @c key
 *         don't exist.
 *
 *  @return A newly allocated string, or the given default value if the value could not be
 *          retrieved.
 **/
gchar *utils_get_setting_string(GKeyFile *config, const gchar *section, const gchar *key,
								const gchar *default_value)
{
	gchar *tmp;
	GError *error = NULL;

	if (config == NULL) return g_strdup(default_value);

	tmp = g_key_file_get_string(config, section, key, &error);
	if (error)
	{
		g_error_free(error);
		return (gchar*) g_strdup(default_value);
	}
	return tmp;
}


gchar *utils_get_hex_from_color(GdkColor *color)
{
	gchar *buffer = g_malloc0(9);

	if (color == NULL) return NULL;

	g_snprintf(buffer, 8, "#%02X%02X%02X",
	      (guint) (utils_scale_round(color->red / 256, 255)),
	      (guint) (utils_scale_round(color->green / 256, 255)),
	      (guint) (utils_scale_round(color->blue / 256, 255)));

	return buffer;
}


/* Get directory from current file in the notebook.
 * Returns dir string that should be freed or NULL, depending on whether current file is valid.
 * Returned string is in UTF-8 encoding */
gchar *utils_get_current_file_dir_utf8(void)
{
	GeanyDocument *doc = document_get_current();

	if (doc != NULL)
	{
		/* get current filename */
		const gchar *cur_fname = doc->file_name;

		if (cur_fname != NULL)
		{
			/* get folder part from current filename */
			return g_path_get_dirname(cur_fname); /* returns "." if no path */
		}
	}

	return NULL; /* no file open */
}


/* very simple convenience function */
void utils_beep(void)
{
	if (prefs.beep_on_errors) gdk_beep();
}


/* taken from busybox, thanks */
gchar *utils_make_human_readable_str(guint64 size, gulong block_size,
									 gulong display_unit)
{
	/* The code will adjust for additional (appended) units. */
	static const gchar zero_and_units[] = { '0', 0, 'K', 'M', 'G', 'T' };
	static const gchar fmt[] = "%Lu %c%c";
	static const gchar fmt_tenths[] = "%Lu.%d %c%c";

	guint64 val;
	gint frac;
	const gchar *u;
	const gchar *f;

	u = zero_and_units;
	f = fmt;
	frac = 0;

	val = size * block_size;
	if (val == 0) return g_strdup(u);

	if (display_unit)
	{
		val += display_unit/2;	/* Deal with rounding. */
		val /= display_unit;	/* Don't combine with the line above!!! */
	}
	else
	{
		++u;
		while ((val >= KILOBYTE) && (u < zero_and_units + sizeof(zero_and_units) - 1))
		{
			f = fmt_tenths;
			++u;
			frac = ((((gint)(val % KILOBYTE)) * 10) + (KILOBYTE/2)) / KILOBYTE;
			val /= KILOBYTE;
		}
		if (frac >= 10)
		{		/* We need to round up here. */
			++val;
			frac = 0;
		}
	}

	/* If f==fmt then 'frac' and 'u' are ignored. */
	return g_strdup_printf(f, val, frac, *u, 'b');
}


 guint utils_get_value_of_hex(const gchar ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	else
		return 0;
}


/* utils_strtod() converts a string containing a hex colour ("0x00ff00") into an integer.
 * Basically, it is the same as strtod() would do, but it does not understand hex colour values,
 * before ANSI-C99. With with_route set, it takes strings of the format "#00ff00". */
gint utils_strtod(const gchar *source, gchar **end, gboolean with_route)
{
	guint red, green, blue, offset = 0;

	if (source == NULL)
		return -1;
	else if (with_route && (strlen(source) != 7 || source[0] != '#'))
		return -1;
	else if (! with_route && (strlen(source) != 8 || source[0] != '0' ||
		(source[1] != 'x' && source[1] != 'X')))
	{
		return -1;
	}

	/* offset is set to 1 when the string starts with 0x, otherwise it starts with #
	 * and we don't need to increase the index */
	if (! with_route)
		offset = 1;

	red = utils_get_value_of_hex(
					source[1 + offset]) * 16 + utils_get_value_of_hex(source[2 + offset]);
	green = utils_get_value_of_hex(
					source[3 + offset]) * 16 + utils_get_value_of_hex(source[4 + offset]);
	blue = utils_get_value_of_hex(
					source[5 + offset]) * 16 + utils_get_value_of_hex(source[6 + offset]);

	return (red | (green << 8) | (blue << 16));
}


/* Returns: newly allocated string with the current time formatted HH:MM:SS. */
gchar *utils_get_current_time_string(void)
{
	const time_t tp = time(NULL);
	const struct tm *tmval = localtime(&tp);
	gchar *result = g_malloc0(9);

	strftime(result, 9, "%H:%M:%S", tmval);
	result[8] = '\0';
	return result;
}


GIOChannel *utils_set_up_io_channel(
				gint fd, GIOCondition cond, gboolean nblock, GIOFunc func, gpointer data)
{
	GIOChannel *ioc;
	/*const gchar *encoding;*/

	#ifdef G_OS_WIN32
	ioc = g_io_channel_win32_new_fd(fd);
	#else
	ioc = g_io_channel_unix_new(fd);
	#endif

	if (nblock)
		g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);

	g_io_channel_set_encoding(ioc, NULL, NULL);
/*
	if (! g_get_charset(&encoding))
	{	// hope this works reliably
		GError *error = NULL;
		g_io_channel_set_encoding(ioc, encoding, &error);
		if (error)
		{
			geany_debug("%s: %s", __func__, error->message);
			g_error_free(error);
			return ioc;
		}
	}
*/
	/* "auto-close" ;-) */
	g_io_channel_set_close_on_unref(ioc, TRUE);

	g_io_add_watch(ioc, cond, func, data);
	g_io_channel_unref(ioc);

	return ioc;
}


gchar **utils_read_file_in_array(const gchar *filename)
{
	gchar **result = NULL;
	gchar *data;

	if (filename == NULL) return NULL;

	g_file_get_contents(filename, &data, NULL, NULL);

	if (data != NULL)
	{
		result = g_strsplit_set(data, "\r\n", -1);
	}

	return result;
}


/* Contributed by Stefan Oltmanns, thanks.
 * Replaces \\, \r, \n, \t and \uXXX by their real counterparts */
gboolean utils_str_replace_escape(gchar *string)
{
	gsize i, j;
	guint unicodechar;

	j = 0;
	for (i = 0; i < strlen(string); i++)
	{
		if (string[i]=='\\')
		{
			if (i++ >= strlen(string))
			{
				return FALSE;
			}
			switch (string[i])
			{
				case '\\':
					string[j] = '\\';
					break;
				case 'n':
					string[j] = '\n';
					break;
				case 'r':
					string[j] = '\r';
					break;
				case 't':
					string[j] = '\t';
					break;
#if 0
				case 'x': /* Warning: May produce illegal utf-8 string! */
					i += 2;
					if (i >= strlen(string))
					{
						return FALSE;
					}
					if (isdigit(string[i-1])) string[j] = string[i-1]-48;
					else if (isxdigit(string[i-1])) string[j] = tolower(string[i-1])-87;
					else return FALSE;
					string[j] <<= 4;
					if (isdigit(string[i])) string[j] |= string[i]-48;
					else if (isxdigit(string[i])) string[j] |= tolower(string[i])-87;
					else return FALSE;
					break;
#endif
				case 'u':
					i += 2;
					if (i >= strlen(string))
					{
						return FALSE;
					}
					if (isdigit(string[i-1])) unicodechar = string[i-1]-48;
					else if (isxdigit(string[i-1])) unicodechar = tolower(string[i-1])-87;
					else return FALSE;
					unicodechar <<= 4;
					if (isdigit(string[i])) unicodechar |= string[i]-48;
					else if (isxdigit(string[i])) unicodechar |= tolower(string[i])-87;
					else return FALSE;
					if (((i+2) < strlen(string)) && (isdigit(string[i+1]) || isxdigit(string[i+1]))
						&& (isdigit(string[i+2]) || isxdigit(string[i+2])))
					{
						i += 2;
						unicodechar <<= 8;
						if (isdigit(string[i-1])) unicodechar |= ((string[i-1]-48)<<4);
						else unicodechar |= ((tolower(string[i-1])-87) << 4);
						if (isdigit(string[i])) unicodechar |= string[i]-48;
						else unicodechar |= tolower(string[i])-87;
					}
					if (((i+2) < strlen(string)) && (isdigit(string[i+1]) || isxdigit(string[i+1]))
						&& (isdigit(string[i+2]) || isxdigit(string[i+2])))
					{
						i += 2;
						unicodechar <<= 8;
						if (isdigit(string[i-1])) unicodechar |= ((string[i-1]-48) << 4);
						else unicodechar |= ((tolower(string[i-1])-87) << 4);
						if (isdigit(string[i])) unicodechar |= string[i]-48;
						else unicodechar |= tolower(string[i])-87;
					}
					if(unicodechar < 0x80)
					{
						string[j] = unicodechar;
					}
					else if (unicodechar < 0x800)
					{
						string[j] = (unsigned char) ((unicodechar >> 6)| 0xC0);
						j++;
						string[j] = (unsigned char) ((unicodechar & 0x3F)| 0x80);
					}
					else if (unicodechar < 0x10000)
					{
						string[j] = (unsigned char) ((unicodechar >> 12) | 0xE0);
						j++;
						string[j] = (unsigned char) (((unicodechar >> 6) & 0x3F) | 0x80);
						j++;
						string[j] = (unsigned char) ((unicodechar & 0x3F) | 0x80);
					}
					else if (unicodechar < 0x110000) /* more chars are not allowed in unicode */
					{
						string[j] = (unsigned char) ((unicodechar >> 18) | 0xF0);
						j++;
						string[j] = (unsigned char) (((unicodechar >> 12) & 0x3F) | 0x80);
						j++;
						string[j] = (unsigned char) (((unicodechar >> 6) & 0x3F) | 0x80);
						j++;
						string[j] = (unsigned char) ((unicodechar & 0x3F) | 0x80);
					}
					else
					{
						return FALSE;
					}
					break;
				default:
					return FALSE;
			}
		}
		else
		{
			string[j] = string[i];
		}
		j++;
	}
	while (j < i)
	{
		string[j] = 0;
		j++;
	}
	return TRUE;
}


/* Wraps a string in place, replacing a space with a newline character.
 * wrapstart is the minimum position to start wrapping or -1 for default */
gboolean utils_wrap_string(gchar *string, gint wrapstart)
{
	gchar *pos, *linestart;
	gboolean ret = FALSE;

	if (wrapstart < 0) wrapstart = 80;

	for (pos = linestart = string; *pos != '\0'; pos++)
	{
		if (pos - linestart >= wrapstart && *pos == ' ')
		{
			*pos = '\n';
			linestart = pos;
			ret = TRUE;
		}
	}
	return ret;
}


/**
 *  Converts the given UTF-8 encoded string into locale encoding.
 *  On Windows platforms, it does nothing and instead it just returns a copy of the input string.
 *
 *  @param utf8_text UTF-8 encoded text.
 *
 *  @return The converted string in locale encoding, or a copy of the input string if conversion
 *    failed. Should be freed with g_free(). If @a utf8_text is @c NULL, @c NULL is returned.
 **/
gchar *utils_get_locale_from_utf8(const gchar *utf8_text)
{
#ifdef G_OS_WIN32
	/* just do nothing on Windows platforms, this ifdef is just to prevent unwanted conversions
	 * which would result in wrongly converted strings */
	return g_strdup(utf8_text);
#else
	gchar *locale_text;

	if (! utf8_text)
		return NULL;
	locale_text = g_locale_from_utf8(utf8_text, -1, NULL, NULL, NULL);
	if (locale_text == NULL)
		locale_text = g_strdup(utf8_text);
	return locale_text;
#endif
}


/**
 *  Converts the given string (in locale encoding) into UTF-8 encoding.
 *  On Windows platforms, it does nothing and instead it just returns a copy of the input string.
 *
 *  @param locale_text Text in locale encoding.
 *
 *  @return The converted string in UTF-8 encoding, or a copy of the input string if conversion
 *    failed. Should be freed with g_free(). If @a locale_text is @c NULL, @c NULL is returned.
 **/
gchar *utils_get_utf8_from_locale(const gchar *locale_text)
{
#ifdef G_OS_WIN32
	/* just do nothing on Windows platforms, this ifdef is just to prevent unwanted conversions
	 * which would result in wrongly converted strings */
	return g_strdup(locale_text);
#else
	gchar *utf8_text;

	if (! locale_text)
		return NULL;
	utf8_text = g_locale_to_utf8(locale_text, -1, NULL, NULL, NULL);
	if (utf8_text == NULL)
		utf8_text = g_strdup(locale_text);
	return utf8_text;
#endif
}


/* Pass pointers to free after arg_count.
 * The last argument must be NULL as an extra check that arg_count is correct. */
void utils_free_pointers(gsize arg_count, ...)
{
	va_list a;
	gsize i;
	gpointer ptr;

	va_start(a, arg_count);
	for (i = 0; i < arg_count; i++)
	{
		ptr = va_arg(a, gpointer);
		g_free(ptr);
	}
	ptr = va_arg(a, gpointer);
	if (ptr)
		g_warning("Wrong arg_count!");
	va_end(a);
}


/* Creates a string array deep copy of a series of non-NULL strings.
 * The first argument is nothing special.
 * The list must be ended with NULL.
 * If first is NULL, NULL is returned. */
gchar **utils_strv_new(const gchar *first, ...)
{
	gsize strvlen, i;
	va_list args;
	gchar *str;
	gchar **strv;

	if (first == NULL)
		return NULL;

	strvlen = 1;	/* for first argument */

    /* count other arguments */
    va_start(args, first);
    for (; va_arg(args, gchar*) != NULL; strvlen++);
	va_end(args);

	strv = g_new(gchar*, strvlen + 1);	/* +1 for NULL terminator */
	strv[0] = g_strdup(first);

    va_start(args, first);
    for (i = 1; str = va_arg(args, gchar*), str != NULL; i++)
    {
		strv[i] = g_strdup(str);
	}
	va_end(args);

	strv[i] = NULL;
	return strv;
}


#if ! GLIB_CHECK_VERSION(2, 8, 0)
/* Taken from GLib SVN, 2007-03-10 */
/**
 * g_mkdir_with_parents:
 * @pathname: a pathname in the GLib file name encoding
 * @mode: permissions to use for newly created directories
 *
 * Create a directory if it doesn't already exist. Create intermediate
 * parent directories as needed, too.
 *
 * Returns: 0 if the directory already exists, or was successfully
 * created. Returns -1 if an error occurred, with errno set.
 *
 * Since: 2.8
 */
int
g_mkdir_with_parents (const gchar *pathname,
		      int          mode)
{
  gchar *fn, *p;

  if (pathname == NULL || *pathname == '\0')
    {
      errno = EINVAL;
      return -1;
    }

  fn = g_strdup (pathname);

  if (g_path_is_absolute (fn))
    p = (gchar *) g_path_skip_root (fn);
  else
    p = fn;

  do
    {
      while (*p && !G_IS_DIR_SEPARATOR (*p))
	p++;

      if (!*p)
	p = NULL;
      else
	*p = '\0';

      if (!g_file_test (fn, G_FILE_TEST_EXISTS))
	{
	  if (g_mkdir (fn, mode) == -1)
	    {
	      int errno_save = errno;
	      g_free (fn);
	      errno = errno_save;
	      return -1;
	    }
	}
      else if (!g_file_test (fn, G_FILE_TEST_IS_DIR))
	{
	  g_free (fn);
	  errno = ENOTDIR;
	  return -1;
	}
      if (p)
	{
	  *p++ = G_DIR_SEPARATOR;
	  while (*p && G_IS_DIR_SEPARATOR (*p))
	    p++;
	}
    }
  while (p);

  g_free (fn);

  return 0;
}
#endif


/**
 *  Create a directory if it doesn't already exist.
 *  Create intermediate parent directories as needed, too.
 *  The permissions of the created directory are set 0700.
 *
 *  @param path The path of the directory to create, in locale encoding.
 *  @param create_parent_dirs Whether to create intermediate parent directories if necessary.
 *
 *  @return 0 if the directory was successfully created, otherwise the @c errno of the
 *          failed operation is returned.
 **/
gint utils_mkdir(const gchar *path, gboolean create_parent_dirs)
{
	gint mode = 0700;
	gint result;

	if (path == NULL || strlen(path) == 0)
		return EFAULT;

	result = (create_parent_dirs) ? g_mkdir_with_parents(path, mode) : g_mkdir(path, mode);
	if (result != 0)
		return errno;
	return 0;
}


/**
 *  Gets a sorted list of files from the specified directory.
 *  Locale encoding is expected for path and used for the file list. The list and the data
 *  in the list should be freed after use.
 *
 *  @param path The path of the directory to scan, in locale encoding.
 *  @param length The location to store the number of non-@a NULL data items in the list,
 *                unless @a NULL.
 *  @param error The is the location for storing a possible error, or @a NULL.
 *
 *  @return A newly allocated list or @a NULL if no files found. The list and its data should be
 *          freed when no longer needed.
 **/
GSList *utils_get_file_list(const gchar *path, guint *length, GError **error)
{
	GSList *list = NULL;
	guint len = 0;
	GDir *dir;

	if (error)
		*error = NULL;
	if (length)
		*length = 0;
	g_return_val_if_fail(path != NULL, NULL);

	dir = g_dir_open(path, 0, error);
	if (dir == NULL)
		return NULL;

	while (1)
	{
		const gchar *filename = g_dir_read_name(dir);
		if (filename == NULL) break;

		list = g_slist_insert_sorted(list, g_strdup(filename), (GCompareFunc) utils_str_casecmp);
		len++;
	}
	g_dir_close(dir);

	if (length)
		*length = len;
	return list;
}


/* returns TRUE if any letter in str is a capital, FALSE otherwise. Should be Unicode safe. */
gboolean utils_str_has_upper(const gchar *str)
{
	gunichar c;

	if (str == NULL || *str == '\0' || ! g_utf8_validate(str, -1, NULL))
		return FALSE;

	while (*str != '\0')
	{
		c = g_utf8_get_char(str);
		/* check only letters and stop once the first non-capital was found */
		if (g_unichar_isalpha(c) && g_unichar_isupper(c))
			return TRUE;
		/* FIXME don't write a const string */
		str = g_utf8_next_char(str);
	}
	return FALSE;
}


/**
 * Replaces all occurrences of @c needle in @c haystack with @c replace.
 * As of Geany 0.16, @a replace can match @a needle, so the following will work:
 * @code utils_string_replace_all(text, "\n", "\r\n"); @endcode
 *
 * @param haystack The input string to operate on. This string is modified in place.
 * @param needle The string which should be replaced.
 * @param replace The replacement for @c needle.
 *
 * @return @a TRUE if @c needle was found, else @a FALSE.
 **/
gboolean utils_string_replace_all(GString *haystack, const gchar *needle, const gchar *replace)
{
	const gchar *stack, *match;
	gssize pos = -1;

	if (haystack->len == 0)
		return FALSE;
	g_return_val_if_fail(NZV(needle), FALSE);

	stack = haystack->str;
	while (1)
	{
		match = strstr(stack, needle);
		if (match == NULL)
			break;
		else
		{
			pos = match - haystack->str;
			g_string_erase(haystack, pos, strlen(needle));

			/* make next search after removed matching text.
			 * (we have to be careful to only use haystack->str as its address may change) */
			stack = haystack->str + pos;

			if (replace)
			{
				g_string_insert(haystack, pos, replace);
				stack = haystack->str + pos + strlen(replace);	/* skip past replacement */
			}
		}
	}
	return (pos != -1);
}


/* Get project or default startup directory (if set), or NULL. */
const gchar *utils_get_default_dir_utf8(void)
{
	if (app->project && NZV(app->project->base_path))
	{
		return app->project->base_path;
	}

	if (NZV(prefs.default_open_path))
	{
		return prefs.default_open_path;
	}
	return NULL;
}


static gboolean check_error(GError **error)
{
	if (error != NULL && *error != NULL)
	{
		/* imitate the GLib warning */
		g_warning(
			"GError set over the top of a previous GError or uninitialized memory.\n"
			"This indicates a bug in someone's code. You must ensure an error is NULL "
			"before it's set.");
		/* after returning the code may segfault, but we don't care because we should
		 * make sure *error is NULL */
		return FALSE;
	}
	return TRUE;
}


/**
 *  This is a wrapper function for g_spawn_sync() and internally calls this function on Unix-like
 *  systems. On Win32 platforms, it uses the Windows API.
 *
 *  @param dir The child's current working directory, or @a NULL to inherit parent's.
 *  @param argv The child's argument vector.
 *  @param env The child's environment, or @a NULL to inherit parent's.
 *  @param flags Flags from GSpawnFlags.
 *  @param child_setup A function to run in the child just before exec().
 *  @param user_data The user data for child_setup.
 *  @param std_out The return location for child output.
 *  @param std_err The return location for child error messages.
 *  @param exit_status The child exit status, as returned by waitpid().
 *  @param error The return location for error or @a NULL.
 *
 *  @return @a TRUE on success, @a FALSE if an error was set.
 **/
gboolean utils_spawn_sync(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
						  GSpawnChildSetupFunc child_setup, gpointer user_data, gchar **std_out,
						  gchar **std_err, gint *exit_status, GError **error)
{
	gboolean result;

	if (! check_error(error))
		return FALSE;

	if (argv == NULL)
	{
		*error = g_error_new(G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "argv must not be NULL");
		return FALSE;
	}

	if (std_out)
		*std_out = NULL;

	if (std_err)
		*std_err = NULL;

#ifdef G_OS_WIN32
	result = win32_spawn(dir, argv, env, flags, std_out, std_err, exit_status, error);
#else
	result = g_spawn_sync(dir, argv, env, flags, NULL, NULL, std_out, std_err, exit_status, error);
#endif

	return result;
}


/**
 *  This is a wrapper function for g_spawn_async() and internally calls this function on Unix-like
 *  systems. On Win32 platforms, it uses the Windows API.
 *
 *  @param dir The child's current working directory, or @a NULL to inherit parent's.
 *  @param argv The child's argument vector.
 *  @param env The child's environment, or @a NULL to inherit parent's.
 *  @param flags Flags from GSpawnFlags.
 *  @param child_setup A function to run in the child just before exec().
 *  @param user_data The user data for child_setup.
 *  @param child_pid The return location for child process ID, or NULL.
 *  @param error The return location for error or @a NULL.
 *
 *  @return @a TRUE on success, @a FALSE if an error was set.
 **/
gboolean utils_spawn_async(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
						   GSpawnChildSetupFunc child_setup, gpointer user_data, GPid *child_pid,
						   GError **error)
{
	gboolean result;

	if (! check_error(error))
		return FALSE;

	if (argv == NULL)
	{
		*error = g_error_new(G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "argv must not be NULL");
		return FALSE;
	}

#ifdef G_OS_WIN32
	result = win32_spawn(dir, argv, env, flags, NULL, NULL, NULL, error);
#else
	result = g_spawn_async(dir, argv, env, flags, NULL, NULL, child_pid, error);
#endif
	return result;
}
