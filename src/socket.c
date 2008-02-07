/*
 *      socket.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
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
 *
 * $Id$
 */

/*
 * Socket setup and messages handling.
 * The socket allows detection and messages to be sent to the first running instance of Geany.
 * Only the first instance loads session files at startup, and opens files from the command-line.
 */

/*
 * Little dev doc:
 * Each command which is sent between two instances (see send_open_command and
 * socket_lock_input_cb) should have the following scheme:
 * command name\n
 * data\n
 * data\n
 * ...
 * .\n
 * The first thing should be the command name followed by the data belonging to the command and
 * to mark the end of data send a single '.'. Each message should be ended with \n.
 *
 * At the moment the commands open, line and column are available.
 */


#include "geany.h"

#ifdef HAVE_SOCKET

#ifndef G_OS_WIN32
# include <string.h>
# include <sys/time.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <glib/gstdio.h>
#else
# include <winsock2.h>
# include <ws2tcpip.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "main.h"
#include "socket.h"
#include "document.h"
#include "support.h"
#include "ui_utils.h"
#include "utils.h"



#ifdef G_OS_WIN32
#define REMOTE_CMD_PORT		49876
#define SockDesc			SOCKET
#define SOCKET_IS_VALID(s)	((s) != INVALID_SOCKET)
#else
#define SockDesc			gint
#define SOCKET_IS_VALID(s)	((s) >= 0)
#define INVALID_SOCKET		(-1)
#endif


#ifdef G_OS_WIN32
static gint socket_fd_connect_inet	(gushort port);
static gint socket_fd_open_inet		(gushort port);
static void socket_init_win32();
#else
static gint socket_fd_connect_unix	(const gchar *path);
static gint socket_fd_open_unix		(const gchar *path);
#endif

static gint socket_fd_write			(gint sock, const gchar *buf, gint len);
static gint socket_fd_write_all		(gint sock, const gchar *buf, gint len);
static gint socket_fd_gets			(gint sock, gchar *buf, gint len);
static gint socket_fd_check_io		(gint fd, GIOCondition cond);
static gint socket_fd_read			(gint sock, gchar *buf, gint len);
static gint socket_fd_recv			(gint fd, gchar *buf, gint len, gint flags);
static gint socket_fd_close			(gint sock);



void send_open_command(gint sock, gint argc, gchar **argv)
{
	gint i;
	gchar *filename;

	g_return_if_fail(argc > 1);
	geany_debug("using running instance of Geany");

	if (cl_options.goto_line >= 0)
	{
		gchar *line = g_strdup_printf("%d\n", cl_options.goto_line);
		socket_fd_write_all(sock, "line\n", 5);
		socket_fd_write_all(sock, line, strlen(line));
		socket_fd_write_all(sock, ".\n", 2);
		g_free(line);
	}

	if (cl_options.goto_column >= 0)
	{
		gchar *col = g_strdup_printf("%d\n", cl_options.goto_column);
		socket_fd_write_all(sock, "column\n", 7);
		socket_fd_write_all(sock, col, strlen(col));
		socket_fd_write_all(sock, ".\n", 2);
		g_free(col);
	}

	socket_fd_write_all(sock, "open\n", 5);

	for(i = 1; i < argc && argv[i] != NULL; i++)
	{
		filename = get_argv_filename(argv[i]);

		// if the filename is valid or if a new file should be opened is check on the other side
		if (filename != NULL)
		{
			socket_fd_write_all(sock, filename, strlen(filename));
			socket_fd_write_all(sock, "\n", 1);
		}
		else
		{
			g_printerr(_("Could not find file '%s'."), filename);
			g_printerr("\n");	// keep translation from open_cl_files() in main.c.
		}
		g_free(filename);
	}
	socket_fd_write_all(sock, ".\n", 2);
}


/* (Unix domain) socket support to replace the old FIFO code
 * (taken from Sylpheed, thanks) */
gint socket_init(gint argc, gchar **argv)
{
	gint sock;

#ifdef G_OS_WIN32
	HANDLE hmutex;
	// we need a mutex name which is unique for the used configuration dir,
	// but we can't use the whole path, so build a hash on the configdir and use it
	gchar *mutex_name = g_strdup_printf("Geany%d", g_str_hash(app->configdir));

	socket_init_win32();
	hmutex = CreateMutexA(NULL, FALSE, mutex_name);
	g_free(mutex_name);
	if (! hmutex)
	{
		geany_debug("cannot create Mutex\n");
		return -1;
	}
	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		sock = socket_fd_open_inet(REMOTE_CMD_PORT);
		if (sock < 0)
			return 0;
		return sock;
	}

	sock = socket_fd_connect_inet(REMOTE_CMD_PORT);
	if (sock < 0)
		return -1;
#else

	if (socket_info.file_name == NULL)
		socket_info.file_name = g_strdup_printf("%s%cgeany_socket", app->configdir, G_DIR_SEPARATOR);

	sock = socket_fd_connect_unix(socket_info.file_name);
	if (sock < 0)
	{
		g_unlink(socket_info.file_name);
		return socket_fd_open_unix(socket_info.file_name);
	}
#endif

	// remote command mode, here we have another running instance and want to use it
	if (argc > 1)
	{
		send_open_command(sock, argc, argv);
	}

	socket_fd_close(sock);
	return -1;
}


gint socket_finalize(void)
{
	if (socket_info.lock_socket < 0) return -1;

	if (socket_info.lock_socket_tag > 0)
		g_source_remove(socket_info.lock_socket_tag);
	if (socket_info.read_ioc)
	{
		g_io_channel_shutdown(socket_info.read_ioc, FALSE, NULL);
		g_io_channel_unref(socket_info.read_ioc);
		socket_info.read_ioc = NULL;
	}

#ifdef G_OS_WIN32
	WSACleanup();
#else
	if (socket_info.file_name != NULL)
	{
		gchar real_path[512];
		gsize len;

		real_path[0] = '\0';

		// read the contents of the symbolic link socket_info.file_name and delete it
		// readlink should return something like "/tmp/geany_socket.1202396669"
		len = readlink(socket_info.file_name, real_path, sizeof(real_path) - 1);
		if ((gint) len > 0)
		{
			real_path[len] = '\0';
			g_unlink(real_path);
		}
		g_unlink(socket_info.file_name);
		g_free(socket_info.file_name);
	}
#endif

	return 0;
}


#ifdef G_OS_UNIX
static gint socket_fd_connect_unix(const gchar *path)
{
	gint sock;
	struct sockaddr_un addr;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
	{
		perror("fd_connect_unix(): socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		socket_fd_close(sock);
		return -1;
	}

	return sock;
}


static gint socket_fd_open_unix(const gchar *path)
{
	gint sock;
	struct sockaddr_un addr;
	gint val;
	gchar *real_path;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	if (sock < 0)
	{
		perror("sock_open_unix(): socket");
		return -1;
	}

	val = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
	{
		perror("setsockopt");
		socket_fd_close(sock);
		return -1;
	}

	// fix for #1888561:
	// in case the configuration directory is located on a network file system or any other
	// file system which doesn't support sockets, we just link the socket there and create the
	// real socket in the system's tmp directory assuming it supports sockets
	real_path = g_strdup_printf("%s%cgeany_socket.%d",
		g_get_tmp_dir(), G_DIR_SEPARATOR, (gint) time(NULL));

	if (utils_is_file_writeable(real_path) != 0)
	{	// if real_path is not writable for us, fall back to /home/user/.geany/geany_socket
		// instead of creating a symlink and print a warning
		g_warning("Socket %s could not be written, using %s as fallback.", real_path, path);
		setptr(real_path, g_strdup(path));
	}
	// create a symlink in e.g. /home/user/.geany/geany_socket to /tmp/geany_socket.1202396669
	else if (symlink(real_path, path) != 0)
	{
		perror("symlink");
		socket_fd_close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, real_path, sizeof(addr.sun_path) - 1);

	g_free(real_path);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		socket_fd_close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0)
	{
		perror("listen");
		socket_fd_close(sock);
		return -1;
	}

	return sock;
}
#endif

static gint socket_fd_close(gint fd)
{
#ifdef G_OS_WIN32
	return closesocket(fd);
#else
	return close(fd);
#endif
}


#ifdef G_OS_WIN32
static gint socket_fd_open_inet(gushort port)
{
	SockDesc sock;
	struct sockaddr_in addr;
	gchar val;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (! SOCKET_IS_VALID(sock))
	{
		geany_debug("fd_open_inet(): socket() failed: %d\n", WSAGetLastError());
		return -1;
	}

	val = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
	{
		perror("setsockopt");
		socket_fd_close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		socket_fd_close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0)
	{
		perror("listen");
		socket_fd_close(sock);
		return -1;
	}

	return sock;
}


static gint socket_fd_connect_inet(gushort port)
{
	SockDesc sock;
	struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (! SOCKET_IS_VALID(sock))
	{
		geany_debug("fd_connect_inet(): socket() failed: %d\n", WSAGetLastError());
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		socket_fd_close(sock);
		return -1;
	}

	return sock;
}


static void socket_init_win32()
{
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != NO_ERROR)
		geany_debug("WSAStartup() failed\n");

	return;
}
#endif


gboolean socket_lock_input_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
	gint fd, sock;
	gchar buf[4096];
	struct sockaddr_in caddr;
	guint caddr_len;

	caddr_len = sizeof(caddr);

	fd = g_io_channel_unix_get_fd(source);
	sock = accept(fd, (struct sockaddr *)&caddr, &caddr_len);

	// first get the command
	while (socket_fd_gets(sock, buf, sizeof(buf)) != -1)
	{
		if (strncmp(buf, "open", 4) == 0)
		{
			document_delay_colourise();

			while (socket_fd_gets(sock, buf, sizeof(buf)) != -1 && *buf != '.')
			{
				g_strstrip(buf); // remove \n char

				if (g_file_test(buf, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK))
					document_open_file(buf, FALSE, NULL, NULL);
				else
				{	// create new file if it doesn't exist
					gint idx;

					idx = document_new_file(buf, NULL, NULL);
					if (DOC_IDX_VALID(idx))
						ui_add_recent_file(doc_list[idx].file_name);
					else
						geany_debug("got data from socket, but it does not look like a filename");
				}
			}
			document_colourise_new();

			gtk_window_deiconify(GTK_WINDOW(app->window));
#ifdef G_OS_WIN32
			gtk_window_present(GTK_WINDOW(app->window));
#endif
		}
		else if (strncmp(buf, "line", 4) == 0)
		{
			while (socket_fd_gets(sock, buf, sizeof(buf)) != -1 && *buf != '.')
			{
				g_strstrip(buf); // remove \n char
				// on any error we get 0 which should be safe enough as fallback
				cl_options.goto_line = atoi(buf);
			}
		}
		else if (strncmp(buf, "column", 6) == 0)
		{
			while (socket_fd_gets(sock, buf, sizeof(buf)) != -1 && *buf != '.')
			{
				g_strstrip(buf); // remove \n char
				// on any error we get 0 which should be safe enough as fallback
				cl_options.goto_column = atoi(buf);
			}
		}
	}
	socket_fd_close(sock);

	return TRUE;
}


static gint socket_fd_gets(gint fd, gchar *buf, gint len)
{
	gchar *newline, *bp = buf;
	gint n;

	if (--len < 1)
		return -1;
	do
	{
		if ((n = socket_fd_recv(fd, bp, len, MSG_PEEK)) <= 0)
			return -1;
		if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = socket_fd_read(fd, bp, n)) < 0)
			return -1;
		bp += n;
		len -= n;
	} while (! newline && len);

	*bp = '\0';
	return bp - buf;
}


static gint socket_fd_recv(gint fd, gchar *buf, gint len, gint flags)
{
	if (socket_fd_check_io(fd, G_IO_IN) < 0)
		return -1;

	return recv(fd, buf, len, flags);
}


static gint socket_fd_read(gint fd, gchar *buf, gint len)
{
	if (socket_fd_check_io(fd, G_IO_IN) < 0)
		return -1;

#ifdef G_OS_WIN32
	return recv(fd, buf, len, 0);
#else
	return read(fd, buf, len);
#endif
}


static gint socket_fd_check_io(gint fd, GIOCondition cond)
{
	struct timeval timeout;
	fd_set fds;
#ifdef G_OS_UNIX
	gint flags;
#endif

	timeout.tv_sec  = 60;
	timeout.tv_usec = 0;

#ifdef G_OS_UNIX
	// checking for non-blocking mode

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		perror("fcntl");
		return 0;
	}

	if ((flags & O_NONBLOCK) != 0)
		return 0;
#endif

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (cond == G_IO_IN)
	{
		select(fd + 1, &fds, NULL, NULL, &timeout);
	}
	else
	{
		select(fd + 1, NULL, &fds, NULL, &timeout);
	}

	if (FD_ISSET(fd, &fds))
	{
		return 0;
	}
	else
	{
		geany_debug("Socket IO timeout\n");
		return -1;
	}
}


static gint socket_fd_write_all(gint fd, const gchar *buf, gint len)
{
	gint n, wrlen = 0;

	while (len)
	{
		n = socket_fd_write(fd, buf, len);
		if (n <= 0)
			return -1;
		len -= n;
		wrlen += n;
		buf += n;
	}

	return wrlen;
}


gint socket_fd_write(gint fd, const gchar *buf, gint len)
{
	if (socket_fd_check_io(fd, G_IO_OUT) < 0)
		return -1;

#ifdef G_OS_WIN32
	return send(fd, buf, len, 0);
#else
	return write(fd, buf, len);
#endif
}


#endif


