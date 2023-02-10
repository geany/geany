/*
 *      keybindingsprivate.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2014 The Geany contributors
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

#include "keybindings.h"

#include <glib.h>

G_BEGIN_DECLS

struct GeanyKeyGroup
{
	const gchar *name;		/* Group name used in the configuration file, such as @c "html_chars" */
	const gchar *label;		/* Group label used in the preferences dialog keybindings tab */
	GeanyKeyGroupCallback callback;	/* use this or individual keybinding callbacks */
	gboolean plugin;		/* used by plugin */
	/* pointers to GeanyKeyBinding structs.
	 * always valid, fixed size or dynamic */
	GPtrArray *key_items;
	/* number of keybindings in plugin_keys */
	gsize plugin_key_count;
	/* array of GeanyKeyBinding structs, used for a dynamically sized group only. */
	GeanyKeyBinding *plugin_keys;
	GeanyKeyGroupFunc cb_func;	/* use this or individual keybinding callbacks (new style) */
	gpointer cb_data;
	GDestroyNotify cb_data_destroy; /* used to destroy handler_data */
};

G_END_DECLS

#endif /* GEANY_KEYBINDINGS_PRIVATE_H */
