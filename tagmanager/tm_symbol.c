/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tm_symbol.h"

static GMemChunk *sym_mem_chunk = NULL;

#define SYM_NEW(T) {\
	if (!sym_mem_chunk) \
		sym_mem_chunk = g_mem_chunk_new("TMSymbol MemChunk", sizeof(TMSymbol), 1024 \
		  , G_ALLOC_AND_FREE); \
	(T) = g_chunk_new0(TMSymbol, sym_mem_chunk);}

#define SYM_FREE(T) g_mem_chunk_free(sym_mem_chunk, (T))

void tm_symbol_print(TMSymbol *sym, guint level)
{
	guint i;

	g_return_if_fail (sym != NULL);
	for (i=0; i < level; ++i)
		fputc('\t', stderr);
	fprintf(stderr, "%s\n", (sym->tag)?sym->tag->name:"Root");
	if (sym->info.children)
	{
		if (sym->tag && tm_tag_function_t == sym->tag->type)
			tm_tag_print(sym->info.equiv, stderr);
		else
		{
			for (i=0; i < sym->info.children->len; ++i)
				tm_symbol_print(TM_SYMBOL(sym->info.children->pdata[i])
				  , level + 1);
		}
	}
}

#define SYM_ORDER(T) (((tm_tag_class_t == (T)->type) || (tm_tag_struct_t ==\
	(T)->type))?1:(((tm_tag_enum_t == (T)->type) || (tm_tag_interface_t ==\
	(T)->type))?2:3))

/* Comparison function for sorting symbols alphabetically */
int tm_symbol_compare(const void *p1, const void *p2)
{
	TMSymbol *s1, *s2;

	if (!p1 && !p2)
		return 0;
	else if (!p2)
		return 1;
	else if (!p1)
		return -1;
	s1 = *(TMSymbol **) p1;
	s2 = *(TMSymbol **) p2;
	if (!s1 && !s2)
		return 0;
	else if (!s2)
		return 1;
	else if (!s1)
		return -1;
	if (!s1->tag && !s2->tag)
		return 0;
	else if (!s2->tag)
		return 1;
	else if (!s1->tag)
		return -1;
	return strcmp(s1->tag->name, s2->tag->name);
}

/* Need this custom compare function to generate a symbol tree
in a simgle pass from tag list */
int tm_symbol_tag_compare(const TMTag **t1, const TMTag **t2)
{
	gint s1, s2;

	if ((!t1 && !t2) || (!*t1 && !*t2))
		return 0;
	else if (!t1 || !*t1)
		return -1;
	else if (!t2 || !*t2)
		return 1;
	if ((tm_tag_file_t == (*t1)->type) && (tm_tag_file_t == (*t2)->type))
		return 0;
	else if (tm_tag_file_t == (*t1)->type)
		return -1;
	else if (tm_tag_file_t == (*t2)->type)
		return 1;

	/* Compare on depth of scope - less depth gets higher priortity */
	s1 = tm_tag_scope_depth(*t1);
	s2 = tm_tag_scope_depth(*t2);
	if (s1 != s2)
		return (s1 - s2);

	/* Compare of tag type using a symbol ordering routine */
	s1 = SYM_ORDER(*t1);
	s2 = SYM_ORDER(*t2);
	if (s1 != s2)
		return (s1 - s2);

	/* Compare names alphabetically */
	s1 = strcmp((*t1)->name, (*t2)->name);
	if (s1 != 0)
		return (s1);

	/* Compare scope alphabetically */
	s1 = strcmp(NVL((*t1)->atts.entry.scope, ""),
	  NVL((*t2)->atts.entry.scope, ""));
	if (s1 != 0)
		return s1;

	/* If none of them are function/prototype, they are effectively equal */
	if ((tm_tag_function_t != (*t1)->type) &&
	    (tm_tag_prototype_t != (*t1)->type)&&
	    (tm_tag_function_t != (*t1)->type) &&
	    (tm_tag_prototype_t != (*t1)->type))
		return 0;

	/* Whichever is not a function/prototype goes first */
	if ((tm_tag_function_t != (*t1)->type) &&
	    (tm_tag_prototype_t != (*t1)->type))
		return -1;
	if ((tm_tag_function_t != (*t2)->type) &&
	    (tm_tag_prototype_t != (*t2)->type))
		return 1;

	/* Compare the argument list */
	s1 = strcmp(NVL((*t1)->atts.entry.arglist, ""),
	  NVL((*t2)->atts.entry.arglist, ""));
	if (s1 != 0)
		return s1;

	/* Functions go before prototypes */
	if ((tm_tag_function_t == (*t1)->type) &&
	    (tm_tag_function_t != (*t2)->type))
		return -1;
	if ((tm_tag_function_t != (*t1)->type) &&
	    (tm_tag_function_t == (*t2)->type))
		return 1;
	
	/* Give up */
	return 0;
}

TMSymbol *tm_symbol_tree_new(GPtrArray *tags_array)
{
	TMSymbol *root = NULL;
	GPtrArray *tags;

#ifdef TM_DEBUG
	g_message("Building symbol tree..");
#endif

	if ((!tags_array) || (tags_array->len <= 0))
		return NULL;

#ifdef TM_DEBUG
	fprintf(stderr, "Dumping all tags..\n");
	tm_tags_array_print(tags_array, stderr);
#endif	
	tags = tm_tags_extract(tags_array, tm_tag_max_t);
#ifdef TM_DEBUG
	fprintf(stderr, "Dumping unordered tags..\n");
	tm_tags_array_print(tags, stderr);
#endif
	if (tags && (tags->len > 0))
	{
		guint i;
		int j;
		int max_parents = -1;
		TMTag *tag;
		TMSymbol *sym = NULL, *sym1;
		char *parent_name;
		char *scope_end;
		gboolean matched;
		int str_match;

		SYM_NEW(root);
		tm_tags_custom_sort(tags, (TMTagCompareFunc) tm_symbol_tag_compare
		  , FALSE);
#ifdef TM_DEBUG
		fprintf(stderr, "Dumping ordered tags..");
		tm_tags_array_print(tags, stderr);
		fprintf(stderr, "Rebuilding symbol table..\n");
#endif
		for (i=0; i < tags->len; ++i)
		{
			tag = TM_TAG(tags->pdata[i]);
			if (tm_tag_prototype_t == tag->type)
			{
				if (sym && (tm_tag_function_t == sym->tag->type) &&
				  (!sym->info.equiv) &&
				  (0 == strcmp(NVL(tag->atts.entry.scope, "")
							 , NVL(sym->tag->atts.entry.scope, ""))))
				{
					sym->info.equiv = tag;
					continue;
				}
			}
			if (max_parents < 0)
			{
				if (SYM_ORDER(tag) > 2)
				{
					max_parents = i;
					if (max_parents > 0)
						qsort(root->info.children->pdata, max_parents
						  , sizeof(gpointer), tm_symbol_compare);
				}
			}
			SYM_NEW(sym);
			sym->tag = tag;
			if ((max_parents <= 0) || (!tag->atts.entry.scope))
			{
				sym->parent = root;
				if (!root->info.children)
					root->info.children = g_ptr_array_new();
				g_ptr_array_add(root->info.children, sym);
			}
			else
			{
				parent_name = tag->atts.entry.scope;
				scope_end = strstr(tag->atts.entry.scope, "::");
				if (scope_end)
					*scope_end = '\0';
				matched = FALSE;
				if (('\0' != parent_name[0]) &&
				  (0 != strcmp(parent_name, "<anonymous>")))
				{
					for (j=0; j < max_parents; ++j)
					{
						sym1 = TM_SYMBOL(root->info.children->pdata[j]);
						str_match = strcmp(sym1->tag->name, parent_name);
						if (str_match == 0)
						{
							matched = TRUE;
							sym->parent = sym1;
							if (!sym1->info.children)
								sym1->info.children = g_ptr_array_new();
							g_ptr_array_add(sym1->info.children, sym);
							break;
						}
						else if (str_match > 0)
							break;
					}
				}
				if (!matched)
				{
					sym->parent = root;
					if (!root->info.children)
						root->info.children = g_ptr_array_new();
					g_ptr_array_add(root->info.children, sym);
				}
				if (scope_end)
					*scope_end = ':';
			}
		}
#ifdef TM_DEBUG
		fprintf(stderr, "Done.Dumping symbol tree..");
		tm_symbol_print(root, 0);
#endif
	}
	if (tags)
		g_ptr_array_free(tags, TRUE);

	return root;
}

static void tm_symbol_free(TMSymbol *sym)
{
	if (!sym)
		return;
	if ((!sym->tag) || ((tm_tag_function_t != sym->tag->type) &&
		    (tm_tag_prototype_t != sym->tag->type)))
	{
		if (sym->info.children)
		{
			guint i;
			for (i=0; i < sym->info.children->len; ++i)
				tm_symbol_free(TM_SYMBOL(sym->info.children->pdata[i]));
			g_ptr_array_free(sym->info.children, TRUE);
			sym->info.children = NULL;
		}
	}
	SYM_FREE(sym);
}

void tm_symbol_tree_free(gpointer root)
{
	if (root)
		tm_symbol_free(TM_SYMBOL(root));
}

TMSymbol *tm_symbol_tree_update(TMSymbol *root, GPtrArray *tags)
{
	if (root)
		tm_symbol_free(root);
	if ((tags) && (tags->len > 0))
		return tm_symbol_tree_new(tags);
	else
		return NULL;
}
