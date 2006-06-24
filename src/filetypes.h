/*
 *      filetypes.h - this file is part of Geany, a fast and lightweight IDE
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
 * $Id$
 */


#ifndef GEANY_FILETYPES_H
#define GEANY_FILETYPES_H 1


enum
{
	GEANY_FILETYPES_C = 0,		// 0
	GEANY_FILETYPES_CPP,		// 1
	GEANY_FILETYPES_JAVA,		// 2
	GEANY_FILETYPES_PERL,		// 3
	GEANY_FILETYPES_PHP,		// 4
	GEANY_FILETYPES_XML,		// 5
	GEANY_FILETYPES_DOCBOOK,	// 6
	GEANY_FILETYPES_PYTHON,		// 7
	GEANY_FILETYPES_LATEX,		// 8
	GEANY_FILETYPES_PASCAL,		// 9
	GEANY_FILETYPES_SH,			// 10
	GEANY_FILETYPES_MAKE,		// 11
	GEANY_FILETYPES_CSS,		// 12
	GEANY_FILETYPES_CONF,		// 13
	GEANY_FILETYPES_ASM,		// 14
	GEANY_FILETYPES_SQL,		// 15
	GEANY_FILETYPES_CAML,		// 16
	GEANY_FILETYPES_OMS,		// 17
	GEANY_FILETYPES_RUBY,		// 18
	GEANY_FILETYPES_TCL,		// 19
	GEANY_FILETYPES_ALL,		// 20
	GEANY_MAX_FILE_TYPES		// 21
};

struct build_menu_items
{
	GtkWidget		*menu;
	GtkWidget		*item_compile;
	GtkWidget		*item_link;
	GtkWidget		*item_exec;
	gboolean		 can_compile;
	gboolean		 can_link;
	gboolean		 can_exec;
};

struct build_programs
{
	gchar *compiler;
	gchar *linker;
	gchar *run_cmd;
	gchar *run_cmd2;
};

typedef struct filetype
{
	guint	 		  id;
	langType 		  lang;				// represents the langType of tagmanager(see the table
										// in tagmanager/parsers.h), -1 represents all, -2 none
	gchar	 		 *name;				// will be used as name for tagmanager
	gboolean 		  has_tags;			// indicates whether there is a tag parser for it or not
	gchar	 		 *title;			// will be shown in the file open dialog
	gchar	 		 *extension;
	gchar			**pattern;
	gchar	 		 *comment_open;
	gchar	 		 *comment_close;
	gboolean  		  comment_use_indent;
	struct build_programs *programs;
	struct build_menu_items *menu_items;
	void (*style_func_ptr) (ScintillaObject*);
} filetype;

filetype *filetypes[GEANY_MAX_FILE_TYPES];


/* inits the filetype array and fill it with the known filetypes
 * and create the filetype menu*/
void filetypes_init_types(void);


/* simple filetype selection based on the filename extension */
filetype *filetypes_get_from_filename(const gchar *filename);

/* frees the array and all related pointers */
void filetypes_free_types(void);

gchar *filetypes_get_template(filetype *ft);

void filetypes_get_config(GKeyFile *config, GKeyFile *configh, gint ft);

#endif
