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

#include "tm_work_object.h"


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
#define TM_SOURCE_FILE(work_object) ((TMSourceFile *) work_object)

/* Checks whether the object is a TMSourceFile */
#define IS_TM_SOURCE_FILE(source_file) (((TMWorkObject *) (source_file))->type \
			== source_file_class_id)

/*!
 The TMSourceFile structure is derived from TMWorkObject and contains all it's
 attributes, plus an integer representing the language of the file.
*/
typedef struct
{
	TMWorkObject work_object; /*!< The base work object */
	langType lang; /*!< Programming language used */
	gboolean inactive; /*!< Whether this file should be scanned for tags */
} TMSourceFile;


/* Initializes a TMSourceFile structure from a file name. */
gboolean tm_source_file_init(TMSourceFile *source_file, const char *file_name,
							 gboolean update, const char *name);

/* Initializes a TMSourceFile structure and returns a pointer to it. */
TMWorkObject *tm_source_file_new(const char *file_name, gboolean update, const char *name);

/* Destroys the contents of the source file. Note that the tags are owned by the
 source file and are also destroyed when the source file is destroyed. If pointers
 to these tags are used elsewhere, then those tag arrays should be rebuilt.
*/
void tm_source_file_destroy(TMSourceFile *source_file);

/* Frees a TMSourceFile structure, including all contents */
void tm_source_file_free(gpointer source_file);

/*! Updates the source file by reparsing if the modification time is greater
 than the timestamp in the structure, or if force is TRUE. The tags array and
 the tags themselves are destroyed and re-created, hence any other tag arrays
 pointing to these tags should be rebuilt as well. All sorting information is
 also lost. The language parameter is automatically set the first time the file
 is parsed.
 \param source_file The source file to update.
 \param force Ignored. The source file is always updated.
 \param recurse This parameter is ignored for source files and is only there for consistency.
 \param update_parent If set to TRUE, sends an update signal to parent if required. You should
 always set this to TRUE if you are calling this function directly.
 \return TRUE if the file was parsed, FALSE otherwise.
 \sa tm_work_object_update(), tm_project_update(), tm_workspace_update()
*/
gboolean tm_source_file_update(TMWorkObject *source_file, gboolean force
  , gboolean recurse, gboolean update_parent);

/*! Updates the source file by reparsing the text-buffer passed as parameter.
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
 \param update_parent If set to TRUE, sends an update signal to parent if required. You should
 always set this to TRUE if you are calling this function directly.
 \return TRUE if the file was parsed, FALSE otherwise.
 \sa tm_work_object_update(), tm_project_update(), tm_workspace_update()
*/
gboolean tm_source_file_buffer_update(TMWorkObject *source_file, guchar* text_buf,
			gint buf_size, gboolean update_parent);

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
gboolean tm_source_file_write(TMWorkObject *source_file, FILE *fp, guint attrs);

/* Contains the id obtained by registering the TMSourceFile class as a child of
 TMWorkObject.
 \sa tm_work_object_register()
*/
extern guint source_file_class_id;

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

#ifdef __cplusplus
}
#endif

#endif /* TM_SOURCE_FILE_H */
