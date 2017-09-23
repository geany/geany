/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_TAG_H
#define TM_TAG_H

/* @file
 The TMTag structure and the associated functions are used to manipulate
 tags and arrays of tags. Normally, you should not create tags individually
 but through an external interface such as tm_source_file_parse(), which generates
 an array of tags for the given source file. Once the tag list is generated,
 you can do various operations such as:
 	-# Extract relevant tags using tm_tags_extract()
	-# Sort an array of tags using tm_tags_sort()
	-# Deduplicate an array of tags using tm_tags_dedup().

 An important thing to remember here is that the tags operations such as extraction,
 sorting and deduplication do not change the tag itself in any way, but rather,
 manipulate pointers to the tags structure. The tags themselves are owned by the
 TMSourceFile structure which created them during parsing. So, be careful, for example,
 while deduping the tags array of a source file directly, since this might lead to
 'dangling' tags whose pointers have been removed from the array. If you need to
 deduplicate, create a copy of the tag pointer array using tm_tags_extract().
*/

#include "tm_source_file.h"
#include "tm_parser.h"
#include <glib-object.h>

G_BEGIN_DECLS

/** Use the TM_TAG() macro to cast a pointer to (TMTag *) */
#define TM_TAG(tag) ((TMTag *) tag)

/**
 Tag Attributes. Note that some attributes are available to file
 pseudotags only. Attributes are useful for specifying as arguments
 to the builtin sort and dedup functions, and during printing or writing
 to file, since these functions can operate on the given set of attributes
 only. Tag attributes are bitmasks and can be 'OR'-ed bitwise to represent
 any combination (line TMTagType).
*/
typedef enum
{
	tm_tag_attr_none_t = 0, /**< Undefined */
	tm_tag_attr_name_t = 1, /**< Tag Name */
	tm_tag_attr_type_t = 2, /**< Tag Type */
	tm_tag_attr_file_t = 4, /**< File in which tag exists */
	tm_tag_attr_line_t = 8, /**< Line number of tag */
	tm_tag_attr_pos_t = 16, /**< Byte position of tag in the file (Obsolete) */
	tm_tag_attr_scope_t = 32, /**< Scope of the tag */
	tm_tag_attr_inheritance_t = 64, /**< Parent classes */
	tm_tag_attr_arglist_t = 128, /**< Argument list */
	tm_tag_attr_local_t = 256, /**< If it has local scope */
	tm_tag_attr_time_t = 512, /**< Modification time (File tag only) */
	tm_tag_attr_vartype_t = 1024, /**< Variable Type */
	tm_tag_attr_access_t = 2048, /**< Access type (public/protected/private) */
	tm_tag_attr_impl_t = 4096, /**< Implementation (e.g. virtual) */
	tm_tag_attr_lang_t = 8192, /**< Language (File tag only) */
	tm_tag_attr_inactive_t = 16384, /**< Inactive file (File tag only, obsolete) */
	tm_tag_attr_pointer_t = 32768, /**< Pointer type */
	tm_tag_attr_max_t = 65535 /**< Maximum value */
} TMTagAttrType;

/** Tag access type for C++/Java member functions and variables */
#define TAG_ACCESS_PUBLIC 'p' /**< Public member */
#define TAG_ACCESS_PROTECTED 'r' /**< Protected member */
#define TAG_ACCESS_PRIVATE 'v' /**< Private member */
#define TAG_ACCESS_FRIEND 'f' /**< Friend members/functions */
#define TAG_ACCESS_DEFAULT 'd' /**< Default access (Java) */
#define TAG_ACCESS_UNKNOWN 'x' /**< Unknown access type */

/** Tag implementation type for functions */
#define TAG_IMPL_VIRTUAL 'v' /**< Virtual implementation */
#define TAG_IMPL_UNKNOWN 'x' /**< Unknown implementation */

/**
 * The TMTag structure represents a single tag in the tag manager.
 **/
typedef struct TMTag
{
	char *name; /**< Name of tag */
	TMTagType type; /**< Tag Type */
	gint refcount; /* the reference count of the tag */
	
	/** These are tag attributes */
	TMSourceFile *file; /**< File in which the tag occurs; NULL for global tags */
	gulong line; /**< Line number of the tag */
	gboolean local; /**< Is the tag of local scope */
	guint pointerOrder;
	char *arglist; /**< Argument list (functions/prototypes/macros) */
	char *scope; /**< Scope of tag */
	char *inheritance; /**< Parent classes */
	char *var_type; /**< Variable type (maps to struct for typedefs) */
	char access; /**< Access type (public/protected/private/etc.) */
	char impl; /**< Implementation (e.g. virtual) */
	TMParserType lang; /* Programming language of the file */
} TMTag;

/* The GType for a TMTag */
#define TM_TYPE_TAG (tm_tag_get_type())

GType tm_tag_get_type(void) G_GNUC_CONST;

#ifdef GEANY_PRIVATE

TMTag *tm_tag_new(void);

void tm_tags_remove_file_tags(TMSourceFile *source_file, GPtrArray *tags_array);

GPtrArray *tm_tags_merge(GPtrArray *big_array, GPtrArray *small_array, 
	TMTagAttrType *sort_attributes, gboolean unref_duplicates);

void tm_tags_sort(GPtrArray *tags_array, TMTagAttrType *sort_attributes,
	gboolean dedup, gboolean unref_duplicates);

GPtrArray *tm_tags_extract(GPtrArray *tags_array, guint tag_types);

void tm_tags_prune(GPtrArray *tags_array);

void tm_tags_dedup(GPtrArray *tags_array, TMTagAttrType *sort_attributes, gboolean unref_duplicates);

TMTag **tm_tags_find(const GPtrArray *tags_array, const char *name,
		gboolean partial, guint * tagCount);

void tm_tags_array_free(GPtrArray *tags_array, gboolean free_all);

const TMTag *tm_get_current_tag(GPtrArray *file_tags, const gulong line, const TMTagType tag_types);

void tm_tag_unref(TMTag *tag);

TMTag *tm_tag_ref(TMTag *tag);

gboolean tm_tags_equal(const TMTag *a, const TMTag *b);

gboolean tm_tag_is_anon(const TMTag *tag);

#ifdef TM_DEBUG /* various debugging functions */

const char *tm_tag_type_name(const TMTag *tag);

TMTagType tm_tag_name_type(const char* tag_name);

void tm_tag_print(TMTag *tag, FILE *fp);

void tm_tags_array_print(GPtrArray *tags, FILE *fp);

gint tm_tag_scope_depth(const TMTag *t);

#endif /* TM_DEBUG */

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* TM_TAG_H */
