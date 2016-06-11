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

G_BEGIN_DECLS


/** The Tag Manager Workspace. This is a singleton object containing a list
 * of individual source files. There is also a global tag list
 * which can be loaded or created. This contains global tags gleaned from
 * /usr/include, etc. and should be used for autocompletion, calltips, etc.
 **/
typedef struct TMWorkspace
{
	GPtrArray *global_tags; /**< Global tags loaded at startup. @elementtype{TMTag} */
	GPtrArray *source_files; /**< An array of TMSourceFile pointers. @elementtype{TMSourceFile} */
	GPtrArray *tags_array; /**< Sorted tags from all source files
		(just pointers to source file tags, the tag objects are owned by the source files). @elementtype{TMTag} */
	GPtrArray *typename_array; /* Typename tags for syntax highlighting (pointers owned by source files) */
	GPtrArray *global_typename_array; /* Like above for global tags */
} TMWorkspace;


void tm_workspace_add_source_file(TMSourceFile *source_file);

void tm_workspace_remove_source_file(TMSourceFile *source_file);

void tm_workspace_add_source_files(GPtrArray *source_files);

void tm_workspace_remove_source_files(GPtrArray *source_files);


#ifdef GEANY_PRIVATE

const TMWorkspace *tm_get_workspace(void);

gboolean tm_workspace_load_global_tags(const char *tags_file, TMParserType mode);

gboolean tm_workspace_create_global_tags(const char *pre_process, const char **includes,
	int includes_count, const char *tags_file, TMParserType lang);

GPtrArray *tm_workspace_find(const char *name, const char *scope, TMTagType type,
	TMTagAttrType *attrs, TMParserType lang);

GPtrArray *tm_workspace_find_prefix(const char *prefix, TMParserType lang, guint max_num);

GPtrArray *tm_workspace_find_scope_members (TMSourceFile *source_file, const char *name,
	gboolean function, gboolean member, const gchar *current_scope, gboolean search_namespace);


void tm_workspace_add_source_file_noupdate(TMSourceFile *source_file);

void tm_workspace_update_source_file_buffer(TMSourceFile *source_file, guchar* text_buf,
	gsize buf_size);

void tm_workspace_free(void);


#ifdef TM_DEBUG
void tm_workspace_dump(void);
#endif /* TM_DEBUG */


#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* TM_WORKSPACE_H */
