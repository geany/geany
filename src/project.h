/*
 *      project.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2007-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef GEANY_PROJECT_H
#define GEANY_PROJECT_H 1

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GEANY_PROJECT_EXT				"geany"


/** Structure for representing a project. */
typedef struct GeanyProject
{
	gchar *name; 			/**< The name of the project. */
	gchar *description; 	/**< Short description of the project. */
	gchar *file_name; 		/**< Where the project file is stored (in UTF-8). */
	gchar *base_path;		/**< Base path of the project directory (in UTF-8, maybe relative). */
	/** Identifier whether it is a pure Geany project or modified/extended
	 * by a plugin. */
	gint type;
	GStrv file_patterns;	/**< Array of filename extension patterns. */

	struct GeanyProjectPrivate	*priv;	/* must be last, append fields before this item */
}
GeanyProject;


void project_write_config(void);


#ifdef GEANY_PRIVATE

typedef struct ProjectPrefs
{
	gchar *session_file;
	gboolean project_session;
	gboolean project_file_in_basedir;
} ProjectPrefs;

extern ProjectPrefs project_prefs;


void project_init(void);

void project_finalize(void);


void project_new(void);

void project_open(void);

gboolean project_close(gboolean open_default);

void project_properties(void);

void project_build_properties(void);

gboolean project_ask_close(void);


gboolean project_load_file(const gchar *locale_file_name);

gboolean project_load_file_with_session(const gchar *locale_file_name);

gchar *project_get_base_path(void);


const struct GeanyFilePrefs *project_get_file_prefs(void);

void project_save_prefs(GKeyFile *config);

void project_load_prefs(GKeyFile *config);

void project_setup_prefs(void);

void project_apply_prefs(void);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_PROJECT_H */
