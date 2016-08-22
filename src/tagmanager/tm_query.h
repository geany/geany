/*
*
*   Copyright (c) 2016 Thomas Martitz <kugel@rockbox.org>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_QUERY_H
#define TM_QUERY_H

#include "tm_workspace.h"
#include "tm_parser.h"
#include "tm_tag.h"

/* Opaque data type representing a tagmanager query */
typedef struct TMQuery TMQuery;

/* Possible data sources for a tagmanager query */
typedef enum {
	TM_QUERY_SOURCE_GLOBAL_TAGS,
	TM_QUERY_SOURCE_SESSION_TAGS,
} TMQuerySource;


TMQuery *tm_query_new(const TMWorkspace *workspace, gint data_sources);
void tm_query_free(TMQuery *q);

gint tm_query_add_name(TMQuery *q, const gchar *name, gssize name_len);
gint tm_query_add_scope(TMQuery *q, const gchar *scope);
gint tm_query_add_lang(TMQuery *q, TMParserType lang);
gint tm_query_set_tag_type(TMQuery *q, TMTagType type);

GPtrArray *tm_query_exec(TMQuery *q, TMTagAttrType *sort_attr, TMTagAttrType *dedup_attr);

#endif
