/*
 *      build.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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
 */


#ifndef GEANY_BUILD_H
#define GEANY_BUILD_H 1

#include "geany.h"

GPid build_make_c_file(gint idx, gboolean cust_target);

GPid build_compile_c_file(gint idx);

GPid build_link_c_file(gint idx);

GPid build_compile_cpp_file(gint idx);

GPid build_link_cpp_file(gint idx);

GPid build_compile_java_file(gint idx);

GPid build_compile_pascal_file(gint idx);

GPid build_spawn_cmd(gint idx, gchar **cmd);

GPid build_run_cmd(gint idx);

void build_exit_cb (GPid child_pid, gint status, gpointer user_data);

GIOChannel *build_set_up_io_channel (gint fd, GIOCondition cond, GIOFunc func, gpointer data);

gboolean build_iofunc(GIOChannel *ioc, GIOCondition cond, gpointer data);

gboolean build_create_shellscript(const gint idx, const gchar *fname, const gchar *exec, const gchar *args);

#endif
