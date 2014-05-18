/*
 *      keybindingsprivate.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2014 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#ifndef GEANY_KEYBINDINGS_PRIVATE_H
#define GEANY_KEYBINDINGS_PRIVATE_H 1

#include <glib.h>
#include "keybindings.h"

G_BEGIN_DECLS

struct GeanyKeyGroup
{
	const gchar *name;		/* Group name used in the configuration file, such as @c "html_chars" */
	const gchar *label;		/* Group label used in the preferences dialog keybindings tab */
	GeanyKeyGroupCallback callback;	/* use this or individual keybinding callbacks */
	gboolean plugin;		/* used by plugin */
	GPtrArray *key_items;	/* pointers to GeanyKeyBinding structs */
	gsize plugin_key_count;			/* number of keybindings the group holds */
	GeanyKeyBinding *plugin_keys;	/* array of GeanyKeyBinding structs */
};

G_END_DECLS

#endif /* GEANY_KEYBINDINGS_PRIVATE_H */
