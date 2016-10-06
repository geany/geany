/*
 *      main.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2014 Matthew Brush <mbrush@codebrainz.ca>
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

/* See libmain.c for the real entry-point code. */

#include "main.h"

#ifdef G_OS_WIN32
# include <windows.h>
#endif

int main(int argc, char **argv)
{
#ifdef G_OS_WIN32
	if (argc > 1)
	{
		int num_arg;
		LPWSTR *szarglist = CommandLineToArgvW(GetCommandLineW(), &num_arg);
		char** utf8argv = g_new0(char *, num_arg + 1);
		int i = num_arg;
		while(i){
			i--;
			utf8argv[i] = g_utf16_to_utf8((gunichar2 *)szarglist[i], -1, NULL, NULL, NULL);
		}

		gint result = main_lib(num_arg, utf8argv);

		i = num_arg;
		while(i){
			i--;
			g_free(utf8argv[i]);
		}
		g_free(utf8argv);
		LocalFree(szarglist);

		return result;
	}
	else
	{
#endif
		return main_lib(argc, argv);
#ifdef G_OS_WIN32
	}
#endif
}
