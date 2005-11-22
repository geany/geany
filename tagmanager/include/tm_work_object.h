/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_WORK_OBJECT_H
#define TM_WORK_OBJECT_H

#include <stdio.h>
#include <time.h>
#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*! Evaluates to X is X is defined, else evaluates to Y */
#define NVL(X,Y) (X)?(X):(Y)

/*! Macro to cast a pointer to (TMWorkObject *) */
#define TM_WORK_OBJECT(work_object) ((TMWorkObject *) work_object)

/*!
 A TMWorkObject structure is the base class for TMSourceFile and TMProject.
 This struct contains data common to all work objects, namely, a file name,
 time when the file was analyzed (for caching) and an array of tags which
 should be populated when the object is analyzed.
*/
typedef struct _TMWorkObject
{
	guint type; /*!< The type of object. Can be a source file or a project */
	char *file_name; /*!< Full file name (inc. path) of the work object */
	char *short_name; /*!< Just the name of the file (without the path) */
	struct _TMWorkObject *parent;
	time_t analyze_time; /*!< Time when the object was last analyzed */
	GPtrArray *tags_array; /*!< Tags obtained by parsing the object */
} TMWorkObject;

/*! Prototype of the update function required to be written by all classes
 derived from TMWorkObject. The function should take a pointer to the
 object and a flag indicating whether the cache should be ignored, and
 update the object's tag array accordingly.
 \sa tm_work_object_update(), tm_workspace_update(), tm_project_update(),
 tm_source_file_update().
*/
typedef gboolean (*TMUpdateFunc) (TMWorkObject *work_object, gboolean force
  , gboolean recurse, gboolean update_parent);

/*! Prototype of the find function required to be written by all classed
 derived from TMWorkObject. The function should take a pointer to the work
 object and a file name and return a pointer to the work object corresponding
 to the file name if the file is part of the object, and NULL otherwise.
 \sa tm_work_object_find()
*/
typedef TMWorkObject *(*TMFindFunc) (TMWorkObject *work_object, const char *file_name
  , gboolean name_only);

/*!
 Contains pointers to functions necessary to handle virtual function calls
 correctly. To create a new object derived from TMWorkObject, you
 need to write the three functions specified as the members of this class and
 register your class before first use using tm_work_object_register()
*/
typedef struct _TMWorkObjectClass
{
	GFreeFunc free_func; /*!< Function to free the derived object */
	TMUpdateFunc update_func; /*!< Function to update the derived object */
	TMFindFunc find_func; /*!< Function to locate contained work objects */
} TMWorkObjectClass;

#define TM_OBJECT_TYPE(work_object) ((TMWorkObject *) work_object)->type /*!< Type of the work object */
#define TM_OBJECT_FILE(work_object) ((TMWorkObject *) work_object)->file_name /*!< File name of the work object */
#define TM_OBJECT_TAGS(work_object) ((TMWorkObject *) work_object)->tags_array /*!< Tag array of the work object */

/*!
 Given a file name, returns a newly allocated string containing the realpath()
 of the file. However, unlike realpath, a reasonable guess is returned even if
 the file does not exist, which may often be the case
 \param file_name The original file_name
 \return A newly allocated string containing the real path to the file
*/
gchar *tm_get_real_path(const gchar *file_name);

/*!
 Initializes the work object structure. This function should be called by the
 initialization routine of the derived classes to ensure that the base members
 are initialized properly. The library user should not have to call this under
 any circumstance.
 \param work_object The work object to be initialized.
 \param type The type of the work object obtained by registering the derived class.
 \param file_name The name of the file corresponding to the work object.
 \param create Whether to create the file if it doesn't exist.
 \return TRUE on success, FALSE on failure.
 \sa tm_work_object_register()
*/
gboolean tm_work_object_init(TMWorkObject *work_object, guint type, const char *file_name
	  , gboolean create);

/*!
 Initializes a new TMWorkObject structure and returns a pointer to it. You souldn't
 have to call this function.
 \return NULL on failure
 \sa tm_source_file_new() , tm_project_new()
*/
TMWorkObject *tm_work_object_new(guint type, const char *file_name, gboolean create);

/*!
 Utility function - Given a file name, returns the timestamp of modification.
 \param file_name Full path to the file.
 \return Timestamp of the file's modification time. 0 on failure.
*/
time_t tm_get_file_timestamp(const char *file_name);

/*!
 Checks if the work object has been modified since the last update by comparing
 the timestamp stored in the work object structure with the modification time
 of the physical file.
 \param work_object Pointer to the work object.
 \return TRUE if the file has changed since last update, FALSE otherwise.
*/
gboolean tm_work_object_is_changed(TMWorkObject *work_object);

/*!
 Destroys a work object's data without freeing the structure itself. It should
 be called by the deallocator function of classes derived from TMWorkObject. The
 user shouldn't have to call this function.
*/
void tm_work_object_destroy(TMWorkObject *work_object);

/*!
 Deallocates a work object and it's component structures. The user can call this
 function directly since it will automatically call the correct deallocator function
 of the derived class if required.
 \param work_object Pointer to a work object or an object derived from it.
*/
void tm_work_object_free(gpointer work_object);

/*!
 This function should be called exactly once by all classes derived from TMWorkObject,
 since it supplies a unique ID on each call and stores the functions to call for
 updation and deallocation of objects of the type allocated. The user should not
 have to use this function unless he/she wants to create a new class derived from
 TMWorkObject.
 \param free_func The function to call to free the derived object.
 \param update_func The function to call to update the derived object.
 \return A unique ID for the derived class.
 \sa TMSourceFile , TMProject
*/
guint tm_work_object_register(GFreeFunc free_func, TMUpdateFunc update_func, TMFindFunc find_func);

/*!
 Writes the tags for the work object to the file specified.
 \param work_object The work object whose tags need to be written.
 \param file The file to which the tags are to be written.
 \param attrs The attributes to write (Can be a bitmask).
*/
void tm_work_object_write_tags(TMWorkObject *work_object, FILE *file, guint attrs);

/*!
 Updates the tags array if necessary. Automatically calls the update function
 of the type to which the object belongs.
 \param work_object Pointer to a work object or an object derived from it.
 \param force Whether the cache is to be ignored.
 \param recurse Whether to recurse into child work objects (for workspace and projects).
 \param update_parent If set to TRUE, calls the update function of the parent if required.
 If you are calling this function, you should set this to TRUE.
 \return TRUE on success, FALSE on failure.
 \sa tm_source_file_update() , tm_project_update()
*/
gboolean tm_work_object_update(TMWorkObject *work_object, gboolean force
  , gboolean recurse, gboolean update_parent);

/*!
 Finds the work object corresponding to the file name passed and returns a pointer
 to it. If not found, returns NULL. This is a virtual function which automatically
 calls the registered find function of teh derived object.
 \sa TMFindFunc
*/
TMWorkObject *tm_work_object_find(TMWorkObject *work_object, const char *file_name
  , gboolean name_only);

/*! Dumps the contents of a work object - useful for debugging */
void tm_work_object_dump(const TMWorkObject *w);

#ifdef __cplusplus
}
#endif

#endif /* TM_WORK_OBJECT_H */
