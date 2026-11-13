/*
 *      main.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 The Geany contributors
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

#include "main.h"

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE pInstance, LPSTR pCmdLine, int nCmdShow){
	if(!GetStdHandle(STD_OUTPUT_HANDLE))
		if(AttachConsole(ATTACH_PARENT_PROCESS)){
			freopen("CONOUT$", "wb", stdout);
			setbuf(stdout, NULL);
			freopen("CONOUT$", "wb", stderr);
		}

	return main_lib(__argc, __argv);
}
#else
int main(int argc, char **argv)
{
	return main_lib(argc, argv);
}
#endif
