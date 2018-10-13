/*
 *      app.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2014 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2014 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2014 Matthew Brush <matt@geany.org>
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

/**
 * @file app.h
 * Contains the GeanyApp.
 */

#ifndef GEANY_APP_H
#define GEANY_APP_H 1

#include "tm_tag.h" /* FIXME: should be included in tm_workspace.h */
#include "tm_workspace.h"
#include "project.h"

#include <glib.h>

G_BEGIN_DECLS

/** Important application fields. */
typedef struct GeanyApp
{
	gboolean			debug_mode;		/**< @c TRUE if debug messages should be printed. */
	/** User configuration directory, usually @c ~/.config/geany.
	 * This is a full path read by @ref utils_get_real_path().
	 * @note Plugin configuration files should be saved as:
	 * @code g_build_path(G_DIR_SEPARATOR_S, geany->app->configdir, "plugins", "pluginname",
	 * 	"file.conf", NULL); @endcode */
	gchar				*configdir;
	gchar				*datadir;
	gchar				*docdir;
	const TMWorkspace	*tm_workspace;	/**< TagManager workspace/session tags. */
	struct GeanyProject	*project;		/**< Currently active project or @c NULL if none is open. */
}
GeanyApp;


#ifdef GEANY_PRIVATE

extern GeanyApp *app;

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_APP_H */
