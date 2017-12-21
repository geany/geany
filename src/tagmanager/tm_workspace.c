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
#include "tm_ctags_wrappers.h"
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

static TMTagType TM_TYPE_WITH_MEMBERS =
	tm_tag_class_t | tm_tag_struct_t | tm_tag_union_t |
	tm_tag_enum_t | tm_tag_interface_t;

static TMTagType TM_GLOBAL_TYPE_MASK =
	tm_tag_class_t | tm_tag_enum_t | tm_tag_interface_t |
	tm_tag_struct_t | tm_tag_typedef_t | tm_tag_union_t | tm_tag_namespace_t;

static TMWorkspace *theWorkspace = NULL;


static gboolean tm_create_workspace(void)
{
	theWorkspace = g_new(TMWorkspace, 1);
	theWorkspace->tags_array = g_ptr_array_new();

	theWorkspace->global_tags = g_ptr_array_new();
	theWorkspace->source_files = g_ptr_array_new();
	theWorkspace->typename_array = g_ptr_array_new();
	theWorkspace->global_typename_array = g_ptr_array_new();

	tm_ctags_init();
	tm_parser_verify_type_mappings();

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
 information is also lost.
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

	g_ptr_array_free(theWorkspace->typename_array, TRUE);
	theWorkspace->typename_array = tm_tags_extract(theWorkspace->tags_array, TM_GLOBAL_TYPE_MASK);
}


/** Adds multiple source files to the workspace and updates the workspace tag arrays.
 This is more efficient than calling tm_workspace_add_source_file() and
 tm_workspace_update_source_file() separately for each of the files.
 @param source_files @elementtype{TMSourceFile} The source files to be added to the workspace.
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
 @param source_files @elementtype{TMSourceFile} The source files to be removed from the workspace.
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
gboolean tm_workspace_load_global_tags(const char *tags_file, TMParserType mode)
{
	GPtrArray *file_tags, *new_tags;

	file_tags = tm_source_file_read_tags_file(tags_file, mode);
	if (!file_tags)
		return FALSE;

	tm_tags_sort(file_tags, global_tags_sort_attrs, TRUE, TRUE);

	/* reorder the whole array, because tm_tags_find expects a sorted array */
	new_tags = tm_tags_merge(theWorkspace->global_tags, 
		file_tags, global_tags_sort_attrs, TRUE);
	g_ptr_array_free(theWorkspace->global_tags, TRUE);
	g_ptr_array_free(file_tags, TRUE);
	theWorkspace->global_tags = new_tags;

	g_ptr_array_free(theWorkspace->global_typename_array, TRUE);
	theWorkspace->global_typename_array = tm_tags_extract(new_tags, TM_GLOBAL_TYPE_MASK);

	return TRUE;
}


static guint tm_file_inode_hash(gconstpointer key)
{
	GStatBuf file_stat;
	const char *filename = (const char*)key;

	if (g_stat(filename, &file_stat) == 0)
	{
#ifdef TM_DEBUG
		g_message ("Hash for '%s' is '%d'\n", filename, file_stat.st_ino);
#endif
		return g_direct_hash ((gpointer)(intptr_t)file_stat.st_ino);
	}

	return 0;
}


static void tm_move_entries_to_g_list(gpointer key, gpointer value, gpointer user_data)
{
	GList **pp_list = (GList**)user_data;

	if (user_data == NULL)
		return;

	*pp_list = g_list_prepend(*pp_list, g_strdup(value));
}


static gboolean write_includes_file(const gchar *outf, GList *includes_files)
{
	FILE *fp = g_fopen(outf, "w");
	GList *node = includes_files;

	if (!fp)
		return FALSE;

	while (node)
	{
		char *str = g_strdup_printf("#include \"%s\"\n", (char*)node->data);
		size_t str_len = strlen(str);

		fwrite(str, str_len, 1, fp);
		g_free(str);
		node = g_list_next(node);
	}

	return fclose(fp) == 0;
}


static gboolean combine_include_files(const gchar *outf, GList *file_list)
{
	FILE *fp = g_fopen(outf, "w");
	GList *node = file_list;

	if (!fp)
		return FALSE;

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

	return fclose(fp) == 0;
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

static GList *lookup_includes(const gchar **includes, gint includes_count)
{
	GList *includes_files = NULL;
	GHashTable *table;
	gint i;
#ifdef HAVE_GLOB_H
	glob_t globbuf;
	size_t idx_glob;
#endif

	table = g_hash_table_new_full(tm_file_inode_hash, g_direct_equal, NULL, g_free);

#ifdef HAVE_GLOB_H
	globbuf.gl_offs = 0;

	if (includes[0][0] == '"')	/* leading \" char for glob matching */
	{
		for (i = 0; i < includes_count; i++)
		{
			size_t dirty_len = strlen(includes[i]);
			gchar *clean_path;

			if (dirty_len < 2)
				continue;

			clean_path = g_malloc(dirty_len - 1);

			strncpy(clean_path, includes[i] + 1, dirty_len - 1);
			clean_path[dirty_len - 2] = 0;

#ifdef TM_DEBUG
			g_message ("[o][%s]\n", clean_path);
#endif
			glob(clean_path, 0, NULL, &globbuf);

#ifdef TM_DEBUG
			g_message ("matches: %d\n", globbuf.gl_pathc);
#endif

			for (idx_glob = 0; idx_glob < globbuf.gl_pathc; idx_glob++)
			{
#ifdef TM_DEBUG
				g_message (">>> %s\n", globbuf.gl_pathv[idx_glob]);
#endif
				if (!g_hash_table_lookup(table, globbuf.gl_pathv[idx_glob]))
				{
					gchar *file_name_copy = g_strdup(globbuf.gl_pathv[idx_glob]);

					g_hash_table_insert(table, file_name_copy, file_name_copy);
#ifdef TM_DEBUG
					g_message ("Added ...\n");
#endif
				}
			}
			globfree(&globbuf);
			g_free(clean_path);
		}
	}
	else
#endif /* HAVE_GLOB_H */
	{
		/* no glob support or globbing not wanted */
		for (i = 0; i < includes_count; i++)
		{
			if (!g_hash_table_lookup(table, includes[i]))
			{
				gchar* file_name_copy = g_strdup(includes[i]);

				g_hash_table_insert(table, file_name_copy, file_name_copy);
			}
		}
	}

	g_hash_table_foreach(table, tm_move_entries_to_g_list, &includes_files);
	g_hash_table_destroy(table);

	return includes_files;
}

static gchar *pre_process_file(const gchar *cmd, const gchar *inf)
{
	gint ret;
	gchar *outf = create_temp_file("tmp_XXXXXX.cpp");
	gchar *tmp_errfile;
	gchar *errors = NULL;
	gchar *command;

	if (!outf)
		return NULL;

	tmp_errfile = create_temp_file("tmp_XXXXXX");
	if (!tmp_errfile)
	{
		g_unlink(outf);
		g_free(outf);
		return NULL;
	}

	command = g_strdup_printf("%s %s >%s 2>%s",
		cmd, inf, outf, tmp_errfile);
#ifdef TM_DEBUG
	g_message("Executing: %s", command);
#endif
	ret = system(command);
	g_free(command);

	g_file_get_contents(tmp_errfile, &errors, NULL, NULL);
	if (errors && *errors)
		g_printerr("%s", errors);
	g_free(errors);
	g_unlink(tmp_errfile);
	g_free(tmp_errfile);

	if (ret == -1)
	{
		g_unlink(outf);
		g_free(outf);
		return NULL;
	}

	return outf;
}

/* Creates a list of global tags. Ideally, this should be created once during
 installations so that all users can use the same file. This is because a full
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
	int includes_count, const char *tags_file, TMParserType lang)
{
	gboolean ret = FALSE;
	TMSourceFile *source_file;
	GList *includes_files;
	gchar *temp_file = create_temp_file("tmp_XXXXXX.cpp");

	if (!temp_file)
		return FALSE;

	includes_files = lookup_includes(includes, includes_count);

#ifdef TM_DEBUG
	g_message ("writing out files to %s\n", temp_file);
#endif
	if (pre_process)
		ret = write_includes_file(temp_file, includes_files);
	else
		ret = combine_include_files(temp_file, includes_files);

	g_list_free_full(includes_files, g_free);
	if (!ret)
		goto cleanup;
	ret = FALSE;

	if (pre_process)
	{
		gchar *temp_file2 = pre_process_file(pre_process, temp_file);

		if (temp_file2)
		{
			g_unlink(temp_file);
			g_free(temp_file);
			temp_file = temp_file2;
		}
		else
			goto cleanup;
	}

	source_file = tm_source_file_new(temp_file, tm_source_file_get_lang_name(lang));
	if (!source_file)
		goto cleanup;
	update_source_file(source_file, NULL, 0, FALSE, FALSE);
	if (source_file->tags_array->len == 0)
	{
		tm_source_file_free(source_file);
		goto cleanup;
	}

	tm_tags_sort(source_file->tags_array, global_tags_sort_attrs, TRUE, FALSE);
	ret = tm_source_file_write_tags_file(tags_file, source_file->tags_array);
	tm_source_file_free(source_file);

cleanup:
	g_unlink(temp_file);
	g_free(temp_file);
	return ret;
}


static void fill_find_tags_array(GPtrArray *dst, const GPtrArray *src,
	const char *name, const char *scope, TMTagType type, TMParserType lang)
{
	TMTag **tag;
	guint i, num;

	if (!src || !dst || !name || !*name)
		return;

	tag = tm_tags_find(src, name, FALSE, &num);
	for (i = 0; i < num; ++i)
	{
		if ((type & (*tag)->type) &&
			tm_parser_langs_compatible(lang, (*tag)->lang) &&
			(!scope || g_strcmp0((*tag)->scope, scope) == 0))
		{
			g_ptr_array_add(dst, *tag);
		}
		tag++;
	}
}


/* Returns all matching tags found in the workspace.
 @param name The name of the tag to find.
 @param scope The scope name of the tag to find, or NULL.
 @param type The tag types to return (TMTagType). Can be a bitmask.
 @param attrs The attributes to sort and dedup on (0 terminated integer array).
 @param lang Specifies the language(see the table in parsers.h) of the tags to be found,
             -1 for all
 @return Array of matching tags.
*/
GPtrArray *tm_workspace_find(const char *name, const char *scope, TMTagType type,
	TMTagAttrType *attrs, TMParserType lang)
{
	GPtrArray *tags = g_ptr_array_new();

	fill_find_tags_array(tags, theWorkspace->tags_array, name, scope, type, lang);
	fill_find_tags_array(tags, theWorkspace->global_tags, name, scope, type, lang);

	if (attrs)
		tm_tags_sort(tags, attrs, TRUE, FALSE);

	return tags;
}


static void fill_find_tags_array_prefix(GPtrArray *dst, const GPtrArray *src,
	const char *name, TMParserType lang, guint max_num)
{
	TMTag **tag, *last = NULL;
	guint i, count, num;

	if (!src || !dst || !name || !*name)
		return;

	num = 0;
	tag = tm_tags_find(src, name, TRUE, &count);
	for (i = 0; i < count && num < max_num; ++i)
	{
		if (tm_parser_langs_compatible(lang, (*tag)->lang) &&
			!tm_tag_is_anon(*tag) &&
			(!last || g_strcmp0(last->name, (*tag)->name) != 0))
		{
			g_ptr_array_add(dst, *tag);
			last = *tag;
			num++;
		}
		tag++;
	}
}


/* Returns tags with the specified prefix sorted by name. If there are several
 tags with the same name, only one of them appears in the resulting array.
 @param prefix The prefix of the tag to find.
 @param lang Specifies the language(see the table in parsers.h) of the tags to be found,
             -1 for all.
 @param max_num The maximum number of tags to return.
 @return Array of matching tags sorted by their name.
*/
GPtrArray *tm_workspace_find_prefix(const char *prefix, TMParserType lang, guint max_num)
{
	TMTagAttrType attrs[] = { tm_tag_attr_name_t, 0 };
	GPtrArray *tags = g_ptr_array_new();

	fill_find_tags_array_prefix(tags, theWorkspace->tags_array, prefix, lang, max_num);
	fill_find_tags_array_prefix(tags, theWorkspace->global_tags, prefix, lang, max_num);

	tm_tags_sort(tags, attrs, TRUE, FALSE);
	if (tags->len > max_num)
		tags->len = max_num;

	return tags;
}


/* Gets all members of type_tag; search them inside the all array.
 * The namespace parameter determines whether we are performing the "namespace"
 * search (user has typed something like "A::" where A is a type) or "scope" search
 * (user has typed "a." where a is a global struct-like variable). With the
 * namespace search we return all direct descendants of any type while with the
 * scope search we return only those which can be invoked on a variable (member,
 * method, etc.). */
static GPtrArray *
find_scope_members_tags (const GPtrArray *all, TMTag *type_tag, gboolean namespace)
{
	TMTagType member_types = tm_tag_max_t & ~(TM_TYPE_WITH_MEMBERS | tm_tag_typedef_t);
	GPtrArray *tags = g_ptr_array_new();
	gchar *scope;
	guint i;

	if (namespace)
		member_types = tm_tag_max_t;

	if (type_tag->scope && *(type_tag->scope))
		scope = g_strconcat(type_tag->scope, tm_parser_context_separator(type_tag->lang), type_tag->name, NULL);
	else
		scope = g_strdup(type_tag->name);

	for (i = 0; i < all->len; ++i)
	{
		TMTag *tag = TM_TAG (all->pdata[i]);

		if (tag && (tag->type & member_types) &&
			tag->scope && tag->scope[0] != '\0' &&
			tm_parser_langs_compatible(tag->lang, type_tag->lang) &&
			strcmp(scope, tag->scope) == 0 &&
			(!namespace || !tm_tag_is_anon(tag)))
		{
			g_ptr_array_add (tags, tag);
		}
	}

	g_free(scope);

	if (tags->len == 0)
	{
		g_ptr_array_free(tags, TRUE);
		return NULL;
	}

	return tags;
}


static gchar *strip_type(const gchar *scoped_name, TMParserType lang)
{
	if (scoped_name != NULL)
	{
		/* remove scope prefix */
		const gchar *sep = tm_parser_context_separator(lang);
		const gchar *base = g_strrstr(scoped_name, sep);
		gchar *name = base ? g_strdup(base + strlen(sep)) : g_strdup(scoped_name);

		/* remove pointers */
		g_strdelimit(name, "*^", ' ');
		g_strstrip(name);

		return name;
	}
	return NULL;
}


/* Gets all members of the type with the given name; search them inside tags_array */
static GPtrArray *
find_scope_members (const GPtrArray *tags_array, const gchar *name, TMSourceFile *file,
	TMParserType lang, gboolean namespace)
{
	GPtrArray *res = NULL;
	gchar *type_name;
	guint i;

	g_return_val_if_fail(name && *name, NULL);

	type_name = g_strdup(name);

	/* Check if type_name is a type that can possibly contain members.
	 * Try to resolve intermediate typedefs to get the real type name. Also
	 * add scope information to the name if applicable.
	 * The loop below loops only when resolving typedefs - avoid possibly infinite
	 * loop when typedefs create a cycle by adding some limits. */
	for (i = 0; i < 5; i++)
	{
		guint j;
		GPtrArray *type_tags;
		TMTagType types = TM_TYPE_WITH_MEMBERS | tm_tag_typedef_t;
		TMTag *tag = NULL;

		if (!namespace)
			types &= ~tm_tag_enum_t;

		type_tags = g_ptr_array_new();
		fill_find_tags_array(type_tags, tags_array, type_name, NULL, types, lang);

		for (j = 0; j < type_tags->len; j++)
		{
			TMTag *test_tag = TM_TAG(type_tags->pdata[j]);

			/* anonymous type defined in a different file than the variable -
			 * this isn't the type we are looking for */
			if (tm_tag_is_anon(test_tag) && (file != test_tag->file || test_tag->file == NULL))
				continue;

			tag = test_tag;

			/* prefer non-typedef tags because we can be sure they contain members */
			if (test_tag->type != tm_tag_typedef_t)
				break;
		}

		g_ptr_array_free(type_tags, TRUE);

		if (!tag) /* not a type that can contain members */
			break;

		/* intermediate typedef - resolve to the real type */
		if (tag->type == tm_tag_typedef_t)
		{
			if (tag->var_type && tag->var_type[0] != '\0')
			{
				g_free(type_name);
				type_name = strip_type(tag->var_type, tag->lang);
				file = tag->file;
				continue;
			}
			break;
		}
		else /* real type with members */
		{
			/* use the same file as the composite type if file information available */
			res = find_scope_members_tags(tag->file ? tag->file->tags_array : tags_array, tag, namespace);
			break;
		}
	}

	g_free(type_name);

	return res;
}


/* Checks whether a member tag is directly accessible from method with method_scope */
static gboolean member_at_method_scope(const GPtrArray *tags, const gchar *method_scope, TMTag *member_tag,
	TMParserType lang)
{
	const gchar *sep = tm_parser_context_separator(lang);
	gboolean ret = FALSE;
	gchar **comps;
	guint len;

	/* method scope is in the form ...::class_name::method_name */
	comps = g_strsplit (method_scope, sep, 0);
	len = g_strv_length(comps);
	if (len > 1)
	{
		gchar *method, *member_scope, *cls, *cls_scope;

		/* get method/member scope */
		method = comps[len - 1];
		comps[len - 1] = NULL;
		member_scope = g_strjoinv(sep, comps);
		comps[len - 1] = method;

		/* get class scope */
		cls = comps[len - 2];
		comps[len - 2] = NULL;
		cls_scope = g_strjoinv(sep, comps);
		comps[len - 2] = cls;
		cls_scope = strlen(cls_scope) > 0 ? cls_scope : NULL;

		/* check whether member inside the class */
		if (g_strcmp0(member_tag->scope, member_scope) == 0)
		{
			const GPtrArray *src = member_tag->file ? member_tag->file->tags_array : tags;
			GPtrArray *cls_tags = g_ptr_array_new();

			/* check whether the class exists */
			fill_find_tags_array(cls_tags, src, cls, cls_scope, TM_TYPE_WITH_MEMBERS | tm_tag_namespace_t, lang);
			ret = cls_tags->len > 0;
			g_ptr_array_free(cls_tags, TRUE);
		}

		g_free(cls_scope);
		g_free(member_scope);
	}

	g_strfreev(comps);
	return ret;
}


/* For an array of variable/type tags, find members inside the types */
static GPtrArray *
find_scope_members_all(const GPtrArray *tags, const GPtrArray *searched_array, TMParserType lang,
	gboolean member, const gchar *current_scope)
{
	GPtrArray *member_tags = NULL;
	guint i;

	/* there may be several variables/types with the same name - try each of them until
	 * we find something */
	for (i = 0; i < tags->len && !member_tags; i++)
	{
		TMTag *tag = TM_TAG(tags->pdata[i]);
		TMTagType member_types = tm_tag_member_t | tm_tag_field_t | tm_tag_method_t;
		TMTagType types = TM_TYPE_WITH_MEMBERS | tm_tag_typedef_t;

		if (tag->type & types)  /* type: namespace search */
		{
			if (tag->type & tm_tag_typedef_t)
				member_tags = find_scope_members(searched_array, tag->name, tag->file, lang, TRUE);
			else
				member_tags = find_scope_members_tags(tag->file ? tag->file->tags_array : searched_array,
					tag, TRUE);
		}
		else if (tag->var_type)  /* variable: scope search */
		{
			/* The question now is whether we should use member tags (such as
			 * tm_tag_field_t, tm_tag_member_t) or not. We want them if member==TRUE
			 * (which means user has typed something like foo.bar.) or if we are
			 * inside a method where foo is a class member, we want scope completion
			 * for foo. */
			if (!(tag->type & member_types) || member ||
				member_at_method_scope(tags, current_scope, tag, lang))
			{
				gchar *tag_type = strip_type(tag->var_type, tag->lang);

				member_tags = find_scope_members(searched_array, tag_type, tag->file, lang, FALSE);
				g_free(tag_type);
			}
		}
	}

	return member_tags;
}


static GPtrArray *find_namespace_members_all(const GPtrArray *tags, const GPtrArray *searched_array, TMParserType lang)
{
	GPtrArray *member_tags = NULL;
	guint i;

	for (i = 0; i < tags->len && !member_tags; i++)
	{
		TMTag *tag = TM_TAG(tags->pdata[i]);

		member_tags = find_scope_members_tags(searched_array, tag, TRUE);
	}

	return member_tags;
}


/* Returns all member tags of a struct/union/class if the provided name is a variable
 of such a type or the name of the type.
 @param source_file TMSourceFile of the edited source file
 @param name Name of the variable/type whose members are searched
 @param function TRUE if the name is a name of a function
 @param member TRUE if invoked on class/struct member (e.g. after the last dot in foo.bar.)
 @param current_scope The current scope in the editor
 @param search_namespace Whether to search the contents of namespace (e.g. after MyNamespace::)
 @return A GPtrArray of TMTag pointers to struct/union/class members or NULL when not found */
GPtrArray *
tm_workspace_find_scope_members (TMSourceFile *source_file, const char *name,
	gboolean function, gboolean member, const gchar *current_scope, gboolean search_namespace)
{
	TMParserType lang = source_file ? source_file->lang : TM_PARSER_NONE;
	GPtrArray *tags, *member_tags = NULL;
	TMTagType function_types = tm_tag_function_t | tm_tag_method_t |
		tm_tag_macro_with_arg_t | tm_tag_prototype_t;
	TMTagType tag_type = tm_tag_max_t &
		~(function_types | tm_tag_enumerator_t | tm_tag_namespace_t | tm_tag_package_t);
	TMTagAttrType sort_attr[] = {tm_tag_attr_name_t, 0};

	if (search_namespace)
	{
		tags = tm_workspace_find(name, NULL, tm_tag_namespace_t, NULL, lang);

		member_tags = find_namespace_members_all(tags, theWorkspace->tags_array, lang);
		if (!member_tags)
			member_tags = find_namespace_members_all(tags, theWorkspace->global_tags, lang);

		g_ptr_array_free(tags, TRUE);
	}

	if (!member_tags)
	{
		if (function)
			tag_type = function_types;

		/* tags corresponding to the variable/type name */
		tags = tm_workspace_find(name, NULL, tag_type, NULL, lang);

		/* Start searching inside the source file, continue with workspace tags and
		 * end with global tags. This way we find the "closest" tag to the current
		 * file in case there are more of them. */
		if (source_file)
			member_tags = find_scope_members_all(tags, source_file->tags_array,
												 lang, member, current_scope);
		if (!member_tags)
			member_tags = find_scope_members_all(tags, theWorkspace->tags_array, lang,
												 member, current_scope);
		if (!member_tags)
			member_tags = find_scope_members_all(tags, theWorkspace->global_tags, lang,
												 member, current_scope);

		g_ptr_array_free(tags, TRUE);
	}

	if (member_tags)
		tm_tags_dedup(member_tags, sort_attr, FALSE);

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
	matches = tm_workspace_find(name, NULL, tm_tag_class_t, type, -1);
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
					matches = tm_workspace_find(*klass, NULL, tm_tag_class_t, type, -1);
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
