/*
 *      win32.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005 Enrico Troeger <enrico.troeger@uvena.de>
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



#ifdef GEANY_WIN32


/*void set_app_font(const char *fontname);

char *default_windows_menu_fontspec(void);

void try_to_get_windows_font(void);
*/
//gchar *win32_get_filters(void);

void win32_show_pref_file_dialog(GtkEntry *item);

void win32_show_file_dialog(gboolean file_open);

void win32_show_font_dialog(void);

void win32_show_color_dialog(void);

double my_strtod(const char *source, char **end);

#endif
