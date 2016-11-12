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
 *      (see gspawn-win32.c), the result being that the grandchildren output (such
 *      as make -> gcc) is not captured.
 *    - With non-blocking pipes, the g_io_add_watch() callbacks are never invoked,
 *      while with blocking pipes, g_io_channel_read_line() blocks.
 *    - Some other problems are explained in separate comments below.
 *
 * Even under Unix, using g_io_channel_read_line() is not a good idea, since it may
 * buffer lines of unlimited length.
 *
 * This module does not depend on Geany when compiled for testing (-DSPAWN_TEST).
 */

/** @file spawn.h
 * Portable and convenient process spawning and communication.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <string.h>

#include "spawn.h"

#ifdef G_OS_WIN32
# include <ctype.h>    /* isspace() */
# include <fcntl.h>    /* _O_RDONLY, _O_WRONLY */
# include <io.h>       /* _open_osfhandle, _close */
# include <windows.h>
#else  /* G_OS_WIN32 */
# include <signal.h>
#endif  /* G_OS_WIN32 */

#ifdef SPAWN_TEST
# define _
# define GEANY_API_SYMBOL
#else
# include "support.h"
#endif

#if ! GLIB_CHECK_VERSION(2, 31, 20) && ! defined(G_SPAWN_ERROR_TOO_BIG)
# define G_SPAWN_ERROR_TOO_BIG G_SPAWN_ERROR_2BIG
#endif

#ifdef G_OS_WIN32
/* Each 4KB under Windows seem to come in 2 portions, so 2K + 2K is more
   balanced than 4095 + 1. May be different on the latest Windows/glib? */
# define DEFAULT_IO_LENGTH 2048
#else
# define DEFAULT_IO_LENGTH 4096

/* helper function that cuts glib citing of the original text on bad quoting: it may be long,
   and only the caller knows whether it's UTF-8. Thought we lose the ' or " failed info. */
static gboolean spawn_parse_argv(const gchar *command_line, gint *argcp, gchar ***argvp,
	GError **error)
{
	GError *gerror = NULL;

	if (g_shell_parse_argv(command_line, argcp, argvp, &gerror))
		return TRUE;

	g_set_error_literal(error, gerror->domain, gerror->code,
		gerror->code == G_SHELL_ERROR_BAD_QUOTING ?
		_("Text ended before matching quote was found") : gerror->message);
	g_error_free(gerror);
	return FALSE;
}
#endif

#define G_IO_FAILURE (G_IO_ERR | G_IO_HUP | G_IO_NVAL)  /* always used together */


/*
 *  Checks whether a command line is syntactically valid and extracts the program name from it.
 *
 *  See @c spawn_check_command() for details.
 *
 *  @param command_line the command line to check and get the program name from.
 *  @param error return location for error.
 *
 *  @return allocated string with the program name on success, @c NULL on error.
 */
static gchar *spawn_get_program_name(const gchar *command_line, GError **error)
{
	gchar *program;

#ifdef G_OS_WIN32
	gboolean open_quote = FALSE;
	const gchar *s, *arguments;

	g_return_val_if_fail(command_line != NULL, FALSE);

	while (*command_line && strchr(" \t\r\n", *command_line))
		command_line++;

	if (!*command_line)
	{
		g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_EMPTY_STRING,
			/* TL note: from glib */
			_("Text was empty (or contained only whitespace)"));
		return FALSE;
	}

	/* To prevent Windows from doing something weird, we want to be 100% sure that the
	   character after the program name is a delimiter, so we allow space and tab only. */

	if (*command_line == '"')
	{
		command_line++;
		/* Windows allows "foo.exe, but we may have extra arguments */
		if ((s = strchr(command_line, '"')) == NULL)
		{
			g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				_("Text ended before matching quote was found"));
			return FALSE;
		}

		if (!strchr(" \t", s[1]))  /* strchr() catches s[1] == '\0' */
		{
			g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
				_("A quoted Windows program name must be entirely inside the quotes"));
			return FALSE;
		}
	}
	else
	{
		const gchar *quote = strchr(command_line, '"');

		/* strchr() catches *s == '\0', and the for body is empty */
		for (s = command_line; !strchr(" \t", *s); s++);

		if (quote && quote < s)
		{
			g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
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
		g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_BAD_QUOTING,
			_("Text ended before matching quote was found"));
		g_free(program);
		return FALSE;
	}
#else  /* G_OS_WIN32 */
	int argc;
	char **argv;

	if (!spawn_parse_argv(command_line, &argc, &argv, error))
		return FALSE;

	/* empty string results in parse error, so argv[0] is not NULL */
	program = g_strdup(argv[0]);
	g_strfreev(argv);
#endif  /* G_OS_WIN32 */

	return program;
}


/**
 *  Checks whether a command line is valid.
 *
 *  Checks if @a command_line is syntactically valid.
 *
 *  All OS:
 *     - any leading spaces, tabs and new lines are skipped
 *     - an empty command is invalid
 *
 *  Unix:
 *     - the standard shell quoting and escaping rules are used, see @c g_shell_parse_argv()
 *     - as a consequence, an unqouted # at the start of an argument comments to the end of line
 *
 *  Windows:
 *     - leading carriage returns are skipped too
 *     - a quoted program name must be entirely inside the quotes. No "C:\Foo\Bar".pdf or
 *       "C:\Foo\Bar".bat, which would be executed by Windows as `C:\Foo\Bar.exe`
 *     - an unquoted program name may not contain spaces. `Foo Bar Qux` will not be considered
 *       `"Foo Bar.exe" Qux` or `"Foo Bar Qux.exe"`, depending on what executables exist, as
 *       Windows normally does.
 *     - the program name must be separated from the arguments by at least one space or tab
 *     - the standard Windows quoting and escaping rules are used: double quote is escaped with
 *       backslash, and any literal backslashes before a double quote must be duplicated.
 *
 *  If @a execute is TRUE, also checks, using @c g_find_program_in_path(), if the program
 *  specified in @a command_line exists and is executable.
 *
 *  @param command_line the command line to check.
 *  @param execute whether to check if the command line is really executable.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE on error.
 *
 *  @since 1.25
 **/
GEANY_API_SYMBOL
gboolean spawn_check_command(const gchar *command_line, gboolean execute, GError **error)
{
	gchar *program = spawn_get_program_name(command_line, error);

	if (!program)
		return FALSE;

	if (execute)
	{
		gchar *executable = g_find_program_in_path(program);

		if (!executable)
		{
			g_set_error_literal(error, G_SHELL_ERROR, G_SHELL_ERROR_FAILED,
				_("Program not found"));
			g_free(program);
			return FALSE;
		}

		g_free(executable);
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
 *  On Unix, sends a SIGTERM to the process.
 *
 *  On Windows, terminates the process with exit code 255 (used sometimes as "generic"
 *  error code, or for programs terminated with Ctrl+C / Ctrl+Break).
 *
 *  @return @c TRUE on success, @c FALSE on error.
 *
 *  @since 1.25
 **/
GEANY_API_SYMBOL
gboolean spawn_kill_process(GPid pid, GError **error)
{
#ifdef G_OS_WIN32
	if (!TerminateProcess(pid, 255))
	{
		gchar *message = g_win32_error_message(GetLastError());

		g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, message);
		g_free(message);
		return FALSE;
	}
#else
	if (kill(pid, SIGTERM))
	{
		g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, g_strerror(errno));
		return FALSE;
	}
#endif
	return TRUE;
}


#ifdef G_OS_WIN32
static gchar *spawn_create_process_with_pipes(wchar_t *w_command_line, const wchar_t *w_working_directory,
	void *w_environment, HANDLE *hprocess, int *stdin_fd, int *stdout_fd, int *stderr_fd)
{
	enum { WRITE_STDIN, READ_STDOUT, READ_STDERR, READ_STDIN, WRITE_STDOUT, WRITE_STDERR };
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	HANDLE hpipe[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	int *fd[3] = { stdin_fd, stdout_fd, stderr_fd };
	const char *failed;     /* failed WIN32/CRTL function, if any */
	gchar *message = NULL;  /* glib WIN32/CTRL error message */
	gchar *failure = NULL;  /* full error text */
	gboolean pipe_io;
	int i;

	ZeroMemory(&startup, sizeof startup);
	startup.cb = sizeof startup;
	pipe_io = stdin_fd || stdout_fd || stderr_fd;

	if (pipe_io)
	{
		startup.dwFlags |= STARTF_USESTDHANDLES;

		/* not all programs accept mixed NULL and non-NULL hStd*, so we create all */
		for (i = 0; i < 3; i++)
		{
			static int pindex[3][2] = { { READ_STDIN, WRITE_STDIN },
					{ READ_STDOUT, WRITE_STDOUT }, { READ_STDERR, WRITE_STDERR } };

			if (!CreatePipe(&hpipe[pindex[i][0]], &hpipe[pindex[i][1]], NULL, 0))
			{
				hpipe[pindex[i][0]] = hpipe[pindex[i][1]] = NULL;
				failed = "create pipe";
				goto leave;
			}

			if (fd[i])
			{
				static int mode[3] = { _O_WRONLY, _O_RDONLY, _O_RDONLY };

				if ((*fd[i] = _open_osfhandle((intptr_t) hpipe[i], mode[i])) == -1)
				{
					failed = "convert pipe handle to file descriptor";
					message = g_strdup(g_strerror(errno));
					goto leave;
				}
			}
			else if (!CloseHandle(hpipe[i]))
			{
				failed = "close pipe";
				goto leave;
			}

			if (!SetHandleInformation(hpipe[i + 3], HANDLE_FLAG_INHERIT,
				HANDLE_FLAG_INHERIT))
			{
				failed = "set pipe handle to inheritable";
				goto leave;
			}
		}
	}

	startup.hStdInput = hpipe[READ_STDIN];
	startup.hStdOutput = hpipe[WRITE_STDOUT];
	startup.hStdError = hpipe[WRITE_STDERR];

	if (!CreateProcessW(NULL, w_command_line, NULL, NULL, TRUE,
		CREATE_UNICODE_ENVIRONMENT | (pipe_io ? CREATE_NO_WINDOW : 0),
		w_environment, w_working_directory, &startup, &process))
	{
		failed = "";  /* report the message only */
		/* further errors will not be reported */
	}
	else
	{
		failed = NULL;
		CloseHandle(process.hThread);  /* we don't need this */

		if (hprocess)
			*hprocess = process.hProcess;
		else
			CloseHandle(process.hProcess);
	}

leave:
	if (failed)
	{
		if (!message)
		{
			size_t len;

			message = g_win32_error_message(GetLastError());
			len = strlen(message);

			/* unlike g_strerror(), the g_win32_error messages may include a final '.' */
			if (len > 0 && message[len - 1] == '.')
				message[len - 1] = '\0';
		}

		if (*failed == '\0')
			failure = message;
		else
		{
			failure = g_strdup_printf("Failed to %s (%s)", failed, message);
			g_free(message);
		}
	}

	if (pipe_io)
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

	return failure;
}


static void spawn_append_argument(GString *command, const char *text)
{
	const char *s;

	if (command->len)
		g_string_append_c(command, ' ');

	for (s = text; *s; s++)
	{
		/* g_ascii_isspace() fails for '\v', and locale spaces (if any) will do no harm */
		if (*s == '"' || isspace(*s))
			break;
	}

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

				if (!*slash || *slash == '"')
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
#endif /* G_OS_WIN32 */


/*
 *  Executes a child program asynchronously and setups pipes.
 *
 *  This is the low-level spawning function. Please use @c spawn_with_callbacks() unless
 *  you need to setup specific event source(s).
 *
 *  A command line or an argument vector must be passed. If both are present, the argument
 *  vector is appended to the command line. An empty command line is not allowed.
 *
 *  Under Windows, if the child is a console application, and at least one file descriptor is
 *  specified, the new child console (if any) will be hidden.
 *
 *  If a @a child_pid is passed, it's your responsibility to invoke @c g_spawn_close_pid().
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
 *  @return @c TRUE on success, @c FALSE on error.
 */
static gboolean spawn_async_with_pipes(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, GPid *child_pid, gint *stdin_fd, gint *stdout_fd,
	gint *stderr_fd, GError **error)
{
	g_return_val_if_fail(command_line != NULL || argv != NULL, FALSE);

#ifdef G_OS_WIN32
	GString *command;
	GArray *w_environment;
	wchar_t *w_working_directory = NULL;
	wchar_t *w_command = NULL;
	gboolean success = TRUE;

	if (command_line)
	{
		gchar *program = spawn_get_program_name(command_line, error);
		const gchar *arguments;

		if (!program)
			return FALSE;

		command = g_string_new(NULL);
		arguments = strstr(command_line, program) + strlen(program);

		if (*arguments == '"')
		{
			g_string_append(command, program);
			arguments++;
		}
		else
		{
			/* quote the first token, to avoid Windows attemps to run two or more
			   unquoted tokens as a program until an existing file name is found */
			g_string_printf(command, "\"%s\"", program);
		}

		g_string_append(command, arguments);
		g_free(program);
	}
	else
		command = g_string_new(NULL);

	w_environment = g_array_new(TRUE, FALSE, sizeof(wchar_t));

	while (argv && *argv)
		spawn_append_argument(command, *argv++);

#if defined(SPAWN_TEST) || defined(GEANY_DEBUG)
	g_message("full spawn command line: %s", command->str);
#endif

	while (envp && *envp && success)
	{
		glong w_entry_len;
		wchar_t *w_entry;
		gchar *tmp = NULL;

		// FIXME: remove this and rely on UTF-8 input
		if (! g_utf8_validate(*envp, -1, NULL))
		{
			tmp = g_locale_to_utf8(*envp, -1, NULL, NULL, NULL);
			if (tmp)
				*envp = tmp;
		}
		/* TODO: better error message */
		w_entry = g_utf8_to_utf16(*envp, -1, NULL, &w_entry_len, error);

		if (! w_entry)
			success = FALSE;
		else
		{
			/* copy the entry, including NUL terminator */
			g_array_append_vals(w_environment, w_entry, w_entry_len + 1);
			g_free(w_entry);
		}

		g_free(tmp);
		envp++;
	}

	/* convert working directory into locale encoding */
	if (success && working_directory)
	{
		GError *gerror = NULL;
		const gchar *utf8_working_directory;
		gchar *tmp = NULL;

		// FIXME: remove this and rely on UTF-8 input
		if (! g_utf8_validate(working_directory, -1, NULL))
		{
			tmp = g_locale_to_utf8(working_directory, -1, NULL, NULL, NULL);
			if (tmp)
				utf8_working_directory = tmp;
		}
		else
			utf8_working_directory = working_directory;

		w_working_directory = g_utf8_to_utf16(utf8_working_directory, -1, NULL, NULL, &gerror);
		if (! w_working_directory)
		{
			/* TODO use the code below post-1.28 as it introduces a new string
			g_set_error(error, gerror->domain, gerror->code,
				_("Failed to convert working directory into locale encoding: %s"), gerror->message);
			*/
			g_propagate_error(error, gerror);
			success = FALSE;
		}
		g_free(tmp);
	}
	/* convert command into locale encoding */
	if (success)
	{
		GError *gerror = NULL;
		const gchar *utf8_cmd;
		gchar *tmp = NULL;

		// FIXME: remove this and rely on UTF-8 input
		if (! g_utf8_validate(command->str, -1, NULL))
		{
			tmp = g_locale_to_utf8(command->str, -1, NULL, NULL, NULL);
			if (tmp)
				utf8_cmd = tmp;
		}
		else
			utf8_cmd = command->str;

		w_command = g_utf8_to_utf16(utf8_cmd, -1, NULL, NULL, &gerror);
		if (! w_command)
		{
			/* TODO use the code below post-1.28 as it introduces a new string
			g_set_error(error, gerror->domain, gerror->code,
				_("Failed to convert command into locale encoding: %s"), gerror->message);
			*/
			g_propagate_error(error, gerror);
			success = FALSE;
		}
	}

	if (success)
	{
		gchar *failure;

		failure = spawn_create_process_with_pipes(w_command, w_working_directory,
			envp ? w_environment->data : NULL, child_pid, stdin_fd, stdout_fd, stderr_fd);

		if (failure)
		{
			g_set_error_literal(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, failure);
			g_free(failure);
		}

		success = failure == NULL;
	}

	g_string_free(command, TRUE);
	g_array_free(w_environment, TRUE);
	g_free(w_working_directory);
	g_free(w_command);

	return success;
#else  /* G_OS_WIN32 */
	int cl_argc;
	char **full_argv;
	gboolean spawned;
	GError *gerror = NULL;

	if (command_line)
	{
		int argc = 0;
		char **cl_argv;

		if (!spawn_parse_argv(command_line, &cl_argc, &cl_argv, error))
			return FALSE;

		if (argv)
			for (argc = 0; argv[argc]; argc++);

		full_argv = g_renew(gchar *, cl_argv, cl_argc + argc + 1);
		memcpy(full_argv + cl_argc, argv, argc * sizeof(gchar *));
		full_argv[cl_argc + argc] = NULL;
	}
	else
		full_argv = argv;

	spawned = g_spawn_async_with_pipes(working_directory, full_argv, envp,
		G_SPAWN_SEARCH_PATH | (child_pid ? G_SPAWN_DO_NOT_REAP_CHILD : 0), NULL, NULL,
		child_pid, stdin_fd, stdout_fd, stderr_fd, &gerror);

	if (!spawned)
	{
		gint en = 0;
		const gchar *message = gerror->message;

		/* try to cut glib citing of the program name or working directory: they may be long,
		   and only the caller knows whether they're UTF-8. We lose the exact chdir error. */
		switch (gerror->code)
		{
		#ifdef EACCES
			case G_SPAWN_ERROR_ACCES : en = EACCES; break;
		#endif
		#ifdef EPERM
			case G_SPAWN_ERROR_PERM : en = EPERM; break;
		#endif
		#ifdef E2BIG
			case G_SPAWN_ERROR_TOO_BIG : en = E2BIG; break;
		#endif
		#ifdef ENOEXEC
			case G_SPAWN_ERROR_NOEXEC : en = ENOEXEC; break;
		#endif
		#ifdef ENAMETOOLONG
			case G_SPAWN_ERROR_NAMETOOLONG : en = ENAMETOOLONG; break;
		#endif
		#ifdef ENOENT
			case G_SPAWN_ERROR_NOENT : en = ENOENT; break;
		#endif
		#ifdef ENOMEM
			case G_SPAWN_ERROR_NOMEM : en = ENOMEM; break;
		#endif
		#ifdef ENOTDIR
			case G_SPAWN_ERROR_NOTDIR : en = ENOTDIR; break;
		#endif
		#ifdef ELOOP
			case G_SPAWN_ERROR_LOOP : en = ELOOP; break;
		#endif
		#ifdef ETXTBUSY
			case G_SPAWN_ERROR_TXTBUSY : en = ETXTBUSY; break;
		#endif
		#ifdef EIO
			case G_SPAWN_ERROR_IO : en = EIO; break;
		#endif
		#ifdef ENFILE
			case G_SPAWN_ERROR_NFILE : en = ENFILE; break;
		#endif
		#ifdef EMFILE
			case G_SPAWN_ERROR_MFILE : en = EMFILE; break;
		#endif
		#ifdef EINVAL
			case G_SPAWN_ERROR_INVAL : en = EINVAL; break;
		#endif
		#ifdef EISDIR
			case G_SPAWN_ERROR_ISDIR : en = EISDIR; break;
		#endif
		#ifdef ELIBBAD
			case G_SPAWN_ERROR_LIBBAD : en = ELIBBAD; break;
		#endif
			case G_SPAWN_ERROR_CHDIR :
			{
				message = _("Failed to change to the working directory");
				break;
			}
			case G_SPAWN_ERROR_FAILED :
			{
				message = _("Unknown error executing child process");
				break;
			}
		}

		if (en)
			message = g_strerror(en);

		g_set_error_literal(error, gerror->domain, gerror->code, message);
		g_error_free(gerror);
	}

	if (full_argv != argv)
	{
		full_argv[cl_argc] = NULL;
		g_strfreev(full_argv);
	}

	return spawned;
#endif  /* G_OS_WIN32 */
}


/**
 *  Executes a child asynchronously.
 *
 *  A command line or an argument vector must be passed. If both are present, the argument
 *  vector is appended to the command line. An empty command line is not allowed.
 *
 *  If a @a child_pid is passed, it's your responsibility to invoke @c g_spawn_close_pid().
 *
 *  @param working_directory @nullable child's current working directory, or @c NULL.
 *  @param command_line @nullable child program and arguments, or @c NULL.
 *  @param argv @nullable child's argument vector, or @c NULL.
 *  @param envp @nullable child's environment, or @c NULL.
 *  @param child_pid @out @optional return location for child process ID, or @c NULL.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE on error.
 *
 *  @since 1.25
 **/
GEANY_API_SYMBOL
gboolean spawn_async(const gchar *working_directory, const gchar *command_line, gchar **argv,
	gchar **envp, GPid *child_pid, GError **error)
{
	return spawn_async_with_pipes(working_directory, command_line, argv, envp, child_pid,
		NULL, NULL, NULL, error);
}


/*
 * Spawn with callbacks - general event sequence:
 *
 * - Launch the child.
 * - Setup any I/O callbacks and a child watch callback.
 * - On sync execution, run a main loop.
 * - Wait for the child to terminate.
 * - Check for active I/O sources. If any, add a timeout source to watch them, they should
 *   become inactive real soon now that the child is dead. Otherwise, finalize immediately.
 * - In the timeout source: check for active I/O sources and finalize if none.
 */

typedef struct _SpawnChannelData
{
	GIOChannel *channel;  /* NULL if not created / already destroyed */
	union
	{
		GIOFunc write;
		SpawnReadFunc read;
	} cb;
	gpointer cb_data;
	/* stdout/stderr only */
	GString *buffer;       /* NULL if recursive */
	GString *line_buffer;  /* NULL if char buffered */
	gsize max_length;
} SpawnChannelData;


static void spawn_destroy_cb(gpointer data)
{
	SpawnChannelData *sc = (SpawnChannelData *) data;

	g_io_channel_shutdown(sc->channel, FALSE, NULL);
	sc->channel = NULL;

	if (sc->buffer)
		g_string_free(sc->buffer, TRUE);

	if (sc->line_buffer)
		g_string_free(sc->line_buffer, TRUE);
}


static gboolean spawn_write_cb(GIOChannel *channel, GIOCondition condition, gpointer data)
{
	SpawnChannelData *sc = (SpawnChannelData *) data;

	if (!sc->cb.write(channel, condition, sc->cb_data))
		return FALSE;

	return !(condition & G_IO_FAILURE);
}


static gboolean spawn_read_cb(GIOChannel *channel, GIOCondition condition, gpointer data)
{
	SpawnChannelData *sc = (SpawnChannelData *) data;
	GString *line_buffer = sc->line_buffer;
	GString *buffer = sc->buffer ? sc->buffer : g_string_sized_new(sc->max_length);
	GIOCondition input_cond = condition & (G_IO_IN | G_IO_PRI);
	GIOCondition failure_cond = condition & G_IO_FAILURE;
	/*
	 * - Normally, read only once. With IO watches, our data processing must be immediate,
	 *   which may give the child time to emit more data, and a read loop may combine it into
	 *   large stdout and stderr portions. Under Windows, looping blocks.
	 * - On failure, read in a loop. It won't block now, there will be no more data, and the
	 *   IO watch is not guaranteed to be called again (under Windows this is the last call).
	 */
	if (input_cond)
	{
		gsize chars_read;
		GIOStatus status;

		if (line_buffer)
		{
			gsize n = line_buffer->len;

			while ((status = g_io_channel_read_chars(channel, line_buffer->str + n,
				DEFAULT_IO_LENGTH, &chars_read, NULL)) == G_IO_STATUS_NORMAL)
			{
				g_string_set_size(line_buffer, n + chars_read);

				while (n < line_buffer->len)
				{
					gsize line_len = 0;

					if (n == sc->max_length)
						line_len = n;
					else if (strchr("\n", line_buffer->str[n]))  /* '\n' or '\0' */
						line_len = n + 1;
					else if (n < line_buffer->len - 1 && line_buffer->str[n] == '\r')
						line_len = n + 1 + (line_buffer->str[n + 1] == '\n');

					if (!line_len)
						n++;
					else
					{
						g_string_append_len(buffer, line_buffer->str, line_len);
						g_string_erase(line_buffer, 0, line_len);
						/* input only, failures are reported separately below */
						sc->cb.read(buffer, input_cond, sc->cb_data);
						g_string_truncate(buffer, 0);
						n = 0;
					}
				}

				if (!failure_cond)
					break;
			}
		}
		else
		{
			while ((status = g_io_channel_read_chars(channel, buffer->str, sc->max_length,
				&chars_read, NULL)) == G_IO_STATUS_NORMAL)
			{
				g_string_set_size(buffer, chars_read);
				/* input only, failures are reported separately below */
				sc->cb.read(buffer, input_cond, sc->cb_data);

				if (!failure_cond)
					break;
			}
		}

		/* Under OSX, after child death, the read watches receive input conditions instead
		   of error conditions, so we convert the termination statuses into conditions.
		   Should not hurt the other OS. */
		if (status == G_IO_STATUS_ERROR)
			failure_cond |= G_IO_ERR;
		else if (status == G_IO_STATUS_EOF)
			failure_cond |= G_IO_HUP;
	}

	if (failure_cond)  /* we must signal the callback */
	{
		if (line_buffer && line_buffer->len)  /* flush the line buffer */
		{
			g_string_append_len(buffer, line_buffer->str, line_buffer->len);
			/* all data may be from a previous call */
			if (!input_cond)
				input_cond = G_IO_IN;
		}
		else
		{
			input_cond = 0;
			g_string_truncate(buffer, 0);
		}

		sc->cb.read(buffer, input_cond | failure_cond, sc->cb_data);
	}

	if (buffer != sc->buffer)
		g_string_free(buffer, TRUE);

	return !failure_cond;
}


typedef struct _SpawnWatcherData
{
	SpawnChannelData sc[3];       /* stdin, stdout, stderr */
	GChildWatchFunc exit_cb;
	gpointer exit_data;
	GPid pid;
	gint exit_status;
	GMainContext *main_context;  /* NULL if async execution */
	GMainLoop *main_loop;        /* NULL if async execution */
} SpawnWatcherData;


static void spawn_finalize(SpawnWatcherData *sw)
{
	if (sw->exit_cb)
		sw->exit_cb(sw->pid, sw->exit_status, sw->exit_data);

	if (sw->main_loop)
	{
		g_main_loop_quit(sw->main_loop);
		g_main_loop_unref(sw->main_loop);
	}

	g_spawn_close_pid(sw->pid);
	g_slice_free(SpawnWatcherData, sw);
}


static gboolean spawn_timeout_cb(gpointer data)
{
	SpawnWatcherData *sw = (SpawnWatcherData *) data;
	int i;

	for (i = 0; i < 3; i++)
		if (sw->sc[i].channel)
			return TRUE;

	spawn_finalize(sw);
	return FALSE;
}


static void spawn_watch_cb(GPid pid, gint status, gpointer data)
{
	SpawnWatcherData *sw = (SpawnWatcherData *) data;
	int i;

	sw->pid = pid;
	sw->exit_status = status;

	for (i = 0; i < 3; i++)
	{
		if (sw->sc[i].channel)
		{
			GSource *source = g_timeout_source_new(50);

			g_source_set_callback(source, spawn_timeout_cb, data, NULL);
			g_source_attach(source, sw->main_context);
			g_source_unref(source);
			return;
		}
	}

	spawn_finalize(sw);
}


/** @girskip
 *  Executes a child program and setups callbacks.
 *
 *  A command line or an argument vector must be passed. If both are present, the argument
 *  vector is appended to the command line. An empty command line is not allowed.
 *
 *  The synchronous execution may not be combined with recursive callbacks.
 *
 *  In line buffered mode, the child input is broken on `\n`, `\r\n`, `\r`, `\0` and max length.
 *
 *  All I/O callbacks are guaranteed to be invoked at least once with @c G_IO_ERR, @c G_IO_HUP
 *  or @c G_IO_NVAL set (except for a @a stdin_cb which returns @c FALSE before that). For the
 *  non-recursive callbacks, this is guaranteed to be the last call, and may be used to free any
 *  resources associated with the callback.
 *
 *  The @a stdin_cb may write to @c channel only once per invocation, only if @c G_IO_OUT is
 *  set, and only a non-zero number of characters.
 *
 *  @c stdout_cb and @c stderr_cb may modify the received strings in any way, but must not
 *  free them.
 *
 *  The default max lengths are 24K for line buffered stdout, 8K for line buffered stderr,
 *  4K for unbuffered input under Unix, and 2K for unbuffered input under Windows.
 *
 *  @c exit_cb is always invoked last, after all I/O callbacks.
 *
 *  The @a child_pid will be closed automatically, after @a exit_cb is invoked.
 *
 *  @param working_directory @nullable child's current working directory, or @c NULL.
 *  @param command_line @nullable child program and arguments, or @c NULL.
 *  @param argv @nullable child's argument vector, or @c NULL.
 *  @param envp @nullable child's environment, or @c NULL.
 *  @param spawn_flags flags from SpawnFlags.
 *  @param stdin_cb @nullable callback to send data to childs's stdin, or @c NULL.
 *  @param stdin_data data to pass to @a stdin_cb.
 *  @param stdout_cb @nullable callback to receive child's stdout, or @c NULL.
 *  @param stdout_data data to pass to @a stdout_cb.
 *  @param stdout_max_length maximum data length to pass to stdout_cb, @c 0 = default.
 *  @param stderr_cb @nullable callback to receive child's stderr, or @c NULL.
 *  @param stderr_data data to pass to @a stderr_cb.
 *  @param stderr_max_length maximum data length to pass to stderr_cb, @c 0 = default.
 *  @param exit_cb @nullable callback to invoke when the child exits, or @c NULL.
 *  @param exit_data data to pass to @a exit_cb.
 *  @param child_pid @out @optional return location for child process ID, or @c NULL.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE on error.
 *
 *  @since 1.25
 **/
GEANY_API_SYMBOL
gboolean spawn_with_callbacks(const gchar *working_directory, const gchar *command_line,
	gchar **argv, gchar **envp, SpawnFlags spawn_flags, GIOFunc stdin_cb, gpointer stdin_data,
	SpawnReadFunc stdout_cb, gpointer stdout_data, gsize stdout_max_length,
	SpawnReadFunc stderr_cb, gpointer stderr_data, gsize stderr_max_length,
	GChildWatchFunc exit_cb, gpointer exit_data, GPid *child_pid, GError **error)
{
	GPid pid;
	int pipe[3] = { -1, -1, -1 };

	g_return_val_if_fail(!(spawn_flags & SPAWN_RECURSIVE) || !(spawn_flags & SPAWN_SYNC),
		FALSE);

	if (spawn_async_with_pipes(working_directory, command_line, argv, envp, &pid,
		stdin_cb ? &pipe[0] : NULL, stdout_cb ? &pipe[1] : NULL,
		stderr_cb ? &pipe[2] : NULL, error))
	{
		SpawnWatcherData *sw = g_slice_new0(SpawnWatcherData);
		gpointer cb_data[3] = { stdin_data, stdout_data, stderr_data };
		GSource *source;
		int i;

		sw->main_context = spawn_flags & SPAWN_SYNC ? g_main_context_new() : NULL;

		if (child_pid)
			*child_pid = pid;

		for (i = 0; i < 3; i++)
		{
			SpawnChannelData *sc = &sw->sc[i];
			GIOCondition condition;
			GSourceFunc callback;

			if (pipe[i] == -1)
				continue;

		#ifdef G_OS_WIN32
			sc->channel = g_io_channel_win32_new_fd(pipe[i]);
		#else
			sc->channel = g_io_channel_unix_new(pipe[i]);
			g_io_channel_set_flags(sc->channel, G_IO_FLAG_NONBLOCK, NULL);
		#endif
			g_io_channel_set_encoding(sc->channel, NULL, NULL);
			/* we have our own buffers, and GIO buffering blocks under Windows */
			g_io_channel_set_buffered(sc->channel, FALSE);
			sc->cb_data = cb_data[i];

			if (i == 0)
			{
				sc->cb.write = stdin_cb;
				condition = G_IO_OUT | G_IO_FAILURE;
				callback = (GSourceFunc) spawn_write_cb;
			}
			else
			{
				gboolean line_buffered = !(spawn_flags &
					((SPAWN_STDOUT_UNBUFFERED >> 1) << i));

				condition = G_IO_IN | G_IO_PRI | G_IO_FAILURE;
				callback = (GSourceFunc) spawn_read_cb;

				if (i == 1)
				{
					sc->cb.read = stdout_cb;
					sc->max_length = stdout_max_length ? stdout_max_length :
						line_buffered ? 24576 : DEFAULT_IO_LENGTH;
				}
				else
				{
					sc->cb.read = stderr_cb;
					sc->max_length = stderr_max_length ? stderr_max_length :
						line_buffered ? 8192 : DEFAULT_IO_LENGTH;
				}

				if (line_buffered)
				{
					sc->line_buffer = g_string_sized_new(sc->max_length +
						DEFAULT_IO_LENGTH);
				}
			}

			source = g_io_create_watch(sc->channel, condition);
			g_io_channel_unref(sc->channel);

			if (spawn_flags & (SPAWN_STDIN_RECURSIVE << i))
				g_source_set_can_recurse(source, TRUE);
			else if (i)  /* to avoid new string on each call */
				sc->buffer = g_string_sized_new(sc->max_length);

			g_source_set_callback(source, callback, sc, spawn_destroy_cb);
			g_source_attach(source, sw->main_context);
			g_source_unref(source);
		}

		sw->exit_cb = exit_cb;
		sw->exit_data = exit_data;
		source = g_child_watch_source_new(pid);
		g_source_set_callback(source, (GSourceFunc) spawn_watch_cb, sw, NULL);
		g_source_attach(source, sw->main_context);
		g_source_unref(source);

		if (spawn_flags & SPAWN_SYNC)
		{
			sw->main_loop = g_main_loop_new(sw->main_context, FALSE);
			g_main_context_unref(sw->main_context);
			g_main_loop_run(sw->main_loop);
		}

		return TRUE;
	}

	return FALSE;
}


/**
 *  Writes (a portion of) the data pointed by @a data->ptr to the @a channel.
 *
 *  If @c G_IO_OUT in @a condition is set, and the @a data->size is > 0, attempts to write
 *  @a data->ptr (or a portion of it, depending on the size) to the @a channel. On success,
 *  increases ptr and decreases size with the number of characters written.
 *
 *  This function may converted to @c GIOFunc and passed to @c spawn_with_callbacks() as
 *  @c stdin_cb, together with a @c SpawnWriteData for @c stdin_data. As with any other
 *  callback data, make sure that @c stdin_data exists while the child is being executed.
 *  (For example, on asynchronous execution, you can allocate the data in the heap, and free
 *  it in your @c spawn_with_callbacks() @c exit_cb callback.)
 *
 * @param channel the channel to write data to.
 * @param condition condition to check for @c G_IO_OUT.
 * @param data @c SpawnWriteData to write to @a channel.
 *
 *  @return @c TRUE if the remaining size is > 0 and @a condition does not indicate any error,
 *  @c FALSE otherwise.
 *
 *  @since 1.25
 **/
GEANY_API_SYMBOL
gboolean spawn_write_data(GIOChannel *channel, GIOCondition condition, SpawnWriteData *data)
{
	if ((condition & G_IO_OUT) && data->size)
	{
		gsize chars_written = 0;

		g_io_channel_write_chars(channel, data->ptr, data->size < DEFAULT_IO_LENGTH ?
			data->size : DEFAULT_IO_LENGTH, &chars_written, NULL);

		/* "This can be nonzero even if the return value is not G_IO_STATUS_NORMAL." */
		if (chars_written)
		{
			data->ptr += chars_written;
			data->size -= chars_written;
		}
	}

	return data->size > 0 && !(condition & G_IO_FAILURE);
}


static void spawn_append_gstring_cb(GString *string, GIOCondition condition, gpointer data)
{
	if (condition & (G_IO_IN | G_IO_PRI))
		g_string_append_len((GString *) data, string->str, string->len);
}


static void spawn_get_exit_status_cb(G_GNUC_UNUSED GPid pid, gint status, gpointer exit_status)
{
	*(gint *) exit_status = status;
}


/**
 *  Executes a child synchronously.
 *
 *  A command line or an argument vector must be passed. If both are present, the argument
 *  vector is appended to the command line. An empty command line is not allowed.
 *
 *  The @a stdin_data is sent to the child with @c spawn_write_data().
 *
 *  All output from the child, including the nul characters, is stored in @a stdout_data and
 *  @a stderr_data (if non-NULL). Any existing data in these strings will be erased.
 *
 *  @param working_directory @nullable child's current working directory, or @c NULL.
 *  @param command_line @nullable child program and arguments, or @c NULL.
 *  @param argv @nullable child's argument vector, or @c NULL.
 *  @param envp @nullable child's environment, or @c NULL.
 *  @param stdin_data @nullable data to send to childs's stdin, or @c NULL.
 *  @param stdout_data @nullable GString location to receive the child's stdout, or @c NULL.
 *  @param stderr_data @nullable GString location to receive the child's stderr, or @c NULL.
 *  @param exit_status @out @optional return location for the child exit code, or @c NULL.
 *  @param error return location for error.
 *
 *  @return @c TRUE on success, @c FALSE on error.
 *
 *  @since 1.25
 **/
GEANY_API_SYMBOL
gboolean spawn_sync(const gchar *working_directory, const gchar *command_line, gchar **argv,
	gchar **envp, SpawnWriteData *stdin_data, GString *stdout_data, GString *stderr_data,
	gint *exit_status, GError **error)
{
	if (stdout_data)
		g_string_truncate(stdout_data, 0);
	if (stderr_data)
		g_string_truncate(stderr_data, 0);

	return spawn_with_callbacks(working_directory, command_line, argv, envp, SPAWN_SYNC |
		SPAWN_UNBUFFERED, stdin_data ? (GIOFunc) spawn_write_data : NULL, stdin_data,
		stdout_data ? spawn_append_gstring_cb : NULL, stdout_data, 0,
		stderr_data ? spawn_append_gstring_cb : NULL, stderr_data, 0,
		exit_status ? spawn_get_exit_status_cb : NULL, exit_status, NULL, error);
}


/* tests, not part of the API */
#ifdef SPAWN_TEST
#include <stdio.h>


static gboolean read_line(const char *prompt, char *buffer, size_t size)
{
	fputs(prompt, stderr);
	*buffer = '\0';

	if (fgets(buffer, size, stdin))
	{
		char *s = strchr(buffer, '\n');

		if (s)
			*s = '\0';
	}

	return *buffer;
}


static GString *read_string(const char *prompt)
{
	char buffer[0x1000];  /* larger portions for spawn < file */
	GString *string = g_string_sized_new(sizeof buffer);

	while (read_line(prompt, buffer, sizeof buffer))
	{
		if (string->len)
			g_string_append_c(string, '\n');

		g_string_append(string, buffer);
	}

	if (!string->len)
	{
		g_string_free(string, TRUE);
		string = NULL;
	}

	return string;
}


static void print_cb(GString *string, GIOCondition condition, gpointer data)
{
	if (condition & (G_IO_IN | G_IO_PRI))
	{
		gsize i;

		printf("%s: ", (const gchar *) data);
		/*fputs(string->str, stdout);*/
		for (i = 0; i < string->len; i++)
		{
			unsigned char c = (unsigned char) string->str[i];
			printf(c >= ' ' && c < 0x80 ? "%c" : "\\x%02x", c);
		}
		putchar('\n');
	}
}


static void print_status(gint status)
{
	fputs("finished, ", stderr);

	if (SPAWN_WIFEXITED(status))
		fprintf(stderr, "exit code %d\n", SPAWN_WEXITSTATUS(status));
	else
		fputs("abnormal termination\n", stderr);
}


static void exit_cb(GPid pid, gint status, G_GNUC_UNUSED gpointer data)
{
	fprintf(stderr, "process %u ", (guint) pid);
	print_status(status);
}


static void watch_cb(GPid pid, gint status, gpointer data)
{
	g_spawn_close_pid(pid);
	exit_cb(pid, status, NULL);
	g_main_loop_quit((GMainLoop *) data);
}


int main(int argc, char **argv)
{
	char *test_type;

	if (argc != 2)
	{
		fputs("usage: spawn <test-type>\n", stderr);
		return 1;
	}

	test_type = argv[1];

	if (!strcmp(test_type, "syntax") || !strcmp(test_type, "syntexec"))
	{
		char command_line[0x100];

		while (read_line("command line: ", command_line, sizeof command_line))
		{
			GError *error = NULL;

			if (spawn_check_command(command_line, argv[1][4] == 'e', &error))
				fputs("valid\n", stderr);
			else
			{
				fprintf(stderr, "error: %s\n", error->message);
				g_error_free(error);
			}
		}
	}
	else if (!strcmp(test_type, "execute"))
	{
		char command_line[0x100];

		while (read_line("command line: ", command_line, sizeof command_line))
		{
			char working_directory[0x100];
			char args[4][0x100];
			char envs[4][0x100];
			char *argv[] = { args[0], args[1], args[2], args[3], NULL };
			char *envp[] = { envs[0], envs[1], envs[2], envs[3], NULL };
			int i;
			GPid pid;
			GError *error = NULL;

			read_line("working directory: ", working_directory, sizeof working_directory);

			fputs("up to 4 arguments\n", stderr);
			for (i = 0; i < 4 && read_line("argument: ", args[i], sizeof args[i]); i++);
			argv[i] = NULL;

			fputs("up to 4 variables, or empty line for parent environment\n", stderr);
			for (i = 0; i < 4 && read_line("variable: ", envs[i], sizeof envs[i]); i++);
			envp[i] = NULL;

			if (spawn_async_with_pipes(*working_directory ? working_directory : NULL,
				*command_line ? command_line : NULL, argv, i ? envp : NULL, &pid, NULL,
				NULL, NULL, &error))
			{
				GMainLoop *loop = g_main_loop_new(NULL, TRUE);

				g_child_watch_add(pid, watch_cb, loop);
				g_main_loop_run(loop);
				g_main_loop_unref(loop);
			}
			else
			{
				fprintf(stderr, "error: %s\n", error->message);
				g_error_free(error);
			}
		}
	}
	else if (!strcmp(test_type, "redirect") || !strcmp(test_type, "redinput"))
	{
		char command_line[0x100];
		gboolean output = test_type[4] == 'r';

		while (read_line("command line: ", command_line, sizeof command_line))
		{
			GString *stdin_text = read_string("text to send: ");
			SpawnWriteData stdin_data;
			GError *error = NULL;

			if (stdin_text)
			{
				stdin_data.ptr = stdin_text->str;
				stdin_data.size = stdin_text->len;
			}

			if (!spawn_with_callbacks(NULL, command_line, NULL, NULL, SPAWN_SYNC,
				stdin_text ? (GIOFunc) spawn_write_data : NULL, &stdin_data,
				output ? print_cb : NULL, "stdout", 0, output ? print_cb : NULL,
				"stderr", 0, exit_cb, NULL, NULL, &error))
			{
				fprintf(stderr, "error: %s\n", error->message);
				g_error_free(error);
			}

			if (stdin_text)
				g_string_free(stdin_text, TRUE);
		}
	}
	else if (!strcmp(test_type, "capture"))
	{
		char command_line[0x100];

		while (read_line("command line: ", command_line, sizeof command_line))
		{
			GString *stdin_text = read_string("text to send: ");
			SpawnWriteData stdin_data = { NULL, 0 };
			GString *stdout_data = g_string_sized_new(0x10000);  /* may grow */
			GString *stderr_data = g_string_sized_new(0x1000);   /* may grow */
			gint exit_status;
			GError *error = NULL;

			if (stdin_text)
			{
				stdin_data.ptr = stdin_text->str;
				stdin_data.size = stdin_text->len;
			}

			if (spawn_sync(NULL, command_line, NULL, NULL, &stdin_data, stdout_data,
				stderr_data, &exit_status, &error))
			{
				printf("stdout: %s\n", stdout_data->str);
				printf("stderr: %s\n", stderr_data->str);
				print_status(exit_status);
			}
			else
			{
				fprintf(stderr, "error: %s\n", error->message);
				g_error_free(error);
			}

			if (stdin_text)
				g_string_free(stdin_text, TRUE);

			g_string_free(stdout_data, TRUE);
			g_string_free(stderr_data, TRUE);
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
