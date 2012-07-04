/*
 *      keyfile.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef GEANY_KEYFILE_H
#define GEANY_KEYFILE_H 1


extern GPtrArray *pref_groups;


void configuration_init(void);

void configuration_finalize(void);

struct StashGroup;

void configuration_add_pref_group(struct StashGroup *group, gboolean for_prefs_dialog);

void configuration_add_various_pref_group(struct StashGroup *group);

void configuration_save(void);

gboolean configuration_load(void);

void configuration_open_files(void);

void configuration_reload_default_session(void);

void configuration_save_default_session(void);

void configuration_load_session_files(GKeyFile *config, gboolean read_recent_files);

void configuration_save_session_files(GKeyFile *config);

/* set some settings which are already read from the config file, but need other things, like the
 * realisation of the main window */
void configuration_apply_settings(void);

#endif
