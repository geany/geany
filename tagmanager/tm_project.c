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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_FNMATCH_H
# include <fnmatch.h>
#endif
#include <glib/gstdio.h>


#include "options.h"
#define LIBCTAGS_DEFINED
#include "tm_tag.h"
#include "tm_workspace.h"
#include "tm_source_file.h"
#include "tm_file_entry.h"
#include "tm_project.h"

#define TM_FILE_NAME ".tm_project.cache"

static const char *s_sources[] = { "*.c", "*.pc" /* C/Pro*C files */
	, "*.C", "*.cpp", "*.cc", "*.cxx", "*.c++" /* C++ files */
	, "*.h", "*.hh", "*.hpp", "*.H", "*.h++", "*.i" /* Header files */
	, "*.oaf", "*.gob", "*.idl" /* CORBA/Bonobo files */
	, "*.l", "*.y" /* Lex/Yacc files */
#if 0
	, "*.ui", "*.moc" /* KDE/QT Files */
	, "*.glade" /* UI files */
#endif
	, "*.java", "*.pl", "*.pm", "*.py", "*.sh" /* Other languages */
	, NULL /* Must terminate with NULL */
};

static const char *s_ignore[] = { "CVS", "intl", "po", NULL };

static GList *glist_from_array(const char **arr)
{
	GList *l = NULL;
	int i;
	for (i =0; arr[i]; ++ i)
		l = g_list_prepend(l, (gpointer) arr[i]);
	return g_list_reverse(l);
}

guint project_class_id = 0;

gboolean tm_project_init(TMProject *project, const char *dir
  , const char **sources, const char **ignore, gboolean force)
{
	struct stat s;
	char *path;

	g_return_val_if_fail((project && dir), FALSE);
#ifdef TM_DEBUG
	g_message("Initializing project %s", dir);
#endif
	if (0 == project_class_id)
	{
		project_class_id = tm_work_object_register(tm_project_free, tm_project_update
		  , tm_project_find_file);
	}

	if ((0 != g_stat(dir, &s)) || (!S_ISDIR(s.st_mode)))
	{
		g_warning("%s: Not a valid directory", dir);
		return FALSE;
	}
	project->dir = tm_get_real_path(dir);
	if (sources)
		project->sources = sources;
	else
		project->sources = s_sources;
	if (ignore)
		project->ignore = ignore;
	else
		project->ignore = s_ignore;
	project->file_list = NULL;
	path = g_strdup_printf("%s/%s", project->dir, TM_FILE_NAME);
	if ((0 != g_stat(path, &s)) || (0 == s.st_size))
		force = TRUE;
	if (FALSE == tm_work_object_init(&(project->work_object),
		  project_class_id, path, force))
	{
		g_warning("Unable to init project file %s", path);
		g_free(project->dir);
		g_free(path);
		return FALSE;
	}
	if (! tm_workspace_add_object(TM_WORK_OBJECT(project)))
	{
		g_warning("Unable to init project file %s", path);
		g_free(project->dir);
		g_free(path);
		return FALSE;
	}
	g_free(path);
	tm_project_open(project, force);
	if (!project->file_list || (0 == project->file_list->len))
		tm_project_autoscan(project);
#ifdef TM_DEBUG
	tm_workspace_dump();
#endif
	return TRUE;
}

TMWorkObject *tm_project_new(const char *dir, const char **sources
  , const char **ignore, gboolean force)
{
	TMProject *project = g_new(TMProject, 1);
	if (FALSE == tm_project_init(project, dir, sources, ignore, force))
	{
		g_free(project);
		return NULL;
	}
	return (TMWorkObject *) project;
}

void tm_project_destroy(TMProject *project)
{
	g_return_if_fail (project != NULL);
#ifdef TM_DEBUG
	g_message("Destroying project: %s", project->work_object.file_name);
#endif

	if (project->file_list)
	{
		guint i;
		for (i = 0; i < project->file_list->len; ++i)
			tm_source_file_free(project->file_list->pdata[i]);
		g_ptr_array_free(project->file_list, TRUE);
	}
	tm_workspace_remove_object(TM_WORK_OBJECT(project), FALSE, TRUE);
	g_free(project->dir);
	tm_work_object_destroy(&(project->work_object));
}

void tm_project_free(gpointer project)
{
	if (NULL != project)
	{
		tm_project_destroy(TM_PROJECT(project));
		g_free(project);
	}
}

gboolean tm_project_add_file(TMProject *project, const char *file_name
  ,gboolean update)
{
	TMWorkObject *source_file;
	const TMWorkObject *workspace = TM_WORK_OBJECT(tm_get_workspace());
	char *path;
	gboolean exists = FALSE;

	g_return_val_if_fail((project && file_name), FALSE);
	path = tm_get_real_path(file_name);
#ifdef TM_DEBUG
	g_message("Adding %s to project", path);
#endif
	/* Check if the file is already loaded in the workspace */
	source_file = tm_workspace_find_object(TM_WORK_OBJECT(workspace), path, FALSE);
	if (NULL != source_file)
	{
		if ((workspace == source_file->parent) || (NULL == source_file->parent))
		{
#ifdef TM_DEBUG
			g_message("%s moved from workspace to project", path);
#endif
			tm_workspace_remove_object(source_file, FALSE, TRUE);
		}
		else if (TM_WORK_OBJECT(project) == source_file->parent)
				{
#ifdef TM_DEBUG
			g_message("%s already exists in project", path);
#endif
			exists = TRUE;
				}
		else
		{
			g_warning("Source file %s is shared among projects - will be duplicated!", path);
			source_file = NULL;
			}
		}
	if (NULL == source_file)
	{
	if (NULL == (source_file = tm_source_file_new(file_name, TRUE, NULL)))
	{
		g_warning("Unable to create source file for file %s", file_name);
		g_free(path);
		return FALSE;
	}
	}
	source_file->parent = TM_WORK_OBJECT(project);
	if (NULL == project->file_list)
		project->file_list = g_ptr_array_new();
	if (!exists)
	g_ptr_array_add(project->file_list, source_file);
	TM_SOURCE_FILE(source_file)->inactive = FALSE;
	if (update)
		tm_project_update(TM_WORK_OBJECT(project), TRUE, FALSE, TRUE);
	g_free(path);
	return TRUE;
}

TMWorkObject *tm_project_find_file(TMWorkObject *work_object
  , const char *file_name, gboolean name_only)
{
	TMProject *project;

	g_return_val_if_fail(work_object && file_name, NULL);
	if (!IS_TM_PROJECT(work_object))
	{
		g_warning("Non project pointer passed to tm_project_find_file(%s)", file_name);
		return NULL;
	}
	project = TM_PROJECT(work_object);
	if ((NULL == project->file_list) || (0 == project->file_list->len))
		return NULL;
	else
	{
		guint i;
		char *name, *name1;
		if (name_only)
		{
			name = strrchr(file_name, '/');
			if (name)
				name = g_strdup(name + 1);
			else
				name = g_strdup(file_name);
		}
		else
			name = tm_get_real_path(file_name);
		for (i=0; i < project->file_list->len; ++i)
		{
			if (name_only)
				name1 = TM_WORK_OBJECT(project->file_list->pdata[i])->short_name;
			else
				name1 = TM_WORK_OBJECT(project->file_list->pdata[i])->file_name;
			if (0 == strcmp(name, name1))
			{
				g_free(name);
				return TM_WORK_OBJECT(project->file_list->pdata[i]);
			}
		}
		g_free(name);
	}
	return NULL;
}

gboolean tm_project_remove_object(TMProject *project, TMWorkObject *w)
{
	guint i;

	g_return_val_if_fail((project && w), FALSE);
	if (!project->file_list)
		return FALSE;
	for (i=0; i < project->file_list->len; ++i)
	{
		if (w == project->file_list->pdata[i])
		{
			tm_work_object_free(w);
			g_ptr_array_remove_index(project->file_list, i);
			tm_project_update(TM_WORK_OBJECT(project), TRUE, FALSE, TRUE);
			return TRUE;
		}
	}
	return FALSE;
}

void tm_project_recreate_tags_array(TMProject *project)
{
	guint i, j;
	TMWorkObject *source_file;

	g_return_if_fail(project);
#ifdef TM_DEBUG
	g_message("Recreating tags for project: %s", project->work_object.file_name);
#endif

	if (NULL != project->work_object.tags_array)
		g_ptr_array_set_size(project->work_object.tags_array, 0);
	else
		project->work_object.tags_array = g_ptr_array_new();

	if (!project->file_list)
		return;
	for (i=0; i < project->file_list->len; ++i)
	{
		source_file = TM_WORK_OBJECT(project->file_list->pdata[i]);
		if ((NULL != source_file) && !(TM_SOURCE_FILE(source_file)->inactive) &&
		  (NULL != source_file->tags_array) && (source_file->tags_array->len > 0))
		{
			for (j = 0; j < source_file->tags_array->len; ++j)
			{
				g_ptr_array_add(project->work_object.tags_array,
					  source_file->tags_array->pdata[j]);
			}
		}
	}
	tm_tags_sort(project->work_object.tags_array, NULL, FALSE);
}

gboolean tm_project_update(TMWorkObject *work_object, gboolean force
  , gboolean recurse, gboolean update_parent)
{
	TMProject *project;
	guint i;
	gboolean update_tags = force;

	if (!work_object || !IS_TM_PROJECT(work_object))
	{
		g_warning("Non project pointer passed to project update");
		return FALSE;
	}

#ifdef TM_DEBUG
	g_message("Updating project: %s", work_object->file_name);
#endif

	project = TM_PROJECT(work_object);
	if ((NULL != project->file_list) && (project->file_list->len > 0))
	{
		if (recurse)
		{
			for (i=0; i < project->file_list->len; ++i)
			{
				if (TRUE == tm_source_file_update(TM_WORK_OBJECT(
					  project->file_list->pdata[i]), FALSE, FALSE, FALSE))
					update_tags = TRUE;
			}
		}
		if (update_tags || (TM_WORK_OBJECT (project)->tags_array == NULL))
		{
#ifdef TM_DEBUG
			g_message ("Tags updated, recreating tags array");
#endif
			tm_project_recreate_tags_array(project);
		}
	}
	/* work_object->analyze_time = time(NULL); */
	if ((work_object->parent) && (update_parent))
		tm_workspace_update(work_object->parent, TRUE, FALSE, FALSE);
	return update_tags;
}


#define IGNORE_FILE ".tm_ignore"
static void tm_project_set_ignorelist(TMProject *project)
{
	struct stat s;
	char *ignore_file = g_strconcat(project->dir, "/", IGNORE_FILE, NULL);
	if (0 == g_stat(ignore_file, &s))
	{
		if (NULL != Option.ignore)
			stringListClear(Option.ignore);
		addIgnoreListFromFile(ignore_file);
	}
	g_free(ignore_file);
}

gboolean tm_project_open(TMProject *project, gboolean force)
{
	FILE *fp;
	TMSourceFile *source_file = NULL;
	TMTag *tag;

	if (!project || !IS_TM_PROJECT(TM_WORK_OBJECT(project)))
		return FALSE;
#ifdef TM_DEBUG
	g_message("Opening project %s", project->work_object.file_name);
#endif
	tm_project_set_ignorelist(project);
	if (NULL == (fp = g_fopen(project->work_object.file_name, "r")))
		return FALSE;
	while (NULL != (tag = tm_tag_new_from_file(source_file, fp, 0, FALSE)))
	{
		if (tm_tag_file_t == tag->type)
		{
			if (!(source_file = TM_SOURCE_FILE(
			  tm_source_file_new(tag->name, FALSE, NULL))))
			{
#ifdef TM_DEBUG
				g_warning("Unable to create source file %s", tag->name);
#endif
				if (!force)
				{
					tm_tag_unref(tag);
					fclose(fp);
					return FALSE;
				}
				else
					source_file = NULL;
			}
			else
			{
				source_file->work_object.parent = TM_WORK_OBJECT(project);
				source_file->lang = tag->atts.file.lang;
				source_file->inactive = tag->atts.file.inactive;
				if (!project->file_list)
					project->file_list = g_ptr_array_new();
				g_ptr_array_add(project->file_list, source_file);
			}
			tm_tag_unref(tag);
		}
		else
		{
			if ((NULL == source_file) || (source_file->inactive)) /* Dangling tag */
			{
#ifdef TM_DEBUG
				g_warning("Dangling tag %s", tag->name);
#endif
				tm_tag_unref(tag);
				if (!force)
				{
					fclose(fp);
					return FALSE;
				}
			}
			else
			{
				if (NULL == source_file->work_object.tags_array)
					source_file->work_object.tags_array = g_ptr_array_new();
				g_ptr_array_add(source_file->work_object.tags_array, tag);
#ifdef TM_DEBUG
				g_message ("Added tag %s", tag->name);
#endif
			}
		}
	}
	fclose(fp);
	tm_project_update((TMWorkObject *) project, FALSE, TRUE, TRUE);
	return TRUE;
}

gboolean tm_project_save(TMProject *project)
{
	guint i;
	FILE *fp;

	if (!project)
		return FALSE;
	if (NULL == (fp = g_fopen(project->work_object.file_name, "w")))
	{
		g_warning("Unable to save project %s", project->work_object.file_name);
		return FALSE;
	}
	if (project->file_list)
	{
		for (i=0; i < project->file_list->len; ++i)
		{
			if (FALSE == tm_source_file_write(TM_WORK_OBJECT(project->file_list->pdata[i])
				, fp, tm_tag_attr_max_t))
			{
				fclose(fp);
				return FALSE;
			}
		}
	}
	fclose(fp);
	return TRUE;
}

static void tm_project_add_file_recursive(TMFileEntry *entry
  , gpointer user_data, guint __unused__ level)
{
	TMProject *project;
	if (!user_data || !entry || (tm_file_dir_t == entry->type))
		return;
	project = TM_PROJECT(user_data);
	tm_project_add_file(project, entry->path, FALSE);
}

gboolean tm_project_autoscan(TMProject *project)
{
	TMFileEntry *root_dir;
	GList *file_match;
	GList *dir_unmatch;

	file_match = glist_from_array(project->sources);
	dir_unmatch = glist_from_array(project->ignore);

	if (!project || !IS_TM_PROJECT(TM_WORK_OBJECT(project))
	  || (!project->dir))
		return FALSE;
	if (!(root_dir = tm_file_entry_new(project->dir, NULL, TRUE
		, file_match, NULL, NULL, dir_unmatch, TRUE, TRUE)))
	{
		g_warning("Unable to create file entry");
		return FALSE;
	}
	g_list_free(file_match);
	g_list_free(dir_unmatch);
	tm_file_entry_foreach(root_dir, tm_project_add_file_recursive
	  , project, 0, FALSE);
	tm_file_entry_free(root_dir);
	tm_project_update(TM_WORK_OBJECT(project), TRUE, FALSE, TRUE);
	return TRUE;
}

gboolean tm_project_sync(TMProject *project, GList *files)
{
	GList *tmp;
	guint i;

	if (project->file_list)
	{
		for (i = 0; i < project->file_list->len; ++i)
			tm_source_file_free(project->file_list->pdata[i]);
		g_ptr_array_free(project->file_list, TRUE);
		project->file_list = NULL;
		if (project->work_object.tags_array)
		{
			g_ptr_array_free(project->work_object.tags_array, TRUE);
			project->work_object.tags_array = NULL;
		}
	}
	for (tmp = files; tmp; tmp = g_list_next(tmp))
	{
		tm_project_add_file(project, (const char *) tmp->data, FALSE);
	}
	tm_project_update(TM_WORK_OBJECT(project), TRUE, FALSE, TRUE);
	return TRUE;
}

void tm_project_dump(const TMProject *p)
{
	if (p)
	{
		tm_work_object_dump(TM_WORK_OBJECT(p));
		if (p->file_list)
		{
			guint i;
			for (i=0; i < p->file_list->len; ++i)
			{
				fprintf(stderr, "->\t");
				tm_work_object_dump(TM_WORK_OBJECT(p->file_list->pdata[i]));
			}
		}
		fprintf(stderr, "-------------------------\n");
	}
}

gboolean tm_project_is_source_file(TMProject *project, const char *file_name)
{
	const char **pr_extn;

	if (!(project && IS_TM_PROJECT(TM_WORK_OBJECT(project))
	  && file_name && project->sources))
		return FALSE;
	for (pr_extn = project->sources; pr_extn && *pr_extn; ++ pr_extn)
	{
		if (0 == fnmatch(*pr_extn, file_name, 0))
			return TRUE;
	}
	return FALSE;
}
