/*
 *      win32.c - this file is part of Geany, a fast and lightweight IDE
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

/*
 * Special functions for the win32 platform, to provide native dialogs.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* Need Windows XP for SHGetFolderPathAndSubDirW */
#define _WIN32_WINNT 0x0501
/* Needed for SHGFP_TYPE */
#define _WIN32_IE 0x0500

#include "win32.h"

#ifdef G_OS_WIN32

#include "dialogs.h"
#include "document.h"
#include "editor.h"
#include "filetypes.h"
#include "project.h"
#include "support.h"
#include "ui_utils.h"
#include "utils.h"

#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

#include <glib/gstdio.h>
#include <gdk/gdkwin32.h>


/* Creates a native Windows message box of the given type and returns always TRUE
 * or FALSE representing th pressed Yes or No button.
 * If type is not GTK_MESSAGE_QUESTION, it returns always TRUE. */
gboolean win32_message_dialog(GtkWidget *parent, GtkMessageType type, const gchar *msg)
{
	gboolean ret = TRUE;
	gint rc;
	guint t;
	const gchar *title;
	HWND parent_hwnd = NULL;
	static wchar_t w_msg[512];
	static wchar_t w_title[512];

	switch (type)
	{
		case GTK_MESSAGE_ERROR:
		{
			t = MB_OK | MB_ICONERROR;
			title = _("Error");
			break;
		}
		case GTK_MESSAGE_QUESTION:
		{
			t = MB_YESNO | MB_ICONQUESTION;
			title = _("Question");
			break;
		}
		case GTK_MESSAGE_WARNING:
		{
			t = MB_OK | MB_ICONWARNING;
			title = _("Warning");
			break;
		}
		default:
		{
			t = MB_OK | MB_ICONINFORMATION;
			title = _("Information");
			break;
		}
	}

	/* convert the Unicode chars to wide chars */
	/** TODO test if LANG == C then possibly skip conversion => g_win32_getlocale() */
	MultiByteToWideChar(CP_UTF8, 0, msg, -1, w_msg, G_N_ELEMENTS(w_msg));
	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, G_N_ELEMENTS(w_title));

	if (parent != NULL)
		parent_hwnd = GDK_WINDOW_HWND(gtk_widget_get_window(parent));
	else if (main_widgets.window != NULL)
		parent_hwnd = GDK_WINDOW_HWND(gtk_widget_get_window(main_widgets.window));

	/* display the message box */
	rc = MessageBoxW(parent_hwnd, w_msg, w_title, t);

	if (type == GTK_MESSAGE_QUESTION && rc != IDYES)
		ret = FALSE;

	return ret;
}


/* Little wrapper for _waccess(), returns errno or 0 if there was no error */
gint win32_check_write_permission(const gchar *dir)
{
	static wchar_t w_dir[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, dir, -1, w_dir, G_N_ELEMENTS(w_dir));
	if (_waccess(w_dir, R_OK | W_OK) != 0)
		return errno;
	else
		return 0;
}


/* Just a simple wrapper function to open a browser window */
void win32_open_browser(const gchar *uri)
{
	gint ret;
	if (strncmp(uri, "file://", 7) == 0)
	{
		uri += 7;
		if (strchr(uri, ':') != NULL)
		{
			while (*uri == '/')
				uri++;
		}
	}
	ret = (gint) ShellExecute(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
	if (ret <= 32)
	{
		gchar *err = g_win32_error_message(GetLastError());
		ui_set_statusbar(TRUE, _("Failed to open URI \"%s\": %s"), uri, err);
		g_warning("ShellExecute failed opening \"%s\" (code %d): %s", uri, ret, err);
		g_free(err);
	}
}


static FILE *open_std_handle(DWORD handle, const char *mode)
{
	HANDLE lStdHandle;
	int hConHandle;
	FILE *fp;

	lStdHandle = GetStdHandle(handle);
	if (lStdHandle == INVALID_HANDLE_VALUE)
	{
		gchar *err = g_win32_error_message(GetLastError());
		g_warning("GetStdHandle(%ld) failed: %s", (long)handle, err);
		g_free(err);
		return NULL;
	}
	hConHandle = _open_osfhandle((intptr_t)lStdHandle, _O_TEXT);
	if (hConHandle == -1)
	{
		gchar *err = g_win32_error_message(GetLastError());
		g_warning("_open_osfhandle(handle(%ld), _O_TEXT) failed: %s", (long)handle, err);
		g_free(err);
		return NULL;
	}
	fp = _fdopen(hConHandle, mode);
	if (! fp)
	{
		gchar *err = g_win32_error_message(GetLastError());
		g_warning("_fdopen(%d, \"%s\") failed: %s", hConHandle, mode, err);
		g_free(err);
		return NULL;
	}
	if (setvbuf(fp, NULL, _IONBF, 0) != 0)
	{
		gchar *err = g_win32_error_message(GetLastError());
		g_warning("setvbuf(%p, NULL, _IONBF, 0) failed: %s", fp, err);
		g_free(err);
		fclose(fp);
		return NULL;
	}

	return fp;
}


static void debug_setup_console(void)
{
	static const WORD MAX_CONSOLE_LINES = 500;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	FILE	*fp;

	/* allocate a console for this app */
	AllocConsole();

	/* set the screen buffer to be big enough to let us scroll text */
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = MAX_CONSOLE_LINES;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	/* redirect unbuffered STDOUT to the console */
	fp = open_std_handle(STD_OUTPUT_HANDLE, "w");
	if (fp)
		*stdout = *fp;

	/* redirect unbuffered STDERR to the console */
	fp = open_std_handle(STD_ERROR_HANDLE, "w");
	if (fp)
		*stderr = *fp;

	/* redirect unbuffered STDIN to the console */
	fp = open_std_handle(STD_INPUT_HANDLE, "r");
	if (fp)
		*stdin = *fp;
}


void win32_init_debug_code(void)
{
	if (app->debug_mode)
	{
		/* create a console window to get log messages on Windows,
		 * especially useful when generating tags files */
		debug_setup_console();
	}
}


/* expands environment placeholders in @str.  input and output is in UTF-8 */
gchar *win32_expand_environment_variables(const gchar *str)
{
	wchar_t *cmdline = g_utf8_to_utf16(str, -1, NULL, NULL, NULL);
	wchar_t expCmdline[32768]; /* 32768 is the limit for ExpandEnvironmentStrings() */
	gchar *expanded = NULL;

	if (cmdline && ExpandEnvironmentStringsW(cmdline, expCmdline, sizeof(expCmdline)) != 0)
		expanded = g_utf16_to_utf8(expCmdline, -1, NULL, NULL, NULL);

	g_free(cmdline);

	return expanded ? expanded : g_strdup(str);
}


/* From GDK (they got it from MS Knowledge Base article Q130698) */
static gboolean resolve_link(HWND hWnd, wchar_t *link, gchar **lpszPath)
{
	WIN32_FILE_ATTRIBUTE_DATA wfad;
	HRESULT hres;
	IShellLinkW *pslW = NULL;
	IPersistFile *ppf = NULL;
	LPVOID pslWV = NULL;
	LPVOID ppfV = NULL;

	/* Check if the file is empty first because IShellLink::Resolve for some reason succeeds
	 * with an empty file and returns an empty "link target". (#524151) */
	if (!GetFileAttributesExW(link, GetFileExInfoStandard, &wfad) ||
		(wfad.nFileSizeHigh == 0 && wfad.nFileSizeLow == 0))
	{
	  return FALSE;
	}

	/* Assume failure to start with: */
	*lpszPath = 0;

	CoInitialize(NULL);

	hres = CoCreateInstance(
		&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, &pslWV);

	if (SUCCEEDED(hres))
	{
		/* The IShellLink interface supports the IPersistFile interface.
		 * Get an interface pointer to it. */
		pslW = (IShellLinkW*) pslWV;
		hres = pslW->lpVtbl->QueryInterface(pslW, &IID_IPersistFile, &ppfV);
	}

	if (SUCCEEDED(hres))
	{
		/* Load the file. */
		ppf = (IPersistFile*) ppfV;
		hres = ppf->lpVtbl->Load(ppf, link, STGM_READ);
	}

	if (SUCCEEDED(hres))
	{
		/* Resolve the link by calling the Resolve() interface function. */
		hres = pslW->lpVtbl->Resolve(pslW, hWnd, SLR_ANY_MATCH | SLR_NO_UI);
	}

	if (SUCCEEDED(hres))
	{
		wchar_t wtarget[MAX_PATH];

		hres = pslW->lpVtbl->GetPath(pslW, wtarget, MAX_PATH, NULL, 0);
		if (SUCCEEDED(hres))
			*lpszPath = g_utf16_to_utf8(wtarget, -1, NULL, NULL, NULL);
	}

	if (ppf)
		ppf->lpVtbl->Release(ppf);

	if (pslW)
		pslW->lpVtbl->Release(pslW);

	return SUCCEEDED(hres);
}


/* Checks whether file_name is a Windows shortcut. file_name is expected in UTF-8 encoding.
 * If file_name is a Windows shortcut, it returns the target in UTF-8 encoding.
 * If it is not a shortcut, it returns a newly allocated copy of file_name. */
gchar *win32_get_shortcut_target(const gchar *file_name)
{
	gchar *path = NULL;
	wchar_t *wfilename = g_utf8_to_utf16(file_name, -1, NULL, NULL, NULL);
	HWND hWnd = NULL;

	if (main_widgets.window != NULL)
	{
		GdkWindow *window = gtk_widget_get_window(main_widgets.window);
		if (window != NULL)
			hWnd = GDK_WINDOW_HWND(window);
	}

	resolve_link(hWnd, wfilename, &path);
	g_free(wfilename);

	if (path == NULL)
		return g_strdup(file_name);
	else
		return path;
}


void win32_set_working_directory(const gchar *dir)
{
	SetCurrentDirectory(dir);
}


gchar *win32_get_installation_dir(void)
{
	return g_win32_get_package_installation_directory_of_module(NULL);
}


gchar *win32_get_user_config_dir(void)
{
	HRESULT hr;
	wchar_t path[MAX_PATH];

	hr = SHGetFolderPathAndSubDirW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, L"geany", path);
	if (SUCCEEDED(hr))
	{
		// GLib always uses UTF-8 for filename encoding on Windows
		int u8_size = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
		if (u8_size > 0)
		{
			gchar *u8_path = g_malloc0(u8_size + 1);
			if (u8_path != NULL &&
				WideCharToMultiByte(CP_UTF8, 0, path, -1, u8_path, u8_size, NULL, NULL))
			{
				return u8_path;
			}
		}
	}

	// glib fallback
	g_warning("Failed to retrieve Windows config dir, falling back to default");
	return g_build_filename(g_get_user_config_dir(), "geany", NULL);
}

#endif
