/*
 *      keyfile.h - this file is part of Geany, a fast and lightweight IDE
 *
 *      Copyright 2005-2007 Enrico Tröger <enrico.troeger@uvena.de>
 *      Copyright 2006-2007 Nick Treleaven <nick.treleaven@btinternet.com>
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
 */


#ifndef GEANY_KEYFILE_H
#define GEANY_KEYFILE_H 1


void configuration_save();

gboolean configuration_load();

gboolean configuration_open_files();

void configuration_load_session_files(GKeyFile *config);

void configuration_save_session_files(GKeyFile *config);

void configuration_read_filetype_extensions();

void configuration_read_autocompletions();

/* set some settings which are already read from the config file, but need other things, like the
 * realisation of the main window */
void configuration_apply_settings();

#ifdef GEANY_DEBUG
/* Generate the config files in "data/" from defaults */
void configuration_generate_data_files();
#endif

#endif
