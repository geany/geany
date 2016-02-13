/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#include <stdlib.h>
#include <string.h>
#include <glib-object.h>

#include "general.h"
#include "entry.h"
#include "parse.h"
#include "read.h"
#define LIBCTAGS_DEFINED
#include "tm_tag.h"
#include "tm_parser.h"


#define TAG_NEW(T)	((T) = g_slice_new0(TMTag))
#define TAG_FREE(T)	g_slice_free(TMTag, (T))


#ifdef DEBUG_TAG_REFS

static GHashTable *alive_tags = NULL;

static void foreach_tags_log(gpointer key, gpointer value, gpointer data)
{
	gsize *ref_count = data;
	const TMTag *tag = value;

	*ref_count += (gsize) tag->refcount;
	g_debug("Leaked TMTag (%d refs): %s", tag->refcount, tag->name);
}

static void log_refs_at_exit(void)
{
	gsize ref_count = 0;

	g_hash_table_foreach(alive_tags, foreach_tags_log, &ref_count);
	g_debug("TMTag references left at exit: %lu", ref_count);
}

static TMTag *log_tag_new(void)
{
	TMTag *tag;

	if (! alive_tags)
	{
		alive_tags = g_hash_table_new(g_direct_hash, g_direct_equal);
		atexit(log_refs_at_exit);
	}
	TAG_NEW(tag);
	g_hash_table_insert(alive_tags, tag, tag);

	return tag;
}

static void log_tag_free(TMTag *tag)
{
	g_return_if_fail(alive_tags != NULL);

	if (! g_hash_table_remove(alive_tags, tag)) {
		g_critical("Freeing invalid TMTag pointer %p", (void *) tag);
	} else {
		TAG_FREE(tag);
	}
}

#undef TAG_NEW
#undef TAG_FREE
#define TAG_NEW(T)	((T) = log_tag_new())
#define TAG_FREE(T)	log_tag_free(T)

#endif /* DEBUG_TAG_REFS */


const TMTagType TM_GLOBAL_TYPE_MASK =
	tm_tag_class_t | tm_tag_enum_t | tm_tag_interface_t |
	tm_tag_struct_t | tm_tag_typedef_t | tm_tag_union_t | tm_tag_namespace_t;


/* Note: To preserve binary compatibility, it is very important
	that you only *append* to this list ! */
enum
{
	TA_NAME = 200,
	TA_LINE,
	TA_LOCAL,
	TA_POS, /* Obsolete */
	TA_TYPE,
	TA_ARGLIST,
	TA_SCOPE,
	TA_VARTYPE,
	TA_INHERITS,
	TA_TIME,
	TA_ACCESS,
	TA_IMPL,
	TA_LANG,
	TA_INACTIVE, /* Obsolete */
	TA_POINTER
};

typedef struct
{
	guint *sort_attrs;
	gboolean partial;
	const GPtrArray *tags_array;
	gboolean first;
} TMSortOptions;

static const char *s_tag_type_names[] = {
	"class", /* classes */
	"enum", /* enumeration names */
	"enumerator", /* enumerators (values inside an enumeration) */
	"externvar", /* external variable declarations */
	"field", /* fields */
	"function", /*  function definitions */
	"interface", /* interfaces */
	"macro", /* macro definitions */
	"member", /* class, struct, and union members */
	"method", /* methods */
	"namespace", /* namespaces */
	"package", /* packages */
	"prototype", /* function prototypes */
	"struct", /* structure names */
	"typedef", /* typedefs */
	"union", /* union names */
	"variable", /* variable definitions */
	"other" /* Other tag type (non C/C++/Java) */
};

static TMTagType s_tag_types[] = {
	tm_tag_class_t,
	tm_tag_enum_t,
	tm_tag_enumerator_t,
	tm_tag_externvar_t,
	tm_tag_field_t,
	tm_tag_function_t,
	tm_tag_interface_t,
	tm_tag_macro_t,
	tm_tag_member_t,
	tm_tag_method_t,
	tm_tag_namespace_t,
	tm_tag_package_t,
	tm_tag_prototype_t,
	tm_tag_struct_t,
	tm_tag_typedef_t,
	tm_tag_union_t,
	tm_tag_variable_t,
	tm_tag_other_t
};

/* Gets the GType for a TMTag */
GType tm_tag_get_type(void)
{
	static GType gtype = 0;
	if (G_UNLIKELY (gtype == 0))
	{
		gtype = g_boxed_type_register_static("TMTag", (GBoxedCopyFunc)tm_tag_ref,
											 (GBoxedFreeFunc)tm_tag_unref);
	}
	return gtype;
}

static TMTagType get_tag_type(const char *tag_name)
{
	unsigned int i;
	int cmp;
	g_return_val_if_fail(tag_name, 0);
	for (i=0; i < sizeof(s_tag_type_names)/sizeof(char *); ++i)
	{
		cmp = strcmp(tag_name, s_tag_type_names[i]);
		if (0 == cmp)
			return s_tag_types[i];
		else if (cmp < 0)
			break;
	}
	/* other is not checked above as it is last, not sorted alphabetically */
	if (strcmp(tag_name, "other") == 0)
		return tm_tag_other_t;
#ifdef TM_DEBUG
	fprintf(stderr, "Unknown tag type %s\n", tag_name);
#endif
	return tm_tag_undef_t;
}

static char get_tag_impl(const char *impl)
{
	if ((0 == strcmp("virtual", impl))
	 || (0 == strcmp("pure virtual", impl)))
		return TAG_IMPL_VIRTUAL;

#ifdef TM_DEBUG
		g_warning("Unknown implementation %s", impl);
#endif
	return TAG_IMPL_UNKNOWN;
}

static char get_tag_access(const char *access)
{
	if (0 == strcmp("public", access))
		return TAG_ACCESS_PUBLIC;
	else if (0 == strcmp("protected", access))
		return TAG_ACCESS_PROTECTED;
	else if (0 == strcmp("private", access))
		return TAG_ACCESS_PRIVATE;
	else if (0 == strcmp("friend", access))
		return TAG_ACCESS_FRIEND;
	else if (0 == strcmp("default", access))
		return TAG_ACCESS_DEFAULT;

#ifdef TM_DEBUG
	g_warning("Unknown access type %s", access);
#endif
	return TAG_ACCESS_UNKNOWN;
}

/*
 Initializes a TMTag structure with information from a tagEntryInfo struct
 used by the ctags parsers. Note that the TMTag structure must be malloc()ed
 before calling this function. This function is called by tm_tag_new() - you
 should not need to call this directly.
 @param tag The TMTag structure to initialize
 @param file Pointer to a TMSourceFile struct (it is assigned to the file member)
 @param tag_entry Tag information gathered by the ctags parser
 @return TRUE on success, FALSE on failure
*/
static gboolean tm_tag_init(TMTag *tag, TMSourceFile *file, const tagEntryInfo *tag_entry)
{
	tag->refcount = 1;
	if (NULL == tag_entry)
		return FALSE;
		
	/* This is a normal tag entry */
	if (NULL == tag_entry->name)
		return FALSE;
	tag->name = g_strdup(tag_entry->name);
	tag->type = get_tag_type(tag_entry->kindName);
	tag->local = tag_entry->isFileScope;
	tag->pointerOrder = 0;	/* backward compatibility (use var_type instead) */
	tag->line = tag_entry->lineNumber;
	if (NULL != tag_entry->extensionFields.arglist)
		tag->arglist = g_strdup(tag_entry->extensionFields.arglist);
	if ((NULL != tag_entry->extensionFields.scope[1]) &&
		(0 != tag_entry->extensionFields.scope[1][0]))
		tag->scope = g_strdup(tag_entry->extensionFields.scope[1]);
	if (tag_entry->extensionFields.inheritance != NULL)
		tag->inheritance = g_strdup(tag_entry->extensionFields.inheritance);
	if (tag_entry->extensionFields.varType != NULL)
		tag->var_type = g_strdup(tag_entry->extensionFields.varType);
	if (tag_entry->extensionFields.access != NULL)
		tag->access = get_tag_access(tag_entry->extensionFields.access);
	if (tag_entry->extensionFields.implementation != NULL)
		tag->impl = get_tag_impl(tag_entry->extensionFields.implementation);
	if ((tm_tag_macro_t == tag->type) && (NULL != tag->arglist))
		tag->type = tm_tag_macro_with_arg_t;
	tag->file = file;
	tag->lang = file->lang;
	return TRUE;
}

/*
 Creates a new tag structure from a tagEntryInfo pointer and a TMSOurceFile pointer
 and returns a pointer to it.
 @param file - Pointer to the TMSourceFile structure containing the tag
 @param tag_entry Contains tag information generated by ctags
 @return the new TMTag structure. This should be free()-ed using tm_tag_free()
*/
TMTag *tm_tag_new(TMSourceFile *file, const tagEntryInfo *tag_entry)
{
	TMTag *tag;

	TAG_NEW(tag);
	if (FALSE == tm_tag_init(tag, file, tag_entry))
	{
		TAG_FREE(tag);
		return NULL;
	}
	return tag;
}

/*
 Initializes an already malloc()ed TMTag structure by reading a tag entry
 line from a file. The structure should be allocated beforehand.
 @param tag The TMTag structure to populate
 @param file The TMSourceFile struct (assigned to the file member)
 @param fp FILE pointer from where the tag line is read
 @return TRUE on success, FALSE on FAILURE
*/
static gboolean tm_tag_init_from_file(TMTag *tag, TMSourceFile *file, FILE *fp)
{
	guchar buf[BUFSIZ];
	guchar *start, *end;
	gboolean status;
	guchar changed_char = TA_NAME;

	tag->refcount = 1;
	if ((NULL == fgets((gchar*)buf, BUFSIZ, fp)) || ('\0' == *buf))
		return FALSE;
	for (start = end = buf, status = TRUE; (TRUE == status); start = end, ++ end)
	{
		while ((*end < TA_NAME) && (*end != '\0') && (*end != '\n'))
			++ end;
		if (('\0' == *end) || ('\n' == *end))
			status = FALSE;
		changed_char = *end;
		*end = '\0';
		if (NULL == tag->name)
		{
			if (!isprint(*start))
				return FALSE;
			else
				tag->name = g_strdup((gchar*)start);
		}
		else
		{
			switch (*start)
			{
				case TA_LINE:
					tag->line = atol((gchar*)start + 1);
					break;
				case TA_LOCAL:
					tag->local = atoi((gchar*)start + 1);
					break;
				case TA_TYPE:
					tag->type = (TMTagType) atoi((gchar*)start + 1);
					break;
				case TA_ARGLIST:
					tag->arglist = g_strdup((gchar*)start + 1);
					break;
				case TA_SCOPE:
					tag->scope = g_strdup((gchar*)start + 1);
					break;
				case TA_POINTER:
					tag->pointerOrder = atoi((gchar*)start + 1);
					break;
				case TA_VARTYPE:
					tag->var_type = g_strdup((gchar*)start + 1);
					break;
				case TA_INHERITS:
					tag->inheritance = g_strdup((gchar*)start + 1);
					break;
				case TA_TIME:  /* Obsolete */
					break;
				case TA_LANG:  /* Obsolete */
					break;
				case TA_INACTIVE:  /* Obsolete */
					break;
				case TA_ACCESS:
					tag->access = (char) *(start + 1);
					break;
				case TA_IMPL:
					tag->impl = (char) *(start + 1);
					break;
				default:
#ifdef GEANY_DEBUG
					g_warning("Unknown attribute %s", start + 1);
#endif
					break;
			}
		}
		*end = changed_char;
	}
	if (NULL == tag->name)
		return FALSE;
	tag->file = file;
	return TRUE;
}

/* alternative parser for Pascal and LaTeX global tags files with the following format
 * tagname|return value|arglist|description\n */
static gboolean tm_tag_init_from_file_alt(TMTag *tag, TMSourceFile *file, FILE *fp)
{
	guchar buf[BUFSIZ];
	guchar *start, *end;
	gboolean status;
	/*guchar changed_char = TA_NAME;*/

	tag->refcount = 1;
	if ((NULL == fgets((gchar*)buf, BUFSIZ, fp)) || ('\0' == *buf))
		return FALSE;
	{
		gchar **fields;
		guint field_len;
		for (start = end = buf, status = TRUE; (TRUE == status); start = end, ++ end)
		{
			while ((*end < TA_NAME) && (*end != '\0') && (*end != '\n'))
				++ end;
			if (('\0' == *end) || ('\n' == *end))
				status = FALSE;
			/*changed_char = *end;*/
			*end = '\0';
			if (NULL == tag->name && !isprint(*start))
					return FALSE;

			fields = g_strsplit((gchar*)start, "|", -1);
			field_len = g_strv_length(fields);

			if (field_len >= 1) tag->name = g_strdup(fields[0]);
			else tag->name = NULL;
			if (field_len >= 2 && fields[1] != NULL) tag->var_type = g_strdup(fields[1]);
			if (field_len >= 3 && fields[2] != NULL) tag->arglist = g_strdup(fields[2]);
			tag->type = tm_tag_prototype_t;
			g_strfreev(fields);
		}
	}

	if (NULL == tag->name)
		return FALSE;
	tag->file = file;
	return TRUE;
}

/*
 Same as tm_tag_init_from_file(), but parsing CTags tag file format
 (http://ctags.sourceforge.net/FORMAT) 
*/
static gboolean tm_tag_init_from_file_ctags(TMTag *tag, TMSourceFile *file, FILE *fp)
{
	gchar buf[BUFSIZ];
	gchar *p, *tab;

	tag->refcount = 1;
	tag->type = tm_tag_function_t; /* default type is function if no kind is specified */
	do
	{
		if ((NULL == fgets(buf, BUFSIZ, fp)) || ('\0' == *buf))
			return FALSE;
	}
	while (strncmp(buf, "!_TAG_", 6) == 0); /* skip !_TAG_ lines */

	p = buf;

	/* tag name */
	if (! (tab = strchr(p, '\t')) || p == tab)
		return FALSE;
	tag->name = g_strndup(p, (gsize)(tab - p));
	p = tab + 1;

	/* tagfile, unused */
	if (! (tab = strchr(p, '\t')))
	{
		g_free(tag->name);
		tag->name = NULL;
		return FALSE;
	}
	p = tab + 1;
	/* Ex command, unused */
	if (*p == '/' || *p == '?')
	{
		gchar c = *p;
		for (++p; *p && *p != c; p++)
		{
			if (*p == '\\' && p[1])
				p++;
		}
	}
	else /* assume a line */
		tag->line = atol(p);
	tab = strstr(p, ";\"");
	/* read extension fields */
	if (tab)
	{
		p = tab + 2;
		while (*p && *p != '\n' && *p != '\r')
		{
			gchar *end;
			const gchar *key, *value = NULL;

			/* skip leading tabulations */
			while (*p && *p == '\t') p++;
			/* find the separator (:) and end (\t) */
			key = end = p;
			while (*end && *end != '\t' && *end != '\n' && *end != '\r')
			{
				if (*end == ':' && ! value)
				{
					*end = 0; /* terminate the key */
					value = end + 1;
				}
				end++;
			}
			/* move p paste the so we won't stop parsing by setting *end=0 below */
			p = *end ? end + 1 : end;
			*end = 0; /* terminate the value (or key if no value) */

			if (! value || 0 == strcmp(key, "kind")) /* tag kind */
			{
				const gchar *kind = value ? value : key;

				if (kind[0] && kind[1])
					tag->type = get_tag_type(kind);
				else
				{
					switch (*kind)
					{
						case 'c': tag->type = tm_tag_class_t; break;
						case 'd': tag->type = tm_tag_macro_t; break;
						case 'e': tag->type = tm_tag_enumerator_t; break;
						case 'F': tag->type = tm_tag_other_t; break;  /* Obsolete */
						case 'f': tag->type = tm_tag_function_t; break;
						case 'g': tag->type = tm_tag_enum_t; break;
						case 'I': tag->type = tm_tag_class_t; break;
						case 'i': tag->type = tm_tag_interface_t; break;
						case 'l': tag->type = tm_tag_variable_t; break;
						case 'M': tag->type = tm_tag_macro_t; break;
						case 'm': tag->type = tm_tag_member_t; break;
						case 'n': tag->type = tm_tag_namespace_t; break;
						case 'P': tag->type = tm_tag_package_t; break;
						case 'p': tag->type = tm_tag_prototype_t; break;
						case 's': tag->type = tm_tag_struct_t; break;
						case 't': tag->type = tm_tag_typedef_t; break;
						case 'u': tag->type = tm_tag_union_t; break;
						case 'v': tag->type = tm_tag_variable_t; break;
						case 'x': tag->type = tm_tag_externvar_t; break;
						default:
#ifdef TM_DEBUG
							g_warning("Unknown tag kind %c", *kind);
#endif
							tag->type = tm_tag_other_t; break;
					}
				}
			}
			else if (0 == strcmp(key, "inherits")) /* comma-separated list of classes this class inherits from */
			{
				g_free(tag->inheritance);
				tag->inheritance = g_strdup(value);
			}
			else if (0 == strcmp(key, "implementation")) /* implementation limit */
				tag->impl = get_tag_impl(value);
			else if (0 == strcmp(key, "line")) /* line */
				tag->line = atol(value);
			else if (0 == strcmp(key, "access")) /* access */
				tag->access = get_tag_access(value);
			else if (0 == strcmp(key, "class") ||
					 0 == strcmp(key, "enum") ||
					 0 == strcmp(key, "function") ||
					 0 == strcmp(key, "struct") ||
					 0 == strcmp(key, "union")) /* Name of the class/enum/function/struct/union in which this tag is a member */
			{
				g_free(tag->scope);
				tag->scope = g_strdup(value);
			}
			else if (0 == strcmp(key, "file")) /* static (local) tag */
				tag->local = TRUE;
			else if (0 == strcmp(key, "signature")) /* arglist */
			{
				g_free(tag->arglist);
				tag->arglist = g_strdup(value);
			}
		}
	}

	tag->file = file;
	return TRUE;
}

/*
 Same as tm_tag_new() except that the tag attributes are read from file.
 @param mode TMParserType to use for the tag.
*/
TMTag *tm_tag_new_from_file(TMSourceFile *file, FILE *fp, TMParserType mode, TMFileFormat format)
{
	TMTag *tag;
	gboolean result = FALSE;

	TAG_NEW(tag);

	switch (format)
	{
		case TM_FILE_FORMAT_TAGMANAGER:
			result = tm_tag_init_from_file(tag, file, fp);
			break;
		case TM_FILE_FORMAT_PIPE:
			result = tm_tag_init_from_file_alt(tag, file, fp);
			break;
		case TM_FILE_FORMAT_CTAGS:
			result = tm_tag_init_from_file_ctags(tag, file, fp);
			break;
	}

	if (! result)
	{
		TAG_FREE(tag);
		return NULL;
	}
	tag->lang = mode;
	return tag;
}

/*
 Writes tag information to the given FILE *.
 @param tag The tag information to write.
 @param file FILE pointer to which the tag information is written.
 @param attrs Attributes to be written (bitmask).
 @return TRUE on success, FALSE on failure.
*/
gboolean tm_tag_write(TMTag *tag, FILE *fp, TMTagAttrType attrs)
{
	fprintf(fp, "%s", tag->name);
	if (attrs & tm_tag_attr_type_t)
		fprintf(fp, "%c%d", TA_TYPE, tag->type);
	if ((attrs & tm_tag_attr_arglist_t) && (NULL != tag->arglist))
		fprintf(fp, "%c%s", TA_ARGLIST, tag->arglist);
	if (attrs & tm_tag_attr_line_t)
		fprintf(fp, "%c%ld", TA_LINE, tag->line);
	if (attrs & tm_tag_attr_local_t)
		fprintf(fp, "%c%d", TA_LOCAL, tag->local);
	if ((attrs & tm_tag_attr_scope_t) && (NULL != tag->scope))
		fprintf(fp, "%c%s", TA_SCOPE, tag->scope);
	if ((attrs & tm_tag_attr_inheritance_t) && (NULL != tag->inheritance))
		fprintf(fp, "%c%s", TA_INHERITS, tag->inheritance);
	if (attrs & tm_tag_attr_pointer_t)
		fprintf(fp, "%c%d", TA_POINTER, tag->pointerOrder);
	if ((attrs & tm_tag_attr_vartype_t) && (NULL != tag->var_type))
		fprintf(fp, "%c%s", TA_VARTYPE, tag->var_type);
	if ((attrs & tm_tag_attr_access_t) && (TAG_ACCESS_UNKNOWN != tag->access))
		fprintf(fp, "%c%c", TA_ACCESS, tag->access);
	if ((attrs & tm_tag_attr_impl_t) && (TAG_IMPL_UNKNOWN != tag->impl))
		fprintf(fp, "%c%c", TA_IMPL, tag->impl);

	if (fprintf(fp, "\n"))
		return TRUE;
	else
		return FALSE;
}

/*
 Destroys a TMTag structure, i.e. frees all elements except the tag itself.
 @param tag The TMTag structure to destroy
 @see tm_tag_free()
*/
static void tm_tag_destroy(TMTag *tag)
{
	g_free(tag->name);
	g_free(tag->arglist);
	g_free(tag->scope);
	g_free(tag->inheritance);
	g_free(tag->var_type);
}


/*
 Drops a reference from a TMTag. If the reference count reaches 0, this function
 destroys all data in the tag and frees the tag structure as well.
 @param tag Pointer to a TMTag structure
*/
void tm_tag_unref(TMTag *tag)
{
	/* be NULL-proof because tm_tag_free() was NULL-proof and we indent to be a
	 * drop-in replacment of it */
	if (NULL != tag && g_atomic_int_dec_and_test(&tag->refcount))
	{
		tm_tag_destroy(tag);
		TAG_FREE(tag);
	}
}

/*
 Adds a reference to a TMTag.
 @param tag Pointer to a TMTag structure
 @return the passed-in TMTag
*/
TMTag *tm_tag_ref(TMTag *tag)
{
	g_atomic_int_inc(&tag->refcount);
	return tag;
}

/*
 Inbuilt tag comparison function.
*/
static gint tm_tag_compare(gconstpointer ptr1, gconstpointer ptr2, gpointer user_data)
{
	unsigned int *sort_attr;
	int returnval = 0;
	TMTag *t1 = *((TMTag **) ptr1);
	TMTag *t2 = *((TMTag **) ptr2);
	TMSortOptions *sort_options = user_data;

	if ((NULL == t1) || (NULL == t2))
	{
		g_warning("Found NULL tag");
		return t2 - t1;
	}
	if (NULL == sort_options->sort_attrs)
	{
		if (sort_options->partial)
			return strncmp(FALLBACK(t1->name, ""), FALLBACK(t2->name, ""), strlen(FALLBACK(t1->name, "")));
		else
			return strcmp(FALLBACK(t1->name, ""), FALLBACK(t2->name, ""));
	}

	for (sort_attr = sort_options->sort_attrs; returnval == 0 && *sort_attr != tm_tag_attr_none_t; ++ sort_attr)
	{
		switch (*sort_attr)
		{
			case tm_tag_attr_name_t:
				if (sort_options->partial)
					returnval = strncmp(FALLBACK(t1->name, ""), FALLBACK(t2->name, ""), strlen(FALLBACK(t1->name, "")));
				else
					returnval = strcmp(FALLBACK(t1->name, ""), FALLBACK(t2->name, ""));
				break;
			case tm_tag_attr_file_t:
				returnval = t1->file - t2->file;
				break;
			case tm_tag_attr_line_t:
				returnval = t1->line - t2->line;
				break;
			case tm_tag_attr_type_t:
				returnval = t1->type - t2->type;
				break;
			case tm_tag_attr_scope_t:
				returnval = strcmp(FALLBACK(t1->scope, ""), FALLBACK(t2->scope, ""));
				break;
			case tm_tag_attr_arglist_t:
				returnval = strcmp(FALLBACK(t1->arglist, ""), FALLBACK(t2->arglist, ""));
				if (returnval != 0)
				{
					int line_diff = (t1->line - t2->line);

					returnval = line_diff ? line_diff : returnval;
				}
				break;
			case tm_tag_attr_vartype_t:
				returnval = strcmp(FALLBACK(t1->var_type, ""), FALLBACK(t2->var_type, ""));
				break;
		}
	}
	return returnval;
}

gboolean tm_tags_equal(const TMTag *a, const TMTag *b)
{
	if (a == b)
		return TRUE;

	return (a->line == b->line &&
			a->file == b->file /* ptr comparison */ &&
			strcmp(FALLBACK(a->name, ""), FALLBACK(b->name, "")) == 0 &&
			a->type == b->type &&
			a->local == b->local &&
			a->pointerOrder == b->pointerOrder &&
			a->access == b->access &&
			a->impl == b->impl &&
			a->lang == b->lang &&
			strcmp(FALLBACK(a->scope, ""), FALLBACK(b->scope, "")) == 0 &&
			strcmp(FALLBACK(a->arglist, ""), FALLBACK(b->arglist, "")) == 0 &&
			strcmp(FALLBACK(a->inheritance, ""), FALLBACK(b->inheritance, "")) == 0 &&
			strcmp(FALLBACK(a->var_type, ""), FALLBACK(b->var_type, "")) == 0);
}

/*
 Removes NULL tag entries from an array of tags. Called after tm_tags_dedup() since 
 this function substitutes duplicate entries with NULL
 @param tags_array Array of tags to dedup
 @return TRUE on success, FALSE on failure
*/
gboolean tm_tags_prune(GPtrArray *tags_array)
{
	guint i, count;
	for (i=0, count = 0; i < tags_array->len; ++i)
	{
		if (NULL != tags_array->pdata[i])
			tags_array->pdata[count++] = tags_array->pdata[i];
	}
	tags_array->len = count;
	return TRUE;
}

/*
 Deduplicates an array on tags using the inbuilt comparison function based on
 the attributes specified. Called by tm_tags_sort() when dedup is TRUE.
 @param tags_array Array of tags to dedup.
 @param sort_attributes Attributes the array is sorted on. They will be deduped
 on the same criteria.
 @return TRUE on success, FALSE on failure
*/
gboolean tm_tags_dedup(GPtrArray *tags_array, TMTagAttrType *sort_attributes, gboolean unref_duplicates)
{
	TMSortOptions sort_options;
	guint i;

	if ((!tags_array) || (!tags_array->len))
		return TRUE;
	sort_options.sort_attrs = sort_attributes;
	sort_options.partial = FALSE;
	for (i = 1; i < tags_array->len; ++i)
	{
		if (0 == tm_tag_compare(&(tags_array->pdata[i - 1]), &(tags_array->pdata[i]), &sort_options))
		{
			if (unref_duplicates)
				tm_tag_unref(tags_array->pdata[i-1]);
			tags_array->pdata[i-1] = NULL;
		}
	}
	tm_tags_prune(tags_array);
	return TRUE;
}

/*
 Sort an array of tags on the specified attribuites using the inbuilt comparison
 function.
 @param tags_array The array of tags to be sorted
 @param sort_attributes Attributes to be sorted on (int array terminated by 0)
 @param dedup Whether to deduplicate the sorted array
 @return TRUE on success, FALSE on failure
*/
gboolean tm_tags_sort(GPtrArray *tags_array, TMTagAttrType *sort_attributes, 
	gboolean dedup, gboolean unref_duplicates)
{
	TMSortOptions sort_options;
	
	if ((!tags_array) || (!tags_array->len))
		return TRUE;
	sort_options.sort_attrs = sort_attributes;
	sort_options.partial = FALSE;
	g_ptr_array_sort_with_data(tags_array, tm_tag_compare, &sort_options);
	if (dedup)
		tm_tags_dedup(tags_array, sort_attributes, unref_duplicates);
	return TRUE;
}

void tm_tags_remove_file_tags(TMSourceFile *source_file, GPtrArray *tags_array)
{
	guint i;

	/* Now we choose between an algorithm with complexity O(tags_array->len) and
	 * O(source_file->tags_array->len * log(tags_array->len)). The latter algorithm
	 * is better when tags_array contains many times more tags than
	 * source_file->tags_array so instead of trying to find the removed tags
	 * linearly, binary search is used. The constant 20 is more or less random
	 * but seems to work well. It's exact value isn't so critical because it's
	 * the extremes where the difference is the biggest: when
	 * source_file->tags_array->len == tags_array->len (single file open) and
	 * source_file->tags_array->len << tags_array->len (the number of tags
	 * from the file is a small fraction of all tags).
	 */
	if (source_file->tags_array->len != 0 &&
		tags_array->len / source_file->tags_array->len < 20)
	{
		for (i = 0; i < tags_array->len; i++)
		{
			TMTag *tag = tags_array->pdata[i];

			if (tag->file == source_file)
				tags_array->pdata[i] = NULL;
		}
	}
	else
	{
		GPtrArray *to_delete = g_ptr_array_sized_new(source_file->tags_array->len);

		for (i = 0; i < source_file->tags_array->len; i++)
		{
			guint j;
			guint tag_count;
			TMTag **found;
			TMTag *tag = source_file->tags_array->pdata[i];

			found = tm_tags_find(tags_array, tag->name, FALSE, &tag_count);

			for (j = 0; j < tag_count; j++)
			{
				if (*found != NULL && (*found)->file == source_file)
				{
					/* we cannot set the pointer to NULL now because the search wouldn't work */
					g_ptr_array_add(to_delete, found);
					/* no break - if there are multiple tags of the same name, we would
					 * always find the first instance and wouldn't remove others; duplicates
					 * in the to_delete list aren't a problem */
				}
				found++;
			}
		}

		for (i = 0; i < to_delete->len; i++)
		{
			TMTag **tag = to_delete->pdata[i];
			*tag = NULL;
		}
		g_ptr_array_free(to_delete, TRUE);
	}

	tm_tags_prune(tags_array);
}

/* Optimized merge sort for merging sorted values from one array to another
 * where one of the arrays is much smaller than the other.
 * The merge complexity depends mostly on the size of the small array
 * and is almost independent of the size of the big array.
 * In addition, get rid of the duplicates (if both big_array and small_array are duplicate-free). */
static GPtrArray *merge(GPtrArray *big_array, GPtrArray *small_array, 
	TMSortOptions *sort_options, gboolean unref_duplicates) {
	guint i1 = 0;  /* index to big_array */
	guint i2 = 0;  /* index to small_array */
	guint initial_step;
	guint step;
	GPtrArray *res_array = g_ptr_array_sized_new(big_array->len + small_array->len);
#ifdef TM_DEBUG
	guint cmpnum = 0;
#endif

	/* swap the arrays if len(small) > len(big) */
	if (small_array->len > big_array->len)
	{
		GPtrArray *tmp = small_array;
		small_array = big_array;
		big_array = tmp;
	}
	
	/* on average, we are merging a value from small_array every 
	 * len(big_array) / len(small_array) values - good approximation for fast jump
	 * step size */
	initial_step = (small_array->len > 0) ? big_array->len / small_array->len : 1;
	initial_step = initial_step > 4 ? initial_step : 1;
	step = initial_step;
	
	while (i1 < big_array->len && i2 < small_array->len)
	{
		gpointer val1;
		gpointer val2 = small_array->pdata[i2];

		if (step > 4)  /* fast path start */
		{
			guint j1 = (i1 + step < big_array->len) ? i1 + step : big_array->len - 1;
			
			val1 = big_array->pdata[j1];
#ifdef TM_DEBUG
			cmpnum++;
#endif
			/* if the value in big_array after making the big step is still smaller
			 * than the value in small_array, we can copy all the values inbetween
			 * into the result without making expensive string comparisons */
			if (tm_tag_compare(&val1, &val2, sort_options) < 0)
			{
				while (i1 <= j1) 
				{
					val1 = big_array->pdata[i1];
					g_ptr_array_add(res_array, val1);
					i1++;
				}
			}
			else 
			{
				/* lower the step and try again */
				step /= 2;
			}
		}  /* fast path end */
		else
		{
			gint cmpval;
			
#ifdef TM_DEBUG
			cmpnum++;
#endif
			val1 = big_array->pdata[i1];
			cmpval = tm_tag_compare(&val1, &val2, sort_options);
			if (cmpval < 0)
			{
				g_ptr_array_add(res_array, val1);
				i1++;
			}
			else
			{
				g_ptr_array_add(res_array, val2);
				i2++;
				/* value from small_array gets merged - reset the step size */
				step = initial_step;
				if (cmpval == 0)
				{
					i1++;  /* remove the duplicate, keep just the newly merged value */
					if (unref_duplicates)
						tm_tag_unref(val1);
				}
			}
		}
	}

	/* end of one of the arrays reached - copy the rest from the other array */
	while (i1 < big_array->len)
		g_ptr_array_add(res_array, big_array->pdata[i1++]);
	while (i2 < small_array->len)
		g_ptr_array_add(res_array, small_array->pdata[i2++]);
		
#ifdef TM_DEBUG
	printf("cmpnums: %u\n", cmpnum);
	printf("total tags: %u\n", big_array->len);
	printf("merged tags: %u\n\n", small_array->len);
#endif

	return res_array;
}

GPtrArray *tm_tags_merge(GPtrArray *big_array, GPtrArray *small_array, 
	TMTagAttrType *sort_attributes, gboolean unref_duplicates)
{
	GPtrArray *res_array;
	TMSortOptions sort_options;
	
	sort_options.sort_attrs = sort_attributes;
	sort_options.partial = FALSE;
	res_array = merge(big_array, small_array, &sort_options, unref_duplicates);
	return res_array;
}

/*
 This function will extract the tags of the specified types from an array of tags.
 The returned value is a GPtrArray which should be free-d with a call to
 g_ptr_array_free(array, TRUE). However, do not free the tags themselves since they
 are not duplicated.
 @param tags_array The original array of tags
 @param tag_types - The tag types to extract. Can be a bitmask. For example, passing
 (tm_tag_typedef_t | tm_tag_struct_t) will extract all typedefs and structures from
 the original array.
 @return an array of tags (NULL on failure)
*/
GPtrArray *tm_tags_extract(GPtrArray *tags_array, TMTagType tag_types)
{
	GPtrArray *new_tags;
	guint i;
	if (NULL == tags_array)
		return NULL;
	new_tags = g_ptr_array_new();
	for (i=0; i < tags_array->len; ++i)
	{
		if (NULL != tags_array->pdata[i])
		{
			if (tag_types & (((TMTag *) tags_array->pdata[i])->type))
				g_ptr_array_add(new_tags, tags_array->pdata[i]);
		}
	}
	return new_tags;
}

/*
 Completely frees an array of tags.
 @param tags_array Array of tags to be freed.
 @param free_array Whether the GptrArray is to be freed as well.
*/
void tm_tags_array_free(GPtrArray *tags_array, gboolean free_all)
{
	if (tags_array)
	{
		guint i;
		for (i = 0; i < tags_array->len; ++i)
			tm_tag_unref(tags_array->pdata[i]);
		if (free_all)
			g_ptr_array_free(tags_array, TRUE);
		else
			g_ptr_array_set_size(tags_array, 0);
	}
}

/* copy/pasted bsearch() from libc extended with user_data for comparison function
 * and using glib types */
static gpointer binary_search(gpointer key, gpointer base, size_t nmemb, 
	GCompareDataFunc compar, gpointer user_data)
{
	gsize l, u, idx;
	gpointer p;
	gint comparison;

	l = 0;
	u = nmemb;
	while (l < u)
	{
		idx = (l + u) / 2;
		p = (gpointer) (((const gchar *) base) + (idx * sizeof(gpointer)));
		comparison = (*compar) (key, p, user_data);
		if (comparison < 0)
			u = idx;
		else if (comparison > 0)
			l = idx + 1;
		else
			return (gpointer) p;
	}

	return NULL;
}

static gint tag_search_cmp(gconstpointer ptr1, gconstpointer ptr2, gpointer user_data)
{
	gint res = tm_tag_compare(ptr1, ptr2, user_data);

	if (res == 0)
	{
		TMSortOptions *sort_options = user_data;
		const GPtrArray *tags_array = sort_options->tags_array;
		TMTag **tag = (TMTag **) ptr2;

		/* if previous/next (depending on sort options) tag equal, we haven't
		 * found the first/last tag in a sequence of equal tags yet */
		if (sort_options->first && ptr2 != &tags_array->pdata[0]) {
			if (tm_tag_compare(ptr1, tag - 1, user_data) == 0)
				return -1;
		}
		else if (!sort_options->first && ptr2 != &tags_array->pdata[tags_array->len-1])
		{
			if (tm_tag_compare(ptr1, tag + 1, user_data) == 0)
				return 1;
		}
	}
	return res;
}

/*
 Returns a pointer to the position of the first matching tag in a (sorted) tags array.
 The passed array of tags must be already sorted by name (searched with binary search).
 @param tags_array Tag array (sorted on name)
 @param name Name of the tag to locate.
 @param partial If TRUE, matches the first part of the name instead of doing exact match.
 @param tagCount Return location of the matched tags.
*/
TMTag **tm_tags_find(const GPtrArray *tags_array, const char *name,
		gboolean partial, guint *tagCount)
{
	TMTag *tag, **first;
	TMSortOptions sort_options;

	*tagCount = 0;
	if (!tags_array || !tags_array->len)
		return NULL;

	tag = g_new0(TMTag, 1);
	tag->name = (char *) name;

	sort_options.sort_attrs = NULL;
	sort_options.partial = partial;
	sort_options.tags_array = tags_array;
	sort_options.first = TRUE;
	first = (TMTag **)binary_search(&tag, tags_array->pdata, tags_array->len,
			tag_search_cmp, &sort_options);

	if (first)
	{
		TMTag **last;
		unsigned first_pos;

		sort_options.first = FALSE;
		first_pos = first - (TMTag **)tags_array->pdata;
		/* search between the first element and end */
		last = (TMTag **)binary_search(&tag, first, tags_array->len - first_pos,
				tag_search_cmp, &sort_options);
		*tagCount = last - first + 1;
	}

	g_free(tag);
	return (TMTag **) first;
}

/* Returns TMTag which "own" given line
 @param line Current line in edited file.
 @param file_tags A GPtrArray of edited file TMTag pointers.
 @param tag_types the tag types to include in the match
 @return TMTag pointers to owner tag. */
const TMTag *
tm_get_current_tag (GPtrArray * file_tags, const gulong line, const TMTagType tag_types)
{
	TMTag *matching_tag = NULL;
	if (file_tags && file_tags->len)
	{
		guint i;
		gulong matching_line = 0;

		for (i = 0; (i < file_tags->len); ++i)
		{
			TMTag *tag = TM_TAG (file_tags->pdata[i]);
			if (tag && tag->type & tag_types &&
				tag->line <= line && tag->line > matching_line)
			{
				matching_tag = tag;
				matching_line = tag->line;
			}
		}
	}
	return matching_tag;
}

const gchar *tm_tag_context_separator(TMParserType lang)
{
	switch (lang)
	{
		case TM_PARSER_C:	/* for C++ .h headers or C structs */
		case TM_PARSER_CPP:
		case TM_PARSER_GLSL:	/* for structs */
		/*case GEANY_FILETYPES_RUBY:*/ /* not sure what to use atm*/
		case TM_PARSER_PHP:
		case TM_PARSER_POWERSHELL:
		case TM_PARSER_RUST:
		case TM_PARSER_ZEPHIR:
			return "::";

		/* avoid confusion with other possible separators in group/section name */
		case TM_PARSER_CONF:
		case TM_PARSER_REST:
			return ":::";

		/* no context separator */
		case TM_PARSER_ASCIIDOC:
		case TM_PARSER_TXT2TAGS:
			return "\x03";

		default:
			return ".";
	}
}

gboolean tm_tag_is_anon(const TMTag *tag)
{
	guint i;
	char dummy;

	if (tag->lang == TM_PARSER_C || tag->lang == TM_PARSER_CPP)
		return sscanf(tag->name, "anon_%*[a-z]_%u%c", &i, &dummy) == 1;
	else if (tag->lang == TM_PARSER_FORTRAN || tag->lang == TM_PARSER_F77)
		return sscanf(tag->name, "Structure#%u%c", &i, &dummy) == 1 ||
			sscanf(tag->name, "Interface#%u%c", &i, &dummy) == 1 ||
			sscanf(tag->name, "Enum#%u%c", &i, &dummy) == 1;
	return FALSE;
}


gboolean tm_tag_langs_compatible(TMParserType lang, TMParserType other)
{
	if (lang == TM_PARSER_NONE || other == TM_PARSER_NONE)
		return FALSE;
	if (lang == other)
		return TRUE;
	/* Accept CPP tags for C lang and vice versa */
	else if (lang == TM_PARSER_C && other == TM_PARSER_CPP)
		return TRUE;
	else if (lang == TM_PARSER_CPP && other == TM_PARSER_C)
		return TRUE;

	return FALSE;
}


#ifdef TM_DEBUG /* various debugging functions */

/*
 Returns the type of tag as a string
 @param tag The tag whose type is required
*/
const char *tm_tag_type_name(const TMTag *tag)
{
	g_return_val_if_fail(tag, NULL);
	switch(tag->type)
	{
		case tm_tag_class_t: return "class";
		case tm_tag_enum_t: return "enum";
		case tm_tag_enumerator_t: return "enumval";
		case tm_tag_field_t: return "field";
		case tm_tag_function_t: return "function";
		case tm_tag_interface_t: return "interface";
		case tm_tag_member_t: return "member";
		case tm_tag_method_t: return "method";
		case tm_tag_namespace_t: return "namespace";
		case tm_tag_package_t: return "package";
		case tm_tag_prototype_t: return "prototype";
		case tm_tag_struct_t: return "struct";
		case tm_tag_typedef_t: return "typedef";
		case tm_tag_union_t: return "union";
		case tm_tag_variable_t: return "variable";
		case tm_tag_externvar_t: return "extern";
		case tm_tag_macro_t: return "define";
		case tm_tag_macro_with_arg_t: return "macro";
		default: return NULL;
	}
	return NULL;
}

/*
 Returns the TMTagType given the name of the type. Reverse of tm_tag_type_name.
 @param tag_name Name of the tag type
*/
TMTagType tm_tag_name_type(const char* tag_name)
{
	g_return_val_if_fail(tag_name, tm_tag_undef_t);

	if (strcmp(tag_name, "class") == 0) return tm_tag_class_t;
	else if (strcmp(tag_name, "enum") == 0) return tm_tag_enum_t;
	else if (strcmp(tag_name, "enumval") == 0) return tm_tag_enumerator_t;
	else if (strcmp(tag_name, "field") == 0) return tm_tag_field_t;
	else if (strcmp(tag_name, "function") == 0) return tm_tag_function_t;
	else if (strcmp(tag_name, "interface") == 0) return tm_tag_interface_t;
	else if (strcmp(tag_name, "member") == 0) return tm_tag_member_t;
	else if (strcmp(tag_name, "method") == 0) return tm_tag_method_t;
	else if (strcmp(tag_name, "namespace") == 0) return tm_tag_namespace_t;
	else if (strcmp(tag_name, "package") == 0) return tm_tag_package_t;
	else if (strcmp(tag_name, "prototype") == 0) return tm_tag_prototype_t;
	else if (strcmp(tag_name, "struct") == 0) return tm_tag_struct_t;
	else if (strcmp(tag_name, "typedef") == 0) return tm_tag_typedef_t;
	else if (strcmp(tag_name, "union") == 0) return tm_tag_union_t;
	else if (strcmp(tag_name, "variable") == 0) return tm_tag_variable_t;
	else if (strcmp(tag_name, "extern") == 0) return tm_tag_externvar_t;
	else if (strcmp(tag_name, "define") == 0) return tm_tag_macro_t;
	else if (strcmp(tag_name, "macro") == 0) return tm_tag_macro_with_arg_t;
	else return tm_tag_undef_t;
}

static const char *tm_tag_impl_name(TMTag *tag)
{
	g_return_val_if_fail(tag, NULL);
	if (TAG_IMPL_VIRTUAL == tag->impl)
		return "virtual";
	else
		return NULL;
}

static const char *tm_tag_access_name(TMTag *tag)
{
	g_return_val_if_fail(tag, NULL);
	if (TAG_ACCESS_PUBLIC == tag->access)
		return "public";
	else if (TAG_ACCESS_PROTECTED == tag->access)
		return "protected";
	else if (TAG_ACCESS_PRIVATE == tag->access)
		return "private";
	else
		return NULL;
}

/*
  Prints information about a tag to the given file pointer.
  @param tag The tag whose info is required.
  @param fp The file pointer of the file to print the info to.
*/
void tm_tag_print(TMTag *tag, FILE *fp)
{
	const char *laccess, *impl, *type;
	if (!tag || !fp)
		return;
	laccess = tm_tag_access_name(tag);
	impl = tm_tag_impl_name(tag);
	type = tm_tag_type_name(tag);
	if (laccess)
		fprintf(fp, "%s ", laccess);
	if (impl)
		fprintf(fp, "%s ", impl);
	if (type)
		fprintf(fp, "%s ", type);
	if (tag->var_type)
		fprintf(fp, "%s ", tag->var_type);
	if (tag->scope)
		fprintf(fp, "%s::", tag->scope);
	fprintf(fp, "%s", tag->name);
	if (tag->arglist)
		fprintf(fp, "%s", tag->arglist);
	if (tag->inheritance)
		fprintf(fp, " : from %s", tag->inheritance);
	if ((tag->file) && (tag->line > 0))
		fprintf(fp, "[%s:%ld]", tag->file->file_name
		  , tag->line);
	fprintf(fp, "\n");
}

/*
  Prints info about all tags in the array to the given file pointer.
*/
void tm_tags_array_print(GPtrArray *tags, FILE *fp)
{
	guint i;
	TMTag *tag;
	if (!(tags && (tags->len > 0) && fp))
		return;
	for (i = 0; i < tags->len; ++i)
	{
		tag = TM_TAG(tags->pdata[i]);
		tm_tag_print(tag, fp);
	}
}

/*
  Returns the depth of tag scope (useful for finding tag hierarchy
*/
gint tm_tag_scope_depth(const TMTag *t)
{
	const gchar *context_sep = tm_tag_context_separator(t->lang);
	gint depth;
	char *s;
	if(!(t && t->scope))
		return 0;
	for (s = t->scope, depth = 0; s; s = strstr(s, context_sep))
	{
		++ depth;
		++ s;
	}
	return depth;
}

#endif /* TM_DEBUG */
