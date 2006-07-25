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
	GEANY_FILETYPES_D,			// 2
	GEANY_FILETYPES_JAVA,		// 3
	GEANY_FILETYPES_PASCAL,		// 4
	GEANY_FILETYPES_ASM,		// 5
	GEANY_FILETYPES_CAML,		// 6
	GEANY_FILETYPES_PERL,		// 7
	GEANY_FILETYPES_PHP,		// 8
	GEANY_FILETYPES_PYTHON,		// 9
	GEANY_FILETYPES_RUBY,		// 10
	GEANY_FILETYPES_TCL,		// 11
	GEANY_FILETYPES_SH,			// 12
	GEANY_FILETYPES_MAKE,		// 13
 	GEANY_FILETYPES_XML,		// 14
	GEANY_FILETYPES_DOCBOOK,	// 15
/*
	GEANY_FILETYPES_HTML,		// 16
*/
	GEANY_FILETYPES_CSS,		// 17
	GEANY_FILETYPES_SQL,		// 18
	GEANY_FILETYPES_LATEX,		// 19
	GEANY_FILETYPES_OMS,		// 20
	GEANY_FILETYPES_CONF,		// 21
	GEANY_FILETYPES_ALL,		// 22
	GEANY_MAX_FILE_TYPES		// 23
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
	guint	 		  uid;				// unique id as reference for saved filetype in config file
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
