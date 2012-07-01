/*
 *      socket.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2006-2012 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2006 Hiroyuki Yamamoto (author of Sylpheed)
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
 * The command window is only available on Windows and takes no additional data, instead it
 * writes back a Windows handle (HWND) for the main window to set it to the foreground (focus).
 *
 * At the moment the commands window, doclist, open, openro, line and column are available.
 *
 * About the socket files on Unix-like systems:
 * Geany creates a socket in /tmp (or any other directory returned by g_get_tmp_dir()) and
 * a symlink in the current configuration to the created socket file. The symlink is named
 * geany_socket_<hostname>_<displayname> (displayname is the name of the active X display).
 * If the socket file cannot be created in the temporary directory, Geany creates the socket file
 * directly in the configuration directory as a fallback.
 *
 */


#include "geany.h"

#ifdef HAVE_SOCKET

#ifndef G_OS_WIN32
# include <sys/time.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <glib/gstdio.h>
#else
# include <gdk/gdkwin32.h>
# include <windows.h>
# include <winsock2.h>
# include <ws2tcpip.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "main.h"
#include "socket.h"
#include "document.h"
#include "support.h"
#include "ui_utils.h"
#include "utils.h"
#include "dialogs.h"
#include "encodings.h"
#include "project.h"


#ifdef G_OS_WIN32
#define REMOTE_CMD_PORT		49876
#define SOCKET_IS_VALID(s)	((s) != INVALID_SOCKET)
#else
#define SOCKET_IS_VALID(s)	((s) >= 0)
#define INVALID_SOCKET		(-1)
#endif
#define BUFFER_LENGTH 4096

struct socket_info_struct socket_info;


#ifdef G_OS_WIN32
static gint socket_fd_connect_inet	(gushort port);
static gint socket_fd_open_inet		(gushort port);
static void socket_init_win32		(void);
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



static void send_open_command(gint sock, gint argc, gchar **argv)
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

	if (cl_options.readonly) /* append "ro" to denote readonly status for new docs */
		socket_fd_write_all(sock, "openro\n", 7);
	else
		socket_fd_write_all(sock, "open\n", 5);

	for (i = 1; i < argc && argv[i] != NULL; i++)
	{
		filename = main_get_argv_filename(argv[i]);

		/* if the filename is valid or if a new file should be opened is check on the other side */
		if (filename != NULL)
		{
			socket_fd_write_all(sock, filename, strlen(filename));
			socket_fd_write_all(sock, "\n", 1);
		}
		else
		{
			g_printerr(_("Could not find file '%s'."), filename);
			g_printerr("\n");	/* keep translation from open_cl_files() in main.c. */
		}
		g_free(filename);
	}
	socket_fd_write_all(sock, ".\n", 2);
}


#ifndef G_OS_WIN32
static void remove_socket_link_full(void)
{
	gchar real_path[512];
	gsize len;

	real_path[0] = '\0';

	/* read the contents of the symbolic link socket_info.file_name and delete it
	 * readlink should return something like "/tmp/geany_socket.499602d2" */
	len = readlink(socket_info.file_name, real_path, sizeof(real_path) - 1);
	if ((gint) len > 0)
	{
		real_path[len] = '\0';
		g_unlink(real_path);
	}
	g_unlink(socket_info.file_name);
}
#endif


static void socket_get_document_list(gint sock)
{
	gchar doc_list[BUFFER_LENGTH];
	gint doc_list_len;

	if (sock < 0)
		return;

	socket_fd_write_all(sock, "doclist\n", 8);

	doc_list_len = socket_fd_read(sock, doc_list, sizeof(doc_list));
	if (doc_list_len >= BUFFER_LENGTH)
		doc_list_len = BUFFER_LENGTH -1;
	doc_list[doc_list_len] = '\0';
	/* if we received ETX (end-of-text), there were no open files, so print only otherwise */
	if (! utils_str_equal(doc_list, "\3"))
		printf("%s", doc_list);
}


#ifndef G_OS_WIN32
static void check_socket_permissions(void)
{
	struct stat socket_stat;

	if (g_lstat(socket_info.file_name, &socket_stat) == 0)
	{	/* If the user id of the process is not the same as the owner of the socket
		 * file, then ignore this socket and start a new session. */
		if (socket_stat.st_uid != getuid())
		{
			const gchar *msg = _(
	/* TODO maybe this message needs a rewording */
	"Geany tried to access the Unix Domain socket of another instance running as another user.\n"
	"This is a fatal error and Geany will now quit.");
			g_warning("%s", msg);
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", msg);
			exit(1);
		}
	}
}
#endif


/* (Unix domain) socket support to replace the old FIFO code
 * (taken from Sylpheed, thanks)
 * Returns the created socket, -1 if an error occurred or -2 if another socket exists and files
 * were sent to it. */
gint socket_init(gint argc, gchar **argv)
{
	gint sock;
#ifdef G_OS_WIN32
	HANDLE hmutex;
	HWND hwnd;
	socket_init_win32();
	hmutex = CreateMutexA(NULL, FALSE, "Geany");
	if (! hmutex)
	{
		geany_debug("cannot create Mutex\n");
		return -1;
	}
	if (GetLastError() != ERROR_ALREADY_EXISTS)
	{
		/* To support multiple instances with different configuration directories (as we do on
		 * non-Windows systems) we would need to use different port number s but it might be
		 * difficult to get a port number which is unique for a configuration directory (path)
		 * and which is unused. This port number has to be guessed by the first and new instance
		 * and the only data is the configuration directory path.
		 * For now we use one port number, that is we support only one instance at all. */
		sock = socket_fd_open_inet(REMOTE_CMD_PORT);
		if (sock < 0)
			return 0;
		return sock;
	}

	sock = socket_fd_connect_inet(REMOTE_CMD_PORT);
	if (sock < 0)
		return -1;
#else
	gchar *display_name = gdk_get_display();
	gchar *hostname = utils_get_hostname();
	gchar *p;

	if (display_name == NULL)
		display_name = g_strdup("NODISPLAY");

	/* these lines are taken from dcopc.c in kdelibs */
	if ((p = strrchr(display_name, '.')) > strrchr(display_name, ':') && p != NULL)
		*p = '\0';
	/* remove characters that may not be acceptable in a filename */
	for (p = display_name; *p; p++)
	{
		if (*p == ':' || *p == '/')
			*p = '_';
	}

	if (socket_info.file_name == NULL)
		socket_info.file_name = g_strdup_printf("%s%cgeany_socket_%s_%s",
			app->configdir, G_DIR_SEPARATOR, hostname, display_name);

	g_free(display_name);
	g_free(hostname);

	/* check whether the real user id is the same as this of the socket file */
	check_socket_permissions();

	sock = socket_fd_connect_unix(socket_info.file_name);
	if (sock < 0)
	{
		remove_socket_link_full(); /* deletes the socket file and the symlink */
		return socket_fd_open_unix(socket_info.file_name);
	}
#endif

	/* remote command mode, here we have another running instance and want to use it */

#ifdef G_OS_WIN32
	/* first we send a request to retrieve the window handle and focus the window */
	socket_fd_write_all(sock, "window\n", 7);
	if (socket_fd_read(sock, (gchar *)&hwnd, sizeof(hwnd)) == sizeof(hwnd))
		SetForegroundWindow(hwnd);
#endif
	/* now we send the command line args */
	if (argc > 1)
	{
		send_open_command(sock, argc, argv);
	}

	if (cl_options.list_documents)
	{
		socket_get_document_list(sock);
	}

	socket_fd_close(sock);
	return -2;
}


gint socket_finalize(void)
{
	if (socket_info.lock_socket < 0)
		return -1;

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
		remove_socket_link_full(); /* deletes the socket file and the symlink */
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

	/* fix for #1888561:
	 * in case the configuration directory is located on a network file system or any other
	 * file system which doesn't support sockets, we just link the socket there and create the
	 * real socket in the system's tmp directory assuming it supports sockets */
	real_path = g_strdup_printf("%s%cgeany_socket.%08x",
		g_get_tmp_dir(), G_DIR_SEPARATOR, g_random_int());

	if (utils_is_file_writable(real_path) != 0)
	{	/* if real_path is not writable for us, fall back to ~/.config/geany/geany_socket_*_* */
		/* instead of creating a symlink and print a warning */
		g_warning("Socket %s could not be written, using %s as fallback.", real_path, path);
		SETPTR(real_path, g_strdup(path));
	}
	/* create a symlink in e.g. ~/.config/geany/geany_socket_hostname__0 to /tmp/geany_socket.499602d2 */
	else if (symlink(real_path, path) != 0)
	{
		perror("symlink");
		socket_fd_close(sock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, real_path, sizeof(addr.sun_path) - 1);

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

	g_chmod(real_path, 0600);

	g_free(real_path);

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
	SOCKET sock;
	struct sockaddr_in addr;
	gchar val;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (G_UNLIKELY(! SOCKET_IS_VALID(sock)))
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
	SOCKET sock;
	struct sockaddr_in addr;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (G_UNLIKELY(! SOCKET_IS_VALID(sock)))
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


static void socket_init_win32(void)
{
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != NO_ERROR)
		geany_debug("WSAStartup() failed\n");

	return;
}
#endif


static void handle_input_filename(const gchar *buf)
{
	gchar *utf8_filename, *locale_filename;

	/* we never know how the input is encoded, so do the best auto detection we can */
	if (! g_utf8_validate(buf, -1, NULL))
		utf8_filename = encodings_convert_to_utf8(buf, -1, NULL);
	else
		utf8_filename = g_strdup(buf);

	locale_filename = utils_get_locale_from_utf8(utf8_filename);
	if (locale_filename)
	{
		if (g_str_has_suffix(locale_filename, ".geany"))
		{
			if (project_ask_close())
				main_load_project_from_command_line(locale_filename, TRUE);
		}
		else
			main_handle_filename(locale_filename);
	}
	g_free(utf8_filename);
	g_free(locale_filename);
}


static gchar *build_document_list(void)
{
	GString *doc_list = g_string_new(NULL);
	guint i;
	const gchar *filename;

	foreach_document(i)
	{
		filename = DOC_FILENAME(documents[i]);
		g_string_append(doc_list, filename);
		g_string_append_c(doc_list, '\n');
	}
	return g_string_free(doc_list, FALSE);
}


gboolean socket_lock_input_cb(GIOChannel *source, GIOCondition condition, gpointer data)
{
	gint fd, sock;
	gchar buf[BUFFER_LENGTH];
	struct sockaddr_in caddr;
	socklen_t caddr_len = sizeof(caddr);
	GtkWidget *window = data;
	gboolean popup = FALSE;

	fd = g_io_channel_unix_get_fd(source);
	sock = accept(fd, (struct sockaddr *)&caddr, &caddr_len);

	/* first get the command */
	while (socket_fd_gets(sock, buf, sizeof(buf)) != -1)
	{
		if (strncmp(buf, "open", 4) == 0)
		{
			cl_options.readonly = strncmp(buf+4, "ro", 2) == 0; /* open in readonly? */
			while (socket_fd_gets(sock, buf, sizeof(buf)) != -1 && *buf != '.')
			{
				handle_input_filename(g_strstrip(buf));
			}
			popup = TRUE;
		}
		else if (strncmp(buf, "doclist", 7) == 0)
		{
			gchar *doc_list = build_document_list();
			if (NZV(doc_list))
				socket_fd_write_all(sock, doc_list, strlen(doc_list));
			else
				/* send ETX (end-of-text) in case we have no open files, we must send anything
				 * otherwise the client would hang on reading */
				socket_fd_write_all(sock, "\3", 1);
			g_free(doc_list);
		}
		else if (strncmp(buf, "line", 4) == 0)
		{
			while (socket_fd_gets(sock, buf, sizeof(buf)) != -1 && *buf != '.')
			{
				g_strstrip(buf); /* remove \n char */
				/* on any error we get 0 which should be safe enough as fallback */
				cl_options.goto_line = atoi(buf);
			}
		}
		else if (strncmp(buf, "column", 6) == 0)
		{
			while (socket_fd_gets(sock, buf, sizeof(buf)) != -1 && *buf != '.')
			{
				g_strstrip(buf); /* remove \n char */
				/* on any error we get 0 which should be safe enough as fallback */
				cl_options.goto_column = atoi(buf);
			}
		}
#ifdef G_OS_WIN32
		else if (strncmp(buf, "window", 6) == 0)
		{
			HWND hwnd = (HWND) gdk_win32_drawable_get_handle(
				GDK_DRAWABLE(gtk_widget_get_window(window)));
			socket_fd_write(sock, (gchar *)&hwnd, sizeof(hwnd));
		}
#endif
	}

	if (popup)
	{
#ifdef GDK_WINDOWING_X11
		/* Set the proper interaction time on the window. This seems necessary to make
		 * gtk_window_present() really bring the main window into the foreground on some
		 * window managers like Gnome's metacity.
		 * Code taken from Gedit. */
		gdk_x11_window_set_user_time(gtk_widget_get_window(window),
			gdk_x11_get_server_time(gtk_widget_get_window(window)));
#endif
		gtk_window_present(GTK_WINDOW(window));
#ifdef G_OS_WIN32
		gdk_window_show(gtk_widget_get_window(window));
#endif
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
	/* checking for non-blocking mode */
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
		geany_debug("Socket IO timeout");
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
