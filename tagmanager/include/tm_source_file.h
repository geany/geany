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

/*! \file
 The TMSourceFile structure and associated functions are used to maintain
 tags for individual files. See the test file tm_source_file_test.c included
 below for an example of how to use this structure and the related functions.

 \include tm_tag_print.c

*/

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

/*! Casts a pointer to a pointer to a TMSourceFile structure */
#define TM_SOURCE_FILE(work_object) ((TMSourceFile *) work_object)

/*! Checks whether the object is a TMSourceFile */
#define IS_TM_SOURCE_FILE(source_file) (((TMWorkObject *) (source_file))->type \
			== source_file_class_id)

/*!
 This example demonstrates the use of the TMSourceFile structure. When run,
 it outputs the tags encountered in the source files specified in the command line.

 \example tm_tag_print.c
*/

/*!
 The TMSourceFile structure is derived from TMWorkObject and contains all it's
 attributes, plus an integer representing the language of the file.
*/
typedef struct _TMSourceFile
{
	TMWorkObject work_object; /*!< The base work object */
	langType lang; /*!< Programming language used */
	gboolean inactive; /*!< Whether this file should be scanned for tags */
} TMSourceFile;

/*! Initializes a TMSourceFile structure from a file name. */
gboolean tm_source_file_init(TMSourceFile *source_file, const char *file_name, gboolean update);

/*! Initializes a TMSourceFile structure and returns a pointer to it. */
TMWorkObject *tm_source_file_new(const char *file_name, gboolean update);

/*! Destroys the contents of the source file. Note that the tags are owned by the
 source file and are also destroyed when the source file is destroyed. If pointers
 to these tags are used elsewhere, then those tag arrays should be rebuilt.
*/
void tm_source_file_destroy(TMSourceFile *source_file);

/*! Frees a TMSourceFile structure, including all contents */
void tm_source_file_free(gpointer source_file);

/*! Updates the source file by reparsing is the modification time is greater
 than the timestamp in the structure, or if force is TRUE. The tags array and
 the tags themselves are destroyed and re-created, hence any other tag arrays
 pointing to these tags should be rebuilt as well. All sorting information is
 also lost. The language parameter is automatically set the first time the file
 is parsed.
 \param source_file The source file to update.
 \param force Whether the cache is to be ignored.
 \param recurse This parameter is ignored for source files and is only there for consistency.
 \param update_parent If set to TRUE, sends an update signal to parent if required. You should
 always set this to TRUE if you are calling this function directly.
 \return TRUE if the file was parsed, FALSE otherwise.
 \sa tm_work_object_update(), tm_project_update(), tm_workspace_update()
*/
gboolean tm_source_file_update(TMWorkObject *source_file, gboolean force
  , gboolean recurse, gboolean update_parent);

/*! Parses the source file and regenarates the tags.
 \param source_file The source file to parse
 \return TRUE on success, FALSE on failure
 \sa tm_source_file_update()
*/
gboolean tm_source_file_parse(TMSourceFile *source_file);

/*!
 This function is registered into the ctags parser when a file is parsed for
 the first time. The function is then called by the ctags parser each time
 it finds a new tag. You should not have to use this function.
 \sa tm_source_file_parse()
*/
int tm_source_file_tags(const tagEntryInfo *tag);

/*!
 Writes all tags of a source file (including the file tag itself) to the passed
 file pointer.
 \param source_file The source file to write.
 \param fp The file pointer to write to.
 \attrs The attributes to write.
 \return TRUE on success, FALSE on failure.
*/
gboolean tm_source_file_write(TMWorkObject *source_file, FILE *fp, guint attrs);

/*! Contains the id obtained by registering the TMSourceFile class as a child of
 TMWorkObject.
 \sa tm_work_object_register()
*/
extern guint source_file_class_id;

#ifdef __cplusplus
}
#endif

#endif /* TM_SOURCE_FILE_H */
