/*
 *      geany-run-helper.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2016 Colomban Wendling <ban(at)herbesfolles(dot)org>
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

/* Helper program to run a command, print its return code and wait on the user */

#include <glib.h>
#include <stdio.h>

#ifdef G_OS_WIN32

/*
 * Uses GetCommandLineW() and CreateProcessW().  It would be a lot shorter to use
 * _wspawnvp(), but like other argv-based Windows APIs (exec* family) it is broken
 * when it comes to "control" characters in the arguments like spaces and quotes:
 * it seems to basically do `CreateProcessW(" ".join(argv))`, which means it
 * re-interprets it as a command line a second time.
 *
 * Interesting read: https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
 *
 * FIXME: maybe just use spawn.c itself?  That would make the actual logic more
 * convoluted (trip around from commandline (UTF16) -> argv (UTF8) -> commandline (UTF16))
 * but would have all the spawn logic in one place.
 *
 * FIXME: handle cmd.exe's quoting rules?  Does that even apply on cmd.exe's
 * command line itself, or only inside the script? (it probably applies to the
 * argument of /C I guess).  That would mean to have a special mode for this,
 * just know we're calling it, or inspect the first argument of what we are
 * supposed to launch and figure out whether it's cmd.exe or not.  Darn it.
 */

#include <windows.h>

static void w32_perror(const gchar *prefix)
{
	gchar *msg = g_win32_error_message(GetLastError());
	fprintf(stderr, "%s: %s\n", prefix, msg);
	g_free(msg);
}

/* Based on spawn_get_program_name().
 * FIXME: this seems unable to handle an argument containing an escaped quote,
 * but OTOH we expect the cmdline to be valid and Windows doesn't allow quotes
 * in filenames */
static LPWSTR w32_strip_first_arg(LPWSTR command_line)
{
	while (*command_line && wcschr(L" \t\r\n", *command_line))
		command_line++;

	if (*command_line == L'"')
	{
		command_line++;
		LPWSTR p = wcschr(command_line, L'"');
		if (p)
			command_line = p + 1;
		else
			command_line = wcschr(command_line, L'\0');
	}
	else
	{
		while (*command_line && ! wcschr(L" \t", *command_line))
			command_line++;
	}

	while (*command_line && wcschr(L" \t\r\n", *command_line))
		command_line++;

	return command_line;
}

int main(void)
{
	int exit_status = 1;
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	LPWSTR command_line = GetCommandLineW();

	ZeroMemory(&startup, sizeof startup);
	startup.cb = sizeof startup;

	command_line = w32_strip_first_arg(command_line); // strip argv[0]
	if (! command_line || ! *command_line)
		fprintf(stderr, "Invalid or missing command\n");
	else if (! CreateProcessW(NULL, command_line, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process))
		w32_perror("CreateProcessW()");
	else
	{
		DWORD code;

		CloseHandle(process.hThread);
		if (WaitForSingleObject(process.hProcess, INFINITE) == WAIT_FAILED)
			w32_perror("WaitForSingleObject()");
		else if (! GetExitCodeProcess(process.hProcess, &code))
			w32_perror("GetExitCodeProcess()");
		else
		{
			printf("\n\n------------------\n");
			printf("(program exited with status %d)\n", code);
			exit_status = code;
		}
		CloseHandle(process.hProcess);
	}

	printf("Press return to continue\n");
	getc(stdin);

	return exit_status;
}

#else

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char **argv)
{
	int exit_status = 1;

	pid_t pid = fork();
	if (pid < 0)
		perror("fork()");
	else if (pid == 0)
	{
		/* in the child */
		execvp(argv[1], &argv[1]);
		perror("execvp()");
		return 127;
	}
	else
	{
		int status;
		int ret;

		do
		{
			ret = waitpid(pid, &status, 0);
		}
		while (ret == -1 && errno == EINTR);

		printf("\n\n------------------\n");
		if (ret == -1)
			perror("waitpid()");
		else if (WIFEXITED(status))
		{
			printf("(program exited with status %d)\n", WEXITSTATUS(status));
			exit_status = WEXITSTATUS(status);
		}
		else if (WIFSIGNALED(status))
		{
			printf("(program exited with signal %d)\n", WTERMSIG(status));
#ifdef WCOREDUMP
			if (WCOREDUMP(status))
				printf("(core dumped)\n");
#endif
		}
		else
			fprintf(stderr, "something funky happened to the child\n");
	}

	printf("Press return to continue\n");
	getc(stdin);

	return exit_status;
}

#endif
