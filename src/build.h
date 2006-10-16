/*
 *      build.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#ifndef GEANY_BUILD_H
#define GEANY_BUILD_H 1

enum	// Geany Build Options
{
	GBO_MAKE_ALL,
	GBO_MAKE_CUSTOM,
	GBO_MAKE_OBJECT
};

typedef struct
{
	gchar *custom_target;
} BuildOptions;

extern BuildOptions build_options;


GPid build_make_file(gint idx, gint build_opts);

GPid build_compile_file(gint idx);

GPid build_link_file(gint idx);

GPid build_compile_tex_file(gint idx, gint mode);

GPid build_view_tex_file(gint idx, gint mode);

GPid build_run_cmd(gint idx);

void build_exit_cb (GPid child_pid, gint status, gpointer user_data);

#endif
