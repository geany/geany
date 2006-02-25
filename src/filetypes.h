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
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#ifndef GEANY_FILETYPES_H
#define GEANY_FILETYPES_H 1


enum
{
	GEANY_FILETYPES_C = 0,
	GEANY_FILETYPES_CPP,
	GEANY_FILETYPES_JAVA,
	GEANY_FILETYPES_PERL,
	GEANY_FILETYPES_PHP,
	GEANY_FILETYPES_XML,
	GEANY_FILETYPES_DOCBOOK,
	GEANY_FILETYPES_PYTHON,
	GEANY_FILETYPES_TEX,
	GEANY_FILETYPES_PASCAL,
	GEANY_FILETYPES_SH,
	GEANY_FILETYPES_MAKE,
	GEANY_FILETYPES_CSS,
	GEANY_FILETYPES_CONF,
	GEANY_FILETYPES_ASM,
	GEANY_FILETYPES_SQL,
	GEANY_FILETYPES_CAML,
	GEANY_FILETYPES_OMS,
	GEANY_FILETYPES_ALL,
	GEANY_MAX_FILE_TYPES
};


typedef struct filetype
{
	guint	 id;
	gchar	*name;			// will be used as name for tagmanager
	gboolean has_tags;		// indicates whether there is a tag parser for it or not
	gchar	*title;			// will be shown in the file open dialog
	gchar	*extension;
	gchar	**pattern;
	void	(*style_func_ptr) (ScintillaObject*);
} filetype;

filetype *filetypes[GEANY_MAX_FILE_TYPES];


/* inits the filetype array and fill it with the known filetypes
 * and create the filetype menu*/
void filetypes_init_types(void);


/* simple filetype selection based on the filename extension */
filetype *filetypes_get_from_filename(const gchar *filename);


void filetypes_create_menu_item(GtkWidget *menu, gchar *label, filetype *ftype);

void filetypes_create_newmenu_item(GtkWidget *menu, gchar *label, filetype *ftype);

/* frees the array and all related pointers */
void filetypes_free_types(void);

gchar *filetypes_get_template(filetype *ft);


#endif
