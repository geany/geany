/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_PROJECT_H
#define TM_PROJECT_H

#include <glib.h>
#include "tm_work_object.h"


/*! \file
 The TMProject structure and associated functions can be used to group together
 related source files in a "project". The update, open and save functions take
 care of automatically updating the project database whenever one or more
 files are changed. The following example demonstrates the use of TMProject.

 \include tm_project_test.c
*/

#ifdef __cplusplus
extern "C"
{
#endif

/*! Casts a pointer to a pointer to a TMProject structure */
#define TM_PROJECT(work_object) ((TMProject *) (work_object))

/*! Checks whether the object is a TMProject */
#define IS_TM_PROJECT(work_object) ((work_object)->type == project_class_id)

/*!
 This example demonstrates the use of TMProject and associated functions
 for managing tags for a group of related source files.

 \example tm_project_test.c
*/

/*!
 The TMProject structure is derived from TMWorkObject and contains all it's
 attributes, plus a project name and a list of source files constituting the project.
*/
typedef struct _TMProject
{
	TMWorkObject work_object; /*!< The parent work object */
	char *dir; /*!< Top project directory */
	const char **sources; /*!< Extensions for source files (wildcards, NULL terminated) */
	const char **ignore; /*!< File patters to ignore */
	GPtrArray *file_list; /*!< Array of TMSourceFile present in the project */
} TMProject;

/*! Initializes a TMProject structure from specified parameters
 \param project The TMProject structure to initialize.
 \param dir The top level directory of the project.
 \param sources The source files you are interested in (as wildcards).
 \param ignore The files you are not interested in (as wildcards).
 \param force Ignore cache (do full-scan of project directory)
 */
gboolean tm_project_init(TMProject *project, const char *dir
  , const char **sources, const char **ignore, gboolean force);

/*! Initializes a TMProject structure with the given parameters and
 returns a pointer to it. The function looks for a file called 'tm.tags'
 inside the top level directory to load the project. If such a file is not
 found, it assumes autoscan mode and imports all source files
 by recursively scanning the directory for Makefile.am and importing them.
 If top Makefile.am is missing as well, it simply imports all source files.
 \param dir The top level directory for the project.
 \param sources The list of source extensions. This should be a NULL terminated
 list of wildcards for the source types that you want to get displayed
 in the source tree. If the default list is acceptable, use NULL.
 \param ignore A NULL terminated list of wildcards for files to ignore
 \param force Ignore cache if present (treat as new project)
 \sa tm_project_init() , tm_project_autoscan()
*/
TMWorkObject *tm_project_new(const char *dir, const char **sources
  , const char **ignore, gboolean force);

/*! Destroys the contents of the project. Note that the tags are owned by the
 source files of the project, so they are also destroyed as each source file
 is deallocated using tm_source_file_free(). If the tags are to be used after
 the project has been destroyed, they should be deep-copied and any arrys
 containing pointers to them should be rebuilt. Destroying a project will
 automatically update and save teh project if required. You should not have
 to use this function since this is automatically called by tm_project_free().
 \param project The project to be destriyed.
*/
void tm_project_destroy(TMProject *project);

/*! Destroys the project contents by calling tm_project_destroy() and frees the
 memory allocated to the project structure.
 \sa tm_project_destroy()
*/
void tm_project_free(gpointer project);

/*! Opens a project by reading contents from the project database. The project
 should have been initialized prior to this using tm_project_new(). You should
 not have to use this since tm_project_new() will open the project if it already
 exists.
 \param project The project to open.
 \param force Whether the cache should be ignored.
 \return TRUE on success, FALSE on failure
*/
gboolean tm_project_open(TMProject *project, gboolean force);

/*! Saves the project in the project database file.
 \param project The project to save.
 \return TRUE on success, FALSE on failure.
*/
gboolean tm_project_save(TMProject *project);

/*! Adds a file to the project by creating a TMSourceFile from the file name
 and pushing it at the end of the project's file list.
 \param project The project to add the file to.
 \param file_name Full path of the file to be added to the project.
 \param update Whether to update tags image after addition.
 \return TRUE on success, FALSE on failure.
*/
gboolean tm_project_add_file(TMProject *project, const char *file_name
  , gboolean update);

/*! Finds a file in a project. If the file exists, returns a pointer to it,
 else returns NULL. This is the overloaded function TMFindFunc for TMProject.
 You should not have to call this function directly since this is automatically
 called by tm_work_object_find().
 \param project The project in which the file is to be searched.
 \param The name of the file to be searched.
 \param name_only Whether the comparison is to be only on name (not full path)
 \return Pointer the file (TMSourceFile) in the project. NULL if the file was not found.
*/
TMWorkObject *tm_project_find_file(TMWorkObject *work_object, const char *file_name
  , gboolean name_only);

/*! Destroys a member object and removes it from the project list.
 \param project The project from the file is to be removed.
 \param w The member work object (source file) to be removed
 \return TRUE on success, FALSE on failure
*/
gboolean tm_project_remove_object(TMProject *project, TMWorkObject *w);

/*! Removes only the project-tags associated with the object. Then resort the project's tags.
 \param project The project from which the file's tags are to be removed.
 \param w The member work object (source file) to remove the tags.
 \return TRUE on success, FALSE on failure
*/
gboolean tm_project_remove_tags_from_list(TMProject *project, TMWorkObject *w);

/*! Updates the project database and all the source files contained in the
 project. All sorting and deduping is lost and should be redone.
 \param work_object The project to update.
 \param force If set to TRUE, the cache is ignored.
 \param recurse If set to TRUE, checks all child objects, otherwise just recreates the
 tag array.
 \param update_parent If set to TRUE, sends an update signal to it's parent if required.
 If you are calling this function directly, you should always set this to TRUE.
 \return TRUE on success, FALSE on failure
*/
gboolean tm_project_update(TMWorkObject *work_object, gboolean force
  , gboolean recurse, gboolean update_parent);

/*! Syncs a project with the given list of file names, i.e., removes all files
** which are not in the list and adding the files which are in the list but not
** in the project.
\param project The project pointer
\param files - A list of file names relative to the top directory.
*/
gboolean tm_project_sync(TMProject *project, GList *files);

/*! Recreates the tags array of the project from the tags arrays of the contituent
 source files. Note that unlike TMSourceFile , the projects tags are not owned
 by the project but by the member source files. So, do not do a tm_tag_free() on
 a tag of the project's tag list
*/
void tm_project_recreate_tags_array(TMProject *project);

/*! Automatically imports all source files from the given directory
 into the project. This is done automatically if tm_project_new() is
 supplied with a directory name as parameter. Auto-scan will occur only
 if the directory is a valid top-level project directory, i.e, if the
 directory contains one of Makefile.am, Makefile.in or Makefile.
*/
gboolean tm_project_autoscan(TMProject *project);

/*! Dumps the current project structure - useful for debugging */
void tm_project_dump(const TMProject *p);

/*! Returns TRUE if the passed file is a source file as matched by the project
  source extensions (project->extn)
*/
gboolean tm_project_is_source_file(TMProject *project, const char *file_name);

/*! Contains the id obtained by registering the TMProject class as a child of
 TMWorkObject.
 \sa tm_work_object_register()
*/
extern guint project_class_id;

#ifdef __cplusplus
}
#endif

#endif /* TM_PROJECT_H */
