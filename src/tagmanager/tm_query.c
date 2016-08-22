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
	q->names = g_ptr_array_new();
	q->langs = g_array_new(FALSE, FALSE, sizeof(TMParserType));
	q->scopes = g_ptr_array_new();
	q->type = -1;
	q->refcount = 1;

	g_ptr_array_set_free_func(q->names, free_g_string);
	g_ptr_array_set_free_func(q->scopes, g_free);

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
gint tm_query_add_scope(TMQuery *q, const gchar *scope)
{
	g_ptr_array_add(q->scopes, g_strdup(scope));

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
 * @param type Bitmasp of the tag time to filter, see @a TMTagType.
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


/* tm_tag_compare() expects pointer to pointers to tags, because it's
 * used for GPtrArrays (therefore it's qsort-like). GQueue passes pointer
 * to tags. */
static gint tag_compare_ptr(gconstpointer a, gconstpointer b, gpointer data)
{
	return tm_tag_compare(&a, &b, data);
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
	TMTag **tags, *tag;
	gint ntags;
	guint i;
	GString *s;
	GQueue res = G_QUEUE_INIT;
	TMSortOptions sort_options;
	GList *node, *next;
	gchar *scope;
	TMParserType *lang;
	GPtrArray *ret;

	sort_options.sort_attrs = NULL;
	/* tags_array isn not needed by tm_tag_compare(), but for tm_search_cmp() */
	sort_options.tags_array = NULL;
	sort_options.first = TRUE;

	foreach_ptr_array(s, i, q->names)
	{
		TMTag **ptag;
		sort_options.cmp_len = s->len;
		if (q->data_sources & TM_QUERY_SOURCE_GLOBAL_TAGS)
		{
			tags = tm_tags_find(q->workspace->global_tags, s->str, s->len, &ntags);
			foreach_c_array(ptag, tags, ntags)
				g_queue_insert_sorted(&res, *ptag, tag_compare_ptr, &sort_options);
		}
		if (q->data_sources & TM_QUERY_SOURCE_SESSION_TAGS)
		{
			tags = tm_tags_find(q->workspace->tags_array, s->str, s->len, &ntags);
			foreach_c_array(ptag, tags, ntags)
				g_queue_insert_sorted(&res, *ptag, tag_compare_ptr, &sort_options);
		}
	}

	/* Filter tags according to scope, type and lang. */
	for (node = res.head; node; node = next)
	{
		gboolean match = TRUE;

		next = node->next;
		tag = node->data;
		foreach_ptr_array(scope, i, q->scopes)
			match = match && (g_strcmp0(tag->scope, scope) == 0);

		foreach_array(gint, lang, q->langs)
			match = match && tm_tag_langs_compatible(*lang, tag->lang);

		if (match && q->type >= 0)
			match = tag->type & q->type;

		/* Remove tag from the results. tags are unowned so removing is easy */
		if (!match)
			g_queue_unlink(&res, node);
	}

	/* Convert GQueue to GPtrArray for sort, dedup and return */
	i = 0;
	ret = g_ptr_array_sized_new(g_queue_get_length(&res));
	foreach_list(node, res.head)
	{
		tag = (TMTag *) node->data;
		g_ptr_array_insert(ret, i++, tag);
	}
	g_list_free(res.head);

	if (sort_attr)
		tm_tags_sort(ret, sort_attr, FALSE, FALSE);

	if (dedup_attr)
		tm_tags_dedup(ret, dedup_attr, FALSE);

	return ret;
}
