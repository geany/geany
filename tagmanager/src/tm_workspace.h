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


/** The Tag Manager Workspace. This is a singleton object containing a list
 of individual source files. There is also a global tag list 
 which can be loaded or created. This contains global tags gleaned from 
 /usr/include, etc. and should be used for autocompletion, calltips, etc.
*/
typedef struct
{
	GPtrArray *global_tags; /**< Global tags loaded at startup */
	GPtrArray *source_files; /**< An array of TMSourceFile pointers */
	GPtrArray *tags_array; /**< Sorted tags from all source files 
		(just pointers to source file tags, the tag objects are owned by the source files) */
	GPtrArray *typename_array; /* Typename tags for syntax highlighting (pointers owned by source files) */
} TMWorkspace;


void tm_workspace_add_source_file(TMSourceFile *source_file);

void tm_workspace_remove_source_file(TMSourceFile *source_file);

void tm_workspace_add_source_files(GPtrArray *source_files);

void tm_workspace_remove_source_files(GPtrArray *source_files);


#ifdef GEANY_PRIVATE

const TMWorkspace *tm_get_workspace(void);

gboolean tm_workspace_load_global_tags(const char *tags_file, gint mode);

gboolean tm_workspace_create_global_tags(const char *pre_process, const char **includes,
	int includes_count, const char *tags_file, int lang);

const GPtrArray *tm_workspace_find(const char *name, TMTagType type, TMTagAttrType *attrs,
	gboolean partial, langType lang);

const GPtrArray *
tm_workspace_find_scoped (const char *name, const char *scope, TMTagType type,
	TMTagAttrType *attrs, gboolean partial, langType lang, gboolean global_search);

const GPtrArray *tm_workspace_find_scope_members(const GPtrArray *file_tags,
                                                 const char *scope_name,
                                                 gboolean find_global,
                                                 gboolean no_definitions);

void tm_workspace_add_source_file_noupdate(TMSourceFile *source_file);

void tm_workspace_update_source_file_buffer(TMSourceFile *source_file, guchar* text_buf,
	gsize buf_size);

void tm_workspace_free(void);


#ifdef TM_DEBUG
void tm_workspace_dump(void);
#endif /* TM_DEBUG */


#endif /* GEANY_PRIVATE */

#ifdef __cplusplus
}
#endif

#endif /* TM_WORKSPACE_H */
