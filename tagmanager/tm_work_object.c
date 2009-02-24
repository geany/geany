/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

/**
 * @file tm_work_object.h
 * A TMWorkObject structure is the base class for TMSourceFile and TMProject.
*/

#include "general.h"	/* must always come first */

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <glib/gstdio.h>
#ifdef G_OS_WIN32
# define VC_EXTRALEAN
# define WIN32_LEAN_AND_MEAN
# include <windows.h> /* for GetFullPathName */
#endif

#include "tm_tag.h"
#include "tm_work_object.h"

static GPtrArray *s_work_object_subclasses = NULL;


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


#ifdef G_OS_WIN32
/* realpath implementation for Windows found at http://bugzilla.gnome.org/show_bug.cgi?id=342926
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

guint tm_work_object_register(GFreeFunc free_func, TMUpdateFunc update_func, TMFindFunc find_func)
{
	TMWorkObjectClass *object_class;
	if (NULL == s_work_object_subclasses)
	{
		s_work_object_subclasses = g_ptr_array_new();
		object_class = g_new(TMWorkObjectClass, 1);
		object_class->free_func = tm_work_object_free;
		object_class->update_func = NULL;
		object_class->find_func = NULL;
		g_ptr_array_add(s_work_object_subclasses, object_class);
	}
	object_class = g_new(TMWorkObjectClass, 1);
	object_class->free_func = free_func;
	object_class->update_func = update_func;
	object_class->find_func = find_func;
	g_ptr_array_add(s_work_object_subclasses, object_class);
	return (s_work_object_subclasses->len - 1);
}

gboolean tm_work_object_init(TMWorkObject *work_object, guint type, const char *file_name
	  , gboolean create)
{
	struct stat s;
	int status;

	if (file_name != NULL)
	{
		if (0 != (status = g_stat(file_name, &s)))
		{
			if (create)
			{
				FILE *f;
				if (NULL == (f = g_fopen(file_name, "a+")))
				{
					g_warning("Unable to create file %s", file_name);
					return FALSE;
				}
				fclose(f);
				status = g_stat(file_name, &s);
			}
		}
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
		work_object->file_name = tm_get_real_path(file_name);
		work_object->short_name = strrchr(work_object->file_name, '/');
		if (work_object->short_name)
			++ work_object->short_name;
		else
			work_object->short_name = work_object->file_name;
	}
	else
	{
		work_object->file_name = NULL;
		work_object->short_name = NULL;
	}
	work_object->type = type;
	work_object->parent = NULL;
	work_object->analyze_time = 0;
	work_object->tags_array = NULL;
	return TRUE;
}

/*
time_t tm_get_file_timestamp(const char *file_name)
{
	struct stat s;

	g_return_val_if_fail(file_name, 0);

	if (0 != g_stat(file_name, &s))
	{
		return (time_t) 0;
	}
	else
		return s.st_mtime;
}

gboolean tm_work_object_is_changed(TMWorkObject *work_object)
{
	return (gboolean) (work_object->analyze_time < tm_get_file_timestamp(work_object->file_name));
}
*/

TMWorkObject *tm_work_object_new(guint type, const char *file_name, gboolean create)
{
	TMWorkObject *work_object = g_new(TMWorkObject, 1);
	if (!tm_work_object_init(work_object, type, file_name, create))
	{
		g_free(work_object);
		return NULL;
	}
	return work_object;
}

void tm_work_object_destroy(TMWorkObject *work_object)
{
	if (work_object)
	{
		g_free(work_object->file_name);
		if (work_object->tags_array)
			g_ptr_array_free(work_object->tags_array, TRUE);
	}
}

void tm_work_object_free(gpointer work_object)
{
	if (NULL != work_object)
	{
		TMWorkObject *w = (TMWorkObject *) work_object;
		if ((w->type > 0) && (w->type < s_work_object_subclasses->len) &&
		  (s_work_object_subclasses->pdata[w->type] != NULL))
		{
			GFreeFunc free_func =
		  	((TMWorkObjectClass *)s_work_object_subclasses->pdata[w->type])->free_func;
			if (NULL != free_func)
				free_func(work_object);
			return;
		}
		tm_work_object_destroy(w);
		g_free(work_object);
	}
}

void tm_work_object_write_tags(TMWorkObject *work_object, FILE *file, guint attrs)
{
	if (NULL != work_object->tags_array)
	{
		guint i;
		for (i=0; i < work_object->tags_array->len; ++i)
			tm_tag_write((TMTag *) g_ptr_array_index(work_object->tags_array, i)
			  , file, (TMTagAttrType) attrs);
	}
}

gboolean tm_work_object_update(TMWorkObject *work_object, gboolean force
  , gboolean recurse, gboolean update_parent)
{
	if ((NULL != work_object) && (work_object->type > 0) &&
		  (work_object->type < s_work_object_subclasses->len) &&
		  (s_work_object_subclasses->pdata[work_object->type] != NULL))
	{
		TMUpdateFunc update_func =
		  ((TMWorkObjectClass *)s_work_object_subclasses->pdata[work_object->type])->update_func;
		if (NULL != update_func)
			return update_func(work_object, force, recurse, update_parent);
	}
	return FALSE;
}

TMWorkObject *tm_work_object_find(TMWorkObject *work_object, const char *file_name
  , gboolean name_only)
{
	if ((NULL != work_object) && (work_object->type > 0) &&
		  (work_object->type < s_work_object_subclasses->len) &&
		  (s_work_object_subclasses->pdata[work_object->type] != NULL))
	{
		TMFindFunc find_func =
		  ((TMWorkObjectClass *)s_work_object_subclasses->pdata[work_object->type])->find_func;
		if (NULL == find_func)
		{
			if (name_only)
			{
				const char *short_name = strrchr(file_name, '/');
				if (short_name)
					++ short_name;
				else
					short_name = file_name;
				if (0 == strcmp(work_object->short_name, short_name))
					return work_object;
				else
					return NULL;
			}
			else
			{
				char *path = tm_get_real_path(file_name);
				int cmp = strcmp(work_object->file_name, file_name);
				g_free(path);
				if (0 == cmp)
					return work_object;
				else
					return NULL;
			}
		}
		else
			return find_func(work_object, file_name, name_only);
	}
	return NULL;
}

void tm_work_object_dump(const TMWorkObject *w)
{
	if (w)
	{
		fprintf(stderr, "%s", w->file_name);
		if (w->parent)
			fprintf(stderr, " <- %s\n", w->parent->file_name);
		else
			fprintf(stderr, " <- NULL\n");
	}
}
