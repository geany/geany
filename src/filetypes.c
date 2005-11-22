/*
 *      filetypes.c - this file is part of Geany, a fast and lightweight IDE
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
 */


#include <string.h>

#include "geany.h"
#include "filetypes.h"
#include "highlighting.h"
#include "support.h"
#include "callbacks.h"
#include "templates.h"


/* inits the filetype array and fill it with the known filetypes
 * and create the filetype menu*/
void filetypes_init_types(void)
{
	GtkWidget *filetype_menu = lookup_widget(app->window, "set_filetype1_menu");
	GtkWidget *template_menu = lookup_widget(app->window, "menu_new_with_template1_menu");

	filetypes[GEANY_FILETYPES_C] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_C]->id = GEANY_FILETYPES_C;
	filetypes[GEANY_FILETYPES_C]->name = g_strdup("C");
	filetypes[GEANY_FILETYPES_C]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_C]->title = g_strdup(_("C source file"));
	filetypes[GEANY_FILETYPES_C]->extension = g_strdup("c");
	filetypes[GEANY_FILETYPES_C]->pattern[0] = g_strdup("*.c");
	filetypes[GEANY_FILETYPES_C]->pattern[1] = g_strdup("*.h");
	filetypes[GEANY_FILETYPES_C]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_C]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_C]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_C]->style_func_ptr = styleset_c;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_C]->title, filetypes[GEANY_FILETYPES_C]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_C]->title, filetypes[GEANY_FILETYPES_C]);

	filetypes[GEANY_FILETYPES_CPP] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_CPP]->id = GEANY_FILETYPES_CPP;
	filetypes[GEANY_FILETYPES_CPP]->name = g_strdup("C++");
	filetypes[GEANY_FILETYPES_CPP]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CPP]->title = g_strdup(_("C++ source file"));
	filetypes[GEANY_FILETYPES_CPP]->extension = g_strdup("cpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern[0] = g_strdup("*.cpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern[1] = g_strdup("*.cxx");
	filetypes[GEANY_FILETYPES_CPP]->pattern[2] = g_strdup("*.h");
	filetypes[GEANY_FILETYPES_CPP]->pattern[3] = g_strdup("*.hpp");
	filetypes[GEANY_FILETYPES_CPP]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_CPP]->style_func_ptr = styleset_c;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_CPP]->title, filetypes[GEANY_FILETYPES_CPP]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_CPP]->title, filetypes[GEANY_FILETYPES_CPP]);

	filetypes[GEANY_FILETYPES_JAVA] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_JAVA]->id = GEANY_FILETYPES_JAVA;
	filetypes[GEANY_FILETYPES_JAVA]->name = g_strdup("Java");
	filetypes[GEANY_FILETYPES_JAVA]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_JAVA]->title = g_strdup(_("Java source file"));
	filetypes[GEANY_FILETYPES_JAVA]->extension = g_strdup("java");
	filetypes[GEANY_FILETYPES_JAVA]->pattern[0] = g_strdup("*.java");
	filetypes[GEANY_FILETYPES_JAVA]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_JAVA]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_JAVA]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_JAVA]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_JAVA]->style_func_ptr = styleset_java;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_JAVA]->title, filetypes[GEANY_FILETYPES_JAVA]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_JAVA]->title, filetypes[GEANY_FILETYPES_JAVA]);

	filetypes[GEANY_FILETYPES_PERL] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_PERL]->id = GEANY_FILETYPES_PERL;
	filetypes[GEANY_FILETYPES_PERL]->name = g_strdup("Perl");
	filetypes[GEANY_FILETYPES_PERL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PERL]->title = g_strdup(_("Perl source file"));
	filetypes[GEANY_FILETYPES_PERL]->extension = g_strdup("perl");
	filetypes[GEANY_FILETYPES_PERL]->pattern[0] = g_strdup("*.pl");
	filetypes[GEANY_FILETYPES_PERL]->pattern[1] = g_strdup("*.perl");
	filetypes[GEANY_FILETYPES_PERL]->pattern[2] = g_strdup("*.pm");
	filetypes[GEANY_FILETYPES_PERL]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_PERL]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_PERL]->style_func_ptr = styleset_perl;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PERL]->title, filetypes[GEANY_FILETYPES_PERL]);

	filetypes[GEANY_FILETYPES_PHP] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_PHP]->id = GEANY_FILETYPES_PHP;
	filetypes[GEANY_FILETYPES_PHP]->name = g_strdup("PHP");
	filetypes[GEANY_FILETYPES_PHP]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PHP]->title = g_strdup(_("PHP / HTML source file"));
	filetypes[GEANY_FILETYPES_PHP]->extension = g_strdup("php");
	filetypes[GEANY_FILETYPES_PHP]->pattern[0] = g_strdup("*.php");
	filetypes[GEANY_FILETYPES_PHP]->pattern[1] = g_strdup("*.php4");
	filetypes[GEANY_FILETYPES_PHP]->pattern[2] = g_strdup("*.html");
	filetypes[GEANY_FILETYPES_PHP]->pattern[3] = g_strdup("*.htm");
	filetypes[GEANY_FILETYPES_PHP]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_PHP]->style_func_ptr = styleset_php;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PHP]->title, filetypes[GEANY_FILETYPES_PHP]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_PHP]->title, filetypes[GEANY_FILETYPES_PHP]);

	filetypes[GEANY_FILETYPES_XML] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_XML]->id = GEANY_FILETYPES_XML;
	filetypes[GEANY_FILETYPES_XML]->name = g_strdup("XML");
	filetypes[GEANY_FILETYPES_XML]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_XML]->title = g_strdup(_("XML source file"));
	filetypes[GEANY_FILETYPES_XML]->extension = g_strdup("xml");
	filetypes[GEANY_FILETYPES_XML]->pattern[0] = g_strdup("*.xml");
	filetypes[GEANY_FILETYPES_XML]->pattern[1] = g_strdup("*.sgml");
	filetypes[GEANY_FILETYPES_XML]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_XML]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_XML]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_XML]->style_func_ptr = styleset_xml;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_XML]->title, filetypes[GEANY_FILETYPES_XML]);

	filetypes[GEANY_FILETYPES_DOCBOOK] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_DOCBOOK]->id = GEANY_FILETYPES_DOCBOOK;
	filetypes[GEANY_FILETYPES_DOCBOOK]->name = g_strdup("Docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_DOCBOOK]->title = g_strdup(_("Docbook source file"));
	filetypes[GEANY_FILETYPES_DOCBOOK]->extension = g_strdup("docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern[0] = g_strdup("*.docbook");
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_DOCBOOK]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_DOCBOOK]->style_func_ptr = styleset_docbook;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_DOCBOOK]->title, filetypes[GEANY_FILETYPES_DOCBOOK]);

	filetypes[GEANY_FILETYPES_PYTHON] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_PYTHON]->id = GEANY_FILETYPES_PYTHON;
	filetypes[GEANY_FILETYPES_PYTHON]->name = g_strdup("Python");
	filetypes[GEANY_FILETYPES_PYTHON]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PYTHON]->title = g_strdup(_("Python source file"));
	filetypes[GEANY_FILETYPES_PYTHON]->extension = g_strdup("py");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[0] = g_strdup("*.py");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[1] = g_strdup("*.pyw");
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_PYTHON]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_PYTHON]->style_func_ptr = styleset_python;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PYTHON]->title, filetypes[GEANY_FILETYPES_PYTHON]);

	filetypes[GEANY_FILETYPES_TEX] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_TEX]->id = GEANY_FILETYPES_TEX;
	filetypes[GEANY_FILETYPES_TEX]->name = g_strdup("Tex");
	filetypes[GEANY_FILETYPES_TEX]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_TEX]->title = g_strdup(_("LaTex source file"));
	filetypes[GEANY_FILETYPES_TEX]->extension = g_strdup("tex");
	filetypes[GEANY_FILETYPES_TEX]->pattern[0] = g_strdup("*.tex");
	filetypes[GEANY_FILETYPES_TEX]->pattern[1] = g_strdup("*.sty");
	filetypes[GEANY_FILETYPES_TEX]->pattern[2] = g_strdup("*.idx");
	filetypes[GEANY_FILETYPES_TEX]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_TEX]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_TEX]->style_func_ptr = styleset_tex;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_TEX]->title, filetypes[GEANY_FILETYPES_TEX]);

	filetypes[GEANY_FILETYPES_PASCAL] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_PASCAL]->id = GEANY_FILETYPES_PASCAL;
	filetypes[GEANY_FILETYPES_PASCAL]->name = g_strdup("Pascal");
	filetypes[GEANY_FILETYPES_PASCAL]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_PASCAL]->title = g_strdup(_("Pascal source file"));
	filetypes[GEANY_FILETYPES_PASCAL]->extension = g_strdup("pas");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[0] = g_strdup("*.pas");
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_PASCAL]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_PASCAL]->style_func_ptr = styleset_pascal;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_PASCAL]->title, filetypes[GEANY_FILETYPES_PASCAL]);
	filetypes_create_newmenu_item(template_menu, filetypes[GEANY_FILETYPES_PASCAL]->title, filetypes[GEANY_FILETYPES_PASCAL]);

	filetypes[GEANY_FILETYPES_SH] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_SH]->id = GEANY_FILETYPES_SH;
	filetypes[GEANY_FILETYPES_SH]->name = g_strdup("Sh");
	filetypes[GEANY_FILETYPES_SH]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_SH]->title = g_strdup(_("Shell script file"));
	filetypes[GEANY_FILETYPES_SH]->extension = g_strdup("sh");
	filetypes[GEANY_FILETYPES_SH]->pattern[0] = g_strdup("*.sh");
	filetypes[GEANY_FILETYPES_SH]->pattern[1] = g_strdup("configure");
	filetypes[GEANY_FILETYPES_SH]->pattern[2] = g_strdup("*.ksh");
	filetypes[GEANY_FILETYPES_SH]->pattern[3] = g_strdup("*.zsh");
	filetypes[GEANY_FILETYPES_SH]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_SH]->style_func_ptr = styleset_sh;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_SH]->title, filetypes[GEANY_FILETYPES_SH]);

	filetypes[GEANY_FILETYPES_MAKE] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_MAKE]->id = GEANY_FILETYPES_MAKE;
	filetypes[GEANY_FILETYPES_MAKE]->name = g_strdup("Make");
	filetypes[GEANY_FILETYPES_MAKE]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_MAKE]->title = g_strdup(_("Makefile"));
	filetypes[GEANY_FILETYPES_MAKE]->extension = g_strdup("mak");
	filetypes[GEANY_FILETYPES_MAKE]->pattern[0] = g_strdup("Makefile*");
	filetypes[GEANY_FILETYPES_MAKE]->pattern[1] = g_strdup("*.mak");
	filetypes[GEANY_FILETYPES_MAKE]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_MAKE]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_MAKE]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_MAKE]->style_func_ptr = styleset_makefile;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_MAKE]->title, filetypes[GEANY_FILETYPES_MAKE]);

	filetypes[GEANY_FILETYPES_CSS] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_CSS]->id = GEANY_FILETYPES_CSS;
	filetypes[GEANY_FILETYPES_CSS]->name = g_strdup("CSS");
	filetypes[GEANY_FILETYPES_CSS]->has_tags = TRUE;
	filetypes[GEANY_FILETYPES_CSS]->title = g_strdup(_("Cascading StyleSheet"));
	filetypes[GEANY_FILETYPES_CSS]->extension = g_strdup("css");
	filetypes[GEANY_FILETYPES_CSS]->pattern[0] = g_strdup("*.css");
	filetypes[GEANY_FILETYPES_CSS]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_CSS]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_CSS]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_CSS]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_CSS]->style_func_ptr = styleset_css;
	filetypes_create_menu_item(filetype_menu, filetypes[GEANY_FILETYPES_CSS]->title, filetypes[GEANY_FILETYPES_CSS]);

	filetypes[GEANY_FILETYPES_ALL] = (filetype*)g_malloc(sizeof(filetype));
	filetypes[GEANY_FILETYPES_ALL]->id = GEANY_FILETYPES_ALL;
	filetypes[GEANY_FILETYPES_ALL]->name = g_strdup("None");
	filetypes[GEANY_FILETYPES_ALL]->has_tags = FALSE;
	filetypes[GEANY_FILETYPES_ALL]->title = g_strdup(_("All files"));
	filetypes[GEANY_FILETYPES_ALL]->extension = g_strdup("*");
	filetypes[GEANY_FILETYPES_ALL]->pattern[0] = g_strdup("*");
	filetypes[GEANY_FILETYPES_ALL]->pattern[1] = NULL;
	filetypes[GEANY_FILETYPES_ALL]->pattern[2] = NULL;
	filetypes[GEANY_FILETYPES_ALL]->pattern[3] = NULL;
	filetypes[GEANY_FILETYPES_ALL]->pattern[4] = NULL;	// always have to be NULL
	filetypes[GEANY_FILETYPES_ALL]->style_func_ptr = styleset_none;
	filetypes_create_menu_item(filetype_menu, _("None"), filetypes[GEANY_FILETYPES_ALL]);

}


/* simple filetype selection based on the filename extension */
filetype *filetypes_get_from_filename(const gchar *filename)
{
	GPatternSpec *pattern;
	gchar *base_filename;
	gint i, j;

	if (filename == NULL)
	{
		return filetypes[GEANY_FILETYPES_C];
	}
	// to match against the basename of the file(because of Makefile*)
	base_filename = g_path_get_basename(g_filename_to_utf8(filename, -1, NULL, NULL, NULL));

	for(i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		for (j = 0; j < GEANY_MAX_PATTERNS && filetypes[i]->pattern[j]; j++)
		{
			pattern = g_pattern_spec_new(filetypes[i]->pattern[j]);
			if (g_pattern_match_string(pattern, base_filename))
			{
				g_free(base_filename);
				g_pattern_spec_free(pattern);
				return filetypes[i];
			}
			g_pattern_spec_free(pattern);
		}
	}

	g_free(base_filename);
	return filetypes[GEANY_FILETYPES_ALL];
}


void filetypes_create_menu_item(GtkWidget *menu, gchar *label, filetype *ftype)
{
	GtkWidget *tmp = gtk_menu_item_new_with_label(label);
	gtk_widget_show(tmp);
	gtk_container_add(GTK_CONTAINER(menu), tmp);
	g_signal_connect((gpointer) tmp, "activate", G_CALLBACK(on_filetype_change), (gpointer) ftype);
}


void filetypes_create_newmenu_item(GtkWidget *menu, gchar *label, filetype *ftype)
{
	gchar *new_label = g_strdup_printf("   %s", label);
	GtkWidget *tmp_menu = gtk_menu_item_new_with_label(new_label);
	GtkWidget *tmp_button = gtk_menu_item_new_with_label(new_label);
	g_free(new_label);
	gtk_widget_show(tmp_menu);
	gtk_widget_show(tmp_button);
	gtk_container_add(GTK_CONTAINER(menu), tmp_menu);
	gtk_container_add(GTK_CONTAINER(app->new_file_menu), tmp_button);
	g_signal_connect((gpointer) tmp_menu, "activate", G_CALLBACK(on_new_with_template), (gpointer) ftype);
	g_signal_connect((gpointer) tmp_button, "activate", G_CALLBACK(on_new_with_template), (gpointer) ftype);
}


/* frees the array and all related pointers */
void filetypes_free_types(void)
{
	gint i;

	for(i = 0; i < GEANY_MAX_FILE_TYPES; i++)
	{
		if (filetypes[i])
		{
			g_free(filetypes[i]->name);
			g_free(filetypes[i]->title);
			g_free(filetypes[i]->extension);
			if (filetypes[i]->pattern[0]) g_free(filetypes[i]->pattern[0]);
			if (filetypes[i]->pattern[1]) g_free(filetypes[i]->pattern[1]);
			if (filetypes[i]->pattern[2]) g_free(filetypes[i]->pattern[2]);
			if (filetypes[i]->pattern[3]) g_free(filetypes[i]->pattern[3]);
			//g_free(filetypes[i]);
		}
	}
}


gchar *filetypes_get_template(filetype *ft)
{
	if (ft == NULL) return NULL;

	switch (ft->id)
	{
		case GEANY_FILETYPES_C:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_C); break;
		case GEANY_FILETYPES_CPP:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_CPP); break;
		case GEANY_FILETYPES_PHP:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_PHP); break;
		case GEANY_FILETYPES_JAVA:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_JAVA); break;
		case GEANY_FILETYPES_PASCAL:
			return templates_get_template_generic(GEANY_TEMPLATE_FILETYPE_PASCAL); break;
		default: return NULL;
	}
}

