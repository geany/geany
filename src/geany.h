/*
 *      geany.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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

/* Main header - should be included first in all source files.
 * externs and function prototypes are implemented in main.c. */

#ifndef GEANY_H
#define GEANY_H 1

/* This is included here for compatibility with when GeanyApp used to be
 * defined in this header. Some plugins (ex. GeanyLua) include individual
 * headers instead of geanyplugin.h for some reason so they wouldn't
 * get the GeanyApp definition if this isn't here. */
#include "app.h"

#include <glib.h>


G_BEGIN_DECLS


/* for detailed description look in the documentation, things are not
 * listed in the documentation should not be changed */
#define GEANY_FILEDEFS_SUBDIR			"filedefs"
#define GEANY_TEMPLATES_SUBDIR			"templates"
#define GEANY_TAGS_SUBDIR				"tags"
#define GEANY_CODENAME					"Gorgon"
#define GEANY_HOMEPAGE					"http://www.geany.org/"
#define GEANY_WIKI						"http://wiki.geany.org/"
#define GEANY_BUG_REPORT				"http://www.geany.org/Support/Bugs"
#define GEANY_DONATE					"http://www.geany.org/service/donate/"
#define GEANY_STRING_UNTITLED			_("untitled")
#define GEANY_DEFAULT_DIALOG_HEIGHT		350
#define GEANY_WINDOW_DEFAULT_WIDTH		900
#define GEANY_WINDOW_DEFAULT_HEIGHT		600

#ifndef G_GNUC_WARN_UNUSED_RESULT
#define G_GNUC_WARN_UNUSED_RESULT
#endif

#if defined(GEANY_PRIVATE) || defined(GEANY_DISABLE_DEPRECATION_WARNINGS)
#	define GEANY_DEPRECATED
#	define GEANY_DEPRECATED_FOR(x)
#else
#	define GEANY_DEPRECATED			G_GNUC_DEPRECATED
#	define GEANY_DEPRECATED_FOR(x)	G_GNUC_DEPRECATED_FOR(x)
#endif

/* Re-defined by plugindata.h as something else */
#ifndef GEANY
# define GEANY(symbol_name) symbol_name
#endif


#ifdef GEANY_PRIVATE

/* prototype is here so that all files can use it. */
void geany_debug(gchar const *format, ...) G_GNUC_PRINTF (1, 2);

#endif /* GEANY_PRIVATE */

G_END_DECLS

#endif /* GEANY_H */
