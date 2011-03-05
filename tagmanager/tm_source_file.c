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

#include "general.h"
#include "entry.h"
#include "parse.h"
#include "read.h"
#define LIBCTAGS_DEFINED
#include "tm_work_object.h"

#include "tm_source_file.h"
#include "tm_tag.h"


guint source_file_class_id = 0;
static TMSourceFile *current_source_file = NULL;

gboolean tm_source_file_init(TMSourceFile *source_file, const char *file_name
  , gboolean update, const char* name)
{
	if (0 == source_file_class_id)
		source_file_class_id = tm_work_object_register(tm_source_file_free
		  , tm_source_file_update, NULL);

#ifdef TM_DEBUG
	g_message("Source File init: %s", file_name);
#endif

	if (FALSE == tm_work_object_init(&(source_file->work_object),
		  source_file_class_id, file_name, FALSE))
		return FALSE;

	source_file->inactive = FALSE;
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

	if (update)
		tm_source_file_update(TM_WORK_OBJECT(source_file), TRUE, FALSE, FALSE);
	return TRUE;
}

TMWorkObject *tm_source_file_new(const char *file_name, gboolean update, const char *name)
{
	TMSourceFile *source_file = g_new(TMSourceFile, 1);
	if (TRUE != tm_source_file_init(source_file, file_name, update, name))
	{
		g_free(source_file);
		return NULL;
	}
	return (TMWorkObject *) source_file;
}

void tm_source_file_destroy(TMSourceFile *source_file)
{
#ifdef TM_DEBUG
	g_message("Destroying source file: %s", source_file->work_object.file_name);
#endif

	if (NULL != TM_WORK_OBJECT(source_file)->tags_array)
	{
		tm_tags_array_free(TM_WORK_OBJECT(source_file)->tags_array, TRUE);
		TM_WORK_OBJECT(source_file)->tags_array = NULL;
	}
	tm_work_object_destroy(&(source_file->work_object));
}

void tm_source_file_free(gpointer source_file)
{
	if (NULL != source_file)
	{
		tm_source_file_destroy(source_file);
		g_free(source_file);
	}
}

gboolean tm_source_file_parse(TMSourceFile *source_file)
{
	const char *file_name;
	gboolean status = TRUE;
	int passCount = 0;

	if ((NULL == source_file) || (NULL == source_file->work_object.file_name))
	{
		g_warning("Attempt to parse NULL file");
		return FALSE;
	}

	file_name = source_file->work_object.file_name;
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

	if (source_file->lang < 0 || ! LanguageTable [source_file->lang]->enabled)
		return status;

	while ((TRUE == status) && (passCount < 3))
	{
		if (source_file->work_object.tags_array)
			tm_tags_array_free(source_file->work_object.tags_array, FALSE);
		if (fileOpen (file_name, source_file->lang))
		{
			if (LanguageTable [source_file->lang]->parser != NULL)
			{
				LanguageTable [source_file->lang]->parser ();
				fileClose ();
				break;
			}
			else if (LanguageTable [source_file->lang]->parser2 != NULL)
				status = LanguageTable [source_file->lang]->parser2 (passCount);
			fileClose ();
		}
		else
		{
			g_warning("%s: Unable to open %s", G_STRFUNC, file_name);
			return FALSE;
		}
		++ passCount;
	}
	return status;
}

gboolean tm_source_file_buffer_parse(TMSourceFile *source_file, guchar* text_buf, gint buf_size)
{
	const char *file_name;
	gboolean status = TRUE;

	if ((NULL == source_file) || (NULL == source_file->work_object.file_name))
	{
		g_warning("Attempt to parse NULL file");
		return FALSE;
	}

	if ((NULL == text_buf) || (0 == buf_size))
	{
		g_warning("Attempt to parse a NULL text buffer");
	}

	file_name = source_file->work_object.file_name;
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
		int passCount = 0;
		while ((TRUE == status) && (passCount < 3))
		{
			if (source_file->work_object.tags_array)
				tm_tags_array_free(source_file->work_object.tags_array, FALSE);
			if (bufferOpen (text_buf, buf_size, file_name, source_file->lang))
			{
				if (LanguageTable [source_file->lang]->parser != NULL)
				{
					LanguageTable [source_file->lang]->parser ();
					bufferClose ();
					break;
				}
				else if (LanguageTable [source_file->lang]->parser2 != NULL)
					status = LanguageTable [source_file->lang]->parser2 (passCount);
				bufferClose ();
			}
			else
			{
				g_warning("Unable to open %s", file_name);
				return FALSE;
			}
			++ passCount;
		}
		return TRUE;
	}
	return status;
}

void tm_source_file_set_tag_arglist(const char *tag_name, const char *arglist)
{
	int count;
	TMTag **tags, *tag;

	if (NULL == arglist ||
		NULL == tag_name ||
		NULL == current_source_file ||
		NULL == current_source_file->work_object.tags_array)
	{
		return;
	}

	tags = tm_tags_find(current_source_file->work_object.tags_array, tag_name, FALSE, &count);
	if (tags != NULL && count == 1)
	{
		tag = tags[0];
		g_free(tag->atts.entry.arglist);
		tag->atts.entry.arglist = g_strdup(arglist);
	}
}

int tm_source_file_tags(const tagEntryInfo *tag)
{
	if (NULL == current_source_file)
		return 0;
	if (NULL == current_source_file->work_object.tags_array)
		current_source_file->work_object.tags_array = g_ptr_array_new();
	g_ptr_array_add(current_source_file->work_object.tags_array,
	  tm_tag_new(current_source_file, tag));
	return TRUE;
}

gboolean tm_source_file_update(TMWorkObject *source_file, gboolean force
  , gboolean __unused__ recurse, gboolean update_parent)
{
	if (force)
	{
		tm_source_file_parse(TM_SOURCE_FILE(source_file));
		tm_tags_sort(source_file->tags_array, NULL, FALSE);
		/* source_file->analyze_time = tm_get_file_timestamp(source_file->file_name); */
		if ((source_file->parent) && update_parent)
		{
			tm_work_object_update(source_file->parent, TRUE, FALSE, TRUE);
		}
		return TRUE;
	}
	else {
#ifdef	TM_DEBUG
		g_message ("no parsing of %s has been done", source_file->file_name);
#endif
		return FALSE;
	}
}


gboolean tm_source_file_buffer_update(TMWorkObject *source_file, guchar* text_buf,
			gint buf_size, gboolean update_parent)
{
#ifdef TM_DEBUG
	g_message("Buffer updating based on source file %s", source_file->file_name);
#endif

	tm_source_file_buffer_parse (TM_SOURCE_FILE(source_file), text_buf, buf_size);
	tm_tags_sort(source_file->tags_array, NULL, FALSE);
	/* source_file->analyze_time = time(NULL); */
	if ((source_file->parent) && update_parent)
	{
#ifdef TM_DEBUG
		g_message("Updating parent [project] from buffer..");
#endif
		tm_work_object_update(source_file->parent, TRUE, FALSE, TRUE);
	}
#ifdef TM_DEBUG
	else
		g_message("Skipping parent update because parent is %s and update_parent is %s"
		  , source_file->parent?"NOT NULL":"NULL", update_parent?"TRUE":"FALSE");

#endif
	return TRUE;
}


gboolean tm_source_file_write(TMWorkObject *source_file, FILE *fp, guint attrs)
{
	TMTag *tag;
	guint i;

	if (NULL != source_file)
	{
		if (NULL != (tag = tm_tag_new(TM_SOURCE_FILE(source_file), NULL)))
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

