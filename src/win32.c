/*
 *      win32.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *  $Id$
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



static gchar *get_file_filters()
{
	gchar *string;
	gint i, j, len;

	GString *str = g_string_sized_new(100);
	GString *all_patterns = g_string_sized_new(100);
	gchar *tmp;

	for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		tmp = g_strjoinv(";", filetypes[i]->pattern);
		g_string_append_printf(str, "%s\t%s\t", filetypes[i]->title, tmp);
		g_free(tmp);
	}
	// create meta file filter "All Source"
	for (i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		for (j = 0; filetypes[i]->pattern[j] != NULL; j++)
		{
			g_string_append(all_patterns, filetypes[i]->pattern[j]);
			g_string_append_c(all_patterns, ';');
		}
	}
	g_string_append_printf(str, "%s\t%s\t", _("All Source"), all_patterns->str);
	g_string_free(all_patterns, TRUE);

	g_string_append_c(str, '\t'); // the final \0 byte to mark the end of the string
	string = str->str;
	g_string_free(str, FALSE);

	// replace all "\t"s by \0
	len = strlen(string);
	for(i = 0; i < len; i++)
	{
		if (string[i] == '\t') string[i] = '\0';
	}
	return string;
}


static gchar *get_filters(gboolean project_files)
{
	gchar *string;
	gint i, len;

	if (project_files)
	{
		string = g_strconcat(_("Geany project files"), "\t", "*." GEANY_PROJECT_EXT, "\t",
			filetypes[GEANY_FILETYPES_ALL]->title, "\t",
			filetypes[GEANY_FILETYPES_ALL]->pattern[0], "\t", NULL);
	}
	else
	{
		string = g_strconcat(_("Executables"), "\t", "*.exe;*.bat;*.cmd", "\t",
			filetypes[GEANY_FILETYPES_ALL]->title, "\t",
			filetypes[GEANY_FILETYPES_ALL]->pattern[0], "\t", NULL);
	}

	// replace all "\t"s by \0
	len = strlen(string);
	for(i = 0; i < len; i++)
	{
		if (string[i] == '\t') string[i] = '\0';
	}
	return string;
}


/* Returns the directory part of the given filename. */
static gchar *get_dir(const gchar *filename)
{
	if (! g_file_test(filename, G_FILE_TEST_IS_DIR))
		return g_path_get_dirname(filename);
	else
		return g_strdup(filename);
}


/* Callback function for setting the initial directory of the folder open dialog. This could also
 * be done with BROWSEINFO.pidlRoot and SHParseDisplayName but SHParseDisplayName is not available
 * on systems below Windows XP. So, we go the hard way by creating a callback which will set up the
 * folder when the dialog is initialised. Yeah, I like Windows. */
INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	switch(uMsg)
	{
		case BFFM_INITIALIZED:
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM) pData);
			break;

		case BFFM_SELCHANGED:
		{
			// set the status window to the currently selected path.
			static TCHAR szDir[MAX_PATH];
			if (SHGetPathFromIDList((LPITEMIDLIST) lp, szDir))
			{
				SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM) szDir);
			}
			break;
		}
	}
	return 0;
}


/* Shows a folder selection dialog.
 * The selected folder name is returned. */
gchar *win32_show_project_folder_dialog(GtkWidget *parent, const gchar *title,
										const gchar *initial_dir)
{
	BROWSEINFO bi;
	LPCITEMIDLIST pidl;
	gchar *fname = g_malloc(MAX_PATH);
	gchar *dir = get_dir(initial_dir);

	if (parent == NULL)
		parent = app->window;

	memset(&bi, 0, sizeof bi);
	bi.hwndOwner = GDK_WINDOW_HWND(parent->window);
	bi.pidlRoot = NULL;
	bi.lpszTitle = title;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = (LPARAM) dir;
	bi.ulFlags = BIF_DONTGOBELOWDOMAIN | BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT;

	pidl = SHBrowseForFolder(&bi);
	g_free(dir);

	// convert the strange Windows folder list item something into an usual path string ;-)
	if (pidl != NULL && SHGetPathFromIDList(pidl, fname))
	{
		// convert the resulting filename into UTF-8 (from whatever encoding it has at this moment)
		setptr(fname, g_locale_to_utf8(fname, -1, NULL, NULL, NULL));
		return fname;
	}
	else
	{
		g_free(fname);
		return NULL;
	}
}


/* Shows a file open dialog.
 * If allow_new_file is set, the file to be opened doesn't have to exist.
 * The selected file name is returned.
 * If project_file_filter is set, the file open dialog will have a file filter for Geany project
 * files, a filter for executables otherwise. */
gchar *win32_show_project_open_dialog(GtkWidget *parent, const gchar *title,
								      const gchar *initial_dir, gboolean allow_new_file,
								      gboolean project_file_filter)
{
	OPENFILENAME of;
	gint retval;
	gchar *fname = g_malloc(2048);
	gchar *filters = get_filters(project_file_filter);
	gchar *dir = get_dir(initial_dir);

	fname[0] = '\0';

	if (parent == NULL)
		parent = app->window;

	/* initialise file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(parent->window);
	of.lpstrFilter = filters;

	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = 0;
	of.lpstrFile = fname;
	of.lpstrInitialDir = dir;
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	of.lpstrTitle = title;
	of.lpstrDefExt = "";
	of.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY;
	if (! allow_new_file)
		of.Flags |= OFN_FILEMUSTEXIST;

	retval = GetOpenFileName(&of);

	g_free(dir);
	g_free(filters);

	if (! retval)
	{
		if (CommDlgExtendedError())
		{
			gchar *error;
			error = g_strdup_printf("File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
			g_free(error);
		}
		g_free(fname);
		return NULL;
	}
	// convert the resulting filename into UTF-8 (from whatever encoding it has at this moment)
	setptr(fname, g_locale_to_utf8(fname, -1, NULL, NULL, NULL));
	return fname;
}


/* initial_dir can be NULL to use the current working directory.
 * Returns: TRUE if the dialog was not cancelled. */
gboolean win32_show_file_dialog(gboolean file_open, const gchar *initial_dir)
{
	OPENFILENAME of;
	gint retval;
	gchar *fname = g_malloc(2048);
	gchar *filters = get_file_filters();

	fname[0] = '\0';

	/* initialize file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(app->window->window);
	of.lpstrFilter = filters;

	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = GEANY_FILETYPES_ALL + 1;
	of.lpstrFile = fname;
	of.lpstrInitialDir = initial_dir;
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	of.lpstrTitle = NULL;
	of.lpstrDefExt = "";
	if (file_open)
	{
		of.Flags = OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_EXPLORER;
		retval = GetOpenFileName(&of);
	}
	else
	{
		of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		retval = GetSaveFileName(&of);
	}

	g_free(filters);

	if (!retval)
	{
		if (CommDlgExtendedError())
		{
			gchar error[100];
			snprintf(error, sizeof error, "File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
		}
		g_free(fname);
		return FALSE;
	}

	if (file_open)
	{
		gchar file_name[255];
		gint x;

		x = of.nFileOffset - 1;
		if (x != strlen(fname))
		{	// open a single file

			// convert the resulting filename into UTF-8
			setptr(fname, g_locale_to_utf8(fname, -1, NULL, NULL, NULL));

			document_open_file(fname, of.Flags & OFN_READONLY, NULL, NULL);
		}
		else
		{	// open multiple files
			document_delay_colourise();

			for (; ;)
			{
				if (! fname[x])
				{
					gchar *utf8_filename;
					if (! fname[x+1]) break;

					g_snprintf(file_name, 254, "%s\\%s", fname, fname + x + 1);

					// convert the resulting filename into UTF-8
					utf8_filename = g_locale_to_utf8(file_name, -1, NULL, NULL, NULL);
					document_open_file(utf8_filename, of.Flags & OFN_READONLY, NULL, NULL);
					g_free(utf8_filename);
				}
				x++;
			}
			document_colourise_new();
		}
	}
	else
	{
		gint idx = document_get_cur_idx();
		// convert the resulting filename into UTF-8
		doc_list[idx].file_name = g_locale_to_utf8(fname, -1, NULL, NULL, NULL);
		document_save_file(idx, TRUE);
	}
	g_free(fname);
	return (retval != 0);
}


void win32_show_font_dialog(void)
{
	CHOOSEFONT cf;
	gint retval;
	static LOGFONT lf;        // logical font structure

	memset(&cf, 0, sizeof cf);
	cf.lStructSize = sizeof cf;
	cf.hwndOwner = GDK_WINDOW_HWND(app->window->window);
	cf.lpLogFont = &lf;
	cf.Flags = CF_APPLY | CF_NOSCRIPTSEL | CF_FORCEFONTEXIST | CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;

	retval = ChooseFont(&cf);

	if (!retval)
	{
		if (CommDlgExtendedError())
		{
			//gchar error[100];
			//snprintf(error, 255, _("Font dialog box error (%x)"), (int)CommDlgExtendedError());
			//MessageBox(NULL, "Font not availab", _("Error"), MB_OK | MB_ICONERROR);
		}
		return;
	}
	else
	{
			gchar *editorfont = g_strdup_printf("%s %d", lf.lfFaceName,
				(cf.iPointSize / 10));
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
	gint idx = document_get_cur_idx();

	// Initialize CHOOSECOLOR
	memset(&cc, 0, sizeof cc);
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = GDK_WINDOW_HWND(app->window->window);
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

		document_insert_colour(idx, hex);
	}
	g_free(hex);
}


void win32_show_pref_file_dialog(GtkEntry *item)
{
	OPENFILENAME of;
	gint retval;
	gchar *fname = g_malloc(512);
	gchar **field, *filename, *tmp;
	gchar *filters = get_filters(FALSE);

	fname[0] = '\0';

	// cut the options from the command line
	field = g_strsplit(gtk_entry_get_text(GTK_ENTRY(item)), " ", 2);
	if (field[0] && g_file_test(field[0], G_FILE_TEST_EXISTS))
	{
		filename = g_find_program_in_path(field[0]);
		strcpy(fname, filename);
		g_free(filename);
	}

	/* initialize file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = GDK_WINDOW_HWND(ui_widgets.prefs_dialog->window);

	of.lpstrFilter = filters;
	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = 1;

	of.lpstrFile = fname;
	of.nMaxFile = 512;
	of.lpstrFileTitle = NULL;
	//of.lpstrInitialDir = g_get_home_dir();
	of.lpstrInitialDir = NULL;
	of.lpstrTitle = NULL;
	of.lpstrDefExt = "exe";
	of.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_EXPLORER;
	retval = GetOpenFileName(&of);

	g_free(filters);

	if (!retval)
	{
		if (CommDlgExtendedError())
		{
			gchar error[100];
			snprintf(error, sizeof error, "File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(NULL, GTK_MESSAGE_ERROR, error);
		}
		g_strfreev(field);
		g_free(fname);
		return;
	}

	if ((of.nFileOffset - 1) != strlen(fname))
	{
		tmp = g_strdup(fname);
		if (g_strv_length(field) > 1)
			// add the command line args of the old command
			/// TODO this fails badly when the old command contained spaces, we need quoting here
			filename = g_strconcat(tmp, " ", field[1], NULL);
		else
		{
			filename = tmp;
			tmp = NULL;
		}
		gtk_entry_set_text(GTK_ENTRY(item), filename);
		g_free(filename);
		g_free(tmp);
	}
	g_strfreev(field);
	g_free(fname);
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

	// convert the Unicode chars to wide chars
	/// TODO test if LANG == C then possibly skip conversion => g_win32_getlocale()
	MultiByteToWideChar(CP_UTF8, 0, msg, -1, w_msg, G_N_ELEMENTS(w_msg));
	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, G_N_ELEMENTS(w_title));

	if (parent != NULL)
		parent_hwnd = GDK_WINDOW_HWND(parent->window);
	else if (app->window != NULL)
		parent_hwnd = GDK_WINDOW_HWND(app->window->window);

	// display the message box
	rc = MessageBoxW(parent_hwnd, w_msg, w_title, t);

	if (type == GTK_MESSAGE_QUESTION && rc != IDYES)
		ret = FALSE;

	return ret;
}


/* Little wrapper for _waccess(), returns errno or 0 if there was no error */
gint win32_check_write_permission(const gchar *dir)
{
	static wchar_t w_dir[512];
	MultiByteToWideChar(CP_UTF8, 0, dir, -1, w_dir, sizeof w_dir);
	if (_waccess(w_dir, R_OK | W_OK) != 0)
		return errno;
	else
		return 0;
}


/* Special dialog to ask for an action when closing an unsaved file */
gint win32_message_dialog_unsaved(const gchar *msg)
{
	static wchar_t w_msg[512];
	static wchar_t w_title[512];
	HWND parent_hwnd = NULL;
	gint ret;

	// convert the Unicode chars to wide chars
	MultiByteToWideChar(CP_UTF8, 0, msg, -1, w_msg, G_N_ELEMENTS(w_msg));
	MultiByteToWideChar(CP_UTF8, 0, _("Question"), -1, w_title, G_N_ELEMENTS(w_title));

	if (app->window != NULL)
		parent_hwnd = GDK_WINDOW_HWND(app->window->window);

	ret = MessageBoxW(parent_hwnd, w_msg, w_title, MB_YESNOCANCEL | MB_ICONQUESTION);
	switch(ret)
	{
		case IDYES: ret = GTK_RESPONSE_YES; break;
		case IDNO: ret = GTK_RESPONSE_NO; break;
		case IDCANCEL: ret = GTK_RESPONSE_CANCEL; break;
	}

	return ret;
}

/* Just a simple wrapper function to open a browser window */
void win32_open_browser(const gchar *uri)
{
	ShellExecute(NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
}


static void debug_setup_console()
{
	static const WORD MAX_CONSOLE_LINES = 500;
	gint	 hConHandle;
	glong	 lStdHandle;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	FILE	*fp;

	// allocate a console for this app
	AllocConsole();

	// set the screen buffer to be big enough to let us scroll text
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = MAX_CONSOLE_LINES;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	// redirect unbuffered STDOUT to the console
	lStdHandle = (long)GetStdHandle(STD_OUTPUT_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen(hConHandle, "w");
	*stdout = *fp;
	setvbuf(stdout, NULL, _IONBF, 0);

	// redirect unbuffered STDERR to the console
	lStdHandle = (long)GetStdHandle(STD_ERROR_HANDLE);
	hConHandle = _open_osfhandle(lStdHandle, _O_TEXT);
	fp = _fdopen(hConHandle, "w");
	*stderr = *fp;
	setvbuf(stderr, NULL, _IONBF, 0);
}


static void debug_log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message,
					   gpointer user_data)
{
	if (log_domain != NULL)
		fprintf(stderr, "%s: %s\n", log_domain, message);
	else
		fprintf(stderr, "%s\n", message);
}


void win32_init_debug_code()
{
#ifndef GEANY_DEBUG
	if (app->debug_mode)
#endif
	{	// create a console window to get log messages on Windows
		debug_setup_console();
		// change the log handlers to output log messages in ther created console window
		g_log_set_handler("GLib",
			G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION, debug_log_handler, NULL);
		g_log_set_default_handler(debug_log_handler, NULL);
	}
}


/* Used to get special Windows folder paths like %appdata%,
 * Code taken from Sylpheed, thanks */
gchar *win32_get_appdata_folder()
{
	gchar *folder = NULL;
	gint nfolder = CSIDL_APPDATA;
	HRESULT hr;

	if (G_WIN32_HAVE_WIDECHAR_API())
	{
		wchar_t path[MAX_PATH + 1];
		hr = SHGetFolderPathW(NULL, nfolder, NULL, 0, path);
		if (hr == S_OK)
			folder = g_utf16_to_utf8(path, -1, NULL, NULL, NULL);
	}
	else
	{
		gchar path[MAX_PATH + 1];
		hr = SHGetFolderPathA(NULL, nfolder, NULL, 0, path);
		if (hr == S_OK)
			folder = g_locale_to_utf8(path, -1, NULL, NULL, NULL);
	}

	return folder;
}

#endif
