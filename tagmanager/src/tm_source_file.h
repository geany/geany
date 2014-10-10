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


/*!
 The TMSourceFile structure represents the source file and its tags in the tag manager.
*/
typedef struct
{
	langType lang; /*!< Programming language used */
	gboolean inactive; /*!< Whether this file should be scanned for tags */
	char *file_name; /*!< Full file name (inc. path) */
	char *short_name; /*!< Just the name of the file (without the path) */
	GPtrArray *tags_array; /*!< Tags obtained by parsing the object */
} TMSourceFile;

/*! Initializes a TMSourceFile structure and returns a pointer to it. 
 * \param file_name The file name.
 * \param update Update the tag array of the file.
 * \param name Name of the used programming language, NULL for autodetection.
 * \return The created TMSourceFile object.
 * */
TMSourceFile *tm_source_file_new(const char *file_name, gboolean update, const char *name);

/*! Updates the source file by reparsing. The tags array and
 the tags themselves are destroyed and re-created, hence any other tag arrays
 pointing to these tags should be rebuilt as well. All sorting information is
 also lost. The language parameter is automatically set the first time the file
 is parsed.
 \param source_file The source file to update.
 \param update_workspace If set to TRUE, sends an update signal to the workspace if required. You should
 always set this to TRUE if you are calling this function directly.
*/
void tm_source_file_update(TMSourceFile *source_file, gboolean update_workspace);

/*! Frees a TMSourceFile structure, including all contents */
void tm_source_file_free(TMSourceFile *source_file);

/*!
 Given a file name, returns a newly allocated string containing the realpath()
 of the file.
 \param file_name The original file_name
 \return A newly allocated string containing the real path to the file. NULL if none is available.
*/
gchar *tm_get_real_path(const gchar *file_name);


#ifdef GEANY_PRIVATE

/* Initializes a TMSourceFile structure from a file name. */
gboolean tm_source_file_init(TMSourceFile *source_file, const char *file_name,
							 gboolean update, const char *name);

/* Destroys the contents of the source file. Note that the tags are owned by the
 source file and are also destroyed when the source file is destroyed. If pointers
 to these tags are used elsewhere, then those tag arrays should be rebuilt.
*/
void tm_source_file_destroy(TMSourceFile *source_file);

/* Updates the source file by reparsing the text-buffer passed as parameter.
 Ctags will use a parsing based on buffer instead of on files.
 You should call this function when you don't want a previous saving of the file
 you're editing. It's useful for a "real-time" updating of the tags.
 The tags array and the tags themselves are destroyed and re-created, hence any
 other tag arrays pointing to these tags should be rebuilt as well. All sorting
 information is also lost. The language parameter is automatically set the first
 time the file is parsed.
 \param source_file The source file to update with a buffer.
 \param text_buf A text buffer. The user should take care of allocate and free it after
 the use here.
 \param buf_size The size of text_buf.
 \param update_workspace If set to TRUE, sends an update signal to the workspace if required. You should
 always set this to TRUE if you are calling this function directly.
 \return TRUE if the file was parsed, FALSE otherwise.
*/
void tm_source_file_buffer_update(TMSourceFile *source_file, guchar* text_buf,
			gint buf_size, gboolean update_workspace);

/* Parses the source file and regenarates the tags.
 \param source_file The source file to parse
 \return TRUE on success, FALSE on failure
 \sa tm_source_file_update()
*/
gboolean tm_source_file_parse(TMSourceFile *source_file);

/* Parses the text-buffer and regenarates the tags.
 \param source_file The source file to parse
 \param text_buf The text buffer to parse
 \param buf_size The size of text_buf.
 \return TRUE on success, FALSE on failure
 \sa tm_source_file_update()
*/
gboolean tm_source_file_buffer_parse(TMSourceFile *source_file, guchar* text_buf, gint buf_size);

/*
 This function is registered into the ctags parser when a file is parsed for
 the first time. The function is then called by the ctags parser each time
 it finds a new tag. You should not have to use this function.
 \sa tm_source_file_parse()
*/
int tm_source_file_tags(const tagEntryInfo *tag);

/*
 Writes all tags of a source file (including the file tag itself) to the passed
 file pointer.
 \param source_file The source file to write.
 \param fp The file pointer to write to.
 \param attrs The attributes to write.
 \return TRUE on success, FALSE on failure.
*/
gboolean tm_source_file_write(TMSourceFile *source_file, FILE *fp, guint attrs);


/* Gets the name associated with the language index.
 \param lang The language index.
 \return The language name, or NULL.
*/
const gchar *tm_source_file_get_lang_name(gint lang);

/* Gets the language index for \a name.
 \param name The language name.
 \return The language index, or -2.
*/
gint tm_source_file_get_named_lang(const gchar *name);

/* Set the argument list of tag identified by its name */
void tm_source_file_set_tag_arglist(const char *tag_name, const char *arglist);

#endif /* GEANY_PRIVATE */

#ifdef __cplusplus
}
#endif

#endif /* TM_SOURCE_FILE_H */
