/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

/**
 * @file tm_source_file.h
 The TMSourceFile structure and associated functions are used to maintain
 tags for individual files.
*/


#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <glib/gstdio.h>
#ifdef G_OS_WIN32
# define VC_EXTRALEAN
# define WIN32_LEAN_AND_MEAN
# include <windows.h> /* for GetFullPathName */
#endif

#include "general.h"
#include "entry.h"
#include "parse.h"
#include "read.h"

#define LIBCTAGS_DEFINED
#include "tm_source_file.h"
#include "tm_tag.h"


static TMSourceFile *current_source_file = NULL;

static int get_path_max(const char *path)
{
#ifdef PATH_MAX
	return PATH_MAX;
#else
	int path_max = pathconf(path, _PC_PATH_MAX);
	if (path_max <= 0)
		path_max = 4096;
	return path_max;
#endif
}


#ifdef G_OS_WIN32
/* realpath implementation for Windows found at http://bugzilla.gnome.org/show_bug.cgi?id=342926
 * this one is better than e.g. liberty's lrealpath because this one uses Win32 API and works
 * with special chars within the filename */
static char *realpath (const char *pathname, char *resolved_path)
{
	int size;

	if (resolved_path != NULL)
	{
		int path_max = get_path_max(pathname);
		size = GetFullPathNameA (pathname, path_max, resolved_path, NULL);
		if (size > path_max)
			return NULL;
		else
			return resolved_path;
	}
	else
	{
		size = GetFullPathNameA (pathname, 0, NULL, NULL);
		resolved_path = g_new0 (char, size);
		GetFullPathNameA (pathname, size, resolved_path, NULL);
		return resolved_path;
	}
}
#endif

/**
 Given a file name, returns a newly allocated string containing the realpath()
 of the file.
 @param file_name The original file_name
 @return A newly allocated string containing the real path to the file. NULL if none is available.
*/
gchar *tm_get_real_path(const gchar *file_name)
{
	if (file_name)
	{
		gsize len = get_path_max(file_name) + 1;
		gchar *path = g_malloc0(len);

		if (realpath(file_name, path))
			return path;
		else
			g_free(path);
	}
	return NULL;
}

/*
 This function is registered into the ctags parser when a file is parsed for
 the first time. The function is then called by the ctags parser each time
 it finds a new tag. You should not have to use this function.
 @see tm_source_file_parse()
*/
static int tm_source_file_tags(const tagEntryInfo *tag)
{
	if (NULL == current_source_file)
		return 0;
	g_ptr_array_add(current_source_file->tags_array, 
		tm_tag_new(current_source_file, tag));
	return TRUE;
}

/* Set the argument list of tag identified by its name */
static void tm_source_file_set_tag_arglist(const char *tag_name, const char *arglist)
{
	guint count;
	TMTag **tags, *tag;

	if (NULL == arglist ||
		NULL == tag_name ||
		NULL == current_source_file)
	{
		return;
	}

	tags = tm_tags_find(current_source_file->tags_array, tag_name, FALSE, FALSE,
			&count);
	if (tags != NULL && count == 1)
	{
		tag = tags[0];
		g_free(tag->arglist);
		tag->arglist = g_strdup(arglist);
	}
}

/* Initializes a TMSourceFile structure from a file name. */
static gboolean tm_source_file_init(TMSourceFile *source_file, const char *file_name, 
	const char* name)
{
	struct stat s;
	int status;

#ifdef TM_DEBUG
	g_message("Source File init: %s", file_name);
#endif

	if (file_name != NULL)
	{
		status = g_stat(file_name, &s);
		if (0 != status)
		{
			/* g_warning("Unable to stat %s", file_name);*/
			return FALSE;
		}
		if (!S_ISREG(s.st_mode))
		{
			g_warning("%s: Not a regular file", file_name);
			return FALSE;
		}
		source_file->file_name = tm_get_real_path(file_name);
		source_file->short_name = strrchr(source_file->file_name, '/');
		if (source_file->short_name)
			++ source_file->short_name;
		else
			source_file->short_name = source_file->file_name;
	}

	source_file->tags_array = g_ptr_array_new();

	if (NULL == LanguageTable)
	{
		initializeParsing();
		installLanguageMapDefaults();
		if (NULL == TagEntryFunction)
			TagEntryFunction = tm_source_file_tags;
		if (NULL == TagEntrySetArglistFunction)
			TagEntrySetArglistFunction = tm_source_file_set_tag_arglist;
	}

	if (name == NULL)
		source_file->lang = LANG_AUTO;
	else
		source_file->lang = getNamedLanguage(name);

	return TRUE;
}

/** Initializes a TMSourceFile structure and returns a pointer to it. The
 * TMSourceFile has to be added to TMWorkspace to start its parsing.
 * @param file_name The file name.
 * @param name Name of the used programming language, NULL for autodetection.
 * @return The created unparsed TMSourceFile object.
 * */
TMSourceFile *tm_source_file_new(const char *file_name, const char *name)
{
	TMSourceFile *source_file = g_new(TMSourceFile, 1);
	if (TRUE != tm_source_file_init(source_file, file_name, name))
	{
		g_free(source_file);
		return NULL;
	}
	return source_file;
}

/* Destroys the contents of the source file. Note that the tags are owned by the
 source file and are also destroyed when the source file is destroyed. If pointers
 to these tags are used elsewhere, then those tag arrays should be rebuilt.
*/
static void tm_source_file_destroy(TMSourceFile *source_file)
{
#ifdef TM_DEBUG
	g_message("Destroying source file: %s", source_file->file_name);
#endif

	g_free(source_file->file_name);
	tm_tags_array_free(source_file->tags_array, TRUE);
	source_file->tags_array = NULL;
}

/** Frees a TMSourceFile structure, including all contents. Before calling this
 function the TMSourceFile has to be removed from the TMWorkspace. 
 @param source_file The source file to free.
*/
void tm_source_file_free(TMSourceFile *source_file)
{
	if (NULL != source_file)
	{
		tm_source_file_destroy(source_file);
		g_free(source_file);
	}
}

/* Parses the text-buffer or source file and regenarates the tags.
 @param source_file The source file to parse
 @param text_buf The text buffer to parse
 @param buf_size The size of text_buf.
 @param use_buffer Set FALSE to ignore the buffer and parse the file directly or
 TRUE to parse the buffer and ignore the file content.
 @return TRUE on success, FALSE on failure
*/
gboolean tm_source_file_parse(TMSourceFile *source_file, guchar* text_buf, gint buf_size,
	gboolean use_buffer)
{
	const char *file_name;
	gboolean retry = TRUE;

	if ((NULL == source_file) || (NULL == source_file->file_name))
	{
		g_warning("Attempt to parse NULL file");
		return FALSE;
	}
	
	file_name = source_file->file_name;
	
	if (!use_buffer)
	{
		if (!g_file_get_contents(file_name, (gchar**)&text_buf, (gsize*)&buf_size, NULL))
		{
			g_warning("Unable to open %s", file_name);
			return FALSE;
		}
	}

	if (NULL == text_buf || 0 == buf_size)
	{
		/* Empty buffer, "parse" by setting empty tag array */
		tm_tags_array_free(source_file->tags_array, FALSE);
		if (!use_buffer)
			g_free(text_buf);
		return TRUE;
	}

	if (NULL == LanguageTable)
	{
		initializeParsing();
		installLanguageMapDefaults();
		if (NULL == TagEntryFunction)
			TagEntryFunction = tm_source_file_tags;
		if (NULL == TagEntrySetArglistFunction)
			TagEntrySetArglistFunction = tm_source_file_set_tag_arglist;
	}
	current_source_file = source_file;
	if (LANG_AUTO == source_file->lang)
		source_file->lang = getFileLanguage (file_name);
	if (source_file->lang == LANG_IGNORE)
	{
#ifdef TM_DEBUG
		g_warning("ignoring %s (unknown language)\n", file_name);
#endif
	}
	else if (! LanguageTable [source_file->lang]->enabled)
	{
#ifdef TM_DEBUG
		g_warning("ignoring %s (language disabled)\n", file_name);
#endif
	}
	else
	{
		guint passCount = 0;
		while (retry && passCount < 3)
		{
			tm_tags_array_free(source_file->tags_array, FALSE);
			if (bufferOpen (text_buf, buf_size, file_name, source_file->lang))
			{
				if (LanguageTable [source_file->lang]->parser != NULL)
				{
					LanguageTable [source_file->lang]->parser ();
					bufferClose ();
					retry = FALSE;
					break;
				}
				else if (LanguageTable [source_file->lang]->parser2 != NULL)
					retry = LanguageTable [source_file->lang]->parser2 (passCount);
				bufferClose ();
			}
			else
			{
				g_warning("Unable to open %s", file_name);
				return FALSE;
			}
			++ passCount;
		}
	}
	
	if (!use_buffer)
		g_free(text_buf);
	return !retry;
}


/* Gets the name associated with the language index.
 @param lang The language index.
 @return The language name, or NULL.
*/
const gchar *tm_source_file_get_lang_name(gint lang)
{
	if (NULL == LanguageTable)
	{
		initializeParsing();
		installLanguageMapDefaults();
		if (NULL == TagEntryFunction)
			TagEntryFunction = tm_source_file_tags;
		if (NULL == TagEntrySetArglistFunction)
			TagEntrySetArglistFunction = tm_source_file_set_tag_arglist;
	}
	return getLanguageName(lang);
}

/* Gets the language index for \a name.
 @param name The language name.
 @return The language index, or -2.
*/
gint tm_source_file_get_named_lang(const gchar *name)
{
	if (NULL == LanguageTable)
	{
		initializeParsing();
		installLanguageMapDefaults();
		if (NULL == TagEntryFunction)
			TagEntryFunction = tm_source_file_tags;
		if (NULL == TagEntrySetArglistFunction)
			TagEntrySetArglistFunction = tm_source_file_set_tag_arglist;
	}
	return getNamedLanguage(name);
}

#if 0
/*
 Writes all tags of a source file (including the file tag itself) to the passed
 file pointer.
 @param source_file The source file to write.
 @param fp The file pointer to write to.
 @param attrs The attributes to write.
 @return TRUE on success, FALSE on failure.
*/
static gboolean tm_source_file_write(TMSourceFile *source_file, FILE *fp, guint attrs)
{
	TMTag *tag;
	guint i;

	if (NULL != source_file)
	{
		if (NULL != (tag = tm_tag_new(source_file, NULL)))
		{
			tm_tag_write(tag, fp, tm_tag_attr_max_t);
			tm_tag_unref(tag);
			if (NULL != source_file->tags_array)
			{
				for (i=0; i < source_file->tags_array->len; ++i)
				{
					tag = TM_TAG(source_file->tags_array->pdata[i]);
					if (TRUE != tm_tag_write(tag, fp, attrs))
						return FALSE;
				}
			}
		}
	}
	return TRUE;
}
#endif
