/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_FILE_ENTRY_H
#define TM_FILE_ENTRY_H

#include <glib.h>

/*! \file
The TMFileEntry structure and associated functions can be used
for file and directory traversal. The following example demonstrates
the use of TMFileEntry.
\include tm_file_tree_dump.c
*/

#ifdef __cplusplus
extern "C"
{
#endif

/*! Enum defining file types */
typedef enum
{
	tm_file_unknown_t, /*!< Unknown file type/file does not exist */
	tm_file_regular_t, /*!< Is a regular file */
	tm_file_dir_t, /*!< Is a directory */
	tm_file_link_t /*!< Is a symbolic link */
} TMFileType;

/*!
 This example demonstrates the use of TMFileEntry and associated functions
 for managing file hierarchies in a project.

 \example tm_file_tree_dump.c
*/

/*! This structure stores the file tree */
typedef struct _TMFileEntry
{
	TMFileType type; /*!< File type */
	char *path; /*!< Full path to the file (incl. dir and name) */
	char *name; /*!< Just the file name (path minus the directory) */
	char *version; /*!< CVS version in case there is a CVS entry for this file */
	struct _TMFileEntry *parent; /*!< The parent directory file entry */
	GSList *children; /*!< List of children (for directory) */
} TMFileEntry;

/*! Prototype for the function that gets called for each entry when
 tm_file_entry_foreach() is called.
*/
typedef void (*TMFileEntryFunc) (TMFileEntry *entry, gpointer user_data
  , guint level);

/*! Convinience casting macro */
#define TM_FILE_ENTRY(E) ((TMFileEntry *) (E))

/*! Function that compares two file entries on name and returns the
 difference
*/
gint tm_file_entry_compare(TMFileEntry *e1, TMFileEntry *e2);

/*! Function to create a new file entry structure.
\param path Path to the file for which the entry is to be created.
\param parent Should be NULL for the first call. Since the function calls
 itself recursively, this parameter is required to build the hierarchy.
\param recurse Whether the entry is to be recursively scanned (for
 directories only)
\param file_match List of file name patterns to match. If set to NULL,
 all files match. You can use wildcards like '*.c'. See the example program
 for usage.
\param file_unmatch Opposite of file_match. All files matching any of the patterns
 supplied are ignored. If set to NULL, no file is ignored.
\param dir_match List of directory name patterns to match. If set to NULL,
 all directories match. You can use wildcards like '\.*'.
\param dir_unmatch Opposite of dir_match. All directories matching any of the
 patterns supplied are ignored. If set to NULL, no directory is ignored.
\param ignore_hidden_files If set to TRUE, hidden files (starting with '.')
 are ignored.
\param ignore_hidden_dirs If set to TRUE, hidden directories (starting with '.')
 are ignored.
\return Populated TMFileEntry structure on success, NULL on failure.
*/
TMFileEntry *tm_file_entry_new(const char *path, TMFileEntry *parent
  , gboolean recurse, GList *file_match, GList *file_unmatch
  , GList *dir_match, GList *dir_unmatch, gboolean ignore_hidden_files
  , gboolean ignore_hidden_dirs);

/*! Frees a TMFileEntry structure. Freeing is recursive, so all child
 entries are freed as well.
\param entry The TMFileEntry structure to be freed.
*/
void tm_file_entry_free(gpointer entry);

/*! This will call the function func() for each file entry.
\param entry The root file entry.
\param func The function to be called.
\param user_data Extra information to be passed to the function.
\param level The recursion level. You should set this to 0 initially.
\param reverse If set to TRUE, traversal is in reverse hierarchical order
*/
void tm_file_entry_foreach(TMFileEntry *entry, TMFileEntryFunc func
  , gpointer user_data, guint level, gboolean reverse);

/*! This is a sample function to show the use of tm_file_entry_foreach().
*/
void tm_file_entry_print(TMFileEntry *entry, gpointer user_data, guint level);

/* Creates a list of path names from a TMFileEntry structure.
\param entry The TMFileEntry structure.
\files Current file list. Should be NULL.
*/
GList *tm_file_entry_list(TMFileEntry *entry, GList *files);

#ifdef __cplusplus
}
#endif

#endif /* TM_FILE_ENTRY_H */
