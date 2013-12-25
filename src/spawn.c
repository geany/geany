/*
 *      spawn.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2013 Dimitar Toshkov Zhekov <dimitar(dot)zhekov(at)gmail(dot)com>
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

/* An ongoing effort to improve the tool spawning situation under Windows.
 * In particular:
 *    - There is no g_shell_parse_argv() for windows. It's not hard to write one, 
 *      but the command line recreated by mscvrt may be wrong.
 *    - GLib converts the argument vector to UNICODE. For non-UTF8 arguments,
 *      the result is often "Invalid string in argument vector at %d: %s: Invalid
 *      byte sequence in conversion input" (YMMV). Our tools (make, grep, gcc, ...)
 *      are "ANSI", so converting to UNICODE and then back only causes problems.
 *    - For various reasons, GLib uses an intermediate program to start children
 *      (see gspawn-win32.c), the result being that children's children output
 *      (such as make -> gcc) is lost.
 *    - With non-blocking pipes, the g_io_add_watch() callbacks are not invoked,
 *      while with blocking pipes, the child blocks on output larger than the
 *      pipe buffer (4KB by default), and Geany blocks waiting for the child.
 *      (g_io_channel_set_flags(G_IO_FLAG_NONBLOCK) does not work too, but that's
 *      a small problem)
 *
 * Note that if no pipe I/O with the child is required, the g_spawn...() functions
 * will generally work. They open the child in a normal window, while spawn_...()
 * use a minimized-by-default window, more suitable for a tool.
 *
 * This module does not depend on Geany.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
# define _NO_W32_PSEUDO_MODIFIERS
# include <windows.h>
# include <errno.h>
# include <io.h>
#else  /* G_OS_WIN32 */
# include <sys/types.h>
# include <sys/wait.h>
# include <signal.h>
# include <unistd.h>
# ifndef O_NONBLOCK
# define O_NONBLOCK O_NDELAY
# endif
#endif  /* G_OS_WIN32 */

#include "spawn.h"

#ifndef _
# define _
#endif

#ifdef G_OS_WIN32
# define BLANKS " \t"
#endif


/**
 *  Checks whether a command line is valid.
 *
 *  All OS:
 *     - any leading spaces and tabs are ignored
 *     - a command consisting solely of spaces and tabs is invalid
 *  *nix:
 *     - the standard shell quoting and escaping rules are used, see @c g_shell_parse_argv()
 *     - as a consequence, an unqouted # at the start of an argument comments to the end of line
 *  Windows:
 *     - a quoted program name must be entirely inside the quotes ("C:\Foo\Bar".pdf is actually
 *       started as "C:\Foo\Bar.exe")
 *     - the standard Windows quoting rules are used: double quote is escaped with slash, and
 *       any literal slashes before a double quote must be duplicated.
 *
 *  @param command_line the command line to check.
 *  @param execute whether to check (using @c g_find_program_in_path() if the program specified
 *         in the command line exists and is executable.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE if an error was set.
 **/
gboolean spawn_check_command(const gchar *command_line, gboolean execute, GError **error)
{
	gchar *program;

#ifdef G_OS_WIN32
	gboolean open_quote = FALSE;
	const gchar *s, *arguments;

	while (*command_line && strchr(BLANKS, *command_line))
		command_line++;

	if (!*command_line)
	{
		/* from glib */
		g_set_error(error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING,
			_("Text was empty (or contained only whitespace)"));
		return FALSE;
	}

	if (*command_line == '"')
	{
		command_line++;
		/* Windows allows "foo.exe, but we may have extra arguments */
		if ((s = strchr(command_line, '"')) == NULL)
		{
			/* from glib, except for the fixed quote */
			g_set_error(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				_("Text ended before matching quote was found for %c."
				  " (The text was '%s')"), '"', command_line);
			return FALSE;
		}

		if (!strchr(BLANKS, s[1]))
		{
			g_set_error(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				_("A quoted Windows program name must be entirely inside the quotes"));
			return FALSE;
		}
	}
	else
	{
		const gchar *quote = strchr(command_line, '"');

		for (s = command_line; !strchr(BLANKS, *s); s++);

		if (quote && quote < s)
		{
			g_set_error(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				_("A quoted Windows program name must be entirely inside the quotes"));
			return FALSE;
		}
	}

	program = g_strndup(command_line, s - command_line);
	arguments = s + (*s == '"');

	for (s = arguments; *s; s++)
	{
		if (*s == '"')
		{
			const char *slash;

			for (slash = s; slash > arguments && slash[-1] == '\\'; slash--);
			if ((s - slash) % 2 == 0)
				open_quote ^= TRUE;
		}
	}

	if (open_quote)
	{
		/* from glib, except for the fixed quote */
		g_set_error(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
			_("Text ended before matching quote was found for %c."
			  " (The text was '%s')"), '"', command_line);
		return FALSE;
	}
#else  /* G_OS_WIN32 */
	int argc;
	char **argv;

	if (!g_shell_parse_argv(command_line, &argc, &argv, error))
		return FALSE;

	program = g_strdup(argv[0]);  /* note: empty string results in parse error */
	g_strfreev(argv);
#endif  /* G_OS_WIN32 */

	if (execute && !g_find_program_in_path(program))
	{
		g_set_error(error, G_SHELL_ERROR, G_SHELL_ERROR_FAILED,  /* or SPAWN error? */
			_("Program '%s' not found in path"), program);
		g_free(program);
		return FALSE;
	}

	g_free(program);
	return TRUE;
}


/**
 *  Kills a process.
 *
 *  @param pid id of the process to kill.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE if an error was set.
 **/
gboolean spawn_kill_process(GPid pid, GError **error)
{
#ifdef G_OS_WIN32
	if (!TerminateProcess(pid, 15))
	{
		gchar *message = g_win32_error_message(GetLastError());

		g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
			_("TerminateProcess() failed: %s"), message);
		g_free(message);
		return FALSE;
	}
#else
	if (kill(pid, SIGTERM))
	{
		g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, g_strerror(errno));
		return FALSE;
	}
#endif
	return TRUE;
}


#ifdef G_OS_WIN32
static gchar *spawn_create_process_with_pipes(char *command_line, const char *working_directory,
	void *environment, HANDLE *hprocess, int *stdin_fd, int *stdout_fd, int *stderr_fd)
{
	enum { WRITE_STDIN, READ_STDOUT, READ_STDERR, READ_STDIN, WRITE_STDOUT, WRITE_STDERR };
	CRITICAL_SECTION critical;  /* probably unneeded, but good practice */
	STARTUPINFO startup;
	PROCESS_INFORMATION process;
	HANDLE hpipe[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	int *fd[3] = { stdin_fd, stdout_fd, stderr_fd };
	const char *failed;     /* failed WIN32/CRTL function, if any */
	gchar *message = NULL;  /* glib WIN32/CTRL error message */
	gchar *failure = NULL;  /* full error text */
	int i;

	ZeroMemory(&startup, sizeof startup);
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESHOWWINDOW;
	startup.wShowWindow = SW_SHOWMINNOACTIVE;  /* so you can switch to it if needed */

	if (stdin_fd || stdout_fd || stderr_fd)
	{
		/* Not all programs accept mixed NULL and non-NULL hStd*, so we create all */
		startup.dwFlags |= STARTF_USESTDHANDLES;

		for (i = 0; i < 3; i++)
		{
			static int pindex[3][2] = { { READ_STDIN, WRITE_STDIN },
					{ READ_STDOUT, WRITE_STDOUT }, { READ_STDERR, WRITE_STDERR } };
			DWORD state;

			if (!CreatePipe(&hpipe[pindex[i][0]], &hpipe[pindex[i][1]], NULL, 0))
			{
				hpipe[pindex[i][0]] = hpipe[pindex[i][1]] = NULL;
				failed = "CreatePipe";
				goto leave;
			}

			if (fd[i])
			{
				static int mode[3] = { _O_WRONLY, _O_RDONLY, _O_RDONLY };

				if (!GetNamedPipeHandleState(hpipe[i], &state, NULL, NULL, NULL, NULL, 0))
				{
					failed = "GetNamedPipeHandleState";
					goto leave;
				}

				state |= PIPE_NOWAIT;
				if (!SetNamedPipeHandleState(hpipe[i], &state, NULL, NULL))
				{
					failed = "SetNamedPipeHandleState";
					goto leave;
				}

				if ((*fd[i] = _open_osfhandle((intptr_t) hpipe[i], mode[i])) == -1)
				{
					failed = "_open_osfhandle";
					message = g_strdup(g_strerror(errno));
					goto leave;
				}
			}
			else if (!CloseHandle(hpipe[i]))
			{
				failed = "CloseHandle";
				goto leave;
			}

			if (!SetHandleInformation(hpipe[i + 3], HANDLE_FLAG_INHERIT,
				HANDLE_FLAG_INHERIT))
			{
				failed = "SetHandleInformation";
				goto leave;
			}
		}
	}

	startup.hStdInput = hpipe[READ_STDIN];
	startup.hStdOutput = hpipe[WRITE_STDOUT];
	startup.hStdError = hpipe[WRITE_STDERR];

	if (!CreateProcess(NULL, command_line, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS,
		environment, working_directory, &startup, &process))
	{
		failed = "CreateProcess";
		/* no errors beyond this point */
	}
	else
	{
		failed = NULL;
		*hprocess = process.hProcess;
	}

leave:
	if (failed)
	{
		if (!message)
			message = g_win32_error_message(GetLastError());

		failure = g_strdup_printf("%s() failed: %s", failed, message);
		g_free(message);
	}

	if (startup.dwFlags & STARTF_USESTDHANDLES)
	{
		for (i = 0; i < 3; i++)
		{
			if (failed)
			{
				if (fd[i] && *fd[i] != -1)
					_close(*fd[i]);
				else if (hpipe[i])
					CloseHandle(hpipe[i]);
			}

			if (hpipe[i + 3])
				CloseHandle(hpipe[i + 3]);
		}
	}

	LeaveCriticalSection(&critical);
	return failure;
}


static void spawn_append_argument(GString *command, const char *text)
{
	const char *s;

	g_string_append_c(command, ' ');
	for (s = text; *s; s++)
		if (*s == '"' || g_ascii_isspace(*s))  /* are all RTL-s standartized? */
			break;

	if (*text && !*s)
		g_string_append(command, text);
	else
	{
		g_string_append_c(command, '"');

		for (s = text; *s; s++)
		{
			const char *slash;

			for (slash = s; *slash == '\\'; slash++);

			if (slash > s)
			{
				g_string_append_len(command, s, slash - s);

				if (strchr("\"", *slash))
				{
					g_string_append_len(command, s, slash - s);

					if (!*slash)
						break;
				}

				s = slash;
			}

			if (*s == '"')
				g_string_append_c(command, '\\');

			g_string_append_c(command, *s);
		}

		g_string_append_c(command, '"');
	}
}


static void spawn_close_pid(GPid pid, G_GNUC_UNUSED gint status, G_GNUC_UNUSED gpointer data)
{
	g_spawn_close_pid(pid);
}
#endif /* G_OS_WIN32 */


/**
 *  Executes a child program asynchronously and setups pipes.
 *
 *  This is the low-level spawning function. Please use @c spawn_with_callbacks() unless
 *  you need to setup a specific event loop.
 *
 *  Either a command line or an argument vector must be passed. If both are present, the
 *  argument vector is appended to the command line. An empty command line is not allowed.
 *
 *  On successful execution, any fd-s are set to non-blocking mode.
 *  If a @a child_pid is passed, it's your responsibility to call @c g_spawn_close_pid().
 *
 *  @param working_directory child's current working directory, or @c NULL.
 *  @param command_line child program and arguments, or @c NULL.
 *  @param argv child's argument vector, or @c NULL.
 *  @param envp child's environment, or @c NULL.
 *  @param child_pid return location for child process ID, or @c NULL.
 *  @param stdin_fd return location for file descriptor to write to child's stdin, or @c NULL.
 *  @param stdout_fd return location for file descriptor to read child's stdout, or @c NULL.
 *  @param stderr_fd return location for file descriptor to read child's stderr, or @c NULL.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE if an error was set.
 **/
gboolean spawn_async_with_pipes(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GPid *child_pid, gint *stdin_fd, gint *stdout_fd,
	gint *stderr_fd, GError **error)
{
	if (command_line)
	{
		if (!spawn_check_command(command_line, FALSE, error))
			return FALSE;
	}
	else if (!argv || !*argv)
	{
		g_set_error(error, G_SPAWN_ERROR, G_SHELL_ERROR_EMPTY_STRING,
			_("Neigher command line nor argument vector specified"));
		return FALSE;
	}

#ifdef G_OS_WIN32
	GString *command = g_string_new(command_line);
	GArray *environment = g_array_new(TRUE, FALSE, sizeof(char));
	GPid pid;
	gchar *failure;

	while (argv && *argv)
		spawn_append_argument(command, *argv++);

#ifdef SPAWN_TEST
	fprintf(stderr, "full command line: %s\n", command->str);
#endif

	while (envp && *envp)
	{
		g_array_append_vals(environment, *envp, strlen(*envp) + 1);
		envp++;
	}
	
	failure = spawn_create_process_with_pipes(command->str, working_directory,
		envp ? environment->data : NULL, child_pid ? child_pid : &pid,
		stdin_fd, stdout_fd, stderr_fd);

	g_string_free(command, TRUE);
	g_array_free(environment, TRUE);

	if (failure)
	{
		g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s", failure);
		g_free(failure);
		return FALSE;
	}
	else if (!child_pid)
		g_child_watch_add(pid, spawn_close_pid, NULL);

	return TRUE;
#else  /* G_OS_WIN32 */
	int cl_argc;
	char **full_argv;
	gboolean spawned;

	if (command_line)
	{
		int argc = 0;
		char **cl_argv;

		if (!g_shell_parse_argv(command_line, &cl_argc, &cl_argv, error))
			return FALSE;

		if (argv)
			for (argc = 0; argv[argc]; argc++);

		full_argv = g_new(gchar *, cl_argc + argc + 1);
		memcpy(full_argv, cl_argv, cl_argc * sizeof(gchar *));
		memcpy(full_argv + cl_argc, argv, argc * sizeof(gchar *));
		full_argv[cl_argc + argc] = NULL;
	}
	else
		full_argv = argv;

	spawned = g_spawn_async_with_pipes(working_directory, full_argv, envp,
		G_SPAWN_SEARCH_PATH | (child_pid ? G_SPAWN_DO_NOT_REAP_CHILD : 0), NULL, NULL,
		child_pid, stdin_fd, stdout_fd, stderr_fd, error);

	if (spawned)
	{
		int *fd[3] = { stdin_fd, stdout_fd, stderr_fd };
		int i;

		for (i = 0; i < 3; i++)
			if (fd[i])
				fcntl(*fd[i], F_SETFL, O_NONBLOCK);
	}

	if (full_argv != argv)
	{
		full_argv[cl_argc] = NULL;
		g_strfreev(full_argv);
	}

	return spawned;
#endif  /* G_OS_WIN32 */
}


enum { OUT, ERR, IN };

typedef struct _SpawnSource
{
	GSource source;
	GPollFD std[3];
	gchar *stdin_text;
	const char *stdin_pos;
	size_t stdin_remaining;
#ifdef G_OS_WIN32
	DWORD last_send_ticks;
#endif
	GIOChannel *ioc[2];
	void (*io_cb[2])(const gchar *text, gpointer user_data);
	gpointer io_data[2];
	GPid pid;
	void (*exit_cb)(gint status, gpointer user_data);
	gpointer exit_data;
	/* interactive */
	GtkWidget *dialog;
	const gchar *command;
} SpawnSource;


static void spawn_source_exit_cb(GPid pid, gint status, gpointer source)
{
	SpawnSource *sp = (SpawnSource *) source;

	if (sp->exit_cb)
		sp->exit_cb(status, sp->exit_data);

	g_spawn_close_pid(pid);
	g_source_unref((GSource *) source);
}


static gboolean spawn_source_prepare(G_GNUC_UNUSED GSource *source, gint *timeout)
{
	*timeout = -1;
	return FALSE;
}


#ifdef G_OS_WIN32
static gboolean spawn_source_peek_pipe(GPollFD *poll)
{
	DWORD available;

	return poll->fd != -1 && (!PeekNamedPipe((HANDLE) _get_osfhandle(poll->fd), NULL, 0, NULL,
		&available, NULL) || available);
}


static gboolean spawn_source_check(GSource *source)
{
	SpawnSource *sp = (SpawnSource *) source;

	return (sp->std[IN].fd != -1 && GetTickCount() - sp->last_send_ticks >= 50) ||
		spawn_source_peek_pipe(&sp->std[OUT]) || spawn_source_peek_pipe(&sp->std[ERR]);
}
#else  /* G_OS_WIN32 */
static gboolean spawn_source_check(GSource *source)
{
	SpawnSource *sp = (SpawnSource *) source;

	return (sp->std[IN].fd != -1 && sp->std[IN].revents) ||
		(sp->std[OUT].fd != -1 && sp->std[OUT].revents) ||
		(sp->std[ERR].fd != -1 && sp->std[ERR].revents);
}
#endif  /* G_OS_WIN32 */


static gboolean spawn_source_dispatch(GSource *source, G_GNUC_UNUSED GSourceFunc callback,
	G_GNUC_UNUSED gpointer user_data)
{
	SpawnSource *sp = (SpawnSource *) source;
	gchar *line;
	int i;
#ifdef G_OS_WIN32
	DWORD status = STILL_ACTIVE;
#else
	int status = -1;
#endif

	if (sp->std[IN].fd != -1)
	{
		ssize_t count = write(sp->std[IN].fd, sp->stdin_pos, sp->stdin_remaining);

		if (count > 0)
		{
			sp->stdin_remaining -= count;

			if (sp->stdin_remaining)
			{
				sp->stdin_pos += count;
			#ifdef G_OS_WIN32
				sp->last_send_ticks = GetTickCount();
			#endif
			}
			else
			{
				close(sp->std[IN].fd);
				sp->std[IN].fd = -1;
				g_free(sp->stdin_text);
				sp->stdin_text = NULL;

				if (sp->std[OUT].fd == -1 && sp->std[ERR].fd == -1)
				{
					g_source_ref(source);
					g_child_watch_add(sp->pid, spawn_source_exit_cb, sp);
					return FALSE;
				}

			#ifdef G_OS_UNIX
				g_source_remove_poll(source, &sp->std[IN]);
			#endif
			}
		}
	}

	for (i = 0; i < 2; i++)
	{
		while (sp->std[i].fd != -1 && g_io_channel_read_line(sp->ioc[i], &line, NULL,
			NULL, NULL) == G_IO_STATUS_NORMAL && line)
		{
			if (sp->io_cb[i])
				sp->io_cb[i](line, sp->io_data[i]);

			g_free(line);
		}
	}

#ifdef G_OS_WIN32
	if (!GetExitCodeProcess(sp->pid, &status) || status != STILL_ACTIVE)
#else
	if (waitpid(sp->pid, &status, WNOHANG))
#endif
	{
		if (sp->exit_cb)
			sp->exit_cb(status, sp->exit_data);

		if (sp->dialog)
			gtk_widget_destroy(sp->dialog);

		g_free(sp->stdin_text);
		g_spawn_close_pid(sp->pid);
		return FALSE;
	}

	return TRUE;
}


static void spawn_source_finalize(G_GNUC_UNUSED GSource *source)
{
}


static GSourceFuncs spawn_source_funcs = { spawn_source_prepare, spawn_source_check,
	spawn_source_dispatch, spawn_source_finalize, NULL, NULL };


static void spawn_dialog_response(GtkDialog *dialog, G_GNUC_UNUSED gint response_id,
	gpointer user_data)
{
	SpawnSource *sp = (SpawnSource *) user_data;

	gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(sp->dialog),
		_("Stopping %s..."), sp->command);
	spawn_kill_process(sp->pid, NULL);
	g_signal_stop_emission_by_name(dialog, "response");
}


/**
 *  Executes a child program and setups callbacks.
 *
 *  Either a command line or an argument vector must be passed. If both are present, the
 *  argument vector is appended to the command line. An empty command line is not allowed.
 *
 *  If a @a block_window is specified, the function will not return until the child exits,
 *  but all other windows (if any) will receive events.
 *
 *  @a stdin_text will be gfree()-d automatically when it's sent entirely to the child, or
 *  when the child exits.
 *
 *  The text passed to @a stdout_cb and @a stderr_cb is read with @c g_io_channel_read_line(),
 *  but is guaranteed to be non-@c NULL.
 *  After @a exit_cb is invoked, there will be no more data to read.
 *
 *  @param working_directory child's current working directory, or @c NULL.
 *  @param command_line child program and arguments, or @c NULL.
 *  @param argv child's argument vector, or @c NULL.
 *  @param envp child's environment, or @c NULL.
 *  @param block_window window whose message loop to block until the child exits, or @c NULL.
 *  @param stdin_text text to send to childs's stdin, or @c NULL.
 *  @param stdout_cb callback to receive child's stdout, or @c NULL.
 *  @param stdout_data data to pass to @a stdout_cb.
 *  @param stderr_cb callback to receive child's stderr, or @c NULL.
 *  @param stdout_data data to pass to @a stderr_cb, or NULL.
 *  @param exit_cb callback to invoke when the child exits, or @c NULL.
 *  @param exit_data data to pass to @a exit_cb.
 *  @param child_pid return location for child process ID, or @c NULL.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE if an error was set.
 **/
gboolean spawn_with_callbacks(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GtkWindow *block_window, gchar *stdin_text,
	void (*stdout_cb)(const gchar *text, gpointer user_data), gpointer stdout_data,
	void (*stderr_cb)(const gchar *text, gpointer user_data), gpointer stderr_data,
	void (*exit_cb)(gint status, gpointer user_data), gpointer exit_data, GPid *child_pid,
	GError **error)
{
	GSource *source = g_source_new(&spawn_source_funcs, sizeof(SpawnSource));
	SpawnSource *sp = (SpawnSource *) source;

	sp->std[IN].fd = sp->std[OUT].fd = sp->std[ERR].fd = -1;
	sp->std[IN].events = G_IO_OUT | G_IO_ERR;
	sp->std[OUT].events = sp->std[ERR].events = G_IO_IN | G_IO_HUP | G_IO_ERR;

	if (spawn_async_with_pipes(working_directory, command_line, argv, envp, &sp->pid,
		stdin_text ? &sp->std[IN].fd : NULL, stdout_cb ? &sp->std[OUT].fd : NULL,
		stderr_cb ? &sp->std[ERR].fd : NULL, error))
	{
		int i;

		if (child_pid)
			*child_pid = sp->pid;

		if (stdin_text)
		{
		#ifdef G_OS_WIN32
			sp->last_send_ticks = GetTickCount();
		#else
			g_source_add_poll(source, &sp->std[IN]);
		#endif
			sp->stdin_pos = sp->stdin_text = stdin_text;
			sp->stdin_remaining = strlen(sp->stdin_text);
		}

		sp->io_cb[OUT] = stdout_cb;
		sp->io_data[OUT] = stdout_data;
		sp->io_cb[ERR] = stderr_cb;
		sp->io_data[ERR] = stderr_data;

		for (i = 0; i < 2; i++)
		{
			if (sp->std[i].fd != -1)
			{
			#ifdef G_OS_WIN32
				sp->ioc[i] = g_io_channel_win32_new_fd(sp->std[i].fd);
			#else
				sp->ioc[i] = g_io_channel_unix_new(sp->std[i].fd);
				g_source_add_poll(source, &sp->std[i]);
			#endif
				g_io_channel_set_encoding(sp->ioc[i], NULL, NULL);
			}
		}

		sp->exit_cb = exit_cb;
		sp->exit_data = exit_data;

		if (stdin_text || stdout_cb || stderr_cb)
		{
			g_source_attach(source, NULL);
			g_source_unref(source);
		}
		else
			g_child_watch_add(sp->pid, spawn_source_exit_cb, sp);

		if (block_window)
		{
			sp->dialog = gtk_message_dialog_new(block_window, GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_OTHER, GTK_BUTTONS_CANCEL, _("Please wait"));
			sp->command = command_line ? command_line : argv[0];
			gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(sp->dialog),
				_("Running %s..."), sp->command);
			g_signal_connect(sp->dialog, "response", G_CALLBACK(spawn_dialog_response), sp);
			gtk_dialog_run(GTK_DIALOG(sp->dialog));
		}

		return TRUE;
	}

	g_source_unref(source);
	return FALSE;
}


static void spawn_capture_text(const gchar *text, gpointer user_data)
{
	g_string_append((GString *) user_data, text);
}


typedef struct _SpawnCapture
{
	gchar **stdout_text;
	GString *stdout_str;
	gchar **stderr_text;
	GString *stderr_str;
	gint *exit_status;
} SpawnCapture;


static void spawn_capture_exit(gint status, gpointer user_data)
{
	SpawnCapture *sp = (SpawnCapture *) user_data;

	if (sp->stdout_text)
		*sp->stdout_text = g_string_free(sp->stdout_str, FALSE);

	if (sp->stderr_text)
		*sp->stderr_text = g_string_free(sp->stderr_str, FALSE);

	if (sp->exit_status)
		*sp->exit_status = status;

	g_free(user_data);
}


/**
 *  Executes a child program and captures it's output.
 *
 *  Either a command line or an argument vector must be passed. If both are present, the
 *  argument vector is appended to the command line. An empty command line is not allowed.
 *
 *  If a @a block_window is specified (recommended), the function will not return until the
 *  child exits, but all other windows (if any) will receive events. Otherwise it'll return,
 *  and it's your responsibility to keep @a stdout_text, @a stderr_text and @a exit_status
 *  valid until the execution finishes.
 *
 *  @a stdin_text will be gfree()-d automatically when it's sent entirely to the child, or
 *  when the child exits.
 *
 *  @a stdout_text and @a stderr_text will be set when the child exits.
 *
 *  @param working_directory child's current working directory, or @c NULL.
 *  @param command_line child program and arguments, or @c NULL.
 *  @param argv child's argument vector, or @c NULL.
 *  @param envp child's environment, or @c NULL.
 *  @param block_window window whose message loop to block until the child exits, or @c NULL.
 *  @param stdin_text text to send to childs's stdin, or @c NULL.
 *  @param stdout_text return location for the child's stdout, or NULL.
 *  @param stderr_text return location for the child's stderr, or NULL.
 *  @param exit_status return location for the child exit code, or NULL.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE if an error was set.
 **/
gboolean spawn_with_capture(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GtkWindow *block_window, gchar *stdin_text,
	gchar **stdout_text, gchar **stderr_text, gint *exit_status, GError **error)
{
	SpawnCapture *sp = g_new(SpawnCapture, 1);

	sp->stdout_text = stdout_text;
	sp->stdout_str = stdout_text ? g_string_sized_new(1024) : NULL;
	sp->stderr_text = stderr_text;
	sp->stderr_str = stderr_text ? g_string_sized_new(1024) : NULL;
	sp->exit_status = exit_status;

	return spawn_with_callbacks(working_directory, command_line, argv, envp, block_window,
		stdin_text, stdout_text ? spawn_capture_text : NULL, sp->stdout_str,
		stderr_text ? spawn_capture_text : NULL, sp->stderr_str, spawn_capture_exit, sp,
		NULL, error);
}


#ifdef SPAWN_TEST
#include <stdio.h>
#include <stdlib.h>

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <sys/wait.h>
#endif


static const char *read_line(const char *prompt, char *buffer, size_t size)
{
	fputs(prompt, stdout);

	if ((buffer = fgets(buffer, size, stdin)) != NULL)
	{
		char *s = strchr(buffer, '\n');

		if (*s)
			*s = '\0';
	}

	return buffer;
}


static void printer(const gchar *s, gpointer user_data)
{
	printf("%s: %s", user_data == stdout ? "stdout" : "stderr", s);
}


static void exit_command(gint status, gpointer user_data)
{
	printf("finished, full exit code %d\n", status);
	g_main_loop_quit((GMainLoop *) user_data);
}


typedef struct _CaptureData
{
	gchar *stdout_text;
	GMainLoop *loop;
} CaptureData;


static gboolean capture(gpointer user_data)
{
	CaptureData *cd = (CaptureData *) user_data;

	if (cd->stdout_text)
	{
		g_main_loop_quit(cd->loop);
		return FALSE;
	}

	return TRUE;
}


int main(int argc, char **argv)
{
	char *test_type;

	if (argc < 2)
	{
		fputs("usage: spawn <test-type>\n", stderr);
		return 1;
	}

	test_type = argv[1];

	if (!strcmp(test_type, "syntax") || !strcmp(test_type, "syntexec"))
	{
		char command_line[0x100];

		while (read_line("command line: ", command_line, sizeof command_line) != NULL)
		{
			GError *error = NULL;

			if (spawn_check_command(command_line, argv[1][4] == 'e', &error))
				puts("valid");
			else
			{
				printf("error: %s\n", error->message);
				g_error_free(error);
			}
		}
	}
	else if (!strcmp(test_type, "execute"))
	{
		char command_line[0x100];

		while (read_line("command line: ", command_line, sizeof command_line) != NULL)
		{
			char args[4][0x100];
			char envs[4][0x100];
			char *argv[] = { args[0], args[1], args[2], args[3], NULL };
			char *envp[] = { envs[0], envs[1], envs[2], envs[3], NULL };
			int i;
			GPid pid;
			GError *error = NULL;

			puts("up to 4 arguments");
			for (i = 0; i < 4 && read_line("argument: ", args[i], sizeof args[i]) != NULL; i++)
			{
				if (!*args[i])
					break;
			}
			argv[i] = NULL;

			puts("up to 4 variables, or empty line for parent environment");
			for (i = 0; i < 4 && read_line("variable: ", envs[i], sizeof envs[i]) != NULL; i++)
			{
				if (!*envp[i])
					break;
			}
			envp[i] = NULL;

			if (spawn_async_with_pipes(NULL, *command_line ? command_line : NULL, argv,
				i ? envp : NULL, &pid, NULL, NULL, NULL, &error))
			{
			#ifdef G_OS_WIN32
				DWORD status;

				WaitForSingleObject(pid, INFINITE);
				GetExitCodeProcess(pid, &status);
				printf("finished, full exit code %lu\n", (unsigned long) status);
			#else  /* G_OS_WIN32 */
				int status;

				waitpid(pid, &status, 0);
				printf("finished, full exit code %d\n", status);
			#endif  /* G_OS_WIN32 */
			}
			else
			{
				printf("error: %s\n", error->message);
				g_error_free(error);
			}
		}
	}
	else if (!strcmp(test_type, "redirect") || !strcmp(test_type, "redinput"))
	{
		char command_line[0x100];
		char stdin_buffer[0x100];
		gboolean output = test_type[4] == 'r';

		while (read_line("command line: ", command_line, sizeof command_line) != NULL &&
			read_line("text to send: ", stdin_buffer, sizeof stdin_buffer) != NULL)
		{
			gchar *stdin_text = *stdin_buffer ? g_strdup(stdin_buffer) : NULL;
			GMainLoop *loop = g_main_loop_new(NULL, TRUE);
			GError *error = NULL;

			if (spawn_with_callbacks(NULL, command_line, NULL, NULL, NULL, stdin_text,
				output ? printer : NULL, stdout, output ? printer : NULL, stderr,
				exit_command, loop, NULL, &error))
			{
				g_main_loop_run(loop);
			}
			else
			{
				g_free(stdin_text);
				printf("error: %s\n", error->message);
				g_error_free(error);
			}

			g_main_loop_unref(loop);
		}
	}
	else if (!strcmp(test_type, "capture"))
	{
		char command_line[0x100];
		char stdin_buffer[0x100];

		while (read_line("command line: ", command_line, sizeof command_line) != NULL &&
			read_line("text to send: ", stdin_buffer, sizeof stdin_buffer) != NULL)
		{
			gchar *stdin_text = *stdin_buffer ? g_strdup(stdin_buffer) : NULL;
			CaptureData cd = { NULL, g_main_loop_new(NULL, TRUE) };
			gchar *stderr_text;
			gint exit_status;
			GError *error = NULL;

			if (spawn_with_capture(NULL, command_line, NULL, NULL, NULL, stdin_text,
				&cd.stdout_text, &stderr_text, &exit_status, &error))
			{
				g_timeout_add(100, capture, &cd);
				g_main_loop_run(cd.loop);
				printf("stdout: %s\n", cd.stdout_text);
				g_free(cd.stdout_text);
				printf("stderr: %s\n", stderr_text);
				g_free(stderr_text);
				printf("finished, full exit code %d\n", exit_status);
			}
			else
			{
				g_free(stdin_text);
				printf("error: %s\n", error->message);
				g_error_free(error);
			}

			g_main_loop_unref(cd.loop);
		}
	}
	else
	{
		fprintf(stderr, "spawn: unknown test type '%s'", argv[1]);
		return 1;
	}

	return 0;
}
#endif  /* SPAWN_TEST */
