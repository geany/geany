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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  $Id$
 */


// special functions for the win32 platform

#include "geany.h"

#ifdef GEANY_WIN32

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


//static gchar appfontname[128] = "tahoma 8"; /* fallback value */
static gchar *filters;
static gchar *exe_filters;

/*
void set_app_font (const char *fontname)
{
    GtkSettings *settings;

    if (fontname != NULL && *fontname == 0) return;

    settings = gtk_settings_get_default();

    if (fontname == NULL)
	{
		g_object_set(G_OBJECT(settings), "gtk-font-name", appfontname, NULL);
    }
	else
	{
		GtkWidget *w;
		PangoFontDescription *pfd;
		PangoContext *pc;
		PangoFont *pfont;

		w = gtk_label_new(NULL);
		pfd = pango_font_description_from_string(fontname);
		pc = gtk_widget_get_pango_context(w);
		pfont = pango_context_load_font(pc, pfd);

		if (pfont != NULL)
		{
			strcpy(appfontname, fontname);
			g_object_set(G_OBJECT(settings), "gtk-font-name", appfontname, NULL);
		}

		gtk_widget_destroy(w);
		pango_font_description_free(pfd);
    }
}

char *default_windows_menu_fontspec (void)
{
    gchar *fontspec = NULL;
    NONCLIENTMETRICS ncm;

    memset(&ncm, 0, sizeof ncm);
    ncm.cbSize = sizeof ncm;

    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
	{
		HDC screen = GetDC(0);
		double y_scale = 72.0 / GetDeviceCaps(screen, LOGPIXELSY);
		int point_size = (int) (ncm.lfMenuFont.lfHeight * y_scale);

		if (point_size < 0) point_size = -point_size;
		fontspec = g_strdup_printf("%s %d", ncm.lfMenuFont.lfFaceName, point_size);
		ReleaseDC(0, screen);
    }

    return fontspec;
}

void try_to_get_windows_font (void)
{
    gchar *fontspec = default_windows_menu_fontspec();

    if (fontspec != NULL)
	{
		int match = 0;
		PangoFontDescription *pfd;
		PangoFont *pfont;
		PangoContext *pc;
		GtkWidget *w;

		pfd = pango_font_description_from_string(fontspec);

		w = gtk_label_new(NULL);
		pc = gtk_widget_get_pango_context(w);
		pfont = pango_context_load_font(pc, pfd);
		match = (pfont != NULL);

		pango_font_description_free(pfd);
		g_object_unref(G_OBJECT(pc));
		gtk_widget_destroy(w);

		if (match) set_app_font(fontspec);
		g_free(fontspec);
    }
}
*/

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
			snprintf(error, 255, "File dialog box error (%x)", (int)CommDlgExtendedError());
			MessageBox(NULL, error, _("Error"), MB_OK | MB_ICONERROR);
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
				dialogs_show_file_open_error();
			}
			else
			{
				document_open_file(-1, fname, 0, of.Flags & OFN_READONLY, NULL);
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
					document_open_file(-1, file_name, 0, of.Flags & OFN_READONLY, NULL);
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
			g_free(app->editor_font);
			app->editor_font = g_strdup_printf("%s %d", lf.lfFaceName, (cf.iPointSize / 10));
			utils_set_font();
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
			snprintf(error, 255, "File dialog box error (%x)", (int)CommDlgExtendedError());
			MessageBox(NULL, error, _("Error"), MB_OK | MB_ICONERROR);
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

#endif
