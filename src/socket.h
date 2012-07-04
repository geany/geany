/*
 *      socket.h - this file is part of Geany, a fast and lightweight IDE
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


#ifndef GEANY_SOCKET_H
#define GEANY_SOCKET_H 1


struct socket_info_struct
{
	gboolean	 ignore_socket;
	gchar		*file_name;
	GIOChannel	*read_ioc;
	gint 		 lock_socket;
	guint 		 lock_socket_tag;
};

extern struct socket_info_struct socket_info;

gint socket_init(gint argc, gchar **argv);

gboolean socket_lock_input_cb(GIOChannel *source, GIOCondition condition, gpointer data);

gint socket_finalize(void);


#endif
