/*
 *      filetypes.h - this file is part of Geany, a fast and lightweight IDE
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * $Id$
 */


#ifndef GEANY_FILETYPES_H
#define GEANY_FILETYPES_H 1

#ifndef PLAT_GTK
#   define PLAT_GTK 1	// needed for ScintillaWidget.h
#endif

#include "Scintilla.h"
#include "ScintillaWidget.h"


typedef enum
{
	GEANY_FILETYPES_C = 0,
	GEANY_FILETYPES_CPP,
	GEANY_FILETYPES_D,
	GEANY_FILETYPES_JAVA,
	GEANY_FILETYPES_PASCAL,
	GEANY_FILETYPES_ASM,
	GEANY_FILETYPES_FORTRAN,
	GEANY_FILETYPES_CAML,
	GEANY_FILETYPES_PERL,
	GEANY_FILETYPES_PHP,
	GEANY_FILETYPES_JS,
	GEANY_FILETYPES_PYTHON,
	GEANY_FILETYPES_RUBY,
	GEANY_FILETYPES_TCL,
	GEANY_FILETYPES_LUA,
	GEANY_FILETYPES_FERITE,
	GEANY_FILETYPES_SH,
	GEANY_FILETYPES_MAKE,
 	GEANY_FILETYPES_XML,
	GEANY_FILETYPES_DOCBOOK,
	GEANY_FILETYPES_HTML,
	GEANY_FILETYPES_CSS,
	GEANY_FILETYPES_SQL,
	GEANY_FILETYPES_LATEX,
	GEANY_FILETYPES_OMS,
	GEANY_FILETYPES_VHDL,
	GEANY_FILETYPES_DIFF,
	GEANY_FILETYPES_CONF,
	GEANY_FILETYPES_ALL,	// must be last filetype
	GEANY_MAX_FILE_TYPES
} filetype_id;

// Safe wrapper to get the id field of a possibly NULL filetype pointer.
#define FILETYPE_ID(filetype_ptr) \
	(((filetype_ptr) != NULL) ? (filetype_ptr)->id : GEANY_FILETYPES_ALL)


struct build_actions
{
	gboolean	can_compile:1;
	gboolean	can_link:1;
	gboolean	can_exec:1;
};

struct build_programs
{
	gchar *compiler;
	gchar *linker;
	gchar *run_cmd;
	gchar *run_cmd2;
	gboolean modified;
};

typedef struct filetype
{
	filetype_id		  id;
	guint	 		  uid;				// unique id as reference for saved filetype in config file
	GtkWidget		 *item;				// holds a pointer to the menu item for this filetypes
	langType 		  lang;				// represents the langType of tagmanager(see the table
										// in tagmanager/parsers.h), -1 represents all, -2 none
	gchar	 		 *name;				// will be used as name for tagmanager
	gboolean 		  has_tags;			// indicates whether there is a tag parser for it or not
	gchar	 		 *title;			// will be shown in the file open dialog
	gchar	 		 *extension;
	gchar			**pattern;
	gchar	 		 *context_action_cmd;
	gchar	 		 *comment_open;
	gchar	 		 *comment_close;
	gboolean  		  comment_use_indent;
	struct build_programs	*programs;
	struct build_actions	*actions;
	void (*style_func_ptr) (ScintillaObject*);
} filetype;

filetype *filetypes[GEANY_MAX_FILE_TYPES];


// If uid is valid, return corresponding filetype, otherwise NULL.
filetype *filetypes_get_from_uid(gint uid);

/* inits the filetype array and fill it with the known filetypes
 * and create the filetype menu*/
void filetypes_init_types();

/* simple filetype selection based on the filename extension */
filetype *filetypes_get_from_filename(gint idx);

/* frees the array and all related pointers */
void filetypes_free_types();

void filetypes_get_config(GKeyFile *config, GKeyFile *configh, gint ft);

void filetypes_save_commands();

void filetypes_select_radio_item(const filetype *ft);

GtkFileFilter *filetypes_create_file_filter(filetype *ft);

GtkFileFilter *filetypes_create_file_filter_all_source();

gchar *filetypes_get_conf_extension(gint filetype_idx);

#endif
