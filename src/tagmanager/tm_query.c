/*
*
*   Copyright (c) 2016 Thomas Martitz <kugel@rockbox.org>
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/


/**
 * @file tm_query.h
 * The TMQuery structure and methods to query tags from the global workspace.
 */

#include <glib.h>
#include <glib-object.h>

#include "utils.h"

#include "tm_workspace.h"
#include "tm_query.h"
#include "tm_tag.h"
#include "tm_parser.h"
#include "tm_source_file.h"


struct TMQuery
{
	const TMWorkspace *workspace;
	gint               data_sources;
	GPtrArray         *names;
	GPtrArray         *scopes;
	gint               type;
	GArray            *langs;
	gint               refcount;
};


static TMQuery *tm_query_dup(TMQuery *q)
{
	g_atomic_int_inc(&q->refcount);

	g_return_val_if_fail(NULL != q, NULL);

	return q;
}


/** Gets the GBoxed-derived GType for TMQuery
 *
 * @return TMQuery type. */
GEANY_API_SYMBOL
GType tm_query_get_type(void);

G_DEFINE_BOXED_TYPE(TMQuery, tm_query, tm_query_dup, tm_query_free);

static void free_g_string(gpointer data)
{
	g_string_free((GString *) data, TRUE);
}


/** Create a tag query
 *
 * The query can be used to retrieve tags from the tagmanager. You can
 * optionally add filters and execute the query with tm_query_exec().
 * Finally it must be freed with tm_query_free().
 *
 * @param workspace The workspace where the tags are stored
 * @param data_sources Bitmask of data sources. @see TMQuerySource
 *
 * @return The opaque query pointer.
 */
GEANY_API_SYMBOL
TMQuery *tm_query_new(const TMWorkspace *workspace, gint data_sources)
{
	TMQuery *q = g_slice_new(TMQuery);

	q->workspace = workspace;
	q->data_sources = data_sources;
	q->names = g_ptr_array_new_with_free_func(free_g_string);
	q->langs = g_array_new(FALSE, FALSE, sizeof(TMParserType));
	q->scopes = g_ptr_array_new_with_free_func(free_g_string);
	q->type = -1;
	q->refcount = 1;

	return q;
}


/** Decrements the reference count of @a q
 *
 * If the reference point drops to 0, then @a q is freed.
 *
 * @param q the query to free.
 */
GEANY_API_SYMBOL
void tm_query_free(TMQuery *q)
{
	if (NULL != q && g_atomic_int_dec_and_test(&q->refcount))
	{
		g_ptr_array_free(q->names, TRUE);
		g_ptr_array_free(q->scopes, TRUE);
		g_array_free(q->langs, TRUE);
		g_slice_free(TMQuery, q);
	}
}


/** Add name filter to a query
 *
 * The query results will be restricted to tags matching the name. The
 * matching is a simple prefix matching. The length to match can be
 * limited to allow multiple tags to match a prefix.
 *
 * @param q The query to operate on.
 * @param name The name to filter.
 * @param name_len The number of characters to use (from the beginning).
 *
 * @return 0 on succcess, < 0 on error.
 */
GEANY_API_SYMBOL
gint tm_query_add_name(TMQuery *q, const gchar *name, gssize name_len)
{
	GString *s;

	if (name_len < 0)
		s = g_string_new(name);
	else
		s = g_string_new_len(name, name_len);

	g_ptr_array_add(q->names, s);

	return 0;
}


/** Add scope filter to a query
 *
 * The query results will be restricted to tags that are subordenates of
 * a container scope matching @a scope, for example a C++ class.
 *
 * @param q The query to operate on.
 * @param scope The name of the container or parent scope.
 *
 * @return 0 on succcess, < 0 on error.
 */
GEANY_API_SYMBOL
gint tm_query_add_scope(TMQuery *q, const gchar *scope, gssize scope_len)
{
	GString *s;

	if (scope_len < 0)
		s = g_string_new(scope);
	else
		s = g_string_new_len(scope, scope_len);

	g_ptr_array_add(q->scopes, s);

	return 0;
}


/** Add language filter to a query
 *
 * The query results will be restricted to tags whose corresponding
 * @ref TMSourceFile is written in a specific programming language.
 *
 * @param q The query to operate on.
 * @param lang The programming language, see @a TMParserType.
 *
 * @return 0 on succcess, < 0 on error.
 */
GEANY_API_SYMBOL
gint tm_query_add_lang(TMQuery *q, TMParserType lang)
{
	g_array_append_val(q->langs, lang);

	return 0;
}


/** Set the tag type filter of a query.
 *
 * The query results will be restricted to tags match any of the given types.
 * You can only set this filter because the @a type is a bitmask containing
 * multiple values.
 *
 * @param q The query to operate on.
 * @param type Bitmask of the tag type to filter, see @a TMTagType.
 *
 * @return 0 on succcess, < 0 on error.
 */
GEANY_API_SYMBOL
gint tm_query_set_tag_type(TMQuery *q, TMTagType type)
{
	if (type > tm_tag_max_t)
		return -1;

	q->type = type;

	return 0;
}


/** Execute a query
 *
 * The query will be executed according to the applied filters. Filters
 * of the same type are combined using logical OR. Then, filters of different
 * types are combined using logcal AND.
 *
 * The execution is stateless. Filters can be added after execution and the
 * query can be re-run. Previous runs don't add side-effects.
 *
 * The results are generally sorted by tag name, even if @a sort_attr is NULL.
 * supplied. The results can be
 * sorted by @a sort_attr, and deduplicated by @a dedup_attr. Deduplication
 * happens after sorting. There is an additional limitation for deduplication:
 * Only adjacent tags can be deduplicated so they must be suitably ordered.
 * This can be achieved by using the same attributes for @a sort_attr and
 * @a dedup_attr.
 *
 * The results are stored in a @c GPtrArray. The individual tags are still
 * owned by the workspace but the container array must be freed with
 * g_ptr_array_free().
 *
 * @param q The query to execute
 * @param sort_attr @nullable @array{zero-terminated=1} Tag attribute to use
 *                   as sort key, or @c NULL to not sort.
 * @param dedup_attr @nullable @array{zero-terminated=1} Tag attribute to use
 *                    as deduplicaiton key, or @c NULL to not perform deduplicaiton.
 * @return @transfer{container} @elementtype{TMTag} Resulting tag array.
 */
GEANY_API_SYMBOL
GPtrArray *tm_query_exec(TMQuery *q, TMTagAttrType *sort_attr, TMTagAttrType *dedup_attr)
{
	TMTag *tag;
	guint i, ntags;
	GString *s;
	TMParserType *lang;
	GPtrArray *ret;
	TMTagAttrType def_sort_attr[] = { tm_tag_attr_name_t, 0 };

	ret = g_ptr_array_new();

	foreach_ptr_array(s, i, q->names)
	{
		TMTag **tags;
		if (q->data_sources & TM_QUERY_SOURCE_GLOBAL_TAGS)
		{
			tags = tm_tags_find(q->workspace->global_tags, s->str, s->len, &ntags);
			if (ntags)
			{
				g_ptr_array_set_size(ret, ret->len + ntags);
				memcpy(&ret->pdata[ret->len], *tags, ntags * sizeof(gpointer));
			}
		}
		if (q->data_sources & TM_QUERY_SOURCE_SESSION_TAGS)
		{
			tags = tm_tags_find(q->workspace->global_tags, s->str, s->len, &ntags);
			if (ntags)
			{
				g_ptr_array_set_size(ret, ret->len + ntags);
				memcpy(&ret->pdata[ret->len], *tags, ntags * sizeof(gpointer));
			}
		}
	}

	/* Filter tags according to scope, type and lang. */
	foreach_ptr_array(tag, i, ret)
	{
		gboolean match = TRUE;

		foreach_ptr_array(s, i, q->scopes)
			match = match && (strncmp(FALLBACK(tag->scope, ""), s->str, s->len) == 0);

		foreach_array(gint, lang, q->langs)
			match = match && tm_tag_langs_compatible(*lang, tag->lang);

		if (match && q->type >= 0)
			match = tag->type & q->type;

		/* Remove tag from the results. Can use the fast variant since order
		 * doesn't matter until after this loop. */
		if (!match)
			g_ptr_array_remove_index_fast(ret, i);
	}

	/* Always sort by name, unless wanted otherwise by the caller */
	if (!sort_attr)
		sort_attr = def_sort_attr;
	tm_tags_sort(ret, sort_attr, FALSE, FALSE);

	if (dedup_attr)
		tm_tags_dedup(ret, dedup_attr, FALSE);

	return ret;
}
