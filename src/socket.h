/*
 *      socket.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 The Geany contributors
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


#ifndef GEANY_SOCKET_H
#define GEANY_SOCKET_H 1

#include <glib.h>

G_BEGIN_DECLS

/* Used on Windows for TCP socket based IPC.
 * The port number is just random but should be below 49152 as Hyper-V tends to bind
 * dynamic port ranges from 49152 to 65535. */
#define SOCKET_WINDOWS_REMOTE_CMD_PORT 45937

struct SocketInfo
{
	gboolean	 ignore_socket;
	gchar		*file_name;
	GIOChannel	*read_ioc;
	gint 		 lock_socket;
	guint 		 lock_socket_tag;
};

extern struct SocketInfo socket_info;

gint socket_init(gint argc, gchar **argv, gushort socket_port);

gboolean socket_lock_input_cb(GIOChannel *source, GIOCondition condition, gpointer data);

gint socket_finalize(void);

G_END_DECLS

#endif /* GEANY_SOCKET_H */
