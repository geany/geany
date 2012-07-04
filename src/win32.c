/*
 *      win32.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2012 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

/*
 * Special functions for the win32 platform, to provide native dialogs.
 */

#include "geany.h"

#ifdef G_OS_WIN32

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <io.h>
#include <fcntl.h>

#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include <gdk/gdkwin32.h>

#include "win32.h"

#include "document.h"
#include "support.h"
#include "utils.h"
#include "ui_utils.h"
#include "sciwrappers.h"
#include "dialogs.h"
#include "filetypes.h"
#include "project.h"
#include "editor.h"

#define BUFSIZE 4096
#define CMDSIZE 32768

struct _geany_win32_spawn
{
	HANDLE hChildStdinRd;
	HANDLE hChildStdinWr;
	HANDLE hChildStdoutRd;
	HANDLE hChildStdoutWr;
	HANDLE hChildStderrRd;
	HANDLE hChildStderrWr;
	HANDLE hInputFile;
	HANDLE hStdout;
	HANDLE hStderr;
	HANDLE processId;
	DWORD dwExitCode;
};
typedef struct _geany_win32_spawn geany_win32_spawn;

static gboolean GetContentFromHandle(HANDLE hFile, gchar **content, GError **error);
static HANDLE GetTempFileHandle(GError **error);
static gboolean CreateChildProcess(geany_win32_spawn *gw_spawn, TCHAR *szCmdline,
		const TCHAR *dir, GError **error);
static VOID ReadFromPipe(HANDLE hRead, HANDLE hWrite, HANDLE hFile, GError **error);


static wchar_t *get_file_filters(void)
{
	gchar *string;
	guint i, j, len;
	static wchar_t title[4096];
	GString *str = g_string_sized_new(100);
	GString *all_patterns = g_string_sized_new(100);
	GSList *node;
	gchar *tmp;

	/* create meta file filter "All files" */
	g_string_append_printf(str, "%s\t*\t", _("All files"));
	/* create meta file filter "All Source" (skip GEANY_FILETYPES_NONE) */
	for (i = GEANY_FILETYPES_NONE + 1; i < filetypes_array->len; i++)
	{
		for (j = 0; filetypes[i]->pattern[j] != NULL; j++)
		{
			g_string_append(all_patterns, filetypes[i]->pattern[j]);
			g_string_append_c(all_patterns, ';');
		}
	}
	g_string_append_printf(str, "%s\t%s\t", _("All Source"), all_patterns->str);
	g_string_free(all_patterns, TRUE);
	/* add 'usual' filetypes */
	foreach_slist(node, filetypes_by_title)
	{
		GeanyFiletype *ft = node->data;

		if (G_UNLIKELY(ft->id == GEANY_FILETYPES_NONE))
			continue;
		tmp = g_strjoinv(";", ft->pattern);
		g_string_append_printf(str, "%s\t%s\t", ft->title, tmp);
		g_free(tmp);
	}
	g_string_append_c(str, '\t'); /* the final \0 byte to mark the end of the string */
	string = str->str;
	g_string_free(str, FALSE);

	/* replace all "\t"s by \0 */
	len = strlen(string);
	g_strdelimit(string, "\t", '\0');
	g_assert(string[len - 1] == 0x0);
	MultiByteToWideChar(CP_UTF8, 0, string, len, title, sizeof(title));
	g_free(string);

	return title;
}


static wchar_t *get_file_filter_all_files(void)
{
	guint len;
	static wchar_t title[4096];
	gchar *filter;

	/* create meta file filter "All files" */
	filter = g_strdup_printf("%s\t*\t", _("All files"));

	len = strlen(filter);
	g_strdelimit(filter, "\t", '\0');
	g_assert(filter[len - 1] == 0x0);
	MultiByteToWideChar(CP_UTF8, 0, filter, len, title, sizeof(title));
	g_free(filter);

	return title;
}


static wchar_t *get_filters(gboolean project_files)
{
	gchar *string;
	gint len;
	static wchar_t title[1024];

	if (project_files)
	{
		string = g_strconcat(_("Geany project files"), "\t", "*." GEANY_PROJECT_EXT, "\t",
			_("All files"), "\t", "*", "\t", NULL);
	}
	else
	{
		string = g_strconcat(_("Executables"), "\t", "*.exe;*.bat;*.cmd", "\t",
			_("All files"), "\t", "*", "\t", NULL);
	}

	/* replace all "\t"s by \0 */
	len = strlen(string);
	g_strdelimit(string, "\t", '\0');
	g_assert(string[len - 1] == 0x0);
	MultiByteToWideChar(CP_UTF8, 0, string, len, title, sizeof(title));
	g_free(string);

	return title;
}


/* Converts the given UTF-8 filename or directory name into something usable for Windows and
 * returns the directory part of the given filename. */
static wchar_t *get_dir_for_path(const gchar *utf8_filename)
{
	static wchar_t w_dir[MAX_PATH];
	gchar *result;

	if (g_file_test(utf8_filename, G_FILE_TEST_IS_DIR))
		result = (gchar*) utf8_filename;
	else
		result = g_path_get_dirname(utf8_filename);

	MultiByteToWideChar(CP_UTF8, 0, result, -1, w_dir, sizeof(w_dir));

	if (result != utf8_filename)
		g_free(result);

	return w_dir;
}


/* Callback function for setting the initial directory of the folder open dialog. This could also
 * be done with BROWSEINFO.pidlRoot and SHParseDisplayName but SHParseDisplayName is not available
 * on systems below Windows XP. So, we go the hard way by creating a callback which will set up the
 * folder when the dialog is initialised. Yeah, I like Windows. */
INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
		{
			SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, pData);
			break;
		}
		case BFFM_SELCHANGED:
		{
			/* set the status window to the currently selected path. */
			static wchar_t szDir[MAX_PATH];
			if (SHGetPathFromIDListW((LPITEMIDLIST) lp, szDir))
			{
				SendMessageW(hwnd, BFFM_SETSTATUSTEXTW, 0, (LPARAM) szDir);
			}
			break;
		}
	}
	return 0;
}


/* Shows a folder selection dialog.
 * initial_dir is expected in UTF-8
 * The selected folder name is returned. */
gchar *win32_show_folder_dialog(GtkWidget *parent, const gchar *title, const gchar *initial_dir)
{
	BROWSEINFOW bi;
	LPCITEMIDLIST pidl;
	gchar *result = NULL;
	wchar_t fname[MAX_PATH];
	wchar_t w_title[512];

	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, sizeof(w_title));

	if (parent == NULL)
		parent = main_widgets.window;

	memset(&bi, 0, sizeof bi);
	bi.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(parent));
	bi.pidlRoot = NULL;
	bi.lpszTitle = w_title;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM) get_dir_for_path(initial_dir);
	bi.ulFlags = BIF_DONTGOBELOWDOMAIN | BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT;

	pidl = SHBrowseForFolderW(&bi);

	/* convert the strange Windows folder list item something into an usual path string ;-) */
	if (pidl != 0)
	{
		if (SHGetPathFromIDListW(pidl, fname))
		{
			result = g_malloc0(MAX_PATH * 2);
			WideCharToMultiByte(CP_UTF8, 0, fname, -1, result, MAX_PATH * 2, NULL, NULL);
		}
		/* SHBrowseForFolder() probably leaks memory here, but how to free the allocated memory? */
	}
	return result;
}


/* Shows a file open dialog.
 * If allow_new_file is set, the file to be opened doesn't have to exist.
 * initial_dir is expected in UTF-8
 * The selected file name is returned.
 * If project_file_filter is set, the file open dialog will have a file filter for Geany project
 * files, a filter for executables otherwise. */
gchar *win32_show_project_open_dialog(GtkWidget *parent, const gchar *title,
								      const gchar *initial_dir, gboolean allow_new_file,
								      gboolean project_file_filter)
{
	OPENFILENAMEW of;
	gint retval;
	wchar_t fname[MAX_PATH];
	wchar_t w_title[512];
	gchar *filename;

	fname[0] = '\0';

	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, sizeof(w_title));

	if (parent == NULL)
		parent = main_widgets.window;

	/* initialise file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(parent));
	of.lpstrFilter = get_filters(project_file_filter);

	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = 0;
	of.lpstrFile = fname;
	of.lpstrInitialDir = get_dir_for_path(initial_dir);
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	of.lpstrTitle = w_title;
	of.lpstrDefExt = L"";
	of.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY;
	if (! allow_new_file)
		of.Flags |= OFN_FILEMUSTEXIST;

	retval = GetOpenFileNameW(&of);

	if (! retval)
	{
		if (CommDlgExtendedError())
		{
			gchar *error;
			error = g_strdup_printf("File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
			g_free(error);
		}
		return NULL;
	}
	/* convert the resulting filename into UTF-8 (from whatever encoding it has at this moment) */
	filename = g_malloc0(MAX_PATH * 2);
	WideCharToMultiByte(CP_UTF8, 0, fname, -1, filename, MAX_PATH * 2, NULL, NULL);

	return filename;
}


/* initial_dir can be NULL to use the current working directory.
 * Returns: TRUE if the dialog was not cancelled. */
gboolean win32_show_document_open_dialog(GtkWindow *parent, const gchar *title, const gchar *initial_dir)
{
	OPENFILENAMEW of;
	gint retval;
	guint x;
	gchar tmp[MAX_PATH];
	wchar_t fname[MAX_PATH];
	wchar_t w_dir[MAX_PATH];
	wchar_t w_title[512];

	fname[0] = '\0';

	if (initial_dir != NULL)
		MultiByteToWideChar(CP_UTF8, 0, initial_dir, -1, w_dir, sizeof(w_dir));

	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, sizeof(w_title));

	/* initialise file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(GTK_WIDGET(parent)));
	of.lpstrFilter = get_file_filters();

	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = GEANY_FILETYPES_NONE + 1;
	of.lpstrFile = fname;
	of.lpstrInitialDir = (initial_dir != NULL) ? w_dir : NULL;
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	of.lpstrTitle = w_title;
	of.lpstrDefExt = L"";
	of.Flags = OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_EXPLORER;

	retval = GetOpenFileNameW(&of);

	if (!retval)
	{
		if (CommDlgExtendedError())
		{
			gchar error[100];
			g_snprintf(error, sizeof error, "File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
		}
		return FALSE;
	}

	x = of.nFileOffset - 1;
	if (x != wcslen(fname))
	{	/* open a single file */
		WideCharToMultiByte(CP_UTF8, 0, fname, -1, tmp, sizeof(tmp), NULL, NULL);
		document_open_file(tmp, of.Flags & OFN_READONLY, NULL, NULL);
	}
	else
	{	/* open multiple files */
		gchar file_name[MAX_PATH];
		gchar dir_name[MAX_PATH];

		WideCharToMultiByte(CP_UTF8, 0, fname, of.nFileOffset,
			dir_name, sizeof(dir_name), NULL, NULL);
		for (; ;)
		{
			if (! fname[x])
			{
				if (! fname[x + 1])
					break;

				WideCharToMultiByte(CP_UTF8, 0, fname + x + 1, -1,
					tmp, sizeof(tmp), NULL, NULL);
				g_snprintf(file_name, 511, "%s\\%s", dir_name, tmp);

				/* convert the resulting filename into UTF-8 */
				document_open_file(file_name, of.Flags & OFN_READONLY, NULL, NULL);
			}
			x++;
		}
	}
	return (retval != 0);
}


gchar *win32_show_document_save_as_dialog(GtkWindow *parent, const gchar *title,
										  const gchar *initial_file)
{
	OPENFILENAMEW of;
	gint retval;
	gchar tmp[MAX_PATH];
	wchar_t w_file[MAX_PATH];
	wchar_t w_title[512];

	w_file[0] = '\0';

	if (initial_file != NULL)
		MultiByteToWideChar(CP_UTF8, 0, initial_file, -1, w_file, sizeof(w_file));

	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, sizeof(w_title));

	/* initialise file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(GTK_WIDGET(parent)));

	of.lpstrFilter = get_file_filter_all_files();
	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = 0;

	of.lpstrFile = w_file;
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	of.lpstrTitle = w_title;
	of.lpstrDefExt = L"";
	of.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
	retval = GetSaveFileNameW(&of);

	if (! retval)
	{
		if (CommDlgExtendedError())
		{
			gchar *error = g_strdup_printf(
				"File dialog box error (%x)", (gint) CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
			g_free(error);
		}
		return NULL;
	}

	WideCharToMultiByte(CP_UTF8, 0, w_file, -1, tmp, sizeof(tmp), NULL, NULL);

	return g_strdup(tmp);
}


/* initial_dir can be NULL to use the current working directory.
 * Returns: the selected filename */
gchar *win32_show_file_dialog(GtkWindow *parent, const gchar *title, const gchar *initial_file)
{
	OPENFILENAMEW of;
	gint retval;
	gchar tmp[MAX_PATH];
	wchar_t w_file[MAX_PATH];
	wchar_t w_title[512];

	w_file[0] = '\0';

	if (initial_file != NULL)
		MultiByteToWideChar(CP_UTF8, 0, initial_file, -1, w_file, sizeof(w_file));

	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, sizeof(w_title));

	/* initialise file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(GTK_WIDGET(parent)));

	of.lpstrFile = w_file;
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	of.lpstrTitle = w_title;
	of.lpstrDefExt = L"";
	of.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER;
	retval = GetOpenFileNameW(&of);

	if (! retval)
	{
		if (CommDlgExtendedError())
		{
			gchar *error = g_strdup_printf(
				"File dialog box error (%x)", (gint) CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
			g_free(error);
		}
		return NULL;
	}

	WideCharToMultiByte(CP_UTF8, 0, w_file, -1, tmp, sizeof(tmp), NULL, NULL);

	return g_strdup(tmp);
}


void win32_show_font_dialog(void)
{
	gint retval;
	CHOOSEFONT cf;
	LOGFONT lf;        /* logical font structure */

	memset(&lf, 0, sizeof lf);
	/* TODO: init lf members */

	memset(&cf, 0, sizeof cf);
	cf.lStructSize = sizeof cf;
	cf.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(main_widgets.window));
	cf.lpLogFont = &lf;
	/* support CF_APPLY? */
	cf.Flags = CF_NOSCRIPTSEL | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;

	retval = ChooseFont(&cf);

	if (retval)
	{
		gchar *editorfont = g_strdup_printf("%s %d", lf.lfFaceName, (cf.iPointSize / 10));
		ui_set_editor_font(editorfont);
		g_free(editorfont);
	}
}


void win32_show_color_dialog(const gchar *colour)
{
	CHOOSECOLOR cc;
	static COLORREF acr_cust_clr[16];
	static DWORD rgb_current;
	gchar *hex = g_malloc0(12);
	GeanyDocument *doc = document_get_current();

	/* Initialize CHOOSECOLOR */
	memset(&cc, 0, sizeof cc);
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(main_widgets.window));
	cc.lpCustColors = (LPDWORD) acr_cust_clr;
	cc.rgbResult = (colour != NULL) ? utils_strtod(colour, NULL, colour[0] == '#') : 0;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;

	if (ChooseColor(&cc))
	{
		rgb_current = cc.rgbResult;
		g_snprintf(hex, 11, "#%02X%02X%02X",
	      (guint) (utils_scale_round(GetRValue(rgb_current), 255)),
	      (guint) (utils_scale_round(GetGValue(rgb_current), 255)),
	      (guint) (utils_scale_round(GetBValue(rgb_current), 255)));

		editor_insert_color(doc->editor, hex);
	}
	g_free(hex);
}


void win32_show_pref_file_dialog(GtkEntry *item)
{
	OPENFILENAMEW of;
	gint retval, len;
	wchar_t fname[MAX_PATH];
	gchar tmp[MAX_PATH];
	gchar **field, *filename;

	fname[0] = '\0';

	/* cut the options from the command line */
	field = g_strsplit(gtk_entry_get_text(GTK_ENTRY(item)), " ", 2);
	if (field[0])
	{
		filename = g_find_program_in_path(field[0]);
		if (filename != NULL && g_file_test(filename, G_FILE_TEST_EXISTS))
		{
			MultiByteToWideChar(CP_UTF8, 0, filename, -1, fname, sizeof(fname));
			g_free(filename);
		}
	}

	/* initialize file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(ui_widgets.prefs_dialog));

	of.lpstrFilter = get_filters(FALSE);
	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = 1;

	of.lpstrFile = fname;
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	/*of.lpstrInitialDir = g_get_home_dir();*/
	of.lpstrInitialDir = NULL;
	of.lpstrTitle = NULL;
	of.lpstrDefExt = L"exe";
	of.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_EXPLORER;
	retval = GetOpenFileNameW(&of);

	if (!retval)
	{
		if (CommDlgExtendedError())
		{
			gchar error[100];
			g_snprintf(error, sizeof error, "File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
		}
		g_strfreev(field);
		return;
	}

	len = WideCharToMultiByte(CP_UTF8, 0, fname, -1, tmp, sizeof(tmp), NULL, NULL);
	if ((of.nFileOffset - 1) != len)
	{
		if (g_strv_length(field) > 1)
			/* add the command line args of the old command */
			/** TODO this fails badly when the old command contained spaces, we need quoting here */
			filename = g_strconcat(tmp, " ", field[1], NULL);
		else
		{
			filename = g_strdup(tmp);
		}
		gtk_entry_set_text(GTK_ENTRY(item), filename);
		g_free(filename);
	}
	g_strfreev(field);
}


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
	MultiByteToWideChar(CP_UTF8, 0, dir, -1, w_dir, sizeof w_dir);
	if (_waccess(w_dir, R_OK | W_OK) != 0)
		return errno;
	else
		return 0;
}


/* Just a simple wrapper function to open a browser window */
void win32_open_browser(const gchar *uri)
{
	if (strncmp(uri, "file://", 7) == 0)
	{
		uri += 7;
		if (strchr(uri, ':') != NULL)
		{
			while (*uri == '/')
				uri++;
		}
	}
	ShellExecute(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
}


/* Returns TRUE if the command, which child_pid refers to, returned with a successful exit code,
 * otherwise FALSE. */
gboolean win32_get_exit_status(GPid child_pid)
{
	DWORD exit_code;
	GetExitCodeProcess(child_pid, &exit_code);

	return (exit_code == 0);
}


static void debug_setup_console()
{
	static const WORD MAX_CONSOLE_LINES = 500;
	gint	 hConHandle;
	glong	 lStdHandle;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	FILE	*fp;

	/* allocate a console for this app */
	AllocConsole();

	/* set the screen buffer to be big enough to let us scroll text */
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = MAX_CONSOLE_LINES;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	/* redirect unbuffered STDOUT to the console */
	lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen(hConHandle, "w");
	*stdout = *fp;
	setvbuf(stdout, NULL, _IONBF, 0);

	/* redirect unbuffered STDERR to the console */
	lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen(hConHandle, "w");
	*stderr = *fp;
	setvbuf(stderr, NULL, _IONBF, 0);

	/* redirect unbuffered STDIN to the console */
	lStdHandle = (long)GetStdHandle(STD_INPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "r" );
	*stdin = *fp;
	setvbuf(stdin, NULL, _IONBF, 0);
}


void win32_init_debug_code(void)
{
	if (app->debug_mode)
	{
		/* create a console window to get log messages on Windows,
		 * especially useful when generating tags files */
		debug_setup_console();
		/* Enable GLib process spawn debug mode when Geany was started with the debug flag */
		g_setenv("G_SPAWN_WIN32_DEBUG", "1", FALSE);
	}
}


gchar *win32_get_hostname(void)
{
	gchar hostname[100];
	DWORD size = sizeof(hostname);

	if (GetComputerName(hostname, &size))
		return g_strdup(hostname);
	else
		return g_strdup("localhost");
}


/* Process spawning implementation for Windows, by Pierre Joye.
 * Don't call this function directly, use utils_spawn_[a]sync() instead. */
gboolean win32_spawn(const gchar *dir, gchar **argv, gchar **env, GSpawnFlags flags,
					 gchar **std_out, gchar **std_err, gint *exit_status, GError **error)
{
	TCHAR  buffer[CMDSIZE]=TEXT("");
	TCHAR  cmdline[CMDSIZE] = TEXT("");
	TCHAR* lpPart[CMDSIZE]={NULL};
	DWORD  retval = 0;
	gint argc = 0, i;
	gint cmdpos = 0;

	SECURITY_ATTRIBUTES saAttr;
	BOOL fSuccess;
	geany_win32_spawn gw_spawn;

	/* Temp file */
	HANDLE hStdoutTempFile = NULL;
	HANDLE hStderrTempFile = NULL;

	gchar *stdout_content = NULL;
	gchar *stderr_content = NULL;

	while (argv[argc])
	{
		++argc;
	}
	g_return_val_if_fail (std_out == NULL ||
						!(flags & G_SPAWN_STDOUT_TO_DEV_NULL), FALSE);
	g_return_val_if_fail (std_err == NULL ||
						!(flags & G_SPAWN_STDERR_TO_DEV_NULL), FALSE);

	if (flags & G_SPAWN_SEARCH_PATH)
	{
		retval = SearchPath(NULL, argv[0], ".exe", sizeof(buffer), buffer, lpPart);
		if (retval > 0)
			g_snprintf(cmdline, sizeof(cmdline), "\"%s\"", buffer);
		else
			g_strlcpy(cmdline, argv[0], sizeof(cmdline));
		cmdpos = 1;
	}

	for (i = cmdpos; i < argc; i++)
	{
		g_snprintf(cmdline, sizeof(cmdline), "%s %s", cmdline, argv[i]);
		/*MessageBox(NULL, cmdline, cmdline, MB_OK);*/
	}

	if (std_err != NULL)
	{
		hStderrTempFile = GetTempFileHandle(error);
		if (hStderrTempFile == INVALID_HANDLE_VALUE)
		{
			gchar *msg = g_win32_error_message(GetLastError());
			geany_debug("win32_spawn: Second CreateFile failed (%d)", (gint) GetLastError());
			g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR, "%s", msg);
			g_free(msg);
			return FALSE;
		}
	}

	if (std_out != NULL)
	{
		hStdoutTempFile = GetTempFileHandle(error);
		if (hStdoutTempFile == INVALID_HANDLE_VALUE)
		{
			gchar *msg = g_win32_error_message(GetLastError());
			geany_debug("win32_spawn: Second CreateFile failed (%d)", (gint) GetLastError());
			g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR, "%s", msg);
			g_free(msg);
			return FALSE;
		}
	}

	/* Set the bInheritHandle flag so pipe handles are inherited. */
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	/* Get the handle to the current STDOUT and STDERR. */
	gw_spawn.hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	gw_spawn.hStderr = GetStdHandle(STD_ERROR_HANDLE);
	gw_spawn.dwExitCode = 0;

	/* Create a pipe for the child process's STDOUT. */
	if (! CreatePipe(&(gw_spawn.hChildStdoutRd), &(gw_spawn.hChildStdoutWr), &saAttr, 0))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("win32_spawn: Stdout pipe creation failed (%d)", (gint) GetLastError());
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_PIPE, "%s", msg);
		g_free(msg);
		return FALSE;
	}

	/* Ensure that the read handle to the child process's pipe for STDOUT is not inherited.*/
	SetHandleInformation(gw_spawn.hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

	/* Create a pipe for the child process's STDERR. */
	if (! CreatePipe(&(gw_spawn.hChildStderrRd), &(gw_spawn.hChildStderrWr), &saAttr, 0))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("win32_spawn: Stderr pipe creation failed");
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_PIPE, "%s", msg);
		g_free(msg);
		return FALSE;
	}

	/* Ensure that the read handle to the child process's pipe for STDOUT is not inherited.*/
	SetHandleInformation(gw_spawn.hChildStderrRd, HANDLE_FLAG_INHERIT, 0);

	/* Create a pipe for the child process's STDIN.  */
	if (! CreatePipe(&(gw_spawn.hChildStdinRd), &(gw_spawn.hChildStdinWr), &saAttr, 0))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("win32_spawn: Stdin pipe creation failed");
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_PIPE, "%s", msg);
		g_free(msg);
		return FALSE;
	}

	/* Ensure that the write handle to the child process's pipe for STDIN is not inherited. */
	SetHandleInformation(gw_spawn.hChildStdinWr, HANDLE_FLAG_INHERIT, 0);

	/* Now create the child process. */
	fSuccess = CreateChildProcess(&gw_spawn, cmdline, dir, error);
	if (exit_status)
	{
		*exit_status = gw_spawn.dwExitCode;
	}

	if (! fSuccess)
	{
		geany_debug("win32_spawn: Create process failed");
		return FALSE;
	}

	/* Read from pipe that is the standard output for child process. */
	if (std_out != NULL)
	{
		ReadFromPipe(gw_spawn.hChildStdoutRd, gw_spawn.hChildStdoutWr, hStdoutTempFile, error);
		if (! GetContentFromHandle(hStdoutTempFile, &stdout_content, error))
		{
			return FALSE;
		}
		*std_out = stdout_content;
	}

	if (std_err != NULL)
	{
		ReadFromPipe(gw_spawn.hChildStderrRd, gw_spawn.hChildStderrWr, hStderrTempFile, error);
		if (! GetContentFromHandle(hStderrTempFile, &stderr_content, error))
		{
			return FALSE;
		}
		*std_err = stderr_content;
	}
	return TRUE;
}


static gboolean GetContentFromHandle(HANDLE hFile, gchar **content, GError **error)
{
	DWORD filesize;
	gchar * buffer;
	DWORD dwRead;

	filesize = GetFileSize(hFile, NULL);
	if (filesize < 1)
	{
		*content = NULL;
		return TRUE;
	}

	buffer = g_malloc(filesize + 1);
	if (! buffer)
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("GetContentFromHandle: Alloc failed");
		g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR, "%s", msg);
		g_free(msg);
		return FALSE;
	}

	SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
	if (! ReadFile(hFile, buffer, filesize, &dwRead, NULL) || dwRead == 0)
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("GetContentFromHandle: Cannot read tempfile");
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_FAILED, "%s", msg);
		g_free(msg);
		return FALSE;
	}

	if (! CloseHandle(hFile))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("GetContentFromHandle: CloseHandle failed (%d)", (gint) GetLastError());
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_FAILED, "%s", msg);
		g_free(msg);
		g_free(buffer);
		*content = NULL;
		return FALSE;
	}
	buffer[filesize] = '\0';
	*content = buffer;
	return TRUE;
}


gchar *win32_expand_environment_variables(const gchar *str)
{
	gchar expCmdline[32768]; /* 32768 is the limit for ExpandEnvironmentStrings() */

	if (ExpandEnvironmentStrings((LPCTSTR) str, (LPTSTR) expCmdline, sizeof(expCmdline)) != 0)
		return g_strdup(expCmdline);
	else
		return g_strdup(str);
}


static gboolean CreateChildProcess(geany_win32_spawn *gw_spawn, TCHAR *szCmdline,
		const TCHAR *dir, GError **error)
{
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFOW siStartInfo;
	BOOL bFuncRetn = FALSE;
	gchar *expandedCmdline;
	wchar_t w_commandline[CMDSIZE];
	wchar_t w_dir[MAX_PATH];

	/* Set up members of the PROCESS_INFORMATION structure. */
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	/* Set up members of the STARTUPINFO structure.*/
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));

	siStartInfo.cb         = sizeof(STARTUPINFO);
	siStartInfo.hStdError  = gw_spawn->hChildStderrWr;
	siStartInfo.hStdOutput = gw_spawn->hChildStdoutWr;
	siStartInfo.hStdInput  = gw_spawn->hChildStdinRd;
	siStartInfo.dwFlags   |= STARTF_USESTDHANDLES;

	/* Expand environment variables like %blah%. */
	expandedCmdline = win32_expand_environment_variables(szCmdline);

	MultiByteToWideChar(CP_UTF8, 0, expandedCmdline, -1, w_commandline, sizeof(w_commandline));
	MultiByteToWideChar(CP_UTF8, 0, dir, -1, w_dir, sizeof(w_dir));

	/* Create the child process. */
	bFuncRetn = CreateProcessW(NULL,
		w_commandline,             /* command line */
		NULL,          /* process security attributes */
		NULL,          /* primary thread security attributes */
		TRUE,          /* handles are inherited */
		CREATE_NO_WINDOW,             /* creation flags */
		NULL,          /* use parent's environment */
		w_dir,           /* use parent's current directory */
		&siStartInfo,  /* STARTUPINFO pointer */
		&piProcInfo);  /* receives PROCESS_INFORMATION */

	g_free(expandedCmdline);

	if (bFuncRetn == 0)
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("CreateChildProcess: CreateProcess failed (%s)", msg);
		g_set_error(error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED, "%s", msg);
		g_free(msg);
		return FALSE;
	}
	else
	{
		gint i;

		for (i = 0; i < 2 && WaitForSingleObject(piProcInfo.hProcess, 30*1000) == WAIT_TIMEOUT; i++)
		{
			geany_debug("CreateChildProcess: CreateProcess failed");
			TerminateProcess(piProcInfo.hProcess, WAIT_TIMEOUT); /* NOTE: This will not kill grandkids. */
		}

		if (!GetExitCodeProcess(piProcInfo.hProcess, &gw_spawn->dwExitCode))
		{
			gchar *msg = g_win32_error_message(GetLastError());
			geany_debug("GetExitCodeProcess failed: %s", msg);
			g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_FAILED, "%s", msg);
			g_free(msg);
		}
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);
		return bFuncRetn;
	}
	return FALSE;
}


static VOID ReadFromPipe(HANDLE hRead, HANDLE hWrite, HANDLE hFile, GError **error)
{
	DWORD dwRead, dwWritten;
	CHAR chBuf[BUFSIZE];

	/* Close the write end of the pipe before reading from the
	   read end of the pipe. */
	if (! CloseHandle(hWrite))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("ReadFromPipe: Closing handle failed");
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR_PIPE, "%s", msg);
		g_free(msg);
		return;
	}

	/* Read output from the child process, and write to parent's STDOUT. */
	for (;;)
	{
		if (! ReadFile(hRead, chBuf, BUFSIZE, &dwRead, NULL) || dwRead == 0)
			break;

		if (! WriteFile(hFile, chBuf, dwRead, &dwWritten, NULL))
			break;
	}
}


static HANDLE GetTempFileHandle(GError **error)
{
	/* Temp file */
	DWORD dwBufSize = BUFSIZE;
	UINT uRetVal;
	TCHAR szTempName[BUFSIZE];
	TCHAR lpPathBuffer[BUFSIZE];
	DWORD dwRetVal;
	HANDLE hTempFile;

	/* Get the temp path. */
	dwRetVal = GetTempPath(dwBufSize,     /* length of the buffer*/
						   lpPathBuffer); /* buffer for path */

	if (dwRetVal > dwBufSize || (dwRetVal == 0))
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("GetTempFileHandle: GetTempPath failed (%d)", (gint) GetLastError());
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR, "%s", msg);
		g_free(msg);
		return NULL;
	}

	/* Create a temporary file for STDOUT. */
	uRetVal = GetTempFileName(lpPathBuffer, /* directory for tmp files */
							  TEXT("GEANY_VCDIFF_"),  /* temp file name prefix */
							  0,            /* create unique name */
							  szTempName);  /* buffer for name */
	if (uRetVal == 0)
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("GetTempFileName failed (%d)", (gint) GetLastError());
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR, "%s", msg);
		g_free(msg);
		return NULL;
	}

	hTempFile = CreateFile((LPTSTR) szTempName, /* file name */
						   GENERIC_READ | GENERIC_WRITE, /* open r-w */
						   0,                    /* do not share */
						   NULL,                 /* default security */
						   CREATE_ALWAYS,        /* overwrite existing */
						   FILE_ATTRIBUTE_NORMAL,/* normal file */
						   NULL);                /* no template */

	if (hTempFile == INVALID_HANDLE_VALUE)
	{
		gchar *msg = g_win32_error_message(GetLastError());
		geany_debug("GetTempFileHandle: Second CreateFile failed (%d)", (gint) GetLastError());
		g_set_error(error, G_SPAWN_ERROR, G_FILE_ERROR, "%s", msg);
		g_free(msg);
		return NULL;
	}
	return hTempFile;
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

	resolve_link(GDK_WINDOW_HWND(gtk_widget_get_window(main_widgets.window)), wfilename, &path);
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


#endif
