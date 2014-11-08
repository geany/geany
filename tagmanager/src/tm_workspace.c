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


static TMWorkspace *theWorkspace = NULL;


static gboolean tm_create_workspace(void)
{
	theWorkspace = g_new(TMWorkspace, 1);
	theWorkspace->tags_array = g_ptr_array_new();

	theWorkspace->global_tags = g_ptr_array_new();
	theWorkspace->source_files = g_ptr_array_new();
	theWorkspace->typename_array = g_ptr_array_new();
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
	}
	tm_source_file_parse(source_file, text_buf, buf_size, use_buffer);
	tm_tags_sort(source_file->tags_array, file_tags_sort_attrs, FALSE, TRUE);
	if (update_workspace)
	{
		GPtrArray *sf_typedefs;

#ifdef TM_DEBUG
		g_message("Updating workspace from source file");
#endif
		tm_workspace_merge_tags(&theWorkspace->tags_array, source_file->tags_array);
		
		sf_typedefs = tm_tags_extract(source_file->tags_array, TM_GLOBAL_TYPE_MASK);
		tm_workspace_merge_tags(&theWorkspace->typename_array, sf_typedefs);
		g_ptr_array_free(sf_typedefs, TRUE);
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
}


/** Adds multiple source files to the workspace and updates the workspace tag arrays.
 This is more efficient than calling tm_workspace_add_source_file() and
 tm_workspace_update_source_file() separately for each of the files.
 @param source_files The source files to be added to the workspace.
*/
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


/* Returns all matching tags found in the workspace.
 @param name The name of the tag to find.
 @param type The tag types to return (TMTagType). Can be a bitmask.
 @param attrs The attributes to sort and dedup on (0 terminated integer array).
 @param partial Whether partial match is allowed.
 @param lang Specifies the language(see the table in parsers.h) of the tags to be found,
             -1 for all
 @return Array of matching tags. Do not free() it since it is a static member.
*/
const GPtrArray *tm_workspace_find(const char *name, TMTagType type, TMTagAttrType *attrs,
	gboolean partial, langType lang)
{
	static GPtrArray *tags = NULL;
	TMTag **matches[2];
	size_t len;
	guint tagCount[2]={0,0}, tagIter;
	gint tags_lang;

	if (!name)
		return NULL;
	len = strlen(name);
	if (!len)
		return NULL;
	if (tags)
		g_ptr_array_set_size(tags, 0);
	else
		tags = g_ptr_array_new();

	matches[0] = tm_tags_find(theWorkspace->tags_array, name, partial, TRUE,
					&tagCount[0]);
	matches[1] = tm_tags_find(theWorkspace->global_tags, name, partial, TRUE, &tagCount[1]);

	/* file tags */
	if (matches[0] && *matches[0])
	{
		tags_lang = (*matches[0])->lang;

		for (tagIter=0;tagIter<tagCount[0];++tagIter)
		{
			if ((type & (*matches[0])->type) && (lang == -1 || tags_lang == lang))
				g_ptr_array_add(tags, *matches[0]);
			if (partial)
			{
				if (0 != strncmp((*matches[0])->name, name, len))
					break;
			}
			else
			{
				if (0 != strcmp((*matches[0])->name, name))
					break;
			}
			++ matches[0];
		}
	}

	/* global tags */
	if (matches[1] && *matches[1])
	{
		int tags_lang_alt = 0;
		tags_lang = (*matches[1])->lang;
		/* tags_lang_alt is used to load C global tags only once for C and C++
		 * lang = 1 is C++, lang = 0 is C
		 * if we have lang 0, than accept also lang 1 for C++ */
		if (tags_lang == 0)	/* C or C++ */
			tags_lang_alt = 1;
		else
			tags_lang_alt = tags_lang; /* otherwise just ignore it */

		for (tagIter=0;tagIter<tagCount[1];++tagIter)
		{
			if ((type & (*matches[1])->type) && (lang == -1 ||
				tags_lang == lang || tags_lang_alt == lang))
				g_ptr_array_add(tags, *matches[1]);

			if (partial)
			{
				if (0 != strncmp((*matches[1])->name, name, len))
					break;
			}
			else
			{
				if (0 != strcmp((*matches[1])->name, name))
					break;
			}
			++ matches[1];
		}
	}

	if (attrs)
		tm_tags_sort(tags, attrs, TRUE, FALSE);
	return tags;
}


static gboolean match_langs(gint lang, const TMTag *tag)
{
	if (tag->file)
	{	/* workspace tag */
		if (lang == tag->file->lang)
			return TRUE;
	}
	else
	{	/* global tag */
		if (lang == tag->lang)
			return TRUE;
	}
	return FALSE;
}


/* scope can be NULL.
 * lang can be -1 */
static guint
fill_find_tags_array (GPtrArray *dst, const GPtrArray *src,
					  const char *name, const char *scope, TMTagType type, gboolean partial,
					  gint lang, gboolean first)
{
	TMTag **match;
	guint tagIter, count;

	if ((!src) || (!dst) || (!name) || (!*name))
		return 0;

	match = tm_tags_find (src, name, partial, TRUE, &count);
	if (count && match && *match)
	{
		for (tagIter = 0; tagIter < count; ++tagIter)
		{
			if (! scope || (match[tagIter]->scope &&
				0 == strcmp(match[tagIter]->scope, scope)))
			{
				if (type & match[tagIter]->type)
				if (lang == -1 || match_langs(lang, match[tagIter]))
				{
					g_ptr_array_add (dst, match[tagIter]);
					if (first)
						break;
				}
			}
		}
	}
	return dst->len;
}


/* Returns all matching tags found in the workspace. Adapted from tm_workspace_find, Anjuta 2.02
 @param name The name of the tag to find.
 @param scope The scope name of the tag to find, or NULL.
 @param type The tag types to return (TMTagType). Can be a bitmask.
 @param attrs The attributes to sort and dedup on (0 terminated integer array).
 @param partial Whether partial match is allowed.
 @param lang Specifies the language(see the table in parsers.h) of the tags to be found,
             -1 for all
 @return Array of matching tags. Do not free() it since it is a static member.
*/
const GPtrArray *
tm_workspace_find_scoped (const char *name, const char *scope, TMTagType type,
		TMTagAttrType *attrs, gboolean partial, langType lang, gboolean global_search)
{
	static GPtrArray *tags = NULL;

	if (tags)
		g_ptr_array_set_size (tags, 0);
	else
		tags = g_ptr_array_new ();

	fill_find_tags_array (tags, theWorkspace->tags_array,
						  name, scope, type, partial, lang, FALSE);
	if (global_search)
	{
		/* for a scoped tag, I think we always want the same language */
		fill_find_tags_array (tags, theWorkspace->global_tags,
							  name, scope, type, partial, lang, FALSE);
	}
	if (attrs)
		tm_tags_sort (tags, attrs, TRUE, FALSE);
	return tags;
}


static int
find_scope_members_tags (const GPtrArray * all, GPtrArray * tags,
						 const langType langJava, const char *name,
						 const char *filename, gboolean no_definitions)
{
	GPtrArray *local = g_ptr_array_new ();
	unsigned int i;
	TMTag *tag;
	size_t len = strlen (name);
	for (i = 0; (i < all->len); ++i)
	{
		tag = TM_TAG (all->pdata[i]);
		if (no_definitions && filename && tag->file &&
			0 != strcmp (filename,
						 tag->file->short_name))
		{
			continue;
		}
		if (tag && tag->scope && tag->scope[0] != '\0')
		{
			if (0 == strncmp (name, tag->scope, len))
			{
				g_ptr_array_add (local, tag);
			}
		}
	}
	if (local->len > 0)
	{
		unsigned int j;
		TMTag *tag2;
		char backup = 0;
		char *s_backup = NULL;
		char *var_type = NULL;
		char *scope;
		for (i = 0; (i < local->len); ++i)
		{
			tag = TM_TAG (local->pdata[i]);
			scope = tag->scope;
			if (scope && 0 == strcmp (name, scope))
			{
				g_ptr_array_add (tags, tag);
				continue;
			}
			s_backup = NULL;
			j = 0;				/* someone could write better code :P */
			while (scope)
			{
				if (s_backup)
				{
					backup = s_backup[0];
					s_backup[0] = '\0';
					if (0 == strcmp (name, tag->scope))
					{
						j = local->len;
						s_backup[0] = backup;
						break;
					}
				}
				if (tag->file
					&& tag->file->lang == langJava)
				{
					scope = strrchr (tag->scope, '.');
					if (scope)
						var_type = scope + 1;
				}
				else
				{
					scope = strrchr (tag->scope, ':');
					if (scope)
					{
						var_type = scope + 1;
						scope--;
					}
				}
				if (s_backup)
				{
					s_backup[0] = backup;
				}
				if (scope)
				{
					if (s_backup)
					{
						backup = s_backup[0];
						s_backup[0] = '\0';
					}
					for (j = 0; (j < local->len); ++j)
					{
						if (i == j)
							continue;
						tag2 = TM_TAG (local->pdata[j]);
						if (tag2->var_type &&
							0 == strcmp (var_type, tag2->var_type))
						{
							break;
						}
					}
					if (s_backup)
						s_backup[0] = backup;
				}
				if (j < local->len)
				{
					break;
				}
				s_backup = scope;
			}
			if (j == local->len)
			{
				g_ptr_array_add (tags, tag);
			}
		}
	}
	g_ptr_array_free (local, TRUE);
	return (int) tags->len;
}


/* Returns all matching members tags found in given struct/union/class name.
 @param name Name of the struct/union/class.
 @param file_tags A GPtrArray of edited file TMTag pointers (for search speedup, can be NULL).
 @return A GPtrArray of TMTag pointers to struct/union/class members */
const GPtrArray *
tm_workspace_find_scope_members (const GPtrArray * file_tags, const char *name,
								 gboolean search_global, gboolean no_definitions)
{
	static GPtrArray *tags = NULL;
	GPtrArray *local = NULL;
	char *new_name = (char *) name;
	char *filename = NULL;
	int found = 0, del = 0;
	static langType langJava = -1;
	TMTag *tag = NULL;

	/* FIXME */
	/* langJava = getNamedLanguage ("Java"); */

	g_return_val_if_fail ((theWorkspace && name && name[0] != '\0'), NULL);

	if (!tags)
		tags = g_ptr_array_new ();

	while (1)
	{
		const GPtrArray *tags2;
		guint got = 0;
		TMTagType types = (tm_tag_class_t | tm_tag_namespace_t |
						   tm_tag_struct_t | tm_tag_typedef_t |
						   tm_tag_union_t | tm_tag_enum_t);

		if (file_tags)
		{
			g_ptr_array_set_size (tags, 0);
			got = fill_find_tags_array (tags, file_tags,
										  new_name, NULL, types, FALSE, -1, FALSE);
		}
		if (got)
		{
			tags2 = tags;
		}
		else
		{
			TMTagAttrType attrs[] = {
				tm_tag_attr_name_t, tm_tag_attr_type_t,
				tm_tag_attr_none_t
			};
			tags2 = tm_workspace_find (new_name, types, attrs, FALSE, -1);
		}

		if ((tags2) && (tags2->len == 1) && (tag = TM_TAG (tags2->pdata[0])))
		{
			if (tag->type == tm_tag_typedef_t && tag->var_type
				&& tag->var_type[0] != '\0')
			{
				char *tmp_name;
				tmp_name = tag->var_type;
				if (strcmp(tmp_name, new_name) == 0) {
					new_name = NULL;
				}
				else {
					new_name = tmp_name;
				}
				continue;
			}
			filename = (tag->file ?
						tag->file->short_name : NULL);
			if (tag->scope && tag->scope[0] != '\0')
			{
				del = 1;
				if (tag->file &&
					tag->file->lang == langJava)
				{
					new_name = g_strdup_printf ("%s.%s",
												tag->scope,
												new_name);
				}
				else
				{
					new_name = g_strdup_printf ("%s::%s",
												tag->scope,
												new_name);
				}
			}
			break;
		}
		else
		{
			return NULL;
		}
	}

	g_ptr_array_set_size (tags, 0);

	if (no_definitions && tag && tag->file)
	{
		local = tm_tags_extract (tag->file->tags_array,
								 (tm_tag_function_t | tm_tag_prototype_t |
								  tm_tag_member_t | tm_tag_field_t |
								  tm_tag_method_t | tm_tag_enumerator_t));
	}
	else
	{
		local = tm_tags_extract (theWorkspace->tags_array,
								 (tm_tag_function_t | tm_tag_prototype_t |
								  tm_tag_member_t | tm_tag_field_t |
								  tm_tag_method_t | tm_tag_enumerator_t));
	}
	if (local)
	{
		found = find_scope_members_tags (local, tags, langJava, new_name,
										 filename, no_definitions);
		g_ptr_array_free (local, TRUE);
	}
	if (!found && search_global)
	{
		GPtrArray *global = tm_tags_extract (theWorkspace->global_tags,
											 (tm_tag_member_t |
											  tm_tag_prototype_t |
											  tm_tag_field_t |
											  tm_tag_method_t |
											  tm_tag_function_t |
											  tm_tag_enumerator_t
											  |tm_tag_struct_t | tm_tag_typedef_t |
											  tm_tag_union_t | tm_tag_enum_t));
		if (global)
		{
			find_scope_members_tags (global, tags, langJava, new_name,
									 filename, no_definitions);
			g_ptr_array_free (global, TRUE);
		}
	}
	if (del)
	{
		g_free (new_name);
	}

	return tags;
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

static int
find_namespace_members_tags (const GPtrArray * all, GPtrArray * tags,
						 	const langType langJava, const char *name,
						 	const char *filename)
{
	GPtrArray *local = g_ptr_array_new ();
	unsigned int i;
	TMTag *tag;
	size_t len = strlen (name);

	g_return_val_if_fail (all != NULL, 0);

	for (i = 0; (i < all->len); ++i)
	{
		tag = TM_TAG (all->pdata[i]);
		if (filename && tag->file &&
			0 != strcmp (filename,
						 tag->file->short_name))
		{
			continue;
		}

		if (tag && tag->scope && tag->scope[0] != '\0')
		{
			if (0 == strncmp (name, tag->scope, len))
			{
				g_ptr_array_add (local, tag);
			}
		}
	}

	if (local->len > 0)
	{
		char *scope;
		for (i = 0; (i < local->len); ++i)
		{
			tag = TM_TAG (local->pdata[i]);
			scope = tag->scope;

			/* if we wanna complete something like
			 * namespace1::
			 * we'll just return the tags that have "namespace1"
			 * as their scope. So we won't return classes/members/namespaces
			 * under, for example, namespace2, where namespace1::namespace2
			 */
			if (scope && 0 == strcmp (name, scope))
			{
				g_ptr_array_add (tags, tag);
			}
		}
	}

	g_ptr_array_free (local, TRUE);
	return (int) tags->len;
}

static const GPtrArray *
tm_workspace_find_namespace_members (const GPtrArray * file_tags, const char *name,
								 gboolean search_global)
{
	static GPtrArray *tags = NULL;
	GPtrArray *local = NULL;
	char *new_name = (char *) name;
	char *filename = NULL;
	int found = 0, del = 0;
	static langType langJava = -1;
	TMTag *tag = NULL;

	g_return_val_if_fail (name && name[0] != '\0', NULL);

	if (!tags)
		tags = g_ptr_array_new ();

	while (1)
	{
		const GPtrArray *tags2;
		guint got = 0;
		TMTagType types = (tm_tag_class_t
						   tm_tag_struct_t | tm_tag_typedef_t |
						   tm_tag_union_t | tm_tag_enum_t);

		if (file_tags)
		{
			g_ptr_array_set_size (tags, 0);
			got = fill_find_tags_array (tags, file_tags,
										  new_name, NULL, types, FALSE, -1, FALSE);
		}


		if (got)
		{
			tags2 = tags;
		}
		else
		{
			TMTagAttrType attrs[] = {
				tm_tag_attr_name_t, tm_tag_attr_type_t,
				tm_tag_attr_none_t
			};
			tags2 = tm_workspace_find (new_name, types, attrs, FALSE, -1);
		}

		if ((tags2) && (tags2->len == 1) && (tag = TM_TAG (tags2->pdata[0])))
		{
			if (tag->type == tm_tag_typedef_t && tag->var_type
				&& tag->var_type[0] != '\0')
			{
				new_name = tag->var_type;
				continue;
			}
			filename = (tag->file ?
						tag->file->short_name : NULL);
			if (tag->scope && tag->scope[0] != '\0')
			{
				del = 1;
				if (tag->file &&
					tag->file->lang == langJava)
				{
					new_name = g_strdup_printf ("%s.%s",
												tag->scope,
												new_name);
				}
				else
				{
					new_name = g_strdup_printf ("%s::%s",
												tag->scope,
												new_name);
				}
			}
			break;
		}
		else
		{
			return NULL;
		}
	}

	g_ptr_array_set_size (tags, 0);

	if (tag && tag->file)
	{
		local = tm_tags_extract (tag->file->tags_array,
								 (tm_tag_function_t |
								  tm_tag_field_t | tm_tag_enumerator_t |
								  tm_tag_namespace_t | tm_tag_class_t ));
	}
	else
	{
		local = tm_tags_extract (theWorkspace->tags_array,
								 (tm_tag_function_t | tm_tag_prototype_t |
								  tm_tag_member_t |
								  tm_tag_field_t | tm_tag_enumerator_t |
								  tm_tag_namespace_t | tm_tag_class_t ));
	}

	if (local)
	{
		found = find_namespace_members_tags (local, tags,
										 langJava, new_name, filename);
		g_ptr_array_free (local, TRUE);
	}


	if (!found && search_global)
	{
		GPtrArray *global = tm_tags_extract (theWorkspace->global_tags,
											 (tm_tag_member_t |
											  tm_tag_prototype_t |
											  tm_tag_field_t |
											  tm_tag_method_t |
											  tm_tag_function_t |
											  tm_tag_enumerator_t |
											  tm_tag_namespace_t |
											  tm_tag_class_t ));

		if (global)
		{
			find_namespace_members_tags (global, tags, langJava,
									 new_name, filename);
/*/
			DEBUG_PRINT ("returning these");
  		    gint i;
			for (i=0; i < tags->len; i++) {
				TMTag *cur_tag;

				cur_tag = (TMTag*)g_ptr_array_index (tags, i);
				tm_tag_print (cur_tag, stdout );
			}
/*/
			g_ptr_array_free (global, TRUE);
		}
	}


	if (del)
	{
		g_free (new_name);
	}

	return tags;
}


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
