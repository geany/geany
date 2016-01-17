/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_SOURCE_FILE_H
#define TM_SOURCE_FILE_H

#include <stdio.h>
#include <glib.h>
#include <glib-object.h>

#ifndef LIBCTAGS_DEFINED
typedef int langType;
typedef void tagEntryInfo;
#endif

#if !defined(tagEntryInfo)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* Casts a pointer to a pointer to a TMSourceFile structure */
#define TM_SOURCE_FILE(source_file) ((TMSourceFile *) source_file)

/* Evaluates to X is X is defined, else evaluates to Y */
#define FALLBACK(X,Y) (X)?(X):(Y)


/**
 The TMSourceFile structure represents the source file and its tags in the tag manager.
*/
typedef struct
{
	langType lang; /**< Programming language used */
	char *file_name; /**< Full file name (inc. path) */
	char *short_name; /**< Just the name of the file (without the path) */
	GPtrArray *tags_array; /**< Sorted tag array obtained by parsing the object */
} TMSourceFile;

GType tm_source_file_get_type(void);

TMSourceFile *tm_source_file_new(const char *file_name, const char *name);

void tm_source_file_free(TMSourceFile *source_file);

gchar *tm_get_real_path(const gchar *file_name);


#ifdef GEANY_PRIVATE

const gchar *tm_source_file_get_lang_name(gint lang);

gint tm_source_file_get_named_lang(const gchar *name);

gboolean tm_source_file_parse(TMSourceFile *source_file, guchar* text_buf, gsize buf_size,
	gboolean use_buffer);

#endif /* GEANY_PRIVATE */

#ifdef __cplusplus
}
#endif

#endif /* TM_SOURCE_FILE_H */
