/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#include "general.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FNMATCH_H
# include <fnmatch.h>
#endif
#include <glib/gstdio.h>

#include "tm_work_object.h"
#include "tm_file_entry.h"


#define FILE_NEW(T)		((T) = g_slice_new0(TMFileEntry))
#define FILE_FREE(T)	g_slice_free(TMFileEntry, (T))


void tm_file_entry_print(TMFileEntry *entry, gpointer __unused__ user_data
  , guint level)
{
	guint i;

	g_return_if_fail(entry);
	for (i=0; i < level; ++i)
		fputc('\t', stderr);
	fprintf(stderr, "%s\n", entry->name);
}

gint tm_file_entry_compare(TMFileEntry *e1, TMFileEntry *e2)
{
	g_return_val_if_fail(e1 && e2 && e1->name && e2->name, 0);
#ifdef TM_DEBUG
	g_message("Comparing %s and %s", e1->name, e2->name);
#endif
	return strcmp(e1->name, e2->name);
}

/* TTimo - modified to handle symlinks */
static TMFileType tm_file_entry_type(const char *path)
{
	struct stat s;

#ifndef G_OS_WIN32
	if (0 != g_lstat(path, &s))
		return tm_file_unknown_t;
#endif
	if S_ISDIR(s.st_mode)
		return tm_file_dir_t;
#ifndef G_OS_WIN32
	else if (S_ISLNK(s.st_mode))
		return tm_file_link_t;
#endif
	else if S_ISREG(s.st_mode)
		return tm_file_regular_t;
	else
		return tm_file_unknown_t;
}

static gboolean apply_filter(const char *name, GList *match, GList *unmatch
  , gboolean ignore_hidden)
{
	GList *tmp;
	gboolean matched = (match == NULL);
	g_return_val_if_fail(name, FALSE);
	if (ignore_hidden && ('.' == name[0]))
		return FALSE;
	/* TTimo - ignore .svn directories */
	if (!strcmp(name, ".svn"))
		return FALSE;
	for (tmp = match; tmp; tmp = g_list_next(tmp))
	{
		if (0 == fnmatch((char *) tmp->data, name, 0))
		{
			matched = TRUE;
			break;
		}
	}
	if (!matched)
		return FALSE;
	for (tmp = unmatch; tmp; tmp = g_list_next(tmp))
	{
		if (0 == fnmatch((char *) tmp->data, name, 0))
		{
			return FALSE;
		}
	}
	return matched;
}

TMFileEntry *tm_file_entry_new(const char *path, TMFileEntry *parent
  , gboolean recurse, GList *file_match, GList *file_unmatch
  , GList *dir_match, GList *dir_unmatch, gboolean ignore_hidden_files
  , gboolean ignore_hidden_dirs)
{
	TMFileEntry *entry;
	/* GList *tmp; */
	char *real_path;
	DIR *dir;
	struct dirent *dir_entry;
	TMFileEntry *new_entry;
	char *file_name;
	struct stat s;
	char *entries = NULL;

	g_return_val_if_fail (path != NULL, NULL);

	/* TTimo - don't follow symlinks */
	if (tm_file_entry_type(path) == tm_file_link_t)
		return NULL;
	real_path = tm_get_real_path(path);
	if (!real_path)
		return NULL;
	FILE_NEW(entry);
	entry->type = tm_file_entry_type(real_path);
	entry->parent = parent;
	entry->path = real_path;
	entry->name = strrchr(entry->path, '/');
	if (entry->name)
		++ (entry->name);
	else
		entry->name = entry->path;
	switch(entry->type)
	{
		case tm_file_unknown_t:
			g_free(real_path);
			FILE_FREE(entry);
			return NULL;
		case tm_file_link_t:
		case tm_file_regular_t:
			if (parent && !apply_filter(entry->name, file_match, file_unmatch
			  , ignore_hidden_files))
			{
				tm_file_entry_free(entry);
				return NULL;
			}
			break;
		case tm_file_dir_t:
			if (parent && !(recurse && apply_filter(entry->name, dir_match
			  , dir_unmatch, ignore_hidden_dirs)))
			{
				tm_file_entry_free(entry);
				return NULL;
			}
			file_name = g_strdup_printf("%s/CVS/Entries", entry->path);
			if (0 == g_stat(file_name, &s))
			{
				if (S_ISREG(s.st_mode))
				{
					int fd;
					entries = g_new(char, s.st_size + 2);
					if (0 > (fd = open(file_name, O_RDONLY)))
					{
						g_free(entries);
						entries = NULL;
					}
					else
					{
						off_t n =0;
						off_t total_read = 1;
						while (0 < (n = read(fd, entries + total_read, s.st_size - total_read)))
							total_read += n;
						entries[s.st_size] = '\0';
						entries[0] = '\n';
						close(fd);
						entry->version = g_strdup("D");
					}
				}
			}
			g_free(file_name);
			if (NULL != (dir = opendir(entry->path)))
			{
				while (NULL != (dir_entry = readdir(dir)))
				{
					if ((0 == strcmp(dir_entry->d_name, "."))
					  || (0 == strcmp(dir_entry->d_name, "..")))
						continue;
					file_name = g_strdup_printf("%s/%s", entry->path, dir_entry->d_name);
					new_entry = tm_file_entry_new(file_name, entry, recurse
					  , file_match, file_unmatch, dir_match, dir_unmatch
			  		  , ignore_hidden_files, ignore_hidden_dirs);
					g_free(file_name);
					if (new_entry)
					{
						if (entries)
						{
							char *str = g_strconcat("\n/", new_entry->name, "/", NULL);
							char *name_pos = strstr(entries, str);
							if (NULL != name_pos)
							{
								int len = strlen(str);
								char *version_pos = strchr(name_pos + len, '/');
								if (NULL != version_pos)
								{
									*version_pos = '\0';
									new_entry->version = g_strdup(name_pos + len);
									*version_pos = '/';
								}
							}
							g_free(str);
						}
						entry->children = g_slist_prepend(entry->children, new_entry);
					}
				}
			}
			closedir(dir);
			entry->children = g_slist_sort(entry->children, (GCompareFunc) tm_file_entry_compare);
			g_free(entries);
			break;
	}
	return entry;
}

void tm_file_entry_free(gpointer entry)
{
	if (entry)
	{
		TMFileEntry *file_entry = TM_FILE_ENTRY(entry);
		if (file_entry->children)
		{
			GSList *tmp;
			for (tmp = file_entry->children; tmp; tmp = g_slist_next(tmp))
				tm_file_entry_free(tmp->data);
			g_slist_free(file_entry->children);
		}
		g_free(file_entry->version);
		g_free(file_entry->path);
		FILE_FREE(file_entry);
	}
}

void tm_file_entry_foreach(TMFileEntry *entry, TMFileEntryFunc func
  , gpointer user_data, guint level, gboolean reverse)
{
	g_return_if_fail (entry != NULL);
	g_return_if_fail (func != NULL);

	if ((reverse) && (entry->children))
	{
		GSList *tmp;
		for (tmp = entry->children; tmp; tmp = g_slist_next(tmp))
			tm_file_entry_foreach(TM_FILE_ENTRY(tmp->data), func
			  , user_data, level + 1, TRUE);
	}
	func(entry, user_data, level);
	if ((!reverse) && (entry->children))
	{
		GSList *tmp;
		for (tmp = entry->children; tmp; tmp = g_slist_next(tmp))
			tm_file_entry_foreach(TM_FILE_ENTRY(tmp->data), func
			  , user_data, level + 1, FALSE);
	}
}

GList *tm_file_entry_list(TMFileEntry *entry, GList *files)
{
	GSList *tmp;
	files = g_list_prepend(files, g_strdup(entry->path));
	for (tmp = entry->children; tmp; tmp = g_slist_next(tmp))
	{
		files = tm_file_entry_list((TMFileEntry *) tmp->data, files);
	}
	if (!files)
		files = g_list_reverse(files);
	return files;
}
