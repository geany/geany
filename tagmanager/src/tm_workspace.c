/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

/**
 * @file tm_workspace.h
 The TMWorkspace structure is meant to be used as a singleton to store application
 wide tag information.

 The workspace is intended to contain a list of global tags
 and a set of individual source files.
*/

#include "general.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_GLOB_H
# include <glob.h>
#endif
#include <glib/gstdio.h>

#include "tm_workspace.h"
#include "tm_tag.h"
#include "tm_parser.h"


/* when changing, always keep the three sort criteria below in sync */
static TMTagAttrType workspace_tags_sort_attrs[] = 
{ 
	tm_tag_attr_name_t, tm_tag_attr_file_t, tm_tag_attr_line_t,
	tm_tag_attr_type_t, tm_tag_attr_scope_t, tm_tag_attr_arglist_t, 0
};

/* for file tags the file is always identical, don't use for sorting */
static TMTagAttrType file_tags_sort_attrs[] = 
{ 
	tm_tag_attr_name_t, tm_tag_attr_line_t,
	tm_tag_attr_type_t, tm_tag_attr_scope_t, tm_tag_attr_arglist_t, 0
};

/* global tags don't have file/line information */
static TMTagAttrType global_tags_sort_attrs[] =
{
	tm_tag_attr_name_t, 
	tm_tag_attr_type_t, tm_tag_attr_scope_t, tm_tag_attr_arglist_t, 0
};

static TMTagType TM_MEMBER_TYPE_MASK =
	tm_tag_function_t | tm_tag_prototype_t |
	tm_tag_member_t | tm_tag_field_t |
	tm_tag_method_t | tm_tag_enumerator_t;

static TMWorkspace *theWorkspace = NULL;


static gboolean tm_create_workspace(void)
{
	theWorkspace = g_new(TMWorkspace, 1);
	theWorkspace->tags_array = g_ptr_array_new();

	theWorkspace->global_tags = g_ptr_array_new();
	theWorkspace->source_files = g_ptr_array_new();
	theWorkspace->typename_array = g_ptr_array_new();
	theWorkspace->global_typename_array = g_ptr_array_new();
	theWorkspace->member_array = g_ptr_array_new();
	theWorkspace->global_member_array = g_ptr_array_new();
	return TRUE;
}


/* Frees the workspace structure and all child source files. Use only when
 exiting from the main program.
*/
void tm_workspace_free(void)
{
	guint i;

#ifdef TM_DEBUG
	g_message("Workspace destroyed");
#endif

	for (i=0; i < theWorkspace->source_files->len; ++i)
		tm_source_file_free(theWorkspace->source_files->pdata[i]);
	g_ptr_array_free(theWorkspace->source_files, TRUE);
	tm_tags_array_free(theWorkspace->global_tags, TRUE);
	g_ptr_array_free(theWorkspace->tags_array, TRUE);
	g_ptr_array_free(theWorkspace->typename_array, TRUE);
	g_ptr_array_free(theWorkspace->global_typename_array, TRUE);
	g_ptr_array_free(theWorkspace->member_array, TRUE);
	g_ptr_array_free(theWorkspace->global_member_array, TRUE);
	g_free(theWorkspace);
	theWorkspace = NULL;
}


/* Since TMWorkspace is a singleton, you should not create multiple
 workspaces, but get a pointer to the workspace whenever required. The first
 time a pointer is requested, or a source file is added to the workspace,
 a workspace is created. Subsequent calls to the function will return the
 created workspace.
*/
const TMWorkspace *tm_get_workspace(void)
{
	if (NULL == theWorkspace)
		tm_create_workspace();
	return theWorkspace;
}


static void tm_workspace_merge_tags(GPtrArray **big_array, GPtrArray *small_array)
{
	GPtrArray *new_tags = tm_tags_merge(*big_array, small_array, workspace_tags_sort_attrs, FALSE);
	/* tags owned by TMSourceFile - free just the pointer array */
	g_ptr_array_free(*big_array, TRUE);
	*big_array = new_tags;
}


static void merge_extracted_tags(GPtrArray **dest, GPtrArray *src, TMTagType tag_types)
{
	GPtrArray *arr;

	arr = tm_tags_extract(src, tag_types);
	tm_workspace_merge_tags(dest, arr);
	g_ptr_array_free(arr, TRUE);
}


static void update_source_file(TMSourceFile *source_file, guchar* text_buf,
	gsize buf_size, gboolean use_buffer, gboolean update_workspace)
{
#ifdef TM_DEBUG
	g_message("Source file updating based on source file %s", source_file->file_name);
#endif

	if (update_workspace)
	{
		/* tm_source_file_parse() deletes the tag objects - remove the tags from
		 * workspace while they exist and can be scanned */
		tm_tags_remove_file_tags(source_file, theWorkspace->tags_array);
		tm_tags_remove_file_tags(source_file, theWorkspace->typename_array);
		tm_tags_remove_file_tags(source_file, theWorkspace->member_array);
	}
	tm_source_file_parse(source_file, text_buf, buf_size, use_buffer);
	tm_tags_sort(source_file->tags_array, file_tags_sort_attrs, FALSE, TRUE);
	if (update_workspace)
	{
#ifdef TM_DEBUG
		g_message("Updating workspace from source file");
#endif
		tm_workspace_merge_tags(&theWorkspace->tags_array, source_file->tags_array);

		merge_extracted_tags(&(theWorkspace->typename_array), source_file->tags_array, TM_GLOBAL_TYPE_MASK);
		merge_extracted_tags(&(theWorkspace->member_array), source_file->tags_array, TM_MEMBER_TYPE_MASK);
	}
#ifdef TM_DEBUG
	else
		g_message("Skipping workspace update because update_workspace is %s",
			update_workspace?"TRUE":"FALSE");

#endif
}


/** Adds a source file to the workspace, parses it and updates the workspace tags.
 @param source_file The source file to add to the workspace.
*/
GEANY_API_SYMBOL
void tm_workspace_add_source_file(TMSourceFile *source_file)
{
	g_return_if_fail(source_file != NULL);

	g_ptr_array_add(theWorkspace->source_files, source_file);
	update_source_file(source_file, NULL, 0, FALSE, TRUE);
}


void tm_workspace_add_source_file_noupdate(TMSourceFile *source_file)
{
	g_return_if_fail(source_file != NULL);

	g_ptr_array_add(theWorkspace->source_files, source_file);
}


/* Updates the source file by reparsing the text-buffer passed as parameter.
 Ctags will use a parsing based on buffer instead of on files.
 You should call this function when you don't want a previous saving of the file
 you're editing. It's useful for a "real-time" updating of the tags.
 The tags array and the tags themselves are destroyed and re-created, hence any
 other tag arrays pointing to these tags should be rebuilt as well. All sorting
 information is also lost. The language parameter is automatically detected 
 the first time the file is parsed if it is set to LANG_AUTO.
 @param source_file The source file to update with a buffer.
 @param text_buf A text buffer. The user should take care of allocate and free it after
 the use here.
 @param buf_size The size of text_buf.
*/
void tm_workspace_update_source_file_buffer(TMSourceFile *source_file, guchar* text_buf,
	gsize buf_size)
{
	update_source_file(source_file, text_buf, buf_size, TRUE, TRUE);
}


/** Removes a source file from the workspace if it exists. This function also removes
 the tags belonging to this file from the workspace. To completely free the TMSourceFile 
 pointer call tm_source_file_free() on it.
 @param source_file Pointer to the source file to be removed.
*/
GEANY_API_SYMBOL
void tm_workspace_remove_source_file(TMSourceFile *source_file)
{
	guint i;

	g_return_if_fail(source_file != NULL);

	for (i=0; i < theWorkspace->source_files->len; ++i)
	{
		if (theWorkspace->source_files->pdata[i] == source_file)
		{
			tm_tags_remove_file_tags(source_file, theWorkspace->tags_array);
			tm_tags_remove_file_tags(source_file, theWorkspace->typename_array);
			tm_tags_remove_file_tags(source_file, theWorkspace->member_array);
			g_ptr_array_remove_index_fast(theWorkspace->source_files, i);
			return;
		}
	}
}


/* Recreates workspace tag array from all member TMSourceFile objects. Use if you 
 want to globally refresh the workspace. This function does not call tm_source_file_update()
 which should be called before this function on source files which need to be
 reparsed.
*/
static void tm_workspace_update(void)
{
	guint i, j;
	TMSourceFile *source_file;

#ifdef TM_DEBUG
	g_message("Recreating workspace tags array");
#endif

	g_ptr_array_set_size(theWorkspace->tags_array, 0);

#ifdef TM_DEBUG
	g_message("Total %d objects", theWorkspace->source_files->len);
#endif
	for (i=0; i < theWorkspace->source_files->len; ++i)
	{
		source_file = theWorkspace->source_files->pdata[i];
#ifdef TM_DEBUG
		g_message("Adding tags of %s", source_file->file_name);
#endif
		if (source_file->tags_array->len > 0)
		{
			for (j = 0; j < source_file->tags_array->len; ++j)
			{
				g_ptr_array_add(theWorkspace->tags_array,
					source_file->tags_array->pdata[j]);
			}
		}
	}
#ifdef TM_DEBUG
	g_message("Total: %d tags", theWorkspace->tags_array->len);
#endif
	tm_tags_sort(theWorkspace->tags_array, workspace_tags_sort_attrs, TRUE, FALSE);
	
	theWorkspace->typename_array = tm_tags_extract(theWorkspace->tags_array, TM_GLOBAL_TYPE_MASK);
	theWorkspace->member_array = tm_tags_extract(theWorkspace->tags_array, TM_MEMBER_TYPE_MASK);
}


/** Adds multiple source files to the workspace and updates the workspace tag arrays.
 This is more efficient than calling tm_workspace_add_source_file() and
 tm_workspace_update_source_file() separately for each of the files.
 @param source_files The source files to be added to the workspace.
*/
GEANY_API_SYMBOL
void tm_workspace_add_source_files(GPtrArray *source_files)
{
	guint i;

	g_return_if_fail(source_files != NULL);

	for (i = 0; i < source_files->len; i++)
	{
		TMSourceFile *source_file = source_files->pdata[i];
		
		tm_workspace_add_source_file_noupdate(source_file);
		update_source_file(source_file, NULL, 0, FALSE, FALSE);
	}
	
	tm_workspace_update();
}


/** Removes multiple source files from the workspace and updates the workspace tag
 arrays. This is more efficient than calling tm_workspace_remove_source_file()
 separately for each of the files. To completely free the TMSourceFile pointers
 call tm_source_file_free() on each of them.
 @param source_files The source files to be removed from the workspace.
*/
GEANY_API_SYMBOL
void tm_workspace_remove_source_files(GPtrArray *source_files)
{
	guint i, j;

	g_return_if_fail(source_files != NULL);

	//TODO: sort both arrays by pointer value and remove in single pass
	for (i = 0; i < source_files->len; i++)
	{
		TMSourceFile *source_file = source_files->pdata[i];
		
		for (j = 0; j < theWorkspace->source_files->len; j++)
		{
			if (theWorkspace->source_files->pdata[j] == source_file)
			{
				g_ptr_array_remove_index_fast(theWorkspace->source_files, j);
				break;
			}
		}
	}
	
	tm_workspace_update();
}


/* Loads the global tag list from the specified file. The global tag list should
 have been first created using tm_workspace_create_global_tags().
 @param tags_file The file containing global tags.
 @return TRUE on success, FALSE on failure.
 @see tm_workspace_create_global_tags()
*/
gboolean tm_workspace_load_global_tags(const char *tags_file, gint mode)
{
	guchar buf[BUFSIZ];
	FILE *fp;
	GPtrArray *file_tags, *new_tags;
	TMTag *tag;
	TMFileFormat format = TM_FILE_FORMAT_TAGMANAGER;

	if (NULL == (fp = g_fopen(tags_file, "r")))
		return FALSE;
	if ((NULL == fgets((gchar*) buf, BUFSIZ, fp)) || ('\0' == *buf))
	{
		fclose(fp);
		return FALSE; /* early out on error */
	}
	else
	{	/* We read the first line for the format specification. */
		if (buf[0] == '#' && strstr((gchar*) buf, "format=pipe") != NULL)
			format = TM_FILE_FORMAT_PIPE;
		else if (buf[0] == '#' && strstr((gchar*) buf, "format=tagmanager") != NULL)
			format = TM_FILE_FORMAT_TAGMANAGER;
		else if (buf[0] == '#' && strstr((gchar*) buf, "format=ctags") != NULL)
			format = TM_FILE_FORMAT_CTAGS;
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
		}
		rewind(fp); /* reset the file pointer, to start reading again from the beginning */
	}
	
	file_tags = g_ptr_array_new();
	while (NULL != (tag = tm_tag_new_from_file(NULL, fp, mode, format)))
		g_ptr_array_add(file_tags, tag);
	fclose(fp);
	
	tm_tags_sort(file_tags, global_tags_sort_attrs, TRUE, TRUE);

	/* reorder the whole array, because tm_tags_find expects a sorted array */
	new_tags = tm_tags_merge(theWorkspace->global_tags, 
		file_tags, global_tags_sort_attrs, TRUE);
	g_ptr_array_free(theWorkspace->global_tags, TRUE);
	g_ptr_array_free(file_tags, TRUE);
	theWorkspace->global_tags = new_tags;

	g_ptr_array_free(theWorkspace->global_typename_array, TRUE);
	theWorkspace->global_typename_array = tm_tags_extract(new_tags, TM_GLOBAL_TYPE_MASK);
	g_ptr_array_free(theWorkspace->global_member_array, TRUE);
	theWorkspace->global_member_array = tm_tags_extract(new_tags, TM_MEMBER_TYPE_MASK);

	return TRUE;
}


static guint tm_file_inode_hash(gconstpointer key)
{
	struct stat file_stat;
	const char *filename = (const char*)key;
	if (g_stat(filename, &file_stat) == 0)
	{
#ifdef TM_DEBUG
		g_message ("Hash for '%s' is '%d'\n", filename, file_stat.st_ino);
#endif
		return g_direct_hash ((gpointer)(gulong)file_stat.st_ino);
	} else {
		return 0;
	}
}


static void tm_move_entries_to_g_list(gpointer key, gpointer value, gpointer user_data)
{
	GList **pp_list = (GList**)user_data;

	if (user_data == NULL)
		return;

	*pp_list = g_list_prepend(*pp_list, value);
}


static void write_includes_file(FILE *fp, GList *includes_files)
{
	GList *node;

	node = includes_files;
	while (node)
	{
		char *str = g_strdup_printf("#include \"%s\"\n", (char*)node->data);
		size_t str_len = strlen(str);

		fwrite(str, str_len, 1, fp);
		g_free(str);
		node = g_list_next(node);
	}
}


static void append_to_temp_file(FILE *fp, GList *file_list)
{
	GList *node;

	node = file_list;
	while (node)
	{
		const char *fname = node->data;
		char *contents;
		size_t length;
		GError *err = NULL;

		if (! g_file_get_contents(fname, &contents, &length, &err))
		{
			fprintf(stderr, "Unable to read file: %s\n", err->message);
			g_error_free(err);
		}
		else
		{
			fwrite(contents, length, 1, fp);
			fwrite("\n", 1, 1, fp);	/* in case file doesn't end in newline (e.g. windows). */
			g_free(contents);
		}
		node = g_list_next (node);
	}
}


static gchar *create_temp_file(const gchar *tpl)
{
	gchar *name;
	gint fd;

	fd = g_file_open_tmp(tpl, &name, NULL);
	if (fd < 0)
		name = NULL;
	else
		close(fd);

	return name;
}


/* Creates a list of global tags. Ideally, this should be created once during
 installations so that all users can use the same file. Thsi is because a full
 scale global tag list can occupy several megabytes of disk space.
 @param pre_process The pre-processing command. This is executed via system(),
 so you can pass stuff like 'gcc -E -dD -P `gnome-config --cflags gnome`'.
 @param includes Include files to process. Wildcards such as '/usr/include/a*.h'
 are allowed.
 @param tags_file The file where the tags will be stored.
 @param lang The language to use for the tags file.
 @return TRUE on success, FALSE on failure.
*/
gboolean tm_workspace_create_global_tags(const char *pre_process, const char **includes,
	int includes_count, const char *tags_file, int lang)
{
#ifdef HAVE_GLOB_H
	glob_t globbuf;
	size_t idx_glob;
#endif
	int idx_inc;
	char *command;
	guint i;
	FILE *fp;
	TMSourceFile *source_file;
	GPtrArray *tags_array;
	GHashTable *includes_files_hash;
	GList *includes_files = NULL;
	gchar *temp_file = create_temp_file("tmp_XXXXXX.cpp");
	gchar *temp_file2 = create_temp_file("tmp_XXXXXX.cpp");

	if (NULL == temp_file || NULL == temp_file2 ||
		NULL == (fp = g_fopen(temp_file, "w")))
	{
		g_free(temp_file);
		g_free(temp_file2);
		return FALSE;
	}

	includes_files_hash = g_hash_table_new_full (tm_file_inode_hash,
												 g_direct_equal,
												 NULL, g_free);

#ifdef HAVE_GLOB_H
	globbuf.gl_offs = 0;

	if (includes[0][0] == '"')	/* leading \" char for glob matching */
	for(idx_inc = 0; idx_inc < includes_count; idx_inc++)
	{
		size_t dirty_len = strlen(includes[idx_inc]);
		char *clean_path = g_malloc(dirty_len - 1);

		strncpy(clean_path, includes[idx_inc] + 1, dirty_len - 1);
		clean_path[dirty_len - 2] = 0;

#ifdef TM_DEBUG
		g_message ("[o][%s]\n", clean_path);
#endif
		glob(clean_path, 0, NULL, &globbuf);

#ifdef TM_DEBUG
		g_message ("matches: %d\n", globbuf.gl_pathc);
#endif

		for(idx_glob = 0; idx_glob < globbuf.gl_pathc; idx_glob++)
		{
#ifdef TM_DEBUG
			g_message (">>> %s\n", globbuf.gl_pathv[idx_glob]);
#endif
			if (!g_hash_table_lookup(includes_files_hash,
									globbuf.gl_pathv[idx_glob]))
			{
				char* file_name_copy = strdup(globbuf.gl_pathv[idx_glob]);
				g_hash_table_insert(includes_files_hash, file_name_copy,
									file_name_copy);
#ifdef TM_DEBUG
				g_message ("Added ...\n");
#endif
			}
		}
		globfree(&globbuf);
		g_free(clean_path);
  	}
  	else
#endif
	/* no glob support or globbing not wanted */
	for(idx_inc = 0; idx_inc < includes_count; idx_inc++)
	{
		if (!g_hash_table_lookup(includes_files_hash,
									includes[idx_inc]))
		{
			char* file_name_copy = strdup(includes[idx_inc]);
			g_hash_table_insert(includes_files_hash, file_name_copy,
								file_name_copy);
		}
  	}

	/* Checks for duplicate file entries which would case trouble */
	g_hash_table_foreach(includes_files_hash, tm_move_entries_to_g_list,
						 &includes_files);

	includes_files = g_list_reverse (includes_files);

#ifdef TM_DEBUG
	g_message ("writing out files to %s\n", temp_file);
#endif
	if (pre_process != NULL)
		write_includes_file(fp, includes_files);
	else
		append_to_temp_file(fp, includes_files);

	g_list_free (includes_files);
	g_hash_table_destroy(includes_files_hash);
	includes_files_hash = NULL;
	includes_files = NULL;
	fclose(fp);

	if (pre_process != NULL)
	{
		gint ret;
		gchar *tmp_errfile = create_temp_file("tmp_XXXXXX");
		gchar *errors = NULL;
		command = g_strdup_printf("%s %s >%s 2>%s",
								pre_process, temp_file, temp_file2, tmp_errfile);
#ifdef TM_DEBUG
		g_message("Executing: %s", command);
#endif
		ret = system(command);
		g_free(command);
		g_unlink(temp_file);
		g_free(temp_file);
		g_file_get_contents(tmp_errfile, &errors, NULL, NULL);
		if (errors && *errors)
			g_printerr("%s", errors);
		g_free(errors);
		g_unlink(tmp_errfile);
		g_free(tmp_errfile);
		if (ret == -1)
		{
			g_unlink(temp_file2);
			return FALSE;
		}
	}
	else
	{
		/* no pre-processing needed, so temp_file2 = temp_file */
		g_unlink(temp_file2);
		g_free(temp_file2);
		temp_file2 = temp_file;
		temp_file = NULL;
	}
	source_file = tm_source_file_new(temp_file2, tm_source_file_get_lang_name(lang));
	update_source_file(source_file, NULL, 0, FALSE, FALSE);
	if (NULL == source_file)
	{
		g_unlink(temp_file2);
		return FALSE;
	}
	g_unlink(temp_file2);
	g_free(temp_file2);
	if (0 == source_file->tags_array->len)
	{
		tm_source_file_free(source_file);
		return FALSE;
	}
	tags_array = tm_tags_extract(source_file->tags_array, tm_tag_max_t);
	if ((NULL == tags_array) || (0 == tags_array->len))
	{
		if (tags_array)
			g_ptr_array_free(tags_array, TRUE);
		tm_source_file_free(source_file);
		return FALSE;
	}
	if (FALSE == tm_tags_sort(tags_array, global_tags_sort_attrs, TRUE, FALSE))
	{
		tm_source_file_free(source_file);
		return FALSE;
	}
	if (NULL == (fp = g_fopen(tags_file, "w")))
	{
		tm_source_file_free(source_file);
		return FALSE;
	}
	fprintf(fp, "# format=tagmanager\n");
	for (i = 0; i < tags_array->len; ++i)
	{
		tm_tag_write(TM_TAG(tags_array->pdata[i]), fp, tm_tag_attr_type_t
		  | tm_tag_attr_scope_t | tm_tag_attr_arglist_t | tm_tag_attr_vartype_t
		  | tm_tag_attr_pointer_t);
	}
	fclose(fp);
	tm_source_file_free(source_file);
	g_ptr_array_free(tags_array, TRUE);
	return TRUE;
}


static gboolean langs_compatible(langType lang, langType other)
{
	if (lang == other || lang == -1 || other == -1)
		return TRUE;
	/* Accept CPP tags for C lang and vice versa */
	else if (lang == TM_PARSER_C && other == TM_PARSER_CPP)
		return TRUE;
	else if (lang == TM_PARSER_CPP && other == TM_PARSER_C)
		return TRUE;

	return FALSE;
}


static guint fill_find_tags_array(GPtrArray *dst, const GPtrArray *src,
	const char *name, const char *scope, TMTagType type, gboolean partial, langType lang)
{
	TMTag **matches;
	guint tagIter;
	guint tagCount;

	if (!src || !dst || !name || !*name)
		return 0;

	matches = tm_tags_find(src, name, partial, &tagCount);
	for (tagIter = 0; tagIter < tagCount; ++tagIter)
	{
		if ((!scope || g_strcmp0((*matches)->scope, scope) == 0) &&
			(type & (*matches)->type) &&
			langs_compatible(lang, (*matches)->lang))
			g_ptr_array_add(dst, *matches);

		matches++;
	}

	return dst->len;
}


/* Returns all matching tags found in the workspace.
 @param name The name of the tag to find.
 @param scope The scope name of the tag to find, or NULL.
 @param type The tag types to return (TMTagType). Can be a bitmask.
 @param attrs The attributes to sort and dedup on (0 terminated integer array).
 @param partial Whether partial match is allowed.
 @param lang Specifies the language(see the table in parsers.h) of the tags to be found,
             -1 for all
 @return Array of matching tags.
*/
GPtrArray *tm_workspace_find(const char *name, const char *scope, TMTagType type,
	TMTagAttrType *attrs, gboolean partial, langType lang)
{
	GPtrArray *tags;
	guint tagCount;

	tags = g_ptr_array_new();

	fill_find_tags_array(tags, theWorkspace->tags_array, name, NULL, type, partial, lang);
	fill_find_tags_array(tags, theWorkspace->global_tags, name, NULL, type, partial, lang);

	if (attrs)
		tm_tags_sort(tags, attrs, TRUE, FALSE);

	return tags;
}


static GPtrArray *
find_scope_members_tags (const GPtrArray * all, const char *scope, langType lang)
{
	GPtrArray *tags = g_ptr_array_new();
	size_t scope_len = strlen (scope);
	gchar sep = lang == TM_PARSER_JAVA ? '.' : ':';
	TMSourceFile *last_file = NULL;
	guint i;

	for (i = 0; i < all->len; ++i)
	{
		TMTag *tag = TM_TAG (all->pdata[i]);

		if (tag && (tag->file == last_file || last_file == NULL) &&
			tag->scope && tag->scope[0] != '\0' &&
			langs_compatible(tag->lang, lang) &&
			strncmp(scope, tag->scope, scope_len) == 0)
		{
			if (tag->scope[scope_len] == '\0' || tag->scope[scope_len] == sep)
			{
				g_ptr_array_add (tags, tag);
				/* once set, always the same thanks to the if above
				 * add only if it's from the same file - this prevents mixing tags from
				 * structs with identical name (typically the anonymous ones) so we
				 * only get members from a single one. For workspace tags it adds
				 * all members because there's no file associated. */
				last_file = tag->file;
			}
		}
	}

	if (tags->len == 0)
	{
		g_ptr_array_free(tags, TRUE);
		return NULL;
	}

	return tags;
}


static GPtrArray *
find_scope_members (const GPtrArray *tags_array, GPtrArray *member_array,
	const char *name, langType lang)
{
	gboolean has_members = FALSE;
	GPtrArray *tags = NULL;
	gchar *type_name;
	guint i;

	g_return_val_if_fail(name && *name, NULL);

	type_name = g_strdup(name);

	/* First check if type_name is a type that can possibly contain members.
	 * Try to resolve intermediate typedefs to get the real type name. Also
	 * add scope information to the name if applicable. The only result of this
	 * part is the updated type_name and boolean has_members.
	 * The loop below loops only when resolving typedefs - avoid possibly infinite
	 * loop when typedefs create a cycle by adding some limits. */
	for (i = 0; i < 5; i++)
	{
		TMTag *tag = NULL;
		GPtrArray *type_tags;
		TMTagType types = (tm_tag_class_t | tm_tag_namespace_t |
						   tm_tag_struct_t | tm_tag_typedef_t |
						   tm_tag_union_t | tm_tag_enum_t);

		type_tags = g_ptr_array_new();
		fill_find_tags_array(type_tags, tags_array, type_name, NULL, types, FALSE, lang);
		if (type_tags)
		{
			guint j;
			for (j = 0; j < type_tags->len; j++)
			{
				tag = TM_TAG(type_tags->pdata[j]);
				/* prefer non-typedef tags because we can be sure they contain members */
				if (tag->type != tm_tag_typedef_t)
					break;
			}
		}

		g_ptr_array_free(type_tags, TRUE);

		if (!tag) /* not a type that can contain members */
			break;

		/* intermediate typedef - resolve to the real type */
		if (tag->type == tm_tag_typedef_t && tag->var_type && tag->var_type[0] != '\0')
		{
			g_free(type_name);
			type_name = g_strdup(tag->var_type);
			continue;
		}
		else /* real type with members */
		{
			has_members = TRUE;
			if (tag->scope && *(tag->scope))
			{
				gchar *tmp_name = type_name;

				if (tag->file && tag->file->lang == TM_PARSER_JAVA)
					type_name = g_strdup_printf("%s.%s", tag->scope, type_name);
				else
					type_name = g_strdup_printf("%s::%s", tag->scope, type_name);
				g_free(tmp_name);
			}
			break;
		}
	}

	if (has_members)
		tags = find_scope_members_tags(member_array, type_name, lang);

	g_free(type_name);

	return tags;
}


static GPtrArray *
find_scope_members_all(GPtrArray *var_array, GPtrArray *searched_array,
		GPtrArray *member_array, langType lang)
{
	GPtrArray *member_tags = NULL;
	guint i;

	/* there may be several variables with the same name - try each of them until
	 * we find something */
	for (i = 0; i < var_array->len; i++)
	{
		TMTag *tag = TM_TAG(var_array->pdata[i]);

		if (tag->var_type)
		{
			member_tags = find_scope_members(searched_array, member_array,
							tag->var_type, lang);
			if (member_tags)
				break;
		}
	}

	return member_tags;
}


/* Returns all matching members tags found in given struct/union/class name.
 @param source_file TMSourceFile of the edited source file or NULL if not available
 @param name Name of the variable whose members are searched
 @return A GPtrArray of TMTag pointers to struct/union/class members or NULL when not found */
GPtrArray *
tm_workspace_find_scope_members (TMSourceFile *source_file, const char *name)
{
	langType lang = source_file ? source_file->lang : -1;
	GPtrArray *tags, *member_tags = NULL;

	tags = tm_workspace_find(name, NULL, tm_tag_max_t, NULL, FALSE, lang);

	if (source_file)
	{
		GPtrArray *file_members = tm_tags_extract(source_file->tags_array, TM_MEMBER_TYPE_MASK);

		member_tags = find_scope_members_all(tags, source_file->tags_array,
							file_members, lang);
		g_ptr_array_free(file_members, TRUE);
	}
	if (!member_tags)
		member_tags = find_scope_members_all(tags, theWorkspace->tags_array,
							theWorkspace->member_array, lang);
	if (!member_tags)
		member_tags = find_scope_members_all(tags, theWorkspace->global_tags,
							theWorkspace->global_member_array, lang);

	g_ptr_array_free(tags, TRUE);

	return member_tags;
}


#ifdef TM_DEBUG

/* Dumps the workspace tree - useful for debugging */
void tm_workspace_dump(void)
{
	guint i;

#ifdef TM_DEBUG
	g_message("Dumping TagManager workspace tree..");
#endif
	for (i=0; i < theWorkspace->source_files->len; ++i)
	{
		TMSourceFile *source_file = theWorkspace->source_files->pdata[i];
		fprintf(stderr, "%s", source_file->file_name);
	}
}
#endif /* TM_DEBUG */


#if 0

/* Returns a list of parent classes for the given class name
 @param name Name of the class
 @return A GPtrArray of TMTag pointers (includes the TMTag for the class) */
static const GPtrArray *tm_workspace_get_parents(const gchar *name)
{
	static TMTagAttrType type[] = { tm_tag_attr_name_t, tm_tag_attr_none_t };
	static GPtrArray *parents = NULL;
	const GPtrArray *matches;
	guint i = 0;
	guint j;
	gchar **klasses;
	gchar **klass;
	TMTag *tag;

	g_return_val_if_fail(name && isalpha(*name),NULL);

	if (NULL == parents)
		parents = g_ptr_array_new();
	else
		g_ptr_array_set_size(parents, 0);
	matches = tm_workspace_find(name, tm_tag_class_t, type, FALSE, -1);
	if ((NULL == matches) || (0 == matches->len))
		return NULL;
	g_ptr_array_add(parents, matches->pdata[0]);
	while (i < parents->len)
	{
		tag = TM_TAG(parents->pdata[i]);
		if ((NULL != tag->inheritance) && (isalpha(tag->inheritance[0])))
		{
			klasses = g_strsplit(tag->inheritance, ",", 10);
			for (klass = klasses; (NULL != *klass); ++ klass)
			{
				for (j=0; j < parents->len; ++j)
				{
					if (0 == strcmp(*klass, TM_TAG(parents->pdata[j])->name))
						break;
				}
				if (parents->len == j)
				{
					matches = tm_workspace_find(*klass, tm_tag_class_t, type, FALSE, -1);
					if ((NULL != matches) && (0 < matches->len))
						g_ptr_array_add(parents, matches->pdata[0]);
				}
			}
			g_strfreev(klasses);
		}
		++ i;
	}
	return parents;
}

#endif
