/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/


#ifndef TM_WORKSPACE_H
#define TM_WORKSPACE_H

#include <glib.h>

#include "tm_tag.h"

#ifdef __cplusplus
extern "C"
{
#endif


/*! The Tag Manager Workspace. This is a singleton object containing a list
 of individual source files. There is also a global tag list 
 which can be loaded or created. This contains global tags gleaned from 
 /usr/include, etc. and should be used for autocompletion, calltips, etc.
*/
typedef struct
{
    GPtrArray *global_tags; /*!< Global tags loaded at startup */
    GPtrArray *source_files; /*!< An array of TMSourceFile pointers */
	GPtrArray *tags_array; /*!< Sorted tags from all source files 
		(just pointers to source file tags, the tag objects are owned by the source files) */
} TMWorkspace;

/*! Adds a source file to the workspace.
 \param source_file The source file to add to the workspace.
 \return TRUE on success, FALSE on failure (e.g. object already exixts).
*/
gboolean tm_workspace_add_source_file(TMSourceFile *source_file);

/*! Removes a member object from the workspace if it exists.
 \param source_file Pointer to the source file to be removed.
 \param do_free Whether the source file is to be freed as well.
 \param update Whether to update workspace objects.
 \return TRUE on success, FALSE on failure (e.g. the source file does not exist).
*/
gboolean tm_workspace_remove_source_file(TMSourceFile *source_file, gboolean do_free, gboolean update);


#ifdef GEANY_PRIVATE

/* Since TMWorkspace is a singleton, you should not create multiple
 workspaces, but get a pointer to the workspace whenever required. The first
 time a pointer is requested, or a source file is added to the workspace,
 a workspace is created. Subsequent calls to the function will return the
 created workspace.
*/
const TMWorkspace *tm_get_workspace(void);


/* Loads the global tag list from the specified file. The global tag list should
 have been first created using tm_workspace_create_global_tags().
 \param tags_file The file containing global tags.
 \return TRUE on success, FALSE on failure.
 \sa tm_workspace_create_global_tags()
*/
gboolean tm_workspace_load_global_tags(const char *tags_file, gint mode);
/*gboolean tm_workspace_load_global_tags(const char *tags_file);*/

/* Creates a list of global tags. Ideally, this should be created once during
 installations so that all users can use the same file. Thsi is because a full
 scale global tag list can occupy several megabytes of disk space.
 \param pre_process The pre-processing command. This is executed via system(),
 so you can pass stuff like 'gcc -E -dD -P `gnome-config --cflags gnome`'.
 \param includes Include files to process. Wildcards such as '/usr/include/a*.h'
 are allowed.
 \param tags_file The file where the tags will be stored.
 \param lang The language to use for the tags file.
 \return TRUE on success, FALSE on failure.
*/
gboolean tm_workspace_create_global_tags(const char *pre_process, const char **includes,
    int includes_count, const char *tags_file, int lang);

/* Recreates the tag array of the workspace by collecting the tags of
 all member source files. You shouldn't have to call this directly since
 this is called automatically by tm_workspace_update().
*/
void tm_workspace_recreate_tags_array(void);

/* Calls tm_source_file_update() for all workspace member source files.
 Use if you want to globally refresh the workspace.
*/
void tm_workspace_update(void);

/* Dumps the workspace tree - useful for debugging */
void tm_workspace_dump(void);

/* Returns all matching tags found in the workspace.
 \param name The name of the tag to find.
 \param type The tag types to return (TMTagType). Can be a bitmask.
 \param attrs The attributes to sort and dedup on (0 terminated integer array).
 \param partial Whether partial match is allowed.
 \param lang Specifies the language(see the table in parsers.h) of the tags to be found,
             -1 for all
 \return Array of matching tags. Do not free() it since it is a static member.
*/
const GPtrArray *tm_workspace_find(const char *name, int type, TMTagAttrType *attrs
 , gboolean partial, langType lang);

/* Returns all matching tags found in the workspace.
 \param name The name of the tag to find.
 \param scope The scope name of the tag to find, or NULL.
 \param type The tag types to return (TMTagType). Can be a bitmask.
 \param attrs The attributes to sort and dedup on (0 terminated integer array).
 \param partial Whether partial match is allowed.
 \param lang Specifies the language(see the table in parsers.h) of the tags to be found,
             -1 for all
 \return Array of matching tags. Do not free() it since it is a static member.
*/
const GPtrArray *
tm_workspace_find_scoped (const char *name, const char *scope, gint type,
    TMTagAttrType *attrs, gboolean partial, langType lang, gboolean global_search);

/* Returns all matching members tags found in given struct/union/class name.
 \param name Name of the struct/union/class.
 \param file_tags A GPtrArray of edited file TMTag pointers (for search speedup, can be NULL).
 \return A GPtrArray of TMTag pointers to struct/union/class members */
const GPtrArray *tm_workspace_find_scope_members(const GPtrArray *file_tags,
                                                 const char *scope_name,
                                                 gboolean find_global,
                                                 gboolean no_definitions);

const GPtrArray *
tm_workspace_find_namespace_members (const GPtrArray * file_tags, const char *name,
                                     gboolean search_global);

/* Returns TMTag which "own" given line
 \param line Current line in edited file.
 \param file_tags A GPtrArray of edited file TMTag pointers.
 \param tag_types the tag types to include in the match
 \return TMTag pointers to owner tag. */
const TMTag *tm_get_current_tag(GPtrArray *file_tags, const gulong line, const guint tag_types);

/* Returns TMTag to function or method which "own" given line
 \param line Current line in edited file.
 \param file_tags A GPtrArray of edited file TMTag pointers.
 \return TMTag pointers to owner function. */
const TMTag *tm_get_current_function(GPtrArray *file_tags, const gulong line);

/* Returns a list of parent classes for the given class name
 \param name Name of the class
 \return A GPtrArray of TMTag pointers (includes the TMTag for the class) */
const GPtrArray *tm_workspace_get_parents(const gchar *name);

/* Frees the workspace structure and all child source files. Use only when
 exiting from the main program.
*/
void tm_workspace_free(void);


#endif /* GEANY_PRIVATE */

#ifdef __cplusplus
}
#endif

#endif /* TM_WORKSPACE_H */
