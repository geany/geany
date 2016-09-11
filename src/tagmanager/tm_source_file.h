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

#include "tm_parser.h"

G_BEGIN_DECLS

/* Casts a pointer to a pointer to a TMSourceFile structure */
#define TM_SOURCE_FILE(source_file) ((TMSourceFile *) source_file)

/* Evaluates to X is X is defined, else evaluates to Y */
#define FALLBACK(X,Y) (X)?(X):(Y)


/**
 * The TMSourceFile structure represents the source file and its tags in the tag manager.
 **/
typedef struct TMSourceFile
{
	TMParserType lang; /* Programming language used */
	char *file_name; /**< Full file name (inc. path) */
	char *short_name; /**< Just the name of the file (without the path) */
	GPtrArray *tags_array; /**< Sorted tag array obtained by parsing the object. @elementtype{TMTag} */
} TMSourceFile;

GType tm_source_file_get_type(void);

TMSourceFile *tm_source_file_new(const char *file_name, const char *name);

void tm_source_file_free(TMSourceFile *source_file);

gchar *tm_get_real_path(const gchar *file_name)
#ifndef GEANY_PRIVATE
G_DEPRECATED_FOR(utils_get_real_path)
#endif
;

#ifdef GEANY_PRIVATE

const gchar *tm_source_file_get_lang_name(TMParserType lang);

TMParserType tm_source_file_get_named_lang(const gchar *name);

gboolean tm_source_file_parse(TMSourceFile *source_file, guchar* text_buf, gsize buf_size,
	gboolean use_buffer);

GPtrArray *tm_source_file_read_tags_file(const gchar *tags_file, TMParserType mode);

gboolean tm_source_file_write_tags_file(const gchar *tags_file, GPtrArray *tags_array);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* TM_SOURCE_FILE_H */
