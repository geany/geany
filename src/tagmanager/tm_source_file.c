/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*   Copyright 2005 The Geany contributors
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

/**
 * @file tm_source_file.h
 The TMSourceFile structure and associated functions are used to maintain
 tags for individual files.
*/


#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gstdio.h>
#ifdef G_OS_WIN32
# define VC_EXTRALEAN
# define WIN32_LEAN_AND_MEAN
# include <windows.h> /* for GetFullPathName */
#endif

#include "readtags.h"

#include "tm_source_file.h"
#include "tm_tag.h"
#include "tm_parser.h"
#include "tm_ctags.h"

typedef struct
{
	TMSourceFile public;
	guint refcount;
} TMSourceFilePriv;


typedef enum {
	TM_FILE_FORMAT_TAGMANAGER,
	TM_FILE_FORMAT_PIPE,
	TM_FILE_FORMAT_CTAGS
} TMFileFormat;

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
	TA_FLAGS
};


#define SOURCE_FILE_NEW(S) ((S) = g_slice_new(TMSourceFilePriv))
#define SOURCE_FILE_FREE(S) g_slice_free(TMSourceFilePriv, (TMSourceFilePriv *) S)

static int get_path_max(const char *path)
{
#ifdef PATH_MAX
	return PATH_MAX;
#else
	int path_max = pathconf(path, _PC_PATH_MAX);
	if (path_max <= 0)
		path_max = 4096;
	return path_max;
#endif
}


#if defined(G_OS_WIN32) && !defined(HAVE_REALPATH)
/* realpath implementation for Windows found at https://bugzilla.gnome.org/show_bug.cgi?id=342926
 * this one is better than e.g. liberty's lrealpath because this one uses Win32 API and works
 * with special chars within the filename */
static char *realpath (const char *pathname, char *resolved_path)
{
	int size;

	if (resolved_path != NULL)
	{
		int path_max = get_path_max(pathname);
		size = GetFullPathNameA (pathname, path_max, resolved_path, NULL);
		if (size > path_max)
			return NULL;
		else
			return resolved_path;
	}
	else
	{
		size = GetFullPathNameA (pathname, 0, NULL, NULL);
		resolved_path = g_new0 (char, size);
		GetFullPathNameA (pathname, size, resolved_path, NULL);
		return resolved_path;
	}
}
#endif

/**
 Given a file name, returns a newly allocated string containing the realpath()
 of the file.
 @param file_name The original file_name
 @return A newly allocated string containing the real path to the file. NULL if none is available.
 @deprecated since 1.32 (ABI 235)
 @see utils_get_real_path()
*/
GEANY_API_SYMBOL
gchar *tm_get_real_path(const gchar *file_name)
{
	if (file_name)
	{
		gsize len = get_path_max(file_name) + 1;
		gchar *path = g_malloc0(len);

		if (realpath(file_name, path))
			return path;
		else
			g_free(path);
	}
	return NULL;
}

gchar tm_source_file_get_tag_impl(const gchar *impl)
{
	if ((0 == strcmp("virtual", impl))
	 || (0 == strcmp("pure virtual", impl)))
		return TAG_IMPL_VIRTUAL;

#ifdef TM_DEBUG
		g_warning("Unknown implementation %s", impl);
#endif
	return TAG_IMPL_UNKNOWN;
}

gchar tm_source_file_get_tag_access(const gchar *access)
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
 Initializes an already malloc()ed TMTag structure by reading a tag entry
 line from a file. The structure should be allocated beforehand.
 @param tag The TMTag structure to populate
 @param file The TMSourceFile struct (assigned to the file member)
 @param fp FILE pointer from where the tag line is read
 @return TRUE on success, FALSE on FAILURE
*/
static gboolean init_tag_from_file(TMTag *tag, TMSourceFile *file, FILE *fp, TMParserType lang)
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
			{
				tag->name = g_strdup((gchar*)start);
				if (tm_parser_is_anon_name(lang, tag->name))
					tag->flags |= tm_tag_flag_anon_t;
			}
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
				case TA_FLAGS:
					tag->flags |= atoi((gchar*)start + 1);
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
static gboolean init_tag_from_file_alt(TMTag *tag, TMSourceFile *file, FILE *fp)
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


static void read_ctags_file(const gchar *tags_file, TMParserType lang, GPtrArray *file_tags)
{
	tagEntry entry;
	tagFile *f = tagsOpen(tags_file, NULL);
	const gchar *lang_kinds = tm_ctags_get_lang_kinds(lang);
	GArray *unknown_fields = g_array_sized_new(FALSE, FALSE, sizeof(guint), 10);

	while (tagsNext(f, &entry))
	{
		TMTagType type;
		TMTag *tag;

		if (!entry.kind)
			continue;

		if (entry.kind[0] && entry.kind[1])
			type = tm_parser_get_tag_type(tm_ctags_get_kind_from_name(entry.kind, lang), lang);  /* 'K' field */
		else
			type = tm_parser_get_tag_type(*entry.kind, lang);  /* 'k' field */

		if (type == tm_tag_undef_t)
			continue;

		tag = tm_tag_new();
		tag->refcount = 1;
		tag->name = g_strdup(entry.name);
		tag->type = type;
		tag->lang = lang;
		tag->local = entry.fileScope;  /* 'f' field */
		tag->line = entry.address.lineNumber;  /* 'n' field */
		tag->file = NULL;

		if (tm_parser_is_anon_name(lang, tag->name))
			tag->flags |= tm_tag_flag_anon_t;

		for (guint i = 0; i < entry.fields.count; i++)
		{
			const gchar *key = entry.fields.list[i].key;
			const gchar *value = entry.fields.list[i].value;

			if (strcmp(key, "scope") == 0)  /* 'sZ' field */
			{
				/* scope:class:A::B::C */
				const gchar *val = strchr(value, ':');
				if (val && *(++val))
				{
					g_free(tag->scope);
					tag->scope = g_strdup(val);
				}
			}
			else if (strcmp(key, "signature") == 0)  /* 'S' field */
			{
				g_free(tag->arglist);
				tag->arglist = g_strdup(value);
			}
			else if (strcmp(key, "inherits") == 0)  /* 'i' field */
			{
				g_free(tag->inheritance);
				tag->inheritance = g_strdup(value);
			}
			else if (strcmp(key, "typeref") == 0)  /* 't' field */
			{
				/* typeref:typename:int */
				const gchar *val = strchr(value, ':');
				if (val && *(++val) &&
					(g_str_has_prefix(value, "typename:") || g_str_has_prefix(value, "unknown:")))
				{
					/* "unknown:" above is used by the php parser, all other parsers use "typename:" */
					g_free(tag->var_type);
					tag->var_type = g_strdup(val);
				}
			}
			else if (strcmp(key, "extras") == 0)  /* 'E' field */
			{
				/* extras may contain multiple values such as extras:fileScope,anonymous */
				if (strstr(value, "anonymous"))
					tag->flags |= tm_tag_flag_anon_t;
			}
			else if (strcmp(key, "implementation") == 0)  /* 'm' field */
				tag->impl = tm_source_file_get_tag_impl(value);
			else if (strcmp(key, "access") == 0)  /* 'a' field */
				tag->access = tm_source_file_get_tag_access(value);
			else if (strcmp(key, "language") == 0)  /* 'l' field */
			{
				TMParserType tag_lang = tm_ctags_get_named_lang(value);
				if (tag_lang >= 0)
					tag->lang = tag_lang;
			}
			else
				g_array_append_val(unknown_fields, i);
		}

		if (!tag->scope)
		{
			/* search for scope introduced by scope kind name only after going
			 * through all extension fields and having tag->lang updated when
			 * "language" field is present */
			for (guint i = 0; !tag->scope && i < unknown_fields->len; i++)
			{
				const guint idx = g_array_index(unknown_fields, guint, i);
				const gchar *key = entry.fields.list[idx].key;
				const gchar *value = entry.fields.list[idx].value;

				for (gint j = 0; lang_kinds[j]; j++)
				{
					const gchar kind = lang_kinds[j];
					const gchar *name = tm_ctags_get_kind_name(kind, tag->lang);
					if (strcmp(key, name) == 0)
					{
						/* 's' field - scope without the 'scope:' prefix */
						tag->scope = g_strdup(value);
						break;
					}
				}
			}
		}

		unknown_fields->len = 0;
		g_ptr_array_add(file_tags, tag);
	}

	g_array_unref(unknown_fields);
	tagsClose(f);
}


static TMTag *new_tag_from_tags_file(TMSourceFile *file, FILE *fp, TMParserType mode, TMFileFormat format)
{
	TMTag *tag = tm_tag_new();
	gboolean result = FALSE;

	switch (format)
	{
		case TM_FILE_FORMAT_TAGMANAGER:
			result = init_tag_from_file(tag, file, fp, mode);
			break;
		case TM_FILE_FORMAT_PIPE:
			result = init_tag_from_file_alt(tag, file, fp);
			break;
		case TM_FILE_FORMAT_CTAGS:
			g_warn_if_reached();  /* this should never be reached; ctags files are handled separately */
			break;
	}

	if (! result)
	{
		tm_tag_unref(tag);
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
static gboolean write_tag(TMTag *tag, FILE *fp, TMTagAttrType attrs)
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
	if (attrs & tm_tag_attr_flags_t)
		fprintf(fp, "%c%d", TA_FLAGS, tag->flags);
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

GPtrArray *tm_source_file_read_tags_file(const gchar *tags_file, TMParserType mode)
{
	guchar buf[BUFSIZ];
	FILE *fp;
	GPtrArray *file_tags;
	TMTag *tag;
	TMFileFormat format = TM_FILE_FORMAT_TAGMANAGER;

	if (NULL == (fp = g_fopen(tags_file, "r")))
		return NULL;
	if ((NULL == fgets((gchar*) buf, BUFSIZ, fp)) || ('\0' == *buf))
	{
		fclose(fp);
		return NULL; /* early out on error */
	}
	else
	{	/* We read (and discard) the first line for the format specification. */
		if (buf[0] == '#' && strstr((gchar*) buf, "format=pipe") != NULL)
			format = TM_FILE_FORMAT_PIPE;
		else if (buf[0] == '#' && strstr((gchar*) buf, "format=tagmanager") != NULL)
			format = TM_FILE_FORMAT_TAGMANAGER;
		else if (buf[0] == '#' && strstr((gchar*) buf, "format=ctags") != NULL)
		{
			format = TM_FILE_FORMAT_CTAGS;
			g_warning("# format=ctags directive is no longer supported; please remove it from %s", tags_file);
		}
		else if (strncmp((gchar*) buf, "!_TAG_", 6) == 0)
			format = TM_FILE_FORMAT_CTAGS;
		else
		{	/* We didn't find a valid format specification, so we try to auto-detect the format
			 * by counting the pipe characters on the first line and asumme pipe format when
			 * we find more than one pipe on the line. */
			guint i, pipe_cnt = 0, tab_cnt = 0;
			for (i = 0; i < BUFSIZ && buf[i] != '\0' && pipe_cnt < 2; i++)
			{
				if (buf[i] == '|')
					pipe_cnt++;
				else if (buf[i] == '\t')
					tab_cnt++;
			}
			if (pipe_cnt > 1)
				format = TM_FILE_FORMAT_PIPE;
			else if (tab_cnt > 1)
				format = TM_FILE_FORMAT_CTAGS;
			/* reset the file pointer, to start reading again from the beginning */
			rewind(fp);
		}
	}

	file_tags = g_ptr_array_new();
	if (format == TM_FILE_FORMAT_CTAGS)
	{
		fclose(fp);  /* the readtags library opens the file by itself */
		read_ctags_file(tags_file, mode, file_tags);
	}
	else
	{
		while (NULL != (tag = new_tag_from_tags_file(NULL, fp, mode, format)))
			g_ptr_array_add(file_tags, tag);
		fclose(fp);
	}

	return file_tags;
}

gboolean tm_source_file_write_tags_file(const gchar *tags_file, GPtrArray *tags_array)
{
	guint i;
	FILE *fp;
	gboolean ret = TRUE;

	g_return_val_if_fail(tags_array && tags_file, FALSE);

	fp = g_fopen(tags_file, "w");
	if (!fp)
		return FALSE;

	fprintf(fp, "# format=tagmanager\n");
	for (i = 0; i < tags_array->len; i++)
	{
		TMTag *tag = TM_TAG(tags_array->pdata[i]);

		ret = write_tag(tag, fp, tm_tag_attr_type_t
		  | tm_tag_attr_scope_t | tm_tag_attr_arglist_t | tm_tag_attr_vartype_t
		  | tm_tag_attr_flags_t);

		if (!ret)
			break;
	}
	fclose(fp);

	return ret;
}


/* Initializes a TMSourceFile structure from a file name. */
static gboolean tm_source_file_init(TMSourceFile *source_file, const char *file_name,
	const char* name)
{
	GStatBuf s;
	int status;

#ifdef TM_DEBUG
	g_message("Source File init: %s", file_name);
#endif

	if (file_name != NULL)
	{
		status = g_stat(file_name, &s);
		if (0 != status)
		{
			/* g_warning("Unable to stat %s", file_name);*/
			return FALSE;
		}
		if (!S_ISREG(s.st_mode))
		{
			g_warning("%s: Not a regular file", file_name);
			return FALSE;
		}
		source_file->file_name = tm_get_real_path(file_name);
		source_file->short_name = strrchr(source_file->file_name, G_DIR_SEPARATOR);
		if (source_file->short_name)
			++ source_file->short_name;
		else
			source_file->short_name = source_file->file_name;
	}

	source_file->tags_array = g_ptr_array_new();

	if (name == NULL)
		source_file->lang = TM_PARSER_NONE;
	else
		source_file->lang = tm_ctags_get_named_lang(name);

	source_file->trust_file_scope = TRUE;

	if (source_file->lang == TM_PARSER_C || source_file->lang == TM_PARSER_CPP)
	{
		const gchar **ext;
		const gchar *common_src_exts[] =
			{".c", ".C", ".cc", ".cp", ".cpp", ".cxx", ".c++", ".CPP", ".CXX", NULL};

		source_file->trust_file_scope = FALSE;
		for (ext = common_src_exts; *ext; ext++)
		{
			if (g_str_has_suffix(source_file->short_name, *ext))
			{
				source_file->trust_file_scope = TRUE;
				break;
			}
		}
	}

	return TRUE;
}

/** Initializes a TMSourceFile structure and returns a pointer to it. The
 * TMSourceFile has to be added to TMWorkspace to start its parsing.
 * @param file_name The file name.
 * @param name Name of the used programming language, NULL to disable parsing.
 * @return The created unparsed TMSourceFile object.
 * */
GEANY_API_SYMBOL
TMSourceFile *tm_source_file_new(const char *file_name, const char *name)
{
	TMSourceFilePriv *priv;

	SOURCE_FILE_NEW(priv);
	if (TRUE != tm_source_file_init(&priv->public, file_name, name))
	{
		SOURCE_FILE_FREE(priv);
		return NULL;
	}
	priv->refcount = 1;
	return &priv->public;
}


static TMSourceFile *tm_source_file_dup(TMSourceFile *source_file)
{
	TMSourceFilePriv *priv = (TMSourceFilePriv *) source_file;

	g_return_val_if_fail(NULL != source_file, NULL);

	g_atomic_int_inc(&priv->refcount);
	return source_file;
}

/* Destroys the contents of the source file. Note that the tags are owned by the
 source file and are also destroyed when the source file is destroyed. If pointers
 to these tags are used elsewhere, then those tag arrays should be rebuilt.
*/
static void tm_source_file_destroy(TMSourceFile *source_file)
{
#ifdef TM_DEBUG
	g_message("Destroying source file: %s", source_file->file_name);
#endif

	g_free(source_file->file_name);
	tm_tags_array_free(source_file->tags_array, TRUE);
	source_file->tags_array = NULL;
}

/** Decrements the reference count of @a source_file
 *
 * If the reference count drops to 0, then @a source_file is freed, including all contents.
 * Make sure the @a source_file is already removed from any TMWorkSpace before the
 * this happens.
 * @param source_file The source file to free.
 * @see tm_workspace_remove_source_file()
*/
GEANY_API_SYMBOL
void tm_source_file_free(TMSourceFile *source_file)
{
	TMSourceFilePriv *priv = (TMSourceFilePriv *) source_file;

	if (NULL != priv && g_atomic_int_dec_and_test(&priv->refcount))
	{
		tm_source_file_destroy(source_file);
		SOURCE_FILE_FREE(priv);
	}
}

/** Gets the GBoxed-derived GType for TMSourceFile
 *
 * @return TMSourceFile type . */
GEANY_API_SYMBOL
GType tm_source_file_get_type(void);

G_DEFINE_BOXED_TYPE(TMSourceFile, tm_source_file, tm_source_file_dup, tm_source_file_free);

/* Parses the text-buffer or source file and regenarates the tags.
 @param source_file The source file to parse
 @param text_buf The text buffer to parse
 @param buf_size The size of text_buf.
 @param use_buffer Set FALSE to ignore the buffer and parse the file directly or
 TRUE to parse the buffer and ignore the file content.
 @return TRUE on success, FALSE on failure
*/
gboolean tm_source_file_parse(TMSourceFile *source_file, guchar* text_buf, gsize buf_size,
	gboolean use_buffer)
{
	const char *file_name;
	gboolean retry = TRUE;

	if ((NULL == source_file) || (NULL == source_file->file_name))
	{
		g_warning("Attempt to parse NULL file");
		return FALSE;
	}

	if (source_file->lang == TM_PARSER_NONE)
	{
		tm_tags_array_free(source_file->tags_array, FALSE);
		return FALSE;
	}

	file_name = source_file->file_name;

	if (use_buffer && (NULL == text_buf || 0 == buf_size))
	{
		/* Empty buffer, "parse" by setting empty tag array */
		tm_tags_array_free(source_file->tags_array, FALSE);
		return TRUE;
	}

	tm_tags_array_free(source_file->tags_array, FALSE);

	tm_ctags_parse(use_buffer ? text_buf : NULL, buf_size, file_name,
		source_file->lang, source_file);

	return !retry;
}

/* Gets the name associated with the language index.
 @param lang The language index.
 @return The language name, or NULL.
*/
const gchar *tm_source_file_get_lang_name(TMParserType lang)
{
	return tm_ctags_get_lang_name(lang);
}

/* Gets the language index for \a name.
 @param name The language name.
 @return The language index, or TM_PARSER_NONE.
*/
TMParserType tm_source_file_get_named_lang(const gchar *name)
{
	return tm_ctags_get_named_lang(name);
}
