/*
 *      win32.c - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2006 Enrico Troeger <enrico.troeger@uvena.de>
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


// special functions for the win32 platform

#include "geany.h"

#ifdef GEANY_WIN32

#include <windows.h>
#include <commdlg.h>

#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "win32.h"

#include "document.h"
#include "support.h"
#include "utils.h"
#include "sciwrappers.h"
#include "dialogs.h"


static gchar *filters;
static gchar *exe_filters;


static gchar *win32_get_filters(gboolean exe)
{
	gchar *string = "";
	gint i, len;

	if (exe)
	{
		string = g_strconcat(string,
				_("Executables"), "\t", "*.exe;*.bat;*.cmd", "\t",
				filetypes[GEANY_FILETYPES_ALL]->title, "\t",
				g_strjoinv(";", filetypes[GEANY_FILETYPES_ALL]->pattern), "\t", NULL);
	}
	else
	{
		for(i = 0; i < GEANY_MAX_FILE_TYPES; i++)
		{
			if (filetypes[i])
			{
				string = g_strconcat(string, filetypes[i]->title, "\t", g_strjoinv(";", filetypes[i]->pattern), "\t", NULL);
			}
		}
	}
	string = g_strconcat(string, "\t", NULL);

	// replace all "\t"s by \0
	len = strlen(string);
	for(i = 0; i < len; i++)
	{
		if (string[i] == '\t') string[i] = '\0';
	}
	return string;
}


void win32_show_file_dialog(gboolean file_open)
{
	OPENFILENAME of;
	gint retval;
	gchar *fname = g_malloc(2048);
	gchar *current_dir = utils_get_current_file_dir();

	fname[0] = '\0';

	if (! filters) filters = win32_get_filters(FALSE);

	/* initialize file dialog info struct */
	memset(&of, 0, sizeof of);
#ifdef OPENFILENAME_SIZE_VERSION_400
	of.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
	of.lStructSize = sizeof of;
#endif
	of.hwndOwner = NULL;
	of.lpstrFilter = filters;

	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = GEANY_FILETYPES_ALL + 1;
	of.lpstrFile = fname;
	of.lpstrInitialDir  = current_dir;
	of.nMaxFile = 2048;
	of.lpstrFileTitle = NULL;
	of.lpstrTitle = NULL;
	of.lpstrDefExt = "c";
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

	g_free(current_dir);

	if (!retval)
	{
		if (CommDlgExtendedError())
		{
			gchar error[100];
			snprintf(error, sizeof error, "File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(GTK_MESSAGE_ERROR, error);
		}
		g_free(fname);
		return;
	}

	if (file_open)
	{
		gchar file_name[255];
		gint x;

		x = of.nFileOffset - 1;
		if (x != strlen(fname))
		{	// open a single file
			if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) >= GEANY_MAX_OPEN_FILES)
			{
				dialogs_show_error(
			_("You have opened too many files. There is a limit of %d concurrent open files."),
			GEANY_MAX_OPEN_FILES);
			}
			else
			{
				document_open_file(-1, fname, 0, of.Flags & OFN_READONLY, NULL, NULL);
			}
		}
		else
		{	// open mutiple files
			for (; ;)
			{
				if (! fname[x])
				{
					if (! fname[x+1] &&	(
						gtk_notebook_get_n_pages(GTK_NOTEBOOK(app->notebook)) < GEANY_MAX_OPEN_FILES))
						break;

					g_snprintf(file_name, 254, "%s\\%s", fname, fname + x + 1);
					document_open_file(-1, file_name, 0, of.Flags & OFN_READONLY, NULL, NULL);
				}
				x++;
			}
		}
	}
	else
	{
		gint idx = document_get_cur_idx();
		doc_list[idx].file_name = g_strdup(fname);
		document_save_file(idx);
	}
	g_free(fname);
}


void win32_show_font_dialog(void)
{
	CHOOSEFONT cf;
	gint retval;
	static LOGFONT lf;        // logical font structure

	memset(&cf, 0, sizeof cf);
	cf.lStructSize = sizeof cf;
	cf.hwndOwner = NULL;
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
			utils_set_editor_font(editorfont);
			g_free(editorfont);
	}
}


void win32_show_color_dialog(void)
{
	CHOOSECOLOR cc;
	static COLORREF acr_cust_clr[16];
	static DWORD rgb_current;
	gchar *hex = g_malloc0(12);
	gint idx = document_get_cur_idx();

	// Initialize CHOOSECOLOR
	memset(&cc, 0, sizeof cc);
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = NULL;
	cc.lpCustColors = (LPDWORD) acr_cust_clr;
	cc.rgbResult = rgb_current;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;

	if (ChooseColor(&cc))
	{
		rgb_current = cc.rgbResult;
		g_snprintf(hex, 11, "#%2X%2X%2X",
	      (guint) (utils_scale_round(GetRValue(rgb_current), 255)),
	      (guint) (utils_scale_round(GetGValue(rgb_current), 255)),
	      (guint) (utils_scale_round(GetBValue(rgb_current), 255)));

		sci_add_text(doc_list[idx].sci, hex);
	}
	g_free(hex);
}


void win32_show_pref_file_dialog(GtkEntry *item)
{
	OPENFILENAME of;
	gint retval;
	gchar *fname = g_malloc(512);
	gchar **field, *filename, *tmp;

	fname[0] = '\0';
	if (! exe_filters) exe_filters = win32_get_filters(TRUE);

	// cut the options from the command line
	field = g_strsplit(gtk_entry_get_text(GTK_ENTRY(item)), " ", 2);
	if (field[0])
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
	of.hwndOwner = NULL;

	of.lpstrFilter = exe_filters;
	of.lpstrCustomFilter = NULL;
	of.nFilterIndex = 1;

	of.lpstrFile = fname;
	of.nMaxFile = 512;
	of.lpstrFileTitle = NULL;
	//of.lpstrInitialDir = g_get_home_dir();
	of.lpstrInitialDir = NULL;
	of.lpstrTitle = NULL;
	of.lpstrDefExt = "exe";
	of.Flags = OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_EXPLORER;
	retval = GetOpenFileName(&of);

	if (!retval)
	{
		if (CommDlgExtendedError())
		{
			gchar error[100];
			snprintf(error, sizeof error, "File dialog box error (%x)", (int)CommDlgExtendedError());
			win32_message_dialog(GTK_MESSAGE_ERROR, error);
		}
		g_strfreev(field);
		g_free(fname);
		return;
	}

	if ((of.nFileOffset - 1) != strlen(fname))
	{
		tmp = g_strdup(fname);
		if (g_strv_length(field) > 1)
			// haha, pfad- und dateinamen mit leerzeichen??
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
gboolean win32_message_dialog(GtkMessageType type, const gchar *msg)
{
	gboolean ret = TRUE;
	gint rc;
	guint t;
	const gchar *title;
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
		default:
		{
			t = MB_OK | MB_ICONINFORMATION;
			title = _("Information");
			break;
		}
	}

	// convert the Unicode chars to wide chars
	/// TODO test if LANG == C then possibly skip conversion
	MultiByteToWideChar(CP_UTF8, 0, msg, -1, w_msg, sizeof(w_msg)/sizeof(w_msg[0]));
	MultiByteToWideChar(CP_UTF8, 0, title, -1, w_title, sizeof(w_title)/sizeof(w_title[0]));

	// display the message box
	rc = MessageBoxW(NULL, w_msg, w_title, t);

	if (type == GTK_MESSAGE_QUESTION && rc != IDYES)
		ret = FALSE;

	return ret;
}


/* Special dialog to ask for an action when closing an unsaved file */
gint win32_message_dialog_unsaved(const gchar *msg)
{
	static wchar_t w_msg[512];
	static wchar_t w_title[512];
	gint ret;

	// convert the Unicode chars to wide chars
	MultiByteToWideChar(CP_UTF8, 0, msg, -1, w_msg, sizeof(w_msg)/sizeof(w_msg[0]));
	MultiByteToWideChar(CP_UTF8, 0, _("Question"), -1, w_title, sizeof(w_title)/sizeof(w_title[0]));

	ret = MessageBoxW(NULL, w_msg, w_title, MB_YESNOCANCEL | MB_ICONQUESTION);
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

#endif
