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

/*! \file
 The TMTag structure and the associated functions are used to manipulate
 tags and arrays of tags. Normally, you should not create tags individually
 but through an external interface such as tm_source_file_parse(), which generates
 an array of tags for the given source file. Once the tag list is generated,
 you can do various operations such as:
 	-# Extract relevant tags using tm_tags_extract()
	-# Sort an array of tags using tm_tags_sort() or tm_tags_custom_sort()
	-# Deduplicate an array of tags using tm_tags_dedup() or tm_tags_dedup_custom().

 An important thing to remember here is that the tags operations such as extraction,
 sorting and deduplication do not change the tag itself in any way, but rather,
 manipulate pointers to the tags structure. The tags themselves are owned by the
 TMSourceFile structure which created them during parsing. So, be careful, for example,
 while deduping the tags array of a source file directly, since this might lead to
 'dangling' tags whose pointers have been removed from the array. If you need to
 deduplicate, create a copy of the tag pointer array using tm_tags_extract().
*/

#include "tm_source_file.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*! Use the TM_TAG() macro to cast a pointer to (TMTag *) */
#define TM_TAG(tag) ((TMTag *) tag)

/*!
 Types of tags. It is a bitmask so that multiple tag types can
 be used simultaneously by 'OR'-ing them bitwise.
 e.g. tm_tag_class_t | tm_tag_struct_t
*/
typedef enum
{
	tm_tag_undef_t = 0, /*!< Unknown type */
	tm_tag_class_t = 1, /*!< Class declaration */
	tm_tag_enum_t = 2, /*!< Enum declaration */
	tm_tag_enumerator_t = 4, /*!< Enumerator value */
	tm_tag_field_t = 8, /*!< Field (Java only) */
	tm_tag_function_t = 16, /*!< Function definition */
	tm_tag_interface_t = 32, /*!< Interface (Java only) */
	tm_tag_member_t = 64, /*!< Member variable of class/struct */
	tm_tag_method_t = 128, /*!< Class method (Java only) */
	tm_tag_namespace_t = 256, /*!< Namespace declaration */
	tm_tag_package_t = 512, /*!< Package (Java only) */
	tm_tag_prototype_t = 1024, /*!< Function prototype */
	tm_tag_struct_t = 2048, /*!< Struct declaration */
	tm_tag_typedef_t = 4096, /*!< Typedef */
	tm_tag_union_t = 8192, /*!< Union */
	tm_tag_variable_t = 16384, /*!< Variable */
	tm_tag_externvar_t = 32768, /*!< Extern or forward declaration */
	tm_tag_macro_t = 65536, /*!<  Macro (without arguments) */
	tm_tag_macro_with_arg_t = 131072, /*!< Parameterized macro */
	tm_tag_file_t = 262144, /*!< File (Pseudo tag) */
	tm_tag_other_t = 524288, /*!< Other (non C/C++/Java tag) */
	tm_tag_max_t = 1048575 /*!< Maximum value of TMTagType */
} TMTagType;

/*!
 Tag Attributes. Note that some attributes are available to file
 pseudotags only. Attributes are useful for specifying as arguments
 to the builtin sort and dedup functions, and during printing or writing
 to file, since these functions can operate on the given set of attributes
 only. Tag attributes are bitmasks and can be 'OR'-ed bitwise to represent
 any combination (line TMTagType).
*/
typedef enum
{
	tm_tag_attr_none_t = 0, /*!< Undefined */
	tm_tag_attr_name_t = 1, /*!< Tag Name */
	tm_tag_attr_type_t = 2, /*!< Tag Type */
	tm_tag_attr_file_t = 4, /*!< File in which tag exists */
	tm_tag_attr_line_t = 8, /*!< Line number of tag */
	tm_tag_attr_pos_t = 16, /*!< Byte position of tag in the file (Obsolete) */
	tm_tag_attr_scope_t = 32, /*!< Scope of the tag */
	tm_tag_attr_inheritance_t = 64, /*!< Parent classes */
	tm_tag_attr_arglist_t = 128, /*!< Argument list */
	tm_tag_attr_local_t = 256, /*!< If it has local scope */
	tm_tag_attr_time_t = 512, /*!< Modification time (File tag only) */
	tm_tag_attr_vartype_t = 1024, /*!< Variable Type */
	tm_tag_attr_access_t = 2048, /*!< Access type (public/protected/private) */
	tm_tag_attr_impl_t = 4096, /*!< Implementation (e.g. virtual) */
	tm_tag_attr_lang_t = 8192, /*!< Language (File tag only) */
	tm_tag_attr_inactive_t = 16384, /*!< Inactive file (File tag only) */
	tm_tag_attr_max_t = 32767 /*!< Maximum value */
} TMTagAttrType;

/*! Tag access type for C++/Java member functions and variables */
#define TAG_ACCESS_PUBLIC 'p' /*!< Public member */
#define TAG_ACCESS_PROTECTED 'r' /*!< Protected member */
#define TAG_ACCESS_PRIVATE 'v' /*!< Private member */
#define TAG_ACCESS_FRIEND 'f' /*!< Friend members/functions */
#define TAG_ACCESS_DEFAULT 'd' /*!< Default access (Java) */
#define TAG_ACCESS_UNKNOWN 'x' /*!< Unknown access type */

/*! Tag implementation type for functions */
#define TAG_IMPL_VIRTUAL 'v' /*!< Virtual implementation */
#define TAG_IMPL_UNKNOWN 'x' /*!< Unknown implementation */

/*!
 This structure holds all information about a tag, including the file
 pseudo tag. It should always be created indirectly with one of the tag
 creation functions such as tm_source_file_parse() or tm_tag_new_from_file().
 Once created, they can be sorted, deduped, etc. using functions such as
 tm_tags_custom_sort(), tm_tags_sort(), tm_tags_dedup() and tm_tags_custom_dedup()
*/
typedef struct _TMTag
{
	char *name; /*!< Name of tag */
	TMTagType type; /*!< Tag Type */
	union
	{
		/*! These are *real* tag attributes */
		struct
		{
			TMSourceFile *file; /*!< File in which the tag occurs */
			gulong line; /*!< Line number of the tag */
			gboolean local; /*!< Is the tag of local scope */
			char *arglist; /*!< Argument list (functions/prototypes/macros) */
			char *scope; /*!< Scope of tag */
			char *inheritance; /*!< Parent classes */
			char *var_type; /*!< Variable type (maps to struct for typedefs) */
			char access; /*!< Access type (public/protected/private/etc.) */
			char impl; /*!< Implementation (e.g. virtual) */
		} entry;
		/*! These are pseudo tag attributes representing a file */
		struct
		{
			time_t timestamp; /*!< Time of parsing of the file */
			langType lang; /*!< Programming language of the file */
			gboolean inactive; /*!< Whether this file is to be parsed */
		} file;
	} atts;
} TMTag;

/*!
 Prototype for user-defined tag comparison function. This is the type
 of argument that needs to be passed to tm_tags_sort_custom() and
 tm_tags_dedup_custom(). The function should take two void pointers,
 cast them to (TMTag **) and return 0, 1 or -1 depending on whether the
 first tag is equal to, greater than or less than the second tag.
*/
typedef int (*TMTagCompareFunc) (const void *ptr1, const void *ptr2);

/*!
 Initializes a TMTag structure with information from a tagEntryInfo struct
 used by the ctags parsers. Note that the TMTag structure must be malloc()ed
 before calling this function. This function is called by tm_tag_new() - you
 should not need to call this directly.
 \param tag The TMTag structure to initialize
 \param file Pointer to a TMSourceFile struct (it is assigned to the file member)
 \param tag_entry Tag information gathered by the ctags parser
 \return TRUE on success, FALSE on failure
*/
gboolean tm_tag_init(TMTag *tag, TMSourceFile *file, const tagEntryInfo *tag_entry);

/*!
 Initializes an already malloc()ed TMTag structure by reading a tag entry
 line from a file. The structure should be allocated beforehand.
 \param tag The TMTag structure to populate
 \param file The TMSourceFile struct (assigned to the file member)
 \param fp FILE pointer from where the tag line is read
 \return TRUE on success, FALSE on FAILURE
*/
gboolean tm_tag_init_from_file(TMTag *tag, TMSourceFile *file, FILE *fp);

/*!
 Creates a new tag structure from a tagEntryInfo pointer and a TMSOurceFile pointer
 and returns a pointer to it.
 \param file - Pointer to the TMSourceFile structure containing the tag
 \param tag_entry Contains tag information generated by ctags
 \return the new TMTag structure. This should be free()-ed using tm_tag_free()
*/
TMTag *tm_tag_new(TMSourceFile *file, const tagEntryInfo *tag_entry);

/*!
 Same as tm_tag_new() except that the tag attributes are read from file
*/
TMTag *tm_tag_new_from_file(TMSourceFile *file, FILE *fp);

/*!
 Writes tag information to the given FILE *.
 \param tag The tag information to write.
 \param file FILE pointer to which the tag information is written.
 \param attrs Attributes to be written (bitmask).
 \return TRUE on success, FALSE on failure.
*/
gboolean tm_tag_write(TMTag *tag, FILE *file, guint attrs);

/*!
 Inbuilt tag comparison function. Do not call directly since it needs some
 static variables to be set. Always use tm_tags_sort() and tm_tags_dedup()
 instead.
*/
int tm_tag_compare(const void *ptr1, const void *ptr2);

/*!
 Sort an array of tags on the specified attribuites using the inbuilt comparison
 function.
 \param tags_array The array of tags to be sorted
 \param sort_attributes Attributes to be sorted on (int array terminated by 0)
 \param dedup Whether to deduplicate the sorted array
 \return TRUE on success, FALSE on failure
*/
gboolean tm_tags_sort(GPtrArray *tags_array, TMTagAttrType *sort_attributes, gboolean dedup);

/*!
 This function should be used whenever more involved sorting is required. For this,
 you need to write a function as per the prototype of TMTagCompareFunc() and pass
 the function as a parameter to this function.
 \param tags_array Array of tags to be sorted
 \param compare_func A function which takes two pointers to (TMTag *)s and returns
 0, 1 or -1 depending on whether the first value is equal to, greater than or less that
 the second
 \param dedup Whether to deduplicate the sorted array. Note that the same comparison
 function will be used
 \return TRUE on success, FALSE on failure
*/
gboolean tm_tags_custom_sort(GPtrArray *tags_array, TMTagCompareFunc compare_func, gboolean dedup);

/*!
 This function will extract the tags of the specified types from an array of tags.
 The returned value is a GPtrArray which should be free-d with a call to
 g_ptr_array_free(array, TRUE). However, do not free the tags themselves since they
 are not duplicated.
 \param tags_array The original array of tags
 \param tag_types - The tag types to extract. Can be a bitmask. For example, passing
 (tm_tag_typedef_t | tm_tag_struct_t) will extract all typedefs and structures from
 the original array.
 \return an array of tags (NULL on failure)
*/
GPtrArray *tm_tags_extract(GPtrArray *tags_array, guint tag_types);

/*!
 Removes NULL tag entries from an array of tags. Called after tm_tags_dedup() and
 tm_tags_custom_dedup() since these functions substitute duplicate entries with NULL
 \param tags_array Array of tags to dedup
 \return TRUE on success, FALSE on failure
*/
gboolean tm_tags_prune(GPtrArray *tags_array);

/*!
 Deduplicates an array on tags using the inbuilt comparison function based on
 the attributes specified. Called by tm_tags_sort() when dedup is TRUE.
 \param tags_array Array of tags to dedup.
 \param sort_attributes Attributes the array is sorted on. They will be deduped
 on the same criteria.
 \return TRUE on success, FALSE on failure
*/
gboolean tm_tags_dedup(GPtrArray *tags_array, TMTagAttrType *sort_attributes);

/*!
 This is a more powerful form of tm_tags_dedup() since it can accomodate user
 defined comparison functions. Called by tm_tags_custom_sort() is dedup is TRUE.
 \param tags_array Array of tags to dedup.
 \compare_function Comparison function
 \return TRUE on success, FALSE on FAILURE
 \sa TMTagCompareFunc
*/
gboolean tm_tags_custom_dedup(GPtrArray *tags_array, TMTagCompareFunc compare_func);

/*!
 Returns a pointer to the position of the first matching tag in a sorted tags array.
 \param sorted_tags_array Tag array sorted on name
 \param name Name of the tag to locate.
 \param partial If TRUE, matches the first part of the name instead of doing exact match.
*/
TMTag **tm_tags_find(GPtrArray *sorted_tags_array, const char *name, gboolean partial, int * tagCount);

/*!
 Completely frees an array of tags.
 \param tags_array Array of tags to be freed.
 \param free_array Whether the GptrArray is to be freed as well.
*/
void tm_tags_array_free(GPtrArray *tags_array, gboolean free_all);

/*!
 Destroys a TMTag structure, i.e. frees all elements except the tag itself.
 \param tag The TMTag structure to destroy
 \sa tm_tag_free()
*/
void tm_tag_destroy(TMTag *tag);

/*!
 Destroys all data in the tag and frees the tag structure as well.
 \param tag Pointer to a TMTag structure
*/
void tm_tag_free(gpointer tag);

/*!
 Returns the type of tag as a string
 \param tag The tag whose type is required
*/
const char *tm_tag_type_name(const TMTag *tag);

/*!
 Returns the TMTagType given the name of the type. Reverse of tm_tag_type_name.
 \param tag_name Name of the tag type
*/
TMTagType tm_tag_name_type(const char* tag_name);

/*!
  Prints information about a tag to the given file pointer.
  \param tag The tag whose info is required.
  \fp The file pointer of teh file to print the info to.
*/
void tm_tag_print(TMTag *tag, FILE *fp);

/*!
  Prints info about all tags in the array to the given file pointer.
*/
void tm_tags_array_print(GPtrArray *tags, FILE *fp);

/*!
  Returns the depth of tag scope (useful for finding tag hierarchy
*/
gint tm_tag_scope_depth(const TMTag *t);

#ifdef __cplusplus
}
#endif

#endif /* TM_TAG_H */
