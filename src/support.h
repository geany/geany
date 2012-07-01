/*
 *      support.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 * @file support.h
 * Defines internationalization macros.
 * @see GLib's @c gi18n-lib.h.
 **/

G_BEGIN_DECLS

#include "geany.h"

#ifdef GETTEXT_PACKAGE
# include <glib/gi18n-lib.h>
#else
# define textdomain(String) (String)
# define bind_textdomain_codeset(String) (String)
# define bindtextdomain(Domain,Charset) (Domain)
# define ngettext(String) (String)
# define _(String) String
# define Q_(String) g_strip_context((String), (String))
# define N_(String) String
#endif

G_END_DECLS
